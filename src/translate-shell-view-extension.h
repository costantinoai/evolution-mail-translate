/*
 * Translate Shell View Extension
 * Integrates translation UI into the Evolution Mail view.
 */

#ifndef TRANSLATE_SHELL_VIEW_EXTENSION_H
#define TRANSLATE_SHELL_VIEW_EXTENSION_H

#include <glib-object.h>
#include <libebackend/libebackend.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_SHELL_VIEW_EXTENSION (translate_shell_view_extension_get_type())

G_DECLARE_DERIVABLE_TYPE(TranslateShellViewExtension, translate_shell_view_extension, TRANSLATE, SHELL_VIEW_EXTENSION, EExtension)

struct _TranslateShellViewExtensionClass {
    EExtensionClass parent_class;
};

GType translate_shell_view_extension_get_type (void);
void  translate_shell_view_extension_type_register (GTypeModule *type_module);

G_END_DECLS

#endif /* TRANSLATE_SHELL_VIEW_EXTENSION_H */

