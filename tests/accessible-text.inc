/* SPDX-License-Identifier: GPL-3.0-or-later */
static char *
accessible_slice (GtkAccessibleText *text, guint start, guint end)
{
	g_autoptr(GBytes) bytes = GTK_ACCESSIBLE_TEXT_GET_IFACE (text)->get_contents (text, start, end);
	gsize length;
	const char *data = g_bytes_get_data (bytes, &length);
	return g_strndup (data, length);
}

/* Interpose only to observe GTK notifications, then forward to the real GTK
 * function. This checks deletion ordering/Unicode payloads without pretending
 * that a test backend proves the desktop AT-SPI transport or Orca speech. */
typedef struct { GtkAccessibleTextContentChange change; guint start, end; char *text; } AccessibleChange;
static GtkAccessibleText *observed_accessible;
static GArray *accessible_changes;

void
gtk_accessible_text_update_contents (GtkAccessibleText *text, GtkAccessibleTextContentChange change,
                                     guint start, guint end)
{
	static void (*real_update) (GtkAccessibleText *, GtkAccessibleTextContentChange, guint, guint);
	if (!real_update) real_update = dlsym (RTLD_NEXT, "gtk_accessible_text_update_contents");
	g_assert_nonnull (real_update);
	if (text == observed_accessible) {
		AccessibleChange event = { change, start, end, accessible_slice (text, start, end) };
		g_array_append_val (accessible_changes, event);
	}
	real_update (text, change, start, end);
}

static void
test_accessible_text (void)
{
	TermacWindow *window = g_object_new (TERMAC_TYPE_WINDOW, "application", app, NULL);
	TermacTerminal *terminal;
	GhosttyTerminal vt;
	GtkAccessibleText *text;
	GtkAccessibleTextInterface *iface;
	g_autofree char *all = NULL;
	const char *fixture = "\033[?2027h\033[2J\033[H日本 é 👩‍💻 world. Next!\r\nsecond line\033[1;5H";
	guint start, end;
	gsize n;
	GtkAccessibleTextRange *ranges = NULL;
	GhosttySelection selection = GHOSTTY_INIT_SIZED (GhosttySelection);
	GhosttyPoint point = { .tag = GHOSTTY_POINT_TAG_VIEWPORT };
	graphene_rect_t rect;
	graphene_point_t position;
	guint offset, cw, ch;
	gtk_window_present (GTK_WINDOW (window));
	pump (700);
	terminal = selected_terminal (window);
	vt = termac_terminal_get_vt_handle (terminal);
	text = GTK_ACCESSIBLE_TEXT (terminal);
	iface = GTK_ACCESSIBLE_TEXT_GET_IFACE (text);
	ghostty_terminal_vt_write (vt, (const uint8_t *) fixture, strlen (fixture));
	gtk_widget_queue_draw (GTK_WIDGET (terminal)); pump (150);
	all = accessible_slice (text, 0, G_MAXUINT);
	g_assert_true (g_str_has_prefix (all, "日本 é 👩‍💻 world. Next!"));
	{
		g_autofree char *slice = accessible_slice (text, 6, 9);
		g_assert_cmpstr (slice, ==, "👩‍💻");
	}
	g_assert_cmpuint (iface->get_caret_position (text), ==, 2);
	{
		g_autoptr(GBytes) bytes = iface->get_contents_at (text, 7, GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER, &start, &end);
		g_assert_cmpuint (start, ==, 7); g_assert_cmpuint (end, ==, 8);
		g_assert_cmpuint (g_bytes_get_size (bytes), ==, strlen ("‍"));
	}
	{
		g_autoptr(GBytes) bytes = iface->get_contents_at (text, 12, GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD, &start, &end);
		g_autofree char *slice = accessible_slice (text, start, end);
		g_assert_true (g_str_has_prefix (slice, "world"));
	}
	{
		g_autoptr(GBytes) bytes = iface->get_contents_at (text, 1, GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE, &start, &end);
		g_assert_cmpuint (start, ==, 0);
		g_autofree char *slice = accessible_slice (text, start, end);
		g_assert_cmpint (slice[strlen (slice) - 1], ==, '\n');
	}
	{
		g_autoptr(GBytes) bytes = iface->get_contents_at (text, 12, GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE, &start, &end);
		g_assert_cmpuint (start, ==, 0); g_assert_cmpuint (end, ==, 17);
	}
	{
		g_autoptr(GBytes) bytes = iface->get_contents_at (text, G_MAXUINT, GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH, &start, &end);
		g_assert_cmpuint (start, ==, end); g_assert_cmpuint (g_bytes_get_size (bytes), ==, 0);
	}
	g_assert_false (iface->get_selection (text, &n, &ranges));
	point.value.coordinate.x = 0; point.value.coordinate.y = 0;
	g_assert_cmpint (ghostty_terminal_grid_ref (vt, point, &selection.start), ==, GHOSTTY_SUCCESS);
	point.value.coordinate.x = 3;
	g_assert_cmpint (ghostty_terminal_grid_ref (vt, point, &selection.end), ==, GHOSTTY_SUCCESS);
	ghostty_terminal_set (vt, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
	g_assert_true (iface->get_selection (text, &n, &ranges));
	g_assert_cmpuint (n, ==, 1); g_assert_cmpuint (ranges[0].start, ==, 0); g_assert_cmpuint (ranges[0].length, ==, 2);
	g_clear_pointer (&ranges, g_free);
	GhosttyGridRef swap = selection.start; selection.start = selection.end; selection.end = swap;
	ghostty_terminal_set (vt, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
	g_assert_true (iface->get_selection (text, &n, &ranges));
	g_assert_cmpuint (n, ==, 1); g_assert_cmpuint (ranges[0].length, ==, 2);
	g_clear_pointer (&ranges, g_free);
	selection.rectangle = true;
	point.value.coordinate.y = 1;
	g_assert_cmpint (ghostty_terminal_grid_ref (vt, point, &selection.start), ==, GHOSTTY_SUCCESS);
	ghostty_terminal_set (vt, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
	g_assert_true (iface->get_selection (text, &n, &ranges));
	g_assert_cmpuint (n, ==, 2); g_assert_cmpuint (ranges[0].length, ==, 2);
	g_assert_cmpuint (ranges[1].length, ==, 4);
	g_clear_pointer (&ranges, g_free);
	termac_vt_clear_selection (vt);
	g_assert_false (iface->get_selection (text, &n, &ranges));
	g_assert_cmpuint (n, ==, 0); g_assert_null (ranges);
	g_assert_false (iface->set_caret_position (text, 0));
	GtkAccessibleTextRange arbitrary = { 0, 2 };
	g_assert_false (iface->set_selection (text, 0, &arbitrary));
	termac_terminal_get_cell_size (terminal, &cw, &ch);
	g_assert_true (iface->get_extents (text, 0, 1, &rect));
	g_assert_cmpfloat (rect.origin.x, ==, 8); g_assert_cmpfloat (rect.origin.y, ==, 12);
	g_assert_cmpfloat (rect.size.width, ==, 2 * cw);
	graphene_point_init (&position, 8 + cw + 1, 13);
	g_assert_true (iface->get_offset (text, &position, &offset)); g_assert_cmpuint (offset, ==, 0);
	graphene_point_init (&position, -1, -1);
	g_assert_false (iface->get_offset (text, &position, &offset));
	/* A query must not steal Ghostty's dirty flags from the persistent renderer. */
	GhosttyRenderState probe;
	g_assert_cmpint (ghostty_render_state_new (NULL, &probe), ==, GHOSTTY_SUCCESS);
	ghostty_render_state_update (probe, vt);
	accessible_changes = g_array_new (FALSE, FALSE, sizeof (AccessibleChange));
	observed_accessible = text;
	ghostty_terminal_vt_write (vt, (const uint8_t *) "\033[HZ", 4);
	g_clear_pointer (&all, g_free); all = accessible_slice (text, 0, 1);
	g_assert_cmpstr (all, ==, "Z");
	ghostty_render_state_update (probe, vt);
	GhosttyRenderStateRowIterator iter;
	GhosttyRenderStateRowCells cells;
	GhosttyCell raw;
	uint32_t codepoint = 0;
	ghostty_render_state_row_iterator_new (NULL, &iter);
	ghostty_render_state_row_cells_new (NULL, &cells);
	ghostty_render_state_get (probe, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &iter);
	g_assert_true (ghostty_render_state_row_iterator_next (iter));
	ghostty_render_state_row_get (iter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells);
	ghostty_render_state_row_cells_next (cells);
	ghostty_render_state_row_cells_get (cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw);
	ghostty_cell_get (raw, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint);
	g_assert_cmpuint (codepoint, ==, 'Z');
	ghostty_render_state_row_cells_free (cells);
	ghostty_render_state_row_iterator_free (iter);
	ghostty_render_state_free (probe);
	gtk_widget_queue_draw (GTK_WIDGET (terminal)); pump (150);
	observed_accessible = NULL;
	g_assert_cmpuint (accessible_changes->len, ==, 2);
	AccessibleChange *removed = &g_array_index (accessible_changes, AccessibleChange, 0);
	AccessibleChange *inserted = &g_array_index (accessible_changes, AccessibleChange, 1);
	g_assert_cmpint (removed->change, ==, GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE);
	g_assert_cmpuint (removed->start, ==, 0); g_assert_cmpuint (removed->end, ==, 1);
	g_assert_cmpstr (removed->text, ==, "日");
	g_assert_cmpint (inserted->change, ==, GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT);
	g_assert_cmpuint (inserted->start, ==, 0); g_assert_cmpuint (inserted->end, ==, 2);
	g_assert_cmpstr (inserted->text, ==, "Z ");
	for (guint i = 0; i < accessible_changes->len; i++) g_free (g_array_index (accessible_changes, AccessibleChange, i).text);
	g_clear_pointer (&accessible_changes, g_array_unref);
	/* History is the same accessible document when scrolled into view. */
	for (guint i = 0; i < termac_terminal_get_rows (terminal) + 10; i++) {
		g_autofree char *line = g_strdup_printf ("\r\nhistory-%03u", i);
		ghostty_terminal_vt_write (vt, (const uint8_t *) line, strlen (line));
	}
	GhosttyTerminalScrollViewport scroll = { .tag = GHOSTTY_SCROLL_VIEWPORT_TOP };
	ghostty_terminal_scroll_viewport (vt, scroll);
	gtk_widget_queue_draw (GTK_WIDGET (terminal)); pump (150);
	g_clear_pointer (&all, g_free); all = accessible_slice (text, 0, G_MAXUINT);
	g_assert_nonnull (strstr (all, "history-000"));
	gtk_window_destroy (GTK_WINDOW (window)); pump (100);
}
