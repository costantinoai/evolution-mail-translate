/* Argos Translate Provider (offline) - skeleton
 * For now, returns the input text asynchronously (echo). */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "translate-provider-argos.h"
#include "translate-provider.h"
#include "../translate-utils.h"

struct _TranslateProviderArgos {
    GObject parent_instance;
};

static void translate_provider_iface_init (TranslateProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    TranslateProviderArgos,
    translate_provider_argos,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_iface_init)
)

static const gchar *
tp_argos_get_id (gpointer self)
{
    (void)self;
    return "argos";
}

static const gchar *
tp_argos_get_name (gpointer self)
{
    (void)self;
    return "Argos Translate (offline)";
}

typedef struct {
    gchar *input;
    gchar *target_lang;
    gboolean is_html;
} TranslateTaskData;

/**
 * extract_translated_field:
 * @json: JSON string from Python helper
 *
 * Extracts the "translated" field from JSON response using json-glib.
 * Expected format: {"translated": "..."}
 *
 * Returns: (transfer full): The translated text, or NULL on error
 */
static gchar *
extract_translated_field (const gchar *json)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonParser) parser = NULL;
    JsonNode *root = NULL;
    JsonObject *obj = NULL;
    const gchar *translated = NULL;

    if (!json || !*json) {
        g_warning ("[argos] Empty JSON response");
        return NULL;
    }

    /* Parse JSON response */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, json, -1, &error)) {
        g_warning ("[argos] Failed to parse JSON: %s", error ? error->message : "unknown error");
        return NULL;
    }

    /* Get root object */
    root = json_parser_get_root (parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT (root)) {
        g_warning ("[argos] JSON root is not an object");
        return NULL;
    }

    obj = json_node_get_object (root);
    if (!json_object_has_member (obj, "translated")) {
        g_warning ("[argos] JSON response missing 'translated' field");
        return NULL;
    }

    /* Extract translated field */
    translated = json_object_get_string_member (obj, "translated");
    if (!translated) {
        g_warning ("[argos] 'translated' field is not a string");
        return NULL;
    }

    return g_strdup (translated);
}

static void
on_helper_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
    (void)source;
    GTask *task = G_TASK (user_data);
    g_autofree gchar *stdout_str = NULL;
    g_autofree gchar *stderr_str = NULL;
    g_autoptr(GError) error = NULL;
    GSubprocess *proc = g_task_get_task_data (task);
    if (!g_subprocess_communicate_utf8_finish (proc, res, &stdout_str, &stderr_str, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        g_object_unref (task);
        return;
    }
    if (!g_subprocess_get_successful (proc)) {
        g_task_return_new_error (task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                 "Translate helper failed: %s", stderr_str ? stderr_str : "unknown");
        g_object_unref (task);
        return;
    }
    g_autofree gchar *translated = extract_translated_field (stdout_str);
    if (!translated) {
        /* Fallback: use whole stdout */
        translated = g_strdup (stdout_str ? stdout_str : "");
    }
    g_task_return_pointer (task, g_steal_pointer (&translated), g_free);
    g_object_unref (task);
}

static void
tp_argos_translate_async (gpointer              self,
                          const gchar          *input,
                          gboolean              is_html,
                          const gchar          *source_lang_opt,
                          const gchar          *target_lang,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
    (void)source_lang_opt;
    /* Basic parameter validation */
    g_return_if_fail (input != NULL);
    g_return_if_fail (target_lang != NULL);
    GTask *task = g_task_new (self, cancellable, callback, user_data);

    /* Build helper command */
    const gchar *helper_env = g_getenv ("TRANSLATE_HELPER_PATH");
    const gchar *python_env = g_getenv ("TRANSLATE_PYTHON_BIN");

    g_autofree gchar *helper_usr = NULL;
    g_autofree gchar *helper_local = NULL;
    g_autofree gchar *helper_choice = NULL;

    g_autofree gchar *python_usr = NULL;
    g_autofree gchar *python_usr3 = NULL;
    g_autofree gchar *python_local = NULL;
    const gchar *python = NULL;
    const gchar *helper_path = NULL;

    /* Preferred helper path order:
     * 1) TRANSLATE_HELPER_PATH (if set)
     * 2) /usr/lib/evolution-translate/translate/translate_runner.py (packaged install)
     * 3) ~/.local/lib/evolution-translate/translate/translate_runner.py (developer install)
     */
    if (helper_env && *helper_env) {
        helper_path = helper_env;
    } else {
        /* Prefer new data install location (architecture-independent) */
        helper_usr = g_build_filename ("/usr", "share", "evolution-translate", "translate", "translate_runner.py", NULL);
        if (g_file_test (helper_usr, G_FILE_TEST_EXISTS)) {
            helper_choice = g_steal_pointer (&helper_usr);
        } else {
            /* Developer/user-local location */
            const gchar *home = g_get_home_dir ();
            helper_local = g_build_filename (home, ".local", "lib", "evolution-translate", "translate", "translate_runner.py", NULL);
            if (g_file_test (helper_local, G_FILE_TEST_EXISTS)) {
                helper_choice = g_steal_pointer (&helper_local);
            }
        }

        if (helper_choice) {
            helper_path = helper_choice;
        } else {
            g_task_return_new_error (task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                     "Translate helper not found. Set TRANSLATE_HELPER_PATH or run 'evolution-translate-setup'.");
            g_object_unref (task);
            return;
        }
    }

    /* Preferred python order:
     * 1) TRANSLATE_PYTHON_BIN (if set)
     * 2) /usr/lib/evolution-translate/venv/bin/python (packaged venv)
     * 3) ~/.local/lib/evolution-translate/venv/bin/python (developer venv)
     * 4) python3 (system)
     */
    if (python_env && *python_env) {
        python = python_env;
    } else {
        /* User-level venv location */
        const gchar *home = g_get_home_dir ();
        python_local = g_build_filename (home, ".local", "lib", "evolution-translate", "venv", "bin", "python", NULL);
        if (g_file_test (python_local, G_FILE_TEST_IS_EXECUTABLE)) {
            python = python_local;
        } else {
            g_task_return_new_error (task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                     "Python environment not found. Set TRANSLATE_PYTHON_BIN or run 'evolution-translate-setup'.");
            g_object_unref (task);
            return;
        }
    }

    g_debug ("[argos] Using helper: %s", helper_path);
    g_debug ("[argos] Using python: %s", python);

    /* Get install-on-demand setting */
    gboolean install_on_demand = translate_utils_get_install_on_demand ();

    /* Build command arguments: pass target language, HTML flag, and install-on-demand flag */
    g_autofree gchar *target_copy = g_strdup (target_lang ? target_lang : "en");
    const gchar *install_flag = install_on_demand ? "--install-on-demand" : "--no-install-on-demand";
    const gchar *argvv[] = { python, helper_path, "--target", target_copy,
                              is_html ? "--html" : "--text", install_flag, NULL };
    g_debug ("[argos] Running: %s %s --target %s %s %s", python, helper_path, target_copy,
             is_html ? "--html" : "--text", install_flag);
    g_autoptr(GError) error = NULL;
    GSubprocess *proc = g_subprocess_newv (argvv, G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error);
    if (!proc) {
        g_task_return_new_error (task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                 "Failed to spawn helper: %s", error ? error->message : "unknown error");
        g_object_unref (task);
        return;
    }

    /* For now, send plain input (no JSON) to the helper */
    const gchar *payload = input ? input : "";

    g_task_set_task_data (task, g_object_ref (proc), g_object_unref);
    /* Release our local reference; task data holds its own reference */
    g_object_unref (proc);
    g_subprocess_communicate_utf8_async (proc,
                                         payload,
                                         cancellable,
                                         on_helper_done,
                                         task);
}

static gboolean
tp_argos_translate_finish (gpointer       self,
                           GAsyncResult  *res,
                           gchar        **out_translated_html,
                           GError       **error)
{
    (void)self;
    g_return_val_if_fail (G_IS_TASK (res), FALSE);
    gchar *ret = g_task_propagate_pointer (G_TASK (res), error);
    if (out_translated_html)
        *out_translated_html = ret;
    return ret != NULL;
}

static void
translate_provider_iface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = tp_argos_translate_async;
    iface->translate_finish = tp_argos_translate_finish;
    iface->get_id = tp_argos_get_id;
    iface->get_name = tp_argos_get_name;
}

static void
translate_provider_argos_class_init (TranslateProviderArgosClass *klass)
{
    (void)klass;
}

static void
translate_provider_argos_class_finalize (TranslateProviderArgosClass *klass)
{
    (void)klass;
}

static void
translate_provider_argos_init (TranslateProviderArgos *self)
{
    (void)self;
}

void
translate_provider_argos_type_register (GTypeModule *type_module)
{
    translate_provider_argos_register_type (type_module);
}
