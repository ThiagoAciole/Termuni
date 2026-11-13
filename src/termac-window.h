/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define TERMAC_TYPE_WINDOW (termac_window_get_type())

G_DECLARE_FINAL_TYPE (TermacWindow, termac_window, TERMAC, WINDOW, AdwApplicationWindow)

void termac_window_set_font_size (TermacWindow *self, guint size);
void termac_window_sync_settings (TermacWindow *self);
void termac_window_set_palette (TermacWindow *self, const char *name);

/* TRUE when a window action owns this key combination, so the terminal must
 * let GTK handle it instead of sending it to the shell. */
gboolean termac_window_shortcut_is_reserved (TermacWindow   *self,
                                             guint           keyval,
                                             GdkModifierType state);

/* Activates the tab whose terminal has the given session id. */
gboolean termac_window_show_session (TermacWindow *self, const char *session_id);

/* Restores tabs saved by termac_window_session_state. */
void termac_window_restore_tabs (TermacWindow *self, GVariant *tabs, gint selected);

/* (cwd: s, manual title: s) tuples and the index of the selected tab. */
GVariant *termac_window_session_state (TermacWindow *self, gint *selected);

/* Number of tabs with a foreground process; used when the app quits. */
guint termac_window_busy_tabs (TermacWindow *self);

gboolean termac_window_attach_file_list (TermacWindow *self, GdkFileList *list);

G_END_DECLS
