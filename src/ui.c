// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ui.c - Alarm Clock applet UI routines
 *
 * Copyright (C) 2007-2008 Johannes H. Jensen <joh@pseudoberries.com>
 * Copyright (C) 2022 Tasos Sahanidis <code@tasossah.com>
 */

#include <time.h>
#include <string.h>
#include <libnotify/notify.h>

#include "alarm-applet.h"
#include "alarm-actions.h"
#include "ui.h"

enum {
    GICON_COL,
    TEXT_COL,
    N_COLUMNS,
};

static void alarm_applet_status_init(AlarmApplet* applet);

/*
 * Load a user interface by name
 */
GtkBuilder* alarm_applet_ui_load(const char* name, AlarmApplet* applet)
{
    GtkBuilder* builder = NULL;
    GError* error = NULL;
    char* filename;

    filename = alarm_applet_get_data_path(name);

    g_assert(filename != NULL);

    builder = gtk_builder_new();

    g_debug("Loading UI from %s...", filename);

    if(gtk_builder_add_from_file(builder, filename, &error)) {
        /* Connect signals */
        gtk_builder_connect_signals(builder, applet);
    } else {
        g_critical("Couldn't load the interface '%s'. %s", filename, error->message);
        g_error_free(error);
    }

    g_free(filename);

    return builder;
}

void display_error_dialog(const gchar* message, const gchar* secondary_text, GtkWindow* parent)
{
    GtkWidget* dialog;

    dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message);

    if(secondary_text != NULL) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary_text);
    }

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static gboolean is_separator(GtkTreeModel* model, GtkTreeIter* iter, gpointer sep_index)
{
    GtkTreePath* path;
    gboolean result;

    path = gtk_tree_model_get_path(model, iter);
    result = gtk_tree_path_get_indices(path)[0] == GPOINTER_TO_INT(sep_index);
    gtk_tree_path_free(path);

    return result;
}

/*
 * Shamelessly stolen from gnome-da-capplet.c
 */
void fill_combo_box(GtkComboBox* combo_box, GList* list, const gchar* custom_label)
{
    GList* l;
    GtkTreeModel* model;
    GtkCellRenderer* renderer;
    GtkTreeIter iter;
    AlarmListEntry* entry;

    g_debug("fill_combo_box... %d", g_list_length(list));

    gtk_combo_box_set_row_separator_func(combo_box, is_separator, GINT_TO_POINTER(g_list_length(list)), NULL);

    model = GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_ICON, G_TYPE_STRING));
    gtk_combo_box_set_model(combo_box, model);

    gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo_box));

    renderer = gtk_cell_renderer_pixbuf_new();

    /* not all cells have a pixbuf, this prevents the combo box to shrink */
    gtk_cell_renderer_set_fixed_size(renderer, -1, 22);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), renderer, "gicon", GICON_COL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), renderer, "text", TEXT_COL, NULL);

    for(l = list; l != NULL; l = g_list_next(l)) {
        GIcon* icon;

        entry = (AlarmListEntry*)l->data;
        icon = g_icon_new_for_string(entry->icon, NULL);

        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, GICON_COL, icon, TEXT_COL, entry->name, -1);

        g_object_unref(icon);
    }

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, -1);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, GICON_COL, NULL, TEXT_COL, custom_label, -1);
}

/**
 * Show a notification
 */
void alarm_applet_notification_show(AlarmApplet* applet, const gchar* summary, const gchar* body, const gchar* icon)

{
    NotifyNotification* n;
    GError* error = NULL;

    n = notify_notification_new(summary, body, icon);

    if(!notify_notification_show(n, &error)) {
        g_warning("Failed to send notification: %s", error->message);
        g_error_free(error);
    }

    g_object_unref(G_OBJECT(n));
}

void alarm_applet_label_update(AlarmApplet* applet)
{
    GList* l;
    Alarm* a;
    Alarm* next_alarm = NULL;
    struct tm* tm;
    gchar* tmp;

    GVariant* state = g_action_get_state(G_ACTION(applet->action_toggle_show_label));
    if(!state)
        return;

    gboolean show_label = g_variant_get_boolean(state);
    g_variant_unref(state);


    if(!show_label) {
        app_indicator_set_label(applet->app_indicator, NULL, NULL);
        return;
    }

    //
    // Show countdown
    //
    for(l = applet->alarms; l; l = l->next) {
        a = ALARM(l->data);
        if(!a->active)
            continue;

        if(!next_alarm || a->timestamp < next_alarm->timestamp) {
            next_alarm = a;
        }
    }

    if(!next_alarm) {
        // No upcoming alarms
        app_indicator_set_label(applet->app_indicator, NULL, NULL);
        return;
    }

    tm = alarm_get_remain(next_alarm);
    tmp = g_strdup_printf("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    app_indicator_set_label(applet->app_indicator, tmp, NULL);
    g_free(tmp);
}

/*
 * Updates label etc
 */
static gboolean alarm_applet_ui_update(AlarmApplet* applet)
{
    alarm_applet_label_update(applet);

    // alarm_applet_update_tooltip (applet);

    return TRUE;
}


void alarm_applet_ui_init(AlarmApplet* applet)
{
    /* Load UI with GtkBuilder */
    applet->ui = alarm_applet_ui_load("alarm-clock.ui", applet);


    /* Initialize status icon */
    alarm_applet_status_init(applet);

    /* Initialize libnotify */
    if(!notify_init(PACKAGE_NAME)) {
        g_critical("Could not intialize libnotify!");
    }

    /* Initialize alarm list window */
    applet->list_window = alarm_list_window_new(applet);

    /* Initialize alarm settings dialog */
    applet->settings_dialog = alarm_settings_dialog_new(applet);

    /* Load CSS */
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, alarm_applet_get_data_path("alarm-clock.css"), NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    /* Connect signals */
    // gtk_builder_connect_signals (applet->ui, applet);

    /* Initialize actions */
    alarm_applet_actions_init(applet);

    /* Initialize preferences dialog */
    prefs_init(applet);

    /* Set up UI updater */
    alarm_applet_ui_update(applet);
    g_timeout_add_seconds(1, (GSourceFunc)alarm_applet_ui_update, applet);
}

/*
 * Initialize status icon
 */
static void alarm_applet_status_init(AlarmApplet* applet)
{
    applet->status_menu = GTK_WIDGET(gtk_builder_get_object(applet->ui, "status_menu"));

    // WARNING: The following line is required to get AppIndicator to function correctly with GActions
    gtk_widget_insert_action_group(applet->status_menu, "app", G_ACTION_GROUP(applet->application));

    applet->app_indicator = app_indicator_new(PACKAGE_NAME, ALARM_ICON, APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_title(applet->app_indicator, _("Alarm Clock"));
    app_indicator_set_status(applet->app_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon(applet->app_indicator, TRIGGERED_ICON);
    app_indicator_set_menu(applet->app_indicator, GTK_MENU(applet->status_menu));
}

/*
 * Update the status icon
 */
void alarm_applet_status_update(AlarmApplet* applet)
{
    if(applet->n_triggered > 0) {
        app_indicator_set_status(applet->app_indicator, APP_INDICATOR_STATUS_ATTENTION);
    } else {
        app_indicator_set_status(applet->app_indicator, APP_INDICATOR_STATUS_ACTIVE);
    }
}

void alarm_applet_status_menu_edit_cb(GtkMenuItem* menuitem, gpointer user_data)
{
    AlarmApplet* applet = (AlarmApplet*)user_data;

    alarm_list_window_show(applet->list_window);
}

void alarm_applet_status_menu_prefs_cb(GtkMenuItem* menuitem, gpointer user_data)
{
    AlarmApplet* applet = (AlarmApplet*)user_data;

    prefs_dialog_show(applet);
}

void alarm_applet_status_menu_about_cb(GtkMenuItem* menuitem, gpointer user_data)
{
    AlarmApplet* applet = (AlarmApplet*)user_data;
    gchar* title;

    gboolean visible;
    GtkAboutDialog* dialog = GTK_ABOUT_DIALOG(gtk_builder_get_object(applet->ui, "about-dialog"));

    g_object_get(dialog, "visible", &visible, NULL);

    if(!visible) {
        // About Alarm Clock
        title = g_strdup_printf(_("About %s"), _(ALARM_NAME));
        g_object_set(G_OBJECT(dialog), "program-name", _(ALARM_NAME), "title", title, "version", VERSION, NULL);
        g_free(title);

        gtk_dialog_run(GTK_DIALOG(dialog));

    } else {
        // Already visible, present it
        gtk_window_present(GTK_WINDOW(dialog));
    }
}

/*
 * An error callback for MediaPlayers
 */
void media_player_error_cb(MediaPlayer* player, GError* err, gpointer data)
{
    GtkWindow* parent = GTK_WINDOW(data);
    gchar *uri, *tmp;

    uri = media_player_get_uri(player);
    tmp = g_strdup_printf("%s: %s", uri, err->message);

    g_critical(_("Could not play '%s': %s"), uri, err->message);
    display_error_dialog(_("Could not play"), tmp, parent);

    g_free(tmp);
    g_free(uri);
}


/**
 * Alarm changed signal handler
 *
 * Here we update any actions/views, if necessary
 */
void alarm_applet_alarm_changed(GObject* object, GParamSpec* pspec, gpointer data)
{
    AlarmApplet* applet = (AlarmApplet*)data;
    Alarm* alarm = ALARM(object);
    const gchar* pname = pspec->name;

    g_debug("AlarmApplet: Alarm '%s' %s changed", alarm->message, pname);

    // Update Actions
    if(g_strcmp0(pname, "active") == 0) {
        alarm_action_update_enabled(applet);
    }

    // Update List Window
    if(applet->list_window && gtk_widget_get_visible(GTK_WIDGET(applet->list_window->window))) {
        // Should really check that the changed param is relevant...
        alarm_list_window_alarm_update(applet->list_window, alarm);
    }

    // Update Settings
    /*if (applet->settings_dialog && applet->settings_dialog->alarm == alarm) {
        g_debug ("TODO: Update settings dialog");
    }*/
}

/**
 * Alarm 'alarm' signal handler
 *
 * Here we update any actions/views, if necessary
 */
void alarm_applet_alarm_triggered(Alarm* alarm, gpointer data)
{
    AlarmApplet* applet = (AlarmApplet*)data;
    gchar *summary, *body;
    const gchar* icon;

    g_debug("AlarmApplet: Alarm '%s' triggered", alarm->message);

    // Keep track of how many alarms have been triggered
    applet->n_triggered++;

    // Show notification
    summary = g_strdup_printf("%s", alarm->message);
    body = g_strdup_printf(_("You can snooze or stop alarms from the Alarm Clock menu."));
    icon = (alarm->type == ALARM_TYPE_TIMER) ? TIMER_ICON : ALARM_ICON;
    alarm_applet_notification_show(applet, summary, body, icon);

    g_free(summary);
    g_free(body);

    // Update status icon
    alarm_applet_status_update(applet);

    // Update actions
    alarm_applet_actions_update_sensitive(applet);
}

/**
 * Alarm 'cleared' signal handler
 *
 * Here we update any actions/views, if necessary
 */
void alarm_applet_alarm_cleared(Alarm* alarm, gpointer data)
{
    AlarmApplet* applet = (AlarmApplet*)data;

    g_debug("AlarmApplet: Alarm '%s' cleared", alarm->message);

    // Keep track of how many alarms have been triggered
    applet->n_triggered--;

    // Update status icon
    alarm_applet_status_update(applet);

    // Update actions
    alarm_applet_actions_update_sensitive(applet);
}
