/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>
#include <ghostty/vt.h>

G_BEGIN_DECLS

/* Helpers over libghostty-vt that do not depend on GTK. */

typedef struct _TermuniVtSelector TermuniVtSelector;

TermuniVtSelector *termuni_vt_selector_new (void);
void              termuni_vt_selector_free (TermuniVtSelector *selector);

/* Applies a pointer event to the selection and installs the result in the
 * terminal. Returns TRUE when a selection exists afterwards. */
gboolean termuni_vt_selector_event (TermuniVtSelector                  *selector,
                                   GhosttyTerminal                    terminal,
                                   GhosttySelectionGestureEventType   type,
                                   guint                              column,
                                   guint                              row,
                                   double                             pixel_x,
                                   double                             pixel_y,
                                   guint64                            time_ns,
                                   const GhosttySelectionGestureGeometry *geometry);

void termuni_vt_selection_clear (GhosttyTerminal terminal);
void termuni_vt_select_all (GhosttyTerminal terminal);
void termuni_vt_clear_selection (GhosttyTerminal terminal);

/* Text of the active selection, unwrapped and trimmed. */
gboolean termuni_vt_selection_text (GhosttyTerminal terminal,
                                   char          **text);

/* Text of a viewport row, not unwrapped. */
gboolean termuni_vt_viewport_row_text (GhosttyTerminal terminal,
                                      guint           columns,
                                      guint           row,
                                      char          **text);

gboolean termuni_vt_bracketed_paste (GhosttyTerminal terminal);
gboolean termuni_vt_mouse_tracking (GhosttyTerminal terminal);
gboolean termuni_vt_alternate_screen (GhosttyTerminal terminal);

/* Returns the bytes to write to the pty for text pasted by the user. */
char *termuni_vt_encode_paste (const char *text,
                              gsize       length,
                              gboolean    bracketed,
                              gsize      *out_length);

/* Searches the whole screen, scrolls the nearest match into view and
 * returns TRUE when one was found. */
gboolean termuni_vt_find (GhosttyTerminal terminal,
                         const char     *needle,
                         gboolean        backwards);

/* As find(), with a stable screen-row anchor for result navigation. */
gboolean termuni_vt_find_target (GhosttyTerminal terminal, const char *needle,
                                gboolean backwards, const guint64 *after_row,
                                guint64 *target_row);

typedef struct { guint row; guint column; guint cells; } TermuniVtMatch;

/* Viewport matches in terminal cells; caller owns the returned array. */
GArray *termuni_vt_find_matches (GhosttyTerminal terminal, guint columns,
                               guint rows, const char *needle);

/* Safe HTTP(S) textual/OSC8 link at a viewport cell; caller frees the URI.
 * Bounds are cell columns, with last_column exclusive. */
char *termuni_vt_link_at (GhosttyTerminal terminal, guint columns, guint row,
                        guint column, guint *first_column, guint *last_column);

G_END_DECLS
