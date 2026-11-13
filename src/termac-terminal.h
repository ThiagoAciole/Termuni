/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gtk/gtk.h>
#include "termac-agent.h"

G_BEGIN_DECLS

#define TERMAC_TYPE_TERMINAL (termac_terminal_get_type ())
G_DECLARE_FINAL_TYPE (TermacTerminal, termac_terminal, TERMAC, TERMINAL, GtkDrawingArea)

/* Title used by the shell integration to report the exit status of a command. */
#define TERMAC_COMMAND_NOTIFICATION_TITLE "termac-command"

GtkWidget *termac_terminal_new (const char *cwd, gboolean shell_integration);
/* Run an explicit initial command after the user's shell initialization. */
GtkWidget *termac_terminal_new_command (const char *cwd, gboolean shell_integration, const char *command);

void        termac_terminal_set_agent (TermacTerminal *self, const TermacAgent *agent);
void        termac_terminal_set_font_size (TermacTerminal *self, guint size);
void        termac_terminal_set_palette (TermacTerminal *self, const char *name);

/* Background and foreground of a palette, for tinting the window chrome.
 * Returns FALSE for an unknown name. */
gboolean    termac_terminal_palette_colors (const char *name,
                                            GdkRGBA    *background,
                                            GdkRGBA    *foreground);
void        termac_terminal_set_scrollback_lines (TermacTerminal *self, guint lines);

const char *termac_terminal_get_session_id (TermacTerminal *self);
const char *termac_terminal_get_cwd (TermacTerminal *self);
const char *termac_terminal_get_error (TermacTerminal *self);
void        termac_terminal_retry (TermacTerminal *self);
gboolean    termac_terminal_has_foreground_job (TermacTerminal *self);

gboolean termac_terminal_copy (TermacTerminal *self);

/* The libghostty-vt terminal handle, for tests that drive the selection directly. */
gpointer termac_terminal_get_vt_handle (TermacTerminal *self);
guint    termac_terminal_get_font_size (TermacTerminal *self);
guint    termac_terminal_get_columns (TermacTerminal *self);
guint    termac_terminal_get_rows (TermacTerminal *self);
void     termac_terminal_get_cell_size (TermacTerminal *self, guint *width, guint *height);
const char *termac_terminal_get_preedit (TermacTerminal *self);
void     termac_terminal_set_preedit (TermacTerminal *self, const char *text);
void     termac_terminal_paste (TermacTerminal *self);
void     termac_terminal_copy_or_interrupt (TermacTerminal *self);
void     termac_terminal_line_start (TermacTerminal *self);
void     termac_terminal_select_all (TermacTerminal *self);
void     termac_terminal_insert_text (TermacTerminal *self, const char *text);
void     termac_terminal_scroll_pages (TermacTerminal *self, gint pages);
void     termac_terminal_scroll_to_bottom (TermacTerminal *self);
void termac_terminal_set_highlight (TermacTerminal *self, const char *needle);
guint termac_terminal_get_highlight_count (TermacTerminal *self);
gboolean termac_terminal_find (TermacTerminal *self, const char *text, gboolean backwards);

void          termac_terminal_add_attachment (TermacTerminal *self, const char *path);
GPtrArray    *termac_terminal_get_attachments (TermacTerminal *self);
void          termac_terminal_remove_attachment (TermacTerminal *self, guint index);

G_END_DECLS
