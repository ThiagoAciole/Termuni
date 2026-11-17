/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termuni-settings.h"

GSettings *
termuni_settings_new (const char *application_id, const char *fallback_schema_dir)
{
	GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
	g_autoptr(GSettingsSchema) schema = source != NULL
		? g_settings_schema_source_lookup (source, application_id, TRUE) : NULL;

	if (schema == NULL && fallback_schema_dir != NULL &&
	    g_file_test (fallback_schema_dir, G_FILE_TEST_IS_DIR))
	{
		g_autoptr(GSettingsSchemaSource) fallback =
			g_settings_schema_source_new_from_directory (fallback_schema_dir, source, FALSE, NULL);
		if (fallback != NULL)
			schema = g_settings_schema_source_lookup (fallback, application_id, TRUE);
	}
	if (schema == NULL)
	{
		g_warning ("Could not load settings schema %s", application_id);
		return NULL;
	}
	return g_settings_new_full (schema, NULL, NULL);
}
