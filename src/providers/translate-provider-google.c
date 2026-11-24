/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Google Translate Provider (online via deep-translator) */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider-google.h"
#include "translate-provider.h"

struct _TranslateProviderGoogle {
    GObject parent_instance;
};

static void translate_provider_iface_init (TranslateProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    TranslateProviderGoogle,
    translate_provider_google,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_iface_init)
)

static const gchar *
tp_google_get_id (gpointer self)
{
    (void)self;
    return "google";
}

static const gchar *
tp_google_get_name (gpointer self)
{
    (void)self;
    return "Google Translate (online)";
}

static void
translate_provider_iface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = translate_provider_helper_async;
    iface->translate_finish = translate_provider_helper_finish;
    iface->get_id = tp_google_get_id;
    iface->get_name = tp_google_get_name;
}

static void
translate_provider_google_class_init (TranslateProviderGoogleClass *klass)
{
    (void)klass;
}

static void
translate_provider_google_class_finalize (TranslateProviderGoogleClass *klass)
{
    (void)klass;
}

static void
translate_provider_google_init (TranslateProviderGoogle *self)
{
    (void)self;
}

void
translate_provider_google_type_register (GTypeModule *type_module)
{
    translate_provider_google_register_type (type_module);
}
