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
