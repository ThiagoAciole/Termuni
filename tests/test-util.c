/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>

#include "termac-util.h"

static GByteArray *
png_header (guint width, guint height)
{
	static const guint8 signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	GByteArray *bytes = g_byte_array_new ();
	guint8 ihdr[] = {
		0, 0, 0, 13, 'I', 'H', 'D', 'R',
		(width >> 24) & 0xff, (width >> 16) & 0xff, (width >> 8) & 0xff, width & 0xff,
		(height >> 24) & 0xff, (height >> 16) & 0xff, (height >> 8) & 0xff, height & 0xff,
		8, 6, 0, 0, 0,
	};

	g_byte_array_append (bytes, signature, sizeof signature);
	g_byte_array_append (bytes, ihdr, sizeof ihdr);
	return bytes;
}

static void
test_probe_png_reads_dimensions (void)
{
	GByteArray *bytes = png_header (640, 480);
	guint width = 0, height = 0;

	g_assert_cmpint (termac_image_probe (bytes->data, bytes->len, &width, &height), ==,
	                 TERMAC_IMAGE_FORMAT_PNG);
	g_assert_cmpuint (width, ==, 640);
	g_assert_cmpuint (height, ==, 480);
	g_byte_array_unref (bytes);
}

static void
test_probe_jpeg_reads_dimensions (void)
{
	/* SOI, APP0 (JFIF), then SOF0 with 480 rows and 640 columns. */
	static const guint8 jpeg[] = {
		0xff, 0xd8,
		0xff, 0xe0, 0x00, 0x04, 0x4a, 0x46,
		0xff, 0xc0, 0x00, 0x0b, 0x08, 0x01, 0xe0, 0x02, 0x80, 0x01, 0x01, 0x11, 0x00,
	};
	guint width = 0, height = 0;

	g_assert_cmpint (termac_image_probe (jpeg, sizeof jpeg, &width, &height), ==,
	                 TERMAC_IMAGE_FORMAT_JPEG);
	g_assert_cmpuint (width, ==, 640);
	g_assert_cmpuint (height, ==, 480);
}

static void
test_probe_rejects_other_data (void)
{
	static const guint8 text[] = "not an image at all, just text";
	guint width = 0, height = 0;

	g_assert_cmpint (termac_image_probe (text, sizeof text, &width, &height), ==,
	                 TERMAC_IMAGE_FORMAT_NONE);
	g_assert_cmpint (termac_image_probe (text, 4, &width, &height), ==,
	                 TERMAC_IMAGE_FORMAT_NONE);
}

static void
test_probe_rejects_truncated_png (void)
{
	GByteArray *bytes = png_header (1, 1);
	guint width = 0, height = 0;

	g_assert_cmpint (termac_image_probe (bytes->data, 20, &width, &height), ==,
	                 TERMAC_IMAGE_FORMAT_NONE);
	g_byte_array_unref (bytes);
}

static void
test_url_is_found_at_column (void)
{
	const char *line = "see https://example.com/a, ok";
	gsize start = 0, end = 0;

	g_assert_true (termac_url_at_column (line, 10, &start, &end));
	g_assert_cmpuint (start, ==, 4);
	g_assert_cmpuint (end - start, ==, strlen ("https://example.com/a"));
	g_assert_false (termac_url_at_column (line, 0, NULL, NULL));
	g_assert_false (termac_url_at_column (line, 25, NULL, NULL));
}

static void
test_display_path_abbreviates_home (void)
{
	g_autofree char *home_path = termac_display_path ("/home/u/projeto", "/home/u");
	g_autofree char *home = termac_display_path ("/home/u", "/home/u");
	g_autofree char *other = termac_display_path ("/home/user2", "/home/u");
	g_autofree char *none = termac_display_path ("/tmp", NULL);

	g_assert_cmpstr (home_path, ==, "~/projeto");
	g_assert_cmpstr (home, ==, "~");
	g_assert_cmpstr (other, ==, "/home/user2");
	g_assert_cmpstr (none, ==, "/tmp");
}

static void
test_path_from_pwd_strips_file_uri (void)
{
	g_autofree char *plain = termac_path_from_pwd ("/home/u/x");
	g_autofree char *uri = termac_path_from_pwd ("file://aciole/home/thiago");
	g_autofree char *spaced = termac_path_from_pwd ("file:///tmp/a%20b");

	g_assert_cmpstr (plain, ==, "/home/u/x");
	g_assert_cmpstr (uri, ==, "/home/thiago");
	g_assert_cmpstr (spaced, ==, "/tmp/a b");
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/util/pwd", test_path_from_pwd_strips_file_uri);
	g_test_add_func ("/util/probe/png", test_probe_png_reads_dimensions);
	g_test_add_func ("/util/probe/jpeg", test_probe_jpeg_reads_dimensions);
	g_test_add_func ("/util/probe/reject", test_probe_rejects_other_data);
	g_test_add_func ("/util/probe/truncated", test_probe_rejects_truncated_png);
	g_test_add_func ("/util/url", test_url_is_found_at_column);
	g_test_add_func ("/util/display-path", test_display_path_abbreviates_home);
	return g_test_run ();
}
