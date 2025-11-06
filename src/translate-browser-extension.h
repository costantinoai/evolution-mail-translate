/* Translate Browser Extension - integrates Translate into EMailBrowser windows */

#ifndef TRANSLATE_BROWSER_EXTENSION_H
#define TRANSLATE_BROWSER_EXTENSION_H

#include <glib-object.h>
#include <libebackend/libebackend.h>

G_BEGIN_DECLS

#define TRANSLATE_TYPE_BROWSER_EXTENSION (translate_browser_extension_get_type())

G_DECLARE_DERIVABLE_TYPE(TranslateBrowserExtension, translate_browser_extension, TRANSLATE, BROWSER_EXTENSION, EExtension)

struct _TranslateBrowserExtensionClass {
    EExtensionClass parent_class;
};

GType translate_browser_extension_get_type (void);
void  translate_browser_extension_type_register (GTypeModule *type_module);

G_END_DECLS

#endif /* TRANSLATE_BROWSER_EXTENSION_H */

