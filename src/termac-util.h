/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	TERMAC_IMAGE_FORMAT_NONE = 0,
	TERMAC_IMAGE_FORMAT_PNG,
	TERMAC_IMAGE_FORMAT_JPEG,
} TermacImageFormat;

/* Largest attachment accepted for agent prompts, and largest dimension. */
#define TERMAC_ATTACHMENT_MAX_BYTES (20 * 1024 * 1024)
#define TERMAC_ATTACHMENT_MAX_DIMENSION 8000

TermacImageFormat termac_image_probe (const guint8 *data,
                                      gsize         length,
                                      guint        *width,
                                      guint        *height);

gboolean termac_url_at_column (const char *line,
                               guint       column,
                               gsize      *start,
                               gsize      *end);
gboolean termac_url_span_at_column (const char *line,
                                    guint       column,
                                    gsize      *start,
                                    gsize      *end,
                                    guint      *first_column,
                                    guint      *last_column);

char *termac_display_path (const char *path,
                           const char *home);

/* Turns an OSC 7 value (file://host/path, or a plain path) into a path. */
char *termac_path_from_pwd (const char *value);

G_END_DECLS
