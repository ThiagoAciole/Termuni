/* termac-application.c
 *
 * Copyright 2026 Thiago Aciole
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "termac-application.h"
#include "termac-attachments.h"
#include "termac-window.h"
#include "termac-terminal.h"

struct _TermacApplication
{
	AdwApplication parent_instance;
	GSettings *settings;
	gboolean css_loaded;
	gboolean session_frozen;
	guint session_save_source;
};

G_DEFINE_FINAL_TYPE (TermacApplication, termac_application, ADW_TYPE_APPLICATION)

static void termac_application_finalize (GObject *object);
static void termac_application_apply_settings (TermacApplication *self);
static void style_dark_changed_cb (AdwStyleManager *manager, GParamSpec *pspec, gpointer user_data);

#define TERMAC_SESSION_FILE "session-v3.gvariant"
#define TERMAC_SESSION_TYPE "a(iiiba(ss))"
#define TERMAC_SESSION_FILE_V2 "session-v2.gvariant"
#define TERMAC_SESSION_TYPE_V2 "a(ia(ss))"
#define TERMAC_SESSION_SAVE_DELAY_MS 800
#define TERMAC_ATTACHMENT_RETENTION_SECONDS (7 * 24 * 60 * 60)

static char *
session_file_path (void)
{
	return g_build_filename (g_get_user_state_dir (), "termac", TERMAC_SESSION_FILE, NULL);
}

static char *
session_file_path_v2 (void)
{
	return g_build_filename (g_get_user_state_dir (), "termac", TERMAC_SESSION_FILE_V2, NULL);
}

/* Stores the sessions in the current file. Takes ownership of sessions. */
static gboolean
write_session_file (GVariant *sessions)
{
	g_autofree char *dir = g_build_filename (g_get_user_state_dir (), "termac", NULL);
	g_autofree char *path = session_file_path ();
	g_autofree guint8 *buffer = NULL;
	gsize size;
	gboolean written = FALSE;

	size = g_variant_get_size (sessions);
	buffer = g_malloc (size);
	g_variant_store (sessions, buffer);
	g_variant_unref (sessions);
	if (g_mkdir_with_parents (dir, 0700) != 0)
		return FALSE;
	/* g_file_set_contents writes a temporary file and renames it into place. */
	written = g_file_set_contents (path, (const char *) buffer, (gssize) size, NULL);
	if (!written)
		g_warning ("Could not save the session");
	return written;
}

/* Writes the tabs of every window that still exists. Each window is stored
 * as (selected index, width, height, maximized, tabs); an empty list is saved
 * as an empty session. */
void
termac_application_save_session_now (TermacApplication *self)
{
	GVariantBuilder builder;
	GList *windows;
	GList *iter;
	GVariant *sessions;

	g_return_if_fail (TERMAC_IS_APPLICATION (self));
	if (self->session_save_source != 0)
	{
		g_source_remove (self->session_save_source);
		self->session_save_source = 0;
	}
	g_variant_builder_init (&builder, G_VARIANT_TYPE (TERMAC_SESSION_TYPE));
	windows = gtk_application_get_windows (GTK_APPLICATION (self));
	for (iter = windows; iter != NULL; iter = iter->next)
	{
		gint selected = 0;
		GVariant *tabs;

		if (!TERMAC_IS_WINDOW (iter->data))
			continue;
		tabs = termac_window_session_state (TERMAC_WINDOW (iter->data), &selected);
		if (g_variant_n_children (tabs) == 0)
		{
			g_variant_unref (tabs);
			continue;
		}
		g_variant_builder_add (&builder, "(iiib@a(ss))", selected,
		                       gtk_widget_get_width (GTK_WIDGET (iter->data)),
		                       gtk_widget_get_height (GTK_WIDGET (iter->data)),
		                       gtk_window_is_maximized (GTK_WINDOW (iter->data)),
		                       tabs);
	}
	sessions = g_variant_builder_end (&builder);
	write_session_file (sessions);
}

static gboolean
session_save_timeout (gpointer user_data)
{
	TermacApplication *self = TERMAC_APPLICATION (user_data);

	self->session_save_source = 0;
	termac_application_save_session_now (self);
	return G_SOURCE_REMOVE;
}

/* A window is closing. While the app quits, the session was already saved
 * and closing windows one by one must not overwrite it with partial state. */
void
termac_application_window_closing (TermacApplication *self)
{
	g_return_if_fail (TERMAC_IS_APPLICATION (self));
	if (!self->session_frozen)
		termac_application_save_session_now (self);
}

/* Called after each change in tabs, folders or titles. Saves shortly after
 * the last change, so bursts of changes write the file once. */
void
termac_application_request_session_save (TermacApplication *self)
{
	g_return_if_fail (TERMAC_IS_APPLICATION (self));
	if (self->session_frozen)
		return;
	if (self->session_save_source == 0)
		self->session_save_source = g_timeout_add (TERMAC_SESSION_SAVE_DELAY_MS,
		                                           session_save_timeout, self);
}

/* Converts the v2 layout, (selected, tabs), to v3 with default geometry. */
static GVariant *
session_from_v2 (GVariant *v2)
{
	GVariantBuilder builder;
	gsize count = g_variant_n_children (v2);
	gsize i;

	g_variant_builder_init (&builder, G_VARIANT_TYPE (TERMAC_SESSION_TYPE));
	for (i = 0; i < count; i++)
	{
		gint selected = 0;
		GVariant *tabs = NULL;

		g_variant_get_child (v2, i, "(i@a(ss))", &selected, &tabs);
		g_variant_builder_add (&builder, "(iiib@a(ss))", selected, 0, 0, FALSE, tabs);
	}
	return g_variant_builder_end (&builder);
}

/* Runs once: when there is no v3 file, the v2 tabs are converted and written
 * as v3. Once v3 exists, v2 is never read again. Returns NULL if no v2 is valid. */
static GVariant *
migrate_v2_session (void)
{
	g_autofree char *path = session_file_path_v2 ();
	g_autofree char *contents = NULL;
	gsize length = 0;
	g_autoptr (GBytes) bytes = NULL;
	g_autoptr (GVariant) v2 = NULL;
	GVariant *sessions;

	if (!g_file_get_contents (path, &contents, &length, NULL))
		return NULL;
	bytes = g_bytes_new (contents, length);
	v2 = g_variant_new_from_bytes (G_VARIANT_TYPE (TERMAC_SESSION_TYPE_V2), bytes, FALSE);
	if (v2 == NULL || !g_variant_is_normal_form (v2))
	{
		g_warning ("Ignoring an invalid v2 session");
		return NULL;
	}
	sessions = g_variant_ref_sink (session_from_v2 (v2));
	/* Keep a reference for the caller: a failed write must not loop back here. */
	write_session_file (g_variant_ref (sessions));
	return sessions;
}

/* Loads the saved windows. Returns NULL when there is no valid session. */
static GVariant *
load_sessions (void)
{
	g_autofree char *path = session_file_path ();
	g_autofree char *contents = NULL;
	gsize length = 0;
	GBytes *bytes;
	GVariant *sessions;

	if (!g_file_get_contents (path, &contents, &length, NULL))
		return migrate_v2_session ();
	bytes = g_bytes_new (contents, length);
	sessions = g_variant_new_from_bytes (G_VARIANT_TYPE (TERMAC_SESSION_TYPE), bytes, FALSE);
	g_bytes_unref (bytes);
	if (sessions == NULL || !g_variant_is_normal_form (sessions))
	{
		g_warning ("Ignoring an invalid saved session");
		g_clear_pointer (&sessions, g_variant_unref);
		return NULL;
	}
	return sessions;
}

static TermacWindow *
termac_application_new_window (TermacApplication *self)
{
	static gboolean pruned = FALSE;

	if (!pruned)
	{
		termac_attachments_prune (TERMAC_ATTACHMENT_RETENTION_SECONDS);
		pruned = TRUE;
	}
	return g_object_new (TERMAC_TYPE_WINDOW, "application", self, NULL);
}

/* Reopens every saved window. Returns FALSE when nothing was restored. */
static gboolean
termac_application_restore_windows (TermacApplication *self)
{
	GVariant *sessions;
	gsize count;
	gsize i;
	gboolean restored = FALSE;

	if (self->settings == NULL || !g_settings_get_boolean (self->settings, "restore-session"))
		return FALSE;
	sessions = load_sessions ();
	if (sessions == NULL)
		return FALSE;
	count = g_variant_n_children (sessions);
	for (i = 0; i < count; i++)
	{
		gint selected = 0;
		gint width = 0;
		gint height = 0;
		gboolean maximized = FALSE;
		GVariant *tabs = NULL;
		TermacWindow *window;

		g_variant_get_child (sessions, i, "(iiib@a(ss))", &selected, &width, &height,
		                     &maximized, &tabs);
		if (g_variant_n_children (tabs) > 0)
		{
			window = termac_application_new_window (self);
			if (width > 0 && height > 0)
				gtk_window_set_default_size (GTK_WINDOW (window), width, height);
			if (maximized)
				gtk_window_maximize (GTK_WINDOW (window));
			termac_window_restore_tabs (window, tabs, selected);
			gtk_window_present (GTK_WINDOW (window));
			restored = TRUE;
		}
		g_variant_unref (tabs);
	}
	g_variant_unref (sessions);
	return restored;
}

GSettings *
termac_application_get_settings (TermacApplication *self)
{
	g_return_val_if_fail (TERMAC_IS_APPLICATION (self), NULL);
	return self->settings;
}

TermacApplication *
termac_application_new (const char        *application_id,
                        GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (TERMAC_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     "resource-base-path", "/thiago/aciole/termac",
	                     NULL);
}

static void
termac_application_activate (GApplication *app)
{
	GtkWindow *window;

	g_assert (TERMAC_IS_APPLICATION (app));
	termac_application_apply_settings (TERMAC_APPLICATION (app));

	window = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (window == NULL && !termac_application_restore_windows (TERMAC_APPLICATION (app)))
		window = GTK_WINDOW (termac_application_new_window (TERMAC_APPLICATION (app)));
	if (window == NULL)
		window = GTK_WINDOW (gtk_application_get_windows (GTK_APPLICATION (app))->data);
	if (TERMAC_IS_WINDOW (window))
		termac_window_sync_settings (TERMAC_WINDOW (window));
	if (!TERMAC_APPLICATION (app)->css_loaded)
	{
		GtkCssProvider *provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_resource (provider,
		                                    "/thiago/aciole/termac/termac.css");
		gtk_style_context_add_provider_for_display (gdk_display_get_default (),
		                                            GTK_STYLE_PROVIDER (provider),
		                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref (provider);
		TERMAC_APPLICATION (app)->css_loaded = TRUE;
		/* The style manager exists only after GTK is initialized. */
		g_signal_connect (adw_style_manager_get_default (), "notify::dark",
		                  G_CALLBACK (style_dark_changed_cb), app);
	}

	gtk_window_present (window);
}

static void
termac_application_class_init (TermacApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	app_class->activate = termac_application_activate;
	object_class->finalize = termac_application_finalize;
}

static void
termac_application_about_action (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
	static const char *developers[] = {"Thiago Aciole", NULL};
	TermacApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (TERMAC_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "Termac",
	                       "application-icon", APPLICATION_ICON,
	                       "developer-name", "Thiago Aciole",
	                       "translator-credits", _("translator-credits"),
	                       "version", "0.1.0",
	                       "developers", developers,
	                       "copyright", "© 2026 Thiago Aciole",
	                       NULL);
}

static void
termac_application_quit_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	TermacApplication *self = user_data;

	g_assert (TERMAC_IS_APPLICATION (self));

	/* Closing the windows below must not overwrite the saved session. */
	self->session_frozen = TRUE;
	termac_application_save_session_now (self);
	g_application_quit (G_APPLICATION (self));
}

static void
termac_application_show_session_action (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
	TermacApplication *self = user_data;
	const char *session_id = g_variant_get_string (parameter, NULL);
	GList *windows = gtk_application_get_windows (GTK_APPLICATION (self));
	GList *iter;

	(void) action;
	for (iter = windows; iter != NULL; iter = iter->next)
		if (TERMAC_IS_WINDOW (iter->data) &&
		    termac_window_show_session (TERMAC_WINDOW (iter->data), session_id))
			return;
}

static void
scheme_changed_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
	static const char *values[] = { "system", "light", "dark" };
	AdwComboRow *row = ADW_COMBO_ROW (object);
	(void) pspec;
	g_settings_set_string (G_SETTINGS (user_data), "color-scheme",
	                       values[MIN (adw_combo_row_get_selected (row), 2)]);
}

static void
scrollback_changed_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
	(void) pspec;
	g_settings_set_uint (G_SETTINGS (user_data), "scrollback-lines",
	                     (guint) adw_spin_row_get_value (ADW_SPIN_ROW (object)));
}

static void
font_changed_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
	(void) pspec;
	g_settings_set_uint (G_SETTINGS (user_data), "font-size",
	                     (guint) adw_spin_row_get_value (ADW_SPIN_ROW (object)));
}

#define PALETTE_COLLAPSED_COUNT 9

static void
palette_card_set_selected (GtkWidget *card, gboolean selected)
{
	GtkWidget *indicator = g_object_get_data (G_OBJECT (card), "palette-indicator");

	if (selected)
		gtk_widget_add_css_class (card, "selected");
	else
		gtk_widget_remove_css_class (card, "selected");
	gtk_image_set_from_icon_name (GTK_IMAGE (indicator),
	                              selected ? "object-select-symbolic" : "radio-symbolic");
}

static void
palette_refresh (GtkWidget *grid)
{
	GtkEditable *search = g_object_get_data (G_OBJECT (grid), "palette-search");
	GPtrArray *cards = g_object_get_data (G_OBJECT (grid), "palette-cards");
	gboolean expanded = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (grid), "palette-expanded"));
	g_autofree char *query = g_utf8_casefold (gtk_editable_get_text (search), -1);
	guint i, shown = 0;

	for (i = 0; i < cards->len; i++)
	{
		GtkWidget *card = g_ptr_array_index (cards, i);
		if (gtk_widget_get_parent (card) == grid)
			gtk_grid_remove (GTK_GRID (grid), card);
	}
	for (i = 0; i < cards->len; i++)
	{
		GtkWidget *card = g_ptr_array_index (cards, i);
		const char *title = g_object_get_data (G_OBJECT (card), "palette-title");
		g_autofree char *folded = g_utf8_casefold (title, -1);

		if (strstr (folded, query) == NULL)
			continue;
		if (!expanded && shown >= PALETTE_COLLAPSED_COUNT)
			break;
		gtk_grid_attach (GTK_GRID (grid), card, (int) (shown % 3), (int) (shown / 3), 1, 1);
		shown++;
	}
}

static void
palette_search_changed_cb (GtkEditable *editable, gpointer user_data)
{
	(void) editable;
	palette_refresh (GTK_WIDGET (user_data));
}

static void
palette_toggle_clicked_cb (GtkButton *button, gpointer user_data)
{
	GtkWidget *grid = user_data;
	gboolean expanded = !GPOINTER_TO_INT (g_object_get_data (G_OBJECT (grid), "palette-expanded"));

	g_object_set_data (G_OBJECT (grid), "palette-expanded", GINT_TO_POINTER (expanded));
	gtk_button_set_label (button, expanded ? _("Show fewer palettes") : _("Show all palettes"));
	palette_refresh (grid);
}

static void
palette_card_clicked_cb (GtkButton *button, gpointer user_data)
{
	GSettings *settings = user_data;
	GtkWidget *grid = g_object_get_data (G_OBJECT (button), "palette-grid");
	GPtrArray *cards = g_object_get_data (G_OBJECT (grid), "palette-cards");
	const char *name = g_object_get_data (G_OBJECT (button), "palette-name");
	guint i;

	g_settings_set_string (settings, "palette", name);
	for (i = 0; i < cards->len; i++)
	{
		GtkWidget *card = g_ptr_array_index (cards, i);
		palette_card_set_selected (card, card == GTK_WIDGET (button));
	}
}

typedef struct
{
	guint32 rgb;
} PaletteSwatch;

static void
palette_swatch_draw (GtkDrawingArea *area,
	                 cairo_t        *cr,
	                 int             width,
	                 int             height,
	                 gpointer        user_data)
{
	PaletteSwatch *swatch = user_data;
	guint32 rgb = swatch->rgb;
	(void) area;
	cairo_set_source_rgb (cr, ((rgb >> 16) & 0xff) / 255.0,
	                      ((rgb >> 8) & 0xff) / 255.0,
	                      (rgb & 0xff) / 255.0);
	cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);
	cairo_fill (cr);
}

static void
palette_swatch_free (gpointer data)
{
	g_free (data);
}

/* ---- attachments page ------------------------------------------------- */

/* Every image stored in the attachment cache, across all sessions. */
static GPtrArray *
attachments_collect (void)
{
	g_autofree char *root = g_build_filename (g_get_user_cache_dir (), "termac", "attachments", NULL);
	GPtrArray *paths = g_ptr_array_new_with_free_func (g_free);
	GDir *sessions = g_dir_open (root, 0, NULL);
	const char *session;

	if (sessions == NULL)
		return paths;
	while ((session = g_dir_read_name (sessions)) != NULL)
	{
		g_autofree char *session_dir = g_build_filename (root, session, NULL);
		GDir *files = g_dir_open (session_dir, 0, NULL);
		const char *name;

		if (files == NULL)
			continue;
		while ((name = g_dir_read_name (files)) != NULL)
			g_ptr_array_add (paths, g_build_filename (session_dir, name, NULL));
		g_dir_close (files);
	}
	g_dir_close (sessions);
	g_ptr_array_sort (paths, (GCompareFunc) g_strcmp0);
	return paths;
}

#define ATTACHMENT_THUMBNAIL_SIZE 64

/* Square, centred 64 px thumbnail: the scaled texture keeps the image from
 * taking its full size in the row. */
static GtkWidget *
attachment_thumbnail_new (const char *path)
{
	GtkWidget *image = gtk_image_new ();
	g_autoptr (GdkPixbuf) source = gdk_pixbuf_new_from_file (path, NULL);

	if (source != NULL)
	{
		int width = gdk_pixbuf_get_width (source);
		int height = gdk_pixbuf_get_height (source);
		int side = MIN (width, height);
		g_autoptr (GdkPixbuf) square = gdk_pixbuf_new_subpixbuf (source,
		                                                         (width - side) / 2,
		                                                         (height - side) / 2,
		                                                         side, side);
		g_autoptr (GdkPixbuf) scaled = gdk_pixbuf_scale_simple (square,
		                                                        ATTACHMENT_THUMBNAIL_SIZE,
		                                                        ATTACHMENT_THUMBNAIL_SIZE,
		                                                        GDK_INTERP_BILINEAR);
		g_autoptr (GBytes) pixels = g_bytes_new (gdk_pixbuf_read_pixels (scaled),
		                                         (gsize) gdk_pixbuf_get_rowstride (scaled) *
		                                         ATTACHMENT_THUMBNAIL_SIZE);
		g_autoptr (GdkTexture) texture = gdk_memory_texture_new (ATTACHMENT_THUMBNAIL_SIZE,
		                                                         ATTACHMENT_THUMBNAIL_SIZE,
		                                                         gdk_pixbuf_get_has_alpha (scaled) ?
		                                                         GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8,
		                                                         pixels,
		                                                         gdk_pixbuf_get_rowstride (scaled));

		gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (texture));
	}
	gtk_image_set_pixel_size (GTK_IMAGE (image), ATTACHMENT_THUMBNAIL_SIZE);
	gtk_widget_add_css_class (image, "attachment-thumb");
	gtk_widget_set_overflow (image, GTK_OVERFLOW_HIDDEN);
	return image;
}

static void attachments_list_populate (GtkListBox *list);

static void
attachment_copy_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
	const char *path = g_object_get_data (G_OBJECT (button), "termac-path");
	(void) user_data;
	if (path != NULL)
		gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), path);
}

static void
attachment_delete_clicked_cb (GtkButton *button,
                              gpointer   user_data)
{
	const char *path = g_object_get_data (G_OBJECT (button), "termac-path");
	GtkListBox *list = user_data;
	if (path != NULL)
		g_unlink (path);
	attachments_list_populate (list);
}

static void
attachments_clear_all_cb (GtkButton *button,
                          gpointer   user_data)
{
	GtkListBox *list = user_data;
	GPtrArray *paths = attachments_collect ();
	guint i;
	(void) button;
	for (i = 0; i < paths->len; i++)
		g_unlink (g_ptr_array_index (paths, i));
	g_ptr_array_unref (paths);
	attachments_list_populate (list);
}

static void
attachments_list_populate (GtkListBox *list)
{
	GPtrArray *paths = attachments_collect ();
	GtkListBoxRow *row;
	guint i;

	while ((row = gtk_list_box_get_row_at_index (list, 0)) != NULL)
		gtk_list_box_remove (list, GTK_WIDGET (row));
	if (paths->len == 0)
	{
		GtkWidget *empty = adw_action_row_new ();
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (empty), _("No attachments"));
		gtk_list_box_append (list, empty);
	}
	for (i = 0; i < paths->len; i++)
	{
		const char *path = g_ptr_array_index (paths, i);
		g_autofree char *base = g_path_get_basename (path);
		GtkWidget *row_widget = adw_action_row_new ();
		GtkWidget *copy = gtk_button_new_from_icon_name ("edit-copy-symbolic");
		GtkWidget *delete_button = gtk_button_new_from_icon_name ("user-trash-symbolic");

		GtkWidget *thumbnail = attachment_thumbnail_new (path);

		gtk_widget_add_css_class (row_widget, "attachment-row");
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row_widget), base);
		adw_action_row_set_subtitle (ADW_ACTION_ROW (row_widget), path);
		adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (row_widget), 1);
		adw_action_row_add_prefix (ADW_ACTION_ROW (row_widget), thumbnail);
		gtk_widget_add_css_class (copy, "flat");
		gtk_widget_add_css_class (delete_button, "flat");
		gtk_widget_set_tooltip_text (copy, _("Copy path"));
		gtk_widget_set_tooltip_text (delete_button, _("Remove"));
		gtk_accessible_update_property (GTK_ACCESSIBLE (copy), GTK_ACCESSIBLE_PROPERTY_LABEL, _("Copy path"), -1);
		gtk_accessible_update_property (GTK_ACCESSIBLE (delete_button), GTK_ACCESSIBLE_PROPERTY_LABEL, _("Remove"), -1);
		g_object_set_data_full (G_OBJECT (copy), "termac-path", g_strdup (path), g_free);
		g_object_set_data_full (G_OBJECT (delete_button), "termac-path", g_strdup (path), g_free);
		g_signal_connect (copy, "clicked", G_CALLBACK (attachment_copy_clicked_cb), NULL);
		g_signal_connect (delete_button, "clicked", G_CALLBACK (attachment_delete_clicked_cb), list);
		adw_action_row_add_suffix (ADW_ACTION_ROW (row_widget), copy);
		adw_action_row_add_suffix (ADW_ACTION_ROW (row_widget), delete_button);
		gtk_list_box_append (list, row_widget);
	}
	g_ptr_array_unref (paths);
}

static GtkWidget *
termac_application_attachments_page (void)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	AdwPreferencesPage *page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
	AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
	GtkWidget *list = gtk_list_box_new ();
	GtkWidget *footer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *clear = gtk_button_new_with_label (_("Clear All"));

	adw_preferences_page_set_title (page, _("Attachments"));
	adw_preferences_page_set_icon_name (page, "mail-attachment-symbolic");
	adw_preferences_group_set_title (group, _("Images"));
	adw_preferences_group_set_description (group, _("Images attached to prompts are kept in the cache. Removing an image does not change prompts that already contain its path."));
	gtk_widget_add_css_class (list, "boxed-list");
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
	adw_preferences_group_add (group, list);
	adw_preferences_page_add (page, group);
	gtk_widget_set_vexpand (GTK_WIDGET (page), TRUE);

	gtk_widget_set_margin_start (footer, 16);
	gtk_widget_set_margin_end (footer, 16);
	gtk_widget_set_margin_top (footer, 8);
	gtk_widget_set_margin_bottom (footer, 16);
	gtk_widget_set_hexpand (clear, TRUE);
	gtk_widget_add_css_class (clear, "destructive-action");
	gtk_box_append (GTK_BOX (footer), clear);
	g_signal_connect (clear, "clicked", G_CALLBACK (attachments_clear_all_cb), list);

	gtk_box_append (GTK_BOX (box), GTK_WIDGET (page));
	gtk_box_append (GTK_BOX (box), footer);
	attachments_list_populate (GTK_LIST_BOX (list));
	return box;
}

static GtkWindow *
application_dialog_parent (TermacApplication *self)
{
	GtkWindow *parent = gtk_application_get_active_window (GTK_APPLICATION (self));
	GList *windows;

	if (parent != NULL)
		return parent;
	windows = gtk_application_get_windows (GTK_APPLICATION (self));
	return windows != NULL ? GTK_WINDOW (windows->data) : NULL;
}

static const struct
{
	const char *action;
	const char *title;
	const char *accel;
} editable_shortcuts[] = {
	{ "win.new-tab", N_("New tab"), "<control><shift>t" },
	{ "win.new-window", N_("New window"), "<control><shift>n" },
	{ "win.toggle-overview", N_("Show open tabs"), "<control><shift>o" },
	{ "win.close-tab", N_("Close tab"), "<control><shift>w" },
	{ "win.fullscreen", N_("Full screen"), "F11" },
	{ "win.rename-tab", N_("Rename tab"), "<control><shift>r" },
	{ "win.find", N_("Find in terminal"), "<control><shift>f" },
	{ "win.terminal-copy", N_("Copy selection or cancel command"), "<control>c" },
	{ "win.terminal-copy-selection", N_("Copy selection"), "<control><shift>c" },
	{ "win.terminal-paste", N_("Paste"), "<control>v" },
	{ "win.terminal-paste-clipboard", N_("Paste from clipboard"), "<control><shift>v" },
	{ "win.terminal-line-start", N_("Start of line"), "<control>Left" },
	{ "win.terminal-select-all", N_("Select all"), "<control>a" },
	{ "win.attach-image", N_("Attach image"), "<control><shift>i" },
	{ "app.preferences", N_("Preferences"), "<control>comma" },
	{ "app.shortcuts", N_("Keyboard shortcuts"), "<control>question" },
};
#define EDITABLE_SHORTCUT_COUNT G_N_ELEMENTS (editable_shortcuts)

static char *
shortcut_override_get (GSettings *settings, const char *action)
{
	g_autoptr (GVariant) overrides = g_settings_get_value (settings, "action-shortcuts");
	const char *accel = NULL;

	if (!g_variant_lookup (overrides, action, "&s", &accel))
		return NULL;
	return g_strdup (accel);
}

static void
shortcut_override_set (GSettings *settings, const char *action, const char *accel)
{
	g_autoptr (GVariant) current = g_settings_get_value (settings, "action-shortcuts");
	GVariantBuilder builder;
	GVariantIter iter;
	const char *key;
	const char *value;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));
	g_variant_iter_init (&iter, current);
	while (g_variant_iter_next (&iter, "{&s&s}", &key, &value))
		if (g_strcmp0 (key, action) != 0)
			g_variant_builder_add (&builder, "{ss}", key, value);
	if (accel != NULL)
		g_variant_builder_add (&builder, "{ss}", action, accel);
	g_settings_set_value (settings, "action-shortcuts", g_variant_builder_end (&builder));
}

/* Registers every editable action with its custom or built-in accelerator. */
static void
termac_application_apply_action_shortcuts (TermacApplication *self)
{
	guint i;

	if (self->settings == NULL)
		return;
	for (i = 0; i < EDITABLE_SHORTCUT_COUNT; i++)
	{
		g_autofree char *override = shortcut_override_get (self->settings, editable_shortcuts[i].action);
		const char *accels[] = { editable_shortcuts[i].accel, NULL };
		guint key = 0;
		GdkModifierType mods = 0;

		if (override != NULL && gtk_accelerator_parse (override, &key, &mods))
			accels[0] = override;
		gtk_application_set_accels_for_action (GTK_APPLICATION (self), editable_shortcuts[i].action, accels);
	}
}

static void
shortcut_button_refresh (GtkButton *button, TermacApplication *self)
{
	const char *action = g_object_get_data (G_OBJECT (button), "shortcut-action");
	const char *fallback = g_object_get_data (G_OBJECT (button), "shortcut-default");
	g_autofree char *override = shortcut_override_get (self->settings, action);
	const char *accel = fallback;
	guint key = 0;
	GdkModifierType mods = 0;
	g_autofree char *label = NULL;

	if (override != NULL && gtk_accelerator_parse (override, &key, &mods))
		accel = override;
	if (gtk_accelerator_parse (accel, &key, &mods))
		label = gtk_accelerator_get_label (key, mods);
	gtk_button_set_label (button, label != NULL ? label : accel);
}

typedef struct
{
	TermacApplication *app;
	GtkButton *button;
	AdwDialog *dialog;
} ShortcutCapture;

static gboolean
shortcut_capture_key_cb (GtkEventControllerKey *controller,
                         guint                  keyval,
                         guint                  keycode,
                         GdkModifierType        state,
                         gpointer               user_data)
{
	ShortcutCapture *capture = user_data;
	GdkModifierType mods = state & gtk_accelerator_get_default_mod_mask ();
	const char *action = g_object_get_data (G_OBJECT (capture->button), "shortcut-action");
	g_autofree char *accel = NULL;

	(void) controller;
	(void) keycode;
	if (keyval == GDK_KEY_Escape)
	{
		adw_dialog_close (capture->dialog);
		return GDK_EVENT_STOP;
	}
	switch (keyval)
	{
	case GDK_KEY_Control_L: case GDK_KEY_Control_R:
	case GDK_KEY_Shift_L: case GDK_KEY_Shift_R:
	case GDK_KEY_Alt_L: case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L: case GDK_KEY_Super_R:
	case GDK_KEY_Meta_L: case GDK_KEY_Meta_R:
	case GDK_KEY_ISO_Level3_Shift:
		return GDK_EVENT_STOP;
	default:
		break;
	}
	if ((mods & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SHIFT_MASK | GDK_SUPER_MASK)) == 0 &&
	    !(keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12))
		return GDK_EVENT_STOP;
	accel = gtk_accelerator_name (keyval, mods);
	shortcut_override_set (capture->app->settings, action, accel);
	termac_application_apply_action_shortcuts (capture->app);
	shortcut_button_refresh (capture->button, capture->app);
	adw_dialog_close (capture->dialog);
	return GDK_EVENT_STOP;
}

static void
shortcut_edit_clicked_cb (GtkButton *button, gpointer user_data)
{
	TermacApplication *app = user_data;
	/* The capture belongs to the window that holds the button (Preferences), not to
	 * whichever window is active. */
	GtkWidget *parent = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));
	AdwDialog *dialog = adw_dialog_new ();
	AdwStatusPage *page = ADW_STATUS_PAGE (adw_status_page_new ());
	GtkEventController *keys = gtk_event_controller_key_new ();
	ShortcutCapture *capture = g_new0 (ShortcutCapture, 1);

	capture->app = app;
	capture->button = button;
	capture->dialog = dialog;
	adw_dialog_set_title (dialog, _("Shortcut"));
	adw_dialog_set_content_width (dialog, 360);
	adw_status_page_set_icon_name (page, "input-keyboard-symbolic");
	adw_status_page_set_title (page, _("Press the new shortcut"));
	adw_status_page_set_description (page, _("Use Ctrl, Alt, Shift or Super. Press Escape to cancel."));
	gtk_widget_set_focusable (GTK_WIDGET (page), TRUE);
	g_signal_connect (keys, "key-pressed", G_CALLBACK (shortcut_capture_key_cb), capture);
	gtk_widget_add_controller (GTK_WIDGET (page), keys);
	adw_dialog_set_child (dialog, GTK_WIDGET (page));
	g_object_set_data_full (G_OBJECT (dialog), "shortcut-capture", capture, g_free);
	adw_dialog_present (dialog, parent);
	gtk_widget_grab_focus (GTK_WIDGET (page));
}

static void
shortcut_reset_clicked_cb (GtkButton *button, gpointer user_data)
{
	TermacApplication *app = user_data;
	GPtrArray *buttons = g_object_get_data (G_OBJECT (button), "shortcut-buttons");
	guint i;

	g_settings_reset (app->settings, "action-shortcuts");
	termac_application_apply_action_shortcuts (app);
	for (i = 0; buttons != NULL && i < buttons->len; i++)
		shortcut_button_refresh (g_ptr_array_index (buttons, i), app);
}

static void
termac_application_preferences_action (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
	TermacApplication *self = user_data;
	GtkWindow *parent = application_dialog_parent (self);
	AdwWindow *dialog = ADW_WINDOW (adw_window_new ());
	AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
	AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
	GtkToggleButton *search_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
	GtkButton *close_button = GTK_BUTTON (gtk_button_new_from_icon_name ("window-close-symbolic"));
	GtkWidget *search_bar = gtk_search_bar_new ();
	GtkWidget *search = gtk_search_entry_new ();
	AdwViewStack *stack = ADW_VIEW_STACK (adw_view_stack_new ());
	AdwViewSwitcherBar *switcher = ADW_VIEW_SWITCHER_BAR (adw_view_switcher_bar_new ());
	AdwPreferencesPage *appearance = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
	AdwPreferencesGroup *palette_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
	AdwPreferencesGroup *theme_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
	AdwPreferencesPage *behavior = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
	AdwPreferencesGroup *terminal_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
	AdwPreferencesPage *shortcuts = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
	AdwComboRow *scheme = ADW_COMBO_ROW (adw_combo_row_new ());
	AdwSpinRow *font = ADW_SPIN_ROW (adw_spin_row_new_with_range (8, 32, 1));
	GtkWidget *grid = gtk_grid_new ();
	GtkWidget *toggle = gtk_button_new_with_label (_("Show all palettes"));
	GPtrArray *cards = g_ptr_array_new_with_free_func (g_object_unref);
	GtkStringList *schemes = gtk_string_list_new ((const char *[]) {
		_("System"), _("Light"), _("Dark"), NULL });
	g_autofree char *scheme_value = g_settings_get_string (self->settings, "color-scheme");
	g_autofree char *palette_value = g_settings_get_string (self->settings, "palette");
	static const char *palette_names[] = { "Campbell", "Dracula", "GNOME", "High Contrast",
		"Horizon", "Linux", "Nord", "Solarized", "Tango", "Ubuntu", "VS Code", "XTerm" };
	static const char *palette_ids[] = { "campbell", "dracula", "gnome", "high-contrast",
		"horizon", "linux", "nord", "solarized", "tango", "ubuntu", "vscode", "xterm" };
	static const guint32 swatches[12][6] = {
		{ 0xc50f1f, 0x13a10e, 0xc19c00, 0x0037da, 0x881798, 0x3a96dd },
		{ 0xff5555, 0x50fa7b, 0xf1fa8c, 0xbd93f9, 0xff79c6, 0x8be9fd },
		{ 0xcc0000, 0x4e9a06, 0xc4a000, 0x3465a4, 0x75507b, 0x06989a },
		{ 0xff4040, 0x40ff40, 0xffff40, 0x4080ff, 0xff40ff, 0x40ffff },
		{ 0xe86671, 0x09976c, 0xfac484, 0x00a1db, 0xb963c1, 0x25bed9 },
		{ 0xaa0000, 0x00aa00, 0xaa5500, 0x0000aa, 0xaa00aa, 0x00aaaa },
		{ 0xbf616a, 0xa3be8c, 0xebcb8b, 0x81a1c1, 0xb48ead, 0x88c0d0 },
		{ 0xdc322f, 0x859900, 0xb58900, 0x268bd2, 0xd33682, 0x2aa198 },
		{ 0xcc0000, 0x4e9a06, 0xc4a000, 0x3465a4, 0x75507b, 0x06989a },
		{ 0xde382b, 0x39b54a, 0xffc706, 0x006fb8, 0x762671, 0x2cb5e9 },
		{ 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd },
		{ 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd },
	};
	guint i, j;
	(void) action;
	(void) parameter;

	gtk_window_set_title (GTK_WINDOW (dialog), _("Preferences"));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 560, 640);

	/* Header: search toggle, title and close button. */
	gtk_button_set_icon_name (GTK_BUTTON (search_button), "system-search-symbolic");
	adw_header_bar_set_title_widget (header, adw_window_title_new (_("Preferences"), NULL));
	adw_header_bar_pack_start (header, GTK_WIDGET (search_button));
	adw_header_bar_pack_end (header, GTK_WIDGET (close_button));
	g_signal_connect_swapped (close_button, "clicked", G_CALLBACK (gtk_window_close), dialog);
	gtk_search_bar_set_child (GTK_SEARCH_BAR (search_bar), search);
	gtk_search_bar_connect_entry (GTK_SEARCH_BAR (search_bar), GTK_EDITABLE (search));
	g_object_bind_property (search_button, "active", search_bar, "search-mode-enabled",
	                        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* Appearance: palette grid with expander, then terminal colors and font. */
	adw_preferences_page_set_title (appearance, _("Appearance"));
	adw_preferences_group_set_title (palette_group, _("Palette"));
	adw_preferences_group_set_header_suffix (palette_group, toggle);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
	gtk_widget_set_margin_start (grid, 8);
	gtk_widget_set_margin_end (grid, 8);
	gtk_widget_set_margin_top (grid, 16);
	gtk_widget_set_margin_bottom (grid, 8);
	g_object_set_data (G_OBJECT (grid), "palette-search", search);
	g_object_set_data (G_OBJECT (grid), "palette-cards", cards);
	g_object_set_data (G_OBJECT (grid), "palette-expanded", GINT_TO_POINTER (FALSE));
	g_signal_connect (toggle, "clicked", G_CALLBACK (palette_toggle_clicked_cb), grid);
	g_signal_connect (search, "changed", G_CALLBACK (palette_search_changed_cb), grid);

	for (i = 0; i < G_N_ELEMENTS (palette_names); i++)
	{
		GtkWidget *card = gtk_button_new ();
		GtkWidget *card_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
		GtkWidget *name = gtk_label_new (palette_names[i]);
		GtkWidget *color_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
		GtkWidget *header_row;
		GtkWidget *indicator;
		g_autofree char *palette_class = g_strdup_printf ("palette-%s", palette_ids[i]);

		gtk_widget_set_size_request (card, 175, 90);
		gtk_widget_add_css_class (card, "palette-card");
		gtk_widget_add_css_class (card, palette_class);
		gtk_widget_set_margin_start (card_box, 12);
		gtk_widget_set_margin_end (card_box, 12);
		gtk_widget_set_margin_top (card_box, 12);
		gtk_widget_set_margin_bottom (card_box, 12);
		gtk_label_set_xalign (GTK_LABEL (name), 0.0f);
		gtk_widget_add_css_class (name, "heading");
		for (j = 0; j < 6; j++)
		{
			GtkWidget *swatch = gtk_drawing_area_new ();
			PaletteSwatch *color = g_new (PaletteSwatch, 1);
			color->rgb = swatches[i][j];
			gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (swatch), palette_swatch_draw,
			                               color, palette_swatch_free);
			gtk_widget_set_size_request (swatch, 18, 18);
			gtk_box_append (GTK_BOX (color_box), swatch);
		}
		header_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
		indicator = gtk_image_new ();
		gtk_widget_set_hexpand (name, TRUE);
		gtk_box_append (GTK_BOX (header_row), name);
		gtk_box_append (GTK_BOX (header_row), indicator);
		gtk_box_append (GTK_BOX (card_box), header_row);
		gtk_box_append (GTK_BOX (card_box), color_box);
		gtk_button_set_child (GTK_BUTTON (card), card_box);
		g_object_set_data (G_OBJECT (card), "palette-indicator", indicator);
		g_object_set_data (G_OBJECT (card), "palette-title", (gpointer) palette_names[i]);
		g_object_set_data (G_OBJECT (card), "palette-grid", grid);
		g_object_set_data (G_OBJECT (card), "palette-name", (gpointer) palette_ids[i]);
		palette_card_set_selected (card, g_strcmp0 (palette_value, palette_ids[i]) == 0);
		g_signal_connect (card, "clicked", G_CALLBACK (palette_card_clicked_cb), self->settings);
		g_ptr_array_add (cards, g_object_ref_sink (card));
	}
	palette_refresh (grid);
	adw_preferences_group_add (palette_group, grid);
	adw_preferences_page_add (appearance, palette_group);

	adw_preferences_group_set_title (theme_group, _("Terminal"));
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (scheme), _("Color scheme"));
	adw_combo_row_set_model (scheme, G_LIST_MODEL (schemes));
	adw_combo_row_set_selected (scheme,
		g_strcmp0 (scheme_value, "light") == 0 ? 1 :
		g_strcmp0 (scheme_value, "dark") == 0 ? 2 : 0);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (font), _("Font size"));
	adw_action_row_set_subtitle (ADW_ACTION_ROW (font), _("Size in points"));
	adw_spin_row_set_value (font, g_settings_get_uint (self->settings, "font-size"));
	g_signal_connect (scheme, "notify::selected", G_CALLBACK (scheme_changed_cb), self->settings);
	g_signal_connect (font, "notify::value", G_CALLBACK (font_changed_cb), self->settings);
	adw_preferences_group_add (theme_group, GTK_WIDGET (scheme));
	adw_preferences_group_add (theme_group, GTK_WIDGET (font));
	adw_preferences_page_add (appearance, theme_group);

	/* General: startup, terminal, shell integration and safety options. */
	adw_preferences_page_set_title (behavior, _("General"));
	{
		AdwPreferencesGroup *startup_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
		AdwPreferencesGroup *integration_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
		AdwPreferencesGroup *safety_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
		AdwSpinRow *scrollback = ADW_SPIN_ROW (adw_spin_row_new_with_range (0, 100000, 500));
		AdwSwitchRow *confirm = ADW_SWITCH_ROW (adw_switch_row_new ());
		AdwSwitchRow *notify = ADW_SWITCH_ROW (adw_switch_row_new ());
		AdwSwitchRow *integration = ADW_SWITCH_ROW (adw_switch_row_new ());
		AdwSwitchRow *restore = ADW_SWITCH_ROW (adw_switch_row_new ());

		adw_preferences_group_set_title (startup_group, _("Startup"));
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (restore), _("Restore tabs at startup"));
		adw_action_row_set_subtitle (ADW_ACTION_ROW (restore), _("Reopens the tabs in their folders. Commands are not run again"));
		g_settings_bind (self->settings, "restore-session", restore, "active", G_SETTINGS_BIND_DEFAULT);
		adw_preferences_group_add (startup_group, GTK_WIDGET (restore));
		adw_preferences_page_add (behavior, startup_group);

		adw_preferences_group_set_title (terminal_group, _("Terminal"));
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (scrollback), _("Scrollback lines"));
		adw_action_row_set_subtitle (ADW_ACTION_ROW (scrollback), _("Lines kept above the screen in each tab"));
		adw_spin_row_set_value (scrollback, g_settings_get_uint (self->settings, "scrollback-lines"));
		g_signal_connect (scrollback, "notify::value", G_CALLBACK (scrollback_changed_cb), self->settings);
		adw_preferences_group_add (terminal_group, GTK_WIDGET (scrollback));
		adw_preferences_page_add (behavior, terminal_group);

		adw_preferences_group_set_title (integration_group, _("Integration"));
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (integration), _("Shell integration"));
		adw_action_row_set_subtitle (ADW_ACTION_ROW (integration), _("Reports command results. Bash only, for new tabs"));
		g_settings_bind (self->settings, "shell-integration", integration, "active", G_SETTINGS_BIND_DEFAULT);
		adw_preferences_group_add (integration_group, GTK_WIDGET (integration));
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (notify), _("Notify when tasks finish"));
		adw_action_row_set_subtitle (ADW_ACTION_ROW (notify), _("Only for tabs that are not in focus"));
		g_settings_bind (self->settings, "notifications", notify, "active", G_SETTINGS_BIND_DEFAULT);
		adw_preferences_group_add (integration_group, GTK_WIDGET (notify));
		adw_preferences_page_add (behavior, integration_group);

		adw_preferences_group_set_title (safety_group, _("Safety"));
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (confirm), _("Confirm before closing"));
		adw_action_row_set_subtitle (ADW_ACTION_ROW (confirm), _("Ask when a program is still running"));
		g_settings_bind (self->settings, "confirm-close", confirm, "active", G_SETTINGS_BIND_DEFAULT);
		adw_preferences_group_add (safety_group, GTK_WIDGET (confirm));
		adw_preferences_page_add (behavior, safety_group);
	}

	/* Shortcuts: editable window and app actions, then the fixed terminal list. */
	adw_preferences_page_set_title (shortcuts, _("Shortcuts"));
	{
		AdwPreferencesGroup *editor_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
		GtkWidget *reset = gtk_button_new_with_label (_("Restore defaults"));
		GPtrArray *buttons = g_ptr_array_new ();
		guint k;

		adw_preferences_group_set_title (editor_group, _("Editable shortcuts"));
		adw_preferences_group_set_header_suffix (editor_group, reset);
		gtk_widget_add_css_class (reset, "flat");
		for (k = 0; k < EDITABLE_SHORTCUT_COUNT; k++)
		{
			AdwActionRow *row = ADW_ACTION_ROW (adw_action_row_new ());
			GtkWidget *button = gtk_button_new ();

			adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _(editable_shortcuts[k].title));
			g_object_set_data (G_OBJECT (button), "shortcut-action", (gpointer) editable_shortcuts[k].action);
			g_object_set_data (G_OBJECT (button), "shortcut-default", (gpointer) editable_shortcuts[k].accel);
			gtk_widget_add_css_class (button, "flat");
			g_signal_connect (button, "clicked", G_CALLBACK (shortcut_edit_clicked_cb), self);
			shortcut_button_refresh (GTK_BUTTON (button), self);
			adw_action_row_add_suffix (row, button);
			adw_preferences_group_add (editor_group, GTK_WIDGET (row));
			g_ptr_array_add (buttons, button);
		}
		g_object_set_data_full (G_OBJECT (reset), "shortcut-buttons", buttons,
		                        (GDestroyNotify) g_ptr_array_unref);
		g_signal_connect (reset, "clicked", G_CALLBACK (shortcut_reset_clicked_cb), self);
		adw_preferences_page_add (shortcuts, editor_group);
	}

	/* Stack and bottom switcher. */
	adw_view_stack_add_titled_with_icon (stack, GTK_WIDGET (behavior), "behavior",
	                                     _("General"), "preferences-system-symbolic");
	adw_view_stack_add_titled_with_icon (stack, GTK_WIDGET (appearance), "appearance",
	                                     _("Appearance"), "applications-graphics-symbolic");
	adw_view_stack_add_titled_with_icon (stack, GTK_WIDGET (shortcuts), "shortcuts",
	                                     _("Shortcuts"), "input-keyboard-symbolic");
	adw_view_stack_add_titled_with_icon (stack, termac_application_attachments_page (),
	                                     "attachments", _("Attachments"), "mail-attachment-symbolic");
	adw_view_switcher_bar_set_stack (switcher, stack);
	adw_view_switcher_bar_set_reveal (switcher, TRUE);

	adw_toolbar_view_add_top_bar (toolbar, GTK_WIDGET (header));
	adw_toolbar_view_add_top_bar (toolbar, search_bar);
	adw_toolbar_view_set_content (toolbar, GTK_WIDGET (stack));
	adw_toolbar_view_add_bottom_bar (toolbar, GTK_WIDGET (switcher));
	adw_window_set_content (dialog, GTK_WIDGET (toolbar));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
termac_application_apply_settings (TermacApplication *self)
{
	g_autofree char *scheme = g_settings_get_string (self->settings, "color-scheme");
	AdwColorScheme color_scheme = ADW_COLOR_SCHEME_DEFAULT;
	GList *windows, *iter;
	if (g_strcmp0 (scheme, "light") == 0)
		color_scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
	else if (g_strcmp0 (scheme, "dark") == 0)
		color_scheme = ADW_COLOR_SCHEME_FORCE_DARK;
	adw_style_manager_set_color_scheme (adw_style_manager_get_default (), color_scheme);
	windows = gtk_application_get_windows (GTK_APPLICATION (self));
	for (iter = windows; iter != NULL; iter = iter->next)
		if (TERMAC_IS_WINDOW (iter->data))
			termac_window_sync_settings (TERMAC_WINDOW (iter->data));
}

static void
termac_application_shortcuts_action (GSimpleAction *action,
	                                 GVariant      *parameter,
	                                 gpointer       user_data)
{
	TermacApplication *self = user_data;
	GtkBuilder *builder = gtk_builder_new_from_resource (
		"/thiago/aciole/termac/shortcuts-dialog.ui");
	GtkWindow *parent = application_dialog_parent (self);
	AdwDialog *dialog = ADW_DIALOG (gtk_builder_get_object (builder, "shortcuts_dialog"));
	(void) action;
	(void) parameter;
	/* The builder owns the dialog: keep it alive until the dialog is closed. */
	g_signal_connect_swapped (dialog, "closed", G_CALLBACK (g_object_unref), builder);
	adw_dialog_present (dialog, GTK_WIDGET (parent));
}

static void
style_dark_changed_cb (AdwStyleManager *manager, GParamSpec *pspec, gpointer user_data)
{
	(void) manager;
	(void) pspec;
	termac_application_apply_settings (TERMAC_APPLICATION (user_data));
}

static void
settings_changed_cb (GSettings *settings, const char *key, gpointer user_data)
{
	(void) settings;
	(void) key;
	termac_application_apply_settings (TERMAC_APPLICATION (user_data));
}

static const GActionEntry app_actions[] = {
	{ "quit", termac_application_quit_action },
	{ "about", termac_application_about_action },
	{ "preferences", termac_application_preferences_action },
	{ "shortcuts", termac_application_shortcuts_action },
	{ "show-session", termac_application_show_session_action, "s" },
};

static void
termac_application_init (TermacApplication *self)
{
	GError *error = NULL;
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	/* Ctrl+letter combinations without Shift reach the shell (Ctrl+W, Ctrl+Q,
	 * Ctrl+G, Ctrl+R...), so application actions use Ctrl+Shift or other keys. */
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<control><shift>q", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.new-tab",
	                                       (const char *[]) { "<control><shift>t", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.new-window",
	                                       (const char *[]) { "<control><shift>n", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.toggle-overview",
	                                       (const char *[]) { "<control><shift>o", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.close-tab",
	                                       (const char *[]) { "<control><shift>w", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.fullscreen",
	                                       (const char *[]) { "F11", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.rename-tab",
	                                       (const char *[]) { "<control><shift>r", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.move-tab-backward",
	                                       (const char *[]) { "<control><shift>Page_Up", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.move-tab-forward",
	                                       (const char *[]) { "<control><shift>Page_Down", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.find",
	                                       (const char *[]) { "<control><shift>f", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.find-next",
	                                       (const char *[]) { "F3", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.find-previous",
	                                       (const char *[]) { "<shift>F3", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.attach-image",
	                                       (const char *[]) { "<control><shift>i", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.preferences",
	                                       (const char *[]) { "<control>comma", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.shortcuts",
	                                       (const char *[]) { "<control>question", NULL });
	source = g_settings_schema_source_new_from_directory (BUILD_SCHEMA_DIR,
		g_settings_schema_source_get_default (), TRUE, &error);
	if (source == NULL)
	{
		g_warning ("Could not load development settings schema: %s", error->message);
		g_clear_error (&error);
		return;
	}
	schema = g_settings_schema_source_lookup (source, APPLICATION_ID, FALSE);
	self->settings = g_settings_new_full (schema, NULL, NULL);
	g_settings_schema_unref (schema);
	g_settings_schema_source_unref (source);
	g_signal_connect (self->settings, "changed::color-scheme",
	                  G_CALLBACK (settings_changed_cb), self);
	g_signal_connect (self->settings, "changed::font-size",
	                  G_CALLBACK (settings_changed_cb), self);
	g_signal_connect (self->settings, "changed::palette",
	                  G_CALLBACK (settings_changed_cb), self);
	g_signal_connect (self->settings, "changed::scrollback-lines",
	                  G_CALLBACK (settings_changed_cb), self);
	termac_application_apply_action_shortcuts (self);

}

static void
termac_application_finalize (GObject *object)
{
	TermacApplication *self = TERMAC_APPLICATION (object);
	if (self->session_save_source != 0)
	{
		g_source_remove (self->session_save_source);
		self->session_save_source = 0;
	}
	g_clear_object (&self->settings);
	G_OBJECT_CLASS (termac_application_parent_class)->finalize (object);
}
