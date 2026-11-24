/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Argos Translate provider (offline). */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider-argos.h"
#include "translate-provider.h"

struct _TranslateProviderArgos {
    GObject parent_instance;
};

static void translate_provider_iface_init (TranslateProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    TranslateProviderArgos,
    translate_provider_argos,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_iface_init)
)

static const gchar *
tp_argos_get_id (gpointer self)
{
    (void)self;
    return "argos";
}

static const gchar *
tp_argos_get_name (gpointer self)
{
    (void)self;
    return "Argos Translate (offline)";
}

static void
translate_provider_iface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = translate_provider_helper_async;
    iface->translate_finish = translate_provider_helper_finish;
    iface->get_id = tp_argos_get_id;
    iface->get_name = tp_argos_get_name;
}

static void
translate_provider_argos_class_init (TranslateProviderArgosClass *klass)
{
    (void)klass;
}

static void
translate_provider_argos_class_finalize (TranslateProviderArgosClass *klass)
{
    (void)klass;
}

static void
translate_provider_argos_init (TranslateProviderArgos *self)
{
    (void)self;
}

void
translate_provider_argos_type_register (GTypeModule *type_module)
{
    translate_provider_argos_register_type (type_module);
}
