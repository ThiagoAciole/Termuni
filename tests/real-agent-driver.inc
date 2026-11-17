/* Opt-in integration driver. Runs only in its own Xvfb test display; never
 * attaches to the user's desktop or to an existing terminal session.
 * Each command performs one operation and publishes a fresh VT/snapshot. */
static void
test_real_agent_driver (void)
{
	const char *agent_id = g_getenv ("TERMUNI_REAL_AGENT");
	const char *control = g_getenv ("TERMUNI_AGENT_CONTROL");
	TermuniWindow *window;
	TermuniTerminal *terminal;
	g_autofree char *text_path = NULL;
	g_autofree char *ack_path = NULL;
	g_autofree char *image_path = NULL;
	gint64 deadline = g_get_monotonic_time () + 600 * G_USEC_PER_SEC;
	guint step = 0;
	gboolean quit_requested = FALSE;
	if (agent_id == NULL || control == NULL)
	{
		g_test_skip ("opt-in real CLI validation; requires existing authentication");
		return;
	}
	g_assert_nonnull (termuni_agent_find (agent_id));
	g_setenv ("TERMUNI_UI_SCREENSHOTS", g_get_user_state_dir (), TRUE);
	g_settings_set_string (termuni_application_get_settings (app), "agent", agent_id);
	g_settings_set_boolean (termuni_application_get_settings (app), "confirm-close", FALSE);
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), 1100, 820);
	{
		GVariantBuilder tabs;
		g_variant_builder_init (&tabs, G_VARIANT_TYPE ("a(ss)"));
		g_variant_builder_add (&tabs, "(ss)", g_get_user_state_dir (), "Agent QA");
		g_autoptr(GVariant) state = g_variant_ref_sink (g_variant_builder_end (&tabs));
		termuni_window_restore_tabs (window, state, 0);
	}
	gtk_window_present (GTK_WINDOW (window));
	pump (300);
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent", NULL);
	for (guint tries = 0; tries < 80 && tab_count (window) < 2; tries++) pump (100);
	g_assert_cmpuint (tab_count (window), ==, 2);
	terminal = selected_terminal (window);
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	text_path = g_strconcat (control, ".screen.txt", NULL);
	ack_path = g_strconcat (control, ".ack", NULL);
	image_path = g_build_filename (g_get_user_state_dir (), "real-agent.png", NULL);
	while (g_get_monotonic_time () < deadline)
	{
		g_autofree char *command = NULL;
		g_autofree char *text = NULL;
		if (step == 0 || g_file_get_contents (control, &command, NULL, NULL))
		{
			char *payload = command != NULL ? strchr (command, '\n') : NULL;
			if (command != NULL) g_unlink (control);
			if (payload != NULL) *payload++ = '\0';
			if (g_strcmp0 (command, "quit") == 0) { quit_requested = TRUE; break; }
			if (g_strcmp0 (command, "type") == 0)
				termuni_terminal_insert_text (terminal, payload != NULL ? payload : "");
			else if (g_strcmp0 (command, "key") == 0)
			{
				gboolean ctrl = payload != NULL && g_str_has_prefix (payload, "Ctrl+");
				gboolean shift = payload != NULL && g_str_has_prefix (payload, "Shift+");
				KeySym key = XStringToKeysym (payload != NULL ? payload + (ctrl ? 5 : shift ? 6 : 0) : "Return");
				g_assert_cmpuint (key, !=, NoSymbol);
				send_keysym_full (key, ctrl, shift);
			}
			else if (g_strcmp0 (command, "drop") == 0)
			{
				GSList *files = g_slist_append (NULL, g_file_new_for_path (payload));
				GdkFileList *list = gdk_file_list_new_from_list (files);
				g_assert_true (termuni_window_attach_file_list (window, list));
				g_boxed_free (GDK_TYPE_FILE_LIST, list);
				g_slist_free_full (files, g_object_unref);
			}
			else if (g_strcmp0 (command, "clipboard") == 0)
			{
				g_autoptr(GFile) file = g_file_new_for_path (payload);
				g_autoptr(GError) error = NULL;
				g_autoptr(GdkTexture) texture = gdk_texture_new_from_file (file, &error);
				g_assert_no_error (error);
				gdk_clipboard_set_texture (gdk_display_get_clipboard (gdk_display_get_default ()), texture);
			}
			pump (500);
			text = screen_text (terminal);
			g_assert_true (g_file_set_contents (text_path, text, -1, NULL));
			save_widget_png (GTK_WIDGET (window), "real-agent.png");
			{
				g_autofree char *ack = g_strdup_printf ("%u\n%s\n%s\n", ++step, image_path, g_get_user_state_dir ());
				g_assert_true (g_file_set_contents (ack_path, ack, -1, NULL));
			}
		}
		pump (100);
	}
	gtk_window_destroy (GTK_WINDOW (window));
	pump (300);
	g_assert_true (quit_requested);
}
