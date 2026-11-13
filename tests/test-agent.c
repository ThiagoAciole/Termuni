/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <glib/gstdio.h>
#include "termac-agent.h"

static void
test_find (void)
{
	gsize count = 0;
	const TermacAgent *const *agents = termac_agent_all (&count);
	g_assert_cmpuint (count, ==, 2);
	g_assert_true (termac_agent_find ("claude") == agents[0]);
	g_assert_true (termac_agent_find ("codex") == agents[1]);
	g_assert_null (termac_agent_find (NULL));
	g_assert_null (termac_agent_find ("nope"));
	g_assert_cmpstr (termac_agent_default ()->id, ==, "claude");
}

static void
test_format (void)
{
	const char *paths[] = { "/tmp/a.png", "/tmp/it's here.png", "/tmp/$HOME `x`.png", "/tmp/ação.png" };
	gsize count;
	const TermacAgent *const *agents = termac_agent_all (&count);
	for (guint a = 0; a < count + 1; a++)
		for (guint i = 0; i < G_N_ELEMENTS (paths); i++)
		{
			g_autofree char *formatted = termac_agent_format_attachment (a < count ? agents[a] : NULL, paths[i]);
			g_autofree char *command = g_strconcat ("printf %s ", formatted, NULL);
			char *argv[] = { "/bin/sh", "-c", command, NULL };
			g_autofree char *output = NULL;
			int status;
			g_assert_true (g_str_has_suffix (formatted, " "));
			g_assert_true (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, &output, NULL, &status, NULL));
			g_assert_true (g_spawn_check_wait_status (status, NULL));
			g_assert_cmpstr (output, ==, paths[i]);
		}
}

static void
test_installed (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termac-agent-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (dir, "claude", NULL);
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_assert_true (g_file_set_contents (path, "#!/bin/sh\nexit 0\n", -1, NULL));
	g_assert_cmpint (g_chmod (path, 0755), ==, 0);
	g_setenv ("PATH", dir, TRUE);
	g_assert_true (termac_agent_is_installed (termac_agent_default ()));
	g_assert_false (termac_agent_is_installed (termac_agent_find ("codex")));
	g_unlink (path);
	g_assert_false (termac_agent_is_installed (termac_agent_default ()));
	if (previous != NULL) g_setenv ("PATH", previous, TRUE); else g_unsetenv ("PATH");
	g_rmdir (dir);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/agent/installed", test_installed);
	g_test_add_func ("/agent/find", test_find);
	g_test_add_func ("/agent/format-hostile", test_format);
	return g_test_run ();
}
