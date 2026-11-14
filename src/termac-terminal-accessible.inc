/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The accessible document is the visible grid, including history when scrolled.
 * Offsets count Unicode codepoints (not bytes, cells or grapheme clusters).
 * Never call render_state_update here: it consumes the renderer's dirty flags.
 * This file belongs to TermacTerminal and runs only on GTK's main thread. */
typedef struct {
	GString *text;
	GArray *starts, *ends;
	guint length, caret;
	guint cols, rows;
} TerminalAccessibleSnapshot;

static void
terminal_accessible_snapshot_free (TerminalAccessibleSnapshot *s)
{
	g_string_free (s->text, TRUE);
	g_array_unref (s->starts);
	g_array_unref (s->ends);
	g_free (s);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (TerminalAccessibleSnapshot, terminal_accessible_snapshot_free)

static gboolean
terminal_accessible_ref (TermacTerminal *self, guint x, guint y, GhosttyGridRef *ref)
{
	GhosttyPoint p = { .tag = GHOSTTY_POINT_TAG_VIEWPORT };
	p.value.coordinate.x = x;
	p.value.coordinate.y = y;
	return self->terminal && ghostty_terminal_grid_ref (self->terminal, p, ref) == GHOSTTY_SUCCESS;
}

static TerminalAccessibleSnapshot *
terminal_accessible_snapshot (TermacTerminal *self)
{
	TerminalAccessibleSnapshot *s = g_new0 (TerminalAccessibleSnapshot, 1);
	uint16_t cx = 0, cy = 0;
	bool active = false;
	s->text = g_string_new (NULL);
	s->starts = g_array_new (FALSE, FALSE, sizeof (guint));
	s->ends = g_array_new (FALSE, FALSE, sizeof (guint));
	s->cols = self->terminal ? self->cols : 0;
	s->rows = self->terminal ? self->rows : 0;
	if (self->terminal) {
		ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_CURSOR_X, &cx);
		ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_CURSOR_Y, &cy);
		ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_VIEWPORT_ACTIVE, &active);
	}
	for (guint y = 0; y < s->rows; y++) {
		for (guint x = 0; x < s->cols; x++) {
			GhosttyGridRef ref = GHOSTTY_INIT_SIZED (GhosttyGridRef);
			GhosttyCell cell;
			GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
			uint32_t local[32], *points = local;
			g_autofree uint32_t *extra = NULL;
			size_t n = 0;
			guint start = s->length;
			if (active && x == cx && y == cy) s->caret = start;
			if (terminal_accessible_ref (self, x, y, &ref) &&
			    ghostty_grid_ref_cell (&ref, &cell) == GHOSTTY_SUCCESS) {
				ghostty_cell_get (cell, GHOSTTY_CELL_DATA_WIDE, &wide);
				if (wide != GHOSTTY_CELL_WIDE_SPACER_TAIL && wide != GHOSTTY_CELL_WIDE_SPACER_HEAD) {
					GhosttyResult result = ghostty_grid_ref_graphemes (&ref, local, G_N_ELEMENTS (local), &n);
					if (result == GHOSTTY_OUT_OF_SPACE) {
						extra = g_new (uint32_t, n); points = extra;
						result = ghostty_grid_ref_graphemes (&ref, points, n, &n);
					}
					if (result == GHOSTTY_SUCCESS)
						for (gsize i = 0; i < n; i++) { g_string_append_unichar (s->text, points[i]); s->length++; }
					if (n == 0) { g_string_append_c (s->text, ' '); s->length++; }
				} else if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL && s->starts->len) {
					start = g_array_index (s->starts, guint, s->starts->len - 1);
				}
			}
			g_array_append_val (s->starts, start);
			g_array_append_val (s->ends, s->length);
		}
		if (y + 1 < s->rows) { g_string_append_c (s->text, '\n'); s->length++; }
	}
	return s;
}

static GBytes *
terminal_accessible_slice (TerminalAccessibleSnapshot *s, guint start, guint end)
{
	const char *a = g_utf8_offset_to_pointer (s->text->str, MIN (start, s->length));
	const char *b = g_utf8_offset_to_pointer (s->text->str, MIN (MAX (start, end), s->length));
	return g_bytes_new (a, b - a);
}

static GBytes *
terminal_accessible_contents (GtkAccessibleText *text, guint start, guint end)
{
	TermacTerminal *self = TERMAC_TERMINAL (text);
	/* A remove notification must still expose the old text to GTK/AT-SPI. */
	if (self->accessible_removing && self->accessible_text) {
		TerminalAccessibleSnapshot old = { .text = self->accessible_text,
			.length = g_utf8_strlen (self->accessible_text->str, -1) };
		return terminal_accessible_slice (&old, start, end);
	}
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (self);
	return terminal_accessible_slice (s, start, end);
}

static GBytes *
terminal_accessible_contents_at (GtkAccessibleText *text, guint offset,
                                 GtkAccessibleTextGranularity granularity, guint *start, guint *end)
{
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (TERMAC_TERMINAL (text));
	g_autofree PangoLogAttr *attrs = g_new0 (PangoLogAttr, s->length + 1);
	guint a = MIN (offset, s->length), b = a;
	pango_get_log_attrs (s->text->str, s->text->len, -1, pango_language_get_default (), attrs, s->length + 1);
	if (a < s->length) {
		if (granularity == GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER) b++;
		else if (granularity == GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE ||
		         granularity == GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH) {
			const char *p = g_utf8_offset_to_pointer (s->text->str, a);
			while (a && *g_utf8_offset_to_pointer (s->text->str, a - 1) != '\n') a--;
			while (b < s->length && *p != '\n') { b++; p = g_utf8_next_char (p); }
			if (b < s->length) b++;
		} else {
			gboolean sentence = granularity == GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE;
			while (a && !(sentence ? attrs[a].is_sentence_start : attrs[a].is_word_start)) a--;
			b = MIN (offset + 1, s->length);
			while (b < s->length && !(sentence ? attrs[b].is_sentence_start : attrs[b].is_word_start)) b++;
		}
	}
	*start = a; *end = b;
	return terminal_accessible_slice (s, a, b);
}

static guint
terminal_accessible_caret (GtkAccessibleText *text)
{
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (TERMAC_TERMINAL (text));
	return s->caret;
}

static gboolean
terminal_accessible_selection (GtkAccessibleText *text, gsize *n_ranges, GtkAccessibleTextRange **ranges)
{
	TermacTerminal *self = TERMAC_TERMINAL (text);
	g_autoptr(TerminalAccessibleSnapshot) s = NULL;
	GhosttySelection selection = GHOSTTY_INIT_SIZED (GhosttySelection);
	GhosttyPointCoordinate a, b, origin;
	GhosttyGridRef ref = GHOSTTY_INIT_SIZED (GhosttyGridRef);
	g_autoptr(GArray) result = g_array_new (FALSE, FALSE, sizeof (GtkAccessibleTextRange));
	*n_ranges = 0; if (ranges) *ranges = NULL;
	if (!self->terminal ||
	    ghostty_terminal_get (self->terminal, GHOSTTY_TERMINAL_DATA_SELECTION, &selection) != GHOSTTY_SUCCESS) return FALSE;
	s = terminal_accessible_snapshot (self);
	if (!s->cols || !s->rows ||
	    !terminal_accessible_ref (self, 0, 0, &ref) ||
	    ghostty_terminal_point_from_grid_ref (self->terminal, &ref, GHOSTTY_POINT_TAG_SCREEN, &origin) != GHOSTTY_SUCCESS ||
	    ghostty_terminal_point_from_grid_ref (self->terminal, &selection.start, GHOSTTY_POINT_TAG_SCREEN, &a) != GHOSTTY_SUCCESS ||
	    ghostty_terminal_point_from_grid_ref (self->terminal, &selection.end, GHOSTTY_POINT_TAG_SCREEN, &b) != GHOSTTY_SUCCESS) return FALSE;
	if (a.y > b.y || (a.y == b.y && a.x > b.x)) { GhosttyPointCoordinate tmp = a; a = b; b = tmp; }
	for (guint y = 0; y < s->rows; y++) {
		guint row = origin.y + y, left, right;
		if (row < a.y || row > b.y) continue;
		left = selection.rectangle ? MIN (a.x, b.x) : (row == a.y ? a.x : 0);
		right = selection.rectangle ? MAX (a.x, b.x) : (row == b.y ? b.x : s->cols - 1);
		left = MIN (left, s->cols - 1); right = MIN (right, s->cols - 1);
		GtkAccessibleTextRange range = { g_array_index (s->starts, guint, y * s->cols + left), 0 };
		range.length = g_array_index (s->ends, guint, y * s->cols + right) - range.start;
		if (!selection.rectangle && result->len) {
			GtkAccessibleTextRange *first = &g_array_index (result, GtkAccessibleTextRange, 0);
			first->length = range.start + range.length - first->start;
		} else if (range.length) g_array_append_val (result, range);
	}
	*n_ranges = result->len;
	if (ranges && result->len) *ranges = g_memdup2 (result->data, result->len * sizeof (GtkAccessibleTextRange));
	return result->len > 0;
}

static gboolean
terminal_accessible_set_caret (GtkAccessibleText *text, guint offset)
{
	/* Terminal cursor belongs to the application running on the PTY. */
	return FALSE;
}

static gboolean
terminal_accessible_set_selection (GtkAccessibleText *text, gsize i, GtkAccessibleTextRange *range)
{
	/* Arbitrary AT text ranges can split a grapheme or rectangular selection;
	 * unsupported rather than pretending to move the PTY application's cursor. */
	return FALSE;
}

static void
terminal_accessible_default_attributes (GtkAccessibleText *text, char ***names, char ***values)
{
	if (names) *names = g_new0 (char *, 1);
	if (values) *values = g_new0 (char *, 1);
}

static gboolean
terminal_accessible_attributes (GtkAccessibleText *text, guint offset, gsize *n,
                                GtkAccessibleTextRange **ranges, char ***names, char ***values)
{
	*n = 0;
	if (ranges) *ranges = NULL;
	if (names) *names = NULL;
	if (values) *values = NULL;
	return FALSE;
}

static gboolean
terminal_accessible_extents (GtkAccessibleText *text, guint start, guint end, graphene_rect_t *extents)
{
	TermacTerminal *self = TERMAC_TERMINAL (text);
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (self);
	gboolean found = FALSE;
	if (start >= s->length || end > s->length || end < start) return FALSE;
	for (guint i = 0; i < s->starts->len; i++) {
		guint a = g_array_index (s->starts, guint, i), b = g_array_index (s->ends, guint, i);
		graphene_rect_t cell;
		if (!(a <= start && start < b) && !(a < end && b > start)) continue;
		graphene_rect_init (&cell, TERMAC_TERMINAL_INSET_START + (i % s->cols) * self->cell_width,
		                   TERMAC_TERMINAL_INSET_TOP + (i / s->cols) * self->cell_height,
		                   self->cell_width, self->cell_height);
		if (found) graphene_rect_union (extents, &cell, extents);
		else { *extents = cell; found = TRUE; }
	}
	return found;
}

static gboolean
terminal_accessible_offset (GtkAccessibleText *text, const graphene_point_t *point, guint *offset)
{
	TermacTerminal *self = TERMAC_TERMINAL (text);
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (self);
	double x = point->x - TERMAC_TERMINAL_INSET_START, y = point->y - TERMAC_TERMINAL_INSET_TOP;
	if (!isfinite (x) || !isfinite (y) || !self->cell_width || !self->cell_height || x < 0 || y < 0 ||
	    x >= s->cols * self->cell_width || y >= s->rows * self->cell_height) return FALSE;
	*offset = g_array_index (s->starts, guint,
	                        (guint) (y / self->cell_height) * s->cols + (guint) (x / self->cell_width));
	return TRUE;
}

static void
terminal_accessible_text_init (GtkAccessibleTextInterface *iface)
{
	iface->get_contents = terminal_accessible_contents;
	iface->get_contents_at = terminal_accessible_contents_at;
	iface->get_caret_position = terminal_accessible_caret;
	iface->get_selection = terminal_accessible_selection;
	iface->get_attributes = terminal_accessible_attributes;
	iface->get_extents = terminal_accessible_extents;
	iface->get_offset = terminal_accessible_offset;
	iface->get_default_attributes = terminal_accessible_default_attributes;
	iface->set_caret_position = terminal_accessible_set_caret;
	iface->set_selection = terminal_accessible_set_selection;
}

static void
terminal_accessible_update (TermacTerminal *self)
{
	g_autoptr(TerminalAccessibleSnapshot) s = terminal_accessible_snapshot (self);
	gboolean changed = !self->accessible_text || strcmp (self->accessible_text->str, s->text->str);
	gsize n = 0;
	GtkAccessibleTextRange *ranges = NULL;
	g_autoptr(GBytes) selection = NULL;
	terminal_accessible_selection (GTK_ACCESSIBLE_TEXT (self), &n, &ranges);
	selection = g_bytes_new_take (ranges, n * sizeof *ranges);
	if (changed) {
		const char *old_text = self->accessible_text ? self->accessible_text->str : "";
		guint old_length = g_utf8_strlen (old_text, -1), prefix = 0, suffix = 0;
		const char *a = old_text, *b = s->text->str;
		while (*a && *b && g_utf8_get_char (a) == g_utf8_get_char (b)) {
			prefix++; a = g_utf8_next_char (a); b = g_utf8_next_char (b);
		}
		a = old_text + strlen (old_text); b = s->text->str + s->text->len;
		while (suffix < old_length - prefix && suffix < s->length - prefix) {
			const char *pa = g_utf8_prev_char (a), *pb = g_utf8_prev_char (b);
			if (g_utf8_get_char (pa) != g_utf8_get_char (pb)) break;
			suffix++; a = pa; b = pb;
		}
		self->accessible_removing = TRUE;
		if (old_length - suffix > prefix)
			gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self), GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE, prefix, old_length - suffix);
		self->accessible_removing = FALSE;
		if (self->accessible_text) g_string_free (self->accessible_text, TRUE);
		self->accessible_text = g_string_new_len (s->text->str, s->text->len);
		if (s->length - suffix > prefix)
			gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self), GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT, prefix, s->length - suffix);
	}
	if (changed || self->accessible_caret != s->caret) {
		self->accessible_caret = s->caret;
		gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (self));
	}
	if (!self->accessible_selection || !g_bytes_equal (self->accessible_selection, selection)) {
		g_clear_pointer (&self->accessible_selection, g_bytes_unref);
		self->accessible_selection = g_bytes_ref (selection);
		gtk_accessible_text_update_selection_bound (GTK_ACCESSIBLE_TEXT (self));
	}
}
