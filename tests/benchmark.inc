/* Opt-in measurements; no performance thresholds on shared CI machines. */
#include <sys/resource.h>

static double
benchmark_cpu_ms (void)
{
	struct rusage usage;
	getrusage (RUSAGE_SELF, &usage);
	return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000.0 +
	    (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
}

static void
test_benchmark (void)
{
	const char *count = g_getenv ("TERMUNI_BENCHMARK_TABS");
	TermuniWindow *window;
	TermuniTerminal *terminals[10];
	AdwTabView *view;
	guint n, lines;
	gint64 start, startup_us, mapped_us, output_us, search_us, resize_us;
	double cpu, focused_cpu, background_cpu;
	GtkWindow *background;
	struct rusage usage;
	g_autoptr(GString) expected = g_string_new (NULL);
	if (!count) { g_test_skip ("opt-in: tools/benchmark.py"); return; }
	n = (guint) g_ascii_strtoull (count, NULL, 10);
	lines = g_getenv ("TERMUNI_BENCHMARK_LINES") ? (guint) g_ascii_strtoull (g_getenv ("TERMUNI_BENCHMARK_LINES"), NULL, 10) : 3000;
	g_assert_true (n == 1 || n == 5 || n == 10);
	g_assert_true (lines >= 1000 && lines <= 30000);
	start = g_get_monotonic_time ();
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	for (guint i = 1; i < n; i++)
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));
	while (!gtk_widget_get_mapped (GTK_WIDGET (selected_terminal (window)))) pump (10);
	mapped_us = g_get_monotonic_time () - start;
	for (guint i = 0; i < n; i++) {
		terminals[i] = terminal_of_page (window, i);
		adw_tab_view_set_selected_page (view, adw_tab_view_get_nth_page (view, i));
		run_in_terminal (window, terminals[i], "printf 'bench-%s\\n' ready");
		pump (100); /* Deliver X events before selecting another page. */
	}
	for (guint i = 0; i < n; i++) {
		if (!wait_for_screen (terminals[i], "bench-ready", 10000)) {
			g_autofree char *dump = screen_text (terminals[i]);
			g_test_message ("tab %u: %s", i, dump);
			g_assert_not_reached ();
		}
	}
	startup_us = g_get_monotonic_time () - start;
	for (guint i = 1; i <= lines; i++) g_string_append_printf (expected, "%u\n", i);
	start = g_get_monotonic_time ();
	for (guint i = 0; i < n; i++) {
		g_autofree char *path = g_strdup_printf ("%s/bench-%u", g_get_user_state_dir (), i);
		g_autofree char *quoted = g_shell_quote (path);
		g_autofree char *command = g_strdup_printf (
		    "set -o pipefail; seq 1 %u | tee %s; printf 'bench-%%s:%%s\\n' complete $?", lines, quoted);
		adw_tab_view_set_selected_page (view, adw_tab_view_get_nth_page (view, i));
		run_in_terminal (window, terminals[i], command);
		pump (100);
	}
	for (guint i = 0; i < n; i++) {
		g_autofree char *path = g_strdup_printf ("%s/bench-%u", g_get_user_state_dir (), i);
		g_autofree char *actual = NULL;
		g_assert_true (wait_for_screen (terminals[i], "bench-complete:0", 30000));
		g_assert_true (g_file_get_contents (path, &actual, NULL, NULL));
		g_assert_cmpstr (actual, ==, expected->str);
		g_unlink (path);
	}
	output_us = g_get_monotonic_time () - start;
	start = g_get_monotonic_time ();
	{
		g_autofree char *needle = g_strdup_printf ("%u", lines - 1);
		for (guint i = 0; i < 20; i++)
			g_assert_true (termuni_terminal_find (terminals[n - 1], needle, i % 2));
	}
	search_us = g_get_monotonic_time () - start;
	start = g_get_monotonic_time ();
	for (guint i = 0; i < 10; i++) {
		gtk_window_set_default_size (GTK_WINDOW (window), 900 + (i % 2) * 150, 600 + (i % 2) * 100);
		pump (30);
	}
	resize_us = g_get_monotonic_time () - start;
	cpu = benchmark_cpu_ms (); pump (500); focused_cpu = benchmark_cpu_ms () - cpu;
	background = GTK_WINDOW (gtk_window_new ());
	gtk_window_present (background); pump (200);
	cpu = benchmark_cpu_ms (); pump (500); background_cpu = benchmark_cpu_ms () - cpu;
	getrusage (RUSAGE_SELF, &usage);
	g_print ("TERMUNI_BENCHMARK {\"tabs\":%u,\"mapped_ms\":%.3f,\"shell_ready_with_driver_ms\":%.3f,\"output_ms\":%.3f,"
	    "\"search_20_ms\":%.3f,\"resize_10_ms\":%.3f,\"rss_kib\":%zu,\"peak_rss_kib\":%ld,"
	    "\"focused_idle_cpu_ms\":%.3f,\"background_idle_cpu_ms\":%.3f,\"lines_per_tab\":%u}\n",
	    n, mapped_us / 1000.0, startup_us / 1000.0, output_us / 1000.0, search_us / 1000.0, resize_us / 1000.0,
	    resident_memory_kb (), usage.ru_maxrss, focused_cpu, background_cpu, lines);
	gtk_window_destroy (background);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (1200);
}
