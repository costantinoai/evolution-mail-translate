/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* LibreTranslate Translate Provider (online via deep-translator) */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider-libre.h"
#include "translate-provider.h"

struct _TranslateProviderLibreTranslate {
    GObject parent_instance;
};

static void translate_provider_iface_init (TranslateProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    TranslateProviderLibreTranslate,
    translate_provider_libre,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_iface_init)
)

static const gchar *
tp_libre_get_id (gpointer self)
{
    (void)self;
    return "libre";
}

static const gchar *
tp_libre_get_name (gpointer self)
{
    (void)self;
    return "LibreTranslate Translate (online)";
}

static void
translate_provider_iface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = translate_provider_helper_async;
    iface->translate_finish = translate_provider_helper_finish;
    iface->get_id = tp_libre_get_id;
    iface->get_name = tp_libre_get_name;
}

static void
translate_provider_libre_class_init (TranslateProviderLibreTranslateClass *klass)
{
    (void)klass;
}

static void
translate_provider_libre_class_finalize (TranslateProviderLibreTranslateClass *klass)
{
    (void)klass;
}

static void
translate_provider_libre_init (TranslateProviderLibreTranslate *self)
{
    (void)self;
}

void
translate_provider_libre_type_register (GTypeModule *type_module)
{
    translate_provider_libre_register_type (type_module);
}
