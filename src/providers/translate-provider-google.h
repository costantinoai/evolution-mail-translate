/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_PROVIDER_GOOGLE (translate_provider_google_get_type ())
G_DECLARE_FINAL_TYPE (TranslateProviderGoogle, translate_provider_google,
                      TRANSLATE, PROVIDER_GOOGLE, GObject)

void translate_provider_google_type_register (GTypeModule *type_module);

G_END_DECLS
