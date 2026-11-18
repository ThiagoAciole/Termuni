/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "termuni-attachments.h"
#include "termuni-application.h"
#include "termuni-terminal.h"
#include "termuni-util.h"
#include "termuni-window.h"
#include "termuni-dialog.h"

#define TERMUNI_NOTIFY_LONG_TASK_MS 5000

struct _TermuniWindow
{
	AdwApplicationWindow parent_instance;
	AdwTabOverview *overview;
	AdwTabView *tab_view;
	AdwTabBar *tab_bar;
	AdwToastOverlay *toast_overlay;
	GtkButton *overview_button;
	GtkLabel *header_title;
	GtkLabel *header_subtitle;
	GtkMenuButton *menu_button;
	GtkMenuButton *agents_button;
	GtkSearchBar *find_bar;
	GtkSearchEntry *find_entry;
	GtkButton *find_close_button;
	GtkToggleButton *theme_buttons[3];
	GtkLabel *zoom_label;
	gboolean close_confirmed;
	guint tab_number;
};

G_DEFINE_FINAL_TYPE (TermuniWindow, termuni_window, ADW_TYPE_APPLICATION_WINDOW)

static AdwTabPage *termuni_window_create_tab (TermuniWindow *self, const char *cwd,
                                             const char *manual_title);
static void termuni_window_update_header (TermuniWindow *self);
static void window_request_save (TermuniWindow *self);

/* ---- small helpers ---------------------------------------------------- */

static GSettings *
window_settings (TermuniWindow *self)
{
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	return app != NULL ? termuni_application_get_settings (TERMUNI_APPLICATION (app)) : NULL;
}

/* The palette for the current light or dark mode. The chosen palette is used
 * in dark mode; light mode uses the built-in light palette. */
/* Tints the header, tab strip and search bar with the palette colors, so the
 * window chrome matches the terminal. A single display-wide provider is used
 * because all windows share the same palette. */
static GtkCssProvider *chrome_provider = NULL;

#define TERMUNI_CHROME_DARKEN 0.9

static void
window_apply_chrome (const char *palette)
{
	GdkRGBA background;
	GdkRGBA foreground;
	g_autofree char *raw_bg = NULL;
	g_autofree char *chrome_bg = NULL;
	g_autofree char *fg = NULL;
	g_autofree char *css = NULL;

	if (!termuni_terminal_palette_colors (palette, &background, &foreground))
		return;
	if (chrome_provider == NULL)
	{
		chrome_provider = gtk_css_provider_new ();
		gtk_style_context_add_provider_for_display (gdk_display_get_default (),
		                                            GTK_STYLE_PROVIDER (chrome_provider),
		                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
	}
	raw_bg = g_strdup_printf ("#%02x%02x%02x", (guint) (background.red * 255),
	                          (guint) (background.green * 255), (guint) (background.blue * 255));
	/* The chrome is a little darker than the terminal so the two areas separate. */
	chrome_bg = g_strdup_printf ("#%02x%02x%02x",
	                             (guint) (background.red * 255 * TERMUNI_CHROME_DARKEN),
	                             (guint) (background.green * 255 * TERMUNI_CHROME_DARKEN),
	                             (guint) (background.blue * 255 * TERMUNI_CHROME_DARKEN));
	fg = g_strdup_printf ("#%02x%02x%02x", (guint) (foreground.red * 255),
	                      (guint) (foreground.green * 255), (guint) (foreground.blue * 255));
	/* libadwaita takes its colors from named variables; overriding them makes
	 * every surface (windows, dialogs, popovers, cards, the tab overview) use
	 * the palette, as the terminal does. */
	css = g_strdup_printf (
		"@define-color window_bg_color %1$s; @define-color window_fg_color %3$s;"
		" @define-color headerbar_bg_color %1$s; @define-color headerbar_fg_color %3$s;"
		" @define-color headerbar_border_color %1$s;"
		" @define-color view_bg_color %2$s; @define-color view_fg_color %3$s;"
		" @define-color popover_bg_color %2$s; @define-color popover_fg_color %3$s;"
		" @define-color dialog_bg_color %2$s; @define-color dialog_fg_color %3$s;"
		" @define-color card_bg_color %2$s; @define-color card_fg_color %3$s;"
		" @define-color sidebar_bg_color %1$s; @define-color sidebar_fg_color %3$s;"
		" window.termuni-window, .termuni-header, .termuni-tabbar, .termuni-searchbar,"
		" popover > contents, popover.menu > contents, dialog, dialog > contents {"
		" background: %1$s; background-image: none; box-shadow: none; border-color: transparent;"
		" color: %3$s; }"
		" .termuni-header label, .termuni-tabbar tab label, .termuni-searchbar entry,"
		" .termuni-header image, .termuni-tabbar image { color: %3$s; }",
		chrome_bg, raw_bg, fg);
	gtk_css_provider_load_from_string (chrome_provider, css);
}

static char *
window_effective_palette (GSettings *settings)
{
	if (!adw_style_manager_get_dark (adw_style_manager_get_default ()))
		return g_strdup ("light");
	return g_settings_get_string (settings, "palette");
}

static TermuniWindow *
window_from_widget (GtkWidget *widget)
{
	GtkRoot *root = gtk_widget_get_root (widget);
	return TERMUNI_IS_WINDOW (root) ? TERMUNI_WINDOW (root) : NULL;
}

static TermuniTerminal *
window_terminal_for_page (AdwTabPage *page)
{
	return TERMUNI_TERMINAL (adw_tab_page_get_child (page));
}

static TermuniTerminal *
window_selected_terminal (TermuniWindow *self)
{
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	return page != NULL ? window_terminal_for_page (page) : NULL;
}

static gboolean
window_confirm_close_enabled (TermuniWindow *self)
{
	GSettings *settings = window_settings (self);
	return settings == NULL || g_settings_get_boolean (settings, "confirm-close");
}

static const TermuniAgent *
window_agent (TermuniWindow *self)
{
	GtkApplication *application = gtk_window_get_application (GTK_WINDOW (self));
	return TERMUNI_IS_APPLICATION (application) ? termuni_application_get_agent (TERMUNI_APPLICATION (application)) : termuni_agent_default ();
}

static void
window_show_toast (TermuniWindow *self,
                   const char   *text)
{
	adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (text));
}

static guint
window_busy_pages (TermuniWindow *self)
{
	guint i;
	guint busy = 0;

	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
	{
		AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
		if (termuni_terminal_has_foreground_job (window_terminal_for_page (page)))
			busy++;
	}
	return busy;
}

guint
termuni_window_busy_tabs (TermuniWindow *self)
{
	g_return_val_if_fail (TERMUNI_IS_WINDOW (self), 0);
	return window_busy_pages (self);
}

/* Shows an AdwAlertDialog with Cancel and a destructive confirmation. */
static AdwDialog *
window_confirm_dialog (const char *heading,
                       const char *body,
                       const char *action_label)
{
	AdwDialog *dialog = adw_alert_dialog_new (heading, body);
	adw_dialog_set_title (dialog, heading);

	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
	                                "cancel", _("Cancel"),
	                                "close", action_label,
	                                NULL);
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "close",
	                                          ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
	adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");
	return dialog;
}

/* ---- menu and theme --------------------------------------------------- */

static GtkWidget *
termuni_window_menu_row (const char *title,
                        const char *shortcut,
                        const char *action_name)
{
	GtkWidget *button = gtk_button_new ();
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
	GtkWidget *label = gtk_label_new (title);
	GtkWidget *accelerator = gtk_label_new (shortcut);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_xalign (GTK_LABEL (accelerator), 1.0f);
	gtk_widget_add_css_class (accelerator, "dim-label");
	gtk_widget_set_hexpand (label, TRUE);
	gtk_box_append (GTK_BOX (box), label);
	gtk_box_append (GTK_BOX (box), accelerator);
	gtk_button_set_child (GTK_BUTTON (button), box);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
	gtk_widget_add_css_class (button, "flat");
	return button;
}

static void
termuni_window_scheme_toggled (GtkToggleButton *button, gpointer user_data)
{
	TermuniWindow *self = user_data;
	GSettings *settings = window_settings (self);
	const char *scheme;
	if (!gtk_toggle_button_get_active (button) || settings == NULL)
		return;
	scheme = g_object_get_data (G_OBJECT (button), "color-scheme");
	g_settings_set_string (settings, "color-scheme", scheme);
}

static void
termuni_window_zoom (GtkButton *button, gpointer user_data)
{
	TermuniWindow *self = user_data;
	GSettings *settings = window_settings (self);
	int delta = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "zoom-delta"));
	guint size;
	g_autofree char *percentage = NULL;
	if (settings == NULL)
		return;
	size = (guint) CLAMP ((int) g_settings_get_uint (settings, "font-size") + delta, 8, 32);
	g_settings_set_uint (settings, "font-size", size);
	percentage = g_strdup_printf ("%u%%", size * 100 / 11);
	gtk_label_set_text (self->zoom_label, percentage);
}

static void
termuni_window_new_window (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
	TermuniWindow *self = user_data;
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	GtkWindow *window;
	(void) action;
	(void) parameter;
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	termuni_window_sync_settings (TERMUNI_WINDOW (window));
	gtk_window_present (window);
}

static void
termuni_window_toggle_fullscreen (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
	GtkWindow *window = GTK_WINDOW (user_data);
	(void) action;
	(void) parameter;
	if (gtk_window_is_fullscreen (window))
		gtk_window_unfullscreen (window);
	else
		gtk_window_fullscreen (window);
}

static void
termuni_window_build_menu (TermuniWindow *self)
{
	GtkWidget *popover = gtk_popover_new ();
	GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *themes = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
	GtkWidget *zoom = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget *zoom_out = gtk_button_new ();
	GtkWidget *zoom_in = gtk_button_new ();
	GtkWidget *zoom_value = gtk_label_new ("100%");
	/* Order matches the reference: light, dark, follow system. */
	const char *theme_names[] = { "light", "dark", "system" };
	const char *theme_resources[] = { "/io/github/thiagoaciole/termuni/tema-claro.svg",
	                                  "/io/github/thiagoaciole/termuni/tema-escuro.svg",
	                                  "/io/github/thiagoaciole/termuni/tema-automatico.svg" };
	const char *theme_tooltips[] = { N_("Light"), N_("Dark"), N_("Follow system theme") };
	GtkToggleButton *previous = NULL;
	guint i;
	gtk_widget_set_margin_start (content, 8);
	gtk_widget_set_margin_end (content, 8);
	gtk_widget_set_margin_top (content, 8);
	gtk_widget_set_margin_bottom (content, 8);
	for (i = 0; i < G_N_ELEMENTS (theme_names); i++)
	{
		GtkWidget *button = gtk_toggle_button_new ();
		/* Render the preview at its display size: the natural size of the SVG
		 * (256 px) would otherwise decide the size of the menu card. */
		g_autoptr(GdkPixbuf) preview = gdk_pixbuf_new_from_resource_at_scale (
			theme_resources[i], 64, 64, TRUE, NULL);
		GtkWidget *picture;
		picture = NULL;
		if (preview != NULL)
		{
			g_autofree char *png = NULL;
			gsize png_length = 0;
			if (gdk_pixbuf_save_to_buffer (preview, &png, &png_length, "png", NULL, NULL))
			{
				g_autoptr(GBytes) bytes = g_bytes_new_take (png, png_length);
				g_autoptr(GdkTexture) texture = gdk_texture_new_from_bytes (bytes, NULL);
				png = NULL;
				if (texture != NULL)
					picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
			}
		}
		if (picture == NULL)
			picture = gtk_picture_new_for_resource (theme_resources[i]);
		gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
		gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
		gtk_widget_set_size_request (picture, 64, 64);
		gtk_button_set_child (GTK_BUTTON (button), picture);
		gtk_widget_add_css_class (button, "theme-card");
		gtk_widget_set_tooltip_text (button, _(theme_tooltips[i]));
		gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (button), previous);
		gtk_widget_set_size_request (button, 72, 72);
		previous = GTK_TOGGLE_BUTTON (button);
		g_object_set_data (G_OBJECT (button), "color-scheme", (gpointer) theme_names[i]);
		g_signal_connect (button, "toggled", G_CALLBACK (termuni_window_scheme_toggled), self);
		self->theme_buttons[i] = GTK_TOGGLE_BUTTON (button);
		gtk_box_append (GTK_BOX (themes), button);
	}
	gtk_widget_set_halign (themes, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top (themes, 8);
	gtk_widget_set_margin_bottom (themes, 8);
	gtk_box_append (GTK_BOX (content), themes);
	gtk_box_append (GTK_BOX (content), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	gtk_widget_set_hexpand (zoom_value, TRUE);
	gtk_widget_add_css_class (zoom_value, "dim-label");
	gtk_widget_add_css_class (zoom_out, "flat");
	gtk_widget_add_css_class (zoom_in, "flat");
	gtk_button_set_label (GTK_BUTTON (zoom_out), "\u2212");
	gtk_button_set_label (GTK_BUTTON (zoom_in), "+");
	g_object_set_data (G_OBJECT (zoom_out), "zoom-delta", GINT_TO_POINTER (-1));
	g_object_set_data (G_OBJECT (zoom_in), "zoom-delta", GINT_TO_POINTER (1));
	g_signal_connect (zoom_out, "clicked", G_CALLBACK (termuni_window_zoom), self);
	g_signal_connect (zoom_in, "clicked", G_CALLBACK (termuni_window_zoom), self);
	gtk_box_append (GTK_BOX (zoom), zoom_out);
	gtk_box_append (GTK_BOX (zoom), zoom_value);
	self->zoom_label = GTK_LABEL (zoom_value);
	gtk_box_append (GTK_BOX (zoom), zoom_in);
	gtk_box_append (GTK_BOX (content), zoom);
	gtk_box_append (GTK_BOX (content), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("New tab"), "Ctrl+Shift+T", "win.new-tab"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("New window"), "Ctrl+Shift+N", "win.new-window"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("Open Agent"), "Ctrl+Shift+A", "win.launch-agent"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("Attach image"), "Ctrl+Shift+I", "win.attach-image"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("Full screen"), "F11", "win.fullscreen"));
	gtk_box_append (GTK_BOX (content), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("Preferences"), "Ctrl+,", "app.preferences"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("Keyboard shortcuts"), "Ctrl+?", "app.shortcuts"));
	gtk_box_append (GTK_BOX (content), termuni_window_menu_row (_("About"), "", "app.about"));
	gtk_popover_set_child (GTK_POPOVER (popover), content);
	gtk_menu_button_set_popover (self->menu_button, popover);
}

/* Quick launch: one entry per registered agent, each opening a new tab. */
static void
termuni_window_build_agents_menu (TermuniWindow *self)
{
	g_autoptr(GMenu) menu = g_menu_new ();
	gsize count = 0;
	const TermuniAgent *const *agents = termuni_agent_all (&count);
	gsize i;
	for (i = 0; i < count; i++)
	{
		g_autoptr(GMenuItem) item = g_menu_item_new (agents[i]->display_name, NULL);
		g_menu_item_set_action_and_target (item, "win.launch-agent-id", "s", agents[i]->id);
		g_menu_append_item (menu, item);
	}
	gtk_menu_button_set_menu_model (self->agents_button, G_MENU_MODEL (menu));
}

/* ---- titles, header, attention --------------------------------------- */

static void
termuni_window_update_header (TermuniWindow *self)
{
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	const char *title = page != NULL ? adw_tab_page_get_title (page) : "Termuni";
	TermuniTerminal *terminal = page != NULL ? window_terminal_for_page (page) : NULL;
	const char *cwd = terminal != NULL ? termuni_terminal_get_cwd (terminal) : NULL;
	g_autofree char *display = NULL;

	gtk_label_set_text (self->header_title, title);
	if (cwd != NULL)
		display = termuni_display_path (cwd, g_get_home_dir ());
	gtk_label_set_text (self->header_subtitle, display != NULL ? display : "~");
}

static void
terminal_title_changed_cb (TermuniTerminal *terminal,
                           const char     *title,
                           gpointer        user_data)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	AdwTabPage *page;
	(void) user_data;
	if (self == NULL || title == NULL || *title == '\0')
		return;
	g_object_set_data_full (G_OBJECT (terminal), "auto-title", g_strdup (title), g_free);
	if (g_object_get_data (G_OBJECT (terminal), "manual-title") != NULL)
		return;
	page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (terminal));
	if (page == NULL)
		return;
	adw_tab_page_set_title (page, title);
	if (page == adw_tab_view_get_selected_page (self->tab_view))
		termuni_window_update_header (self);
}

static void
terminal_cwd_changed_cb (TermuniTerminal *terminal,
                         const char     *cwd,
                         gpointer        user_data)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	(void) cwd;
	(void) user_data;
	if (self == NULL)
		return;
	window_request_save (self);
	if (window_selected_terminal (self) == terminal)
		termuni_window_update_header (self);
}

static void
terminal_flag_attention (TermuniTerminal *terminal)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	AdwTabPage *page;

	if (self == NULL)
		return;
	/* The selected tab is the one on screen: it never needs attention. */
	page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (terminal));
	if (page != NULL && page != adw_tab_view_get_selected_page (self->tab_view))
		adw_tab_page_set_needs_attention (page, TRUE);
}

static void
terminal_message_cb (TermuniTerminal *terminal,
                     const char     *message,
                     gpointer        user_data)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	(void) user_data;
	if (self != NULL && message != NULL)
		window_show_toast (self, message);
}

static void
terminal_bell_cb (TermuniTerminal *terminal, gpointer user_data)
{
	(void) user_data;
	terminal_flag_attention (terminal);
}

/* The shell prints its prompt when a tab opens; that is not activity. */
#define TERMUNI_ACTIVITY_GRACE_US (3 * G_USEC_PER_SEC)

static void
terminal_output_cb (TermuniTerminal *terminal, gpointer user_data)
{
	gint64 *created = g_object_get_data (G_OBJECT (terminal), "termuni-created-us");
	(void) user_data;
	if (created == NULL || g_get_monotonic_time () - *created < TERMUNI_ACTIVITY_GRACE_US)
		return;
	terminal_flag_attention (terminal);
}

static void
terminal_notify_user (TermuniWindow   *self,
                      TermuniTerminal *terminal,
                      const char     *title,
                      const char     *body,
                      gboolean        took_long_enough)
{
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	GSettings *settings = window_settings (self);
	g_autoptr(GNotification) notification = NULL;
	const char *session = termuni_terminal_get_session_id (terminal);

	if (app == NULL || !termuni_should_notify (
	        settings != NULL && g_settings_get_boolean (settings, "notifications"),
	        gtk_window_is_active (GTK_WINDOW (self)),
	        window_selected_terminal (self) == terminal, took_long_enough))
		return;
	notification = g_notification_new (title);
	g_notification_set_body (notification, body);
	g_notification_set_default_action_and_target (notification, "app.show-session", "s", session);
	g_application_send_notification (G_APPLICATION (app), session, notification);
}

static void
terminal_command_finished_cb (TermuniTerminal *terminal,
                              gint            exit_code,
                              gpointer        user_data)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	g_autofree char *body = NULL;
	(void) user_data;
	if (self == NULL)
		return;
	terminal_flag_attention (terminal);
	body = exit_code == 0 ? NULL : g_strdup_printf (_("Exit status %d"), exit_code);
	/* Explicit shell completion reports are eligible regardless of duration;
	 * the process-polling fallback below retains its long-task threshold. */
	terminal_notify_user (self, terminal, _("Command finished"),
	                      body != NULL ? body : _("Completed successfully"), TRUE);
}

static void
terminal_job_finished_cb (TermuniTerminal *terminal,
                          guint64         duration_ms,
                          gpointer        user_data)
{
	TermuniWindow *self = window_from_widget (GTK_WIDGET (terminal));
	GSettings *settings;
	(void) user_data;
	if (self == NULL)
		return;
	settings = window_settings (self);
	/* With shell integration the command result arrives as its own event. */
	if (settings != NULL && g_settings_get_boolean (settings, "shell-integration"))
		return;
	if (duration_ms < TERMUNI_NOTIFY_LONG_TASK_MS)
		return;
	terminal_flag_attention (terminal);
	terminal_notify_user (self, terminal, _("Task finished"), _("A long-running task in a tab has finished."),
	                      duration_ms >= TERMUNI_NOTIFY_LONG_TASK_MS);
}

/* ---- tabs ------------------------------------------------------------- */

static AdwTabPage *
termuni_window_create_tab_with_command (TermuniWindow *self,
                                      const char *cwd,
                                      const char *manual_title,
                                      const char *command)
{
	GSettings *settings = window_settings (self);
	gboolean integration = settings != NULL && g_settings_get_boolean (settings, "shell-integration");
	GtkWidget *terminal = termuni_terminal_new_command (cwd, integration, command);
	AdwTabPage *page;
	g_autofree char *title = NULL;
	const char *user = g_get_user_name ();
	const char *host = g_get_host_name ();

	if (command != NULL && termuni_terminal_get_error (TERMUNI_TERMINAL (terminal)) != NULL)
	{
		window_show_toast (self, termuni_terminal_get_error (TERMUNI_TERMINAL (terminal)));
		g_object_ref_sink (terminal);
		g_object_unref (terminal);
		return NULL;
	}
	termuni_terminal_set_agent (TERMUNI_TERMINAL (terminal),
		window_agent (self));
	page = adw_tab_view_append (self->tab_view, terminal);
	if (settings != NULL)
	{
		g_autofree char *palette = NULL;
		termuni_terminal_set_font_size (TERMUNI_TERMINAL (terminal), g_settings_get_uint (settings, "font-size"));
		termuni_terminal_set_scrollback_lines (TERMUNI_TERMINAL (terminal),
		                                      g_settings_get_uint (settings, "scrollback-lines"));
		palette = window_effective_palette (settings);
		termuni_terminal_set_palette (TERMUNI_TERMINAL (terminal), palette);
	}
	{
		gint64 *created = g_new (gint64, 1);
		*created = g_get_monotonic_time ();
		g_object_set_data_full (G_OBJECT (terminal), "termuni-created-us", created, g_free);
	}
	g_signal_connect (terminal, "title-changed", G_CALLBACK (terminal_title_changed_cb), NULL);
	g_signal_connect (terminal, "cwd-changed", G_CALLBACK (terminal_cwd_changed_cb), NULL);
	g_signal_connect (terminal, "bell", G_CALLBACK (terminal_bell_cb), NULL);
	g_signal_connect (terminal, "message", G_CALLBACK (terminal_message_cb), NULL);
	g_signal_connect (terminal, "output", G_CALLBACK (terminal_output_cb), NULL);
	g_signal_connect (terminal, "command-finished", G_CALLBACK (terminal_command_finished_cb), NULL);
	g_signal_connect (terminal, "job-finished", G_CALLBACK (terminal_job_finished_cb), NULL);

	title = g_strdup_printf ("%s@%s: ~", user, host);
	self->tab_number++;
	adw_tab_page_set_title (page, title);
	{
		g_autoptr (GIcon) icon = g_themed_icon_new ("utilities-terminal-symbolic");

		adw_tab_page_set_icon (page, icon);
	}
	if (manual_title != NULL && *manual_title != '\0')
	{
		g_object_set_data_full (G_OBJECT (terminal), "manual-title", g_strdup (manual_title), g_free);
		adw_tab_page_set_title (page, manual_title);
	}
	return page;
}

static AdwTabPage *
termuni_window_create_tab (TermuniWindow *self, const char *cwd, const char *manual_title)
{
	return termuni_window_create_tab_with_command (self, cwd, manual_title, NULL);
}

typedef struct { GWeakRef window; const TermuniAgent *agent; char *cwd; gboolean quick; } AgentLaunch;

/* Both launch actions share one availability check at a time. */
static void
window_set_agent_actions_enabled (TermuniWindow *self, gboolean enabled)
{
	static const char *const names[] = { "launch-agent", "launch-agent-id" };
	guint i;
	for (i = 0; i < G_N_ELEMENTS (names); i++)
		g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), names[i])),
		                             enabled);
}

static void
window_run_agent_in_current_tab (TermuniWindow *self, const char *command)
{
	TermuniTerminal *terminal = window_selected_terminal (self);
	if (terminal == NULL)
		return;
	if (termuni_terminal_has_foreground_job (terminal))
	{
		/* Typed input would reach the running program instead of the shell. */
		window_show_toast (self, _("Finish the running program first"));
		return;
	}
	termuni_terminal_run_command (terminal, command);
	gtk_widget_grab_focus (GTK_WIDGET (terminal));
}

static void
agent_launch_checked (GObject *source, GAsyncResult *result, gpointer data)
{
	AgentLaunch *launch = data;
	g_autoptr(TermuniWindow) self = g_weak_ref_get (&launch->window);
	g_autoptr(GError) error = NULL;
	TermuniAgentAvailability status = termuni_agent_check_finish (result, &error);
	if (self != NULL && gtk_widget_get_visible (GTK_WIDGET (self)))
	{
		window_set_agent_actions_enabled (self, TRUE);
		if (error != NULL)
			window_show_toast (self, _("Could not check agent availability. Try again."));
		else if (status == TERMUNI_AGENT_MISSING)
		{
			g_autofree char *message = g_strdup_printf (_("%s is not installed"), launch->agent->display_name);
			window_show_toast (self, message);
		}
		else if (status == TERMUNI_AGENT_DIRECTORY_MISSING)
			window_show_toast (self, _("The folder no longer exists"));
		else
		{
			g_autofree char *command = termuni_agent_build_launch_command (launch->agent);
			/* Reuse the selected shell when it is idle; preserve a running
			 * foreground program by launching the agent in a new tab. */
			if (launch->quick &&
			    !termuni_terminal_has_foreground_job (window_selected_terminal (self)))
				window_run_agent_in_current_tab (self, command);
			else
			{
				AdwTabPage *page = termuni_window_create_tab_with_command (self, launch->cwd, NULL, command);
				if (page != NULL)
				{
					adw_tab_view_set_selected_page (self->tab_view, page);
					gtk_widget_grab_focus (adw_tab_page_get_child (page));
				}
			}
		}
	}
	g_weak_ref_clear (&launch->window);
	g_free (launch->cwd);
	g_free (launch);
}

static void
window_launch_agent_in_tab (TermuniWindow *self, const TermuniAgent *agent, gboolean quick)
{
	TermuniTerminal *terminal = window_selected_terminal (self);
	const char *cwd = terminal != NULL ? termuni_terminal_get_cwd (terminal) : NULL;
	AgentLaunch *launch = g_new0 (AgentLaunch, 1);
	g_weak_ref_init (&launch->window, self);
	launch->agent = agent;
	launch->quick = quick;
	launch->cwd = g_strdup (cwd != NULL ? cwd : g_get_home_dir ());
	window_set_agent_actions_enabled (self, FALSE);
	termuni_agent_check_async (launch->agent, launch->cwd, NULL, agent_launch_checked, launch);
}

void
termuni_window_launch_agent (TermuniWindow *self)
{
	window_launch_agent_in_tab (self, window_agent (self), FALSE);
}

static void
termuni_window_launch_agent_action (GSimpleAction *action, GVariant *parameter, gpointer data)
{
	termuni_window_launch_agent (TERMUNI_WINDOW (data));
}

/* Launches a specific agent, chosen from the header quick-launch menu. */
static void
termuni_window_launch_agent_id_action (GSimpleAction *action, GVariant *parameter, gpointer data)
{
	TermuniWindow *self = TERMUNI_WINDOW (data);
	const TermuniAgent *agent = termuni_agent_find (g_variant_get_string (parameter, NULL));
	(void) action;
	if (agent == NULL)
	{
		window_show_toast (self, _("Unknown agent"));
		return;
	}
	window_launch_agent_in_tab (self, agent, TRUE);
}

static void
termuni_window_add_tab (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page;
	(void) action;
	(void) parameter;
	page = termuni_window_create_tab (self, NULL, NULL);
	adw_tab_view_set_selected_page (self->tab_view, page);
	termuni_window_update_header (self);
	gtk_widget_grab_focus (adw_tab_page_get_child (page));
}

static AdwTabPage *
termuni_window_create_tab_cb (AdwTabOverview *overview,
                             gpointer        user_data)
{
	(void) overview;
	return termuni_window_create_tab (TERMUNI_WINDOW (user_data), NULL, NULL);
}

static void
termuni_window_toggle_overview (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
	TermuniWindow *self = user_data;
	(void) action;
	(void) parameter;
	adw_tab_overview_set_open (self->overview,
	                           !adw_tab_overview_get_open (self->overview));
}

static void
termuni_window_close_tab (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	(void) action;
	(void) parameter;
	if (page != NULL)
		adw_tab_view_close_page (self->tab_view, page);
}

static void
close_page_response (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page = g_object_get_data (source, "termuni-page");
	gboolean confirmed = g_strcmp0 (adw_alert_dialog_choose_finish (ADW_ALERT_DIALOG (source), result),
	                                "close") == 0;

	adw_tab_view_close_page_finish (self->tab_view, page, confirmed);
	if (confirmed && adw_tab_view_get_n_pages (self->tab_view) == 0)
		gtk_window_close (GTK_WINDOW (self));
	g_object_unref (self);
}

static gboolean
termuni_window_close_page_cb (AdwTabView *view,
                             AdwTabPage *page,
                             gpointer    user_data)
{
	TermuniWindow *self = user_data;
	TermuniTerminal *terminal = window_terminal_for_page (page);

	if (!window_confirm_close_enabled (self) || !termuni_terminal_has_foreground_job (terminal))
	{
		adw_tab_view_close_page_finish (view, page, TRUE);
		if (adw_tab_view_get_n_pages (view) == 0)
			gtk_window_close (GTK_WINDOW (self));
		return GDK_EVENT_STOP;
	}
	{
		AdwDialog *dialog = window_confirm_dialog (_("Close this tab?"),
		                                           _("A program is still running in this tab. Closing the tab stops it."),
		                                           _("Close Tab"));
		g_object_set_data_full (G_OBJECT (dialog), "termuni-page", g_object_ref (page), g_object_unref);
		termuni_alert_choose_window (ADW_ALERT_DIALOG (dialog), GTK_WINDOW (self),
		                         close_page_response, g_object_ref (self));
	}
	return GDK_EVENT_STOP;
}

static void
close_window_response (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	TermuniWindow *self = user_data;

	if (g_strcmp0 (adw_alert_dialog_choose_finish (ADW_ALERT_DIALOG (source), result), "close") == 0)
	{
		self->close_confirmed = TRUE;
		gtk_window_close (GTK_WINDOW (self));
	}
	g_object_unref (self);
}

static gboolean
termuni_window_close_request_cb (GtkWindow *window,
                                gpointer   user_data)
{
	TermuniWindow *self = user_data;
	guint busy;
	AdwDialog *dialog;
	g_autofree char *body = NULL;

	(void) window;
	if (self->close_confirmed || !window_confirm_close_enabled (self))
		busy = 0;
	else
		busy = window_busy_pages (self);
	if (busy == 0)
	{
		GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
		/* Keep the session of the last window open when the app closes. */
		if (app != NULL)
			termuni_application_window_closing (TERMUNI_APPLICATION (app));
		return GDK_EVENT_PROPAGATE;
	}
	body = g_strdup_printf (ngettext ("%u tab has a program running. Closing the window stops it.",
	                                  "%u tabs have programs running. Closing the window stops them.", busy),
	                        busy);
	dialog = window_confirm_dialog (_("Close window?"), body, _("Close Window"));
	termuni_alert_choose_window (ADW_ALERT_DIALOG (dialog), GTK_WINDOW (self),
	                         close_window_response, g_object_ref (self));
	return GDK_EVENT_STOP;
}

static void
tab_count_changed_cb (AdwTabView *view, GParamSpec *pspec, gpointer user_data)
{
	(void) view;
	(void) pspec;
	window_request_save (TERMUNI_WINDOW (user_data));
}

/* Asks the application to save the session after a change in this window. */
static void
window_request_save (TermuniWindow *self)
{
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	if (app != NULL)
		termuni_application_request_session_save (TERMUNI_APPLICATION (app));
}

static void
selected_page_changed_cb (AdwTabView *view,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
	AdwTabPage *page = adw_tab_view_get_selected_page (view);
	TermuniWindow *self = user_data;
	(void) pspec;
	window_request_save (self);
	termuni_window_update_header (self);
	if (page != NULL)
	{
		adw_tab_page_set_needs_attention (page, FALSE);
		termuni_terminal_set_highlight (TERMUNI_TERMINAL (adw_tab_page_get_child (page)),
		    gtk_search_bar_get_search_mode (self->find_bar) ? gtk_editable_get_text (GTK_EDITABLE (self->find_entry)) : NULL);
		gtk_widget_grab_focus (adw_tab_page_get_child (page));
	}
}

static void
termuni_window_move_tab (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	gboolean backward = g_strcmp0 (g_action_get_name (G_ACTION (action)), "move-tab-backward") == 0;
	(void) parameter;
	if (page == NULL)
		return;
	if (backward)
		adw_tab_view_reorder_backward (self->tab_view, page);
	else
		adw_tab_view_reorder_forward (self->tab_view, page);
}

static void
termuni_window_move_tab_to_new_window (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
	TermuniWindow *self = user_data;
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	TermuniWindow *window;
	(void) action;
	(void) parameter;
	if (page == NULL || app == NULL)
		return;
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	termuni_window_sync_settings (window);
	/* The new window starts with a default tab; the transferred tab
	 * becomes the second one, so the default can be closed safely. */
	adw_tab_view_transfer_page (self->tab_view, page, window->tab_view, 0);
	adw_tab_view_close_page (window->tab_view, adw_tab_view_get_nth_page (window->tab_view, 1));
	adw_tab_view_set_selected_page (window->tab_view, adw_tab_view_get_nth_page (window->tab_view, 0));
	gtk_window_present (GTK_WINDOW (window));
	if (adw_tab_view_get_n_pages (self->tab_view) == 0)
		gtk_window_close (GTK_WINDOW (self));
}

static void
termuni_window_setup_menu_cb (AdwTabView *view,
                             AdwTabPage *page,
                             gpointer    user_data)
{
	(void) user_data;
	if (page != NULL)
		adw_tab_view_set_selected_page (view, page);
}

/* ---- find ------------------------------------------------------------- */

static void
window_find_text (TermuniWindow *self,
                  gboolean      backwards)
{
	const char *text = gtk_editable_get_text (GTK_EDITABLE (self->find_entry));
	TermuniTerminal *terminal = window_selected_terminal (self);

	if (terminal == NULL || text == NULL || *text == '\0')
		return;
	if (!termuni_terminal_find (terminal, text, backwards))
		window_show_toast (self, _("No matches"));
}

static void
termuni_window_find_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	TermuniWindow *self = user_data;
	const char *name = g_action_get_name (G_ACTION (action));
	(void) parameter;

	if (g_strcmp0 (name, "find") == 0)
	{
		gboolean open = !gtk_search_bar_get_search_mode (self->find_bar);
		gtk_widget_set_visible (GTK_WIDGET (self->find_bar), open);
		gtk_search_bar_set_search_mode (self->find_bar, open);
		if (!open)
			return;
		gtk_widget_grab_focus (GTK_WIDGET (self->find_entry));
		return;
	}
	window_find_text (self, g_strcmp0 (name, "find-previous") == 0);
}

/* The bar takes no space while closed, so the tab content starts right
 * below the tab strip. */
static void
find_close_clicked_cb (GtkButton *button, TermuniWindow *self)
{
	gtk_search_bar_set_search_mode (self->find_bar, FALSE);
}

static void
find_bar_mode_changed_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
	(void) pspec;
	TermuniWindow *self = user_data;
	if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (object))) {
		gtk_widget_set_visible (GTK_WIDGET (object), FALSE);
		for (int i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++) {
			AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
			termuni_terminal_set_highlight (TERMUNI_TERMINAL (adw_tab_page_get_child (page)), NULL);
		}
	}
}

static void
find_entry_activate_cb (GtkSearchEntry *entry,
                        gpointer        user_data)
{
	(void) entry;
	window_find_text (TERMUNI_WINDOW (user_data), FALSE);
}

static void
find_entry_search_changed_cb (GtkSearchEntry *entry,
                              gpointer        user_data)
{
	TermuniWindow *self = user_data;
	TermuniTerminal *terminal = window_selected_terminal (self);
	const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
	(void) entry;
	if (terminal != NULL) {
		termuni_terminal_set_highlight (terminal, gtk_search_bar_get_search_mode (self->find_bar) ? text : NULL);
		if (gtk_search_bar_get_search_mode (self->find_bar) && text != NULL && *text != '\0') termuni_terminal_find (terminal, text, FALSE);
	}
}

/* ---- attachments ------------------------------------------------------ */

static void
window_attach_stored (TermuniWindow *self,
                      TermuniTerminal *terminal,
                      const char     *stored_path)
{
	g_autofree char *inserted = termuni_agent_format_attachment (
		window_agent (self), stored_path);

	termuni_terminal_add_attachment (terminal, stored_path);
	termuni_terminal_insert_text (terminal, inserted);
	window_show_toast (self, _("Image attached to the prompt"));
}

static void
window_attach_file (TermuniWindow   *self,
                    TermuniTerminal *terminal,
                    const char     *path)
{
	g_autoptr(GError) error = NULL;
	g_autofree char *stored = termuni_attachments_import (termuni_terminal_get_session_id (terminal),
	                                                     path, &error);
	if (stored == NULL)
	{
		window_show_toast (self, error != NULL ? error->message : _("The image could not be attached"));
		return;
	}
	window_attach_stored (self, terminal, stored);
}

static void
attach_open_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
	TermuniWindow *self = user_data;
	g_autoptr(GFile) file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, NULL);
	g_autofree char *path = file != NULL ? g_file_get_path (file) : NULL;
	TermuniTerminal *terminal = window_selected_terminal (self);

	if (path != NULL && terminal != NULL)
		window_attach_file (self, terminal, path);
	g_object_unref (self);
}

static void
termuni_window_attach_image (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
	TermuniWindow *self = user_data;
	GtkFileDialog *dialog = gtk_file_dialog_new ();
	GtkFileFilter *filter = gtk_file_filter_new ();
	(void) action;
	(void) parameter;
	gtk_file_filter_set_name (filter, _("PNG and JPEG images"));
	gtk_file_filter_add_mime_type (filter, "image/png");
	gtk_file_filter_add_mime_type (filter, "image/jpeg");
	gtk_file_dialog_set_default_filter (dialog, filter);
	gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, attach_open_cb, g_object_ref (self));
	g_object_unref (filter);
	g_object_unref (dialog);
}

gboolean
termuni_window_attach_file_list (TermuniWindow *self, GdkFileList *list)
{
	TermuniTerminal *terminal = window_selected_terminal (self);
	GSList *iter;

	g_return_val_if_fail (TERMUNI_IS_WINDOW (self), FALSE);
	if (terminal == NULL || list == NULL)
		return FALSE;
	for (iter = gdk_file_list_get_files (list); iter != NULL; iter = iter->next)
	{
		g_autofree char *path = g_file_get_path (G_FILE (iter->data));
		if (path != NULL)
			window_attach_file (self, terminal, path);
	}
	return TRUE;
}

static gboolean
window_drop_files (GtkDropTarget *target,
                   const GValue  *value,
                   double         x,
                   double         y,
                   gpointer       user_data)
{
	TermuniWindow *self = user_data;
	(void) target;
	(void) x;
	(void) y;

	if (!G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
		return GDK_EVENT_PROPAGATE;
	return termuni_window_attach_file_list (self, (GdkFileList *) g_value_peek_pointer (value))
	       ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

/* ---- rename ------------------------------------------------------------ */

static void
rename_response_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page = g_object_get_data (source, "termuni-page");
	GtkEntry *entry = g_object_get_data (source, "termuni-entry");
	TermuniTerminal *terminal = window_terminal_for_page (page);
	const char *text;

	if (g_strcmp0 (adw_alert_dialog_choose_finish (ADW_ALERT_DIALOG (source), result), "close") == 0)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (entry));
		if (text == NULL || *text == '\0')
		{
			const char *auto_title = g_object_get_data (G_OBJECT (terminal), "auto-title");
			g_object_set_data_full (G_OBJECT (terminal), "manual-title", NULL, NULL);
			adw_tab_page_set_title (page, auto_title != NULL ? auto_title : adw_tab_page_get_title (page));
		}
		else
		{
			g_object_set_data_full (G_OBJECT (terminal), "manual-title", g_strdup (text), g_free);
			adw_tab_page_set_title (page, text);
		}
		termuni_window_update_header (self);
		window_request_save (self);
	}
	g_object_unref (self);
}

static void
termuni_window_rename_tab (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
	TermuniWindow *self = user_data;
	AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
	AdwDialog *dialog;
	GtkWidget *entry;
	(void) action;
	(void) parameter;
	if (page == NULL)
		return;
	dialog = window_confirm_dialog (_("Rename tab"),
	                                _("Leave empty to return to the automatic title."),
	                                _("Rename"));
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "close", ADW_RESPONSE_SUGGESTED);
	entry = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry), adw_tab_page_get_title (page));
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), entry);
	adw_alert_dialog_set_response_label (ADW_ALERT_DIALOG (dialog), "close", _("Rename"));
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
	g_object_set_data_full (G_OBJECT (dialog), "termuni-page", g_object_ref (page), g_object_unref);
	g_object_set_data (G_OBJECT (dialog), "termuni-entry", entry);
	termuni_alert_choose_window (ADW_ALERT_DIALOG (dialog), GTK_WINDOW (self),
	                         rename_response_cb, g_object_ref (self));
}

/* ---- window actions and lifecycle ------------------------------------ */

/* Terminal shortcuts are window actions, so they can be edited like the others.
 * They act on the terminal of the selected tab. */
static void
termuni_window_terminal_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
	TermuniWindow *self = user_data;
	TermuniTerminal *terminal = window_selected_terminal (self);
	const char *name = g_action_get_name (G_ACTION (action));
	(void) parameter;

	if (terminal == NULL)
		return;
	if (g_strcmp0 (name, "terminal-copy") == 0)
		termuni_terminal_copy_or_interrupt (terminal);
	else if (g_strcmp0 (name, "terminal-copy-selection") == 0)
		termuni_terminal_copy (terminal);
	else if (g_strcmp0 (name, "terminal-paste") == 0 ||
	         g_strcmp0 (name, "terminal-paste-clipboard") == 0)
		termuni_terminal_paste (terminal);
	else if (g_strcmp0 (name, "terminal-select-all") == 0)
		termuni_terminal_select_all (terminal);
	else
		termuni_terminal_line_start (terminal);
}

static const GActionEntry window_actions[] = {
	{ "new-tab", termuni_window_add_tab },
	{ "launch-agent", termuni_window_launch_agent_action },
	{ "launch-agent-id", termuni_window_launch_agent_id_action, "s" },
	{ "new-window", termuni_window_new_window },
	{ "toggle-overview", termuni_window_toggle_overview },
	{ "close-tab", termuni_window_close_tab },
	{ "fullscreen", termuni_window_toggle_fullscreen },
	{ "rename-tab", termuni_window_rename_tab },
	{ "move-tab-backward", termuni_window_move_tab },
	{ "move-tab-forward", termuni_window_move_tab },
	{ "move-tab-to-new-window", termuni_window_move_tab_to_new_window },
	{ "find", termuni_window_find_action },
	{ "find-next", termuni_window_find_action },
	{ "find-previous", termuni_window_find_action },
	{ "attach-image", termuni_window_attach_image },
	{ "terminal-copy", termuni_window_terminal_action },
	{ "terminal-copy-selection", termuni_window_terminal_action },
	{ "terminal-paste", termuni_window_terminal_action },
	{ "terminal-paste-clipboard", termuni_window_terminal_action },
	{ "terminal-line-start", termuni_window_terminal_action },
	{ "terminal-select-all", termuni_window_terminal_action },
};

static void termuni_window_constructed (GObject *object);

static void
termuni_window_class_init (TermuniWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = termuni_window_constructed;
	gtk_widget_class_set_template_from_resource (widget_class,
	                                            "/io/github/thiagoaciole/termuni/termuni-window.ui");
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, overview);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, tab_view);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, tab_bar);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, toast_overlay);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, overview_button);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, header_title);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, header_subtitle);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, menu_button);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, agents_button);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, find_bar);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, find_entry);
	gtk_widget_class_bind_template_child (widget_class, TermuniWindow, find_close_button);
}

static void
termuni_window_init (TermuniWindow *self)
{
	GtkDropTarget *drop;
	GMenu *tab_menu;

	gtk_widget_init_template (GTK_WIDGET (self));
	gtk_widget_add_css_class (GTK_WIDGET (self), "termuni-window");
	adw_tab_bar_set_view (self->tab_bar, self->tab_view);
	/* Match the reference: hide the tab strip until there are multiple tabs. */
	adw_tab_bar_set_autohide (self->tab_bar, TRUE);
	adw_tab_overview_set_view (self->overview, self->tab_view);
	g_signal_connect (self->overview, "create-tab",
	                  G_CALLBACK (termuni_window_create_tab_cb), self);
	g_signal_connect (self->tab_view, "close-page",
	                  G_CALLBACK (termuni_window_close_page_cb), self);
	g_signal_connect (self->tab_view, "notify::selected-page",
	                  G_CALLBACK (selected_page_changed_cb), self);
	g_signal_connect (self->tab_view, "notify::n-pages",
	                  G_CALLBACK (tab_count_changed_cb), self);
	g_signal_connect (self->tab_view, "setup-menu",
	                  G_CALLBACK (termuni_window_setup_menu_cb), self);
	g_signal_connect (self->find_close_button, "clicked", G_CALLBACK (find_close_clicked_cb), self);
	g_signal_connect (self->find_bar, "notify::search-mode-enabled",
	                  G_CALLBACK (find_bar_mode_changed_cb), self);
	g_signal_connect (self->find_entry, "search-changed",
	                  G_CALLBACK (find_entry_search_changed_cb), self);
	g_signal_connect (self->find_entry, "activate",
	                  G_CALLBACK (find_entry_activate_cb), self);
	g_signal_connect (self, "close-request", G_CALLBACK (termuni_window_close_request_cb), self);
	g_action_map_add_action_entries (G_ACTION_MAP (self), window_actions,
	                                 G_N_ELEMENTS (window_actions), self);
	tab_menu = g_menu_new ();
	g_menu_append (tab_menu, _("Rename Tab"), "win.rename-tab");
	g_menu_append (tab_menu, _("Move to New Window"), "win.move-tab-to-new-window");
	adw_tab_view_set_menu_model (self->tab_view, G_MENU_MODEL (tab_menu));
	g_object_unref (tab_menu);
	drop = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
	g_signal_connect (drop, "drop", G_CALLBACK (window_drop_files), self);
	gtk_widget_add_controller (GTK_WIDGET (self->tab_view), GTK_EVENT_CONTROLLER (drop));
	termuni_window_build_menu (self);
	termuni_window_build_agents_menu (self);
}

/* The first tab is created here, once the application is known, so that it
 * gets the same settings (shell integration, history) as the others. */
static void
termuni_window_constructed (GObject *object)
{
	TermuniWindow *self = TERMUNI_WINDOW (object);

	G_OBJECT_CLASS (termuni_window_parent_class)->constructed (object);
	termuni_window_create_tab (self, NULL, NULL);
	adw_tab_view_set_selected_page (self->tab_view, adw_tab_view_get_nth_page (self->tab_view, 0));
	termuni_window_update_header (self);
}

/* ---- public API ------------------------------------------------------- */

gboolean
termuni_window_shortcut_is_reserved (TermuniWindow   *self,
                                    guint           keyval,
                                    GdkModifierType state)
{
	static const char *const actions[] = {
		"win.new-tab", "win.new-window", "win.toggle-overview", "win.close-tab",
		"win.fullscreen", "win.rename-tab", "win.move-tab-backward", "win.move-tab-forward",
		"win.find", "win.find-next", "win.find-previous", "win.attach-image", "win.launch-agent",
		"win.terminal-copy", "win.terminal-copy-selection", "win.terminal-paste",
		"win.terminal-paste-clipboard", "win.terminal-line-start", "win.terminal-select-all",
		"app.preferences", "app.shortcuts", "app.quit",
		NULL,
	};
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
	GdkModifierType wanted = state & gtk_accelerator_get_default_mod_mask ();
	guint i;

	if (app == NULL)
		return FALSE;
	for (i = 0; actions[i] != NULL; i++)
	{
		char **accels = gtk_application_get_accels_for_action (app, actions[i]);
		char **accel;
		for (accel = accels; accel != NULL && *accel != NULL; accel++)
		{
			guint key = 0;
			GdkModifierType mods = 0;
			if (gtk_accelerator_parse (*accel, &key, &mods) &&
			    gdk_keyval_to_lower (key) == gdk_keyval_to_lower (keyval) &&
			    mods == wanted)
			{
				g_strfreev (accels);
				return TRUE;
			}
		}
		g_strfreev (accels);
	}
	return FALSE;
}

gboolean
termuni_window_show_session (TermuniWindow *self,
                            const char   *session_id)
{
	guint i;

	g_return_val_if_fail (TERMUNI_IS_WINDOW (self), FALSE);
	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
	{
		AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
		if (g_strcmp0 (termuni_terminal_get_session_id (window_terminal_for_page (page)), session_id) == 0)
		{
			adw_tab_view_set_selected_page (self->tab_view, page);
			gtk_window_present (GTK_WINDOW (self));
			return TRUE;
		}
	}
	return FALSE;
}

GVariant *
termuni_window_session_state (TermuniWindow *self,
                             gint         *selected)
{
	GVariantBuilder builder;
	AdwTabPage *current = adw_tab_view_get_selected_page (self->tab_view);
	guint i;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
	*selected = 0;
	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
	{
		AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
		TermuniTerminal *terminal = window_terminal_for_page (page);
		const char *cwd = termuni_terminal_get_cwd (terminal);
		const char *manual = g_object_get_data (G_OBJECT (terminal), "manual-title");
		if (page == current)
			*selected = (gint) i;
		g_variant_builder_add (&builder, "(ss)", cwd != NULL ? cwd : g_get_home_dir (),
		                       manual != NULL ? manual : "");
	}
	return g_variant_builder_end (&builder);
}

void
termuni_window_restore_tabs (TermuniWindow *self,
                            GVariant     *tabs,
                            gint          selected)
{
	GVariantIter iter;
	const char *cwd;
	const char *manual;
	AdwTabPage *first_default = adw_tab_view_get_nth_page (self->tab_view, 0);
	gint count = 0;

	g_return_if_fail (TERMUNI_IS_WINDOW (self));
	g_variant_iter_init (&iter, tabs);
	while (g_variant_iter_next (&iter, "(&s&s)", &cwd, &manual))
	{
		g_autofree char *directory = NULL;
		AdwTabPage *page;
		directory = termuni_session_directory (cwd);
		page = termuni_window_create_tab (self, directory, manual);
		if (count == selected)
			adw_tab_view_set_selected_page (self->tab_view, page);
		count++;
	}
	if (count > 0 && first_default != NULL)
		adw_tab_view_close_page (self->tab_view, first_default);
}

void
termuni_window_set_font_size (TermuniWindow *self, guint size)
{
	guint i;
	AdwTabPage *page;
	g_return_if_fail (TERMUNI_IS_WINDOW (self));
	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
	{
		page = adw_tab_view_get_nth_page (self->tab_view, i);
		termuni_terminal_set_font_size (TERMUNI_TERMINAL (adw_tab_page_get_child (page)), size);
	}
}

void
termuni_window_sync_settings (TermuniWindow *self)
{
	GSettings *settings;
	g_autofree char *scheme = NULL;
	g_autofree char *palette = NULL;
	g_autofree char *percentage = NULL;
	guint size;
	guint selected;
	guint lines;
	guint i;
	g_return_if_fail (TERMUNI_IS_WINDOW (self));
	settings = window_settings (self);
	if (settings == NULL)
		return;
	scheme = g_settings_get_string (settings, "color-scheme");
	selected = g_strcmp0 (scheme, "light") == 0 ? 0 :
	           g_strcmp0 (scheme, "dark") == 0 ? 1 : 2;
	gtk_toggle_button_set_active (self->theme_buttons[selected], TRUE);
	size = g_settings_get_uint (settings, "font-size");
	termuni_window_set_font_size (self, size);
	lines = g_settings_get_uint (settings, "scrollback-lines");
	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
	{
		TermuniTerminal *terminal = TERMUNI_TERMINAL (adw_tab_page_get_child (adw_tab_view_get_nth_page (self->tab_view, i)));
		termuni_terminal_set_scrollback_lines (terminal, lines);
		termuni_terminal_set_agent (terminal,
			window_agent (self));
	}
	palette = window_effective_palette (settings);
	window_apply_chrome (palette);
	termuni_window_set_palette (self, palette);
	percentage = g_strdup_printf ("%u%%", size * 100 / 11);
	gtk_label_set_text (self->zoom_label, percentage);
}

void
termuni_window_set_palette (TermuniWindow *self, const char *name)
{
	guint i;
	g_return_if_fail (TERMUNI_IS_WINDOW (self));
	for (i = 0; i < adw_tab_view_get_n_pages (self->tab_view); i++)
		termuni_terminal_set_palette (TERMUNI_TERMINAL (
			adw_tab_page_get_child (adw_tab_view_get_nth_page (self->tab_view, i))), name);
}
