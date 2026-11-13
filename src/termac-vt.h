/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>
#include <ghostty/vt.h>

G_BEGIN_DECLS

/* Helpers over libghostty-vt that do not depend on GTK. */

typedef struct _TermacVtSelector TermacVtSelector;

TermacVtSelector *termac_vt_selector_new (void);
void              termac_vt_selector_free (TermacVtSelector *selector);

/* Applies a pointer event to the selection and installs the result in the
 * terminal. Returns TRUE when a selection exists afterwards. */
gboolean termac_vt_selector_event (TermacVtSelector                  *selector,
                                   GhosttyTerminal                    terminal,
                                   GhosttySelectionGestureEventType   type,
                                   guint                              column,
                                   guint                              row,
                                   double                             pixel_x,
                                   double                             pixel_y,
                                   guint64                            time_ns,
                                   const GhosttySelectionGestureGeometry *geometry);

void termac_vt_selection_clear (GhosttyTerminal terminal);
void termac_vt_select_all (GhosttyTerminal terminal);
void termac_vt_clear_selection (GhosttyTerminal terminal);

/* Text of the active selection, unwrapped and trimmed. */
gboolean termac_vt_selection_text (GhosttyTerminal terminal,
                                   char          **text);

/* Text of a viewport row, not unwrapped. */
gboolean termac_vt_viewport_row_text (GhosttyTerminal terminal,
                                      guint           columns,
                                      guint           row,
                                      char          **text);

gboolean termac_vt_bracketed_paste (GhosttyTerminal terminal);
gboolean termac_vt_mouse_tracking (GhosttyTerminal terminal);
gboolean termac_vt_alternate_screen (GhosttyTerminal terminal);

/* Returns the bytes to write to the pty for text pasted by the user. */
char *termac_vt_encode_paste (const char *text,
                              gsize       length,
                              gboolean    bracketed,
                              gsize      *out_length);

/* Searches the whole screen, scrolls the nearest match into view and
 * returns TRUE when one was found. */
gboolean termac_vt_find (GhosttyTerminal terminal,
                         const char     *needle,
                         gboolean        backwards);

/* As find(), with a stable screen-row anchor for result navigation. */
gboolean termac_vt_find_target (GhosttyTerminal terminal, const char *needle,
                                gboolean backwards, const guint64 *after_row,
                                guint64 *target_row);

typedef struct { guint row; guint column; guint cells; } TermacVtMatch;

/* Viewport matches in terminal cells; caller owns the returned array. */
GArray *termac_vt_find_matches (GhosttyTerminal terminal, guint columns,
                               guint rows, const char *needle);

G_END_DECLS
