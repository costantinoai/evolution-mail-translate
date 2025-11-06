/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * translate-common.h
 * Common translation logic shared across UI components
 */

#ifndef TRANSLATE_COMMON_H
#define TRANSLATE_COMMON_H

#include <glib.h>
#include <gio/gio.h>
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

G_END_DECLS

#endif /* TRANSLATE_COMMON_H */
