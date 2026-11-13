/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "termac-agent.h"
#include <glib/gi18n.h>
#include "termac-attachments.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const TermacAgent registry[] = {
	{ "claude", "Claude Code", "claude" },
	{ "codex", "Codex CLI", "codex" },
};
static const TermacAgent *agents[G_N_ELEMENTS (registry)];
static gsize registry_initialized;
static void
ensure_registry (void)
{
	if (g_once_init_enter (&registry_initialized))
	{
		for (guint i = 0; i < G_N_ELEMENTS (registry); i++) agents[i] = &registry[i];
		g_once_init_leave (&registry_initialized, 1);
	}
}

const TermacAgent *
termac_agent_find (const char *id)
{
	ensure_registry ();
	for (guint i = 0; i < G_N_ELEMENTS (agents); i++)
		if (g_strcmp0 (id, agents[i]->id) == 0)
			return agents[i];
	return NULL;
}

const TermacAgent *termac_agent_default (void) { return &registry[0]; }

const TermacAgent *const *
termac_agent_all (gsize *count)
{
	ensure_registry ();
	if (count != NULL) *count = G_N_ELEMENTS (agents);
	return agents;
}

char *
termac_agent_format_attachment (const TermacAgent *agent, const char *path)
{
	g_autofree char *quoted = termac_attachments_quote (path);
	/* Both initial adapters accept a quoted path. NULL is the plain-shell case. */
	(void) agent;
	return g_strconcat (quoted, " ", NULL);
}

gboolean
termac_agent_is_installed (const TermacAgent *agent)
{
	g_autofree char *program = NULL;
	if (agent == NULL) return FALSE;
	program = g_find_program_in_path (agent->binary);
	return program != NULL;
}

char *
termac_agent_build_launch_command (const TermacAgent *agent)
{
	return g_shell_quote ((agent != NULL ? agent : termac_agent_default ())->binary);
}

typedef struct {
	GSubprocess *process;
	GCancellable *cancel;
	gulong cancel_id;
	guint timeout_id;
	gboolean timed_out;
	pid_t session;
} AgentCheck;

static void
agent_check_free (gpointer data)
{
	AgentCheck *check = data;
	if (check->timeout_id != 0) g_source_remove (check->timeout_id);
	if (check->cancel_id != 0) g_cancellable_disconnect (check->cancel, check->cancel_id);
	g_clear_object (&check->cancel);
	g_clear_object (&check->process);
	g_free (check);
}

static gboolean
agent_check_force_exit (gpointer data)
{
	GSubprocess *process = data;
	if (g_subprocess_get_identifier (process) != NULL) g_subprocess_force_exit (process);
	return G_SOURCE_REMOVE;
}

static void
agent_check_stop (AgentCheck *check)
{
	if (check->session > 0)
	{
		g_autoptr(GDir) proc = NULL;
		const char *name;
		/* Native checks have their own session. Kill it, including startup
		 * jobs that explicitly created another process group in that session. */
		kill (-check->session, SIGKILL);
		proc = g_dir_open ("/proc", 0, NULL);
		while (proc != NULL && (name = g_dir_read_name (proc)) != NULL)
		{
			g_autofree char *path = NULL;
			g_autofree char *stat = NULL;
			char *fields;
			char state;
			int ppid, group, session;
			if (!g_ascii_isdigit (*name)) continue;
			path = g_build_filename ("/proc", name, "stat", NULL);
			if (!g_file_get_contents (path, &stat, NULL, NULL)) continue;
			fields = strrchr (stat, ')');
			if (fields != NULL && sscanf (fields + 1, " %c %d %d %d", &state, &ppid, &group, &session) == 4 && session == check->session)
				kill ((pid_t) g_ascii_strtoll (name, NULL, 10), SIGKILL);
		}
	}
	if (g_subprocess_get_identifier (check->process) != NULL)
	{
		if (check->session > 0) g_subprocess_force_exit (check->process);
		else
		{
			/* Give the host supervisor time to reap its own startup jobs. */
			g_subprocess_send_signal (check->process, SIGTERM);
			g_timeout_add_full (G_PRIORITY_DEFAULT, 250, agent_check_force_exit, g_object_ref (check->process), g_object_unref);
		}
	}
}

static void
agent_check_child_setup (gpointer data)
{
	/* Only async-signal-safe operations between fork and exec. */
	if (setsid () < 0) _exit (127);
}

static void
agent_check_cancel (GCancellable *cancel, gpointer data)
{
	agent_check_stop (data);
}

static gboolean
agent_check_timeout (gpointer data)
{
	AgentCheck *check = g_task_get_task_data (G_TASK (data));
	check->timed_out = TRUE;
	check->timeout_id = 0;
	agent_check_stop (check);
	return G_SOURCE_REMOVE;
}

static void
agent_check_done (GObject *source, GAsyncResult *result, gpointer data)
{
	g_autoptr(GTask) task = data;
	AgentCheck *check = g_task_get_task_data (task);
	g_autoptr(GError) error = NULL;
	if (check->timeout_id != 0)
	{
		g_source_remove (check->timeout_id);
		check->timeout_id = 0;
	}
	if (check->cancel_id != 0)
	{
		g_cancellable_disconnect (check->cancel, check->cancel_id);
		check->cancel_id = 0;
	}
	/* Cleanup also applies if an rc file left background jobs and returned. */
	if (check->session > 0) agent_check_stop (check);
	if (!g_subprocess_wait_finish (check->process, result, &error))
		g_task_return_error (task, g_steal_pointer (&error));
	else if (check->timed_out)
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_TIMED_OUT, _("Agent availability check timed out"));
	else if (!g_subprocess_get_if_exited (check->process))
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, _("Agent availability check was interrupted"));
	else
	{
		int status = g_subprocess_get_exit_status (check->process);
		if (status == 0) g_task_return_int (task, TERMAC_AGENT_AVAILABLE);
		else if (status == 43) g_task_return_int (task, TERMAC_AGENT_MISSING);
		else if (status == 44) g_task_return_int (task, TERMAC_AGENT_DIRECTORY_MISSING);
		else g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, _("Host availability check failed (status %d)"), status);
	}
}

void
termac_agent_check_async (const TermacAgent *agent, const char *cwd, gboolean host,
                         GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr(GTask) task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, termac_agent_check_async);
	if (g_task_return_error_if_cancelled (task)) return;
	if (!host && cwd != NULL && !g_file_test (cwd, G_FILE_TEST_IS_DIR))
	{
		g_task_return_int (task, TERMAC_AGENT_DIRECTORY_MISSING);
		return;
	}
	{
		g_autofree char *binary = termac_agent_build_launch_command (agent);
		g_autofree char *query = g_strdup_printf ("command -v -- %s >/dev/null || exit 43", binary);
		g_autofree char *directory = g_strdup_printf ("--env=TERMAC_AGENT_CWD=%s", cwd != NULL ? cwd : "");
		const char *argv[] = { "flatpak-spawn", "--host", "--watch-bus", "--directory=/", directory,
			"/bin/sh", "-c", /* Keep the host shell in its own session, so the supervisor can
			 * terminate its rc children on normal completion, timeout or cancel. */
			"if [ -n \"$TERMAC_AGENT_CWD\" ]; then cd -- \"$TERMAC_AGENT_CWD\" 2>/dev/null || exit 44; else cd \"$HOME\" || exit 44; fi; "
			"child=; cleanup() { [ -n \"$child\" ] || return; kill -KILL \"-$child\" 2>/dev/null; "
			"for stat in /proc/[0-9]*/stat; do IFS= read -r record < \"$stat\" 2>/dev/null || continue; fields=${record##*) }; "
			"set -- $fields; [ \"$4\" = \"$child\" ] || continue; pid=${stat#/proc/}; pid=${pid%/stat}; kill -KILL \"$pid\" 2>/dev/null; done; }; "
			"trap cleanup EXIT; trap 'exit 125' HUP INT TERM; setsid \"${SHELL:-/bin/sh}\" -ic \"$1\" & child=$!; wait \"$child\"; status=$?; exit \"$status\"", "termac-agent-check", query, NULL };
		const char *shell = g_getenv ("SHELL");
		const char *native_argv[] = { shell != NULL && *shell != '\0' ? shell : "/bin/sh", "-ic", query, NULL };
		g_autoptr(GError) error = NULL;
		g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
		AgentCheck *check = g_new0 (AgentCheck, 1);
		if (!host)
		{
			g_subprocess_launcher_set_cwd (launcher, cwd != NULL ? cwd : g_get_home_dir ());
			g_subprocess_launcher_set_child_setup (launcher, agent_check_child_setup, NULL, NULL);
		}
		check->process = g_subprocess_launcher_spawnv (launcher, host ? argv : native_argv, &error);
		if (check->process == NULL)
		{
			g_free (check);
			g_task_return_error (task, g_steal_pointer (&error));
			return;
		}
		if (!host && g_subprocess_get_identifier (check->process) != NULL)
			check->session = (pid_t) g_ascii_strtoll (g_subprocess_get_identifier (check->process), NULL, 10);
		g_task_set_task_data (task, check, agent_check_free);
		if (cancellable != NULL)
		{
			check->cancel = g_object_ref (cancellable);
			check->cancel_id = g_cancellable_connect (cancellable, G_CALLBACK (agent_check_cancel), check, NULL);
		}
		check->timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 5000, agent_check_timeout, g_object_ref (task), g_object_unref);
		/* Reap even when cancelled; cancellation kills the helper and --watch-bus
		 * terminates its host query, rather than leaving shell startup running. */
		g_subprocess_wait_async (check->process, NULL, agent_check_done, g_object_ref (task));
	}
}

TermacAgentAvailability
termac_agent_check_finish (GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, NULL), TERMAC_AGENT_MISSING);
	g_return_val_if_fail (g_async_result_is_tagged (result, termac_agent_check_async), TERMAC_AGENT_MISSING);
	return g_task_propagate_int (G_TASK (result), error);
}
