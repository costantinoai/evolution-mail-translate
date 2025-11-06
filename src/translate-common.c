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
#include <e-util/e-activity.h>
#include <shell/e-shell-backend.h>
#include "translate-common.h"
#include "translate-utils.h"
#include "providers/translate-provider.h"

/**
 * TranslateAsyncData:
 * Internal structure for managing async translation with activity feedback
 */
typedef struct {
    GAsyncReadyCallback user_callback;
    gpointer user_data;
    EActivity *activity;
    gchar *provider_name;
} TranslateAsyncData;

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
        provider_id = g_strdup ("google");  /* Default to google if not set */
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

/**
 * internal_translate_callback:
 * Internal callback that wraps user callback and updates activity status
 */
static void
internal_translate_callback (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
    TranslateAsyncData *data = (TranslateAsyncData*)user_data;
    TranslateProvider *provider = (TranslateProvider*)source_object;

    g_autofree gchar *translated = NULL;
    g_autoptr(GError) error = NULL;

    gboolean success = translate_provider_translate_finish (
        provider, res, &translated, &error
    );

    if (data->activity) {
        if (success) {
            /* Update activity to show completion */
            e_activity_set_state (data->activity, E_ACTIVITY_COMPLETED);

            g_autofree gchar *msg = g_strdup_printf (
                "Text translated by %s",
                data->provider_name ? data->provider_name : "translator"
            );
            e_activity_set_text (data->activity, msg);
        } else {
            /* Handle error */
            e_activity_set_state (data->activity, E_ACTIVITY_CANCELLED);
            g_autofree gchar *error_msg = g_strdup_printf (
                "Translation failed: %s",
                error ? error->message : "unknown error"
            );
            e_activity_set_text (data->activity, error_msg);
        }
    }

    /* Call user's original callback */
    if (data->user_callback) {
        data->user_callback (source_object, res, data->user_data);
    }

    /* Cleanup */
    g_free (data->provider_name);
    g_free (data);
}

/**
 * translate_common_translate_async_with_activity:
 * @body_html: The HTML content to translate
 * @shell_backend: EShellBackend to display activity status
 * @callback: (scope async): Callback to invoke when translation completes
 * @user_data: User data to pass to the callback
 *
 * Initiates an asynchronous translation with status bar activity feedback.
 * Shows progress messages in the Evolution status bar:
 * 1. "Requesting translation from <provider>..."
 * 2. "Translation request sent. Waiting for response..."
 * 3. "Text translated by <provider>." (on success)
 *
 * This function provides the same functionality as translate_common_translate_async
 * but adds visual feedback via the EActivity system.
 */
void
translate_common_translate_async_with_activity (const gchar         *body_html,
                                                 EShellBackend       *shell_backend,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
    g_return_if_fail (body_html != NULL);
    g_return_if_fail (*body_html != '\0');
    g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

    /* Get target language from settings */
    g_autofree gchar *target_lang = translate_utils_get_target_language ();

    /* Get provider ID from settings */
    g_autofree gchar *provider_id = translate_utils_get_provider_id ();
    if (!provider_id || !*provider_id) {
        provider_id = g_strdup ("google");
    }

    /* Create the translation provider */
    g_autoptr(GObject) provider_obj = translate_provider_new_by_id (provider_id);
    if (!provider_obj) {
        g_warning ("[translate] No provider found for '%s'", provider_id);
        return;
    }

    /* Get provider display name for status messages */
    const gchar *provider_name = translate_provider_get_name ((TranslateProvider*)provider_obj);
    if (!provider_name || !*provider_name) {
        provider_name = provider_id;
    }

    /* Create activity for status display */
    EActivity *activity = e_activity_new ();

    /* Show initial status message */
    g_autofree gchar *initial_msg = g_strdup_printf (
        "Requesting translation from %s...",
        provider_name
    );
    e_activity_set_text (activity, initial_msg);
    e_activity_set_state (activity, E_ACTIVITY_RUNNING);
    e_activity_set_icon_name (activity, "view-refresh");

    /* Make it cancellable (optional) */
    GCancellable *cancellable = g_cancellable_new ();
    e_activity_set_cancellable (activity, cancellable);
    g_object_unref (cancellable);

    /* Add to backend - makes it visible in status bar */
    e_shell_backend_add_activity (shell_backend, activity);

    /* Update status to show request was sent */
    g_autofree gchar *waiting_msg = g_strdup_printf (
        "Translation request sent. Waiting for response..."
    );
    e_activity_set_text (activity, waiting_msg);

    /* Prepare wrapper data */
    TranslateAsyncData *data = g_new0 (TranslateAsyncData, 1);
    data->user_callback = callback;
    data->user_data = user_data;
    data->activity = activity;
    data->provider_name = g_strdup (provider_name);

    /* Initiate async translation */
    translate_provider_translate_async ((TranslateProvider*)provider_obj,
                                        body_html,
                                        TRUE,  /* is_html */
                                        NULL,  /* source (auto-detect) */
                                        target_lang,
                                        NULL,  /* cancellable */
                                        internal_translate_callback,
                                        data);
}
