/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Each child is a freshly exec'ed binary with private XDG and a private Xvfb
 * inherited from the suite. NON_UNIQUE prevents attaching to another app. */
static gboolean
session_process_child (gpointer data)
{
	const char *mode = data;
	g_autofree char *expected_path = g_build_filename (g_get_user_state_dir (), "expected", NULL);
	g_autofree char *selection_path = g_build_filename (g_get_user_state_dir (), "selection", NULL);
	TermuniWindow *window;
	g_autoptr(GVariant) state = NULL;
	gint selected = 0;
	g_application_activate (G_APPLICATION (app));
	pump (500);
	window = main_window ();
	g_assert_nonnull (window);
	if (g_str_equal (mode, "resistant-quit"))
	{
		g_autofree char *script = g_build_filename (g_get_user_state_dir (), "resistant.py", NULL);
		g_autofree char *pidfile = g_build_filename (g_get_user_state_dir (), "resistant.pid", NULL);
		g_autofree char *quoted = g_shell_quote (script);
		g_autofree char *command = g_strdup_printf ("exec python3 %s", quoted);
		g_autofree char *program = g_strdup_printf (
		    "import os,signal,time\nfrom pathlib import Path\n"
		    "signal.signal(signal.SIGHUP, signal.SIG_IGN)\n"
		    "Path(__file__).with_suffix('.pid').write_text(str(os.getpid()))\n"
		    "while True: time.sleep(1)\n");
		gint64 deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
		g_assert_true (g_file_set_contents (script, program, -1, NULL));
		run_in_terminal (window, selected_terminal (window), command);
		while (!g_file_test (pidfile, G_FILE_TEST_EXISTS) && g_get_monotonic_time () < deadline)
			pump (50);
		g_assert_true (g_file_test (pidfile, G_FILE_TEST_EXISTS));
		g_settings_set_boolean (termuni_application_get_settings (app), "confirm-close", FALSE);
		g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
		return G_SOURCE_REMOVE;
	}
	if (g_str_equal (mode, "write"))
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (500);
		report_cwd (window, terminal_of_page (window, 1), "/tmp");
		g_object_set_data_full (G_OBJECT (terminal_of_page (window, 0)), "manual-title",
		                        g_strdup ("Build; touch never-execute"), g_free);
	}
	state = g_variant_ref_sink (termuni_window_session_state (window, &selected));
	if (g_str_equal (mode, "unreadable"))
	{
		/* An unreadable current session must never resurrect obsolete v2. */
		g_assert_cmpuint (g_variant_n_children (state), ==, 1);
		g_application_quit (G_APPLICATION (app));
		return G_SOURCE_REMOVE;
	}
	if (g_str_equal (mode, "migrate"))
	{
		g_autoptr(GVariant) expected = g_variant_ref_sink (g_variant_new_parsed (
		    "[('/tmp', 'Legacy build'), ('/usr', 'Legacy tests')]"));
		g_assert_true (g_variant_equal (state, expected));
		g_assert_cmpint (selected, ==, 1);
	}
	if (g_str_equal (mode, "read"))
	{
		g_autofree char *contents = NULL, *selection = NULL;
		gsize length;
		g_autoptr(GBytes) bytes = NULL;
		g_autoptr(GVariant) expected = NULL;
		g_assert_true (g_file_get_contents (expected_path, &contents, &length, NULL));
		bytes = g_bytes_new (contents, length);
		expected = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(ss)"), bytes, FALSE));
		g_assert_true (g_variant_equal (state, expected));
		g_assert_true (g_file_get_contents (selection_path, &selection, NULL, NULL));
		g_assert_cmpint (selected, ==, atoi (selection));
	}
	else
	{
		g_autoptr(GBytes) bytes = g_variant_get_data_as_bytes (state);
		g_autofree char *selection = g_strdup_printf ("%d", selected);
		g_assert_cmpuint (g_variant_n_children (state), ==, 2);
		termuni_application_save_session_now (app);
		g_assert_true (g_file_set_contents (expected_path, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL));
		g_assert_true (g_file_set_contents (selection_path, selection, -1, NULL));
	}
	/* Explicit quit snapshots before closing and drains all older queued I/O. */
	g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
	return G_SOURCE_REMOVE;
}

static void
run_session_child (const char *root, const char *mode)
{
	g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
	g_autoptr(GSubprocess) child = NULL;
	g_autoptr(GError) error = NULL;
	g_subprocess_launcher_setenv (launcher, "TERMUNI_SESSION_PROCESS_ROOT", root, TRUE);
	g_subprocess_launcher_setenv (launcher, "TERMUNI_SESSION_PROCESS_MODE", mode, TRUE);
	/* Never use the desktop session bus, even if the runner inherited one. */
	g_subprocess_launcher_setenv (launcher, "DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/termuni-test-bus", TRUE);
	child = g_subprocess_launcher_spawn (launcher, &error, "/proc/self/exe", NULL);
	g_assert_no_error (error);
	g_assert_true (g_subprocess_wait_check (child, NULL, &error));
	g_assert_no_error (error);
}

static void
test_session_cross_process_restart (void)
{
	g_autofree char *root = g_dir_make_tmp ("termuni-restart-XXXXXX", NULL);
	g_autofree char *v2 = g_build_filename (root, "termuni", "session-v2.gvariant", NULL);
	g_autofree char *dir = g_path_get_dirname (v2);
	g_autofree char *source = NULL;
	gsize length, source_length;
	GVariantBuilder tabs, windows;
	g_autoptr(GVariant) legacy = NULL;
	g_autoptr(GBytes) bytes = NULL;

	run_session_child (root, "write");
	run_session_child (root, "read");
	/* Simulate upgrade: start another binary process with only a v2 source. */
	g_assert_cmpint (g_mkdir_with_parents (dir, 0700), ==, 0);
	g_variant_builder_init (&tabs, G_VARIANT_TYPE ("a(ss)"));
	g_variant_builder_add (&tabs, "(ss)", "/tmp", "Legacy build");
	g_variant_builder_add (&tabs, "(ss)", "/usr", "Legacy tests");
	g_variant_builder_init (&windows, G_VARIANT_TYPE ("a(ia(ss))"));
	g_variant_builder_add (&windows, "(i@a(ss))", 1, g_variant_builder_end (&tabs));
	legacy = g_variant_ref_sink (g_variant_builder_end (&windows));
	bytes = g_variant_get_data_as_bytes (legacy);
	g_assert_true (g_file_set_contents (v2, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL));
	{
		g_autofree char *v3 = g_build_filename (dir, "session-v3.gvariant", NULL);
		/* Real failed read while an old v2 exists; chmod is ineffective as root. */
		if (geteuid () != 0)
		{
			g_assert_cmpint (g_chmod (v3, 0000), ==, 0);
			run_session_child (root, "unreadable");
			g_assert_cmpint (g_chmod (v3, 0600), ==, 0);
		}
		g_unlink (v3);
		g_assert_cmpint (symlink ("/nonexistent/termuni-unreadable-session", v3), ==, 0);
		run_session_child (root, "unreadable");
		g_assert_true (g_file_test (v3, G_FILE_TEST_IS_SYMLINK));
		g_unlink (v3);
	}
	run_session_child (root, "migrate");
	g_assert_true (g_file_get_contents (v2, &source, &source_length, NULL));
	length = g_bytes_get_size (bytes);
	g_assert_cmpmem (source, source_length, g_bytes_get_data (bytes, NULL), length);
	g_assert_true (g_file_set_contents (v2, "obsolete invalid v2", -1, NULL));
	run_session_child (root, "read");
	/* Directory target forces migration write failure in a separate process. */
	{
		g_autofree char *v3 = g_build_filename (dir, "session-v3.gvariant", NULL);
		g_unlink (v3);
		g_assert_cmpint (g_mkdir (v3, 0700), ==, 0);
		g_assert_true (g_file_set_contents (v2, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL));
		run_session_child (root, "migrate");
		g_clear_pointer (&source, g_free);
		g_assert_true (g_file_get_contents (v2, &source, &source_length, NULL));
		g_assert_cmpmem (source, source_length, g_bytes_get_data (bytes, NULL), length);
		g_rmdir (v3);
		run_session_child (root, "migrate");
		run_session_child (root, "read");
	}
}

static void
test_process_quit_reaps_resistant_child (void)
{
	g_autofree char *root = g_dir_make_tmp ("termuni-quit-process-XXXXXX", NULL);
	g_autofree char *pidfile = g_build_filename (root, "resistant.pid", NULL);
	g_autofree char *contents = NULL;
	pid_t pid;
	run_session_child (root, "resistant-quit");
	g_assert_true (g_file_get_contents (pidfile, &contents, NULL, NULL));
	pid = (pid_t) g_ascii_strtoll (contents, NULL, 10);
	g_assert_cmpint (pid, >, 1);
	g_assert_cmpint (kill (pid, 0), ==, -1);
	g_assert_cmpint (errno, ==, ESRCH);
}
