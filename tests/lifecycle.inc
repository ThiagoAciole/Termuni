/* Child teardown must not keep the terminal widget or stall the main loop. */
static void
test_async_child_close (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termac-child-stop-XXXXXX", NULL);
	g_autofree char *script = g_build_filename (dir, "ignore-hup.py", NULL);
	g_autofree char *pidfile = g_build_filename (dir, "pid", NULL);
	g_autofree char *python = g_find_program_in_path ("python3");
	g_autofree char *qpython = g_shell_quote (python);
	g_autofree char *qscript = g_shell_quote (script);
	g_autofree char *qpidfile = g_shell_quote (pidfile);
	g_autofree char *command = g_strdup_printf ("exec %s %s %s", qpython, qscript, qpidfile);
	g_autofree char *contents = NULL;
	GtkWindow *window = GTK_WINDOW (gtk_window_new ());
	TermacTerminal *terminal;
	gint64 start, deadline;
	pid_t pid;

	g_assert_true (g_file_set_contents (script,
	    "import os,signal,sys,time\nfrom pathlib import Path\n"
	    "signal.signal(signal.SIGHUP, signal.SIG_IGN)\n"
	    "Path(sys.argv[1]).write_text(str(os.getpid()))\n"
	    "while True: time.sleep(1)\n", -1, NULL));
	terminal = TERMAC_TERMINAL (termac_terminal_new_command ("/tmp", FALSE, command));
	gtk_window_set_child (window, GTK_WIDGET (terminal));
	gtk_window_present (window);
	deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
	while (!g_file_test (pidfile, G_FILE_TEST_EXISTS) && g_get_monotonic_time () < deadline)
		pump (50);
	g_assert_true (g_file_get_contents (pidfile, &contents, NULL, NULL));
	pid = (pid_t) g_ascii_strtoll (contents, NULL, 10);
	g_assert_cmpint (pid, >, 1);
	g_object_add_weak_pointer (G_OBJECT (terminal), (gpointer *) &terminal);
	start = g_get_monotonic_time ();
	gtk_window_destroy (window);
	pump (30);
	g_assert_cmpint (g_get_monotonic_time () - start, <, 250 * 1000);
	g_assert_null (terminal);
	deadline = g_get_monotonic_time () + 4 * G_USEC_PER_SEC;
	while (kill (pid, 0) == 0 && g_get_monotonic_time () < deadline)
		pump (50);
	g_assert_cmpint (kill (pid, 0), ==, -1);
	g_assert_cmpint (errno, ==, ESRCH);
	g_unlink (script);
	g_unlink (pidfile);
	g_rmdir (dir);
}

static void
test_quit_confirmation (void)
{
	TermacWindow *window;
	TermacTerminal *terminal;
	GSettings *settings = termac_application_get_settings (app);
	AdwDialog *dialog;
	destroy_all_windows ();
	forget_saved_sessions ();
	g_settings_set_boolean (settings, "confirm-close", TRUE);
	window = g_object_new (TERMAC_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	pump (600);
	terminal = selected_terminal (window);
	run_in_terminal (window, terminal, "sleep 30");
	pump (500);
	g_assert_true (termac_terminal_has_foreground_job (terminal));
	g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
	pump (100);
	dialog = adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window));
	g_assert_true (ADW_IS_ALERT_DIALOG (dialog));
	g_signal_emit_by_name (find_button_labelled (GTK_WIDGET (dialog), "Cancel"), "clicked");
	pump (250);
	g_assert_true (termac_terminal_has_foreground_job (terminal));
	g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
	pump (100);
	dialog = adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window));
	g_assert_true (ADW_IS_ALERT_DIALOG (dialog));
	g_signal_emit_by_name (find_button_labelled (GTK_WIDGET (dialog), "Quit"), "clicked");
	pump (500);
	g_assert_null (main_window ());
}

static void
input_problem_cb (TermacTerminal *terminal, const char *message, gpointer data)
{
	guint *count = data;
	g_assert_nonnull (strstr (message, "connection is closed"));
	(*count)++;
}

static void
test_closed_input_feedback (void)
{
	TermacTerminal *terminal = TERMAC_TERMINAL (termac_terminal_new_command ("/tmp", FALSE, "exec /bin/true"));
	guint messages = 0;
	g_object_ref_sink (terminal);
	pump (500);
	g_signal_connect (terminal, "message", G_CALLBACK (input_problem_cb), &messages);
	termac_terminal_insert_text (terminal, "undeliverable input");
	pump (100);
	g_assert_cmpuint (messages, ==, 1);
	termac_terminal_insert_text (terminal, "more input");
	pump (100);
	g_assert_cmpuint (messages, ==, 1);
	g_object_unref (terminal);
	pump (100);
}
