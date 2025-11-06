/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate Shell View Extension
 * Wires Translate UI into Mail view (preview pane). */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <shell/e-shell-view.h>

#include "translate-shell-view-extension.h"
#include "translate-mail-ui.h"

G_DEFINE_DYNAMIC_TYPE(TranslateShellViewExtension, translate_shell_view_extension, E_TYPE_EXTENSION)

static void
translate_shell_view_extension_constructed (GObject *object)
{
    EExtension *extension;
    EExtensible *extensible;
    EShellViewClass *shell_view_class;

    G_OBJECT_CLASS (translate_shell_view_extension_parent_class)->constructed (object);

    extension = E_EXTENSION (object);
    extensible = e_extension_get_extensible (extension);

    shell_view_class = E_SHELL_VIEW_GET_CLASS (extensible);
    if (!shell_view_class)
        return;

    /* Only integrate with Mail view */
    if (g_strcmp0 (shell_view_class->ui_manager_id, "org.gnome.evolution.mail") == 0) {
        translate_mail_ui_init (E_SHELL_VIEW (extensible));
    }
}

static void
translate_shell_view_extension_class_init (TranslateShellViewExtensionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    EExtensionClass *extension_class = E_EXTENSION_CLASS (klass);

    object_class->constructed = translate_shell_view_extension_constructed;

    extension_class->extensible_type = E_TYPE_SHELL_VIEW;
}

static void
translate_shell_view_extension_class_finalize (TranslateShellViewExtensionClass *klass)
{
}

static void
translate_shell_view_extension_init (TranslateShellViewExtension *self)
{
}

void
translate_shell_view_extension_type_register (GTypeModule *type_module)
{
    translate_shell_view_extension_register_type (type_module);
}
