/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate DOM helpers - apply translated HTML to preview and manage state */

#ifndef TRANSLATE_DOM_H
#define TRANSLATE_DOM_H

#include <shell/e-shell-view.h>

G_BEGIN_DECLS

/* Apply translated HTML into the current preview pane. Placeholder implementation. */
void translate_dom_apply_to_shell_view (EShellView *shell_view,
                                        const gchar *translated_html);

/* Toggle back to original content (placeholder). */
void translate_dom_restore_original (EShellView *shell_view);

/* Returns TRUE if the current preview is showing a translated version. */
gboolean translate_dom_is_translated (EShellView *shell_view);

/* Reader variants for browser windows */
void     translate_dom_apply_to_reader   (EMailReader *reader, const gchar *translated_html);
void     translate_dom_restore_original_reader (EMailReader *reader);
gboolean translate_dom_is_translated_reader (EMailReader *reader);

/* Clear translation state if the displayed message has changed */
void     translate_dom_clear_if_message_changed (EShellView *shell_view);
void     translate_dom_clear_if_message_changed_reader (EMailReader *reader);

G_END_DECLS

#endif /* TRANSLATE_DOM_H */
