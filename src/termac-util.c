/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termac-util.h"

#include <string.h>

static guint
read_be16 (const guint8 *data)
{
	return ((guint) data[0] << 8) | data[1];
}

static guint
read_be32 (const guint8 *data)
{
	return ((guint) data[0] << 24) | ((guint) data[1] << 16) |
	       ((guint) data[2] << 8) | data[3];
}

static gboolean
probe_png (const guint8 *data,
           gsize         length,
           guint        *width,
           guint        *height)
{
	static const guint8 signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

	if (length < 24 || memcmp (data, signature, sizeof signature) != 0)
		return FALSE;
	if (memcmp (data + 12, "IHDR", 4) != 0)
		return FALSE;
	*width = read_be32 (data + 16);
	*height = read_be32 (data + 20);
	return TRUE;
}

static gboolean
is_jpeg_sof (guint8 marker)
{
	/* SOF0-SOF15 except DHT (C4), JPG (C8) and DAC (CC). */
	return marker >= 0xc0 && marker <= 0xcf &&
	       marker != 0xc4 && marker != 0xc8 && marker != 0xcc;
}

static gboolean
probe_jpeg (const guint8 *data,
            gsize         length,
            guint        *width,
            guint        *height)
{
	gsize pos = 2;

	if (length < 4 || data[0] != 0xff || data[1] != 0xd8)
		return FALSE;
	while (pos + 4 <= length)
	{
		guint8 marker;
		guint segment_length;

		if (data[pos] != 0xff)
			return FALSE;
		marker = data[pos + 1];
		if (marker == 0xff)
		{
			pos++;
			continue;
		}
		/* Standalone markers carry no length: TEM, RSTn, SOI, EOI. */
		if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd9))
		{
			pos += 2;
			continue;
		}
		segment_length = read_be16 (data + pos + 2);
		if (segment_length < 2 || pos + 2 + segment_length > length)
			return FALSE;
		if (is_jpeg_sof (marker))
		{
			if (segment_length < 8)
				return FALSE;
			*height = read_be16 (data + pos + 5);
			*width = read_be16 (data + pos + 7);
			return TRUE;
		}
		pos += 2 + segment_length;
	}
	return FALSE;
}

/**
 * termac_image_probe:
 * @data: file contents, at least the header
 * @length: number of bytes in @data
 * @width: (out): image width in pixels
 * @height: (out): image height in pixels
 *
 * Identifies a PNG or JPEG by its signature and reads its dimensions.
 * Only the header is inspected; the image is not decoded.
 *
 * Returns: the detected format, or %TERMAC_IMAGE_FORMAT_NONE
 */
TermacImageFormat
termac_image_probe (const guint8 *data,
                    gsize         length,
                    guint        *width,
                    guint        *height)
{
	g_return_val_if_fail (data != NULL, TERMAC_IMAGE_FORMAT_NONE);
	g_return_val_if_fail (width != NULL && height != NULL, TERMAC_IMAGE_FORMAT_NONE);

	if (probe_png (data, length, width, height))
		return TERMAC_IMAGE_FORMAT_PNG;
	if (probe_jpeg (data, length, width, height))
		return TERMAC_IMAGE_FORMAT_JPEG;
	return TERMAC_IMAGE_FORMAT_NONE;
}

static gboolean
is_url_char (char c)
{
	if (g_ascii_isalnum (c))
		return TRUE;
	return c != '\0' && strchr ("-._~:/?#[]@!$&'()*+,;=%", c) != NULL;
}

static gboolean
is_trailing_punctuation (char c)
{
	return c != '\0' && strchr (".,;:!?)]'\"", c) != NULL;
}

/* Number of UTF-8 characters before the byte offset; used as the column. */
static guint
columns_before (const char *line,
                const char *position)
{
	guint count = 0;
	const char *p;

	for (p = line; p < position; p = g_utf8_next_char (p))
		count++;
	return count;
}

/**
 * termac_url_at_column:
 * @line: UTF-8 text of one terminal row
 * @column: column measured in characters from the start of @line
 * @start: (out): byte offset of the URL start
 * @end: (out): byte offset just after the URL
 *
 * Finds an http or https URL that covers @column. Columns count
 * characters, so wide characters before a URL shift its hit area.
 *
 * Returns: %TRUE if a URL covers the column
 */
gboolean
termac_url_span_at_column (const char *line,
                           guint       column,
                           gsize      *start,
                           gsize      *end,
                           guint      *first_column,
                           guint      *last_column)
{
	const char *search = line;

	g_return_val_if_fail (line != NULL, FALSE);

	while ((search = strstr (search, "http")) != NULL)
	{
		const char *candidate = search;
		const char *stop;
		guint first;
		guint last;

		if (g_str_has_prefix (candidate, "https://"))
			candidate += strlen ("https://");
		else if (g_str_has_prefix (candidate, "http://"))
			candidate += strlen ("http://");
		else
		{
			search++;
			continue;
		}
		stop = candidate;
		while (is_url_char (*stop))
			stop++;
		while (stop > candidate && is_trailing_punctuation (*(stop - 1)))
			stop--;
		if (stop == candidate)
		{
			search++;
			continue;
		}
		first = columns_before (line, search);
		last = columns_before (line, stop);
		if (column >= first && column < last)
		{
			if (start != NULL)
				*start = (gsize) (search - line);
			if (end != NULL)
				*end = (gsize) (stop - line);
			if (first_column != NULL)
				*first_column = first;
			if (last_column != NULL)
				*last_column = last;
			return TRUE;
		}
		search = stop;
	}
	return FALSE;
}

gboolean
termac_url_at_column (const char *line,
                      guint       column,
                      gsize      *start,
                      gsize      *end)
{
	return termac_url_span_at_column (line, column, start, end, NULL, NULL);
}

/**
 * termac_display_path:
 * @path: absolute path
 * @home: (nullable): home directory to abbreviate as "~"
 *
 * Returns: a newly allocated path for display
 */
char *
termac_display_path (const char *path,
                     const char *home)
{
	gsize home_length;

	g_return_val_if_fail (path != NULL, NULL);

	if (home == NULL || *home == '\0')
		return g_strdup (path);
	home_length = strlen (home);
	if (strncmp (path, home, home_length) != 0)
		return g_strdup (path);
	if (path[home_length] == '\0')
		return g_strdup ("~");
	if (path[home_length] != '/')
		return g_strdup (path);
	return g_strconcat ("~", path + home_length, NULL);
}

/**
 * termac_path_from_pwd:
 * @value: working directory reported by the shell
 *
 * OSC 7 reports "file://host/path". Returns the path part, or a copy of
 * @value when it is already a plain path.
 *
 * Returns: a newly allocated path
 */
char *
termac_path_from_pwd (const char *value)
{
	const char *slash;

	g_return_val_if_fail (value != NULL, NULL);
	if (!g_str_has_prefix (value, "file://"))
		return g_strdup (value);
	slash = strchr (value + strlen ("file://"), '/');
	return g_uri_unescape_string (slash != NULL ? slash : "/", NULL);
}

char **
termac_shell_command_argv (const char *shell, const char *rcfile, const char *command)
{
	GPtrArray *args = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (args, g_strdup (shell));
	if (rcfile != NULL)
	{
		g_ptr_array_add (args, g_strdup ("--rcfile"));
		g_ptr_array_add (args, g_strdup (rcfile));
	}
	g_ptr_array_add (args, g_strdup ("-i"));
	if (command != NULL)
	{
		GString *run = g_string_new (command);
		g_string_append (run, "; exec");
		for (guint i = 0; i < args->len; i++)
		{
			g_autofree char *quoted = g_shell_quote (g_ptr_array_index (args, i));
			g_string_append_c (run, ' ');
			g_string_append (run, quoted);
		}
		g_ptr_array_add (args, g_strdup ("-c"));
		g_ptr_array_add (args, g_string_free (run, FALSE));
	}
	g_ptr_array_add (args, NULL);
	return (char **) g_ptr_array_free (args, FALSE);
}

char *
termac_session_directory (const char *cwd)
{
	if (cwd != NULL && g_path_is_absolute (cwd) && g_file_test (cwd, G_FILE_TEST_IS_DIR))
		return g_strdup (cwd);
	return NULL;
}

char **
termac_shell_argv (const char *shell, const char *rcfile)
{
	return termac_shell_command_argv (shell, rcfile, NULL);
}

gboolean
termac_should_notify (gboolean setting_enabled, gboolean window_active,
                      gboolean tab_selected, gboolean took_long_enough)
{
	return setting_enabled && (!window_active || !tab_selected) && took_long_enough;
}
