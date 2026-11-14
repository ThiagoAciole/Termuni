/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <adwaita.h>

static inline void
termac_dialog_parent_unrealized (AdwDialog *dialog)
{
	/* Use the normal close path so alert choices finish with their cancel
	 * response and release the async callback's reference to the parent. */
	if (gtk_widget_get_parent (GTK_WIDGET (dialog)) != NULL)
	{
		adw_dialog_set_can_close (dialog, TRUE);
		adw_dialog_close (dialog);
	}
}

static inline void
termac_dialog_closed (AdwDialog *dialog, GtkWindow *parent)
{
	g_signal_handlers_disconnect_by_func (parent, termac_dialog_parent_unrealized, dialog);
}

/* A NULL presentation parent selects libadwaita's separate GtkWindow path.
 * Restore the transient/modal relationship without adding an in-window sheet. */
static inline void
termac_dialog_configure_window (AdwDialog *dialog, GtkWindow *parent)
{
	GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (dialog)));
	GtkApplication *app = parent ? gtk_window_get_application (parent) : NULL;
	GtkWindow *context = parent;

	while (context != NULL && !G_IS_ACTION_GROUP (context))
		context = gtk_window_get_transient_for (context);
	if (app == NULL && context != NULL)
		app = gtk_window_get_application (context);
	gtk_window_set_transient_for (window, parent);
	gtk_window_set_modal (window, TRUE);
	if (app != NULL)
	{
		gtk_window_set_application (window, app);
		gtk_widget_insert_action_group (GTK_WIDGET (dialog), "app", G_ACTION_GROUP (app));
	}
	if (context != NULL)
		gtk_widget_insert_action_group (GTK_WIDGET (dialog), "win", G_ACTION_GROUP (context));
	if (parent != NULL)
	{
		g_signal_connect_object (parent, "unrealize", G_CALLBACK (termac_dialog_parent_unrealized),
		                         dialog, G_CONNECT_SWAPPED);
		g_signal_connect_object (dialog, "closed", G_CALLBACK (termac_dialog_closed), parent, 0);
	}
}

static inline void
termac_dialog_present_window (AdwDialog *dialog, GtkWindow *parent)
{
	adw_dialog_present (dialog, NULL);
	termac_dialog_configure_window (dialog, parent);
}

static inline void
termac_alert_choose_window (AdwAlertDialog *dialog, GtkWindow *parent,
                            GAsyncReadyCallback callback, gpointer data)
{
	adw_alert_dialog_choose (dialog, NULL, NULL, callback, data);
	termac_dialog_configure_window (ADW_DIALOG (dialog), parent);
}
