/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termuni-session-store.h"
#include <glib/gstdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct _TermuniSessionStore
{
	GMutex mutex;
	GCond changed;
	GThread *thread;
	char *path;
	GBytes *pending;
	guint64 requested;
	guint64 completed;
	gboolean stopping;
};

gboolean
termuni_session_store_write (const char *path, GBytes *bytes)
{
	g_autofree char *dir = g_path_get_dirname (path);
	g_autoptr(GError) error = NULL;
	gsize size;
	const char *data = g_bytes_get_data (bytes, &size);
	GStatBuf existing;
	/* GLib preserves permissions when replacing a regular file. Tighten an
	 * older session before replacement so snapshots remain private too. */
	if (g_lstat (path, &existing) == 0 && S_ISLNK (existing.st_mode))
	{
		g_warning ("Could not save the session: refusing a symbolic link");
		return FALSE;
	}
	if (g_lstat (path, &existing) == 0 && S_ISREG (existing.st_mode))
	{
		/* Never chmod through a symlink, including a concurrent replacement. */
		int fd = g_open (path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC, 0);
		GStatBuf opened;
		if (fd < 0 || fstat (fd, &opened) != 0 ||
		    !S_ISREG (opened.st_mode) || fchmod (fd, 0600) != 0)
		{
			int saved_errno = errno;
			if (fd >= 0) close (fd);
			g_warning ("Could not save the session: %s", g_strerror (saved_errno));
			return FALSE;
		}
		close (fd);
	}

	if (g_mkdir_with_parents (dir, 0700) != 0 ||
	    !g_file_set_contents_full (path, data != NULL ? data : "", size,
	                              G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE,
	                              0600, &error))
	{
		g_warning ("Could not save the session: %s", error ? error->message : "state directory unavailable");
		return FALSE;
	}
	return TRUE;
}

static gpointer
session_writer (gpointer data)
{
	TermuniSessionStore *store = data;

	g_mutex_lock (&store->mutex);
	for (;;)
	{
		GBytes *bytes;
		guint64 sequence;
		while (store->pending == NULL && !store->stopping)
			g_cond_wait (&store->changed, &store->mutex);
		if (store->pending == NULL)
			break;
		bytes = g_steal_pointer (&store->pending);
		sequence = store->requested;
		g_mutex_unlock (&store->mutex);
		termuni_session_store_write (store->path, bytes);
		g_bytes_unref (bytes);
		g_mutex_lock (&store->mutex);
		store->completed = sequence;
		g_cond_broadcast (&store->changed);
	}
	g_mutex_unlock (&store->mutex);
	return NULL;
}

TermuniSessionStore *
termuni_session_store_new (const char *path)
{
	TermuniSessionStore *store = g_new0 (TermuniSessionStore, 1);
	g_mutex_init (&store->mutex);
	g_cond_init (&store->changed);
	store->path = g_strdup (path);
	store->thread = g_thread_new ("session-writer", session_writer, store);
	return store;
}

void
termuni_session_store_save (TermuniSessionStore *store, GBytes *bytes, gboolean flush)
{
	guint64 sequence;
	g_mutex_lock (&store->mutex);
	g_clear_pointer (&store->pending, g_bytes_unref);
	store->pending = g_bytes_ref (bytes);
	sequence = ++store->requested;
	g_cond_broadcast (&store->changed);
	while (flush && store->completed < sequence)
		g_cond_wait (&store->changed, &store->mutex);
	g_mutex_unlock (&store->mutex);
}

void
termuni_session_store_free (TermuniSessionStore *store)
{
	if (store == NULL)
		return;
	g_mutex_lock (&store->mutex);
	store->stopping = TRUE;
	g_cond_broadcast (&store->changed);
	g_mutex_unlock (&store->mutex);
	g_thread_join (store->thread);
	g_free (store->path);
	g_cond_clear (&store->changed);
	g_mutex_clear (&store->mutex);
	g_free (store);
}
