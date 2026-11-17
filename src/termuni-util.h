/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	TERMUNI_IMAGE_FORMAT_NONE = 0,
	TERMUNI_IMAGE_FORMAT_PNG,
	TERMUNI_IMAGE_FORMAT_JPEG,
} TermuniImageFormat;

/* Largest attachment accepted for agent prompts, and largest dimension. */
#define TERMUNI_ATTACHMENT_MAX_BYTES (20 * 1024 * 1024)
#define TERMUNI_ATTACHMENT_MAX_DIMENSION 8000

TermuniImageFormat termuni_image_probe (const guint8 *data,
                                      gsize         length,
                                      guint        *width,
                                      guint        *height);

gboolean termuni_url_at_column (const char *line,
                               guint       column,
                               gsize      *start,
                               gsize      *end);
gboolean termuni_url_span_at_column (const char *line,
                                    guint       column,
                                    gsize      *start,
                                    gsize      *end,
                                    guint      *first_column,
                                    guint      *last_column);

char *termuni_display_path (const char *path,
                           const char *home);

/* Turns an OSC 7 value (file://host/path, or a plain path) into a path. */
char *termuni_path_from_pwd (const char *value);

/* Build argv before fork; paths remain individual arguments, never shell code. */
char *termuni_session_directory (const char *cwd);
char **termuni_shell_argv (const char *shell, const char *rcfile);
char **termuni_shell_command_argv (const char *shell, const char *rcfile, const char *command);

/* Only background views with explicitly enabled, eligible completion events. */
gboolean termuni_should_notify (gboolean setting_enabled, gboolean window_active,
                               gboolean tab_selected, gboolean took_long_enough);

G_END_DECLS
