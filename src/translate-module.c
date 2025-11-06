/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

/* Translate extension includes */
#include "translate-shell-view-extension.h"
#include "translate-browser-extension.h"
#include "providers/translate-provider-argos.h"
#include "providers/translate-provider.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register translate shell view extension (Mail view integration) */
	translate_shell_view_extension_type_register (type_module);
	translate_browser_extension_type_register (type_module);

	/* Register providers and add Argos to the registry */
	translate_provider_argos_type_register (type_module);
	translate_provider_register (TRANSLATE_TYPE_PROVIDER_ARGOS);

	/* Log a message so automated checks can verify the module loaded */
	g_message ("[translate] Module loaded");
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
