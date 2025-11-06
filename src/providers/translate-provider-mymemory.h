/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_PROVIDER_MYMEMORY (translate_provider_mymemory_get_type ())
G_DECLARE_FINAL_TYPE (TranslateProviderMyMemory, translate_provider_mymemory,
                      TRANSLATE, PROVIDER_MYMEMORY, GObject)

void translate_provider_mymemory_type_register (GTypeModule *type_module);

G_END_DECLS
