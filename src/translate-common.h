/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-common.h
 * Common translation logic shared across UI components
 */

#ifndef TRANSLATE_COMMON_H
#define TRANSLATE_COMMON_H

#include <glib.h>
#include <gio/gio.h>
#include <shell/e-shell-backend.h>
#include "providers/translate-provider.h"

G_BEGIN_DECLS

/**
 * translate_common_translate_async:
 * @body_html: The HTML content to translate
 * @callback: (scope async): Callback to invoke when translation completes
 * @user_data: User data to pass to the callback
 *
 * Initiates an asynchronous translation of the provided HTML content.
 * This function handles:
 * - Retrieving the target language from settings
 * - Creating the appropriate translation provider
 * - Initiating the async translation operation
 * - Proper memory management (fixes the target_lang_copy leak)
 *
 * The callback will be invoked with the translation results.
 * Use translate_provider_translate_finish() in your callback to get the results.
 */
void translate_common_translate_async (const gchar     *body_html,
                                       GAsyncReadyCallback callback,
                                       gpointer         user_data);

/**
 * translate_common_translate_async_with_activity:
 * @body_html: The HTML content to translate
 * @shell_backend: EShellBackend to display activity status
 * @callback: (scope async): Callback to invoke when translation completes
 * @user_data: User data to pass to the callback
 *
 * Initiates an asynchronous translation with visual status feedback.
 * Shows progress messages in the Evolution status bar at the bottom:
 * 1. "Requesting translation from <provider>..."
 * 2. "Translation request sent. Waiting for response..."
 * 3. "Text translated by <provider>." (on success)
 *
 * This function provides the same functionality as translate_common_translate_async
 * but adds visual feedback via the EActivity system for better user experience.
 *
 * The callback will be invoked with the translation results.
 * Use translate_provider_translate_finish() in your callback to get the results.
 */
void translate_common_translate_async_with_activity (const gchar     *body_html,
                                                      EShellBackend   *shell_backend,
                                                      GAsyncReadyCallback callback,
                                                      gpointer         user_data);

G_END_DECLS

#endif /* TRANSLATE_COMMON_H */
