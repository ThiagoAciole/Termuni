/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <gio/gio.h>

#include "termuni-attachments.h"
#include "termuni-util.h"

static const guint8 png_1x1[] = {
	0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
	0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0,
};

static void
test_store_writes_valid_png (void)
{
	g_autoptr(GError) error = NULL;
	g_autofree char *path = termuni_attachments_store ("sessao-1", png_1x1, sizeof png_1x1, &error);
	g_autofree char *contents = NULL;
	gsize length = 0;

	g_assert_no_error (error);
	g_assert_nonnull (path);
	g_assert_true (g_str_has_suffix (path, ".png"));
	g_assert_true (g_file_get_contents (path, &contents, &length, NULL));
	g_assert_cmpmem (contents, length, png_1x1, sizeof png_1x1);
	termuni_attachments_clear_session ("sessao-1");
	g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
}

static void
test_store_rejects_text_and_unsafe_session (void)
{
	g_autoptr(GError) error = NULL;
	g_autofree char *path = termuni_attachments_store ("sessao-2", (const guint8 *) "texto", 5, &error);

	g_assert_null (path);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
	g_clear_error (&error);

	path = termuni_attachments_store ("../fora", png_1x1, sizeof png_1x1, &error);
	g_assert_null (path);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

static void
test_quote_keeps_spaces_as_one_argument (void)
{
	g_autofree char *quoted = termuni_attachments_quote ("/tmp/minha imagem's.png");

	g_assert_cmpstr (quoted, ==, "'/tmp/minha imagem'\\''s.png'");
}

static void
test_prune_keeps_recent_files (void)
{
	g_autoptr(GError) error = NULL;
	g_autofree char *path = termuni_attachments_store ("sessao-3", png_1x1, sizeof png_1x1, &error);

	g_assert_no_error (error);
	termuni_attachments_prune (3600);
	g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));
	termuni_attachments_clear_session ("sessao-3");
}

int
main (int argc, char **argv)
{
	g_autofree char *cache = g_dir_make_tmp ("termuni-attachments-XXXXXX", NULL);

	g_assert_nonnull (cache);
	g_setenv ("XDG_CACHE_HOME", cache, TRUE);
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/attachments/store/png", test_store_writes_valid_png);
	g_test_add_func ("/attachments/store/reject", test_store_rejects_text_and_unsafe_session);
	g_test_add_func ("/attachments/quote", test_quote_keeps_spaces_as_one_argument);
	g_test_add_func ("/attachments/prune", test_prune_keeps_recent_files);
	return g_test_run ();
}
