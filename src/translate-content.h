/* Translate content extraction helpers */

#ifndef TRANSLATE_CONTENT_H
#define TRANSLATE_CONTENT_H

#include <shell/e-shell-view.h>
#include <mail/e-mail-reader.h>

G_BEGIN_DECLS

/* Returns newly allocated HTML for the selected message body in a reader. */
gchar * translate_get_selected_message_body_html_from_reader (EMailReader *reader);

/* Convenience wrapper to fetch from shell view's current mail view reader. */
gchar * translate_get_selected_message_body_html_from_shell_view (EShellView *shell_view);

G_END_DECLS

#endif /* TRANSLATE_CONTENT_H */

