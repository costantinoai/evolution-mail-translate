/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_PROVIDER_LIBRE (translate_provider_libre_get_type ())
G_DECLARE_FINAL_TYPE (TranslateProviderLibreTranslate, translate_provider_libre,
                      TRANSLATE, PROVIDER_LIBRE, GObject)

void translate_provider_libre_type_register (GTypeModule *type_module);

G_END_DECLS
