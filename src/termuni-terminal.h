/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gtk/gtk.h>
#include "termuni-agent.h"

G_BEGIN_DECLS

#define TERMUNI_TYPE_TERMINAL (termuni_terminal_get_type ())
G_DECLARE_FINAL_TYPE (TermuniTerminal, termuni_terminal, TERMUNI, TERMINAL, GtkDrawingArea)

/* Title used by the shell integration to report the exit status of a command. */
#define TERMUNI_COMMAND_NOTIFICATION_TITLE "termuni-command"

GtkWidget *termuni_terminal_new (const char *cwd, gboolean shell_integration);
/* Run an explicit initial command after the user's shell initialization. */
GtkWidget *termuni_terminal_new_command (const char *cwd, gboolean shell_integration, const char *command);

void        termuni_terminal_set_agent (TermuniTerminal *self, const TermuniAgent *agent);
void        termuni_terminal_set_font_size (TermuniTerminal *self, guint size);
void        termuni_terminal_set_palette (TermuniTerminal *self, const char *name);

/* Background and foreground of a palette, for tinting the window chrome.
 * Returns FALSE for an unknown name. */
gboolean    termuni_terminal_palette_colors (const char *name,
                                            GdkRGBA    *background,
                                            GdkRGBA    *foreground);
void        termuni_terminal_set_scrollback_lines (TermuniTerminal *self, guint lines);

const char *termuni_terminal_get_session_id (TermuniTerminal *self);
const char *termuni_terminal_get_cwd (TermuniTerminal *self);
const char *termuni_terminal_get_error (TermuniTerminal *self);
void        termuni_terminal_retry (TermuniTerminal *self);
gboolean    termuni_terminal_has_foreground_job (TermuniTerminal *self);
/* Types a command line into the shell already running in this terminal. */
void        termuni_terminal_run_command (TermuniTerminal *self, const char *command);

gboolean termuni_terminal_copy (TermuniTerminal *self);

/* The libghostty-vt terminal handle, for tests that drive the selection directly. */
gpointer termuni_terminal_get_vt_handle (TermuniTerminal *self);
guint    termuni_terminal_get_font_size (TermuniTerminal *self);
guint    termuni_terminal_get_columns (TermuniTerminal *self);
guint    termuni_terminal_get_rows (TermuniTerminal *self);
void     termuni_terminal_get_cell_size (TermuniTerminal *self, guint *width, guint *height);
const char *termuni_terminal_get_preedit (TermuniTerminal *self);
void     termuni_terminal_set_preedit (TermuniTerminal *self, const char *text);
void     termuni_terminal_paste (TermuniTerminal *self);
void     termuni_terminal_copy_or_interrupt (TermuniTerminal *self);
void     termuni_terminal_line_start (TermuniTerminal *self);
void     termuni_terminal_select_all (TermuniTerminal *self);
void     termuni_terminal_insert_text (TermuniTerminal *self, const char *text);
void     termuni_terminal_scroll_pages (TermuniTerminal *self, gint pages);
void     termuni_terminal_scroll_to_bottom (TermuniTerminal *self);
void termuni_terminal_set_highlight (TermuniTerminal *self, const char *needle);
guint termuni_terminal_get_highlight_count (TermuniTerminal *self);
gboolean termuni_terminal_find (TermuniTerminal *self, const char *text, gboolean backwards);

void          termuni_terminal_add_attachment (TermuniTerminal *self, const char *path);
GPtrArray    *termuni_terminal_get_attachments (TermuniTerminal *self);
void          termuni_terminal_remove_attachment (TermuniTerminal *self, guint index);

G_END_DECLS
