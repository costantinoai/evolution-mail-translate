#include <gio/gio.h>
#include <gmodule.h>

typedef struct { GTypeModule parent; } ProbeModule;
typedef struct { GTypeModuleClass parent; } ProbeModuleClass;
G_DEFINE_TYPE (ProbeModule, probe_module, G_TYPE_TYPE_MODULE)
static gboolean probe_load (GTypeModule *module) { (void) module; return TRUE; }
static void probe_unload (GTypeModule *module) { (void) module; }
static void probe_module_class_init (ProbeModuleClass *klass) {
    G_TYPE_MODULE_CLASS (klass)->load = probe_load;
    G_TYPE_MODULE_CLASS (klass)->unload = probe_unload;
}
static void probe_module_init (ProbeModule *self) { (void) self; }

static GMainLoop *loop;
static gboolean completed;
static gboolean common_mode;
static gboolean (*finish) (gpointer, GAsyncResult *, gchar **, GError **);
static void done (GObject *source, GAsyncResult *result, gpointer data) {
    (void) data;
    gchar *translated = NULL;
    GError *error = NULL;
    gboolean ok;
    if (common_mode) {
        translated = g_task_propagate_pointer (G_TASK (result), &error);
        ok = translated != NULL;
        const gchar *notice = g_object_get_data (G_OBJECT (result), "translate-fallback-notice");
        if (notice) g_print ("fallback notice: %s\n", notice);
    } else {
        ok = finish (source, result, &translated, &error);
    }
    g_print ("callback: success=%d; error=%s\n", ok, error ? error->message : "none");
    if (translated) g_print ("translation: %s\n", translated);
    completed = TRUE;
    g_free (translated);
    g_clear_error (&error);
    g_main_loop_quit (loop);
}
static gboolean timeout_probe (gpointer data) {
    (void) data;
    g_print ("callback: NOT DELIVERED within 5 seconds\n");
    g_main_loop_quit (loop);
    return G_SOURCE_REMOVE;
}
int main (int argc, char **argv) {
    if (argc != 3) return 2;
    GModule *library = g_module_open (argv[1], G_MODULE_BIND_LAZY);
    if (!library) { g_printerr ("%s\n", g_module_error ()); return 2; }
    void (*load) (GTypeModule *);
    GObject *(*create) (const gchar *);
    void (*start) (gpointer, const gchar *, gboolean, const gchar *, const gchar *, GCancellable *, GAsyncReadyCallback, gpointer);
    if (!g_module_symbol (library, "e_module_load", (gpointer *) &load) ||
        !g_module_symbol (library, "translate_provider_new_by_id", (gpointer *) &create) ||
        !g_module_symbol (library, "translate_provider_translate_async", (gpointer *) &start) ||
        !g_module_symbol (library, "translate_provider_translate_finish", (gpointer *) &finish)) return 2;
    ProbeModule *module = g_object_new (probe_module_get_type (), NULL);
    g_type_module_use (G_TYPE_MODULE (module));
    load (G_TYPE_MODULE (module));
    common_mode = g_str_equal (argv[2], "common") || g_str_equal (argv[2], "unknown-common");
    GObject *provider = create (common_mode ? "google" : argv[2]);
    if (!provider) { g_printerr ("provider unavailable: %s\n", argv[2]); return 2; }
    loop = g_main_loop_new (NULL, FALSE);
    if (common_mode) {
        void (*start_common) (const gchar *, GAsyncReadyCallback, gpointer);
        if (!g_module_symbol (library, "translate_common_translate_async", (gpointer *) &start_common)) return 2;
        GSettings *settings = g_settings_new ("org.gnome.evolution.translate");
        g_settings_set_string (settings, "provider-id",
            g_str_equal (argv[2], "unknown-common") ? "does-not-exist" : "google");
        g_object_unref (settings);
        start_common ("<p>Buongiorno, questa è una prova.</p>", done, NULL);
    } else {
        start (provider, "<p>Buongiorno, questa è una prova.</p>", TRUE, "it", "en", NULL, done, NULL);
    }
    g_timeout_add_seconds (5, timeout_probe, NULL);
    g_main_loop_run (loop);
    g_object_unref (provider);
    return completed ? 0 : 1;
}
