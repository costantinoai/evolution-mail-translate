/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-utils.c
 * Common utility functions used across the translate extension
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "translate-utils.h"

/* Cached GSettings instances */
static GSettings *settings_cache = NULL;
static GSettings *provider_settings_cache = NULL;

/**
 * translate_utils_get_settings:
 *
 * Gets the GSettings object for the translate extension.
 * This function caches the settings object internally to avoid
 * creating multiple instances.
 *
 * Returns: (transfer none): The GSettings object for "org.gnome.evolution.translate"
 */
GSettings *
translate_utils_get_settings (void)
{
    if (!settings_cache) {
        settings_cache = g_settings_new ("org.gnome.evolution.translate");
    }
    return settings_cache;
}

/**
 * translate_utils_get_target_language:
 *
 * Gets the target language setting from GSettings.
 * If no language is configured or the setting is empty,
 * returns "en" (English) as the default.
 *
 * Returns: (transfer full): A newly allocated string containing the target
 *          language code. Free with g_free() when done.
 */
gchar *
translate_utils_get_target_language (void)
{
    GSettings *settings = translate_utils_get_settings ();
    g_autofree gchar *target_lang = NULL;

    if (settings) {
        target_lang = g_settings_get_string (settings, "target-language");
    }

    /* Return configured language or default to "en" */
    if (target_lang && *target_lang) {
        return g_strdup (target_lang);
    }

    return g_strdup ("en");
}

/**
 * translate_utils_get_provider_settings:
 *
 * Gets the GSettings object for provider-specific settings.
 * This function caches the settings object internally to avoid
 * creating multiple instances.
 *
 * Returns: (transfer none): The GSettings object for "org.gnome.evolution.translate.provider"
 */
GSettings *
translate_utils_get_provider_settings (void)
{
    if (!provider_settings_cache) {
        provider_settings_cache = g_settings_new ("org.gnome.evolution.translate.provider");
    }
    return provider_settings_cache;
}

/**
 * translate_utils_get_install_on_demand:
 *
 * Gets the install-on-demand setting for automatic model downloads.
 * If not configured, returns TRUE as the default (auto-install is enabled by default).
 *
 * Returns: TRUE if automatic model installation is enabled, FALSE otherwise
 */
gboolean
translate_utils_get_install_on_demand (void)
{
    GSettings *provider_settings = translate_utils_get_provider_settings ();

    if (provider_settings) {
        return g_settings_get_boolean (provider_settings, "install-on-demand");
    }

    /* Default: enable install-on-demand */
    return TRUE;
}

/**
 * translate_utils_get_provider_id:
 *
 * Gets the provider ID setting from GSettings.
 * If no provider is configured or the setting is empty,
 * returns "google" as the default (free online provider).
 *
 * Returns: (transfer full): A newly allocated string containing the provider
 *          ID. Free with g_free() when done.
 */
gchar *
translate_utils_get_provider_id (void)
{
    GSettings *settings = translate_utils_get_settings ();
    g_autofree gchar *provider_id = NULL;

    if (settings) {
        provider_id = g_settings_get_string (settings, "provider-id");
    }

    /* Return configured provider or default to "google" */
    if (provider_id && *provider_id) {
        return g_strdup (provider_id);
    }

    return g_strdup ("google");
}
