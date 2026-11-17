/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gio/gio.h>
#include "termuni-settings.h"

static void
check_schema (const char *mode)
{
	if (g_test_subprocess ())
	{
		g_autoptr(GSettings) settings = NULL;
		g_autofree char *dir = g_test_build_filename (G_TEST_BUILT, "..", "data", NULL);
		gboolean installed = g_str_equal (mode, "installed");
		gboolean missing = g_str_equal (mode, "missing");
		/* Each subprocess initializes GLib's cached default source independently. */
		if (installed)
			g_setenv ("GSETTINGS_SCHEMA_DIR", dir, TRUE);
		else
		{
			g_unsetenv ("GSETTINGS_SCHEMA_DIR");
			g_setenv ("XDG_DATA_DIRS", "/nonexistent", TRUE);
		}
		if (missing)
			g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "*Could not load settings schema*");
		settings = termuni_settings_new (APPLICATION_ID, installed || missing ? "/nonexistent" : dir);
		if (missing)
		{
			g_assert_null (settings);
			g_test_assert_expected_messages ();
		}
		else
		{
			g_assert_nonnull (settings);
			g_assert_cmpuint (g_settings_get_uint (settings, "font-size"), ==, 11);
			g_assert_false (g_settings_get_boolean (settings, "notifications"));
		}
		return;
	}
	g_test_trap_subprocess (NULL, 0, 0);
	g_test_trap_assert_passed ();
}
static void installed (void) { check_schema ("installed"); }
static void fallback (void) { check_schema ("fallback"); }
static void missing (void) { check_schema ("missing"); }
int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/settings/installed-schema", installed);
	g_test_add_func ("/settings/build-dir-fallback", fallback);
	g_test_add_func ("/settings/missing", missing);
	return g_test_run ();
}
