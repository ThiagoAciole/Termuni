/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include <ghostty/vt.h>
#include <glib.h>

#include "termac-vt.h"

#define COLS 40
#define ROWS 10
#define CELL_WIDTH 10

static GhosttyTerminal
new_terminal (void)
{
	GhosttyTerminal terminal = NULL;

	g_assert_cmpint (ghostty_terminal_new (NULL, &terminal, COLS, ROWS), ==, GHOSTTY_SUCCESS);
	return terminal;
}

static void
write_text (GhosttyTerminal terminal, const char *text)
{
	ghostty_terminal_vt_write (terminal, (const uint8_t *) text, strlen (text));
}

static GhosttySelectionGestureGeometry
geometry (void)
{
	GhosttySelectionGestureGeometry value = { 0 };

	value.columns = COLS;
	value.cell_width = CELL_WIDTH;
	value.padding_left = 0;
	value.screen_height = ROWS * 20;
	return value;
}

static void
drag_select (TermacVtSelector *selector,
             GhosttyTerminal   terminal,
             guint             from_column,
             guint             to_column,
             guint             row)
{
	GhosttySelectionGestureGeometry geo = geometry ();

	termac_vt_selector_event (selector, terminal, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS,
	                          from_column, row, from_column * CELL_WIDTH + 2, row * 20 + 10,
	                          1000, NULL);
	termac_vt_selector_event (selector, terminal, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG,
	                          to_column, row, to_column * CELL_WIDTH + 7, row * 20 + 10,
	                          0, &geo);
	termac_vt_selector_event (selector, terminal, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE,
	                          to_column, row, to_column * CELL_WIDTH + 7, row * 20 + 10,
	                          0, NULL);
}

static void
test_selection_copies_ascii_text (void)
{
	GhosttyTerminal terminal = new_terminal ();
	TermacVtSelector *selector = termac_vt_selector_new ();
	g_autofree char *text = NULL;

	write_text (terminal, "hello world\r\n");
	drag_select (selector, terminal, 0, 4, 0);
	g_assert_true (termac_vt_selection_text (terminal, &text));
	g_assert_cmpstr (text, ==, "hello");

	termac_vt_selection_clear (terminal);
	g_assert_false (termac_vt_selection_text (terminal, &text));

	termac_vt_selector_free (selector);
	ghostty_terminal_free (terminal);
}

static void
test_selection_copies_wide_characters (void)
{
	GhosttyTerminal terminal = new_terminal ();
	TermacVtSelector *selector = termac_vt_selector_new ();
	g_autofree char *text = NULL;

	write_text (terminal, "\xe6\xbc\xa2\xe5\xad\x97\r\n"); /* 漢字 */
	drag_select (selector, terminal, 0, 3, 0);
	g_assert_true (termac_vt_selection_text (terminal, &text));
	g_assert_cmpstr (text, ==, "\xe6\xbc\xa2\xe5\xad\x97");

	termac_vt_selector_free (selector);
	ghostty_terminal_free (terminal);
}

static void
test_bracketed_paste_mode_is_reported (void)
{
	GhosttyTerminal terminal = new_terminal ();

	g_assert_false (termac_vt_bracketed_paste (terminal));
	write_text (terminal, "\x1b[?2004h");
	g_assert_true (termac_vt_bracketed_paste (terminal));
	write_text (terminal, "\x1b[?2004l");
	g_assert_false (termac_vt_bracketed_paste (terminal));

	ghostty_terminal_free (terminal);
}

static void
test_paste_is_wrapped_only_when_bracketed (void)
{
	gsize length = 0;
	g_autofree char *wrapped = NULL;
	g_autofree char *plain = NULL;
	g_autofree char *discarded = NULL;

	wrapped = termac_vt_encode_paste ("abc", 3, TRUE, &length);
	g_assert_nonnull (wrapped);
	g_assert_cmpmem (wrapped, length, "\x1b[200~abc\x1b[201~", 12 + 3);

	discarded = termac_vt_encode_paste ("x", 1, FALSE, &length);
	plain = termac_vt_encode_paste ("a\nb", 3, FALSE, &length);
	g_assert_cmpmem (plain, length, "a\rb", 3);
}

/* Text that contains the end-of-paste marker must not end the paste early. */
static void
test_paste_cannot_inject_end_marker (void)
{
	const char *text = "a\x1b[201~b";
	gsize length = 0;
	gsize count = 0;
	g_autofree char *encoded = termac_vt_encode_paste (text, strlen (text), TRUE, &length);
	gsize i;

	g_assert_nonnull (encoded);
	for (i = 0; i + 6 <= length; i++)
		if (memcmp (encoded + i, "\x1b[201~", 6) == 0)
			count++;
	g_assert_cmpuint (count, ==, 1);
	g_assert_cmpmem (encoded + length - 6, 6, "\x1b[201~", 6);
}

static void
test_mouse_tracking_is_reported (void)
{
	GhosttyTerminal terminal = new_terminal ();

	g_assert_false (termac_vt_mouse_tracking (terminal));
	write_text (terminal, "\x1b[?1000h");
	g_assert_true (termac_vt_mouse_tracking (terminal));
	ghostty_terminal_free (terminal);
}

static void
test_find_scrolls_to_match (void)
{
	GhosttyTerminal terminal = new_terminal ();
	GhosttyTerminalScrollbar bar = { 0 };
	guint i;

	for (i = 0; i < 50; i++)
	{
		char line[32];
		g_snprintf (line, sizeof line, "line %u\r\n", i);
		write_text (terminal, line);
	}
	g_assert_false (termac_vt_find (terminal, "", FALSE));
	g_assert_false (termac_vt_find (terminal, "  ", FALSE));
	g_assert_false (termac_vt_find (terminal, "absent", FALSE));

	/* "line 7" is screen row 7 because nothing precedes the output. */
	g_assert_true (termac_vt_find (terminal, "LINE 7", TRUE));
	g_assert_cmpint (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar), ==,
	                 GHOSTTY_SUCCESS);
	g_assert_cmpuint (bar.offset, ==, 7 - bar.len / 2);

	ghostty_terminal_free (terminal);
}

static void
test_find_matches (void)
{
 GhosttyTerminal terminal = new_terminal ();
 GArray *matches;
 const guint positions[][2] = { {0,0}, {0,8}, {1,1} };
 write_text (terminal, "foo bar Foo\r\nxfoo");
 matches = termac_vt_find_matches (terminal, COLS, ROWS, "foo");
 g_assert_cmpuint (matches->len, ==, 3);
 for (guint i = 0; i < matches->len; i++) {
  TermacVtMatch m = g_array_index (matches, TermacVtMatch, i);
  g_assert_cmpuint (m.row, ==, positions[i][0]);
  g_assert_cmpuint (m.column, ==, positions[i][1]);
  g_assert_cmpuint (m.cells, ==, 3);
 }
 g_array_unref (matches);
 ghostty_terminal_free (terminal);
}

static void
test_find_matches_wide (void)
{
 const char *lines[] = { "日本語 foo", "é foo", "\x1b[?2027h👩‍💻 foo", "\x1b[?2027h❤️ foo", "👩‍💻 foo" };
 const guint columns[] = { 7, 2, 3, 3, 5 };
 for (guint i = 0; i < G_N_ELEMENTS (lines); i++) {
  GhosttyTerminal terminal = new_terminal ();
  GArray *matches;
  write_text (terminal, lines[i]);
  matches = termac_vt_find_matches (terminal, COLS, ROWS, "foo");
  g_assert_cmpuint (matches->len, ==, 1);
  g_assert_cmpuint (g_array_index (matches, TermacVtMatch, 0).column, ==, columns[i]);
  g_array_unref (matches);
  ghostty_terminal_free (terminal);
 }
}

static void
test_find_matches_empty (void)
{
 GhosttyTerminal terminal = new_terminal ();
 const char *queries[] = { NULL, "", "  ", "\t\n", "　" };
 for (guint i = 0; i < G_N_ELEMENTS (queries); i++) {
  GArray *matches = termac_vt_find_matches (terminal, COLS, ROWS, queries[i]);
  g_assert_cmpuint (matches->len, ==, 0);
  g_array_unref (matches);
 }
 ghostty_terminal_free (terminal);
}

static void
test_find_matches_preserves_render_updates (void)
{
	GhosttyTerminal terminal = new_terminal ();
	GhosttyRenderState state = NULL;
	GhosttyRenderStateRowIterator iterator = NULL;
	GhosttyRenderStateRowCells cells = NULL;
	g_autoptr(GArray) matches = NULL;
	char text[16] = { 0 };
	GhosttyBuffer buffer = { .ptr = (uint8_t *) text, .cap = sizeof text - 1 };

	g_assert_cmpint (ghostty_render_state_new (NULL, &state), ==, GHOSTTY_SUCCESS);
	g_assert_cmpint (ghostty_render_state_row_iterator_new (NULL, &iterator), ==, GHOSTTY_SUCCESS);
	g_assert_cmpint (ghostty_render_state_row_cells_new (NULL, &cells), ==, GHOSTTY_SUCCESS);
	write_text (terminal, "old");
	g_assert_cmpint (ghostty_render_state_update (state, terminal), ==, GHOSTTY_SUCCESS);
	write_text (terminal, "\rnew");
	matches = termac_vt_find_matches (terminal, COLS, ROWS, "new");
	g_assert_cmpuint (matches->len, ==, 1);
	/* The normal renderer must still receive this pending change after a query. */
	g_assert_cmpint (ghostty_render_state_update (state, terminal), ==, GHOSTTY_SUCCESS);
	g_assert_cmpint (ghostty_render_state_get (state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &iterator), ==, GHOSTTY_SUCCESS);
	g_assert_true (ghostty_render_state_row_iterator_next (iterator));
	g_assert_cmpint (ghostty_render_state_row_get (iterator, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells), ==, GHOSTTY_SUCCESS);
	g_assert_true (ghostty_render_state_row_cells_next (cells));
	g_assert_cmpint (ghostty_render_state_row_cells_get (cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &buffer), ==, GHOSTTY_SUCCESS);
	g_assert_cmpstr (text, ==, "n");
	ghostty_render_state_row_cells_free (cells);
	ghostty_render_state_row_iterator_free (iterator);
	ghostty_render_state_free (state);
	ghostty_terminal_free (terminal);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
 g_test_add_func ("/vt/find-matches", test_find_matches);
 g_test_add_func ("/vt/find-matches-preserves-render-updates", test_find_matches_preserves_render_updates);
 g_test_add_func ("/vt/find-matches-wide", test_find_matches_wide);
 g_test_add_func ("/vt/find-matches-empty", test_find_matches_empty);
	g_test_add_func ("/vt/selection/ascii", test_selection_copies_ascii_text);
	g_test_add_func ("/vt/selection/wide", test_selection_copies_wide_characters);
	g_test_add_func ("/vt/bracketed-paste/mode", test_bracketed_paste_mode_is_reported);
	g_test_add_func ("/vt/paste/encode", test_paste_is_wrapped_only_when_bracketed);
	g_test_add_func ("/vt/paste/no-end-marker-injection", test_paste_cannot_inject_end_marker);
	g_test_add_func ("/vt/mouse/tracking", test_mouse_tracking_is_reported);
	g_test_add_func ("/vt/find/scroll", test_find_scrolls_to_match);
	return g_test_run ();
}
