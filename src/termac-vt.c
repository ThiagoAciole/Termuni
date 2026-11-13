/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termac-vt.h"

#include <string.h>

struct _TermacVtSelector
{
	GhosttySelectionGesture gesture;
	GhosttySelectionGestureEvent events[5];
};

static const GhosttySelectionGestureEventType selector_types[] = {
	GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS,
	GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE,
	GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG,
	GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_AUTOSCROLL_TICK,
	GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DEEP_PRESS,
};

static const guint64 repeat_interval_ns = 500 * 1000 * 1000;
static const double repeat_distance = 4.0;

TermacVtSelector *
termac_vt_selector_new (void)
{
	TermacVtSelector *selector = g_new0 (TermacVtSelector, 1);
	guint i;

	if (ghostty_selection_gesture_new (NULL, &selector->gesture) != GHOSTTY_SUCCESS)
	{
		g_free (selector);
		return NULL;
	}
	for (i = 0; i < G_N_ELEMENTS (selector->events); i++)
		ghostty_selection_gesture_event_new (NULL, &selector->events[i], selector_types[i]);
	return selector;
}

void
termac_vt_selector_free (TermacVtSelector *selector)
{
	guint i;

	if (selector == NULL)
		return;
	for (i = 0; i < G_N_ELEMENTS (selector->events); i++)
		ghostty_selection_gesture_event_free (selector->events[i]);
	ghostty_selection_gesture_free (selector->gesture, NULL);
	g_free (selector);
}

static gboolean
viewport_ref (GhosttyTerminal terminal,
              guint           column,
              guint           row,
              GhosttyGridRef *ref)
{
	GhosttyPoint point = { 0 };

	point.tag = GHOSTTY_POINT_TAG_VIEWPORT;
	point.value.coordinate.x = (uint16_t) column;
	point.value.coordinate.y = (uint32_t) row;
	return ghostty_terminal_grid_ref (terminal, point, ref) == GHOSTTY_SUCCESS;
}

void
termac_vt_selection_clear (GhosttyTerminal terminal)
{
	ghostty_terminal_set (terminal, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
}

void
termac_vt_select_all (GhosttyTerminal terminal)
{
	GhosttySelection selection = GHOSTTY_INIT_SIZED (GhosttySelection);

	if (ghostty_terminal_select_all (terminal, &selection) == GHOSTTY_SUCCESS)
		ghostty_terminal_set (terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
}

void
termac_vt_clear_selection (GhosttyTerminal terminal)
{
	/* A NULL selection clears the active screen selection. */
	ghostty_terminal_set (terminal, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
}

gboolean
termac_vt_selector_event (TermacVtSelector                  *selector,
                          GhosttyTerminal                    terminal,
                          GhosttySelectionGestureEventType   type,
                          guint                              column,
                          guint                              row,
                          double                             pixel_x,
                          double                             pixel_y,
                          guint64                            time_ns,
                          const GhosttySelectionGestureGeometry *geometry)
{
	GhosttySelectionGestureEvent event = NULL;
	GhosttyGridRef ref = GHOSTTY_INIT_SIZED (GhosttyGridRef);
	GhosttySelection selection = GHOSTTY_INIT_SIZED (GhosttySelection);
	GhosttySurfacePosition position;
	GhosttyResult result;
	guint i;

	g_return_val_if_fail (selector != NULL, FALSE);
	for (i = 0; i < G_N_ELEMENTS (selector->events); i++)
		if (selector_types[i] == type)
			event = selector->events[i];
	if (event == NULL)
		return FALSE;

	position.x = pixel_x;
	position.y = pixel_y;
	if (viewport_ref (terminal, column, row, &ref))
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref);
	else
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, NULL);
	ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION, &position);
	if (type == GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS)
	{
		double distance = repeat_distance;
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS, &time_ns);
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS,
		                                     &repeat_interval_ns);
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE, &distance);
	}
	if (geometry != NULL)
		ghostty_selection_gesture_event_set (event, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY, geometry);

	result = ghostty_selection_gesture_event (selector->gesture, terminal, event, &selection);
	if (result == GHOSTTY_SUCCESS)
	{
		ghostty_terminal_set (terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
		return TRUE;
	}
	return FALSE;
}

static gboolean
format_selection (GhosttyTerminal          terminal,
                  const GhosttySelection  *selection,
                  gboolean                 unwrap,
                  gboolean                 trim,
                  char                   **text)
{
	GhosttyTerminalSelectionFormatOptions options = GHOSTTY_INIT_SIZED (GhosttyTerminalSelectionFormatOptions);
	uint8_t *buffer = NULL;
	size_t length = 0;
	GhosttyResult result;

	options.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
	options.unwrap = unwrap;
	options.trim = trim;
	options.selection = selection;
	result = ghostty_terminal_selection_format_alloc (terminal, NULL, options, &buffer, &length);
	if (result != GHOSTTY_SUCCESS)
		return FALSE;
	*text = g_strndup ((const char *) buffer, length);
	ghostty_free (NULL, buffer, length);
	return TRUE;
}

gboolean
termac_vt_selection_text (GhosttyTerminal terminal,
                          char          **text)
{
	return format_selection (terminal, NULL, TRUE, TRUE, text);
}

gboolean
termac_vt_viewport_row_text (GhosttyTerminal terminal,
                             guint           columns,
                             guint           row,
                             char          **text)
{
	GhosttySelection selection = GHOSTTY_INIT_SIZED (GhosttySelection);

	if (columns == 0 || !viewport_ref (terminal, 0, row, &selection.start) ||
	    !viewport_ref (terminal, columns - 1, row, &selection.end))
		return FALSE;
	return format_selection (terminal, &selection, FALSE, FALSE, text);
}

gboolean
termac_vt_bracketed_paste (GhosttyTerminal terminal)
{
	GhosttyTerminalModeConfig config = { GHOSTTY_MODE_BRACKETED_PASTE, FALSE };

	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_MODE, &config) != GHOSTTY_SUCCESS)
		return FALSE;
	return config.value;
}

gboolean
termac_vt_mouse_tracking (GhosttyTerminal terminal)
{
	bool enabled = false;

	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &enabled) != GHOSTTY_SUCCESS)
		return FALSE;
	return enabled;
}

gboolean
termac_vt_alternate_screen (GhosttyTerminal terminal)
{
	GhosttyTerminalScreen screen = GHOSTTY_TERMINAL_SCREEN_PRIMARY;

	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN, &screen) != GHOSTTY_SUCCESS)
		return FALSE;
	return screen == GHOSTTY_TERMINAL_SCREEN_ALTERNATE;
}

char *
termac_vt_encode_paste (const char *text,
                        gsize       length,
                        gboolean    bracketed,
                        gsize      *out_length)
{
	gsize capacity = length + 16;
	gsize written = 0;
	char *output;
	GhosttyResult result;

	g_return_val_if_fail (text != NULL || length == 0, NULL);
	g_return_val_if_fail (out_length != NULL, NULL);

	for (;;)
	{
		/* The encoder edits its input in place, so each attempt gets a copy. */
		g_autofree char *work = g_memdup2 (text != NULL ? text : "", length + 1);
		output = g_malloc (capacity);
		result = ghostty_paste_encode (work, length, bracketed, output, capacity, &written);
		if (result == GHOSTTY_SUCCESS)
		{
			*out_length = written;
			return output;
		}
		g_free (output);
		if (result != GHOSTTY_OUT_OF_SPACE || written <= capacity)
			break;
		capacity = written;
	}
	*out_length = 0;
	return NULL;
}

/* Returns the absolute screen row of each line whose text contains needle,
 * in increasing order. Each formatted line is one screen row. */
static gboolean
find_rows (GhosttyTerminal  terminal,
           const char      *needle,
           GArray         **rows)
{
	GhosttySelection everything = GHOSTTY_INIT_SIZED (GhosttySelection);
	GhosttyPointCoordinate origin = { 0 };
	g_autofree char *text = NULL;
	g_autofree char *folded_needle = NULL;
	g_auto(GStrv) lines = NULL;
	guint i;

	if (ghostty_terminal_select_all (terminal, &everything) != GHOSTTY_SUCCESS)
		return FALSE;
	if (ghostty_terminal_point_from_grid_ref (terminal, &everything.start,
	                                          GHOSTTY_POINT_TAG_SCREEN, &origin) != GHOSTTY_SUCCESS)
		return FALSE;
	if (!format_selection (terminal, &everything, FALSE, FALSE, &text))
		return FALSE;

	lines = g_strsplit (text, "\n", -1);
	folded_needle = g_utf8_casefold (needle, -1);
	*rows = g_array_new (FALSE, FALSE, sizeof (guint64));
	for (i = 0; lines[i] != NULL; i++)
	{
		g_autofree char *folded_line = g_utf8_casefold (lines[i], -1);
		if (strstr (folded_line, folded_needle) != NULL)
		{
			guint64 row = (guint64) origin.y + i;
			g_array_append_val (*rows, row);
		}
	}
	return TRUE;
}

gboolean
termac_vt_find_target (GhosttyTerminal terminal, const char *needle,
                        gboolean backwards, const guint64 *after_row,
                        guint64 *target_row)
{
	GhosttyTerminalScrollbar bar = { 0 };
	GArray *rows = NULL;
	guint64 target = 0, origin;
	gboolean found = FALSE, nonspace = FALSE;
	guint i;
	GhosttyTerminalScrollViewport viewport = { 0 };

	g_return_val_if_fail (needle != NULL, FALSE);
	for (const char *p = needle; *p; p = g_utf8_next_char (p))
		nonspace |= !g_unichar_isspace (g_utf8_get_char (p));
	if (!nonspace)
		return FALSE;
	if (ghostty_terminal_get (terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar) != GHOSTTY_SUCCESS)
		return FALSE;
	origin = after_row != NULL ? *after_row : bar.offset;
	if (!find_rows (terminal, needle, &rows) || rows->len == 0)
	{
		if (rows != NULL)
			g_array_unref (rows);
		return FALSE;
	}

	if (!backwards)
	{
		for (i = 0; i < rows->len && !found; i++)
			if (g_array_index (rows, guint64, i) > origin)
			{
				target = g_array_index (rows, guint64, i);
				found = TRUE;
			}
		if (!found)
			target = g_array_index (rows, guint64, 0);
	}
	else
	{
		for (i = rows->len; i > 0 && !found; i--)
			if (g_array_index (rows, guint64, i - 1) < origin)
			{
				target = g_array_index (rows, guint64, i - 1);
				found = TRUE;
			}
		if (!found)
			target = g_array_index (rows, guint64, rows->len - 1);
	}
	g_array_unref (rows);

	/* Place the match near the middle of the viewport. */
	viewport.tag = GHOSTTY_SCROLL_VIEWPORT_ROW;
	viewport.value.row = target > bar.len / 2 ? (size_t) (target - bar.len / 2) : 0;
	ghostty_terminal_scroll_viewport (terminal, viewport);
	if (target_row != NULL) *target_row = target;
	return TRUE;
}

gboolean
termac_vt_find (GhosttyTerminal terminal, const char *needle, gboolean backwards)
{
	return termac_vt_find_target (terminal, needle, backwards, NULL, NULL);
}

/* Read the grid without updating a render state: Ghostty render updates
 * consume dirty flags owned by the terminal's persistent renderer. */
GArray *
termac_vt_find_matches (GhosttyTerminal terminal, guint columns,
                        guint rows, const char *needle)
{
	GArray *matches = g_array_new (FALSE, FALSE, sizeof (TermacVtMatch));
	g_autofree char *query = NULL;
	gboolean nonspace = FALSE;

	if (terminal == NULL || needle == NULL || !g_utf8_validate (needle, -1, NULL))
		return matches;
	for (const char *p = needle; *p; p = g_utf8_next_char (p))
		nonspace |= !g_unichar_isspace (g_utf8_get_char (p));
	if (!nonspace)
		return matches;
	query = g_utf8_casefold (needle, -1);
	for (guint row = 0; row < rows; row++)
	{
		g_autoptr(GString) text = g_string_new (NULL);
		g_autoptr(GString) cluster = g_string_new (NULL);
		g_autoptr(GArray) starts = g_array_new (FALSE, FALSE, sizeof (guint));
		g_autoptr(GArray) ends = g_array_new (FALSE, FALSE, sizeof (guint));

		for (guint column = 0; column < columns; column++)
		{
			GhosttyGridRef ref = GHOSTTY_INIT_SIZED (GhosttyGridRef);
			GhosttyCell raw;
			GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
			uint32_t local[32], *codepoints = local;
			size_t length = 0;
			GhosttyResult result;
			g_autofree uint32_t *extra = NULL;
			g_autofree char *folded = NULL;
			gsize bytes;
			guint end;

			if (!viewport_ref (terminal, column, row, &ref) ||
			    ghostty_grid_ref_cell (&ref, &raw) != GHOSTTY_SUCCESS ||
			    ghostty_cell_get (raw, GHOSTTY_CELL_DATA_WIDE, &wide) != GHOSTTY_SUCCESS)
				break;
			if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL || wide == GHOSTTY_CELL_WIDE_SPACER_HEAD)
				continue;
			result = ghostty_grid_ref_graphemes (&ref, local, G_N_ELEMENTS (local), &length);
			if (result == GHOSTTY_OUT_OF_SPACE)
			{
				extra = g_new (uint32_t, length);
				codepoints = extra;
				result = ghostty_grid_ref_graphemes (&ref, codepoints, length, &length);
			}
			if (result != GHOSTTY_SUCCESS)
				break;
			g_string_truncate (cluster, 0);
			for (size_t i = 0; i < length; i++)
				g_string_append_unichar (cluster, codepoints[i]);
			folded = length ? g_utf8_casefold (cluster->str, cluster->len) : g_strdup (" ");
			bytes = strlen (folded);
			end = MIN (column + (wide == GHOSTTY_CELL_WIDE_WIDE ? 2 : 1), columns);
			for (gsize i = 0; i < bytes; i++)
			{
				g_array_append_val (starts, column);
				g_array_append_val (ends, end);
			}
			g_string_append (text, folded);
		}
		for (const char *p = text->str; (p = strstr (p, query)) != NULL; p += strlen (query))
		{
			gsize start = p - text->str, last = start + strlen (query) - 1;
			TermacVtMatch match = { row, g_array_index (starts, guint, start), 0 };

			match.cells = g_array_index (ends, guint, last) - match.column;
			/* Casefold expansion may otherwise report one cell twice. */
			if (matches->len == 0 ||
			    g_array_index (matches, TermacVtMatch, matches->len - 1).row != row ||
			    g_array_index (matches, TermacVtMatch, matches->len - 1).column != match.column)
				g_array_append_val (matches, match);
		}
	}
	return matches;
}
