/* termuni-application.h
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

#include "termuni-window.h"
#include "termuni-agent.h"

G_BEGIN_DECLS

#define TERMUNI_TYPE_APPLICATION (termuni_application_get_type())

G_DECLARE_FINAL_TYPE (TermuniApplication, termuni_application, TERMUNI, APPLICATION, AdwApplication)

TermuniApplication *termuni_application_new (const char        *application_id,
                                           GApplicationFlags  flags);
GSettings *termuni_application_get_settings (TermuniApplication *self);
const TermuniAgent *termuni_application_get_agent (TermuniApplication *self);

/* Explicit synchronous checkpoint of all windows; waits for ordered disk I/O.
 * Failures are logged and preserve the previous consistent file. */
void termuni_application_save_session_now (TermuniApplication *self);

/* Queues an immutable snapshot when a window closes, unless quitting. */
void termuni_application_window_closing (TermuniApplication *self);

/* Schedules a save shortly after a change in tabs, folders or titles. */
void termuni_application_request_session_save (TermuniApplication *self);

G_END_DECLS
