/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* UI integration adapted from MoDD0/moddo-evolution-translator, commits
 * 967149e205bcb1632571595846029d3834f17314 and
 * 6865134515f7e52458eadfc62b30cb94f867f404 (LGPL-2.1-or-later).
 * Modified by Andrea Costantino on 2026-11-24. */
/**
 * translate-browser-extension.c
 * Adds translation actions to EMailBrowser windows.
 *
 * This module integrates translation functionality into Evolution's
 * separate mail browser windows (opened via "Open in New Window").
 *
 * Supports both legacy GtkUIManager (Evolution < 3.56) and
 * new EUIManager (Evolution >= 3.56) via compile-time detection.
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
do_translate_browser (TranslateBrowserExtension *self)
{
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));
    EShell *shell = e_shell_get_default ();
    EShellBackend *backend = shell ? e_shell_get_backend_by_name (shell, "mail") : NULL;
    translate_common_translate_reader (reader, backend);
}

static void
do_show_original_browser (TranslateBrowserExtension *self)
{
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));
    translate_common_restore_reader (reader);
}

static void
do_translate_settings_browser (TranslateBrowserExtension *self)
{
    GtkWindow *parent = GTK_WINDOW (e_extension_get_extensible (E_EXTENSION (self)));
    translate_preferences_show (parent);
}

#ifdef HAVE_EUI_MANAGER
/* ============================================================================
 * Evolution >= 3.56: EUIManager / EUIAction API
 * ============================================================================ */

#define BROWSER_ACTION_GROUP "translate-browser-actions"

static void
action_translate_message_cb (EUIAction *action,
                             GVariant  *parameter,
                             gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_translate_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static void
action_show_original_cb (EUIAction *action,
                         GVariant  *parameter,
                         gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_show_original_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static void
action_translate_settings_cb (EUIAction *action,
                              GVariant  *parameter,
                              gpointer   user_data)
{
    (void) action;
    (void) parameter;
    do_translate_settings_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static const EUIActionEntry browser_entries[] = {
    { "translate-menu",
      NULL, N_("_Translate"), NULL, NULL,
      NULL, NULL, NULL, NULL },
    { "translate-message-action",
      NULL, N_("_Translate"), NULL,
      N_("Translate the selected message"),
      action_translate_message_cb, NULL, NULL, NULL },
    { "translate-show-original-action",
      NULL, N_("Show _Original"), NULL,
      N_("Show the original content"),
      action_show_original_cb, NULL, NULL, NULL },
    { "translate-settings-action",
      NULL, N_("Translate _Settings\xe2\x80\xa6"), NULL,
      N_("Configure translation options"),
      action_translate_settings_cb, NULL, NULL, NULL }
};

static void
update_actions_cb (TranslateBrowserExtension *self)
{
    EMailReader *reader = E_MAIL_READER (e_extension_get_extensible (E_EXTENSION (self)));
    gboolean has_message = FALSE;

    translate_dom_clear_if_message_changed_reader (reader);

    if (reader) {
        g_autoptr(GPtrArray) uids = e_mail_reader_get_selected_uids (reader);
        has_message = (uids && uids->len > 0);
    }

    EUIManager *ui_manager = e_mail_reader_get_ui_manager (reader);

    static const gchar *translate_names[] = { "translate-message-action", NULL };
    static const gchar *original_names[] = { "translate-show-original-action", NULL };

    m_utils_enable_actions_by_name (ui_manager, BROWSER_ACTION_GROUP,
                                     translate_names, has_message);
    m_utils_enable_actions_by_name (ui_manager, BROWSER_ACTION_GROUP,
                                     original_names,
                                     translate_dom_is_translated_reader (reader));
}

static void
add_ui (TranslateBrowserExtension *self, EMailBrowser *browser)
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
        "  <toolbar id='main-toolbar-with-headerbar'>"
        "    <placeholder id='mail-toolbar-common'>"
        "      <item action='translate-message-action'/>"
        "    </placeholder>"
        "  </toolbar>"
        "  <toolbar id='main-toolbar-without-headerbar'>"
        "    <placeholder id='mail-toolbar-common'>"
        "      <item action='translate-message-action'/>"
        "    </placeholder>"
        "  </toolbar>"
        "</eui>";

    EMailReader *reader = E_MAIL_READER (browser);
    EUIManager *ui_manager = e_mail_reader_get_ui_manager (reader);
    e_ui_manager_add_actions_with_eui_data (
        ui_manager,
        BROWSER_ACTION_GROUP,
        GETTEXT_PACKAGE,
        browser_entries, G_N_ELEMENTS (browser_entries),
        self,
        eui_def);
    m_utils_configure_translate_action (e_ui_manager_get_action (ui_manager, "translate-message-action"));

    g_signal_connect_swapped (browser, "update-actions",
                              G_CALLBACK (update_actions_cb), self);
    update_actions_cb (self);
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
    do_translate_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static void
action_show_original_cb (GtkAction *action,
                         gpointer   user_data)
{
    (void) action;
    do_show_original_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static const GtkActionEntry browser_entries[] = {
    { "translate-message-action",
      NULL,
      N_("_Translate"),
      NULL,
      N_("Translate the selected message"),
      G_CALLBACK (action_translate_message_cb) },
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
    (void) action;
    do_translate_settings_browser (TRANSLATE_BROWSER_EXTENSION (user_data));
}

static const GtkActionEntry browser_settings_entries[] = {
    { "translate-settings-action",
      GTK_STOCK_PREFERENCES,
      N_("Translate _Settings\xe2\x80\xa6"),
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

    translate_dom_clear_if_message_changed_reader (reader);

    if (reader) {
        g_autoptr(GPtrArray) uids = e_mail_reader_get_selected_uids (reader);
        has_message = (uids && uids->len > 0);
    }
    m_utils_enable_actions (ui_manager, browser_entries, 1, has_message);
    m_utils_enable_actions (ui_manager, browser_entries + 1, 1, translate_dom_is_translated_reader (reader));
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
        "  <toolbar name='main-toolbar'>"
        "    <placeholder name='mail-toolbar-common'>"
        "      <toolitem action='translate-message-action'/>"
        "    </placeholder>"
        "  </toolbar>"
        "</ui>";

    GtkUIManager *ui_manager = e_mail_browser_get_ui_manager (browser);
    GtkActionGroup *group = gtk_action_group_new ("translate-browser-actions");
    gtk_action_group_set_translation_domain (group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (group, browser_entries, G_N_ELEMENTS (browser_entries), self);
    gtk_action_group_add_actions (group, browser_settings_entries, G_N_ELEMENTS (browser_settings_entries), self);
    gtk_ui_manager_insert_action_group (ui_manager, group, 0);
    m_utils_configure_translate_action (gtk_action_group_get_action (group, "translate-message-action"));
    g_object_unref (group);

    GError *error = NULL;
    gtk_ui_manager_add_ui_from_string (ui_manager, eui_def, -1, &error);
    if (error) {
        g_warning ("[translate-browser] Failed to add UI: %s", error->message);
        g_error_free (error);
    }

    g_signal_connect_swapped (browser, "update-actions",
                              G_CALLBACK (update_actions_cb), self);
    update_actions_cb (self);
}

#endif /* HAVE_EUI_MANAGER */

/* ============================================================================
 * Common boilerplate (shared by both API paths)
 * ============================================================================ */

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
