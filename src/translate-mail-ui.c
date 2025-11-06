/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-mail-ui.c
 * Adds translation actions to the Message menu in Mail view.
 *
 * This module integrates translation functionality into Evolution's
 * main mail view interface, adding menu items and toolbar buttons.
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

static inline gchar *
get_selected_message_body_html (EShellView *shell_view)
{
    return translate_get_selected_message_body_html_from_shell_view (shell_view);
}

static void
on_translate_finished (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    TranslateProvider *provider = (TranslateProvider*)source_object;
    EShellView *shell_view = E_SHELL_VIEW (user_data);
    g_autofree gchar *translated = NULL;
    g_autoptr(GError) error = NULL;

    if (!translate_provider_translate_finish (provider, res, &translated, &error)) {
        g_warning ("Translate failed: %s", error ? error->message : "unknown error");
        return;
    }

    translate_dom_apply_to_shell_view (shell_view, translated);
}

/**
 * action_translate_message_cb:
 * @action: The GTK action that triggered this callback
 * @user_data: The EShellView instance
 *
 * Handles the "Translate Message" action from the menu/toolbar.
 * Extracts the current message body and initiates translation
 * using the common translation logic.
 */
static void
action_translate_message_cb (GtkAction *action,
                             gpointer   user_data)
{
    EShellView *shell_view = user_data;
    g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

    /* Toggle behavior: if already translated, restore original */
    if (translate_dom_is_translated (shell_view)) {
        translate_dom_restore_original (shell_view);
        return;
    }

    /* Extract the message body HTML */
    g_autofree gchar *body_html = get_selected_message_body_html (shell_view);
    if (!body_html || !*body_html) {
        g_message ("[translate] No message body available to translate");
        return;
    }

    /* Use the centralized translation logic (fixes memory leak) */
    translate_common_translate_async (body_html,
                                      on_translate_finished,
                                      shell_view);
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
      GTK_STOCK_ADD,
      N_("_Translate Message"),
      "<Control><Shift>T",
      N_("Translate the selected message"),
      G_CALLBACK (action_translate_message_cb) }
};

static void
action_show_original_cb (GtkAction *action,
                         gpointer   user_data)
{
    EShellView *shell_view = user_data;
    translate_dom_restore_original (shell_view);
}

static const GtkActionEntry translate_show_original_entries[] = {
    { "translate-show-original-action",
      GTK_STOCK_REFRESH,
      N_("Show _Original"),
      "<Control><Shift>O",
      N_("Show the original content"),
      G_CALLBACK (action_show_original_cb) }
};

static void
action_translate_settings_cb (GtkAction *action,
                              gpointer   user_data)
{
    EShellView *shell_view = user_data;
    EShellWindow *sw = e_shell_view_get_shell_window (shell_view);
    GtkWindow *parent = sw ? GTK_WINDOW (sw) : NULL;
    translate_preferences_show (parent);
}

static const GtkActionEntry translate_settings_entries[] = {
    { "translate-settings-action",
      GTK_STOCK_PREFERENCES,
      N_("Translate _Settings…"),
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

    /* Clear translation state if the displayed message has changed */
    translate_dom_clear_if_message_changed (shell_view);

    shell_content = e_shell_view_get_shell_content (shell_view);
    g_object_get (shell_content, "mail-view", &mail_view, NULL);
    if (E_IS_MAIL_PANED_VIEW (mail_view)) {
        GtkWidget *message_list;
        message_list = e_mail_reader_get_message_list (E_MAIL_READER (mail_view));
        has_message = message_list_selected_count (MESSAGE_LIST (message_list)) > 0;
    }

    EShellWindow *sw = e_shell_view_get_shell_window (shell_view);
    ui_manager = sw ? e_shell_window_get_ui_manager (sw) : NULL;
    m_utils_enable_actions (ui_manager,
                            translate_message_menu_entries,
                            G_N_ELEMENTS (translate_message_menu_entries),
                            has_message);

    /* Enable 'Show Original' when a translation is currently applied */
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
        "  <toolbar name='mail-toolbar'>"
        "    <placeholder name='mail-toolbar-actions'>"
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
