/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <gio/gio.h>

typedef struct _TermuniSessionStore TermuniSessionStore;
TermuniSessionStore *termuni_session_store_new (const char *path);
/* Immutable bytes are referenced. Async requests coalesce while I/O is active.
 * flush waits for this request (and every older write) without pumping GTK. */
void termuni_session_store_save (TermuniSessionStore *store, GBytes *bytes, gboolean flush);
/* Drains the latest queued snapshot, joins the worker, then frees all state. */
void termuni_session_store_free (TermuniSessionStore *store);
gboolean termuni_session_store_write (const char *path, GBytes *bytes);
