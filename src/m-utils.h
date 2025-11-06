/* m-utils.h
 * Simple UI manager utilities for enabling/disabling actions
 */

#ifndef M_UTILS_H
#define M_UTILS_H

#include <gtk/gtk.h>

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

#endif /* M_UTILS_H */
