/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* MyMemory Translate Provider (online via deep-translator) */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider-mymemory.h"
#include "translate-provider.h"

struct _TranslateProviderMyMemory {
    GObject parent_instance;
};

static void translate_provider_iface_init (TranslateProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    TranslateProviderMyMemory,
    translate_provider_mymemory,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_iface_init)
)

static const gchar *
tp_mymemory_get_id (gpointer self)
{
    (void)self;
    return "mymemory";
}

static const gchar *
tp_mymemory_get_name (gpointer self)
{
    (void)self;
    return "MyMemory Translate (online)";
}

static void
translate_provider_iface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = translate_provider_helper_async;
    iface->translate_finish = translate_provider_helper_finish;
    iface->get_id = tp_mymemory_get_id;
    iface->get_name = tp_mymemory_get_name;
}

static void
translate_provider_mymemory_class_init (TranslateProviderMyMemoryClass *klass)
{
    (void)klass;
}

static void
translate_provider_mymemory_class_finalize (TranslateProviderMyMemoryClass *klass)
{
    (void)klass;
}

static void
translate_provider_mymemory_init (TranslateProviderMyMemory *self)
{
    (void)self;
}

void
translate_provider_mymemory_type_register (GTypeModule *type_module)
{
    translate_provider_mymemory_register_type (type_module);
}
