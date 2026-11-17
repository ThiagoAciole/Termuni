/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <gio/gio.h>
G_BEGIN_DECLS
GSettings *termuni_settings_new (const char *application_id, const char *fallback_schema_dir);
G_END_DECLS
