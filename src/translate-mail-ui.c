/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* UI integration adapted from MoDD0/moddo-evolution-translator, commits
 * 967149e205bcb1632571595846029d3834f17314 and
 * 6865134515f7e52458eadfc62b30cb94f867f404 (LGPL-2.1-or-later).
 * Modified by Andrea Costantino on 2026-11-24. */
/**
 * translate-mail-ui.c
 * Adds translation actions to the Message menu in Mail view.
 *
 * This module integrates translation functionality into Evolution's
 * main mail view interface, adding menu items and toolbar buttons.
 *
 * Supports both legacy GtkUIManager (Evolution < 3.56) and
 * new EUIManager (Evolution >= 3.56) via compile-time detection.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include <mail/e-mail-reader.h>
#include <mail/e-mail-view.h>
#include <mail/e-mail-paned-view.h>
#include <mail/message-list.h>
#include <gio/gio.h>
#include <e-util/e-util.h>

#include "translate-mail-ui.h"
#include "translate-common.h"
#include "providers/translate-provider.h"
#include "translate-dom.h"
#include "translate-content.h"
#include "translate-preferences.h"
#include "m-utils.h"

static void
do_translate_message (EShellView *shell_view)
{
    EMailReader *reader = translate_ref_reader_from_shell_view (shell_view);
    if (reader) {
        translate_common_translate_reader (reader, e_shell_view_get_shell_backend (shell_view));
        g_object_unref (reader);
    }
}

static void
do_show_original (EShellView *shell_view)
{
    EMailReader *reader = translate_ref_reader_from_shell_view (shell_view);
    if (reader) {
        translate_common_restore_reader (reader);
        g_object_unref (reader);
    }
}

static void
do_translate_settings (EShellView *shell_view)
{
    EShellWindow *sw = e_shell_view_get_shell_window (shell_view);
    GtkWindow *parent = sw ? GTK_WINDOW (sw) : NULL;
    translate_preferences_show (parent);
}

#ifdef HAVE_EUI_MANAGER
/* ============================================================================
 * Evolution >= 3.56: EUIManager / EUIAction API
 * ============================================================================ */

#define TRANSLATE_ACTION_GROUP "translate-mail-actions"

static void
action_translate_message_cb (EUIAction *action,
                             GVariant  *parameter,
                             gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_translate_message (E_SHELL_VIEW (user_data));
}

static void
action_show_original_cb (EUIAction *action,
                         GVariant  *parameter,
                         gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_show_original (E_SHELL_VIEW (user_data));
}

static void
action_translate_settings_cb (EUIAction *action,
                              GVariant  *parameter,
                              gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_translate_settings (E_SHELL_VIEW (user_data));
}

static const EUIActionEntry translate_entries[] = {
    { "translate-menu",
      NULL, N_("_Translate"), NULL, NULL,
      NULL, NULL, NULL, NULL },
    { "translate-message-action",
      NULL, N_("_Translate Message"), NULL,
      N_("Translate the selected message"),
      action_translate_message_cb, NULL, NULL, NULL },
    { "translate-show-original-action",
      NULL, N_("Show _Original"), "<Control><Shift>O",
      N_("Show the original content"),
      action_show_original_cb, NULL, NULL, NULL },
    { "translate-settings-action",
      NULL, N_("Translate _Settings\xe2\x80\xa6"), NULL,
      N_("Configure translation options"),
      action_translate_settings_cb, NULL, NULL, NULL }
};

static void
translate_mail_ui_update_actions_cb (EShellView *shell_view)
{
    EShellContent *shell_content;
    EMailView *mail_view = NULL;
    gboolean has_message = FALSE;

    translate_dom_clear_if_message_changed (shell_view);

    shell_content = e_shell_view_get_shell_content (shell_view);
    g_object_get (shell_content, "mail-view", &mail_view, NULL);
    if (E_IS_MAIL_PANED_VIEW (mail_view)) {
        GtkWidget *message_list;
        message_list = e_mail_reader_get_message_list (E_MAIL_READER (mail_view));
        has_message = message_list_selected_count (MESSAGE_LIST (message_list)) > 0;
    }
    if (mail_view)
        g_object_unref (mail_view);

    EUIManager *ui_manager = e_shell_view_get_ui_manager (shell_view);

    static const gchar *translate_names[] = { "translate-message-action", NULL };
    static const gchar *original_names[] = { "translate-show-original-action", NULL };
    static const gchar *settings_names[] = { "translate-settings-action", NULL };

    m_utils_enable_actions_by_name (ui_manager, TRANSLATE_ACTION_GROUP,
                                     translate_names, has_message);
    m_utils_enable_actions_by_name (ui_manager, TRANSLATE_ACTION_GROUP,
                                     original_names,
                                     translate_dom_is_translated (shell_view));
    m_utils_enable_actions_by_name (ui_manager, TRANSLATE_ACTION_GROUP,
                                     settings_names, TRUE);
}

void
translate_mail_ui_init (EShellView *shell_view)
{
    const gchar *eui_def =
        "<eui>"
        "  <menu id='main-menu'>"
        "    <placeholder id='custom-menus'>"
        "      <submenu action='translate-menu'>"
        "        <item action='translate-message-action'/>"
        "        <item action='translate-show-original-action'/>"
        "        <separator/>"
        "        <item action='translate-settings-action'/>"
        "      </submenu>"
        "    </placeholder>"
        "  </menu>"
        "  <toolbar id='mail-preview-toolbar'>"
        "    <placeholder id='mail-toolbar-common'>"
        "      <item action='translate-message-action'/>"
        "    </placeholder>"
        "  </toolbar>"
        "</eui>";

    g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

    EUIManager *ui_manager = e_shell_view_get_ui_manager (shell_view);
    e_ui_manager_add_actions_with_eui_data (
        ui_manager,
        TRANSLATE_ACTION_GROUP,
        GETTEXT_PACKAGE,
        translate_entries, G_N_ELEMENTS (translate_entries),
        shell_view,
        eui_def);
    m_utils_configure_translate_action (e_ui_manager_get_action (ui_manager, "translate-message-action"));

    g_signal_connect (shell_view, "update-actions",
                      G_CALLBACK (translate_mail_ui_update_actions_cb), NULL);
}

#else /* Legacy GtkUIManager API (Evolution < 3.56) */
/* ============================================================================
 * Evolution < 3.56: GtkUIManager / GtkAction API
 * ============================================================================ */

static void
action_translate_message_cb (GtkAction *action,
                             gpointer   user_data)
{
    (void) action;
    do_translate_message (E_SHELL_VIEW (user_data));
}

static void
action_show_original_cb (GtkAction *action,
                         gpointer   user_data)
{
    (void) action;
    do_show_original (E_SHELL_VIEW (user_data));
}

static void
action_translate_settings_cb (GtkAction *action,
                              gpointer   user_data)
{
    (void) action;
    do_translate_settings (E_SHELL_VIEW (user_data));
}

static const GtkActionEntry translate_menu_action[] = {
    { "translate-menu",
      NULL,
      N_("_Translate"),
      NULL,
      NULL,
      NULL }
};

static const GtkActionEntry translate_message_menu_entries[] = {
    { "translate-message-action",
      NULL,
      N_("_Translate Message"),
      NULL,
      N_("Translate the selected message"),
      G_CALLBACK (action_translate_message_cb) }
};

static const GtkActionEntry translate_show_original_entries[] = {
    { "translate-show-original-action",
      GTK_STOCK_REFRESH,
      N_("Show _Original"),
      "<Control><Shift>O",
      N_("Show the original content"),
      G_CALLBACK (action_show_original_cb) }
};

static const GtkActionEntry translate_settings_entries[] = {
    { "translate-settings-action",
      GTK_STOCK_PREFERENCES,
      N_("Translate _Settings\xe2\x80\xa6"),
      NULL,
      N_("Configure translation options"),
      G_CALLBACK (action_translate_settings_cb) }
};

static void
translate_mail_ui_update_actions_cb (EShellView *shell_view)
{
    EShellContent *shell_content;
    EMailView *mail_view = NULL;
    GtkUIManager *ui_manager;
    gboolean has_message = FALSE;

    translate_dom_clear_if_message_changed (shell_view);

    shell_content = e_shell_view_get_shell_content (shell_view);
    g_object_get (shell_content, "mail-view", &mail_view, NULL);
    if (E_IS_MAIL_PANED_VIEW (mail_view)) {
        GtkWidget *message_list;
        message_list = e_mail_reader_get_message_list (E_MAIL_READER (mail_view));
        has_message = message_list_selected_count (MESSAGE_LIST (message_list)) > 0;
    }
    if (mail_view)
        g_object_unref (mail_view);

    EShellWindow *sw = e_shell_view_get_shell_window (shell_view);
    ui_manager = sw ? e_shell_window_get_ui_manager (sw) : NULL;
    m_utils_enable_actions (ui_manager,
                            translate_message_menu_entries,
                            G_N_ELEMENTS (translate_message_menu_entries),
                            has_message);

    m_utils_enable_actions (ui_manager,
                            translate_show_original_entries,
                            G_N_ELEMENTS (translate_show_original_entries),
                            translate_dom_is_translated (shell_view));
    m_utils_enable_actions (ui_manager,
                            translate_settings_entries,
                            G_N_ELEMENTS (translate_settings_entries),
                            TRUE);
}

void
translate_mail_ui_init (EShellView *shell_view)
{
    const gchar *eui_def =
        "<ui>"
        "  <menubar name='main-menu'>"
        "    <menu action='translate-menu'>"
        "      <menuitem action='translate-message-action'/>"
        "      <menuitem action='translate-show-original-action'/>"
        "      <separator/>"
        "      <menuitem action='translate-settings-action'/>"
        "    </menu>"
        "  </menubar>"
        "  <toolbar name='main-toolbar'>"
        "    <placeholder name='mail-toolbar-common'>"
        "      <toolitem action='translate-message-action'/>"
        "    </placeholder>"
        "  </toolbar>"
        "</ui>";

    GtkUIManager *ui_manager;

    g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
    EShellWindow *sw = e_shell_view_get_shell_window (shell_view);
    ui_manager = sw ? e_shell_window_get_ui_manager (sw) : NULL;

    GtkActionGroup *group = gtk_action_group_new ("translate-mail-actions");
    gtk_action_group_set_translation_domain (group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (group,
                                  translate_menu_action,
                                  G_N_ELEMENTS (translate_menu_action),
                                  shell_view);
    gtk_action_group_add_actions (group,
                                  translate_message_menu_entries,
                                  G_N_ELEMENTS (translate_message_menu_entries),
                                  shell_view);
    gtk_action_group_add_actions (group,
                                  translate_show_original_entries,
                                  G_N_ELEMENTS (translate_show_original_entries),
                                  shell_view);
    gtk_action_group_add_actions (group,
                                  translate_settings_entries,
                                  G_N_ELEMENTS (translate_settings_entries),
                                  shell_view);
    gtk_ui_manager_insert_action_group (ui_manager, group, 0);
    m_utils_configure_translate_action (gtk_action_group_get_action (group, "translate-message-action"));
    g_object_unref (group);

    GError *error = NULL;
    gtk_ui_manager_add_ui_from_string (ui_manager, eui_def, -1, &error);
    if (error) {
        g_warning ("[translate] Failed to add UI: %s", error->message);
        g_error_free (error);
    }

    g_signal_connect (shell_view, "update-actions",
                      G_CALLBACK (translate_mail_ui_update_actions_cb), NULL);
}

#endif /* HAVE_EUI_MANAGER */
