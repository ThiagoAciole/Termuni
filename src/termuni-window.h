/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define TERMUNI_TYPE_WINDOW (termuni_window_get_type())

G_DECLARE_FINAL_TYPE (TermuniWindow, termuni_window, TERMUNI, WINDOW, AdwApplicationWindow)

void termuni_window_launch_agent (TermuniWindow *self);

void termuni_window_set_font_size (TermuniWindow *self, guint size);
void termuni_window_sync_settings (TermuniWindow *self);
void termuni_window_set_palette (TermuniWindow *self, const char *name);

/* TRUE when a window action owns this key combination, so the terminal must
 * let GTK handle it instead of sending it to the shell. */
gboolean termuni_window_shortcut_is_reserved (TermuniWindow   *self,
                                             guint           keyval,
                                             GdkModifierType state);

/* Activates the tab whose terminal has the given session id. */
gboolean termuni_window_show_session (TermuniWindow *self, const char *session_id);

/* Restores tabs saved by termuni_window_session_state. */
void termuni_window_restore_tabs (TermuniWindow *self, GVariant *tabs, gint selected);

/* (cwd: s, manual title: s) tuples and the index of the selected tab. */
GVariant *termuni_window_session_state (TermuniWindow *self, gint *selected);

/* Number of tabs with a foreground process; used when the app quits. */
guint termuni_window_busy_tabs (TermuniWindow *self);

gboolean termuni_window_attach_file_list (TermuniWindow *self, GdkFileList *list);

G_END_DECLS
