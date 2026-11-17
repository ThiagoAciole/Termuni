/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <string.h>
#include "termuni-agent.h"

static void
test_find (void)
{
	gsize count = 0;
	const TermuniAgent *const *agents = termuni_agent_all (&count);
	g_assert_cmpuint (count, ==, 2);
	g_assert_true (termuni_agent_find ("claude") == agents[0]);
	g_assert_true (termuni_agent_find ("codex") == agents[1]);
	g_assert_null (termuni_agent_find (NULL));
	g_assert_null (termuni_agent_find ("nope"));
	g_assert_cmpstr (termuni_agent_default ()->id, ==, "claude");
}

static void
test_format_plain (void)
{
	g_autofree char *formatted = termuni_agent_format_attachment (termuni_agent_default (), "/tmp/a.png");
	g_assert_cmpstr (formatted, ==, "'/tmp/a.png' ");
}

static void
test_format (void)
{
	const char *paths[] = { "/tmp/a.png", "/tmp/it's here.png", "/tmp/$HOME `x`.png", "/tmp/ação.png" };
	gsize count;
	const TermuniAgent *const *agents = termuni_agent_all (&count);
	for (guint a = 0; a < count + 1; a++)
		for (guint i = 0; i < G_N_ELEMENTS (paths); i++)
		{
			g_autofree char *formatted = termuni_agent_format_attachment (a < count ? agents[a] : NULL, paths[i]);
			g_autofree char *command = g_strconcat ("printf %s ", formatted, NULL);
			char *argv[] = { "/bin/sh", "-c", command, NULL };
			g_autofree char *output = NULL;
			int status;
			g_assert_true (g_str_has_suffix (formatted, " "));
			g_assert_true (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, &output, NULL, &status, NULL));
			g_assert_true (g_spawn_check_wait_status (status, NULL));
			g_assert_cmpstr (output, ==, paths[i]);
		}
}

static void
test_installed (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (dir, "claude", NULL);
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_assert_true (g_file_set_contents (path, "#!/bin/sh\nexit 0\n", -1, NULL));
	g_assert_cmpint (g_chmod (path, 0755), ==, 0);
	g_setenv ("PATH", dir, TRUE);
	g_assert_true (termuni_agent_is_installed (termuni_agent_default ()));
	g_assert_false (termuni_agent_is_installed (termuni_agent_find ("codex")));
	g_unlink (path);
	g_assert_false (termuni_agent_is_installed (termuni_agent_default ()));
	if (previous != NULL) g_setenv ("PATH", previous, TRUE); else g_unsetenv ("PATH");
	g_rmdir (dir);
}

typedef struct { gboolean done; TermuniAgentAvailability status; GError *error; } CheckResult;
static void
check_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
	CheckResult *check = user_data;
	check->status = termuni_agent_check_finish (result, &check->error);
	check->done = TRUE;
}
static void
wait_check (CheckResult *check)
{
	gint64 deadline = g_get_monotonic_time () + 8 * G_USEC_PER_SEC;
	while (!check->done && g_get_monotonic_time () < deadline)
	{
		while (g_main_context_iteration (NULL, FALSE));
		g_usleep (1000);
	}
	g_assert_true (check->done);
}
static void
test_shell_path (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-path-XXXXXX", NULL);
	g_autofree char *shell = g_build_filename (dir, "shell", NULL);
	g_autofree char *binary = g_build_filename (dir, "claude", NULL);
	g_autofree char *old_path = g_strdup (g_getenv ("PATH"));
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	g_autofree char *script = g_strdup_printf ("#!/bin/sh\nPATH='%s'; export PATH\nexec /bin/sh -c \"$2\"\n", dir);
	CheckResult check = { 0 };
	g_assert_true (g_file_set_contents (shell, script, -1, NULL));
	g_assert_true (g_file_set_contents (binary, "#!/bin/sh\nexit 0\n", -1, NULL));
	g_chmod (shell, 0755); g_chmod (binary, 0755);
	g_setenv ("PATH", "/nonexistent", TRUE);
	g_setenv ("SHELL", shell, TRUE);
	g_assert_false (termuni_agent_is_installed (termuni_agent_default ()));
	termuni_agent_check_async (termuni_agent_default (), dir, NULL, check_done, &check);
	wait_check (&check);
	g_assert_no_error (check.error);
	g_assert_cmpint (check.status, ==, TERMUNI_AGENT_AVAILABLE);
	g_setenv ("PATH", old_path != NULL ? old_path : "", TRUE);
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_unlink (shell); g_unlink (binary); g_rmdir (dir);
}

static void
test_relative_path (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-cwd-XXXXXX", NULL);
	g_autofree char *bin = g_build_filename (dir, "bin", NULL);
	g_autofree char *program = g_build_filename (bin, "claude", NULL);
	g_autofree char *old_path = g_strdup (g_getenv ("PATH"));
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	CheckResult check = { 0 };
	g_mkdir (bin, 0700);
	g_assert_true (g_file_set_contents (program, "#!/bin/sh\nexit 0\n", -1, NULL));
	g_chmod (program, 0755);
	g_setenv ("PATH", "bin", TRUE);
	g_setenv ("SHELL", "/bin/sh", TRUE);
	termuni_agent_check_async (termuni_agent_default (), dir, NULL, check_done, &check);
	wait_check (&check);
	g_assert_no_error (check.error);
	g_assert_cmpint (check.status, ==, TERMUNI_AGENT_AVAILABLE);
	g_setenv ("PATH", old_path != NULL ? old_path : "", TRUE);
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_unlink (program); g_rmdir (bin); g_rmdir (dir);
}

static void
test_check_reaps_children (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-reap-XXXXXX", NULL);
	g_autofree char *shell = g_build_filename (dir, "shell", NULL);
	g_autofree char *pidfile = g_build_filename (dir, "pid", NULL);
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	g_autofree char *script = g_strdup_printf ("#!/bin/sh\n/bin/sleep 60 &\necho $! > '%s'\nwait\n", pidfile);
	CheckResult check = { 0 };
	g_autofree char *pidtext = NULL;
	guint64 pid;
	g_assert_true (g_file_set_contents (shell, script, -1, NULL)); g_chmod (shell, 0755);
	g_setenv ("SHELL", shell, TRUE);
	termuni_agent_check_async (termuni_agent_default (), dir, NULL, check_done, &check);
	wait_check (&check);
	g_assert_error (check.error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT); g_clear_error (&check.error);
	g_assert_true (g_file_get_contents (pidfile, &pidtext, NULL, NULL));
	pid = g_ascii_strtoull (pidtext, NULL, 10);
	{
		g_autofree char *proc = g_strdup_printf ("/proc/%" G_GUINT64_FORMAT "/stat", pid);
		g_autofree char *state = NULL;
		g_file_get_contents (proc, &state, NULL, NULL);
		/* A subreaper outside this test can temporarily own a dead zombie. */
		gboolean stopped = state == NULL || strstr (state, ") Z ") != NULL;
		if (!stopped) kill ((pid_t) pid, SIGKILL); /* clean up this test's own PID on RED */
		g_assert_true (stopped);
	}
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_unlink (shell); g_unlink (pidfile); g_rmdir (dir);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/agent/relative-path", test_relative_path);
	g_test_add_func ("/agent/check-reaps-children", test_check_reaps_children);
	g_test_add_func ("/agent/shell-path", test_shell_path);
	g_test_add_func ("/agent/installed", test_installed);
	g_test_add_func ("/agent/find", test_find);
	g_test_add_func ("/agent/format-plain", test_format_plain);
	g_test_add_func ("/agent/format-hostile", test_format);
	return g_test_run ();
}
