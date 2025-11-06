/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Argos Translate Provider (offline) - header */

#ifndef TRANSLATE_PROVIDER_ARGOS_H
#define TRANSLATE_PROVIDER_ARGOS_H

#include <glib-object.h>
#include "translate-provider.h"

G_BEGIN_DECLS

#define TRANSLATE_TYPE_PROVIDER_ARGOS (translate_provider_argos_get_type())

G_DECLARE_FINAL_TYPE(TranslateProviderArgos, translate_provider_argos, TRANSLATE, PROVIDER_ARGOS, GObject)

GType translate_provider_argos_get_type (void);
void  translate_provider_argos_type_register (GTypeModule *type_module);

G_END_DECLS

#endif /* TRANSLATE_PROVIDER_ARGOS_H */
