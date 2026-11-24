/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* m-utils.c
 * Simple UI manager utilities for enabling/disabling actions.
 * Supports both legacy GtkUIManager and new EUIManager APIs.
 */

#include "m-utils.h"
#include "translate-utils.h"

/* Shortcut integration adapted from MoDD0/moddo-evolution-translator,
 * commit 6865134515f7e52458eadfc62b30cb94f867f404 (LGPL-2.1-or-later).
 * Modified by Andrea Costantino on 2026-11-24: share live bindings across
 * both UI APIs and disconnect settings callbacks when their action dies. */
static void
update_translate_shortcut (GSettings *settings, const gchar *key, gpointer action)
{
    (void) key;
    g_autofree gchar *accel = g_settings_get_string (settings, "translate-shortcut");
    guint keyval;
    GdkModifierType modifiers;
    gtk_accelerator_parse (accel, &keyval, &modifiers);
    if (*accel && !gtk_accelerator_valid (keyval, modifiers)) {
        g_autoptr(GVariant) fallback = g_settings_get_default_value (settings, "translate-shortcut");
        g_free (accel);
        accel = g_variant_dup_string (fallback, NULL);
        gtk_accelerator_parse (accel, &keyval, &modifiers);
    }
#ifdef HAVE_EUI_MANAGER
    e_ui_action_set_accel (E_UI_ACTION (action), *accel ? accel : NULL);
#else
    const gchar *path = gtk_action_get_accel_path (GTK_ACTION (action));
    if (path)
        gtk_accel_map_change_entry (path, keyval, modifiers, TRUE);
#endif
}

void
m_utils_configure_translate_action (
#ifdef HAVE_EUI_MANAGER
    EUIAction *action)
#else
    GtkAction *action)
#endif
{
    if (!action)
        return;
#ifdef HAVE_EUI_MANAGER
    e_ui_action_set_icon_name (action, "accessories-dictionary-symbolic");
#else
    gtk_action_set_icon_name (action, "accessories-dictionary-symbolic");
#endif
    GSettings *settings = translate_utils_get_settings ();
    update_translate_shortcut (settings, NULL, action);
    g_signal_connect_object (settings, "changed::translate-shortcut",
                             G_CALLBACK (update_translate_shortcut), action, 0);
}

#ifdef HAVE_EUI_MANAGER

void
m_utils_enable_actions_by_name (EUIManager  *ui_manager,
                                 const gchar *group_name,
                                 const gchar * const *action_names,
                                 gboolean     enable)
{
    EUIActionGroup *group;

    if (!ui_manager || !group_name || !action_names)
        return;

    group = e_ui_manager_get_action_group (ui_manager, group_name);
    if (!group)
        return;

    for (guint i = 0; action_names[i] != NULL; i++) {
        EUIAction *action = e_ui_action_group_get_action (group, action_names[i]);
        if (action)
            e_ui_action_set_sensitive (action, enable);
    }
}

#else /* Legacy GtkUIManager API */

void
m_utils_enable_actions (GtkUIManager *ui_manager,
                        const GtkActionEntry *entries,
                        guint n_entries,
                        gboolean enable)
{
    GtkActionGroup *action_group;
    GtkAction *action;
    guint i;

    if (!ui_manager)
        return;

    GList *groups = gtk_ui_manager_get_action_groups (ui_manager);
    if (!groups)
        return;

    for (i = 0; i < n_entries; i++) {
        GList *group_iter;
        for (group_iter = groups; group_iter; group_iter = group_iter->next) {
            action_group = GTK_ACTION_GROUP (group_iter->data);
            action = gtk_action_group_get_action (action_group, entries[i].name);

            if (action) {
                gtk_action_set_sensitive (action, enable);
                break;
            }
        }
    }
}

#endif /* HAVE_EUI_MANAGER */
