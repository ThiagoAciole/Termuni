/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/wait.h>

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

/* The span also reports the first and last columns, which the link underline uses. */
static void
test_url_span_reports_columns (void)
{
	const char *line = "see https://example.com/a, ok";
	guint first = 0, last = 0;

	g_assert_true (termac_url_span_at_column (line, 10, NULL, NULL, &first, &last));
	g_assert_cmpuint (first, ==, 4);
	g_assert_cmpuint (last, ==, 4 + strlen ("https://example.com/a"));
	g_assert_false (termac_url_span_at_column (line, 0, NULL, NULL, &first, &last));
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

static void
test_session_directory (void)
{
	g_autofree char *host = termac_session_directory ("/inaccessible-host-folder", TRUE);
	g_autofree char *native = termac_session_directory ("/inaccessible-host-folder", FALSE);
	g_assert_cmpstr (host, ==, "/inaccessible-host-folder");
	g_assert_null (native);
}

static void
test_shell_argv (void)
{
	g_auto(GStrv) host = termac_shell_argv ("/usr/bin/bash", "/tmp/a b; echo injected", "/tmp/rc file", TRUE);
	const char *expected[] = { "flatpak-spawn", "--host", "--watch-bus", "--directory=/", "--env=TERMAC_HOST_CWD=/tmp/a b; echo injected", "--env=TERM=xterm-256color", "--env=TERMAC_HOST_RCFILE=/tmp/rc file", "script", "-qefc", "cd -- \"$TERMAC_HOST_CWD\" 2>/dev/null || cd \"$HOME\" || exit; shell=${SHELL:-/bin/sh}; if [ \"${shell##*/}\" = bash ] && [ -n \"$TERMAC_HOST_RCFILE\" ]; then exec \"$shell\" --rcfile \"$TERMAC_HOST_RCFILE\" -i; else exec \"$shell\" -i; fi", "/dev/null", NULL };
	g_assert_true (g_strv_equal ((const char * const *) host, expected));
	g_auto(GStrv) local = termac_shell_argv ("/bin/sh", "/tmp", NULL, FALSE);
	const char *native[] = { "/bin/sh", "-i", NULL };
	g_assert_true (g_strv_equal ((const char * const *) local, native));
}

static void
test_initial_command_after_rc (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termac-initial-command-XXXXXX", NULL);
	g_autofree char *rc = g_build_filename (dir, "rc with ' quote", NULL);
	g_auto(GStrv) args = NULL;
	g_autofree char *output = NULL;
	int status;
	g_assert_true (g_file_set_contents (rc, "TERMAC_TEST_INITIAL=rc-loaded; export TERMAC_TEST_INITIAL\n", -1, NULL));
	args = termac_shell_command_argv ("/bin/bash", dir, rc, FALSE, "printf %s \"$TERMAC_TEST_INITIAL\"; exit 23");
	g_assert_true (g_spawn_sync (dir, args, NULL, G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &output, NULL, &status, NULL));
	g_assert_cmpstr (output, ==, "rc-loaded");
	g_assert_cmpint (WEXITSTATUS (status), ==, 23);
	g_unlink (rc); g_rmdir (dir);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/util/initial-command-after-rc", test_initial_command_after_rc);
	g_test_add_func ("/util/session-directory", test_session_directory);
	g_test_add_func ("/util/shell-argv", test_shell_argv);
	g_test_add_func ("/util/pwd", test_path_from_pwd_strips_file_uri);
	g_test_add_func ("/util/probe/png", test_probe_png_reads_dimensions);
	g_test_add_func ("/util/probe/jpeg", test_probe_jpeg_reads_dimensions);
	g_test_add_func ("/util/probe/reject", test_probe_rejects_other_data);
	g_test_add_func ("/util/probe/truncated", test_probe_rejects_truncated_png);
	g_test_add_func ("/util/url", test_url_is_found_at_column);
	g_test_add_func ("/util/url-span", test_url_span_reports_columns);
	g_test_add_func ("/util/display-path", test_display_path_abbreviates_home);
	return g_test_run ();
}
