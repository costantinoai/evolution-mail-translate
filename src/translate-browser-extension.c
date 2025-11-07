/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-browser-extension.c
 * Adds translation actions to EMailBrowser windows.
 *
 * This module integrates translation functionality into Evolution's
 * separate mail browser windows (opened via "Open in New Window").
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <mail/e-mail-browser.h>
#include <mail/e-mail-reader.h>
#include <shell/e-shell.h>

#include "translate-browser-extension.h"
#include "translate-common.h"
#include "providers/translate-provider.h"
#include "translate-content.h"
#include "translate-dom.h"
#include "translate-preferences.h"
#include "m-utils.h"

G_DEFINE_DYNAMIC_TYPE(TranslateBrowserExtension, translate_browser_extension, E_TYPE_EXTENSION)

static void
on_translate_finished_browser (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
    TranslateProvider *provider = (TranslateProvider*)source_object;
    EMailReader *reader = E_MAIL_READER (user_data);
    g_autofree gchar *translated = NULL;
    g_autoptr(GError) error = NULL;
    if (!translate_provider_translate_finish (provider, res, &translated, &error)) {
        g_warning ("Translate failed: %s", error ? error->message : "unknown error");
        return;
    }
    /* Apply into this reader's display */
    translate_dom_apply_to_reader (reader, translated);
}

/**
 * action_translate_message_cb:
 * @action: The GTK action that triggered this callback
 * @user_data: The TranslateBrowserExtension instance
 *
 * Handles the "Translate Message" action in the browser window.
 * Extracts the current message body and initiates translation
 * using the common translation logic with status bar feedback.
 */
static void
action_translate_message_cb (GtkAction *action,
                             gpointer   user_data)
{
    (void)action;  /* Unused parameter */
    TranslateBrowserExtension *self = user_data;
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));

    /* Toggle behavior: if already translated, restore original */
    if (translate_dom_is_translated_reader (reader)) {
        translate_dom_restore_original_reader (reader);
        return;
    }

    /* Extract the message body HTML */
    g_autofree gchar *body_html = translate_get_selected_message_body_html_from_reader (reader);
    if (!body_html || !*body_html)
        return;

    /* Get the shell backend for activity display */
    EShell *shell = e_shell_get_default ();
    EShellBackend *shell_backend = e_shell_get_backend_by_name (shell, "mail");

    /* Use the centralized translation logic with activity feedback */
    translate_common_translate_async_with_activity (body_html,
                                                     shell_backend,
                                                     on_translate_finished_browser,
                                                     reader);
}

static void
action_show_original_cb (GtkAction *action,
                         gpointer   user_data)
{
    TranslateBrowserExtension *self = user_data;
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));
    translate_dom_restore_original_reader (reader);
}

static const GtkActionEntry browser_entries[] = {
    { "translate-message-action",
      GTK_STOCK_ADD,
      N_("_Translate"),
      NULL,
      N_("Translate the selected message"),
      G_CALLBACK (action_translate_message_cb) },
    { "translate-show-original-action",
      GTK_STOCK_REFRESH,
      N_("Show _Original"),
      NULL,
      N_("Show the original content"),
      G_CALLBACK (action_show_original_cb) }
};

static void
action_translate_settings_cb (GtkAction *action,
                              gpointer   user_data)
{
    TranslateBrowserExtension *self = user_data;
    GtkWindow *parent = GTK_WINDOW (e_extension_get_extensible (E_EXTENSION (self)));
    translate_preferences_show (parent);
}

static const GtkActionEntry browser_settings_entries[] = {
    { "translate-settings-action",
      GTK_STOCK_PREFERENCES,
      N_("Translate _Settings…"),
      NULL,
      N_("Configure translation options"),
      G_CALLBACK (action_translate_settings_cb) }
};

static void
update_actions_cb (TranslateBrowserExtension *self)
{
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));
    GtkUIManager *ui_manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (reader));
    gboolean has_message = FALSE;

    /* Clear translation state if the displayed message has changed */
    translate_dom_clear_if_message_changed_reader (reader);

    if (reader) {
        g_autoptr(GPtrArray) uids = e_mail_reader_get_selected_uids (reader);
        has_message = (uids && uids->len > 0);
    }
    m_utils_enable_actions (ui_manager, browser_entries, 1 /* first action only */, has_message);
    m_utils_enable_actions (ui_manager, browser_entries + 1, 1 /* show original */, translate_dom_is_translated_reader (reader));
}

static void
add_ui (TranslateBrowserExtension *self, EMailBrowser *browser)
{
    const gchar *eui_def =
        "<ui>"
        "  <menubar name='main-menu'>"
        "    <menu action='view-menu'>"
        "      <placeholder name='view-menu-actions'>"
        "        <menuitem action='translate-message-action'/>"
        "        <menuitem action='translate-show-original-action'/>"
        "      </placeholder>"
        "    </menu>"
        "  </menubar>"
        "</ui>";

    GtkUIManager *ui_manager = e_mail_browser_get_ui_manager (browser);
    GtkActionGroup *group = gtk_action_group_new ("translate-browser-actions");
    gtk_action_group_set_translation_domain (group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (group, browser_entries, G_N_ELEMENTS (browser_entries), self);
    gtk_action_group_add_actions (group, browser_settings_entries, G_N_ELEMENTS (browser_settings_entries), self);
    gtk_ui_manager_insert_action_group (ui_manager, group, 0);
    g_object_unref (group);

    GError *error = NULL;
    gtk_ui_manager_add_ui_from_string (ui_manager, eui_def, -1, &error);
    if (error) {
        g_warning ("[translate-browser] Failed to add UI: %s", error->message);
        g_error_free (error);
    }

    /* Update enablement on focus/selection changes */
    g_signal_connect_swapped (browser, "update-actions",
                              G_CALLBACK (update_actions_cb), self);
    update_actions_cb (self);
}

static void
translate_browser_extension_constructed (GObject *object)
{
    G_OBJECT_CLASS (translate_browser_extension_parent_class)->constructed (object);
    EExtensible *ext = e_extension_get_extensible (E_EXTENSION (object));
    add_ui (TRANSLATE_BROWSER_EXTENSION (object), E_MAIL_BROWSER (ext));
}

static void
translate_browser_extension_class_init (TranslateBrowserExtensionClass *klass)
{
    GObjectClass *obj_class = G_OBJECT_CLASS (klass);
    EExtensionClass *ext_class = E_EXTENSION_CLASS (klass);
    obj_class->constructed = translate_browser_extension_constructed;
    ext_class->extensible_type = E_TYPE_MAIL_BROWSER;
}

static void
translate_browser_extension_class_finalize (TranslateBrowserExtensionClass *klass)
{
    (void)klass;
}

static void
translate_browser_extension_init (TranslateBrowserExtension *self)
{
    (void)self;
}

void
translate_browser_extension_type_register (GTypeModule *type_module)
{
    translate_browser_extension_register_type (type_module);
}
