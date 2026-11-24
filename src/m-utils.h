/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Modified by Andrea Costantino on 2026-11-24: shared Translate action setup. */
/* m-utils.h
 * Simple UI manager utilities for enabling/disabling actions.
 * Supports both legacy GtkUIManager (Evolution < 3.56) and
 * new EUIManager (Evolution >= 3.56) via compile-time detection.
 */

#ifndef M_UTILS_H
#define M_UTILS_H

#ifdef HAVE_EUI_MANAGER
#include <e-util/e-util.h>
#else
#include <gtk/gtk.h>
#endif

#ifdef HAVE_EUI_MANAGER

void m_utils_configure_translate_action (EUIAction *action);

/**
 * m_utils_enable_actions_by_name:
 * @ui_manager: The EUIManager containing the actions
 * @group_name: Name of the action group
 * @action_names: NULL-terminated array of action names
 * @enable: TRUE to enable actions, FALSE to disable them
 *
 * Helper to enable or disable a set of UI actions by name (EUIManager path).
 */
void m_utils_enable_actions_by_name (EUIManager  *ui_manager,
                                      const gchar *group_name,
                                      const gchar * const *action_names,
                                      gboolean     enable);

#else /* Legacy GtkUIManager API */

void m_utils_configure_translate_action (GtkAction *action);

/**
 * m_utils_enable_actions:
 * @ui_manager: The UI manager containing the actions
 * @entries: Array of GtkActionEntry structures
 * @n_entries: Number of entries to process
 * @enable: TRUE to enable actions, FALSE to disable them
 *
 * Helper function to enable or disable a set of UI actions.
 */
void m_utils_enable_actions (GtkUIManager *ui_manager,
                              const GtkActionEntry *entries,
                              guint n_entries,
                              gboolean enable);

#endif /* HAVE_EUI_MANAGER */

#endif /* M_UTILS_H */
