/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <gio/gio.h>

typedef struct _TermacSessionStore TermacSessionStore;
TermacSessionStore *termac_session_store_new (const char *path);
/* Immutable bytes are referenced. Async requests coalesce while I/O is active.
 * flush waits for this request (and every older write) without pumping GTK. */
void termac_session_store_save (TermacSessionStore *store, GBytes *bytes, gboolean flush);
/* Drains the latest queued snapshot, joins the worker, then frees all state. */
void termac_session_store_free (TermacSessionStore *store);
gboolean termac_session_store_write (const char *path, GBytes *bytes);
