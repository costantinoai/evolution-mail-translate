/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-utils.h
 * Common utility functions used across the translate extension
 */

#ifndef TRANSLATE_UTILS_H
#define TRANSLATE_UTILS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * translate_utils_get_target_language:
 *
 * Gets the target language setting from GSettings.
 * Returns the configured target language code, or "en" as default.
 *
 * Returns: (transfer full): A newly allocated string containing the target
 *          language code. Free with g_free().
 */
gchar *translate_utils_get_target_language (void);

/**
 * translate_utils_get_settings:
 *
 * Gets the GSettings object for the translate extension.
 * This function caches the settings object internally.
 *
 * Returns: (transfer none): The GSettings object for "org.gnome.evolution.translate"
 */
GSettings *translate_utils_get_settings (void);

/**
 * translate_utils_get_provider_settings:
 *
 * Gets the GSettings object for provider-specific settings.
 * This function caches the settings object internally.
 *
 * Returns: (transfer none): The GSettings object for "org.gnome.evolution.translate.provider"
 */
GSettings *translate_utils_get_provider_settings (void);

/**
 * translate_utils_get_install_on_demand:
 *
 * Gets the install-on-demand setting for automatic model downloads.
 * If not configured, returns TRUE as the default.
 *
 * Returns: TRUE if automatic model installation is enabled, FALSE otherwise
 */
gboolean translate_utils_get_install_on_demand (void);

/**
 * translate_utils_get_provider_id:
 *
 * Gets the provider ID setting from GSettings.
 * Returns the configured provider ID, or "argos" as default.
 *
 * Returns: (transfer full): A newly allocated string containing the provider
 *          ID. Free with g_free().
 */
gchar *translate_utils_get_provider_id (void);

G_END_DECLS

#endif /* TRANSLATE_UTILS_H */
