/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <glib/gi18n.h>
#include <math.h>
#include <ghostty/vt.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "termac-attachments.h"
#include "termac-terminal.h"
#include "termac-util.h"
#include "termac-vt.h"
#include "termac-window.h"

#define READ_BUDGET (64 * 1024)
#define WRITE_QUEUE_LIMIT (1024 * 1024)
/* Input that does not fit the queue waits here, in order, and is moved in as the
 * program reads. The cap only guards against a runaway producer. */
#define WRITE_BACKLOG_LIMIT (256 * 1024 * 1024)
#define TERMAC_TERMINAL_INSET_START 8
#define TERMAC_TERMINAL_INSET_TOP 12
#define TERMAC_TERMINAL_WHEEL_LINES 3
#define TERMAC_TERMINAL_JOB_POLL_MS 500
#define TERMAC_TERMINAL_CLOSE_GRACE_MS 1000
#define TERMAC_TERMINAL_DEFAULT_SCROLLBACK 10000

typedef struct {
	TermacTerminal *terminal; /* weak; detached before widget disposal */
	GPid pid;
	pid_t foreground;
	guint kill_source;
	GApplication *application; /* held only while stopping */
} TerminalChildWatch;

struct _TermacTerminal
{
	GtkDrawingArea parent_instance;
	GhosttyTerminal terminal;
	GhosttyKeyEncoder key_encoder;
	GhosttyKeyEvent key_event;
	GhosttyMouseEncoder mouse_encoder;
	GhosttyMouseEvent mouse_event;
	GhosttyRenderState render_state;
	GhosttyRenderStateRowIterator row_iterator;
	GhosttyRenderStateRowCells cells;
	TermacVtSelector *selector;
	GtkIMContext *im_context;
	char *preedit;
	char *highlight;
	gboolean highlight_has_current;
	guint64 highlight_row;
	guint highlight_column;
	GByteArray *write_queue;
	GByteArray *write_backlog;
	GPtrArray *attachments;
	const TermacAgent *agent;
	char *initial_command;
	GPid child_pid;
	TerminalChildWatch *child_watch;
	char *proc_cwd;
	guint io_source;
	guint write_source;
	guint child_source;
	guint job_source;
	int pty_fd;
	int cell_width;
	int cell_height;
	int surface_width;
	int surface_height;
	guint font_size;
	guint scrollback_lines;
	uint16_t cols;
	uint16_t rows;
	gboolean child_exited;
	gboolean closing;
	gboolean selecting;
	guint mouse_button;
	double pointer_x;
	double pointer_y;
	gboolean job_active;
	gint64 job_started_us;
	gint64 interrupt_armed_us;
	gboolean hover_link;
	guint hover_row;
	guint hover_first;
	guint hover_last;
	gboolean shell_integration;
	gboolean host_shell;
	char *error;
	char *cwd;
	char *initial_cwd;
	char *session_id;
	GString *accessible_text;
	GBytes *accessible_selection;
	guint accessible_caret;
	gboolean accessible_removing;
};

static void terminal_accessible_text_init (GtkAccessibleTextInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE (TermacTerminal, termac_terminal, GTK_TYPE_DRAWING_AREA,
                              G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT, terminal_accessible_text_init))

#include "termac-terminal-accessible.inc"

enum
{
	SIGNAL_TITLE_CHANGED,
	SIGNAL_CWD_CHANGED,
	SIGNAL_BELL,
	SIGNAL_OUTPUT,
	SIGNAL_COMMAND_FINISHED,
	SIGNAL_JOB_FINISHED,
	SIGNAL_MESSAGE,
	N_SIGNALS
};

static guint terminal_signals[N_SIGNALS];

static void terminal_queue_bytes (TermacTerminal *self, const uint8_t *bytes, gsize len);
static void terminal_send_input (TermacTerminal *self, const char *bytes, gsize len);
static gboolean terminal_spawn (TermacTerminal *self);
static void terminal_resize (TermacTerminal *self, int width, int height);

/* ---- helpers ---------------------------------------------------------- */

static char *
ghostty_string_copy (const uint8_t *ptr,
                     size_t         len)
{
	if (ptr == NULL || len == 0)
		return g_strdup ("");
	return g_utf8_make_valid ((const char *) ptr, (gssize) len);
}

static void
terminal_set_error (TermacTerminal *self,
                    const char     *message)
{
	g_clear_pointer (&self->error, g_free);
	self->error = g_strdup (message);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
terminal_mode_enabled (TermacTerminal *self,
                       GhosttyMode     mode)
{
	GhosttyTerminalModeConfig config = { mode, FALSE };

	if (self->terminal == NULL)
		return FALSE;
	if (ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_MODE, &config) != GHOSTTY_SUCCESS)
		return FALSE;
	return config.value;
}

static GhosttyMods
terminal_ghostty_mods (GdkModifierType state)
{
	GhosttyMods mods = 0;

	if (state & GDK_SHIFT_MASK) mods |= GHOSTTY_MODS_SHIFT;
	if (state & GDK_CONTROL_MASK) mods |= GHOSTTY_MODS_CTRL;
	if (state & GDK_ALT_MASK) mods |= GHOSTTY_MODS_ALT;
	if (state & GDK_SUPER_MASK) mods |= GHOSTTY_MODS_SUPER;
	return mods;
}

static gint64
terminal_now_us (void)
{
	return g_get_monotonic_time ();
}

static void
terminal_cell_at (TermacTerminal *self,
                  double          x,
                  double          y,
                  guint          *column,
                  guint          *row)
{
	int col = (int) ((x - TERMAC_TERMINAL_INSET_START) / MAX (self->cell_width, 1));
	int line = (int) ((y - TERMAC_TERMINAL_INSET_TOP) / MAX (self->cell_height, 1));

	*column = (guint) CLAMP (col, 0, MAX (self->cols - 1, 0));
	*row = (guint) CLAMP (line, 0, MAX (self->rows - 1, 0));
}

/* ---- pty I/O ---------------------------------------------------------- */

static gboolean
terminal_write_ready (gint         fd,
                      GIOCondition condition,
                      gpointer     user_data)
{
	TermacTerminal *self = user_data;
	gsize written_total = 0;

	if (condition & (G_IO_ERR | G_IO_NVAL))
		goto done;

	for (;;)
	{
		gsize room = WRITE_QUEUE_LIMIT - self->write_queue->len;
		gsize move = MIN (room, (gsize) self->write_backlog->len);

		if (move > 0)
		{
			g_byte_array_append (self->write_queue, self->write_backlog->data, (guint) move);
			g_byte_array_remove_range (self->write_backlog, 0, (guint) move);
		}
		if (self->write_queue->len == 0 || written_total >= READ_BUDGET)
			break;
		{
			ssize_t n = write (fd, self->write_queue->data, self->write_queue->len);
			if (n > 0)
			{
				g_byte_array_remove_range (self->write_queue, 0, (guint) n);
				written_total += (gsize) n;
				continue;
			}
			if (n < 0 && errno == EINTR)
				continue;
			if (n < 0 && errno == EAGAIN)
				return G_SOURCE_CONTINUE;
			goto done;
		}
	}

	if (self->write_queue->len > 0 || self->write_backlog->len > 0)
		return G_SOURCE_CONTINUE;

done:
	self->write_source = 0;
	return G_SOURCE_REMOVE;
}

static void
terminal_queue_bytes (TermacTerminal *self,
                      const uint8_t  *bytes,
                      gsize           len)
{
	if (self->pty_fd < 0 || len == 0 || self->closing)
		return;
	if (self->write_backlog->len > 0 || len > WRITE_QUEUE_LIMIT - self->write_queue->len)
	{
		if (len > WRITE_BACKLOG_LIMIT - self->write_backlog->len)
		{
			g_warning ("terminal input backlog reached its 256 MiB limit");
			return;
		}
		g_byte_array_append (self->write_backlog, bytes, (guint) len);
	}
	else
		g_byte_array_append (self->write_queue, bytes, (guint) len);
	if (self->write_source == 0)
		self->write_source = g_unix_fd_add (self->pty_fd,
		                                    G_IO_OUT | G_IO_ERR | G_IO_HUP,
		                                    terminal_write_ready,
		                                    self);
}

/* Input typed by the user returns the view to the newest output. */
static void
terminal_send_input (TermacTerminal *self,
                     const char     *bytes,
                     gsize           len)
{
	if (self->terminal != NULL)
	{
		/* Typing ends the selection, so a later Ctrl+C reaches the program again. */
		termac_vt_clear_selection (self->terminal);
		ghostty_terminal_scroll_viewport (self->terminal,
		    (GhosttyTerminalScrollViewport) { .tag = GHOSTTY_SCROLL_VIEWPORT_BOTTOM });
		gtk_widget_queue_draw (GTK_WIDGET (self));
	}
	terminal_queue_bytes (self, (const uint8_t *) bytes, len);
}

static void
terminal_write_pty (GhosttyTerminal *terminal,
                    void            *userdata,
                    const uint8_t   *data,
                    size_t           len)
{
	(void) terminal;
	terminal_queue_bytes (TERMAC_TERMINAL (userdata), data, len);
}

static void
terminal_refresh (TermacTerminal *self)
{
	if (self->terminal != NULL)
		ghostty_render_state_update (self->render_state, self->terminal);
	terminal_accessible_update (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
terminal_read_ready (gint         fd,
                     GIOCondition condition,
                     gpointer     user_data)
{
	TermacTerminal *self = user_data;
	uint8_t buffer[4096];
	gsize total = 0;
	gboolean changed = FALSE;

	if (condition & (G_IO_ERR | G_IO_NVAL))
	{
		self->io_source = 0;
		return G_SOURCE_REMOVE;
	}

	while (total < READ_BUDGET)
	{
		ssize_t n = read (fd, buffer, sizeof buffer);
		if (n > 0)
		{
			ghostty_terminal_vt_write (self->terminal, buffer, (size_t) n);
			total += (gsize) n;
			changed = TRUE;
			continue;
		}
		if (n == 0)
			break;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN)
			break;
		if (errno != EIO)
			g_warning ("PTY read failed: %s", g_strerror (errno));
		self->io_source = 0;
		if (changed)
		{
			terminal_refresh (self);
			g_signal_emit (self, terminal_signals[SIGNAL_OUTPUT], 0);
		}
		return G_SOURCE_REMOVE;
	}

	if (changed)
	{
		terminal_refresh (self);
		g_signal_emit (self, terminal_signals[SIGNAL_OUTPUT], 0);
	}
	if (condition & G_IO_HUP)
	{
		self->io_source = 0;
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

/* ---- child process ---------------------------------------------------- */

static void
terminal_child_watch_free (gpointer data)
{
	TerminalChildWatch *watch = data;
	if (watch->kill_source != 0) g_source_remove (watch->kill_source);
	if (watch->application != NULL)
	{
		g_application_release (watch->application);
		g_object_unref (watch->application);
	}
	g_free (watch);
}

static void
terminal_child_exited (GPid pid, gint status, gpointer user_data)
{
	TerminalChildWatch *watch = user_data;
	TermacTerminal *self = watch->terminal;
	(void) status;
	if (self != NULL)
	{
		self->child_exited = TRUE;
		self->child_pid = 0;
		self->child_source = 0;
		self->child_watch = NULL;
		if (self->job_source != 0)
		{
			g_source_remove (self->job_source);
			self->job_source = 0;
		}
		self->job_active = FALSE;
		if (self->pty_fd >= 0 && self->io_source == 0)
			gtk_widget_queue_draw (GTK_WIDGET (self));
	}
	else if (watch->foreground > 0 && watch->foreground != pid && watch->foreground != getpgrp ())
	{
		/* The shell exited while its old foreground job was still alive. */
		kill (-watch->foreground, SIGKILL);
	}
	g_spawn_close_pid (pid);
}

static void
terminal_watch_child (TermacTerminal *self)
{
	TerminalChildWatch *watch = g_new0 (TerminalChildWatch, 1);
	watch->terminal = self;
	watch->pid = self->child_pid;
	self->child_watch = watch;
	self->child_source = g_child_watch_add_full (G_PRIORITY_DEFAULT, self->child_pid,
	    terminal_child_exited, watch, terminal_child_watch_free);
}

static gboolean
terminal_child_force_exit (gpointer data)
{
	TerminalChildWatch *watch = data;
	watch->kill_source = 0;
	if (watch->foreground > 0 && watch->foreground != watch->pid && watch->foreground != getpgrp ())
		kill (-watch->foreground, SIGKILL);
	kill (watch->pid, SIGKILL);
	return G_SOURCE_REMOVE;
}

static gboolean
terminal_has_job (TermacTerminal *self)
{
	pid_t group;

	if (self->pty_fd < 0 || self->child_pid <= 0 || self->child_exited)
		return FALSE;
	/* Host process groups are outside this PID namespace. Treat a live
	 * host session as busy rather than silently discarding running work. */
	if (self->host_shell)
		return TRUE;
	group = tcgetpgrp (self->pty_fd);
	return group > 0 && group != self->child_pid;
}

gboolean
termac_terminal_has_foreground_job (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), FALSE);
	return terminal_has_job (self);
}

static gboolean
terminal_job_poll (gpointer user_data)
{
	TermacTerminal *self = user_data;
	gboolean busy = terminal_has_job (self);

	if (busy && !self->job_active)
	{
		self->job_active = TRUE;
		self->job_started_us = terminal_now_us ();
	}
	else if (!busy && self->job_active)
	{
		guint64 duration_ms = (guint64) ((terminal_now_us () - self->job_started_us) / 1000);
		self->job_active = FALSE;
		g_signal_emit (self, terminal_signals[SIGNAL_JOB_FINISHED], 0, duration_ms);
	}
	return G_SOURCE_CONTINUE;
}

/* Host Bash reports its cwd because the sandbox cannot inspect its /proc.
 * Command notifications remain opt-in. Always source the user's rc first. */
static char *
terminal_write_bash_integration (gboolean report_commands)
{
	static const char script[] =
		"# Termac shell integration (opt-in). Generated by Termac.\n"
		"if [ -r \"$HOME/.bashrc\" ]; then . \"$HOME/.bashrc\"; fi\n"
		"__termac_started=0\n"
		"__termac_in_prompt=0\n"
		"__termac_user_prompt_command=\"${PROMPT_COMMAND}\"\n"
		"__termac_preexec() {\n"
		"  [ \"$__termac_in_prompt\" = 1 ] && return\n"
		"  case \"$BASH_COMMAND\" in __termac_precmd*) return ;; esac\n"
		"  [ -n \"$COMP_LINE\" ] && return\n"
		"  __termac_started=1\n"
		"}\n"
		"__termac_precmd() {\n"
		"  local status=$?\n"
		"  __termac_in_prompt=1\n"
		"  if [ \"$__termac_started\" = 1 ]; then\n"
		"    __termac_started=0\n"
		"    printf '\\033]777;notify;" TERMAC_COMMAND_NOTIFICATION_TITLE ";%d\\007' \"$status\"\n"
		"  fi\n"
		"  eval \"$__termac_user_prompt_command\"\n"
		"  __termac_report_pwd\n"
		"  __termac_in_prompt=0\n"
		"  return $status\n"
		"}\n"
		"PROMPT_COMMAND=__termac_precmd\n"
		"trap '__termac_preexec' DEBUG\n";
	static const char cwd_script[] =
		"__termac_report_pwd() {\n"
		"  local status=$? LC_ALL=C path=$PWD encoded= byte hex i\n"
		"  for ((i=0; i<${#path}; i++)); do\n"
		"    byte=${path:i:1}\n"
		"    case \"$byte\" in [a-zA-Z0-9/._~-]) encoded+=\"$byte\" ;;\n"
		"      *) printf -v hex '%%%02X' \"'$byte\"; encoded+=\"$hex\" ;; esac\n"
		"  done\n"
		"  printf '\\033]7;file://localhost%s\\007' \"$encoded\"\n"
		"  return \"$status\"\n"
		"}\n";
	g_autofree char *dir = g_build_filename (g_get_user_cache_dir (), "termac", "shell", NULL);
	g_autofree char *path = g_build_filename (dir, report_commands ? "bash-integration.sh" : "bash-host-cwd.sh", NULL);
	g_autofree char *contents = g_strconcat (report_commands ? script :
		"# Termac host cwd integration. Generated by Termac.\nif [ -r \"$HOME/.bashrc\" ]; then . \"$HOME/.bashrc\"; fi\n",
		cwd_script, report_commands ? "" : "PROMPT_COMMAND+=(__termac_report_pwd)\n", NULL);

	if (g_mkdir_with_parents (dir, 0700) != 0)
		return NULL;
	if (!g_file_set_contents (path, contents, -1, NULL))
		return NULL;
	return g_steal_pointer (&path);
}

static gboolean
terminal_spawn (TermacTerminal *self)
{
	struct winsize size = { 0 };
	const char *shell;
	const char *shell_name;
	const char *home_dir = g_get_home_dir ();
	g_autofree char *rcfile = NULL;
	g_auto(GStrv) shell_argv = NULL;
	gboolean host_shell = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
	int error_pipe[2];
	int flags;
	pid_t pid;

	self->host_shell = host_shell;
	size.ws_col = self->cols;
	size.ws_row = self->rows;
	size.ws_xpixel = (unsigned short) (self->cols * self->cell_width);
	size.ws_ypixel = (unsigned short) (self->rows * self->cell_height);

	shell = g_getenv ("SHELL");
	if (shell == NULL || *shell == '\0')
	{
		struct passwd *pw = getpwuid (getuid ());
		shell = pw != NULL && pw->pw_shell != NULL && *pw->pw_shell != '\0' ? pw->pw_shell : "/bin/sh";
	}
	shell_name = strrchr (shell, '/');
	shell_name = shell_name != NULL ? shell_name + 1 : shell;
	if (host_shell || (self->shell_integration && g_strcmp0 (shell_name, "bash") == 0))
		rcfile = terminal_write_bash_integration (self->shell_integration);

	shell_argv = termac_shell_command_argv (shell,
		self->initial_cwd != NULL ? self->initial_cwd : home_dir, rcfile, host_shell, self->initial_command);

	if (pipe (error_pipe) != 0)
	{
		terminal_set_error (self, g_strdup_printf (_("Could not create the terminal: %s"),
		                                          g_strerror (errno)));
		return FALSE;
	}
	fcntl (error_pipe[0], F_SETFD, FD_CLOEXEC);
	fcntl (error_pipe[1], F_SETFD, FD_CLOEXEC);
	pid = forkpty (&self->pty_fd, NULL, NULL, &size);
	if (pid < 0)
	{
		int saved = errno;
		close (error_pipe[0]);
		close (error_pipe[1]);
		self->pty_fd = -1;
		terminal_set_error (self, g_strdup_printf (_("Could not create the terminal: %s"),
		                                          g_strerror (saved)));
		return FALSE;
	}
	if (pid == 0)
	{
		int saved;
		close (error_pipe[0]);
		if (self->initial_cwd != NULL && g_file_test (self->initial_cwd, G_FILE_TEST_IS_DIR))
			chdir (self->initial_cwd);
		else if (home_dir != NULL)
			chdir (home_dir);
		setenv ("TERM", "xterm-256color", 1);
		execvp (shell_argv[0], shell_argv);
		saved = errno;
		{
			ssize_t ignored = write (error_pipe[1], &saved, sizeof saved);
			(void) ignored;
		}
		_exit (127);
	}
	self->child_pid = pid;
	close (error_pipe[1]);
	{
		int child_errno = 0;
		ssize_t n;
		do
			n = read (error_pipe[0], &child_errno, sizeof child_errno);
		while (n < 0 && errno == EINTR);
		close (error_pipe[0]);
		if (n == (ssize_t) sizeof child_errno)
		{
			g_autofree char *message = g_strdup_printf (_("Could not start the shell %s: %s"),
			                                           shell, g_strerror (child_errno));
			waitpid (pid, NULL, 0);
			self->child_pid = 0;
			close (self->pty_fd);
			self->pty_fd = -1;
			terminal_set_error (self, message);
			return FALSE;
		}
	}

	flags = fcntl (self->pty_fd, F_GETFL);
	if (flags < 0 || fcntl (self->pty_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		g_warning ("Could not make PTY nonblocking: %s", g_strerror (errno));
		kill (self->child_pid, SIGHUP);
		terminal_set_error (self, _("Could not configure terminal input."));
		return FALSE;
	}
	self->child_exited = FALSE;
	self->io_source = g_unix_fd_add (self->pty_fd,
	                                 G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                 terminal_read_ready,
	                                 self);
	terminal_watch_child (self);
	self->job_source = g_timeout_add (TERMAC_TERMINAL_JOB_POLL_MS, terminal_job_poll, self);
	return TRUE;
}

/* Transfer process ownership to the existing child watch. It outlives the
 * widget without retaining it; normal exit cancels escalation and reaps once. */
static void
terminal_stop_child (TermacTerminal *self)
{
	TerminalChildWatch *watch;
	GApplication *application;
	if (self->child_pid <= 0 || self->child_exited) return;
	if (self->child_watch == NULL) terminal_watch_child (self);
	watch = self->child_watch;
	watch->terminal = NULL;
	watch->foreground = self->pty_fd >= 0 ? tcgetpgrp (self->pty_fd) : -1;
	application = g_application_get_default ();
	if (application != NULL && g_application_get_is_registered (application))
	{
		watch->application = g_object_ref (application);
		g_application_hold (application);
	}
	if (watch->foreground > 0 && watch->foreground != watch->pid && watch->foreground != getpgrp ())
		kill (-watch->foreground, SIGHUP);
	kill (watch->pid, SIGHUP);
	watch->kill_source = g_timeout_add (TERMAC_TERMINAL_CLOSE_GRACE_MS,
	    terminal_child_force_exit, watch);
	self->child_watch = NULL;
	self->child_source = 0;
	self->child_pid = 0;
	self->child_exited = TRUE;
}

/* ---- engine callbacks ------------------------------------------------- */

static void
terminal_title_changed (GhosttyTerminal terminal, void *userdata)
{
	TermacTerminal *self = TERMAC_TERMINAL (userdata);
	GhosttyString title = { 0 };
	g_autofree char *valid_title = NULL;

	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) != GHOSTTY_SUCCESS)
		return;
	valid_title = ghostty_string_copy (title.ptr, title.len);
	g_signal_emit (self, terminal_signals[SIGNAL_TITLE_CHANGED], 0, valid_title);
}

static void
terminal_pwd_changed (GhosttyTerminal terminal, void *userdata)
{
	TermacTerminal *self = TERMAC_TERMINAL (userdata);
	GhosttyString pwd = { 0 };
	g_autofree char *value = NULL;

	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_PWD, &pwd) != GHOSTTY_SUCCESS)
		return;
	value = ghostty_string_copy (pwd.ptr, pwd.len);
	if (*value == '\0')
		return;
	g_free (self->cwd);
	self->cwd = termac_path_from_pwd (value);
	g_signal_emit (self, terminal_signals[SIGNAL_CWD_CHANGED], 0, self->cwd);
}

static void
terminal_bell (GhosttyTerminal terminal, void *userdata)
{
	(void) terminal;
	g_signal_emit (TERMAC_TERMINAL (userdata), terminal_signals[SIGNAL_BELL], 0);
}

static void
terminal_desktop_notification (GhosttyTerminal                          terminal,
                               void                                    *userdata,
                               const GhosttyTerminalDesktopNotification *notification)
{
	TermacTerminal *self = TERMAC_TERMINAL (userdata);
	g_autofree char *title = NULL;
	g_autofree char *body = NULL;

	(void) terminal;
	if (notification == NULL)
		return;
	title = ghostty_string_copy (notification->title.ptr, notification->title.len);
	if (g_strcmp0 (title, TERMAC_COMMAND_NOTIFICATION_TITLE) != 0)
		return;
	body = ghostty_string_copy (notification->body.ptr, notification->body.len);
	g_signal_emit (self, terminal_signals[SIGNAL_COMMAND_FINISHED], 0,
	               (gint) g_ascii_strtoll (body, NULL, 10));
}

static void
terminal_apply_scrollback (TermacTerminal *self)
{
	size_t lines = self->scrollback_lines;

	if (self->terminal != NULL)
		ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES, &lines);
}

/* Creates the libghostty-vt engine and its helpers. Returns FALSE and sets
 * the widget error when the engine cannot be created. */
static gboolean
terminal_create_engine (TermacTerminal *self)
{
	GhosttyResult result;

	if (self->terminal != NULL)
		return TRUE;
	result = ghostty_terminal_new (NULL, &self->terminal, self->cols, self->rows);
	if (result != GHOSTTY_SUCCESS)
	{
		self->terminal = NULL;
		terminal_set_error (self, g_strdup_printf (_("The terminal engine could not be created (%d)."),
		                                           result));
		return FALSE;
	}
	ghostty_key_encoder_new (NULL, &self->key_encoder);
	ghostty_key_event_new (NULL, &self->key_event);
	ghostty_mouse_encoder_new (NULL, &self->mouse_encoder);
	ghostty_mouse_event_new (NULL, &self->mouse_event);
	ghostty_terminal_resize (self->terminal, self->cols, self->rows,
	                         (uint32_t) self->cell_width, (uint32_t) self->cell_height);
	ghostty_render_state_new (NULL, &self->render_state);
	ghostty_render_state_row_iterator_new (NULL, &self->row_iterator);
	ghostty_render_state_row_cells_new (NULL, &self->cells);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_USERDATA, self);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
	                      (const void *) terminal_write_pty);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
	                      (const void *) terminal_title_changed);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_PWD_CHANGED,
	                      (const void *) terminal_pwd_changed);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_BELL,
	                      (const void *) terminal_bell);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_DESKTOP_NOTIFICATION,
	                      (const void *) terminal_desktop_notification);
	terminal_apply_scrollback (self);
	return TRUE;
}

/* ---- rendering -------------------------------------------------------- */

static void
terminal_draw (GtkDrawingArea *area,
               cairo_t        *cr,
               int             width,
               int             height,
               gpointer        user_data)
{
	TermacTerminal *self = user_data;
	GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED (GhosttyRenderStateColors);
	PangoLayout *layout;
	PangoFontDescription *font;
	uint16_t cursor_x = 0, cursor_y = 0;
	gboolean cursor_visible = FALSE;
	gboolean cursor_in_viewport = FALSE;
	int row = 0;
	(void) area;

	self->surface_width = width;
	self->surface_height = height;
	if (self->error != NULL)
	{
		g_autofree char *message = g_strdup_printf ("%s\n\n%s", self->error,
		                                           _("Press Enter or click to try again."));

		cairo_set_source_rgb (cr, 30 / 255.0, 30 / 255.0, 30 / 255.0);
		cairo_paint (cr);
		layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), message);
		pango_layout_set_width (layout, MAX (width - 2 * TERMAC_TERMINAL_INSET_START, 1) * PANGO_SCALE);
		pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
		cairo_set_source_rgb (cr, 246 / 255.0, 245 / 255.0, 244 / 255.0);
		cairo_move_to (cr, TERMAC_TERMINAL_INSET_START, TERMAC_TERMINAL_INSET_TOP);
		pango_cairo_show_layout (cr, layout);
		g_object_unref (layout);
		return;
	}

	terminal_resize (self,
	                 MAX (1, width - TERMAC_TERMINAL_INSET_START),
	                 MAX (1, height - TERMAC_TERMINAL_INSET_TOP));
	if (self->terminal == NULL || self->render_state == NULL)
		return;
	ghostty_render_state_update (self->render_state, self->terminal);
	terminal_accessible_update (self);
	if (ghostty_render_state_colors_get (self->render_state, &colors) != GHOSTTY_SUCCESS)
		return;

	cairo_set_source_rgb (cr, colors.background.r / 255.0,
	                      colors.background.g / 255.0,
	                      colors.background.b / 255.0);
	cairo_paint (cr);
	cairo_translate (cr, TERMAC_TERMINAL_INSET_START, TERMAC_TERMINAL_INSET_TOP);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
	font = pango_font_description_new ();
	pango_font_description_set_family (font, "Monospace");
	pango_font_description_set_size (font, (int) self->font_size * PANGO_SCALE);
	pango_layout_set_font_description (layout, font);
	ghostty_render_state_get (self->render_state,
	                          GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,
	                          &cursor_visible);
	ghostty_render_state_get (self->render_state,
	                          GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,
	                          &cursor_in_viewport);
	ghostty_render_state_get (self->render_state,
	                          GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,
	                          &cursor_x);
	ghostty_render_state_get (self->render_state,
	                          GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,
	                          &cursor_y);
	ghostty_render_state_get (self->render_state,
	                          GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,
	                          &self->row_iterator);

	while (ghostty_render_state_row_iterator_next (self->row_iterator))
	{
		int col = 0;
		GhosttyRenderStateRowSelection selection = GHOSTTY_INIT_SIZED (GhosttyRenderStateRowSelection);

		if (ghostty_render_state_row_get (self->row_iterator,
		        GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION, &selection) == GHOSTTY_SUCCESS)
		{
			cairo_set_source_rgba (cr, colors.foreground.r / 255.0, colors.foreground.g / 255.0,
			                       colors.foreground.b / 255.0, 0.28);
			cairo_rectangle (cr, selection.start_x * self->cell_width, row * self->cell_height,
			                 (selection.end_x - selection.start_x + 1) * self->cell_width,
			                 self->cell_height);
			cairo_fill (cr);
		}
		if (ghostty_render_state_row_get (self->row_iterator,
		        GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &self->cells) != GHOSTTY_SUCCESS)
			continue;
		while (ghostty_render_state_row_cells_next (self->cells))
		{
			GhosttyColorRgb fg = colors.foreground;
			GhosttyColorRgb bg;
			GhosttyStyle style = GHOSTTY_INIT_SIZED (GhosttyStyle);
			GhosttyBuffer text_buffer;
			char text[256] = { 0 };
			gboolean has_bg;
			int x = col * self->cell_width;
			int y = row * self->cell_height;

			text_buffer.ptr = (uint8_t *) text;
			text_buffer.cap = sizeof text - 1;
			text_buffer.len = 0;
			ghostty_render_state_row_cells_get (self->cells,
			        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,
			        &text_buffer);
			has_bg = ghostty_render_state_row_cells_get (self->cells,
			        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bg) == GHOSTTY_SUCCESS;
			ghostty_render_state_row_cells_get (self->cells,
			        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fg);
			ghostty_render_state_row_cells_get (self->cells,
			        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);
			if (style.inverse)
			{
				GhosttyColorRgb tmp = fg;
				fg = has_bg ? bg : colors.background;
				bg = tmp;
				has_bg = TRUE;
			}
			if (has_bg)
			{
				cairo_set_source_rgb (cr, bg.r / 255.0, bg.g / 255.0, bg.b / 255.0);
				cairo_rectangle (cr, x, y, self->cell_width, self->cell_height);
				cairo_fill (cr);
			}
			if (text_buffer.len > 0 && text_buffer.len < sizeof text)
			{
				text[text_buffer.len] = '\0';
				pango_layout_set_text (layout, text, (int) text_buffer.len);
				pango_font_description_set_weight (font, style.bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
				pango_font_description_set_style (font, style.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
				pango_layout_set_font_description (layout, font);
				cairo_set_source_rgb (cr, fg.r / 255.0, fg.g / 255.0, fg.b / 255.0);
				cairo_move_to (cr, x, y);
				pango_cairo_show_layout (cr, layout);
			}
			col++;
		}
		row++;
	}

	if (self->highlight != NULL) {
		g_autoptr(GArray) matches = termac_vt_find_matches (self->terminal, self->cols, self->rows, self->highlight);
		GhosttyTerminalScrollbar bar = { 0 };
		g_autoptr(GdkRGBA) accent = adw_style_manager_get_accent_color_rgba (adw_style_manager_get_default ());
		ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar);
		for (guint i = 0; i < matches->len; i++) {
			TermacVtMatch m = g_array_index (matches, TermacVtMatch, i);
			cairo_set_source_rgba (cr, accent->red, accent->green, accent->blue,
			                       self->highlight_has_current && m.row + bar.offset == self->highlight_row &&
			                       m.column == self->highlight_column ? 0.55 : 0.35);
			cairo_rectangle (cr, m.column * self->cell_width, m.row * self->cell_height,
			                 m.cells * self->cell_width, self->cell_height);
			cairo_fill (cr);
		}
	}

	if (self->hover_link && self->hover_row < self->rows)
	{
		cairo_set_source_rgb (cr, colors.foreground.r / 255.0, colors.foreground.g / 255.0,
		                      colors.foreground.b / 255.0);
		cairo_rectangle (cr, self->hover_first * self->cell_width,
		                 (self->hover_row + 1) * self->cell_height - 1,
		                 (self->hover_last - self->hover_first) * self->cell_width, 1);
		cairo_fill (cr);
	}
	if (cursor_visible && cursor_in_viewport && cursor_x < self->cols && cursor_y < self->rows)
	{
		GhosttyColorRgb cursor = colors.cursor_has_value ? colors.cursor : colors.foreground;
		cairo_set_source_rgba (cr, cursor.r / 255.0, cursor.g / 255.0,
		                       cursor.b / 255.0, 0.55);
		cairo_rectangle (cr, cursor_x * self->cell_width,
		                 cursor_y * self->cell_height,
		                 self->cell_width, self->cell_height);
		cairo_fill (cr);
	}
	if (cursor_in_viewport && cursor_x < self->cols && cursor_y < self->rows)
	{
		GdkRectangle location = {
			TERMAC_TERMINAL_INSET_START + cursor_x * self->cell_width,
			TERMAC_TERMINAL_INSET_TOP + cursor_y * self->cell_height,
			self->cell_width, self->cell_height
		};

		/* Tells the input method where to place its candidate window. */
		if (self->im_context != NULL)
			gtk_im_context_set_cursor_location (self->im_context, &location);
		if (self->preedit != NULL && *self->preedit != '\0')
		{
			PangoRectangle ink;

			pango_layout_set_attributes (layout, NULL);
			pango_layout_set_text (layout, self->preedit, -1);
			pango_layout_get_pixel_extents (layout, &ink, NULL);
			cairo_set_source_rgb (cr, colors.background.r / 255.0, colors.background.g / 255.0,
			                      colors.background.b / 255.0);
			cairo_rectangle (cr, cursor_x * self->cell_width, cursor_y * self->cell_height,
			                 MAX (ink.width, self->cell_width), self->cell_height);
			cairo_fill (cr);
			cairo_set_source_rgb (cr, colors.foreground.r / 255.0, colors.foreground.g / 255.0,
			                      colors.foreground.b / 255.0);
			cairo_move_to (cr, cursor_x * self->cell_width, cursor_y * self->cell_height);
			pango_cairo_show_layout (cr, layout);
			cairo_rectangle (cr, cursor_x * self->cell_width,
			                 (cursor_y + 1) * self->cell_height - 1,
			                 MAX (ink.width, self->cell_width), 1);
			cairo_fill (cr);
		}
	}

	g_object_unref (layout);
	pango_font_description_free (font);
}

static void
terminal_resize (TermacTerminal *self,
                 int             width,
                 int             height)
{
	struct winsize size = { 0 };
	uint16_t cols = (uint16_t) CLAMP (width / MAX (self->cell_width, 1), 1, G_MAXUINT16);
	uint16_t rows = (uint16_t) CLAMP (height / MAX (self->cell_height, 1), 1, G_MAXUINT16);
	if (cols == self->cols && rows == self->rows)
		return;
	self->cols = cols;
	self->rows = rows;
	if (self->terminal != NULL)
		ghostty_terminal_resize (self->terminal, cols, rows,
		                       (uint32_t) self->cell_width,
		                       (uint32_t) self->cell_height);
	if (self->pty_fd >= 0)
	{
		size.ws_col = cols;
		size.ws_row = rows;
		size.ws_xpixel = (unsigned short) MIN (width, G_MAXUINT16);
		size.ws_ypixel = (unsigned short) MIN (height, G_MAXUINT16);
		ioctl (self->pty_fd, TIOCSWINSZ, &size);
	}
	terminal_refresh (self);
}

/* ---- keyboard --------------------------------------------------------- */

static GhosttyKey
terminal_ghostty_key (guint keyval)
{
	switch (keyval)
	{
	case GDK_KEY_Return: case GDK_KEY_KP_Enter: return GHOSTTY_KEY_ENTER;
	case GDK_KEY_BackSpace: return GHOSTTY_KEY_BACKSPACE;
	case GDK_KEY_Tab: return GHOSTTY_KEY_TAB;
	case GDK_KEY_ISO_Left_Tab: return GHOSTTY_KEY_TAB;
	case GDK_KEY_Escape: return GHOSTTY_KEY_ESCAPE;
	case GDK_KEY_Up: return GHOSTTY_KEY_ARROW_UP;
	case GDK_KEY_Down: return GHOSTTY_KEY_ARROW_DOWN;
	case GDK_KEY_Right: return GHOSTTY_KEY_ARROW_RIGHT;
	case GDK_KEY_Left: return GHOSTTY_KEY_ARROW_LEFT;
	case GDK_KEY_Home: return GHOSTTY_KEY_HOME;
	case GDK_KEY_End: return GHOSTTY_KEY_END;
	case GDK_KEY_Delete: return GHOSTTY_KEY_DELETE;
	case GDK_KEY_Insert: return GHOSTTY_KEY_INSERT;
	case GDK_KEY_Page_Up: return GHOSTTY_KEY_PAGE_UP;
	case GDK_KEY_Page_Down: return GHOSTTY_KEY_PAGE_DOWN;
	case GDK_KEY_F1: return GHOSTTY_KEY_F1;
	case GDK_KEY_F2: return GHOSTTY_KEY_F2;
	case GDK_KEY_F3: return GHOSTTY_KEY_F3;
	case GDK_KEY_F4: return GHOSTTY_KEY_F4;
	case GDK_KEY_F5: return GHOSTTY_KEY_F5;
	case GDK_KEY_F6: return GHOSTTY_KEY_F6;
	case GDK_KEY_F7: return GHOSTTY_KEY_F7;
	case GDK_KEY_F8: return GHOSTTY_KEY_F8;
	case GDK_KEY_F9: return GHOSTTY_KEY_F9;
	case GDK_KEY_F10: return GHOSTTY_KEY_F10;
	case GDK_KEY_F11: return GHOSTTY_KEY_F11;
	case GDK_KEY_F12: return GHOSTTY_KEY_F12;
	default: return GHOSTTY_KEY_UNIDENTIFIED;
	}
}

#define TERMAC_INTERRUPT_WINDOW_US (1000 * 1000)

/* Ctrl+C copies an active selection; without one, two presses within a second
 * send the interrupt to the running program. */
void
termac_terminal_copy_or_interrupt (TermacTerminal *self)
{
	gint64 now = g_get_monotonic_time ();
	char interrupt = 0x03;

	if (termac_terminal_copy (self))
	{
		self->interrupt_armed_us = 0;
		return;
	}
	if (self->interrupt_armed_us != 0 &&
	    now - self->interrupt_armed_us <= TERMAC_INTERRUPT_WINDOW_US)
	{
		self->interrupt_armed_us = 0;
		terminal_send_input (self, &interrupt, 1);
		return;
	}
	self->interrupt_armed_us = now;
}

void
termac_terminal_select_all (TermacTerminal *self)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	termac_vt_select_all (self->terminal);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
terminal_shortcut_reserved (TermacTerminal *self,
                            guint           keyval,
                            GdkModifierType state)
{
	GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

	return TERMAC_IS_WINDOW (root) &&
	       termac_window_shortcut_is_reserved (TERMAC_WINDOW (root), keyval, state);
}

/* Encodes a named key with the terminal's current modes and sends it to the
 * shell. Returns FALSE when the key has no named encoding. */
static gboolean
terminal_send_key (TermacTerminal *self, guint keyval, GdkModifierType state)
{
	GhosttyKey key = terminal_ghostty_key (keyval);
	char encoded[128];
	size_t encoded_len = 0;

	if (key == GHOSTTY_KEY_UNIDENTIFIED || self->key_encoder == NULL || self->key_event == NULL)
		return FALSE;
	ghostty_key_encoder_setopt_from_terminal (self->key_encoder, self->terminal);
	ghostty_key_event_set_action (self->key_event, GHOSTTY_KEY_ACTION_PRESS);
	ghostty_key_event_set_key (self->key_event, key);
	ghostty_key_event_set_mods (self->key_event, terminal_ghostty_mods (state));
	ghostty_key_event_set_utf8 (self->key_event, NULL, 0);
	if (ghostty_key_encoder_encode (self->key_encoder, self->key_event,
	                                encoded, sizeof encoded, &encoded_len) == GHOSTTY_SUCCESS &&
	    encoded_len > 0)
		terminal_send_input (self, encoded, encoded_len);
	return TRUE;
}

/* Moves the cursor to the start of the line, as the Home key does. */
void
termac_terminal_line_start (TermacTerminal *self)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	terminal_send_key (self, GDK_KEY_Home, 0);
}

static gboolean
terminal_key_pressed (GtkEventControllerKey *controller,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      gpointer               user_data)
{
	TermacTerminal *self = user_data;
	GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
	gboolean ctrl = (state & GDK_CONTROL_MASK) != 0;
	gboolean shift = (state & GDK_SHIFT_MASK) != 0;
	gboolean alt = (state & GDK_ALT_MASK) != 0;
	gunichar ch;
	char utf8[7] = { 0 };
	(void) keycode;

	if (self->error != NULL)
	{
		if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
			termac_terminal_retry (self);
		return terminal_shortcut_reserved (self, keyval, state) ? GDK_EVENT_PROPAGATE : GDK_EVENT_STOP;
	}
	if (gtk_im_context_filter_keypress (self->im_context, event))
		return GDK_EVENT_STOP;
	if (terminal_shortcut_reserved (self, keyval, state))
		return GDK_EVENT_PROPAGATE;

	if (shift && !ctrl && !alt)
	{
		switch (keyval)
		{
		case GDK_KEY_Page_Up:
			termac_terminal_scroll_pages (self, -1);
			return GDK_EVENT_STOP;
		case GDK_KEY_Page_Down:
			termac_terminal_scroll_pages (self, 1);
			return GDK_EVENT_STOP;
		case GDK_KEY_Home:
			ghostty_terminal_scroll_viewport (self->terminal,
			    (GhosttyTerminalScrollViewport) { .tag = GHOSTTY_SCROLL_VIEWPORT_TOP });
			gtk_widget_queue_draw (GTK_WIDGET (self));
			return GDK_EVENT_STOP;
		case GDK_KEY_End:
			termac_terminal_scroll_to_bottom (self);
			return GDK_EVENT_STOP;
		default:
			break;
		}
	}

	if (terminal_send_key (self, keyval, state))
		return GDK_EVENT_STOP;

	if (ctrl)
	{
		guint lower = gdk_keyval_to_lower (keyval);
		char control = 0;

		if (lower >= GDK_KEY_a && lower <= GDK_KEY_z)
			control = (char) (lower - GDK_KEY_a + 1);
		else if (keyval == GDK_KEY_space)
			control = 0;
		else
			return GDK_EVENT_PROPAGATE;
		if (alt)
		{
			char pair[2] = { 0x1b, control };
			terminal_send_input (self, pair, 2);
		}
		else
			terminal_send_input (self, &control, 1);
		return GDK_EVENT_STOP;
	}

	ch = gdk_keyval_to_unicode (keyval);
	if (ch != 0)
	{
		gsize utf8_len;
		if (alt)
			terminal_send_input (self, "\x1b", 1);
		utf8_len = g_unichar_to_utf8 (ch, utf8);
		terminal_send_input (self, utf8, utf8_len);
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}

static void
terminal_realize (GtkWidget *widget)
{
	TermacTerminal *self = TERMAC_TERMINAL (widget);

	if (self->im_context != NULL)
		gtk_im_context_set_client_widget (self->im_context, widget);
}

static void
terminal_unrealize (GtkWidget *widget)
{
	TermacTerminal *self = TERMAC_TERMINAL (widget);

	if (self->im_context != NULL)
		gtk_im_context_set_client_widget (self->im_context, NULL);
}

static void
terminal_focus_enter (GtkEventControllerFocus *controller,
                      gpointer                 user_data)
{
	TermacTerminal *self = user_data;
	char report[8];
	size_t written = 0;

	(void) controller;
	gtk_im_context_focus_in (self->im_context);
	if (terminal_mode_enabled (self, GHOSTTY_MODE_FOCUS_EVENT) &&
	    ghostty_focus_encode (GHOSTTY_FOCUS_GAINED, report, sizeof report, &written) == GHOSTTY_SUCCESS)
		terminal_queue_bytes (self, (const uint8_t *) report, written);
}

static void
terminal_focus_leave (GtkEventControllerFocus *controller,
                      gpointer                 user_data)
{
	TermacTerminal *self = user_data;
	char report[8];
	size_t written = 0;

	(void) controller;
	gtk_im_context_focus_out (self->im_context);
	if (terminal_mode_enabled (self, GHOSTTY_MODE_FOCUS_EVENT) &&
	    ghostty_focus_encode (GHOSTTY_FOCUS_LOST, report, sizeof report, &written) == GHOSTTY_SUCCESS)
		terminal_queue_bytes (self, (const uint8_t *) report, written);
}

static void
terminal_commit_text (GtkIMContext *context,
                      const char   *text,
                      gpointer      user_data)
{
	TermacTerminal *self = user_data;
	(void) context;
	terminal_send_input (self, text, strlen (text));
}

/* Sets the text being composed, shown at the cursor until it is committed.
 * NULL or an empty string clears it. */
void
termac_terminal_set_preedit (TermacTerminal *self,
                             const char     *text)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	g_clear_pointer (&self->preedit, g_free);
	if (text != NULL && *text != '\0')
		self->preedit = g_strdup (text);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
terminal_preedit_changed (GtkIMContext *context,
                          gpointer      user_data)
{
	TermacTerminal *self = user_data;
	g_autofree char *text = NULL;

	gtk_im_context_get_preedit_string (context, &text, NULL, NULL);
	termac_terminal_set_preedit (self, text);
}

const char *
termac_terminal_get_preedit (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), "");
	return self->preedit != NULL ? self->preedit : "";
}

/* ---- mouse ------------------------------------------------------------ */

static GhosttyMouseButton
terminal_ghostty_button (guint button)
{
	switch (button)
	{
	case GDK_BUTTON_PRIMARY: return GHOSTTY_MOUSE_BUTTON_LEFT;
	case GDK_BUTTON_MIDDLE: return GHOSTTY_MOUSE_BUTTON_MIDDLE;
	case GDK_BUTTON_SECONDARY: return GHOSTTY_MOUSE_BUTTON_RIGHT;
	default: return GHOSTTY_MOUSE_BUTTON_UNKNOWN;
	}
}

static gboolean
terminal_mouse_reporting (TermacTerminal *self,
                          GdkModifierType state)
{
	return !(state & GDK_SHIFT_MASK) && termac_vt_mouse_tracking (self->terminal);
}

/* Sends a mouse report to the program when it has enabled mouse tracking. */
static void
terminal_send_mouse (TermacTerminal    *self,
                     GhosttyMouseAction action,
                     GhosttyMouseButton button,
                     GdkModifierType    state,
                     double             x,
                     double             y)
{
	GhosttyMouseEncoderSize size = GHOSTTY_INIT_SIZED (GhosttyMouseEncoderSize);
	GhosttyMousePosition position;
	char report[64];
	size_t written = 0;
	bool any_pressed = self->mouse_button != 0;

	if (self->mouse_encoder == NULL || self->mouse_event == NULL)
		return;
	size.screen_width = (uint32_t) MAX (self->surface_width, 1);
	size.screen_height = (uint32_t) MAX (self->surface_height, 1);
	size.cell_width = (uint32_t) self->cell_width;
	size.cell_height = (uint32_t) self->cell_height;
	size.padding_left = TERMAC_TERMINAL_INSET_START;
	size.padding_top = TERMAC_TERMINAL_INSET_TOP;

	ghostty_mouse_encoder_setopt_from_terminal (self->mouse_encoder, self->terminal);
	ghostty_mouse_encoder_setopt (self->mouse_encoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &size);
	ghostty_mouse_encoder_setopt (self->mouse_encoder, GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED, &any_pressed);
	ghostty_mouse_event_set_action (self->mouse_event, action);
	if (button != GHOSTTY_MOUSE_BUTTON_UNKNOWN)
		ghostty_mouse_event_set_button (self->mouse_event, button);
	else
		ghostty_mouse_event_clear_button (self->mouse_event);
	ghostty_mouse_event_set_mods (self->mouse_event, terminal_ghostty_mods (state));
	position.x = (float) x;
	position.y = (float) y;
	ghostty_mouse_event_set_position (self->mouse_event, position);
	if (ghostty_mouse_encoder_encode (self->mouse_encoder, self->mouse_event,
	                                  report, sizeof report, &written) == GHOSTTY_SUCCESS)
		terminal_queue_bytes (self, (const uint8_t *) report, written);
}

static void
terminal_selection_event (TermacTerminal                   *self,
                          GhosttySelectionGestureEventType  type,
                          double                            x,
                          double                            y)
{
	guint column;
	guint row;
	GhosttySelectionGestureGeometry geometry = { 0 };
	gint64 now_ns = terminal_now_us () * 1000;

	terminal_cell_at (self, x, y, &column, &row);
	geometry.columns = self->cols;
	geometry.cell_width = (uint32_t) self->cell_width;
	geometry.padding_left = TERMAC_TERMINAL_INSET_START;
	geometry.screen_height = (uint32_t) MAX (self->surface_height, 1);
	termac_vt_selector_event (self->selector, self->terminal, type, column, row, x, y,
	                          (guint64) now_ns,
	                          type == GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG ? &geometry : NULL);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
terminal_open_url_at (TermacTerminal *self,
                      double          x,
                      double          y)
{
	guint column;
	guint row;
	g_autofree char *url = NULL;
	GtkRoot *root;

	terminal_cell_at (self, x, y, &column, &row);
	url = termac_vt_link_at (self->terminal, self->cols, row, column, NULL, NULL);
	if (url == NULL) return FALSE;
	{
		GtkUriLauncher *launcher = gtk_uri_launcher_new (url);

		root = gtk_widget_get_root (GTK_WIDGET (self));
		gtk_uri_launcher_launch (launcher, GTK_IS_WINDOW (root) ? GTK_WINDOW (root) : NULL,
		                         NULL, NULL, NULL);
		g_object_unref (launcher);
	}
	return TRUE;
}

static void
terminal_click_pressed (GtkGestureClick *gesture,
                        int              n_press,
                        double           x,
                        double           y,
                        gpointer         user_data)
{
	TermacTerminal *self = user_data;
	GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
	guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	(void) n_press;

	gtk_widget_grab_focus (GTK_WIDGET (self));
	self->pointer_x = x;
	self->pointer_y = y;
	if (self->error != NULL)
	{
		termac_terminal_retry (self);
		return;
	}
	if (button == GDK_BUTTON_PRIMARY && (state & GDK_CONTROL_MASK) &&
	    terminal_open_url_at (self, x, y))
		return;
	if (terminal_mouse_reporting (self, state))
	{
		self->mouse_button = button;
		terminal_send_mouse (self, GHOSTTY_MOUSE_ACTION_PRESS,
		                     terminal_ghostty_button (button), state, x, y);
		return;
	}
	if (button != GDK_BUTTON_PRIMARY)
		return;
	self->selecting = TRUE;
	terminal_selection_event (self, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS, x, y);
}

static void
terminal_click_released (GtkGestureClick *gesture,
                         int              n_press,
                         double           x,
                         double           y,
                         gpointer         user_data)
{
	TermacTerminal *self = user_data;
	GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
	guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	(void) n_press;

	if (self->mouse_button != 0 && button == self->mouse_button)
	{
		terminal_send_mouse (self, GHOSTTY_MOUSE_ACTION_RELEASE,
		                     terminal_ghostty_button (button), state, x, y);
		self->mouse_button = 0;
		return;
	}
	if (self->selecting && button == GDK_BUTTON_PRIMARY)
	{
		self->selecting = FALSE;
		terminal_selection_event (self, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE, x, y);
	}
}

static void
terminal_clear_link_hover (TermacTerminal *self)
{
	if (!self->hover_link)
		return;
	self->hover_link = FALSE;
	gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
	gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* With Ctrl held, a link under the pointer gets a pointer cursor and an underline. */
static void
terminal_update_link_hover (TermacTerminal *self,
                            double          x,
                            double          y,
                            GdkModifierType state)
{
	guint column = 0;
	guint row = 0;
	guint first = 0;
	guint last = 0;
	g_autofree char *uri = NULL;

	if ((state & GDK_CONTROL_MASK) == 0 || self->error != NULL || self->terminal == NULL)
	{
		terminal_clear_link_hover (self);
		return;
	}
	terminal_cell_at (self, x, y, &column, &row);
	uri = termac_vt_link_at (self->terminal, self->cols, row, column, &first, &last);
	if (uri == NULL)
	{
		terminal_clear_link_hover (self);
		return;
	}
	if (!self->hover_link)
		gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "pointer");
	gtk_widget_set_tooltip_text (GTK_WIDGET (self), uri);
	self->hover_link = TRUE;
	self->hover_row = row;
	self->hover_first = first;
	self->hover_last = last;
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
terminal_pointer_leave (GtkEventControllerMotion *controller,
                        gpointer                  user_data)
{
	(void) controller;
	terminal_clear_link_hover (user_data);
}

static void
terminal_pointer_motion (GtkEventControllerMotion *controller,
                         double                    x,
                         double                    y,
                         gpointer                  user_data)
{
	TermacTerminal *self = user_data;
	GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (controller));

	self->pointer_x = x;
	self->pointer_y = y;
	terminal_update_link_hover (self, x, y, state);
	if (terminal_mouse_reporting (self, state))
	{
		if (self->mouse_button != 0)
			terminal_send_mouse (self, GHOSTTY_MOUSE_ACTION_MOTION,
			                     terminal_ghostty_button (self->mouse_button), state, x, y);
		return;
	}
	if (self->selecting)
		terminal_selection_event (self, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG, x, y);
}

static gboolean
terminal_scroll (GtkEventControllerScroll *controller,
                 double                    dx,
                 double                    dy,
                 gpointer                  user_data)
{
	TermacTerminal *self = user_data;
	GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (controller));
	(void) dx;

	if (dy == 0)
		return GDK_EVENT_PROPAGATE;
	if (terminal_mouse_reporting (self, state))
	{
		GhosttyMouseButton button = dy < 0 ? GHOSTTY_MOUSE_BUTTON_FOUR : GHOSTTY_MOUSE_BUTTON_FIVE;
		int count = MAX (1, (int) (ABS (dy) + 0.5));
		int i;
		for (i = 0; i < count; i++)
			terminal_send_mouse (self, GHOSTTY_MOUSE_ACTION_PRESS, button, state,
			                     self->pointer_x, self->pointer_y);
		return GDK_EVENT_STOP;
	}
	{
		intptr_t lines = (intptr_t) (dy * TERMAC_TERMINAL_WHEEL_LINES);
		if (lines == 0)
			lines = dy < 0 ? -1 : 1;
		ghostty_terminal_scroll_viewport (self->terminal,
		    (GhosttyTerminalScrollViewport) {
		        .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
		        .value.delta = lines,
		    });
		gtk_widget_queue_draw (GTK_WIDGET (self));
	}
	return GDK_EVENT_STOP;
}

void
termac_terminal_set_agent (TermacTerminal *self, const TermacAgent *agent)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	self->agent = agent != NULL ? agent : termac_agent_default ();
}

/* ---- clipboard, scroll, search, attachments --------------------------- */

guint
termac_terminal_get_font_size (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), 0);
	return self->font_size;
}

guint
termac_terminal_get_columns (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), 0);
	return self->cols;
}

guint
termac_terminal_get_rows (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), 0);
	return self->rows;
}

void
termac_terminal_get_cell_size (TermacTerminal *self, guint *width, guint *height)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	if (width != NULL) *width = self->cell_width;
	if (height != NULL) *height = self->cell_height;
}

gpointer
termac_terminal_get_vt_handle (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), NULL);
	return self->terminal;
}

gboolean
termac_terminal_copy (TermacTerminal *self)
{
	g_autofree char *text = NULL;

	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), FALSE);
	if (!termac_vt_selection_text (self->terminal, &text) || *text == '\0')
		return FALSE;
	gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), text);
	return TRUE;
}

static void
terminal_send_paste (TermacTerminal *self,
                     const char     *text,
                     gsize           length)
{
	gsize encoded_length = 0;
	g_autofree char *encoded = NULL;
	gboolean bracketed = termac_vt_bracketed_paste (self->terminal);

	encoded = termac_vt_encode_paste (text, length, bracketed, &encoded_length);
	if (encoded != NULL)
		terminal_send_input (self, encoded, encoded_length);
}

static void
terminal_paste_read (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TermacTerminal *self = TERMAC_TERMINAL (user_data);
	g_autofree char *text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source), result, NULL);

	if (text != NULL && !self->closing)
		terminal_send_paste (self, text, strlen (text));
	g_object_unref (self);
}

/* Stores an image from the clipboard as an attachment and types its path
 * into the prompt, without submitting it. */
static void
terminal_attach_texture (TermacTerminal *self,
                         GdkTexture     *texture)
{
	g_autoptr(GBytes) png = gdk_texture_save_to_png_bytes (texture);
	g_autoptr(GError) error = NULL;
	gsize length = 0;
	const guint8 *data = g_bytes_get_data (png, &length);
	g_autofree char *stored = termac_attachments_store (self->session_id, data, length, &error);
	g_autofree char *inserted = NULL;

	if (stored == NULL)
	{
		g_signal_emit (self, terminal_signals[SIGNAL_MESSAGE], 0, error->message);
		return;
	}
	termac_terminal_add_attachment (self, stored);
	inserted = termac_agent_format_attachment (self->agent, stored);
	terminal_send_paste (self, inserted, strlen (inserted));
	g_signal_emit (self, terminal_signals[SIGNAL_MESSAGE], 0, _("Image attached to the prompt"));
}

static void
terminal_paste_texture_read (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	TermacTerminal *self = TERMAC_TERMINAL (user_data);
	g_autoptr(GdkTexture) texture = gdk_clipboard_read_texture_finish (GDK_CLIPBOARD (source), result, NULL);

	if (texture != NULL && !self->closing)
		terminal_attach_texture (self, texture);
	g_object_unref (self);
}

/* Pastes text, or attaches the image when the clipboard holds only an image. */
void
termac_terminal_paste (TermacTerminal *self)
{
	GdkClipboard *clipboard;
	GdkContentFormats *formats;

	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
	formats = gdk_clipboard_get_formats (clipboard);
	if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_TEXTURE) &&
	    !gdk_content_formats_contain_gtype (formats, G_TYPE_STRING))
	{
		gdk_clipboard_read_texture_async (clipboard, NULL,
		                                  terminal_paste_texture_read, g_object_ref (self));
		return;
	}
	gdk_clipboard_read_text_async (clipboard, NULL, terminal_paste_read, g_object_ref (self));
}

void
termac_terminal_insert_text (TermacTerminal *self,
                             const char     *text)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	g_return_if_fail (text != NULL);
	terminal_send_paste (self, text, strlen (text));
}

void
termac_terminal_scroll_pages (TermacTerminal *self,
                              gint            pages)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	ghostty_terminal_scroll_viewport (self->terminal,
	    (GhosttyTerminalScrollViewport) {
	        .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
	        .value.delta = (intptr_t) pages * MAX (self->rows - 1, 1),
	    });
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
termac_terminal_scroll_to_bottom (TermacTerminal *self)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	ghostty_terminal_scroll_viewport (self->terminal,
	    (GhosttyTerminalScrollViewport) { .tag = GHOSTTY_SCROLL_VIEWPORT_BOTTOM });
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
termac_terminal_set_highlight (TermacTerminal *self, const char *needle)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	if (g_strcmp0 (self->highlight, needle) != 0) {
		g_free (self->highlight);
		self->highlight = g_strdup (needle);
		self->highlight_has_current = FALSE;
	}
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

guint
termac_terminal_get_highlight_count (TermacTerminal *self)
{
	g_autoptr(GArray) matches = NULL;
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), 0);
	matches = termac_vt_find_matches (self->terminal, self->cols, self->rows, self->highlight);
	return matches->len;
}

gboolean
termac_terminal_find (TermacTerminal *self, const char *text, gboolean backwards)
{
	g_autoptr(GArray) matches = NULL;
	GhosttyTerminalScrollbar bar = { 0 };
	guint64 target = 0;
	int current = -1, next;
	gboolean found;

	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), FALSE);
	termac_terminal_set_highlight (self, text);
	matches = termac_vt_find_matches (self->terminal, self->cols, self->rows, text);
	ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar);
	for (guint i = 0; self->highlight_has_current && i < matches->len; i++) {
		TermacVtMatch m = g_array_index (matches, TermacVtMatch, i);
		if (m.row + bar.offset == self->highlight_row && m.column == self->highlight_column)
			current = i;
	}
	next = current + (backwards ? -1 : 1);
	if (current >= 0 && next >= 0 && next < (int) matches->len) {
		TermacVtMatch m = g_array_index (matches, TermacVtMatch, next);
		self->highlight_row = m.row + bar.offset;
		self->highlight_column = m.column;
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return TRUE;
	}
	found = text != NULL && termac_vt_find_target (self->terminal, text, backwards,
	    self->highlight_has_current ? &self->highlight_row : NULL, &target);
	self->highlight_has_current = FALSE;
	g_clear_pointer (&matches, g_array_unref);
	matches = termac_vt_find_matches (self->terminal, self->cols, self->rows, text);
	ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar);
	for (guint i = 0; found && i < matches->len; i++) {
		TermacVtMatch m = g_array_index (matches, TermacVtMatch, i);
		if (m.row + bar.offset != target) continue;
		self->highlight_has_current = TRUE;
		self->highlight_row = target;
		self->highlight_column = m.column;
		if (!backwards) break;
	}
	gtk_widget_queue_draw (GTK_WIDGET (self));
	return found;
}

const char *
termac_terminal_get_session_id (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), NULL);
	return self->session_id;
}

const char *
termac_terminal_get_cwd (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), NULL);
	/* The shell's own report (OSC 7) wins; without one, ask the kernel where the
	 * shell is, so a tab that never reported a folder still restores to it. */
	if (self->cwd != NULL)
		return self->cwd;
	/* /proc below describes flatpak-spawn, not the remote shell. OSC 7
	 * remains authoritative; otherwise retain the session's initial folder. */
	if (self->host_shell)
		return self->initial_cwd != NULL ? self->initial_cwd : g_get_home_dir ();
	if (self->child_pid > 0 && !self->child_exited)
	{
		g_autofree char *link = g_strdup_printf ("/proc/%d/cwd", (int) self->child_pid);
		char *target = g_file_read_link (link, NULL);

		if (target != NULL)
		{
			g_free (self->proc_cwd);
			self->proc_cwd = target;
			return self->proc_cwd;
		}
	}
	return NULL;
}

const char *
termac_terminal_get_error (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), NULL);
	return self->error;
}

void
termac_terminal_retry (TermacTerminal *self)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	if (self->child_pid > 0 && !self->child_exited)
		return;
	g_clear_pointer (&self->error, g_free);
	if (!terminal_create_engine (self))
		return;
	terminal_spawn (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
termac_terminal_add_attachment (TermacTerminal *self,
                                const char     *path)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	g_return_if_fail (path != NULL);
	g_ptr_array_add (self->attachments, g_strdup (path));
}

GPtrArray *
termac_terminal_get_attachments (TermacTerminal *self)
{
	g_return_val_if_fail (TERMAC_IS_TERMINAL (self), NULL);
	return self->attachments;
}

void
termac_terminal_remove_attachment (TermacTerminal *self,
                                   guint           index)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	if (index < self->attachments->len)
		g_ptr_array_remove_index (self->attachments, index);
}

/* ---- object lifecycle ------------------------------------------------- */

static void
terminal_dispose (GObject *object)
{
	TermacTerminal *self = TERMAC_TERMINAL (object);

	self->closing = TRUE;
	if (self->io_source != 0) g_source_remove (self->io_source);
	if (self->write_source != 0) g_source_remove (self->write_source);
	if (self->job_source != 0) g_source_remove (self->job_source);
	self->io_source = self->write_source = self->job_source = 0;
	terminal_stop_child (self);
	if (self->pty_fd >= 0) close (self->pty_fd);
	self->pty_fd = -1;
	if (self->im_context != NULL)
	{
		gtk_im_context_set_client_widget (self->im_context, NULL);
		g_clear_object (&self->im_context);
	}
	g_clear_pointer (&self->preedit, g_free);
	if (self->key_event != NULL) ghostty_key_event_free (self->key_event);
	if (self->key_encoder != NULL) ghostty_key_encoder_free (self->key_encoder);
	if (self->mouse_event != NULL) ghostty_mouse_event_free (self->mouse_event);
	if (self->mouse_encoder != NULL) ghostty_mouse_encoder_free (self->mouse_encoder);
	if (self->cells != NULL) ghostty_render_state_row_cells_free (self->cells);
	if (self->row_iterator != NULL) ghostty_render_state_row_iterator_free (self->row_iterator);
	if (self->render_state != NULL) ghostty_render_state_free (self->render_state);
	if (self->terminal != NULL) ghostty_terminal_free (self->terminal);
	self->key_event = NULL;
	self->key_encoder = NULL;
	self->mouse_event = NULL;
	self->mouse_encoder = NULL;
	self->cells = NULL;
	self->row_iterator = NULL;
	self->render_state = NULL;
	self->terminal = NULL;
	termac_vt_selector_free (self->selector);
	self->selector = NULL;
	g_clear_pointer (&self->write_queue, g_byte_array_unref);
	g_clear_pointer (&self->write_backlog, g_byte_array_unref);
	g_clear_pointer (&self->attachments, g_ptr_array_unref);
	g_clear_pointer (&self->error, g_free);
	g_clear_pointer (&self->cwd, g_free);
	g_clear_pointer (&self->proc_cwd, g_free);
	g_clear_pointer (&self->initial_cwd, g_free);
	g_clear_pointer (&self->initial_command, g_free);
	g_clear_pointer (&self->session_id, g_free);
	g_clear_pointer (&self->highlight, g_free);
	if (self->accessible_text)
	{
		g_string_free (self->accessible_text, TRUE);
		self->accessible_text = NULL;
	}
	g_clear_pointer (&self->accessible_selection, g_bytes_unref);
	G_OBJECT_CLASS (termac_terminal_parent_class)->dispose (object);
}

static void
termac_terminal_class_init (TermacTerminalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = terminal_dispose;
	gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TERMINAL);

	terminal_signals[SIGNAL_TITLE_CHANGED] = g_signal_new (
		"title-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	terminal_signals[SIGNAL_CWD_CHANGED] = g_signal_new (
		"cwd-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	terminal_signals[SIGNAL_BELL] = g_signal_new (
		"bell", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 0);
	terminal_signals[SIGNAL_OUTPUT] = g_signal_new (
		"output", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 0);
	terminal_signals[SIGNAL_COMMAND_FINISHED] = g_signal_new (
		"command-finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);
	terminal_signals[SIGNAL_JOB_FINISHED] = g_signal_new (
		"job-finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT64);
	terminal_signals[SIGNAL_MESSAGE] = g_signal_new (
		"message", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
termac_terminal_init (TermacTerminal *self)
{
	GtkEventController *controller;
	GtkEventController *focus_controller;
	GtkEventController *motion_controller;
	GtkEventController *scroll_controller;
	GtkGesture *click_gesture;
	PangoContext *context;
	PangoFontDescription *font;
	PangoFontMetrics *metrics;

	self->pty_fd = -1;
	self->font_size = 11;
	self->scrollback_lines = TERMAC_TERMINAL_DEFAULT_SCROLLBACK;
	self->cols = 80;
	self->rows = 24;
	self->session_id = g_uuid_string_random ();
	self->write_queue = g_byte_array_new ();
	self->write_backlog = g_byte_array_new ();
	self->attachments = g_ptr_array_new_with_free_func (g_free);
	self->agent = termac_agent_default ();
	self->selector = termac_vt_selector_new ();
	context = gtk_widget_get_pango_context (GTK_WIDGET (self));
	font = pango_font_description_new ();
	pango_font_description_set_family (font, "Monospace");
	pango_font_description_set_size (font, (int) self->font_size * PANGO_SCALE);
	metrics = pango_context_get_metrics (context, font, pango_context_get_language (context));
	self->cell_width = MAX (1, PANGO_PIXELS_CEIL (pango_font_metrics_get_approximate_char_width (metrics)));
	self->cell_height = MAX (1, PANGO_PIXELS_CEIL (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)));
	pango_font_metrics_unref (metrics);
	pango_font_description_free (font);

	self->im_context = gtk_im_multicontext_new ();
	/* The client widget is set only while realized: keeping it set would make the
	 * input context and the terminal hold each other, so a closed tab never freed. */
	g_signal_connect (self->im_context, "commit", G_CALLBACK (terminal_commit_text), self);
	g_signal_connect (self->im_context, "preedit-changed", G_CALLBACK (terminal_preedit_changed), self);
	g_signal_connect (self, "realize", G_CALLBACK (terminal_realize), NULL);
	g_signal_connect (self, "unrealize", G_CALLBACK (terminal_unrealize), NULL);
	controller = gtk_event_controller_key_new ();
	g_signal_connect (controller, "key-pressed", G_CALLBACK (terminal_key_pressed), self);
	gtk_widget_add_controller (GTK_WIDGET (self), controller);
	focus_controller = gtk_event_controller_focus_new ();
	g_signal_connect (focus_controller, "enter", G_CALLBACK (terminal_focus_enter), self);
	g_signal_connect (focus_controller, "leave", G_CALLBACK (terminal_focus_leave), self);
	gtk_widget_add_controller (GTK_WIDGET (self), focus_controller);
	click_gesture = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), 0);
	g_signal_connect (click_gesture, "pressed", G_CALLBACK (terminal_click_pressed), self);
	g_signal_connect (click_gesture, "released", G_CALLBACK (terminal_click_released), self);
	gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (click_gesture));
	motion_controller = gtk_event_controller_motion_new ();
	g_signal_connect (motion_controller, "motion", G_CALLBACK (terminal_pointer_motion), self);
	g_signal_connect (motion_controller, "leave", G_CALLBACK (terminal_pointer_leave), self);
	gtk_widget_add_controller (GTK_WIDGET (self), motion_controller);
	scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect (scroll_controller, "scroll", G_CALLBACK (terminal_scroll), self);
	gtk_widget_add_controller (GTK_WIDGET (self), scroll_controller);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self), terminal_draw, self, NULL);
	gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
	gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
	gtk_widget_set_size_request (GTK_WIDGET (self), 320, 200);
	gtk_accessible_update_property (GTK_ACCESSIBLE (self),
	                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Terminal"), -1);
}

GtkWidget *
termac_terminal_new_command (const char *cwd,
                             gboolean shell_integration,
                             const char *command)
{
	TermacTerminal *self = g_object_new (TERMAC_TYPE_TERMINAL, NULL);

	self->initial_cwd = g_strdup (cwd);
	self->initial_command = g_strdup (command);
	self->shell_integration = shell_integration;
	if (terminal_create_engine (self))
		terminal_spawn (self);
	return GTK_WIDGET (self);
}

void
termac_terminal_set_scrollback_lines (TermacTerminal *self,
                                      guint           lines)
{
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	self->scrollback_lines = lines;
	terminal_apply_scrollback (self);
}

void
termac_terminal_set_font_size (TermacTerminal *self, guint size)
{
	PangoContext *context;
	PangoFontDescription *font;
	PangoFontMetrics *metrics;
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	size = CLAMP (size, 8, 32);
	if (self->font_size == size)
		return;
	self->font_size = size;
	context = gtk_widget_get_pango_context (GTK_WIDGET (self));
	font = pango_font_description_new ();
	pango_font_description_set_family (font, "Monospace");
	pango_font_description_set_size (font, (int) size * PANGO_SCALE);
	metrics = pango_context_get_metrics (context, font, pango_context_get_language (context));
	self->cell_width = MAX (1, PANGO_PIXELS_CEIL (pango_font_metrics_get_approximate_char_width (metrics)));
	self->cell_height = MAX (1, PANGO_PIXELS_CEIL (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)));
	pango_font_metrics_unref (metrics);
	pango_font_description_free (font);
	gtk_widget_queue_resize (GTK_WIDGET (self));
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

#define RGB(r_, g_, b_) { (r_), (g_), (b_) }
typedef struct
{
	const char *name;
	GhosttyColorRgb foreground;
	GhosttyColorRgb background;
	GhosttyColorRgb colors[16];
} TermacPalette;

static const TermacPalette terminal_palettes[] = {
	{ "gnome", RGB (246, 245, 244), RGB (30, 30, 30), {
		RGB (23, 20, 33), RGB (192, 28, 40), RGB (38, 162, 105), RGB (229, 165, 10),
		RGB (26, 95, 180), RGB (145, 65, 172), RGB (14, 165, 233), RGB (211, 218, 227),
		RGB (94, 92, 100), RGB (246, 97, 81), RGB (143, 240, 164), RGB (248, 228, 92),
		RGB (120, 174, 237), RGB (192, 97, 203), RGB (77, 208, 225), RGB (255, 255, 255) } },
	{ "campbell", RGB (204, 204, 204), RGB (12, 12, 12), {
		RGB (12, 12, 12), RGB (197, 15, 31), RGB (19, 161, 14), RGB (193, 156, 0),
		RGB (0, 55, 218), RGB (136, 23, 152), RGB (58, 150, 221), RGB (204, 204, 204),
		RGB (118, 118, 118), RGB (231, 72, 86), RGB (22, 198, 12), RGB (249, 241, 165),
		RGB (59, 120, 255), RGB (180, 0, 158), RGB (97, 214, 214), RGB (242, 242, 242) } },
	{ "dracula", RGB (248, 248, 242), RGB (40, 42, 54), {
		RGB (33, 34, 44), RGB (255, 85, 85), RGB (80, 250, 123), RGB (241, 250, 140),
		RGB (189, 147, 249), RGB (255, 121, 198), RGB (139, 233, 253), RGB (248, 248, 242),
		RGB (98, 114, 164), RGB (255, 110, 110), RGB (105, 255, 148), RGB (255, 255, 165),
		RGB (214, 172, 255), RGB (255, 146, 223), RGB (164, 255, 255), RGB (255, 255, 255) } },
	{ "ubuntu", RGB (238, 232, 223), RGB (48, 10, 36), {
		RGB (1, 1, 1), RGB (222, 56, 43), RGB (57, 181, 74), RGB (255, 199, 6),
		RGB (0, 111, 184), RGB (118, 38, 113), RGB (44, 181, 233), RGB (204, 204, 204),
		RGB (128, 128, 128), RGB (255, 0, 0), RGB (0, 255, 0), RGB (255, 255, 0),
		RGB (0, 0, 255), RGB (255, 0, 255), RGB (0, 255, 255), RGB (255, 255, 255) } },
	{ "nord", RGB (216, 222, 233), RGB (46, 52, 64), {
		RGB (59, 66, 82), RGB (191, 97, 106), RGB (163, 190, 140), RGB (235, 203, 139),
		RGB (129, 161, 193), RGB (180, 142, 173), RGB (136, 192, 208), RGB (229, 233, 240),
		RGB (76, 86, 106), RGB (191, 97, 106), RGB (163, 190, 140), RGB (235, 203, 139),
		RGB (129, 161, 193), RGB (180, 142, 173), RGB (143, 188, 187), RGB (236, 239, 244) } },
	{ "solarized", RGB (131, 148, 150), RGB (0, 43, 54), {
		RGB (7, 54, 66), RGB (220, 50, 47), RGB (133, 153, 0), RGB (181, 137, 0),
		RGB (38, 139, 210), RGB (211, 54, 130), RGB (42, 161, 152), RGB (238, 232, 213),
		RGB (0, 43, 54), RGB (203, 75, 22), RGB (88, 110, 117), RGB (101, 123, 131),
		RGB (131, 148, 150), RGB (108, 113, 196), RGB (147, 161, 161), RGB (253, 246, 227) } },
	{ "high-contrast", RGB (255, 255, 255), RGB (0, 0, 0), {
		RGB (0, 0, 0), RGB (255, 64, 64), RGB (64, 255, 64), RGB (255, 255, 64),
		RGB (64, 128, 255), RGB (255, 64, 255), RGB (64, 255, 255), RGB (255, 255, 255),
		RGB (96, 96, 96), RGB (255, 96, 96), RGB (96, 255, 96), RGB (255, 255, 96),
		RGB (96, 160, 255), RGB (255, 96, 255), RGB (96, 255, 255), RGB (255, 255, 255) } },
	{ "horizon", RGB (216, 222, 229), RGB (28, 30, 38), {
		RGB (22, 23, 29), RGB (232, 102, 113), RGB (9, 151, 108), RGB (250, 196, 132),
		RGB (0, 161, 219), RGB (185, 99, 193), RGB (37, 190, 217), RGB (216, 222, 229),
		RGB (81, 83, 96), RGB (232, 102, 113), RGB (9, 151, 108), RGB (250, 196, 132),
		RGB (0, 161, 219), RGB (185, 99, 193), RGB (37, 190, 217), RGB (250, 250, 250) } },
	/* Used in place of the chosen palette while the app uses light colors:
	 * a warm paper tone with dark brown text and muted ANSI colors. */
	{ "light", RGB (60, 50, 40), RGB (245, 240, 230), {
		RGB (60, 50, 40), RGB (176, 48, 40), RGB (74, 120, 40), RGB (160, 120, 20),
		RGB (50, 90, 150), RGB (130, 70, 120), RGB (40, 120, 120), RGB (200, 190, 170),
		RGB (110, 100, 90), RGB (200, 70, 60), RGB (110, 160, 70), RGB (190, 150, 50),
		RGB (80, 120, 180), RGB (160, 100, 150), RGB (70, 150, 150), RGB (245, 240, 230) } },
	{ "linux", RGB (238, 238, 238), RGB (0, 0, 0), {
		RGB (0, 0, 0), RGB (170, 0, 0), RGB (0, 170, 0), RGB (170, 85, 0),
		RGB (0, 0, 170), RGB (170, 0, 170), RGB (0, 170, 170), RGB (170, 170, 170),
		RGB (85, 85, 85), RGB (255, 85, 85), RGB (85, 255, 85), RGB (255, 255, 85),
		RGB (85, 85, 255), RGB (255, 85, 255), RGB (85, 255, 255), RGB (255, 255, 255) } },
	{ "tango", RGB (211, 215, 207), RGB (46, 52, 54), {
		RGB (46, 52, 54), RGB (204, 0, 0), RGB (78, 154, 6), RGB (196, 160, 0),
		RGB (52, 101, 164), RGB (117, 80, 123), RGB (6, 152, 154), RGB (211, 215, 207),
		RGB (85, 87, 83), RGB (239, 41, 41), RGB (138, 226, 52), RGB (252, 233, 79),
		RGB (114, 159, 207), RGB (173, 127, 168), RGB (52, 226, 226), RGB (238, 238, 236) } },
	{ "vscode", RGB (212, 212, 212), RGB (30, 30, 30), {
		RGB (0, 0, 0), RGB (205, 49, 49), RGB (13, 188, 121), RGB (229, 229, 16),
		RGB (36, 114, 200), RGB (188, 63, 188), RGB (17, 168, 205), RGB (229, 229, 229),
		RGB (102, 102, 102), RGB (241, 76, 76), RGB (35, 209, 139), RGB (245, 245, 67),
		RGB (59, 142, 234), RGB (214, 112, 214), RGB (41, 184, 219), RGB (255, 255, 255) } },
	{ "xterm", RGB (255, 255, 255), RGB (0, 0, 0), {
		RGB (0, 0, 0), RGB (205, 0, 0), RGB (0, 205, 0), RGB (205, 205, 0),
		RGB (0, 0, 238), RGB (205, 0, 205), RGB (0, 205, 205), RGB (229, 229, 229),
		RGB (127, 127, 127), RGB (255, 0, 0), RGB (0, 255, 0), RGB (255, 255, 0),
		RGB (92, 92, 255), RGB (255, 0, 255), RGB (0, 255, 255), RGB (255, 255, 255) } },
};

gboolean
termac_terminal_palette_colors (const char *name,
                                GdkRGBA    *background,
                                GdkRGBA    *foreground)
{
	const TermacPalette *theme = NULL;
	guint i;

	g_return_val_if_fail (background != NULL && foreground != NULL, FALSE);
	for (i = 0; i < G_N_ELEMENTS (terminal_palettes); i++)
		if (g_strcmp0 (terminal_palettes[i].name, name) == 0)
			theme = &terminal_palettes[i];
	if (theme == NULL)
		return FALSE;
	background->red = theme->background.r / 255.0;
	background->green = theme->background.g / 255.0;
	background->blue = theme->background.b / 255.0;
	background->alpha = 1.0;
	foreground->red = theme->foreground.r / 255.0;
	foreground->green = theme->foreground.g / 255.0;
	foreground->blue = theme->foreground.b / 255.0;
	foreground->alpha = 1.0;
	return TRUE;
}

void
termac_terminal_set_palette (TermacTerminal *self, const char *name)
{
	const TermacPalette *theme = &terminal_palettes[0];
	GhosttyColorRgb base[256];
	GhosttyColorRgb generated[256];
	guint i;
	g_return_if_fail (TERMAC_IS_TERMINAL (self));
	if (self->terminal == NULL)
		return;
	for (i = 0; i < G_N_ELEMENTS (terminal_palettes); i++)
		if (g_strcmp0 (terminal_palettes[i].name, name) == 0)
		{
			theme = &terminal_palettes[i];
			break;
		}
	ghostty_color_palette_default (base);
	memcpy (base, theme->colors, sizeof theme->colors);
	ghostty_color_palette_generate (base, NULL, &theme->background,
	                                &theme->foreground, true, generated);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND,
	                     &theme->foreground);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND,
	                     &theme->background);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_COLOR_CURSOR,
	                     &theme->foreground);
	ghostty_terminal_set (self->terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE,
	                     generated);
	terminal_refresh (self);
}
#undef RGB

GtkWidget *
termac_terminal_new (const char *cwd, gboolean shell_integration)
{
	return termac_terminal_new_command (cwd, shell_integration, NULL);
}
