/* termac-application.h
 *
 * Copyright 2026 Thiago Aciole
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "termac-window.h"

G_BEGIN_DECLS

#define TERMAC_TYPE_APPLICATION (termac_application_get_type())

G_DECLARE_FINAL_TYPE (TermacApplication, termac_application, TERMAC, APPLICATION, AdwApplication)

TermacApplication *termac_application_new (const char        *application_id,
                                           GApplicationFlags  flags);
GSettings *termac_application_get_settings (TermacApplication *self);

/* Saves the tabs of a window so they can be restored at the next start. */
void termac_application_save_session_now (TermacApplication *self);

/* Saves synchronously when a window closes, unless the app is quitting. */
void termac_application_window_closing (TermacApplication *self);

/* Schedules a save shortly after a change in tabs, folders or titles. */
void termac_application_request_session_save (TermacApplication *self);

G_END_DECLS
