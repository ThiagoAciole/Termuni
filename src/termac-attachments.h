/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Attachments live in the user cache, one folder per terminal session:
 * $XDG_CACHE_HOME/termac/attachments/<session-id>/ */
char *termac_attachments_session_dir (const char *session_id);

/* Validates and stores image bytes (PNG or JPEG) for a session. On success
 * the returned path points to a complete file, written atomically. */
char *termac_attachments_store (const char *session_id,
                                const guint8 *data,
                                gsize         length,
                                GError      **error);

/* Same as termac_attachments_store, reading the image from a file. */
char *termac_attachments_import (const char *session_id,
                                 const char *source_path,
                                 GError    **error);

void termac_attachments_clear_session (const char *session_id);

/* Removes stored attachments older than max_age_seconds, in every session. */
void termac_attachments_prune (gint64 max_age_seconds);

/* Text that a shell will read back as exactly one argument. */
char *termac_attachments_quote (const char *path);

G_END_DECLS
