/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Drives the real application under a virtual display: opens the window,
 * creates and closes tabs, opens Preferences, and saves the session. Runs with
 * isolated XDG directories and an in-memory GSettings backend. */
#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <gtk/gtk.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <gdk/x11/gdkx.h>
#include <adwaita.h>

#include "termuni-application.h"
#include "termuni-agent.h"
#include "termuni-terminal.h"
#include "termuni-vt.h"
#include "termuni-window.h"

static TermuniApplication *app;

static void
pump (guint ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		while (g_main_context_pending (NULL))
			g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}
}

static TermuniWindow *
main_window (void)
{
	GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
	GList *iter;

	for (iter = windows; iter != NULL; iter = iter->next)
		if (TERMUNI_IS_WINDOW (iter->data))
			return TERMUNI_WINDOW (iter->data);
	return NULL;
}

static guint
tab_count (TermuniWindow *window)
{
	gint selected = 0;
	GVariant *tabs = termuni_window_session_state (window, &selected);
	guint count = (guint) g_variant_n_children (tabs);

	g_variant_unref (tabs);
	return count;
}

static gboolean
wait_for_tab_count (TermuniWindow *window, guint expected, guint timeout_ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		if (tab_count (window) == expected)
			return TRUE;
		pump (50);
	}
	return tab_count (window) == expected;
}

/* Saves what GTK draws for a widget as a PNG, so the screen can be inspected
 * without a screenshot tool. Enabled with TERMUNI_UI_SCREENSHOTS=<directory>. */
static void
save_widget_png (GtkWidget *widget, const char *name)
{
	const char *dir = g_getenv ("TERMUNI_UI_SCREENSHOTS");
	g_autofree char *path = NULL;
	GdkPaintable *paintable;
	GtkSnapshot *snapshot;
	GskRenderNode *node;
	GskRenderer *renderer;
	GdkTexture *texture;
	int width;
	int height;

	if (dir == NULL)
		return;
	g_test_message ("Capturing %s", name);
	path = g_build_filename (dir, name, NULL);
	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	paintable = gtk_widget_paintable_new (widget);
	snapshot = gtk_snapshot_new ();
	gdk_paintable_snapshot (paintable, GDK_SNAPSHOT (snapshot), width, height);
	node = gtk_snapshot_free_to_node (snapshot);
	renderer = gtk_native_get_renderer (gtk_widget_get_native (widget));
	texture = gsk_renderer_render_texture (renderer, node, NULL);
	g_assert_nonnull (texture);
	g_assert_true (gdk_texture_save_to_png (texture, path));
	g_object_unref (texture);
	gsk_render_node_unref (node);
	g_object_unref (paintable);
}

static TermuniTerminal *
find_terminal (GtkWidget *widget)
{
	GtkWidget *child;

	if (TERMUNI_IS_TERMINAL (widget))
		return TERMUNI_TERMINAL (widget);
	for (child = gtk_widget_get_first_child (widget); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
	{
		TermuniTerminal *found = find_terminal (child);

		if (found != NULL)
			return found;
	}
	return NULL;
}

/* Real key events go through the X server (XTest), so the terminal receives
 * them the same way it does from a keyboard. libXtst is loaded at run time. */
typedef int (*XTestFakeKeyEventFn) (Display *display, unsigned int keycode, int is_press,
                                    unsigned long delay);
typedef int (*XTestFakeMotionEventFn) (Display *display, int screen, int x, int y,
                                       unsigned long delay);
typedef int (*XTestFakeButtonEventFn) (Display *display, unsigned int button, int is_press,
                                       unsigned long delay);

static XTestFakeKeyEventFn fake_key_event;
static XTestFakeMotionEventFn fake_motion_event;
static XTestFakeButtonEventFn fake_button_event;
static Display *x_display;

static void
xtest_load (void)
{
	void *library = dlopen ("libXtst.so.6", RTLD_NOW);

	g_assert_nonnull (library);
	fake_key_event = (XTestFakeKeyEventFn) dlsym (library, "XTestFakeKeyEvent");
	fake_motion_event = (XTestFakeMotionEventFn) dlsym (library, "XTestFakeMotionEvent");
	fake_button_event = (XTestFakeButtonEventFn) dlsym (library, "XTestFakeButtonEvent");
	g_assert_nonnull (fake_button_event);
	g_assert_nonnull (fake_key_event);
	g_assert_nonnull (fake_motion_event);
}

static void
send_keysym_full (KeySym sym, gboolean ctrl, gboolean shift)
{
	KeyCode code = XKeysymToKeycode (x_display, sym);
	KeyCode control = XKeysymToKeycode (x_display, XK_Control_L);
	KeyCode shift_key = XKeysymToKeycode (x_display, XK_Shift_L);

	g_assert_true (code != 0);
	if (ctrl)
		fake_key_event (x_display, control, 1, 0);
	if (shift)
		fake_key_event (x_display, shift_key, 1, 0);
	fake_key_event (x_display, code, 1, 0);
	fake_key_event (x_display, code, 0, 0);
	if (shift)
		fake_key_event (x_display, shift_key, 0, 0);
	if (ctrl)
		fake_key_event (x_display, control, 0, 0);
	XFlush (x_display);
}

static void
send_keysym (KeySym sym, gboolean ctrl)
{
	send_keysym_full (sym, ctrl, FALSE);
}

/* Moves the pointer with XTest device motion, to coordinates relative to the
 * surface, so the button events that follow come from the same device. */
static void
mouse_move (GtkWidget *window, int x, int y)
{
	GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
	Window xwindow = gdk_x11_surface_get_xid (surface);
	Window child = None;
	int root_x = 0;
	int root_y = 0;

	XTranslateCoordinates (x_display, xwindow, DefaultRootWindow (x_display), x * gtk_widget_get_scale_factor (window), y * gtk_widget_get_scale_factor (window),
	                       &root_x, &root_y, &child);
	fake_motion_event (x_display, -1, root_x, root_y, 0);
	XFlush (x_display);
	pump (150);
}

static void
mouse_button (guint button, gboolean press)
{
	fake_button_event (x_display, button, press, 0);
	XFlush (x_display);
	pump (150);
}

/* Window coordinates of a point inside the terminal widget. */
static void
terminal_point_in_window (GtkWidget *window, GtkWidget *terminal, int x, int y, int *out_x, int *out_y)
{
	graphene_point_t point = GRAPHENE_POINT_INIT ((float) x, (float) y);
	graphene_point_t result;

	g_assert_true (gtk_widget_compute_point (terminal, window, &point, &result));
	*out_x = (int) result.x;
	*out_y = (int) result.y;
}

static void
type_text (const char *text)
{
	const char *c;

	for (c = text; *c != '\0'; c++)
	{
		KeySym sym = (KeySym) (guchar) *c;
		KeyCode code = XKeysymToKeycode (x_display, sym);
		gboolean shift = code != 0 && XkbKeycodeToKeysym (x_display, code, 0, 0) != sym;

		send_keysym_full (sym, FALSE, shift);
	}
}

static void
focus_terminal (GtkWidget *window, GtkWidget *terminal)
{
	GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));

	gtk_window_present (GTK_WINDOW (window));
	gtk_widget_grab_focus (terminal);
	pump (300);
	XWarpPointer (x_display, None, gdk_x11_surface_get_xid (surface), 0, 0, 0, 0, 120 * gtk_widget_get_scale_factor (window), 120 * gtk_widget_get_scale_factor (window));
	XFlush (x_display);
	pump (300);
}

/* Text of every viewport row, joined with newlines. */
static char *
screen_text (TermuniTerminal *terminal)
{
	GhosttyTerminal vt = (GhosttyTerminal) termuni_terminal_get_vt_handle (terminal);
	guint columns = termuni_terminal_get_columns (terminal);
	guint rows = termuni_terminal_get_rows (terminal);
	GString *all = g_string_new (NULL);
	guint row;

	for (row = 0; row < rows; row++)
	{
		char *line = NULL;

		if (termuni_vt_viewport_row_text (vt, columns, row, &line) && line != NULL)
			g_string_append (all, line);
		g_free (line);
		g_string_append_c (all, '\n');
	}
	return g_string_free (all, FALSE);
}

static gboolean
screen_contains (TermuniTerminal *terminal, const char *needle)
{
	g_autofree char *text = screen_text (terminal);

	return strstr (text, needle) != NULL;
}

static gboolean
wait_for_screen (TermuniTerminal *terminal, const char *needle, guint timeout_ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		if (screen_contains (terminal, needle))
			return TRUE;
		pump (100);
	}
	return screen_contains (terminal, needle);
}

static gboolean
wait_for_alternate_screen (GhosttyTerminal vt, gboolean wanted, guint timeout_ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		if (termuni_vt_alternate_screen (vt) == wanted)
			return TRUE;
		pump (100);
	}
	return termuni_vt_alternate_screen (vt) == wanted;
}

static gboolean
wait_for_job_end (TermuniTerminal *terminal, guint timeout_ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		if (!termuni_terminal_has_foreground_job (terminal))
			return TRUE;
		pump (100);
	}
	return !termuni_terminal_has_foreground_job (terminal);
}

static gboolean
wait_for_job_start (TermuniTerminal *terminal, guint timeout_ms)
{
	gint64 end = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

	while (g_get_monotonic_time () < end)
	{
		if (termuni_terminal_has_foreground_job (terminal))
			return TRUE;
		pump (100);
	}
	return termuni_terminal_has_foreground_job (terminal);
}

static void
run_in_terminal (TermuniWindow *window, TermuniTerminal *terminal, const char *command)
{
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	type_text (command);
	send_keysym (XK_Return, FALSE);
}

static void
test_window_tabs (void)
{
	TermuniWindow *window;

	g_application_activate (G_APPLICATION (app));
	pump (1500);
	window = main_window ();
	g_assert_nonnull (window);
	g_assert_cmpuint (tab_count (window), ==, 1);

	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (800);
	g_assert_cmpuint (tab_count (window), ==, 2);

	g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
	pump (800);
	g_assert_cmpuint (tab_count (window), ==, 1);
	save_widget_png (GTK_WIDGET (window), "main-window.png");
}

static void
test_tab_churn (void)
{
	TermuniWindow *window = main_window ();
	const char *rounds_env = g_getenv ("TERMUNI_CHURN_ROUNDS");
	guint rounds = rounds_env != NULL ? (guint) g_ascii_strtoull (rounds_env, NULL, 10) : 10;
	guint i;

	g_assert_nonnull (window);
	for (i = 0; i < rounds; i++)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (300);
		g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
		pump (300);
	}
	g_assert_cmpuint (tab_count (window), ==, 1);
}

/* Each load/input scenario owns its shell, including when selected with -p. */
static TermuniWindow *
fresh_test_window (void)
{
	TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);

	gtk_window_present (GTK_WINDOW (window));
	pump (700);
	return window;
}

static void
test_typing_and_interrupt (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_autofree char *status = g_build_filename (g_get_user_state_dir (), "interrupt-status", NULL);
	g_autofree char *quoted = g_shell_quote (status);
	g_autofree char *command = g_strdup_printf ("printf '%%s' $? > %s; printf 'after-%%s\\n' interrupt", quoted);
	g_autofree char *actual = NULL;

	g_assert_nonnull (terminal);
	/* Neither expected result is present literally in the echoed input. */
	run_in_terminal (window, terminal, "printf 'termuni-%s\\n' executed");
	g_assert_true (wait_for_screen (terminal, "termuni-executed", 4000));
	save_widget_png (GTK_WIDGET (window), "typed.png");

	run_in_terminal (window, terminal, "sleep 30");
	pump (1000);
	g_assert_true (termuni_terminal_has_foreground_job (terminal));
	send_keysym (XK_c, TRUE);
	pump (200);
	g_assert_true (termuni_terminal_has_foreground_job (terminal));
	g_assert_false (g_file_test (status, G_FILE_TEST_EXISTS));

	send_keysym (XK_c, TRUE);
	g_assert_true (wait_for_job_end (terminal, 5000));
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_screen (terminal, "after-interrupt", 4000));
	g_assert_true (g_file_get_contents (status, &actual, NULL, NULL));
	g_assert_cmpstr (actual, ==, "130");
	g_test_message ("sleep interrupted with status 130; subsequent command executed");
	save_widget_png (GTK_WIDGET (window), "interrupted.png");
	g_unlink (status);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* Finds the first widget of a GType in the tree below root. */
static GtkWidget *
find_widget_of_type (GtkWidget *root, GType type)
{
	GtkWidget *child;

	if (g_type_is_a (G_OBJECT_TYPE (root), type))
		return root;
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
	{
		GtkWidget *found = find_widget_of_type (child, type);

		if (found != NULL)
			return found;
	}
	return NULL;
}

static GtkButton *
find_button_for_action (GtkWidget *root, const char *action)
{
	GtkWidget *child;

	if (GTK_IS_BUTTON (root) && g_strcmp0 (g_object_get_data (G_OBJECT (root), "shortcut-action"), action) == 0)
		return GTK_BUTTON (root);
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
	{
		GtkButton *found = find_button_for_action (child, action);

		if (found != NULL)
			return found;
	}
	return NULL;
}

static GtkButton *
find_button_labelled (GtkWidget *root, const char *label)
{
	GtkWidget *child;

	if (GTK_IS_BUTTON (root) && g_strcmp0 (gtk_button_get_label (GTK_BUTTON (root)), label) == 0)
		return GTK_BUTTON (root);
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
	{
		GtkButton *found = find_button_labelled (child, label);

		if (found != NULL)
			return found;
	}
	return NULL;
}

static GtkWindow *
find_preferences_window (void)
{
	GList *toplevels = gtk_window_list_toplevels ();
	GList *iter;
	GtkWindow *found = NULL;

	for (iter = toplevels; iter != NULL; iter = iter->next)
		if (GTK_IS_WINDOW (iter->data) &&
		    g_strcmp0 (gtk_window_get_title (GTK_WINDOW (iter->data)), "Preferences") == 0)
			found = GTK_WINDOW (iter->data);
	g_list_free (toplevels);
	return found;
}

static AdwDialog *
find_window_dialog (GtkWindow *parent, GType type)
{
	GList *windows = gtk_window_list_toplevels ();
	AdwDialog *found = NULL;
	if (ADW_IS_APPLICATION_WINDOW (parent))
		g_assert_null (adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (parent)));
	else if (ADW_IS_WINDOW (parent))
		g_assert_null (adw_window_get_visible_dialog (ADW_WINDOW (parent)));
	for (GList *iter = windows; iter; iter = iter->next)
	{
		GtkWindow *window = iter->data;
		if (gtk_window_get_transient_for (window) == parent)
		{
			GtkWidget *dialog = find_widget_of_type (GTK_WIDGET (window), type);
			if (dialog != NULL)
			{
				g_assert_true (gtk_window_get_modal (window));
				g_assert_true (gtk_widget_get_root (dialog) != GTK_ROOT (parent));
				found = ADW_DIALOG (dialog);
				break;
			}
		}
	}
	g_list_free (windows);
	return found;
}

static gsize
attachment_count (void)
{
	g_autofree char *root = g_build_filename (g_get_user_cache_dir (), "termuni", "attachments", NULL);
	GDir *sessions = g_dir_open (root, 0, NULL);
	const char *session;
	gsize count = 0;

	if (sessions == NULL)
		return 0;
	while ((session = g_dir_read_name (sessions)) != NULL)
	{
		g_autofree char *dir = g_build_filename (root, session, NULL);
		GDir *files = g_dir_open (dir, 0, NULL);

		if (files == NULL)
			continue;
		while (g_dir_read_name (files) != NULL)
			count++;
		g_dir_close (files);
	}
	g_dir_close (sessions);
	return count;
}

/* Writes a small PNG and returns its path; the caller frees it. */
static char *
write_test_png (const char *name)
{
	guchar pixels[4 * 16 * 16];
	g_autoptr (GBytes) bytes = NULL;
	g_autoptr (GdkTexture) texture = NULL;
	char *path = g_build_filename (g_get_user_state_dir (), name, NULL);
	guint i;

	for (i = 0; i < G_N_ELEMENTS (pixels); i += 4)
	{
		pixels[i] = 200;
		pixels[i + 1] = 40;
		pixels[i + 2] = 40;
		pixels[i + 3] = 255;
	}
	bytes = g_bytes_new (pixels, sizeof pixels);
	texture = gdk_memory_texture_new (16, 16, GDK_MEMORY_R8G8B8A8, bytes, 16 * 4);
	g_assert_true (gdk_texture_save_to_png (texture, path));
	return path;
}

static void
test_drop_image (void)
{
	TermuniWindow *window = fresh_test_window ();
	g_autofree char *png = write_test_png ("termuni-drop.png");
	GSList *files = NULL;
	GdkFileList *list;
	gsize before;

	g_assert_nonnull (window);
	before = attachment_count ();
	files = g_slist_append (files, g_file_new_for_path (png));
	list = gdk_file_list_new_from_list (files);
	g_assert_true (termuni_window_attach_file_list (window, list));
	g_boxed_free (GDK_TYPE_FILE_LIST, list);
	pump (1500);
	g_assert_cmpuint (attachment_count (), ==, before + 1);
	save_widget_png (GTK_WIDGET (window), "drop-image.png");
	g_unlink (png);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

static void
test_paste_image_clipboard (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_autofree char *png = write_test_png ("termuni-clipboard.png");
	g_autoptr (GdkTexture) texture = gdk_texture_new_from_filename (png, NULL);
	GdkClipboard *clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
	gsize before;

	g_assert_nonnull (terminal);
	g_assert_nonnull (texture);
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	before = attachment_count ();
	gdk_clipboard_set_texture (clipboard, texture);
	pump (300);
	send_keysym (XK_v, TRUE);
	pump (2000);
	g_assert_cmpuint (attachment_count (), ==, before + 1);
	save_widget_png (GTK_WIDGET (window), "paste-image.png");
	g_unlink (png);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

typedef struct
{
	gboolean done;
	char *text;
} ClipboardRead;

static void
clipboard_read_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
	ClipboardRead *read = user_data;

	read->text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source), result, NULL);
	read->done = TRUE;
}

/* Reads the clipboard text, waiting up to three seconds for the answer. */
static char *
read_clipboard_text (void)
{
	GdkClipboard *clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
	ClipboardRead read = { FALSE, NULL };
	gint64 end = g_get_monotonic_time () + 3 * G_USEC_PER_SEC;

	gdk_clipboard_read_text_async (clipboard, NULL, clipboard_read_done, &read);
	while (!read.done && g_get_monotonic_time () < end)
		pump (20);
	return read.text;
}

/* The selection gesture is the same one the mouse drives. The pointer does not
 * reach GTK under the virtual X server, so the press, drag and release are fed
 * to the selector directly; the copy itself uses the keyboard shortcut. */
static void
test_mouse_selection_copy (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	GhosttyTerminal vt;
	TermuniVtSelector *selector;
	GhosttySelectionGestureGeometry geometry = { 0 };
	g_autofree char *text = NULL;

	g_assert_nonnull (terminal);
	run_in_terminal (window, terminal, "printf 'selection-%s\\n' complete");
	g_assert_true (wait_for_screen (terminal, "selection-complete", 3000));
	vt = (GhosttyTerminal) termuni_terminal_get_vt_handle (terminal);
	selector = termuni_vt_selector_new ();
	g_assert_nonnull (selector);
	guint cell_width, cell_height;
	guint columns = termuni_terminal_get_columns (terminal);
	guint rows = termuni_terminal_get_rows (terminal);
	termuni_terminal_get_cell_size (terminal, &cell_width, &cell_height);
	geometry.columns = columns;
	geometry.cell_width = cell_width;
	geometry.padding_left = 8;
	geometry.screen_height = gtk_widget_get_height (GTK_WIDGET (terminal));

	termuni_vt_selector_event (selector, vt, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS,
	                          0, 0, 8, 12, 1000, NULL);
	termuni_vt_selector_event (selector, vt, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG,
	                          columns - 1, rows - 1, 8 + (columns - 1) * cell_width, 12 + (rows - 1) * cell_height, 2000, &geometry);
	termuni_vt_selector_event (selector, vt, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE,
	                          columns - 1, rows - 1, 8 + (columns - 1) * cell_width, 12 + (rows - 1) * cell_height, 3000, &geometry);
	termuni_vt_selector_free (selector);

	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	send_keysym_full (XK_C, TRUE, TRUE);
	pump (500);
	save_widget_png (GTK_WIDGET (window), "mouse-selection.png");
	text = read_clipboard_text ();
	g_assert_nonnull (text);
	g_assert_nonnull (strstr (text, "selection-complete"));
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* Compares accelerators by their parsed key and modifiers, so the order and case
 * GTK uses for the same combination do not matter. */
static void
assert_accel_is (const char *actual, const char *expected)
{
	guint actual_key = 0;
	guint expected_key = 0;
	GdkModifierType actual_mods = 0;
	GdkModifierType expected_mods = 0;

	g_assert_nonnull (actual);
	g_assert_true (gtk_accelerator_parse (actual, &actual_key, &actual_mods));
	g_assert_true (gtk_accelerator_parse (expected, &expected_key, &expected_mods));
	g_assert_cmpuint (gdk_keyval_to_lower (actual_key), ==, gdk_keyval_to_lower (expected_key));
	g_assert_cmpuint (actual_mods, ==, expected_mods);
}

static void capture_shortcut (GtkWindow *preferences, guint key, GdkModifierType mods);

static void
test_shortcut_editor (void)
{
	TermuniWindow *window = main_window ();
	GtkWindow *preferences;
	GtkButton *button;
	GtkButton *reset;
	GtkWidget *stack;
	AdwDialog *capture_dialog;
	g_auto (GStrv) accels = NULL;

	(void) window;
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (800);
	preferences = find_preferences_window ();
	g_assert_nonnull (preferences);
	stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
	g_assert_nonnull (stack);
	adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "shortcuts");
	pump (300);

	button = find_button_for_action (GTK_WIDGET (preferences), "win.new-tab");
	g_assert_nonnull (button);
	g_signal_emit_by_name (button, "clicked");
	pump (800);
	capture_dialog = find_window_dialog (preferences, ADW_TYPE_DIALOG);
	g_assert_nonnull (capture_dialog);
	capture_shortcut (preferences, GDK_KEY_y, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
	pump (800);

	accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.new-tab");
	assert_accel_is (accels[0], "<Control><Shift>y");

	reset = find_button_labelled (GTK_WIDGET (preferences), "Restore defaults");
	g_assert_nonnull (reset);
	g_signal_emit_by_name (reset, "clicked");
	pump (300);
	g_strfreev (accels);
	accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.new-tab");
	assert_accel_is (accels[0], "<Control><Shift>t");
	gtk_window_close (preferences);
	pump (300);
}

static guint
child_process_count (void)
{
	g_autofree char *path = g_strdup_printf ("/proc/%d/task", (int) getpid ());
	GDir *tasks = g_dir_open (path, 0, NULL);
	const char *task;
	guint count = 0;

	if (tasks == NULL)
		return 0;
	while ((task = g_dir_read_name (tasks)) != NULL)
	{
		g_autofree char *file = g_strdup_printf ("%s/%s/children", path, task);
		g_autofree char *contents = NULL;
		gchar **pids;
		guint i;

		if (!g_file_get_contents (file, &contents, NULL, NULL))
			continue;
		pids = g_strsplit (contents, " ", -1);
		for (i = 0; pids[i] != NULL; i++)
			if (pids[i][0] != '\0')
				count++;
		g_strfreev (pids);
	}
	g_dir_close (tasks);
	return count;
}

static gsize
resident_memory_kb (void)
{
	g_autofree char *contents = NULL;
	char *line;

	g_file_get_contents ("/proc/self/status", &contents, NULL, NULL);
	line = contents != NULL ? strstr (contents, "VmRSS:") : NULL;
	return line != NULL ? (gsize) g_ascii_strtoull (line + 6, NULL, 10) : 0;
}

/* tee records every line produced by the real seq; the viewport and a second
 * command establish that the PTY drained and the shell accepts further input. */
static void
test_large_output (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_autofree char *output = g_build_filename (g_get_user_state_dir (), "seq-output", NULL);
	g_autofree char *quoted = g_shell_quote (output);
	g_autofree char *command = g_strdup_printf (
	    "set -o pipefail; seq 1 30000 | tee %s; printf 'load-%%s:%%s\\n' complete $?", quoted);
	g_autofree char *actual = NULL;
	GString *expected = g_string_new (NULL);

	g_assert_nonnull (terminal);
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_screen (terminal, "load-complete:0", 30000));
	g_assert_true (wait_for_job_end (terminal, 3000));
	g_assert_true (screen_contains (terminal, "29999\n30000\n"));
	g_assert_true (g_file_get_contents (output, &actual, NULL, NULL));
	for (guint i = 1; i <= 30000; i++)
		g_string_append_printf (expected, "%u\n", i);
	g_assert_cmpstr (actual, ==, expected->str);
	g_test_message ("seq 1 30000: exact %u lines / %zu bytes; pipeline status 0 and PTY tail verified",
	                30000, expected->len);
	g_string_free (expected, TRUE);
	run_in_terminal (window, terminal, "printf 'after-%s\\n' output");
	g_assert_true (wait_for_screen (terminal, "after-output", 4000));
	g_assert_true (wait_for_job_end (terminal, 3000));
	save_widget_png (GTK_WIDGET (window), "large-output.png");
	g_unlink (output);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* A full-screen program (less) uses the alternate screen and gives it back on exit. */
static void
test_tui_alternate_screen (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	GhosttyTerminal vt;

	g_assert_nonnull (terminal);
	vt = (GhosttyTerminal) termuni_terminal_get_vt_handle (terminal);
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	type_text ("less /etc/services");
	send_keysym (XK_Return, FALSE);
	pump (1200);
	g_assert_true (termuni_vt_alternate_screen (vt));
	save_widget_png (GTK_WIDGET (window), "tui-less.png");
	send_keysym (XK_q, FALSE);
	pump (1000);
	g_assert_false (termuni_vt_alternate_screen (vt));
	g_assert_false (termuni_terminal_has_foreground_job (terminal));
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* A program that turns on mouse tracking receives press and release as SGR
 * reports; Shift bypasses the report and selects text instead. */
static void
test_mouse_tracking_report (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	GhosttyTerminal vt;
	g_autofree char *script = g_build_filename (g_get_user_state_dir (), "termuni-mouse-test.py", NULL);
	g_autofree char *command = g_strdup_printf ("python3 %s", script);
	KeyCode shift_key;
	int x0;
	int y0;
	int x1;
	int y1;
	const char *body =
		"import sys,tty,os,select,termios\n"
		"sys.stdout.write('\\x1b[2J\\x1b[H')\n"
		"for i in range(12): sys.stdout.write('SELECTME-' * 6 + '\\r\\n')\n"
		"saved = termios.tcgetattr(0)\n"
		"tty.setraw(0)\n"
		"sys.stdout.write('\\x1b[?1000h\\x1b[?1006h'); sys.stdout.flush()\n"
		"buf = b''\n"
		"while select.select([0], [], [], 5.0)[0]:\n"
		"    buf += os.read(0, 64)\n"
		"    if buf.count(b'\\x1b[<') >= 2: break\n"
		"termios.tcsetattr(0, termios.TCSADRAIN, saved)\n"
		"sys.stdout.write('\\x1b[?1000l\\x1b[?1006l' + 'GOT:' + repr(buf) + '\\r\\n'); sys.stdout.flush()\n";

	g_assert_nonnull (terminal);
	vt = (GhosttyTerminal) termuni_terminal_get_vt_handle (terminal);
	g_assert_true (g_file_set_contents (script, body, -1, NULL));
	shift_key = XKeysymToKeycode (x_display, XK_Shift_L);

	/* Plain click: reported to the program. */
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_screen (terminal, "SELECTME-", 4000));
	pump (400);
	g_assert_true (termuni_vt_mouse_tracking (vt));
	terminal_point_in_window (GTK_WIDGET (window), GTK_WIDGET (terminal), 40, 60, &x0, &y0);
	mouse_move (GTK_WIDGET (window), x0, y0);
	mouse_button (1, TRUE);
	mouse_button (1, FALSE);
	g_assert_true (wait_for_screen (terminal, "GOT:b'\\x1b[<0;", 4000));
	g_assert_true (wait_for_job_end (terminal, 3000));

	/* Shift+drag: no report, a selection appears. */
	run_in_terminal (window, terminal, command);
	{
		gint64 deadline = g_get_monotonic_time () + 4 * G_USEC_PER_SEC;
		while (!termuni_vt_mouse_tracking (vt) && g_get_monotonic_time () < deadline) pump (20);
	}
	g_assert_true (termuni_vt_mouse_tracking (vt));
	terminal_point_in_window (GTK_WIDGET (window), GTK_WIDGET (terminal), 30, 40, &x0, &y0);
	terminal_point_in_window (GTK_WIDGET (window), GTK_WIDGET (terminal), 300, 80, &x1, &y1);
	mouse_move (GTK_WIDGET (window), x0, y0);
	fake_key_event (x_display, shift_key, 1, 0);
	mouse_button (1, TRUE);
	mouse_move (GTK_WIDGET (window), x1, y1);
	mouse_button (1, FALSE);
	fake_key_event (x_display, shift_key, 0, 0);
	XFlush (x_display);
	g_assert_true (termuni_terminal_copy (terminal));
	g_assert_true (wait_for_screen (terminal, "GOT:b''", 8000));
	g_remove (script);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* Dead keys go through the input method; the composed text is what reaches the
 * program. Ctrl+Shift+U hex entry is not covered: it needs a real input-method
 * engine, which the virtual display does not have. */
static void
test_dead_key_and_unicode (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));

	g_assert_nonnull (terminal);
	/* The virtual keyboard has no dead keys: bind two spare keycodes to them. */
	{
		KeySym acute = XK_dead_acute;
		KeySym tilde = XK_dead_tilde;

		if (XKeysymToKeycode (x_display, acute) == 0)
			XChangeKeyboardMapping (x_display, 250, 1, &acute, 1);
		if (XKeysymToKeycode (x_display, tilde) == 0)
			XChangeKeyboardMapping (x_display, 251, 1, &tilde, 1);
		XFlush (x_display);
		pump (500);
		g_assert_true (XKeysymToKeycode (x_display, acute) != 0);
		g_assert_true (XKeysymToKeycode (x_display, tilde) != 0);
	}
	run_in_terminal (window, terminal, "cat");
	pump (600);
	/* Text being composed is drawn at the cursor and never sent to the program.
	 * (The simple input context on the virtual display reports no preedit for
	 * dead keys, so the display path is driven directly.) */
	termuni_terminal_set_preedit (terminal, "\xc2\xb4");
	pump (300);
	g_assert_cmpstr (termuni_terminal_get_preedit (terminal), ==, "\xc2\xb4");
	g_assert_false (screen_contains (terminal, "\xc2\xb4"));
	save_widget_png (GTK_WIDGET (window), "preedit.png");
	termuni_terminal_set_preedit (terminal, NULL);
	g_assert_cmpstr (termuni_terminal_get_preedit (terminal), ==, "");
	send_keysym (XK_dead_acute, FALSE);
	send_keysym (XK_a, FALSE);
	pump (400);
	g_assert_cmpstr (termuni_terminal_get_preedit (terminal), ==, "");
	g_assert_true (wait_for_screen (terminal, "\xc3\xa1", 3000));
	send_keysym (XK_dead_tilde, FALSE);
	send_keysym (XK_n, FALSE);
	pump (400);
	g_assert_true (wait_for_screen (terminal, "\xc3\xb1", 3000));
	send_keysym (XK_Return, FALSE);
	send_keysym (XK_d, TRUE);
	if (!wait_for_job_end (terminal, 5000))
	{
		g_autofree char *dump = screen_text (terminal);
		g_printerr ("SCREEN:\n%s\n", dump);
	}
	g_assert_true (wait_for_job_end (terminal, 100));
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

static void
set_clipboard_and_paste (TermuniTerminal *terminal, const char *text)
{
	gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (terminal)), text);
	pump (300);
	termuni_terminal_paste (terminal);
}

/* Pasted text arrives intact: line breaks survive and nothing is dropped, even
 * far above the size of the input queue. */
static void
test_paste_integrity (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_autofree char *path = g_build_filename (g_get_user_state_dir (), "termuni-paste-out.txt", NULL);
	g_autofree char *command = g_strdup_printf ("cat > %s", path);
	g_autofree char *contents = NULL;
	GString *big = g_string_new (NULL);
	guint i;

	g_assert_nonnull (terminal);
	g_remove (path);

	run_in_terminal (window, terminal, command);
	pump (500);
	set_clipboard_and_paste (terminal, "one\ntwo\nthree");
	pump (500);
	send_keysym (XK_Return, FALSE);
	send_keysym (XK_d, TRUE);
	g_assert_true (wait_for_job_end (terminal, 4000));
	g_assert_true (g_file_get_contents (path, &contents, NULL, NULL));
	g_assert_cmpstr (contents, ==, "one\ntwo\nthree\n");

	/* 26214 lines of 80 bytes: about 2 MiB, past the 1 MiB write queue. */
	for (i = 0; i < 26214; i++)
		g_string_append (big, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
	g_assert_cmpuint (big->len, ==, 2097120);
	run_in_terminal (window, terminal, "wc -c");
	pump (500);
	set_clipboard_and_paste (terminal, big->str);
	pump (500);
	send_keysym (XK_d, TRUE);
	g_assert_true (wait_for_screen (terminal, "2097120", 30000));
	g_string_free (big, TRUE);
	g_remove (path);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

static TermuniTerminal *
terminal_of_page (TermuniWindow *window, int index)
{
	AdwTabView *view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));
	AdwTabPage *page = adw_tab_view_get_nth_page (view, index);

	g_assert_nonnull (page);
	return find_terminal (adw_tab_page_get_child (page));
}

/* Two tabs are two sessions: environment, folder and typed input do not leak. */
static void
test_sessions_are_isolated (void)
{
	TermuniWindow *window = main_window ();
	TermuniTerminal *first;
	TermuniTerminal *second;

	g_assert_nonnull (window);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (1200);
	g_assert_cmpuint (tab_count (window), ==, 2);
	first = terminal_of_page (window, 0);
	second = terminal_of_page (window, 1);
	g_assert_true (first != second);
	g_assert_cmpstr (termuni_terminal_get_session_id (first), !=, termuni_terminal_get_session_id (second));

	run_in_terminal (window, second, "export TERMUNI_ISOLATION=two; cd /usr; echo \"[$TERMUNI_ISOLATION] $(pwd)\"");
	g_assert_true (wait_for_screen (second, "[two] /usr", 4000));

	{
		AdwTabView *view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));

		adw_tab_view_set_selected_page (view, adw_tab_view_get_nth_page (view, 0));
		pump (500);
	}
	run_in_terminal (window, first, "echo \"[$TERMUNI_ISOLATION] $(pwd)\"");
	g_assert_true (wait_for_screen (first, "[] ", 4000));
	g_assert_false (screen_contains (first, "[two]"));
	g_assert_false (screen_contains (first, "/usr"));

	/* Text typed in the second tab never shows in the first. */
	{
		AdwTabView *view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));

		adw_tab_view_set_selected_page (view, adw_tab_view_get_nth_page (view, 1));
		pump (500);
	}
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (second));
	type_text ("isolated-input");
	g_assert_true (wait_for_screen (second, "isolated-input", 3000));
	g_assert_false (screen_contains (first, "isolated-input"));
	send_keysym (XK_u, TRUE);
	g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
	pump (1200);
	g_assert_cmpuint (tab_count (window), ==, 1);
}

static void
destroy_all_windows (void)
{
	/* Destroying a window removes it from the application's list, so iterate over a copy. */
	GList *windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (app)));
	GList *iter;

	for (iter = windows; iter != NULL; iter = iter->next)
		if (TERMUNI_IS_WINDOW (iter->data))
			gtk_window_destroy (GTK_WINDOW (iter->data));
	g_list_free (windows);
	pump (300);
}

static char *
session_file (void)
{
	return g_build_filename (g_get_user_state_dir (), "termuni", "session-v3.gvariant", NULL);
}

/* Removes both session files, so the next activation opens one fresh tab. */
static void
forget_saved_sessions (void)
{
	g_autofree char *v3 = session_file ();
	g_autofree char *v2 = g_build_filename (g_get_user_state_dir (), "termuni", "session-v2.gvariant", NULL);

	g_unlink (v3);
	g_unlink (v2);
}

static void
report_cwd (TermuniWindow *window, TermuniTerminal *terminal, const char *dir)
{
	g_autofree char *command = g_strdup_printf ("printf '\\033]7;file://%s\\a'", dir);
	gint64 start = g_get_monotonic_time ();

	run_in_terminal (window, terminal, command);
	while (g_strcmp0 (termuni_terminal_get_cwd (terminal), dir) != 0)
	{
		pump (100);
		if (g_get_monotonic_time () - start > 5000000)
			break;
	}
	g_assert_cmpstr (termuni_terminal_get_cwd (terminal), ==, dir);
}

/* Saves three tabs, reopens the application and gets the same tabs back. */
static void
test_session_restore_roundtrip (void)
{
	TermuniWindow *window = main_window ();
	AdwTabView *view;
	g_autofree char *path = session_file ();
	g_autofree char *saved = NULL;
	gsize saved_length = 0;
	g_autoptr (GVariant) before = NULL;
	g_autoptr (GVariant) after = NULL;
	gint selected_before = 0;
	gint selected_after = 0;
	TermuniTerminal *second;
	TermuniTerminal *third;

	/* Start from one fresh tab whatever an earlier test left behind. */
	destroy_all_windows ();
	forget_saved_sessions ();
	g_application_activate (G_APPLICATION (app));
	pump (800);
	window = TERMUNI_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
	g_assert_true (TERMUNI_IS_WINDOW (window));
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (800);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (800);
	g_assert_cmpuint (tab_count (window), ==, 3);
	view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));
	second = terminal_of_page (window, 1);
	third = terminal_of_page (window, 2);
	report_cwd (window, third, "/tmp");
	adw_tab_view_set_selected_page (view, adw_tab_view_get_nth_page (view, 1));
	pump (400);
	report_cwd (window, second, "/usr");
	g_object_set_data_full (G_OBJECT (second), "manual-title", g_strdup ("Build"), g_free);

	before = g_variant_ref_sink (termuni_window_session_state (window, &selected_before));
	g_assert_cmpint (selected_before, ==, 1);
	termuni_application_save_session_now (app);
	g_assert_true (g_file_get_contents (path, &saved, &saved_length, NULL));

	destroy_all_windows ();
	/* Closing the windows saves again; put back the file from before. */
	g_assert_true (g_file_set_contents (path, saved, (gssize) saved_length, NULL));
	g_application_activate (G_APPLICATION (app));
	pump (1500);
	window = TERMUNI_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
	g_assert_true (TERMUNI_IS_WINDOW (window));
	g_assert_cmpuint (tab_count (window), ==, 3);
	after = g_variant_ref_sink (termuni_window_session_state (window, &selected_after));
	g_assert_cmpint (selected_after, ==, selected_before);
	g_assert_true (g_variant_equal (before, after));

	/* Leave a plain single-tab window for the tests that follow. */
	destroy_all_windows ();
	forget_saved_sessions ();
	g_application_activate (G_APPLICATION (app));
	pump (800);
	g_assert_cmpuint (tab_count (TERMUNI_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)))), ==, 1);
}

/* A damaged session file never prevents startup: one fresh tab appears. */
static void
test_session_corrupt_file (void)
{
	g_autofree char *path = session_file ();
	const char *bodies[] = { "garbage", "", "\x01\x02\x03\x04\x05\x06\x07\x08\x09" };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (bodies); i++)
	{
		TermuniWindow *window;

		destroy_all_windows ();
		g_assert_cmpint (g_mkdir_with_parents (g_path_get_dirname (path), 0700), ==, 0);
		g_assert_true (g_file_set_contents (path, bodies[i], (gssize) (i == 2 ? 9 : strlen (bodies[i])), NULL));
		g_application_activate (G_APPLICATION (app));
		pump (800);
		window = TERMUNI_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
		g_assert_true (TERMUNI_IS_WINDOW (window));
		g_assert_cmpuint (tab_count (window), ==, 1);
	}
	destroy_all_windows ();
	forget_saved_sessions ();
	g_application_activate (G_APPLICATION (app));
	pump (800);
}

static void collect_terminals (GtkWidget *root, GPtrArray *out);

/* vi is a real full-screen program: edit and save through the PTY, follow a
 * window resize, and leave nothing running when its tab is closed. */
static void
test_vim_session (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	GhosttyTerminal vt;
	GSettings *settings = termuni_application_get_settings (app);
	g_autofree char *path = g_build_filename (g_get_user_state_dir (), "termuni-vim-test.txt", NULL);
	g_autofree char *command = NULL;
	g_autofree char *contents = NULL;
	guint rows_before;
	guint rows_after;
	guint children;
	g_autofree char *expected = NULL;
	g_autofree char *quoted_dir = g_shell_quote (g_get_user_state_dir ());

	g_assert_nonnull (terminal);
	vt = (GhosttyTerminal) termuni_terminal_get_vt_handle (terminal);
	g_remove (path);

	/* Edit and save. */
	/* A long temporary prefix otherwise makes vi show a hit-enter prompt. */
	command = g_strdup_printf ("cd %s && vi -u NONE -n termuni-vim-test.txt", quoted_dir);
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_alternate_screen (vt, TRUE, 4000));
	type_text ("ihello-vim");
	send_keysym (XK_Escape, FALSE);
	type_text (":wq");
	send_keysym (XK_Return, FALSE);
	g_assert_true (wait_for_alternate_screen (vt, FALSE, 4000));
	g_assert_true (g_file_get_contents (path, &contents, NULL, NULL));
	g_assert_cmpstr (contents, ==, "hello-vim\n");
	g_assert_true (wait_for_job_end (terminal, 3000));

	/* The PTY follows a window resize while vi runs. */
	rows_before = termuni_terminal_get_rows (terminal);
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_alternate_screen (vt, TRUE, 4000));
	/* The virtualized HiDPI renderer can take longer to expose the second
	 * vi frame even after the alternate screen has already been entered. */
	if (!wait_for_screen (terminal, "hello-vim", 10000))
	{
		g_autofree char *visible = screen_text (terminal);
		g_test_message ("Vim screen timed out (%ux%u, alternate=%d):\n%s",
		                termuni_terminal_get_columns (terminal),
		                termuni_terminal_get_rows (terminal),
		                termuni_vt_alternate_screen (vt), visible);
		g_assert_not_reached ();
	}
	gtk_window_set_default_size (GTK_WINDOW (window), 900, 420);
	pump (1200);
	rows_after = termuni_terminal_get_rows (terminal);
	g_assert_cmpuint (rows_after, !=, rows_before);
	type_text (":q");
	send_keysym (XK_Return, FALSE);
	g_assert_true (wait_for_alternate_screen (vt, FALSE, 4000));
	run_in_terminal (window, terminal, "stty size");
	expected = g_strdup_printf ("%u %u", rows_after, termuni_terminal_get_columns (terminal));
	g_assert_true (wait_for_screen (terminal, expected, 3000));

	/* Closing a tab that runs vi leaves no process behind. */
	g_settings_set_boolean (settings, "confirm-close", FALSE);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (1000);
	children = child_process_count ();
	terminal = find_terminal (GTK_WIDGET (window));
	{
		GPtrArray *all = g_ptr_array_new ();

		collect_terminals (GTK_WIDGET (window), all);
		terminal = g_ptr_array_index (all, all->len - 1);
		g_ptr_array_unref (all);
	}
	run_in_terminal (window, terminal, command);
	pump (1500);
	g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
	pump (2500);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_cmpuint (child_process_count (), <=, children - 1);
	g_settings_set_boolean (settings, "confirm-close", TRUE);
	g_remove (path);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* Each tab is its own session with its own shell. */
static void
test_tabs_are_independent (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *first = find_terminal (GTK_WIDGET (window));
	const char *first_id;
	guint before;
	guint count;

	g_assert_nonnull (first);
	/* Drain reapers from preceding tests before taking a global PID baseline. */
	pump (1200);
	before = child_process_count ();
	first_id = termuni_terminal_get_session_id (first);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (1200);
	count = tab_count (window);
	g_assert_cmpuint (count, ==, 2);
	g_assert_cmpuint (child_process_count (), >, before);
	g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
	pump (800);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_nonnull (first_id);
	gtk_window_destroy (GTK_WINDOW (window));
	pump (1200);
}

/* A closed tab must release its terminal, which stops the shell it owns. */
static void
test_closed_tab_releases_terminal (void)
{
	TermuniWindow *window = main_window ();
	GtkWidget *tab_view;
	AdwTabPage *page;
	TermuniTerminal *terminal;
	gint pages_before;

	g_assert_nonnull (window);
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (800);
	tab_view = find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW);
	g_assert_nonnull (tab_view);
	pages_before = adw_tab_view_get_n_pages (ADW_TAB_VIEW (tab_view));
	page = adw_tab_view_get_selected_page (ADW_TAB_VIEW (tab_view));
	terminal = TERMUNI_TERMINAL (adw_tab_page_get_child (page));
	g_assert_nonnull (terminal);
	g_object_add_weak_pointer (G_OBJECT (terminal), (gpointer *) &terminal);
	g_object_add_weak_pointer (G_OBJECT (page), (gpointer *) &page);
	g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
	pump (1000);
	g_assert_cmpint (adw_tab_view_get_n_pages (ADW_TAB_VIEW (tab_view)), ==, pages_before - 1);
	g_assert_null (terminal);
}

/* Closing a tab must leave no shell behind. */
static void
test_no_orphan_shells (void)
{
	TermuniWindow *window = main_window ();
	guint before;
	guint i;

	g_assert_nonnull (window);
	for (i = 0; i < 3; i++)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (600);
	}
	pump (800);
	before = child_process_count ();
	for (i = 0; i < 3; i++)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
		pump (800);
	}
	pump (1000);
	g_assert_cmpuint (child_process_count (), <, before);
	g_assert_cmpuint (tab_count (window), ==, 1);
}

/* Memory must not grow with repeated tab churn; the threshold catches a leak
 * that repeats per tab, not normal allocator noise. */
static void
test_memory_stable_under_churn (void)
{
	TermuniWindow *window = main_window ();
	gsize before;
	gsize after;
	guint i;

	g_assert_nonnull (window);
	for (i = 0; i < 5; i++)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (250);
		g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
		pump (250);
	}
	before = resident_memory_kb ();
	for (i = 0; i < 40; i++)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (250);
		g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
		pump (250);
	}
	after = resident_memory_kb ();
	g_test_message ("resident memory before=%zu KB after=%zu KB", before, after);
	g_assert_cmpuint (after, <, before + 20 * 1024);
}

/* The saved session keeps the window size in the v3 file. */
static void
test_geometry_saved (void)
{
	TermuniWindow *window = main_window ();
	g_autofree char *path = g_build_filename (g_get_user_state_dir (), "termuni",
	                                          "session-v3.gvariant", NULL);
	g_autofree char *contents = NULL;
	gsize length = 0;
	GVariant *sessions;
	gint selected;
	gint width;
	gint height;
	gboolean maximized;
	GVariant *tabs;

	g_assert_nonnull (window);
	gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);
	pump (300);
	termuni_application_save_session_now (app);
	g_assert_true (g_file_get_contents (path, &contents, &length, NULL));
	sessions = g_variant_new_from_data (G_VARIANT_TYPE ("a(iiiba(ss))"), contents, length, TRUE, NULL, NULL);
	g_assert_nonnull (sessions);
	g_variant_get_child (sessions, 0, "(iiib@a(ss))", &selected, &width, &height, &maximized, &tabs);
	g_assert_cmpint (width, >, 0);
	g_assert_cmpint (height, >, 0);
	g_variant_unref (tabs);
	g_variant_unref (sessions);
}

static void
test_preferences_window (void)
{
	GList *before = gtk_window_list_toplevels ();
	guint count_before = g_list_length (before);
	GList *after;
	GList *iter;
	GtkWindow *preferences = NULL;

	g_list_free (before);
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (800);
	after = gtk_window_list_toplevels ();
	g_assert_cmpuint (g_list_length (after), >, count_before);
	for (iter = after; iter != NULL; iter = iter->next)
		if (GTK_IS_WINDOW (iter->data) && !TERMUNI_IS_WINDOW (iter->data))
			preferences = GTK_WINDOW (iter->data);
	g_assert_nonnull (preferences);
	pump (300);
	save_widget_png (GTK_WIDGET (preferences), "preferences.png");
	gtk_window_close (preferences);
	g_list_free (after);
	pump (300);
}

static void
test_session_saved (void)
{
	g_autofree char *path = g_build_filename (g_get_user_state_dir (), "termuni",
	                                          "session-v3.gvariant", NULL);

	termuni_application_save_session_now (app);
	g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));
}

/* Runs the button's clicked handlers, as a real click would. */
static void
click_button (GtkWidget *button)
{
	g_assert_nonnull (button);
	g_signal_emit_by_name (button, "clicked");
}

/* Collects every button under root that carries the given object data key. */
static void
collect_buttons_with_data (GtkWidget *root, const char *key, GPtrArray *out)
{
	GtkWidget *child;

	if (GTK_IS_BUTTON (root) && g_object_get_data (G_OBJECT (root), key) != NULL)
		g_ptr_array_add (out, root);
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
		collect_buttons_with_data (child, key, out);
}

static void
collect_terminals (GtkWidget *root, GPtrArray *out)
{
	GtkWidget *child;

	if (TERMUNI_IS_TERMINAL (root))
		g_ptr_array_add (out, root);
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
		collect_terminals (child, out);
}

static gboolean
label_has_text (GtkWidget *root, const char *text)
{
	GtkWidget *child;

	if (GTK_IS_LABEL (root) && g_strcmp0 (gtk_label_get_text (GTK_LABEL (root)), text) == 0)
		return TRUE;
	for (child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
		if (label_has_text (child, text))
			return TRUE;
	return FALSE;
}

/* Every open tab, selected or not, must report the expected font size. */
static void
assert_all_terminals_font_size (TermuniWindow *window, guint expected)
{
	GPtrArray *terminals = g_ptr_array_new ();
	guint i;

	collect_terminals (GTK_WIDGET (window), terminals);
	g_assert_cmpuint (terminals->len, ==, tab_count (window));
	for (i = 0; i < terminals->len; i++)
		g_assert_cmpuint (termuni_terminal_get_font_size (g_ptr_array_index (terminals, i)), ==, expected);
	g_ptr_array_unref (terminals);
}

/* Changes theme and zoom with three tabs open; every tab must follow the change. */
static void
test_theme_zoom_with_tabs (void)
{
	TermuniWindow *window = main_window ();
	AdwStyleManager *style = adw_style_manager_get_default ();
	GPtrArray *buttons = g_ptr_array_new ();
	GtkWidget *terminal;
	g_autofree char *zoom_text = NULL;
	guint size_before;
	guint i;

	g_assert_nonnull (window);
	while (tab_count (window) < 3)
	{
		g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
		pump (400);
	}

	collect_buttons_with_data (GTK_WIDGET (window), "color-scheme", buttons);
	g_assert_cmpuint (buttons->len, >=, 3);
	for (i = 0; i < buttons->len; i++)
	{
		GtkWidget *button = g_ptr_array_index (buttons, i);
		const char *scheme = g_object_get_data (G_OBJECT (button), "color-scheme");
		AdwColorScheme expected = ADW_COLOR_SCHEME_DEFAULT;

		if (g_strcmp0 (scheme, "light") == 0)
			expected = ADW_COLOR_SCHEME_FORCE_LIGHT;
		else if (g_strcmp0 (scheme, "dark") == 0)
			expected = ADW_COLOR_SCHEME_FORCE_DARK;
		click_button (button);
		pump (300);
		g_assert_cmpint (adw_style_manager_get_color_scheme (style), ==, expected);
	}
	g_ptr_array_unref (buttons);
	save_widget_png (GTK_WIDGET (window), "theme-dark-tabs.png");

	terminal = GTK_WIDGET (find_terminal (GTK_WIDGET (window)));
	size_before = termuni_terminal_get_font_size (TERMUNI_TERMINAL (terminal));
	buttons = g_ptr_array_new ();
	collect_buttons_with_data (GTK_WIDGET (window), "zoom-delta", buttons);
	g_assert_cmpuint (buttons->len, ==, 2);
	for (i = 0; i < 4; i++)
	{
		guint j;

		/* Zoom in three times and out once: net change of +2. */
		for (j = 0; j < buttons->len; j++)
		{
			GtkWidget *button = g_ptr_array_index (buttons, j);
			int delta = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "zoom-delta"));

			if ((i < 3 && delta > 0) || (i == 3 && delta < 0))
			{
				click_button (button);
				pump (300);
			}
		}
	}
	g_ptr_array_unref (buttons);
	g_assert_cmpuint (size_before, >, 0);
	assert_all_terminals_font_size (window, size_before + 2);
	zoom_text = g_strdup_printf ("%u%%", (size_before + 2) * 100 / 11);
	g_assert_true (label_has_text (GTK_WIDGET (window), zoom_text));
	g_assert_cmpuint (tab_count (window), ==, 3);
	save_widget_png (GTK_WIDGET (window), "theme-zoom-tabs.png");
}

/* Adds, removes one and clears attachments through the real Preferences page. */
static void
test_attachment_sequence (void)
{
	TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	GSettings *settings = termuni_application_get_settings (app);
	g_autofree char *first = write_test_png ("termuni-seq-1.png");
	g_autofree char *second = write_test_png ("termuni-seq-2.png");
	GSList *files = NULL;
	GdkFileList *list;
	GPtrArray *buttons = g_ptr_array_new ();
	GtkWindow *preferences;
	GtkWidget *stack;
	gsize before;
	guint i;

	g_assert_nonnull (window);
	before = attachment_count ();
	gtk_window_present (GTK_WINDOW (window));
	pump (500);
	for (guint a = 0; a < 2; a++)
	{
		const TermuniAgent *agent = termuni_agent_find (a == 0 ? "claude" : "codex");
		g_autofree char *capture = g_build_filename (g_get_user_state_dir (), "agent-attachment-input.txt", NULL);
		g_autofree char *quoted_capture = g_shell_quote (capture);
		g_autofree char *command = g_strconcat ("/bin/cat > ", quoted_capture, NULL);
		g_autofree char *actual = NULL;
		g_autofree char *formatted = NULL;
		g_autofree char *expected = NULL;
		GPtrArray *attachments;
		g_settings_set_string (settings, "agent", agent->id);
		pump (100);
		run_in_terminal (window, terminal, command);
		pump (200);
		files = g_slist_append (NULL, g_file_new_for_path (a == 0 ? first : second));
		list = gdk_file_list_new_from_list (files);
		g_assert_true (termuni_window_attach_file_list (window, list));
		g_boxed_free (GDK_TYPE_FILE_LIST, list);
		g_slist_free_full (files, g_object_unref);
		pump (200);
		attachments = termuni_terminal_get_attachments (terminal);
		g_assert_cmpuint (attachments->len, ==, a + 1);
		formatted = termuni_agent_format_attachment (agent, g_ptr_array_index (attachments, a));
		expected = g_strconcat (formatted, "\n", NULL);
		send_keysym (XK_Return, FALSE);
		send_keysym (XK_d, TRUE);
		pump (300);
		g_assert_true (g_file_get_contents (capture, &actual, NULL, NULL));
		g_assert_cmpstr (actual, ==, expected);
		g_unlink (capture);
	}
	g_settings_reset (settings, "agent");
	g_assert_cmpuint (attachment_count (), ==, before + 2);

	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (800);
	preferences = find_preferences_window ();
	g_assert_nonnull (preferences);

	stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
	g_assert_nonnull (stack);
	adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "attachments");
	pump (300);
	g_assert_cmpstr (adw_view_stack_get_visible_child_name (ADW_VIEW_STACK (stack)), ==, "attachments");

	/* The list shows a copy and a remove button per attachment; only "Remove" deletes. */
	collect_buttons_with_data (GTK_WIDGET (preferences), "termuni-path", buttons);
	g_assert_cmpuint (buttons->len, ==, 2 * (before + 2));
	for (i = 0; i < buttons->len; i++)
	{
		GtkWidget *button = g_ptr_array_index (buttons, i);

		if (g_strcmp0 (gtk_widget_get_tooltip_text (button), "Remove") == 0)
		{
			click_button (button);
			break;
		}
	}
	g_ptr_array_unref (buttons);
	pump (500);
	g_assert_cmpuint (attachment_count (), ==, before + 1);

	click_button (GTK_WIDGET (find_button_labelled (GTK_WIDGET (preferences), "Clear All")));
	pump (500);
	g_assert_cmpuint (attachment_count (), ==, 0);
	g_assert_true (label_has_text (GTK_WIDGET (preferences), "No attachments"));
	save_widget_png (GTK_WIDGET (preferences), "attachments-cleared.png");

	gtk_window_close (preferences);
	pump (300);
	g_unlink (first);
	g_unlink (second);
	gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_attach_without_agent_binary (void)
{
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_setenv ("PATH", "/nonexistent", TRUE);
	test_attachment_sequence ();
	g_setenv ("PATH", previous != NULL ? previous : "", TRUE);
}

static GtkButton *
find_shortcut_button (GtkWidget *root, const char *action)
{
	GPtrArray *buttons = g_ptr_array_new ();
	GtkButton *found = NULL;
	guint i;

	collect_buttons_with_data (root, "shortcut-action", buttons);
	for (i = 0; i < buttons->len; i++)
	{
		GtkWidget *button = g_ptr_array_index (buttons, i);

		if (g_strcmp0 (g_object_get_data (G_OBJECT (button), "shortcut-action"), action) == 0)
		{
			found = GTK_BUTTON (button);
			break;
		}
	}
	g_ptr_array_unref (buttons);
	return found;
}

/* Rebinds the terminal copy action to Ctrl+Shift+Y. Ctrl+C then no longer copies or
 * waits for a second press: one press reaches the running program. */
static void
test_terminal_shortcut_rebind (void)
{
	TermuniWindow *window;
	TermuniTerminal *terminal;
	GtkWindow *preferences;
	GtkWidget *stack;
	GtkButton *button;
	GtkButton *reset;
	AdwDialog *capture_dialog;
	g_auto (GStrv) accels = NULL;

	if (main_window () == NULL)
	{
		g_application_activate (G_APPLICATION (app));
		pump (1500);
	}
	window = main_window ();
	g_assert_nonnull (window);
	terminal = find_terminal (GTK_WIDGET (window));
	g_assert_nonnull (terminal);
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (800);
	preferences = find_preferences_window ();
	g_assert_nonnull (preferences);
	stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
	adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "shortcuts");
	pump (300);

	button = find_shortcut_button (GTK_WIDGET (preferences), "win.terminal-copy");
	g_assert_nonnull (button);
	click_button (GTK_WIDGET (button));
	pump (800);
	capture_dialog = find_window_dialog (preferences, ADW_TYPE_DIALOG);
	g_assert_nonnull (capture_dialog);
	capture_shortcut (preferences, GDK_KEY_y, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
	pump (800);
	accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.terminal-copy");
	assert_accel_is (accels[0], "<Control><Shift>y");
	gtk_window_close (preferences);
	pump (300);

	/* Restores the default binding. */
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (800);
	preferences = find_preferences_window ();
	stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
	adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "shortcuts");
	pump (300);
	reset = find_button_labelled (GTK_WIDGET (preferences), "Restore defaults");
	g_assert_nonnull (reset);
	click_button (GTK_WIDGET (reset));
	pump (300);
	g_strfreev (accels);
	accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.terminal-copy");
	assert_accel_is (accels[0], "<Control>c");
	gtk_window_close (preferences);
	pump (300);
}

/* Ctrl+A selects the whole screen through the window action, so a copy then has text. */
static void
test_select_all_shortcut (void)
{
	TermuniWindow *window = fresh_test_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_auto (GStrv) accels = NULL;

	g_assert_nonnull (terminal);
	accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.terminal-select-all");
	assert_accel_is (accels[0], "<Control>a");
	focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
	type_text ("echo select-all-marker");
	send_keysym (XK_Return, FALSE);
	pump (800);
	send_keysym (XK_a, TRUE);
	pump (300);
	g_assert_true (termuni_terminal_copy (terminal));
	/* Typing ends the selection: a later copy has nothing to take. */
	send_keysym (XK_x, FALSE);
	pump (300);
	g_assert_false (termuni_terminal_copy (terminal));
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

/* Ctrl+Shift+C and Ctrl+Shift+V are editable actions, with their usual keys by default. */
static void
test_clipboard_shortcuts_default (void)
{
	g_auto (GStrv) copy = NULL;
	g_auto (GStrv) paste = NULL;

	copy = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.terminal-copy-selection");
	paste = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.terminal-paste-clipboard");
	assert_accel_is (copy[0], "<Control><Shift>c");
	assert_accel_is (paste[0], "<Control><Shift>v");
}

/* A v2 session from the previous version is converted once to v3 and keeps its tabs.
 * Runs last: it closes the windows of the shared application, then activates it again. */
static void
test_session_v2_migration (void)
{
	g_autofree char *dir = g_build_filename (g_get_user_state_dir (), "termuni", NULL);
	g_autofree char *v2 = g_build_filename (dir, "session-v2.gvariant", NULL);
	g_autofree char *v3 = g_build_filename (dir, "session-v3.gvariant", NULL);
	GVariantBuilder tabs;
	GVariantBuilder sessions;
	g_autoptr (GVariant) legacy = NULL;
	g_autoptr (GBytes) data = NULL;
	GList *windows;
	GList *iter;
	TermuniWindow *window;
	g_autofree char *source = NULL;
	g_autofree char *saved_v3 = NULL;
	gsize source_length = 0, saved_length = 0;
	g_autoptr(GVariant) before = NULL;
	g_autoptr(GVariant) after = NULL;
	gint selected_before = 0, selected_after = 0;

	/* Destroying a window removes it from the application's list, so iterate over a copy. */
	windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (app)));
	for (iter = windows; iter != NULL; iter = iter->next)
		if (TERMUNI_IS_WINDOW (iter->data))
			gtk_window_destroy (GTK_WINDOW (iter->data));
	g_list_free (windows);
	pump (300);
	g_assert_cmpint (g_mkdir_with_parents (dir, 0700), ==, 0);
	g_unlink (v3);
	g_variant_builder_init (&tabs, G_VARIANT_TYPE ("a(ss)"));
	g_variant_builder_add (&tabs, "(ss)", "/tmp", "Build");
	g_variant_builder_add (&tabs, "(ss)", "/usr", "");
	g_variant_builder_init (&sessions, G_VARIANT_TYPE ("a(ia(ss))"));
	g_variant_builder_add (&sessions, "(i@a(ss))", 1, g_variant_builder_end (&tabs));
	legacy = g_variant_ref_sink (g_variant_builder_end (&sessions));
	data = g_variant_get_data_as_bytes (legacy);
	g_assert_true (g_file_set_contents (v2, g_bytes_get_data (data, NULL),
	                                    (gssize) g_bytes_get_size (data), NULL));

	g_application_activate (G_APPLICATION (app));
	pump (300);

	g_assert_true (g_file_test (v3, G_FILE_TEST_EXISTS));
	g_assert_true (g_file_test (v2, G_FILE_TEST_EXISTS));
	window = TERMUNI_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
	g_assert_true (TERMUNI_IS_WINDOW (window));
	g_assert_cmpuint (tab_count (window), ==, 2);
	g_assert_true (g_file_get_contents (v2, &source, &source_length, NULL));
	g_assert_cmpmem (source, source_length, g_bytes_get_data (data, NULL), g_bytes_get_size (data));
	before = g_variant_ref_sink (termuni_window_session_state (window, &selected_before));
	g_assert_true (g_file_get_contents (v3, &saved_v3, &saved_length, NULL));

	/* Once v3 exists, stale (even invalid) v2 must not overwrite it. */
	destroy_all_windows ();
	g_assert_true (g_file_set_contents (v3, saved_v3, (gssize) saved_length, NULL));
	g_assert_true (g_file_set_contents (v2, "do not migrate again", -1, NULL));
	g_application_activate (G_APPLICATION (app));
	pump (300);
	window = main_window ();
	g_assert_nonnull (window);
	after = g_variant_ref_sink (termuni_window_session_state (window, &selected_after));
	g_assert_cmpint (selected_before, ==, selected_after);
	g_assert_true (g_variant_equal (before, after));
	g_clear_pointer (&source, g_free);
	g_assert_true (g_file_get_contents (v2, &source, NULL, NULL));
	g_assert_cmpstr (source, ==, "do not migrate again");
}

static void
collect_widgets_of_type (GtkWidget *root, GType type, GPtrArray *out)
{
	if (g_type_is_a (G_OBJECT_TYPE (root), type))
		g_ptr_array_add (out, root);
	for (GtkWidget *child = gtk_widget_get_first_child (root); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
		collect_widgets_of_type (child, type, out);
}

static gboolean
has_label_text (GtkWidget *widget)
{
	if (GTK_IS_LABEL (widget) && gtk_widget_get_visible (widget) &&
	    *gtk_label_get_text (GTK_LABEL (widget)) != '\0')
		return TRUE;
	for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
	     child = gtk_widget_get_next_sibling (child))
		if (has_label_text (child))
			return TRUE;
	return FALSE;
}

static void
audit_button_names (GtkWidget *root)
{
	g_autoptr(GPtrArray) buttons = g_ptr_array_new ();
	collect_widgets_of_type (root, GTK_TYPE_BUTTON, buttons);
	collect_widgets_of_type (root, GTK_TYPE_MENU_BUTTON, buttons);
	for (guint i = 0; i < buttons->len; i++)
	{
		GtkWidget *widget = g_ptr_array_index (buttons, i);
		/* GtkMenuButton exposes the name; its private toggle is implementation detail. */
		if (!gtk_widget_get_visible (widget) || GTK_IS_MENU_BUTTON (gtk_widget_get_parent (widget)))
			continue;
		/* Spin button's private +/- controls belong to its accessible spin role. */
		gboolean private_spin_control = FALSE;
		for (GtkWidget *parent = gtk_widget_get_parent (widget); parent != NULL; parent = gtk_widget_get_parent (parent))
			if (GTK_IS_SPIN_BUTTON (parent)) private_spin_control = TRUE;
		if (private_spin_control) continue;
		const char *tooltip = gtk_widget_get_tooltip_text (widget);
		gboolean named = has_label_text (widget) || (tooltip != NULL && *tooltip != '\0');
		if (!named && gtk_test_accessible_has_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_LABEL))
		{
			g_autofree char *difference = gtk_test_accessible_check_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_LABEL, "");
			named = difference != NULL;
		}
		if (!named)
		{
			for (GtkWidget *parent = widget; parent != NULL; parent = gtk_widget_get_parent (parent))
				g_test_message ("ancestor: %s", G_OBJECT_TYPE_NAME (parent));
		}
		if (!named)
			g_test_message ("Unlabelled %s icon=%s", G_OBJECT_TYPE_NAME (widget),
			 GTK_IS_BUTTON (widget) ? gtk_button_get_icon_name (GTK_BUTTON (widget)) : gtk_menu_button_get_icon_name (GTK_MENU_BUTTON (widget)));
		g_assert_true (named);
	}
}

static void
test_accessibility_labels (void)
{
	g_application_activate (G_APPLICATION (app));
	pump (400);
	TermuniWindow *window = main_window ();
	audit_button_names (GTK_WIDGET (window));
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (terminal)), ==, GTK_ACCESSIBLE_ROLE_TERMINAL);
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (400);
	GtkWindow *preferences = find_preferences_window ();
	g_assert_nonnull (preferences);
	audit_button_names (GTK_WIDGET (preferences));
	gtk_window_destroy (preferences);
}

static void
assert_grid_fits (TermuniTerminal *terminal)
{
	guint width, height;
	termuni_terminal_get_cell_size (terminal, &width, &height);
	g_assert_cmpuint (termuni_terminal_get_columns (terminal) * width, <=, gtk_widget_get_width (GTK_WIDGET (terminal)) - 8);
	g_assert_cmpuint (termuni_terminal_get_rows (terminal) * height, <=, gtk_widget_get_height (GTK_WIDGET (terminal)) - 12);
}

static void
test_grid_fits_widget (void)
{
	g_application_activate (G_APPLICATION (app));
	pump (500);
	TermuniWindow *window = main_window ();
	TermuniTerminal *terminal = find_terminal (GTK_WIDGET (window));
	g_assert_cmpint (gtk_widget_get_scale_factor (GTK_WIDGET (terminal)), ==, g_getenv ("GDK_SCALE") != NULL ? MAX (atoi (g_getenv ("GDK_SCALE")), 1) : 1);
	assert_grid_fits (terminal);
	guint font_before = termuni_terminal_get_font_size (terminal);
	guint columns = termuni_terminal_get_columns (terminal);
	guint rows = termuni_terminal_get_rows (terminal);
	g_autoptr(GPtrArray) buttons = g_ptr_array_new ();
	collect_buttons_with_data (GTK_WIDGET (window), "zoom-delta", buttons);
	for (guint i = 0; i < buttons->len; i++)
		if (GPOINTER_TO_INT (g_object_get_data (g_ptr_array_index (buttons, i), "zoom-delta")) == 1)
			g_signal_emit_by_name (g_ptr_array_index (buttons, i), "clicked");
	pump (500);
	assert_grid_fits (terminal);
	g_assert_cmpuint (termuni_terminal_get_columns (terminal), <, columns);
	g_assert_cmpuint (termuni_terminal_get_rows (terminal), <, rows);
	g_settings_set_uint (termuni_application_get_settings (app), "font-size", font_before);
}

static AdwComboRow *
find_agent_row (GtkWidget *root)
{
	if (ADW_IS_COMBO_ROW (root) && g_strcmp0 (adw_preferences_row_get_title (ADW_PREFERENCES_ROW (root)), "Agent") == 0)
		return ADW_COMBO_ROW (root);
	for (GtkWidget *child = gtk_widget_get_first_child (root); child != NULL; child = gtk_widget_get_next_sibling (child))
	{
		AdwComboRow *row = find_agent_row (child);
		if (row != NULL) return row;
	}
	return NULL;
}

static void
test_agent_choice (void)
{
	GtkWindow *preferences;
	AdwComboRow *row;
	GSettings *settings = termuni_application_get_settings (app);
	g_autofree char *choice = NULL;
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (400);
	preferences = find_preferences_window ();
	g_assert_nonnull (preferences);
	row = find_agent_row (GTK_WIDGET (preferences));
	g_assert_nonnull (row);
	adw_combo_row_set_selected (row, 1);
	choice = g_settings_get_string (settings, "agent");
	g_assert_cmpstr (choice, ==, "codex");
	g_assert_cmpstr (termuni_application_get_agent (app)->id, ==, "codex");
	g_settings_set_string (settings, "agent", "bogus");
	g_assert_true (termuni_application_get_agent (app) == termuni_agent_default ());
	pump (100);
	g_assert_cmpuint (adw_combo_row_get_selected (row), ==, 0);
	g_settings_reset (settings, "agent");
	gtk_window_destroy (preferences);
}

static TermuniTerminal *
selected_terminal (TermuniWindow *window)
{
	AdwTabView *view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));
	return TERMUNI_TERMINAL (adw_tab_page_get_child (adw_tab_view_get_selected_page (view)));
}

static void
test_launch_agent (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-ui-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (dir, "claude", NULL);
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	TermuniWindow *window;
	g_assert_true (g_file_set_contents (path, "#!/bin/sh\nprintf 'AGENT-%s\\n' STARTED\nexec /bin/sleep 30\n", -1, NULL));
	g_assert_cmpint (g_chmod (path, 0755), ==, 0);
	g_setenv ("PATH", dir, TRUE);
	g_setenv ("SHELL", "/bin/sh", TRUE);
	g_settings_reset (termuni_application_get_settings (app), "agent");
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	pump (200);
	g_assert_true (g_action_group_has_action (G_ACTION_GROUP (window), "launch-agent"));
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent", NULL);
	pump (300);
	g_assert_cmpuint (tab_count (window), ==, 2);
	g_assert_true (wait_for_screen (selected_terminal (window), "AGENT-STARTED", 5000));
	gtk_window_destroy (GTK_WINDOW (window));
	g_setenv ("PATH", previous != NULL ? previous : "", TRUE);
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_unlink (path);
	g_rmdir (dir);
}

static void
test_launch_agent_quick_menu (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-quick-XXXXXX", NULL);
	g_autofree char *path = g_build_filename (dir, "codex", NULL);
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	TermuniWindow *window;
	GtkMenuButton *button;
	GMenuModel *model;
	g_assert_true (g_file_set_contents (path, "#!/bin/sh\nprintf 'CODEX-%s\\n' QUICK\nexec /bin/sleep 30\n", -1, NULL));
	g_assert_cmpint (g_chmod (path, 0755), ==, 0);
	g_setenv ("PATH", dir, TRUE);
	g_setenv ("SHELL", "/bin/sh", TRUE);
	g_settings_reset (termuni_application_get_settings (app), "agent");
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	pump (200);
	button = GTK_MENU_BUTTON (gtk_widget_get_template_child (GTK_WIDGET (window), TERMUNI_TYPE_WINDOW, "agents_button"));
	model = gtk_menu_button_get_menu_model (button);
	g_assert_nonnull (model);
	g_assert_cmpint (g_menu_model_get_n_items (model), ==, 2);
	/* With an idle selected shell, the quick menu reuses it. */
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent-id", g_variant_new_string ("codex"));
	pump (300);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_true (wait_for_screen (selected_terminal (window), "CODEX-QUICK", 5000));
	/* With several tabs, an idle selected shell is still reused. */
	g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
	pump (300);
	g_assert_cmpuint (tab_count (window), ==, 2);
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent-id", g_variant_new_string ("codex"));
	pump (300);
	g_assert_cmpuint (tab_count (window), ==, 2);
	g_assert_true (wait_for_screen (selected_terminal (window), "CODEX-QUICK", 5000));
	/* The running agent owns the foreground, so the next launch gets a new tab. */
	g_assert_true (wait_for_job_start (selected_terminal (window), 3000));
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent-id", g_variant_new_string ("codex"));
	g_assert_true (wait_for_tab_count (window, 3, 5000));
	g_assert_true (wait_for_screen (selected_terminal (window), "CODEX-QUICK", 5000));
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent-id", g_variant_new_string ("bogus"));
	pump (100);
	g_assert_cmpuint (tab_count (window), ==, 3);
	g_assert_true (label_has_text (GTK_WIDGET (window), "Unknown agent"));
	gtk_window_destroy (GTK_WINDOW (window));
	g_setenv ("PATH", previous != NULL ? previous : "", TRUE);
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_unlink (path);
	g_rmdir (dir);
}

static void
test_launch_agent_missing (void)
{
	g_autofree char *previous = g_strdup (g_getenv ("PATH"));
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	g_setenv ("PATH", "/nonexistent", TRUE);
	g_setenv ("SHELL", "/bin/sh", TRUE);
	g_assert_true (g_action_group_has_action (G_ACTION_GROUP (window), "launch-agent"));
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent", NULL);
	pump (400);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_true (label_has_text (GTK_WIDGET (window), "Claude Code is not installed"));
	g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
	pump (200);
	{
		GtkWindow *preferences = find_preferences_window ();
		AdwComboRow *row = find_agent_row (GTK_WIDGET (preferences));
		g_assert_cmpstr (adw_action_row_get_subtitle (ADW_ACTION_ROW (row)), ==, "Not installed");
		g_assert_true (g_str_has_suffix (gtk_string_list_get_string (GTK_STRING_LIST (adw_combo_row_get_model (row)), 0), "Not installed"));
		gtk_window_destroy (preferences);
	}
	g_setenv ("PATH", previous != NULL ? previous : "", TRUE);
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_launch_agent_missing_folder (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-agent-folder-XXXXXX", NULL);
	g_autofree char *command = g_strdup_printf ("cd '%s'", dir);
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	TermuniWindow *window;
	TermuniTerminal *terminal;
	g_setenv ("SHELL", "/bin/sh", TRUE);
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_present (GTK_WINDOW (window));
	terminal = selected_terminal (window);
	pump (300);
	run_in_terminal (window, terminal, command);
	pump (300);
	g_assert_cmpstr (termuni_terminal_get_cwd (terminal), ==, dir);
	g_assert_cmpint (g_rmdir (dir), ==, 0);
	g_action_group_activate_action (G_ACTION_GROUP (window), "launch-agent", NULL);
	pump (400);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_true (label_has_text (GTK_WIDGET (window), "The folder no longer exists"));
	gtk_window_destroy (GTK_WINDOW (window));
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
}

static void
record_command_event (TermuniTerminal *terminal, gint status, gpointer data)
{
	g_array_append_val ((GArray *) data, status);
}

static void
test_bash_command_events (void)
{
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	g_autofree char *old_home = g_strdup (g_getenv ("HOME"));
	g_autofree char *rc = g_build_filename (g_get_user_state_dir (), ".bashrc", NULL);
	g_autoptr(GArray) events = g_array_new (FALSE, FALSE, sizeof (gint));
	GtkWidget *window;
	TermuniTerminal *terminal;
	g_assert_true (g_file_set_contents (rc, "PROMPT_COMMAND=':'\n", -1, NULL));
	g_setenv ("SHELL", "/bin/bash", TRUE);
	g_setenv ("HOME", g_get_user_state_dir (), TRUE);
	window = gtk_application_window_new (GTK_APPLICATION (app));
	terminal = TERMUNI_TERMINAL (termuni_terminal_new (g_get_user_state_dir (), TRUE));
	g_signal_connect (terminal, "command-finished", G_CALLBACK (record_command_event), events);
	gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (terminal));
	gtk_window_present (GTK_WINDOW (window));
	pump (600);
	g_assert_cmpuint (events->len, ==, 0);
	focus_terminal (window, GTK_WIDGET (terminal));
	termuni_terminal_insert_text (terminal, "false"); send_keysym (XK_Return, FALSE); pump (400);
	g_assert_cmpuint (events->len, ==, 1);
	g_assert_cmpint (g_array_index (events, gint, 0), ==, 1);
	termuni_terminal_insert_text (terminal, "true"); send_keysym (XK_Return, FALSE); pump (400);
	g_assert_cmpuint (events->len, ==, 2);
	g_assert_cmpint (g_array_index (events, gint, 1), ==, 0);
	gtk_window_destroy (GTK_WINDOW (window));
	g_setenv ("SHELL", old_shell != NULL ? old_shell : "/bin/sh", TRUE);
	g_setenv ("HOME", old_home != NULL ? old_home : g_get_home_dir (), TRUE);
	g_unlink (rc);
}

typedef struct { gboolean done; TermuniAgentAvailability status; GError *error; } UiAgentCheck;
static void
ui_agent_checked (GObject *source, GAsyncResult *result, gpointer data)
{
	UiAgentCheck *check = data;
	check->status = termuni_agent_check_finish (result, &check->error);
	check->done = TRUE;
}

static void
test_native_agent_context (void)
{
	gsize count;
	const TermuniAgent *const *agents = termuni_agent_all (&count);
	GtkWidget *window;
	TermuniTerminal *terminal;
	g_autoptr(GArray) events = g_array_new (FALSE, FALSE, sizeof (gint));
	if (g_getenv ("TERMUNI_TEST_NATIVE_AGENTS") == NULL)
	{
		g_test_skip ("set TERMUNI_TEST_NATIVE_AGENTS=1 to use installed agent CLIs");
		return;
	}
	for (guint i = 0; i < count; i++)
	{
		UiAgentCheck check = { 0 };
		termuni_agent_check_async (agents[i], "/tmp", NULL, ui_agent_checked, &check);
		for (guint tries = 0; !check.done && tries < 80; tries++) pump (100);
		g_assert_true (check.done);
		g_assert_no_error (check.error);
		g_assert_cmpint (check.status, ==, TERMUNI_AGENT_AVAILABLE);
	}
	window = gtk_application_window_new (GTK_APPLICATION (app));
	terminal = TERMUNI_TERMINAL (termuni_terminal_new_command ("/tmp", FALSE, "printf 'P2%s:%s\\n' AGENT_START \"$PWD\""));
	g_signal_connect (terminal, "command-finished", G_CALLBACK (record_command_event), events);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 650);
	gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (terminal));
	gtk_window_present (GTK_WINDOW (window));
	g_assert_true (wait_for_screen (terminal, "P2AGENT_START:/tmp", 5000));
	focus_terminal (window, GTK_WIDGET (terminal));
	termuni_terminal_insert_text (terminal, "printf 'P2%s\\n' AFTER_RETURN");
	send_keysym (XK_Return, FALSE);
	g_assert_true (wait_for_screen (terminal, "P2AFTER_RETURN", 5000));
	{
		g_autofree char *directory = g_build_filename (g_get_user_cache_dir (), "project's ação", NULL);
		g_autofree char *quoted = g_shell_quote (directory);
		g_autofree char *command = g_strdup_printf ("cd -- %s", quoted);
		g_assert_cmpint (g_mkdir_with_parents (directory, 0700), ==, 0);
		termuni_terminal_insert_text (terminal, command);
		send_keysym (XK_Return, FALSE);
		for (guint tries = 0; tries < 30 && g_strcmp0 (termuni_terminal_get_cwd (terminal), directory) != 0; tries++) pump (100);
		g_assert_cmpstr (termuni_terminal_get_cwd (terminal), ==, directory);
		g_rmdir (directory);
	}
	g_assert_cmpuint (events->len, ==, 0); /* cwd reporting must not enable notifications */
	gtk_window_destroy (GTK_WINDOW (window));
	pump (500);
}

#include "real-agent-driver.inc"
#include "accessible-text.inc"

static void
test_search_highlight (void)
{
 TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
 TermuniTerminal *terminal;
 GtkWidget *entry, *bar;
 termuni_window_sync_settings (window);
 gtk_window_present (GTK_WINDOW (window));
 pump (1000);
 terminal = selected_terminal (window);
 focus_terminal (GTK_WIDGET (window), GTK_WIDGET (terminal));
 type_text ("echo marker marker");
 send_keysym (XK_Return, FALSE);
 pump (500);
 g_action_group_activate_action (G_ACTION_GROUP (window), "find", NULL);
 entry = find_widget_of_type (GTK_WIDGET (window), GTK_TYPE_SEARCH_ENTRY);
 bar = find_widget_of_type (GTK_WIDGET (window), GTK_TYPE_SEARCH_BAR);
 g_assert_nonnull (entry);
 gtk_editable_set_text (GTK_EDITABLE (entry), "marker");
 pump (600);
 g_assert_cmpuint (termuni_terminal_get_highlight_count (terminal), >=, 2);
 save_widget_png (GTK_WIDGET (window), "search-highlight.png");
 gtk_editable_set_text (GTK_EDITABLE (entry), "  ");
 pump (600);
 g_assert_cmpuint (termuni_terminal_get_highlight_count (terminal), ==, 0);
 gtk_editable_set_text (GTK_EDITABLE (entry), "marker");
 pump (600);
 gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (bar), FALSE);
 pump (100);
 g_assert_cmpuint (termuni_terminal_get_highlight_count (terminal), ==, 0);
 /* A delayed search-changed signal must not restore a closed highlight. */
 gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (bar), TRUE);
 gtk_editable_set_text (GTK_EDITABLE (entry), "marker marker");
 gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (bar), FALSE);
 pump (700);
 g_assert_cmpuint (termuni_terminal_get_highlight_count (terminal), ==, 0);
 gtk_window_destroy (GTK_WINDOW (window));
 pump (200);
}

static void
capture_shortcut (GtkWindow *preferences, guint key, GdkModifierType mods)
{
 /* Xvfb has no window manager to focus a transient; inject at the modal root. */
 AdwDialog *dialog = find_window_dialog (preferences, ADW_TYPE_DIALOG);
 GtkWidget *controller_root;
 g_autoptr(GListModel) controllers = NULL;
 gboolean handled = FALSE;
 g_assert_nonnull (dialog);
 controller_root = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (dialog)));
 controllers = gtk_widget_observe_controllers (controller_root);
 for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
  g_autoptr(GObject) controller = g_list_model_get_item (controllers, i);
  if (GTK_IS_EVENT_CONTROLLER_KEY (controller)) {
   g_signal_emit_by_name (controller, "key-pressed", key, 0, mods, &handled);
   g_assert_true (handled);
   return;
  }
 }
 g_assert_not_reached ();
}

static void
test_shortcut_conflict (void)
{
 GtkWindow *preferences;
 GtkWidget *stack;
 GtkButton *button;
 GSettings *settings = termuni_application_get_settings (app);
 g_autoptr(GVariant) before = NULL;
 const guint keys[] = { GDK_KEY_w, GDK_KEY_W, GDK_KEY_c, GDK_KEY_F3 };
 const GdkModifierType mods[] = { GDK_CONTROL_MASK | GDK_SHIFT_MASK,
  GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_CONTROL_MASK, 0 };
 const char *owners[] = { "Close tab", "Close tab", "Copy selection or cancel command", "Next match" };
 g_settings_reset (settings, "action-shortcuts");
 before = g_settings_get_value (settings, "action-shortcuts");
 g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
 pump (300);
 preferences = find_preferences_window ();
 stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
 adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "shortcuts");
 button = find_button_for_action (GTK_WIDGET (preferences), "win.new-tab");
 for (guint i = 0; i < G_N_ELEMENTS (keys); i++) {
  AdwDialog *dialog;
  g_autoptr(GVariant) after = NULL;
  g_auto(GStrv) accels = NULL;
  g_signal_emit_by_name (button, "clicked");
  pump (200);
  capture_shortcut (preferences, keys[i], mods[i]);
  pump (100);
  after = g_settings_get_value (settings, "action-shortcuts");
  g_assert_true (g_variant_equal (before, after));
  accels = gtk_application_get_accels_for_action (GTK_APPLICATION (app), "win.new-tab");
  assert_accel_is (accels[0], "<Control><Shift>t");
  dialog = find_window_dialog (preferences, ADW_TYPE_DIALOG);
  g_assert_nonnull (dialog);
  g_assert_nonnull (strstr (adw_status_page_get_description (ADW_STATUS_PAGE (adw_dialog_get_child (dialog))), owners[i]));
  adw_dialog_close (dialog);
  pump (200);
 }
 g_signal_emit_by_name (button, "clicked");
 pump (200);
 capture_shortcut (preferences, GDK_KEY_t, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
 pump (300);
 g_assert_null (find_window_dialog (preferences, ADW_TYPE_DIALOG));
 g_settings_reset (settings, "action-shortcuts");
 gtk_window_destroy (preferences);
 pump (200);
}

static void
test_show_session_closed_tab (void)
{
 TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
 AdwTabView *view;
 AdwTabPage *first, *second;
 g_autoptr(TermuniTerminal) closed_terminal = NULL;
 g_autofree char *closed_id = NULL, *live_id = NULL;
 gtk_window_present (GTK_WINDOW (window));
 pump (500);
 view = ADW_TAB_VIEW (find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_VIEW));
 first = adw_tab_view_get_selected_page (view);
 live_id = g_strdup (termuni_terminal_get_session_id (selected_terminal (window)));
 g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
 pump (500);
 closed_terminal = g_object_ref (selected_terminal (window));
 closed_id = g_strdup (termuni_terminal_get_session_id (selected_terminal (window)));
 g_action_group_activate_action (G_ACTION_GROUP (window), "close-tab", NULL);
 pump (300);
 g_assert_cmpuint (adw_tab_view_get_n_pages (view), ==, 1);
 g_signal_emit_by_name (closed_terminal, "job-finished", (guint64) 12000);
 g_action_group_activate_action (G_ACTION_GROUP (app), "show-session", g_variant_new_string (closed_id));
 g_assert_true (adw_tab_view_get_selected_page (view) == first);
 g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
 pump (300);
 second = adw_tab_view_get_selected_page (view);
 g_assert_true (second != first);
 g_action_group_activate_action (G_ACTION_GROUP (app), "show-session", g_variant_new_string (live_id));
 g_assert_true (adw_tab_view_get_selected_page (view) == first);
 gtk_window_destroy (GTK_WINDOW (window));
 pump (200);
}

static void
test_visual_screens (void)
{
 TermuniWindow *window;
 GtkWindow *preferences;
 GtkWidget *stack, *bar, *overview, *menu;
 GSettings *settings = termuni_application_get_settings (app);
 const char *pages[] = { "behavior", "appearance", "shortcuts", "attachments" };
 const char *schemes[] = { "light", "dark", "system" };
 const char *palettes[] = { "gnome", "dracula", "ubuntu", "nord", "high-contrast" };
 g_autofree char *old_scheme = NULL, *old_palette = NULL;
 guint old_font;

 if (g_getenv ("TERMUNI_UI_SCREENSHOTS") == NULL) {
  g_test_skip ("Set TERMUNI_UI_SCREENSHOTS to capture the visual review");
  return;
 }
 old_scheme = g_settings_get_string (settings, "color-scheme");
 old_palette = g_settings_get_string (settings, "palette");
 old_font = g_settings_get_uint (settings, "font-size");
 g_settings_set_uint (settings, "font-size", 11);
 g_settings_set_string (settings, "color-scheme", "dark");
 g_settings_set_string (settings, "palette", "gnome");
 g_application_activate (G_APPLICATION (app));
 pump (500);
 window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
 termuni_window_sync_settings (window);
 gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
 gtk_window_present (GTK_WINDOW (window));
 pump (800);
 bar = find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_BAR);
 g_assert_false (adw_tab_bar_get_tabs_revealed (ADW_TAB_BAR (bar)));
 save_widget_png (GTK_WIDGET (window), "main-one-tab.png");
 g_action_group_activate_action (G_ACTION_GROUP (window), "new-tab", NULL);
 pump (500);
 g_assert_true (adw_tab_bar_get_tabs_revealed (ADW_TAB_BAR (bar)));
 save_widget_png (GTK_WIDGET (window), "main-two-tabs.png");
 g_action_group_activate_action (G_ACTION_GROUP (window), "toggle-overview", NULL);
 pump (500);
 overview = find_widget_of_type (GTK_WIDGET (window), ADW_TYPE_TAB_OVERVIEW);
 g_assert_true (adw_tab_overview_get_open (ADW_TAB_OVERVIEW (overview)));
 pump (800);
 save_widget_png (GTK_WIDGET (window), "tab-overview.png");
 adw_tab_overview_set_open (ADW_TAB_OVERVIEW (overview), FALSE);
 pump (300);
 menu = find_widget_of_type (GTK_WIDGET (window), GTK_TYPE_MENU_BUTTON);
 for (guint i = 0; i < G_N_ELEMENTS (schemes); i++) {
  g_autofree char *name = g_strdup_printf ("menu-%s.png", schemes[i]);
  g_settings_set_string (settings, "color-scheme", schemes[i]);
  gtk_menu_button_popup (GTK_MENU_BUTTON (menu));
  pump (400);
  g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (menu)))));
  save_widget_png (GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (menu))), name);
  gtk_menu_button_popdown (GTK_MENU_BUTTON (menu));
 }
 g_settings_set_string (settings, "color-scheme", "dark");
 for (guint i = 0; i < G_N_ELEMENTS (palettes); i++) {
  g_autofree char *name = g_strdup_printf ("palette-%s.png", palettes[i]);
  g_settings_set_string (settings, "palette", palettes[i]);
  pump (200);
  save_widget_png (GTK_WIDGET (window), name);
 }
 g_settings_set_string (settings, "palette", "gnome");
 g_action_group_activate_action (G_ACTION_GROUP (app), "preferences", NULL);
 pump (500);
 preferences = find_preferences_window ();
 stack = find_widget_of_type (GTK_WIDGET (preferences), ADW_TYPE_VIEW_STACK);
 for (guint i = 0; i < G_N_ELEMENTS (pages); i++) {
  g_autofree char *name = g_strdup_printf ("preferences-%s.png", pages[i]);
  adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), pages[i]);
  pump (300);
  save_widget_png (GTK_WIDGET (preferences), name);
 }
 adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), "appearance");
 {
  GtkButton *all = find_button_labelled (GTK_WIDGET (preferences), "Show all palettes");
  g_assert_nonnull (all);
  g_signal_emit_by_name (all, "clicked");
  pump (300);
  save_widget_png (GTK_WIDGET (preferences), "preferences-all-palettes.png");
 }
 gtk_window_destroy (preferences);
 gtk_window_present (GTK_WINDOW (window));
 pump (300);
 g_action_group_activate_action (G_ACTION_GROUP (app), "about", NULL);
 pump (400);
 g_assert_nonnull (find_window_dialog (GTK_WINDOW (window), ADW_TYPE_DIALOG));
 save_widget_png (GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (find_window_dialog (GTK_WINDOW (window), ADW_TYPE_ABOUT_DIALOG)))), "about.png");
 adw_dialog_close (find_window_dialog (GTK_WINDOW (window), ADW_TYPE_DIALOG));
 pump (300);
 g_action_group_activate_action (G_ACTION_GROUP (app), "shortcuts", NULL);
 pump (400);
 save_widget_png (GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (find_window_dialog (GTK_WINDOW (window), ADW_TYPE_SHORTCUTS_DIALOG)))), "shortcuts.png");
 adw_dialog_close (find_window_dialog (GTK_WINDOW (window), ADW_TYPE_DIALOG));
 pump (300);
 gtk_window_destroy (GTK_WINDOW (window));
 g_settings_set_string (settings, "color-scheme", old_scheme);
 g_settings_set_string (settings, "palette", old_palette);
 g_settings_set_uint (settings, "font-size", old_font);
 pump (200);
}

/* Sample the blank top pixel of a matched cell, excluding glyph antialiasing. */
static guint
highlight_pixel (TermuniTerminal *terminal, guint row)
{
 GtkWidget *widget = GTK_WIDGET (terminal);
 g_autoptr(GdkPaintable) paintable = gtk_widget_paintable_new (widget);
 GtkSnapshot *snapshot = gtk_snapshot_new ();
 GskRenderNode *node;
 g_autoptr(GdkTexture) texture = NULL;
 g_autofree guchar *pixels = NULL;
 guint cw, ch;
 gsize stride, offset;
 termuni_terminal_get_cell_size (terminal, &cw, &ch);
 gdk_paintable_snapshot (paintable, GDK_SNAPSHOT (snapshot), gtk_widget_get_width (widget), gtk_widget_get_height (widget));
 node = gtk_snapshot_free_to_node (snapshot);
 g_assert_nonnull (node);
 texture = gsk_renderer_render_texture (gtk_native_get_renderer (gtk_widget_get_native (widget)), node, NULL);
 gsk_render_node_unref (node);
 stride = gdk_texture_get_width (texture) * 4;
 pixels = g_malloc (stride * gdk_texture_get_height (texture));
 gdk_texture_download (texture, pixels, stride);
 offset = (12 + row * ch + 1) * stride + 8 * 4;
 return pixels[offset] + pixels[offset + 1] + pixels[offset + 2];
}

static void
test_search_current_match (void)
{
 TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
 TermuniTerminal *terminal;
 GhosttyTerminal vt;
 const char *text = "\033[2J\033[Hfoo\r\nfoo\r\n";
 gtk_window_present (GTK_WINDOW (window));
 pump (800);
 terminal = selected_terminal (window);
 termuni_terminal_set_palette (terminal, "high-contrast");
 vt = termuni_terminal_get_vt_handle (terminal);
 ghostty_terminal_vt_write (vt, (const uint8_t *) text, strlen (text));
 g_assert_true (termuni_terminal_find (terminal, "foo", FALSE));
 pump (200);
 g_assert_cmpuint (highlight_pixel (terminal, 1), >, highlight_pixel (terminal, 0));
 g_assert_true (termuni_terminal_find (terminal, "foo", FALSE));
 pump (200);
 g_assert_cmpuint (highlight_pixel (terminal, 0), >, highlight_pixel (terminal, 1));
 g_assert_true (termuni_terminal_find (terminal, "foo", TRUE));
 pump (200);
 g_assert_cmpuint (highlight_pixel (terminal, 1), >, highlight_pixel (terminal, 0));
 gtk_window_destroy (GTK_WINDOW (window));
 pump (200);
}

static void
test_session_migration_write_failure (void)
{
	g_autofree char *dir = g_build_filename (g_get_user_state_dir (), "termuni", NULL);
	g_autofree char *v2 = g_build_filename (dir, "session-v2.gvariant", NULL);
	g_autofree char *v3 = session_file ();
	g_autoptr(GVariant) legacy = g_variant_parse (G_VARIANT_TYPE ("a(ia(ss))"),
	    "[(0, [('/tmp', 'Safe')])]", NULL, NULL, NULL);
	g_autoptr(GBytes) data = g_variant_get_data_as_bytes (legacy);
	g_autofree char *contents = NULL;
	gsize length = 0;
	TermuniWindow *window;

	destroy_all_windows ();
	forget_saved_sessions ();
	g_assert_cmpint (g_mkdir_with_parents (dir, 0700), ==, 0);
	g_assert_true (g_file_set_contents (v2, g_bytes_get_data (data, NULL),
	    (gssize) g_bytes_get_size (data), NULL));
	/* A directory at the destination forces a real write failure, even as root. */
	g_assert_cmpint (g_mkdir (v3, 0700), ==, 0);
	g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "*Could not save the session*");
	g_application_activate (G_APPLICATION (app));
	g_test_assert_expected_messages ();
	pump (200);
	window = main_window ();
	g_assert_nonnull (window);
	g_assert_cmpuint (tab_count (window), ==, 1);
	g_assert_cmpstr (termuni_terminal_get_cwd (selected_terminal (window)), ==, "/tmp");
	g_assert_true (g_file_test (v3, G_FILE_TEST_IS_DIR));
	g_assert_true (g_file_get_contents (v2, &contents, &length, NULL));
	g_assert_cmpmem (contents, length, g_bytes_get_data (data, NULL), g_bytes_get_size (data));
	g_assert_cmpint (g_rmdir (v3), ==, 0);
	destroy_all_windows ();
	forget_saved_sessions ();
	g_application_activate (G_APPLICATION (app));
	pump (300);
}

static void
test_truecolor_unicode_render (void)
{
	TermuniWindow *window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	TermuniTerminal *terminal;
	GhosttyTerminal vt;
	const char *text = "\033[?2027h\033[2J\033[H日本語 é 👩‍💻 ❤️\r\n"
	    "\033[38;2;17;83;149;48;2;203;41;7m  \033[0m\033[?25l";
	g_autofree char *line = NULL;
	g_autoptr(GdkPaintable) paintable = NULL;
	GtkSnapshot *snapshot;
	GskRenderNode *node;
	g_autoptr(GdkTexture) texture = NULL;
	g_autofree guchar *pixels = NULL;
	gsize stride, offset;
	guint cw, ch;
	guint32 pixel;

	gtk_window_present (GTK_WINDOW (window));
	pump (700);
	terminal = selected_terminal (window);
	termuni_terminal_set_font_size (terminal, 11);
	vt = termuni_terminal_get_vt_handle (terminal);
	ghostty_terminal_vt_write (vt, (const uint8_t *) text, strlen (text));
	gtk_widget_queue_draw (GTK_WIDGET (terminal));
	pump (300);
	g_assert_true (termuni_vt_viewport_row_text (vt, termuni_terminal_get_columns (terminal), 0, &line));
	g_assert_nonnull (strstr (line, "日本語 é 👩‍💻 ❤️"));
	paintable = gtk_widget_paintable_new (GTK_WIDGET (terminal));
	snapshot = gtk_snapshot_new ();
	gdk_paintable_snapshot (paintable, GDK_SNAPSHOT (snapshot),
	    gtk_widget_get_width (GTK_WIDGET (terminal)), gtk_widget_get_height (GTK_WIDGET (terminal)));
	node = gtk_snapshot_free_to_node (snapshot);
	g_assert_nonnull (node);
	texture = gsk_renderer_render_texture (gtk_native_get_renderer (
	    gtk_widget_get_native (GTK_WIDGET (terminal))), node, NULL);
	gsk_render_node_unref (node);
	termuni_terminal_get_cell_size (terminal, &cw, &ch);
	g_assert_cmpint (gdk_texture_get_width (texture), >, 8);
	g_assert_cmpint (gdk_texture_get_height (texture), >, 12 + ch + 1);
	stride = gdk_texture_get_width (texture) * 4;
	pixels = g_malloc (stride * gdk_texture_get_height (texture));
	gdk_texture_download (texture, pixels, stride);
	offset = (12 + ch + 1) * stride + 8 * 4;
	/* GDK_MEMORY_DEFAULT is native-endian premultiplied ARGB32. The sampled
	 * cell is opaque, with no glyph/cursor, so its RGB must be exact. */
	memcpy (&pixel, pixels + offset, sizeof pixel);
	g_assert_cmpuint ((pixel >> 16) & 0xff, ==, 203);
	g_assert_cmpuint ((pixel >> 8) & 0xff, ==, 41);
	g_assert_cmpuint (pixel & 0xff, ==, 7);
	save_widget_png (GTK_WIDGET (window), "truecolor-unicode.png");
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
}

static void
test_modern_tui (void)
{
	g_autofree char *btop = g_find_program_in_path ("btop");
	g_autofree char *config = g_build_filename (g_get_user_config_dir (), "termuni-btop-test.conf", NULL);
	g_autofree char *quoted_config = NULL;
	g_autofree char *quoted_btop = NULL;
	g_autofree char *command = NULL;
	g_autofree char *text = NULL;
	g_autofree char *folded = NULL;
	TermuniWindow *window;
	TermuniTerminal *terminal;
	GhosttyTerminal vt;

	if (btop == NULL)
	{
		g_test_skip ("btop is not installed; modern-TUI acceptance remains pending");
		return;
	}
	/* Isolated configuration; do not show the host's process commands. */
	g_assert_true (g_file_set_contents (config,
	    "shown_boxes = \"cpu mem\"\ntruecolor = true\ncheck_temp = false\nshow_disks = false\n", -1, NULL));
	quoted_config = g_shell_quote (config);
	quoted_btop = g_shell_quote (btop);
	command = g_strdup_printf ("%s --config %s --force-utf --no-tty", quoted_btop, quoted_config);
	window = g_object_new (TERMUNI_TYPE_WINDOW, "application", app, NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), 1100, 720);
	gtk_window_present (GTK_WINDOW (window));
	pump (700);
	terminal = selected_terminal (window);
	termuni_terminal_set_font_size (terminal, 11);
	vt = termuni_terminal_get_vt_handle (terminal);
	run_in_terminal (window, terminal, command);
	g_assert_true (wait_for_alternate_screen (vt, TRUE, 6000));
	pump (1500);
	g_assert_true (termuni_terminal_has_foreground_job (terminal));
	text = screen_text (terminal);
	folded = g_utf8_casefold (text, -1);
	g_assert_nonnull (strstr (folded, "cpu"));
	g_assert_nonnull (strstr (folded, "mem"));
	save_widget_png (GTK_WIDGET (window), "modern-tui-btop.png");
	gtk_window_set_default_size (GTK_WINDOW (window), 1000, 640);
	pump (700);
	g_assert_true (termuni_vt_alternate_screen (vt));
	g_clear_pointer (&text, g_free);
	g_clear_pointer (&folded, g_free);
	text = screen_text (terminal);
	folded = g_utf8_casefold (text, -1);
	g_assert_nonnull (strstr (folded, "cpu"));
	g_assert_nonnull (strstr (folded, "mem"));
	send_keysym (XK_q, FALSE);
	g_assert_true (wait_for_alternate_screen (vt, FALSE, 5000));
	g_assert_true (wait_for_job_end (terminal, 3000));
	gtk_window_destroy (GTK_WINDOW (window));
	pump (200);
	g_unlink (config);
}

static void
test_save_empty_session (void)
{
	g_autofree char *path = session_file ();
	g_autofree char *contents = NULL;
	gsize length = 1;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GVariant) sessions = NULL;

	destroy_all_windows ();
	termuni_application_save_session_now (app);
	g_assert_true (g_file_get_contents (path, &contents, &length, NULL));
	bytes = g_bytes_new (contents, length);
	sessions = g_variant_ref_sink (g_variant_new_from_bytes (
	    G_VARIANT_TYPE ("a(iiiba(ss))"), bytes, FALSE));
	g_assert_true (g_variant_is_normal_form (sessions));
	g_assert_cmpuint (g_variant_n_children (sessions), ==, 0);
	forget_saved_sessions ();
	g_application_activate (G_APPLICATION (app));
	pump (300);
}

static void
test_truecolor_environment (void)
{
	g_autofree char *dir = g_dir_make_tmp ("termuni-color-env-XXXXXX", NULL);
	g_autofree char *file = g_build_filename (dir, "environment", NULL);
	g_autofree char *quoted = g_shell_quote (file);
	g_autofree char *command = g_strdup_printf ("printf '%%s' \"$COLORTERM\" > %s", quoted);
	g_autofree char *old_shell = g_strdup (g_getenv ("SHELL"));
	g_autofree char *old_env = g_strdup (g_getenv ("ENV"));
	g_autofree char *old_color = g_strdup (g_getenv ("COLORTERM"));
	g_autofree char *contents = NULL;
	TermuniTerminal *terminal;
	gint64 deadline;
	g_setenv ("SHELL", "/bin/sh", TRUE);
	g_setenv ("ENV", "/dev/null", TRUE);
	g_setenv ("COLORTERM", "incorrect-parent-value", TRUE);
	terminal = TERMUNI_TERMINAL (termuni_terminal_new_command ("/tmp", FALSE, command));
	g_object_ref_sink (terminal);
	if (old_shell) g_setenv ("SHELL", old_shell, TRUE); else g_unsetenv ("SHELL");
	if (old_env) g_setenv ("ENV", old_env, TRUE); else g_unsetenv ("ENV");
	if (old_color) g_setenv ("COLORTERM", old_color, TRUE); else g_unsetenv ("COLORTERM");
	deadline = g_get_monotonic_time () + 4 * G_USEC_PER_SEC;
	while (!g_file_test (file, G_FILE_TEST_EXISTS) && g_get_monotonic_time () < deadline) pump (50);
	g_assert_true (g_file_get_contents (file, &contents, NULL, NULL));
	g_assert_cmpstr (contents, ==, "truecolor");
	g_object_unref (terminal);
	pump (150);
	g_unlink (file); g_rmdir (dir);
}

#include "lifecycle.inc"
#include "session-process.inc"
#include "benchmark.inc"

int
main (int argc, char **argv)
{
	g_autofree char *root = NULL;

	g_setenv ("LANGUAGE", "C", TRUE);
	if (g_getenv ("TERMUNI_SESSION_PROCESS_ROOT") != NULL)
		root = g_strdup (g_getenv ("TERMUNI_SESSION_PROCESS_ROOT"));
	else if (g_getenv ("TERMUNI_TEST_CACHE_DIR") != NULL)
	{
		/* The QA driver binds only its private temporary directory at the
		 * same path on both sides, so host rc files and images are visible. */
		const char *cache = g_getenv ("TERMUNI_TEST_CACHE_DIR");
		g_assert_nonnull (cache);
		g_assert_cmpint (g_mkdir_with_parents (cache, 0700), ==, 0);
		root = g_build_filename (cache, "termuni-ui-XXXXXX", NULL);
		g_assert_nonnull (g_mkdtemp (root));
	}
	else root = g_dir_make_tmp ("termuni-ui-XXXXXX", NULL);
	g_assert_nonnull (root);
	g_setenv ("XDG_STATE_HOME", root, TRUE);
	g_setenv ("XDG_CACHE_HOME", root, TRUE);
	g_setenv ("XDG_CONFIG_HOME", root, TRUE);
	g_setenv ("XDG_DATA_HOME", root, TRUE);
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
	if (!gtk_init_check ())
	{
		g_test_message ("skipped: no display available");
		return 77;
	}
	if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
	{
		g_test_message ("skipped: the input test needs the X11 backend");
		return 77;
	}
	x_display = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
	xtest_load ();

	app = termuni_application_new (APPLICATION_ID, G_APPLICATION_NON_UNIQUE);
	g_application_register (G_APPLICATION (app), NULL, NULL);

	if (g_getenv ("TERMUNI_SESSION_PROCESS_MODE") != NULL)
	{
		int status;
		g_idle_add (session_process_child, (gpointer) g_getenv ("TERMUNI_SESSION_PROCESS_MODE"));
		status = g_application_run (G_APPLICATION (app), 0, NULL);
		g_object_unref (app);
		return status;
	}
	g_test_init (&argc, &argv, NULL);
	/* g_test_init makes warnings fatal; criticals stay fatal, GTK warnings from the
	 * virtual keyboard layout are reported but do not stop the run. */
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);
	/* Runs first: it only needs Preferences, and a main window on top would take its keys. */
	g_test_add_func ("/ui/shortcut-editor", test_shortcut_editor);
	g_test_add_func ("/ui/terminal-shortcut-rebind", test_terminal_shortcut_rebind);
	g_test_add_func ("/ui/select-all-shortcut", test_select_all_shortcut);
	g_test_add_func ("/ui/window-tabs", test_window_tabs);
	g_test_add_func ("/ui/tab-churn", test_tab_churn);
	g_test_add_func ("/ui/typing-and-interrupt", test_typing_and_interrupt);
	g_test_add_func ("/ui/drop-image", test_drop_image);
	g_test_add_func ("/ui/paste-image-clipboard", test_paste_image_clipboard);
	g_test_add_func ("/ui/mouse-selection-copy", test_mouse_selection_copy);
	g_test_add_func ("/ui/preferences-window", test_preferences_window);
	g_test_add_func ("/ui/session-saved", test_session_saved);
	g_test_add_func ("/ui/large-output", test_large_output);
	g_test_add_func ("/ui/tui-alternate-screen", test_tui_alternate_screen);
	g_test_add_func ("/ui/vim-session", test_vim_session);
	g_test_add_func ("/ui/mouse-tracking", test_mouse_tracking_report);
	g_test_add_func ("/ui/input-method", test_dead_key_and_unicode);
	g_test_add_func ("/ui/paste-integrity", test_paste_integrity);
	g_test_add_func ("/ui/sessions-isolated", test_sessions_are_isolated);
	g_test_add_func ("/ui/tabs-independent", test_tabs_are_independent);
	g_test_add_func ("/ui/closed-tab-releases-terminal", test_closed_tab_releases_terminal);
	g_test_add_func ("/ui/no-orphan-shells", test_no_orphan_shells);
	g_test_add_func ("/ui/memory-churn", test_memory_stable_under_churn);
	g_test_add_func ("/ui/geometry-saved", test_geometry_saved);
	g_test_add_func ("/ui/theme-zoom-with-tabs", test_theme_zoom_with_tabs);
	g_test_add_func ("/ui/attachment-sequence", test_attachment_sequence);
	g_test_add_func ("/ui/clipboard-shortcuts-default", test_clipboard_shortcuts_default);
	g_test_add_func ("/ui/session-v2-migration", test_session_v2_migration);
	g_test_add_func ("/ui/session-restore-roundtrip", test_session_restore_roundtrip);
	g_test_add_func ("/ui/session-corrupt-file", test_session_corrupt_file);
	g_test_add_func ("/ui/attach-without-agent-binary", test_attach_without_agent_binary);
	g_test_add_func ("/ui/launch-agent", test_launch_agent);
	g_test_add_func ("/ui/launch-agent-quick-menu", test_launch_agent_quick_menu);
	g_test_add_func ("/ui/launch-agent-missing-folder", test_launch_agent_missing_folder);
	g_test_add_func ("/ui/launch-agent-missing", test_launch_agent_missing);
	g_test_add_func ("/ui/native-agent-context", test_native_agent_context);
	g_test_add_func ("/ui/bash-command-events", test_bash_command_events);
	g_test_add_func ("/ui/real-agent-driver", test_real_agent_driver);
	g_test_add_func ("/ui/agent-choice", test_agent_choice);
	g_test_add_func ("/ui/accessibility-labels", test_accessibility_labels);
	g_test_add_func ("/ui/accessible-text", test_accessible_text);
	g_test_add_func ("/ui/accessible-text-atspi-fixture", test_accessible_text_atspi_fixture);
	g_test_add_func ("/ui/grid-fits-widget", test_grid_fits_widget);
 g_test_add_func ("/ui/search-highlight", test_search_highlight);
 g_test_add_func ("/ui/shortcut-conflict", test_shortcut_conflict);
 g_test_add_func ("/ui/show-session-closed-tab", test_show_session_closed_tab);
 g_test_add_func ("/ui/visual-screens", test_visual_screens);
 g_test_add_func ("/ui/search-current-match", test_search_current_match);
	g_test_add_func ("/ui/session-migration-write-failure", test_session_migration_write_failure);
	g_test_add_func ("/ui/save-empty-session", test_save_empty_session);
	g_test_add_func ("/ui/session-cross-process-restart", test_session_cross_process_restart);
	g_test_add_func ("/ui/process-quit-reaps-resistant-child", test_process_quit_reaps_resistant_child);
	g_test_add_func ("/ui/truecolor-unicode-render", test_truecolor_unicode_render);
	g_test_add_func ("/ui/truecolor-environment", test_truecolor_environment);
	g_test_add_func ("/ui/modern-tui", test_modern_tui);
	g_test_add_func ("/ui/async-child-close", test_async_child_close);
	g_test_add_func ("/ui/closed-input-feedback", test_closed_input_feedback);
	g_test_add_func ("/ui/benchmark", test_benchmark);
	g_test_add_func ("/ui/dialog-windows", test_dialog_windows);
	g_test_add_func ("/ui/quit-confirmation", test_quit_confirmation);
	{
		int status = g_test_run ();

		g_object_unref (app);
		return status;
	}
}
