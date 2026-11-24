/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef TRANSLATE_COMMON_H
#define TRANSLATE_COMMON_H

#include <gio/gio.h>
#include <shell/e-shell.h>
#include <mail/e-mail-reader.h>
#include "providers/translate-provider.h"

G_BEGIN_DECLS

/* Callbacks consume results with translate_common_translate_finish(). */
void translate_common_translate_async (const gchar *body_html,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);
void translate_common_translate_async_with_activity (const gchar *body_html,
                                                      EShellBackend *shell_backend,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);
void translate_common_translate_async_full (const gchar *body_html,
                                            EShellBackend *shell_backend,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data);
gboolean translate_common_translate_finish (GAsyncResult *result,
                                             gchar **translated,
                                             GError **error);
void translate_common_show_error (GtkWindow *parent, const GError *error);
void translate_common_translate_reader (EMailReader *reader, EShellBackend *backend);
void translate_common_cancel_reader (EMailReader *reader);
void translate_common_restore_reader (EMailReader *reader);

G_END_DECLS

#endif /* TRANSLATE_COMMON_H */
