/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-common.c
 * Common translation logic shared across UI components
 *
 * This module centralizes the translation request logic that was previously
 * duplicated in translate-mail-ui.c and translate-browser-extension.c.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include "translate-common.h"
#include "translate-utils.h"
#include "providers/translate-provider.h"

/**
 * translate_common_translate_async:
 * @body_html: The HTML content to translate
 * @callback: (scope async): Callback to invoke when translation completes
 * @user_data: User data to pass to the callback
 *
 * Initiates an asynchronous translation of the provided HTML content.
 *
 * This is the centralized translation request function that handles:
 * 1. Validating input
 * 2. Retrieving target language from settings (via translate_utils)
 * 3. Creating the translation provider ("argos" by default)
 * 4. Initiating the async translation with proper parameters
 * 5. Proper memory management (no leaks!)
 *
 * The callback signature should be:
 *   void callback (GObject *source_object, GAsyncResult *result, gpointer user_data)
 *
 * In your callback, use translate_provider_translate_finish() to retrieve
 * the translated text.
 */
void
translate_common_translate_async (const gchar         *body_html,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    g_return_if_fail (body_html != NULL);
    g_return_if_fail (*body_html != '\0');
    g_return_if_fail (callback != NULL);

    /* Get target language from settings - properly managed memory */
    g_autofree gchar *target_lang = translate_utils_get_target_language ();

    /* Get provider ID from settings */
    g_autofree gchar *provider_id = translate_utils_get_provider_id ();
    if (!provider_id || !*provider_id) {
        provider_id = g_strdup ("argos");  /* Default to argos if not set */
    }

    /* Create the translation provider */
    g_autoptr(GObject) provider_obj = translate_provider_new_by_id (provider_id);
    if (!provider_obj) {
        g_warning ("[translate] No provider found for '%s'", provider_id);
        return;
    }

    /* Initiate async translation
     * Note: The provider will copy the target_lang internally, so we don't leak it.
     * Our g_autofree will clean it up when this function exits.
     */
    translate_provider_translate_async ((TranslateProvider*)provider_obj,
                                        body_html,
                                        TRUE,  /* is_html */
                                        NULL,  /* source (auto-detect) */
                                        target_lang,
                                        NULL,  /* cancellable */
                                        callback,
                                        user_data);

    /* Note: provider_obj stays alive during the async operation because
     * translate_provider_translate_async takes a reference to it internally.
     * Our g_autoptr will release our reference when this function exits,
     * but the async operation holds its own reference until completion.
     */
}
