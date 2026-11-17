/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termuni-session-store.h"
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <unistd.h>

static GMutex gate;
static GCond changed;
static gboolean block_write, entered, released;
static guint writes;
static gboolean fail_write;

gboolean __real_g_file_set_contents_full (const gchar *, const gchar *, gssize,
                                          GFileSetContentsFlags, int, GError **);
gboolean
__wrap_g_file_set_contents_full (const gchar *path, const gchar *data, gssize length,
                                 GFileSetContentsFlags flags, int mode, GError **error)
{
	g_assert_cmpint (flags, ==, G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE);
	g_assert_cmpint (mode, ==, 0600);
	if (fail_write)
	{
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOSPC, "injected full disk");
		return FALSE;
	}
	g_mutex_lock (&gate);
	writes++;
	if (block_write && !entered)
	{
		entered = TRUE;
		g_cond_broadcast (&changed);
		while (!released)
			g_cond_wait (&changed, &gate);
	}
	g_mutex_unlock (&gate);
	return __real_g_file_set_contents_full (path, data, length, flags, mode, error);
}

static void
assert_contents (const char *path, const char *expected)
{
	g_autofree char *contents = NULL;
	g_assert_true (g_file_get_contents (path, &contents, NULL, NULL));
	g_assert_cmpstr (contents, ==, expected);
}

static void
test_create_replace_failure (void)
{
	g_autofree char *root = g_dir_make_tmp ("termuni-store-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (root, "private", "session", NULL);
	g_autofree char *dir = g_path_get_dirname (path);
	g_autoptr(GBytes) first = g_bytes_new_static ("first", 5);
	g_autoptr(GBytes) last = g_bytes_new_static ("last", 4);
	struct stat st;
	g_assert_true (termuni_session_store_write (path, first));
	assert_contents (path, "first");
	g_assert_cmpint (g_stat (path, &st), ==, 0);
	g_assert_cmpint (st.st_mode & 0777, ==, 0600);
	g_assert_cmpint (g_stat (dir, &st), ==, 0);
	g_assert_cmpint (st.st_mode & 0777, ==, 0700);
	/* Upgrade files created by the old default-mode writer as well. */
	g_assert_cmpint (g_chmod (path, 0664), ==, 0);
	g_assert_true (termuni_session_store_write (path, last));
	assert_contents (path, "last");
	g_assert_cmpint (g_stat (path, &st), ==, 0);
	g_assert_cmpint (st.st_mode & 0777, ==, 0600);
	fail_write = TRUE;
	g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "*Could not save the session*full disk*");
	g_assert_false (termuni_session_store_write (path, first));
	g_test_assert_expected_messages ();
	fail_write = FALSE;
	assert_contents (path, "last");
	/* Deterministic filesystem error, including when tests run as root. */
	g_assert_cmpint (g_unlink (path), ==, 0);
	g_assert_cmpint (g_mkdir (path, 0700), ==, 0);
	g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "*Could not save the session*");
	g_assert_false (termuni_session_store_write (path, first));
	g_test_assert_expected_messages ();
	g_assert_true (g_file_test (path, G_FILE_TEST_IS_DIR));
	g_rmdir (path);
	/* A new request retries successfully after the cause is fixed. */
	g_assert_true (termuni_session_store_write (path, last));
	assert_contents (path, "last");
	/* Refuse symlink destinations without chmod or writes to their targets. */
	{
		g_autofree char *target = g_build_filename (root, "unrelated", NULL);
		g_assert_true (g_file_set_contents (target, "unrelated", -1, NULL));
		g_assert_cmpint (g_chmod (target, 0644), ==, 0);
		g_unlink (path);
		g_assert_cmpint (symlink (target, path), ==, 0);
		g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "*Could not save the session*symbolic link*");
		g_assert_false (termuni_session_store_write (path, last));
		g_test_assert_expected_messages ();
		assert_contents (target, "unrelated");
		g_assert_cmpint (g_stat (target, &st), ==, 0);
		g_assert_cmpint (st.st_mode & 0777, ==, 0644);
		g_assert_true (g_file_test (path, G_FILE_TEST_IS_SYMLINK));
		g_unlink (target);
	}
	g_unlink (path); g_rmdir (dir); g_rmdir (root);
}

static void
test_order_coalesce_drain (void)
{
	g_autofree char *root = g_dir_make_tmp ("termuni-store-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (root, "session", NULL);
	g_autoptr(GBytes) first = g_bytes_new_static ("first", 5);
	g_autoptr(GBytes) middle = g_bytes_new_static ("middle", 6);
	g_autoptr(GBytes) newest = g_bytes_new_static ("newest", 6);
	TermuniSessionStore *store = termuni_session_store_new (path);
	g_mutex_lock (&gate);
	block_write = TRUE; entered = released = FALSE; writes = 0;
	g_mutex_unlock (&gate);
	termuni_session_store_save (store, first, FALSE);
	g_mutex_lock (&gate);
	while (!entered) g_cond_wait (&changed, &gate);
	g_mutex_unlock (&gate);
	/* The disk thread is stuck, but GUI-side queueing must return immediately. */
	termuni_session_store_save (store, middle, FALSE);
	termuni_session_store_save (store, newest, FALSE);
	g_mutex_lock (&gate);
	released = TRUE;
	g_cond_broadcast (&changed);
	g_mutex_unlock (&gate);
	/* Destruction drains pending work and joins; newest must win over first. */
	termuni_session_store_free (store);
	assert_contents (path, "newest");
	g_assert_cmpuint (writes, ==, 2);
	block_write = FALSE;
	store = termuni_session_store_new (path);
	termuni_session_store_save (store, first, TRUE);
	assert_contents (path, "first");
	termuni_session_store_free (store);
	g_unlink (path); g_rmdir (root);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/session-store/create-replace-failure", test_create_replace_failure);
	g_test_add_func ("/session-store/order-coalesce-drain", test_order_coalesce_drain);
	return g_test_run ();
}
