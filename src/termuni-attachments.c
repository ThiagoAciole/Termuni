/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termuni-attachments.h"

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "termuni-util.h"

static char *
attachments_root (void)
{
	return g_build_filename (g_get_user_cache_dir (), "termuni", "attachments", NULL);
}

char *
termuni_attachments_session_dir (const char *session_id)
{
	g_return_val_if_fail (session_id != NULL && *session_id != '\0', NULL);
	return g_build_filename (g_get_user_cache_dir (), "termuni", "attachments", session_id, NULL);
}

static gboolean
is_safe_session_id (const char *session_id)
{
	const char *p;

	if (session_id == NULL || *session_id == '\0')
		return FALSE;
	for (p = session_id; *p != '\0'; p++)
		if (!g_ascii_isalnum (*p) && *p != '-')
			return FALSE;
	return TRUE;
}

char *
termuni_attachments_store (const char    *session_id,
                          const guint8  *data,
                          gsize          length,
                          GError       **error)
{
	TermuniImageFormat format;
	guint width = 0;
	guint height = 0;
	g_autofree char *dir = NULL;
	g_autofree char *name = NULL;
	g_autofree char *path = NULL;
	const char *extension;

	g_return_val_if_fail (data != NULL || length == 0, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!is_safe_session_id (session_id))
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		             _("Invalid attachment session"));
		return NULL;
	}
	if (length == 0 || length > TERMUNI_ATTACHMENT_MAX_BYTES)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		             _("Images must be up to 20 MiB"));
		return NULL;
	}
	format = termuni_image_probe (data, length, &width, &height);
	if (format == TERMUNI_IMAGE_FORMAT_NONE)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		             _("Only PNG and JPEG images are accepted"));
		return NULL;
	}
	if (width == 0 || height == 0 ||
	    width > TERMUNI_ATTACHMENT_MAX_DIMENSION || height > TERMUNI_ATTACHMENT_MAX_DIMENSION)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		             _("The image is larger than %d px in width or height"),
		             TERMUNI_ATTACHMENT_MAX_DIMENSION);
		return NULL;
	}

	extension = format == TERMUNI_IMAGE_FORMAT_PNG ? "png" : "jpg";
	dir = termuni_attachments_session_dir (session_id);
	if (g_mkdir_with_parents (dir, 0700) != 0)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		             _("Could not create the attachments folder"));
		return NULL;
	}
	name = g_strdup_printf ("%s.%s", g_uuid_string_random (), extension);
	path = g_build_filename (dir, name, NULL);
	/* g_file_set_contents writes a temporary file and renames it into place. */
	if (!g_file_set_contents (path, (const char *) data, (gssize) length, error))
		return NULL;
	return g_steal_pointer (&path);
}

char *
termuni_attachments_import (const char *session_id,
                           const char *source_path,
                           GError    **error)
{
	g_autofree char *contents = NULL;
	gsize length = 0;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFileInfo) info = NULL;

	g_return_val_if_fail (source_path != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	file = g_file_new_for_path (source_path);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
	                          G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL)
		return NULL;
	if ((gint64) g_file_info_get_size (info) > TERMUNI_ATTACHMENT_MAX_BYTES)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		             _("Images must be up to 20 MiB"));
		return NULL;
	}
	if (!g_file_get_contents (source_path, &contents, &length, error))
		return NULL;
	return termuni_attachments_store (session_id, (const guint8 *) contents, length, error);
}

void
termuni_attachments_clear_session (const char *session_id)
{
	g_autofree char *dir = NULL;
	g_autoptr(GFile) folder = NULL;

	if (!is_safe_session_id (session_id))
		return;
	dir = termuni_attachments_session_dir (session_id);
	folder = g_file_new_for_path (dir);
	if (g_file_query_exists (folder, NULL))
	{
		g_autoptr(GError) error = NULL;
		g_autoptr(GFileEnumerator) files = g_file_enumerate_children (folder,
		    G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, &error);
		if (files != NULL)
		{
			GFileInfo *info;
			while ((info = g_file_enumerator_next_file (files, NULL, NULL)) != NULL)
			{
				g_autoptr(GFile) child = g_file_get_child (folder, g_file_info_get_name (info));
				g_file_delete (child, NULL, NULL);
				g_object_unref (info);
			}
		}
		g_file_delete (folder, NULL, NULL);
	}
}

static void
prune_directory (GFile *directory,
                 gint64 cutoff)
{
	g_autoptr(GFileEnumerator) files = g_file_enumerate_children (directory,
	    G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
	    G_FILE_QUERY_INFO_NONE, NULL, NULL);
	GFileInfo *info;
	GDateTime *modified;

	if (files == NULL)
		return;
	while ((info = g_file_enumerator_next_file (files, NULL, NULL)) != NULL)
	{
		g_autoptr(GFileInfo) owned = info;
		g_autoptr(GFile) child = g_file_get_child (directory, g_file_info_get_name (info));

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
		{
			prune_directory (child, cutoff);
			g_file_delete (child, NULL, NULL);
			continue;
		}
		modified = g_file_info_get_modification_date_time (info);
		if (modified != NULL)
		{
			if (g_date_time_to_unix (modified) < cutoff)
				g_file_delete (child, NULL, NULL);
			g_date_time_unref (modified);
		}
	}
}

void
termuni_attachments_prune (gint64 max_age_seconds)
{
	g_autofree char *root = attachments_root ();
	g_autoptr(GFile) directory = g_file_new_for_path (root);
	gint64 cutoff = g_get_real_time () / G_USEC_PER_SEC - max_age_seconds;

	if (g_file_query_exists (directory, NULL))
		prune_directory (directory, cutoff);
}

char *
termuni_attachments_quote (const char *path)
{
	g_return_val_if_fail (path != NULL, NULL);
	return g_shell_quote (path);
}
