/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate Provider - Interface and Registry implementation (skeleton) */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider.h"

static GHashTable *provider_registry; /* id -> GType */

G_DEFINE_INTERFACE(TranslateProvider, translate_provider, G_TYPE_OBJECT)

static void
translate_provider_default_init (TranslateProviderInterface *iface)
{
    (void)iface;
}

void
translate_provider_translate_async (TranslateProvider  *self,
                                    const gchar       *input,
                                    gboolean           is_html,
                                    const gchar       *source_lang_opt,
                                    const gchar       *target_lang,
                                    GCancellable      *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer           user_data)
{
    g_return_if_fail (TRANSLATE_IS_PROVIDER (self));
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    g_return_if_fail (iface->translate_async != NULL);
    iface->translate_async (self, input, is_html, source_lang_opt, target_lang, cancellable, callback, user_data);
}

gboolean
translate_provider_translate_finish (TranslateProvider *self,
                                     GAsyncResult      *res,
                                     gchar            **out_translated_html,
                                     GError           **error)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), FALSE);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    g_return_val_if_fail (iface->translate_finish != NULL, FALSE);
    return iface->translate_finish (self, res, out_translated_html, error);
}

const gchar*
translate_provider_get_id (TranslateProvider *self)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), NULL);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    return iface->get_id ? iface->get_id (self) : NULL;
}

const gchar*
translate_provider_get_name (TranslateProvider *self)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), NULL);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    return iface->get_name ? iface->get_name (self) : NULL;
}

void
translate_provider_register (GType provider_type)
{
    if (!provider_registry)
        provider_registry = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    /* Create a temporary instance to query ID */
    GObject *obj = g_object_new (provider_type, NULL);
    const gchar *id = translate_provider_get_id ((TranslateProvider*)obj);
    if (id && *id) {
        g_hash_table_replace (provider_registry, g_strdup (id), (gpointer)provider_type);
        g_debug ("Registered translate provider: %s", id);
    } else {
        g_warning ("Provider type has no valid ID; skipping registration");
    }
    g_object_unref (obj);
}

GObject*
translate_provider_new_by_id (const gchar *id)
{
    if (!provider_registry || !id)
        return NULL;
    GType t = (GType)g_hash_table_lookup (provider_registry, id);
    if (!t)
        return NULL;
    return g_object_new (t, NULL);
}

gchar**
translate_provider_list_ids (void)
{
    if (!provider_registry)
        return NULL;
    GList *keys = g_hash_table_get_keys (provider_registry);
    guint n = g_list_length (keys);
    gchar **arr = g_new0 (gchar*, n + 1);
    guint i = 0;
    for (GList *l = keys; l; l = l->next) {
        arr[i++] = g_strdup ((const gchar*)l->data);
    }
    g_list_free (keys);
    return arr; /* NULL-terminated */
}
