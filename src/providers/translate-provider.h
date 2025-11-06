/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate Provider - GInterface and Registry */

#ifndef TRANSLATE_PROVIDER_H
#define TRANSLATE_PROVIDER_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_PROVIDER (translate_provider_get_type())
G_DECLARE_INTERFACE(TranslateProvider, translate_provider, TRANSLATE, PROVIDER, GObject)

struct _TranslateProviderInterface {
    GTypeInterface parent_iface;

    /* Async translate. Implementations must be non-blocking. */
    void (*translate_async) (gpointer          self,
                             const gchar      *input,
                             gboolean          is_html,
                             const gchar      *source_lang_opt,
                             const gchar      *target_lang,
                             GCancellable     *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer          user_data);

    gboolean (*translate_finish) (gpointer      self,
                                  GAsyncResult *res,
                                  gchar       **out_translated_html,
                                  GError      **error);

    const gchar* (*get_id)   (gpointer self);
    const gchar* (*get_name) (gpointer self);
};

/* Interface wrappers */
void translate_provider_translate_async (TranslateProvider  *self,
                                         const gchar       *input,
                                         gboolean           is_html,
                                         const gchar       *source_lang_opt,
                                         const gchar       *target_lang,
                                         GCancellable      *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer           user_data);

gboolean translate_provider_translate_finish (TranslateProvider *self,
                                              GAsyncResult      *res,
                                              gchar            **out_translated_html,
                                              GError           **error);

const gchar* translate_provider_get_id   (TranslateProvider *self);
const gchar* translate_provider_get_name (TranslateProvider *self);

/* Registry */
void      translate_provider_register      (GType provider_type);
GObject*  translate_provider_new_by_id     (const gchar *id);
gchar**   translate_provider_list_ids      (void); /* NULL-terminated list, free with g_strfreev */

G_END_DECLS

#endif /* TRANSLATE_PROVIDER_H */
