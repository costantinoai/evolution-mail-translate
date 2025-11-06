/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* m-utils.c
 * Simple UI manager utilities for enabling/disabling actions
 */

#include "m-utils.h"

/**
 * m_utils_enable_actions:
 * @ui_manager: The UI manager containing the actions
 * @entries: Array of GtkActionEntry structures
 * @n_entries: Number of entries to process
 * @enable: TRUE to enable actions, FALSE to disable them
 *
 * Helper function to enable or disable a set of UI actions.
 * Iterates through the action entries and sets their sensitivity.
 */
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

    /* Get the first action group (or iterate all if needed) */
    GList *groups = gtk_ui_manager_get_action_groups (ui_manager);
    if (!groups)
        return;

    for (i = 0; i < n_entries; i++) {
        /* Search for the action in all action groups */
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
