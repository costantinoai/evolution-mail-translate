/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate Provider - Interface, registry and shared helper transport */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "translate-provider.h"
#include "../translate-utils.h"
#include <json-glib/json-glib.h>

#ifndef TRANSLATE_HELPER_DIR
#define TRANSLATE_HELPER_DIR "/usr/share/evolution-translate/translate"
#endif

typedef struct {
    GSubprocess *process;
    GCancellable *cancellable;
    gulong cancel_handler;
    guint timeout_id;
    gboolean timed_out;
} HelperRequest;

static void
helper_request_free (gpointer data)
{
    HelperRequest *request = data;
    if (request->timeout_id)
        g_source_remove (request->timeout_id);
    if (request->cancel_handler)
        g_cancellable_disconnect (request->cancellable, request->cancel_handler);
    g_clear_object (&request->cancellable);
    g_clear_object (&request->process);
    g_free (request);
}

static void
cancel_helper (GCancellable *cancellable, gpointer user_data)
{
    (void) cancellable;
    g_subprocess_force_exit (G_SUBPROCESS (user_data));
}

static gboolean
timeout_helper (gpointer user_data)
{
    HelperRequest *request = user_data;
    request->timeout_id = 0;
    request->timed_out = TRUE;
    g_subprocess_force_exit (request->process);
    return G_SOURCE_REMOVE;
}

static gchar *
parse_helper_response (const gchar *output, GError **error)
{
    g_autoptr(JsonParser) parser = json_parser_new ();
    JsonNode *root, *value;
    JsonObject *object;

    if (!output || !*output) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Translation helper returned an empty response.");
        return NULL;
    }
    if (!json_parser_load_from_data (parser, output, -1, error))
        return NULL;
    root = json_parser_get_root (parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT (root)) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Translation helper response must be a JSON object.");
        return NULL;
    }
    object = json_node_get_object (root);
    if (json_object_has_member (object, "error")) {
        value = json_object_get_member (object, "error");
        const gchar *message = JSON_NODE_HOLDS_VALUE (value) &&
            json_node_get_value_type (value) == G_TYPE_STRING
            ? json_node_get_string (value) : NULL;
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                     message && *message ? message : "Translation helper reported an error.");
        return NULL;
    }
    value = json_object_get_member (object, "translated");
    if (!value || !JSON_NODE_HOLDS_VALUE (value) ||
        json_node_get_value_type (value) != G_TYPE_STRING) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Translation helper response is missing a translated string.");
        return NULL;
    }
    return g_strdup (json_node_get_string (value));
}

static void
helper_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    HelperRequest *request = g_task_get_task_data (task);
    g_autofree gchar *output = NULL;
    g_autofree gchar *diagnostic = NULL;
    g_autoptr(GError) error = NULL;

    gboolean communicated = g_subprocess_communicate_utf8_finish (
        G_SUBPROCESS (source), result, &output, &diagnostic, &error);
    if (request->timeout_id) {
        g_source_remove (request->timeout_id);
        request->timeout_id = 0;
    }
    if (request->timed_out) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                                 "Translation timed out. Please try again.");
        return;
    }
    if (!communicated) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }
    g_autofree gchar *translated = parse_helper_response (output, &error);
    if (!g_subprocess_get_successful (request->process)) {
        /* A structured error is more useful than a Python traceback. */
        if (error)
            g_task_return_error (task, g_steal_pointer (&error));
        else
            g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     "Translation helper failed. Run evolution-translate-setup to check its dependencies.");
        return;
    }
    if (error) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }
    g_task_return_pointer (task, g_steal_pointer (&translated), g_free);
}

static gchar *
find_helper (gboolean offline)
{
    const gchar *override = g_getenv ("TRANSLATE_HELPER_PATH");
    const gchar *filename = offline ? "translate_runner.py" : "translate_runner_online.py";
    if (override && *override)
        return g_strdup (override);
    const gchar *directories[] = {
        TRANSLATE_HELPER_DIR, "/usr/share/evolution-translate/translate", NULL
    };
    for (guint i = 0; directories[i]; i++) {
        gchar *candidate = g_build_filename (directories[i], filename, NULL);
        if (g_file_test (candidate, G_FILE_TEST_IS_REGULAR))
            return candidate;
        g_free (candidate);
    }
    return g_build_filename (g_get_home_dir (), ".local", "lib",
                             "evolution-translate", "translate", filename, NULL);
}

static gchar *
find_python (void)
{
    const gchar *override = g_getenv ("TRANSLATE_PYTHON_BIN");
    if (override && *override)
        return g_strdup (override);
    g_autofree gchar *configured = g_settings_get_string (
        translate_utils_get_provider_settings (), "venv-path");
    if (configured && *configured) {
        if (g_str_has_prefix (configured, "~/")) {
            gchar *expanded = g_build_filename (g_get_home_dir (), configured + 2, NULL);
            g_free (configured);
            configured = expanded;
        }
        return g_build_filename (configured, "bin", "python", NULL);
    }
    return g_build_filename (g_get_home_dir (), ".local", "lib",
                             "evolution-translate", "venv", "bin", "python", NULL);
}

void
translate_provider_helper_async (gpointer self, const gchar *input,
                                 gboolean is_html, const gchar *source_lang_opt,
                                 const gchar *target_lang, GCancellable *cancellable,
                                 GAsyncReadyCallback callback, gpointer user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    if (!input || !*input || !target_lang || !*target_lang) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                 "A message body and target language are required.");
        g_object_unref (task);
        return;
    }
    gboolean offline = g_strcmp0 (translate_provider_get_id (self), "argos") == 0;
    g_autofree gchar *helper = find_helper (offline);
    g_autofree gchar *python = find_python ();
    if (!g_file_test (helper, G_FILE_TEST_IS_REGULAR)) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                 "Translation helper not found. Reinstall the extension or set TRANSLATE_HELPER_PATH.");
        g_object_unref (task);
        return;
    }
    if (!g_file_test (python, G_FILE_TEST_IS_EXECUTABLE)) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                 "Python environment not found. Run evolution-translate-setup or configure the Python environment in Translate Settings.");
        g_object_unref (task);
        return;
    }
    const gchar *argv[14];
    guint argc = 0;
    argv[argc++] = python;
    argv[argc++] = helper;
    argv[argc++] = "--target";
    argv[argc++] = target_lang;
    argv[argc++] = is_html ? "--html" : "--text";
    if (is_html && !g_settings_get_boolean (translate_utils_get_settings (), "preserve-format"))
        argv[argc++] = "--no-preserve-format";
    if (offline) {
        argv[argc++] = translate_utils_get_install_on_demand () &&
            !g_object_get_data (G_OBJECT (self), "translate-installed-models-only")
            ? "--install-on-demand" : "--no-install-on-demand";
    } else {
        argv[argc++] = "--provider";
        argv[argc++] = translate_provider_get_id (self);
    }
    /* Explicit source languages are optional; omit auto-detection overrides. */
    if (source_lang_opt && *source_lang_opt) {
        argv[argc++] = "--source";
        argv[argc++] = source_lang_opt;
    }
    argv[argc] = NULL;
    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_newv (argv,
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (!process) {
        g_task_return_error (task, g_steal_pointer (&error));
        g_object_unref (task);
        return;
    }
    HelperRequest *request = g_new0 (HelperRequest, 1);
    request->process = process;
    if (cancellable) {
        request->cancellable = g_object_ref (cancellable);
        request->cancel_handler = g_cancellable_connect (cancellable,
            G_CALLBACK (cancel_helper), g_object_ref (process), g_object_unref);
    }
    request->timeout_id = g_timeout_add_seconds (180, timeout_helper, request);
    g_task_set_task_data (task, request, helper_request_free);
    g_subprocess_communicate_utf8_async (process, input, cancellable, helper_done, task);
}

gboolean
translate_provider_helper_finish (gpointer self, GAsyncResult *result,
                                  gchar **out_translated_html, GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
    gchar *translated = g_task_propagate_pointer (G_TASK (result), error);
    if (!translated)
        return FALSE;
    if (out_translated_html)
        *out_translated_html = translated;
    else
        g_free (translated);
    return TRUE;
}

static GHashTable *provider_registry; /* id -> GType */

G_DEFINE_INTERFACE(TranslateProvider, translate_provider, G_TYPE_OBJECT)

static void
translate_provider_default_init (TranslateProviderInterface *iface)
{
    (void)iface;
}

void
translate_provider_translate_async (TranslateProvider  *self,
                                    const gchar       *input,
                                    gboolean           is_html,
                                    const gchar       *source_lang_opt,
                                    const gchar       *target_lang,
                                    GCancellable      *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer           user_data)
{
    g_return_if_fail (TRANSLATE_IS_PROVIDER (self));
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    g_return_if_fail (iface->translate_async != NULL);
    iface->translate_async (self, input, is_html, source_lang_opt, target_lang, cancellable, callback, user_data);
}

gboolean
translate_provider_translate_finish (TranslateProvider *self,
                                     GAsyncResult      *res,
                                     gchar            **out_translated_html,
                                     GError           **error)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), FALSE);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    g_return_val_if_fail (iface->translate_finish != NULL, FALSE);
    return iface->translate_finish (self, res, out_translated_html, error);
}

const gchar*
translate_provider_get_id (TranslateProvider *self)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), NULL);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    return iface->get_id ? iface->get_id (self) : NULL;
}

const gchar*
translate_provider_get_name (TranslateProvider *self)
{
    g_return_val_if_fail (TRANSLATE_IS_PROVIDER (self), NULL);
    TranslateProviderInterface *iface = TRANSLATE_PROVIDER_GET_IFACE (self);
    return iface->get_name ? iface->get_name (self) : NULL;
}

void
translate_provider_register (GType provider_type)
{
    if (!provider_registry)
        provider_registry = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    /* Create a temporary instance to query ID */
    GObject *obj = g_object_new (provider_type, NULL);
    const gchar *id = translate_provider_get_id ((TranslateProvider*)obj);
    if (id && *id) {
        g_hash_table_replace (provider_registry, g_strdup (id), (gpointer)provider_type);
        g_debug ("Registered translate provider: %s", id);
    } else {
        g_warning ("Provider type has no valid ID; skipping registration");
    }
    g_object_unref (obj);
}

GObject*
translate_provider_new_by_id (const gchar *id)
{
    if (!provider_registry || !id)
        return NULL;
    GType t = (GType)g_hash_table_lookup (provider_registry, id);
    if (!t)
        return NULL;
    return g_object_new (t, NULL);
}

gchar**
translate_provider_list_ids (void)
{
    if (!provider_registry)
        return NULL;
    GList *keys = g_list_sort (g_hash_table_get_keys (provider_registry), (GCompareFunc) g_strcmp0);
    guint n = g_list_length (keys);
    gchar **arr = g_new0 (gchar*, n + 1);
    guint i = 0;
    for (GList *l = keys; l; l = l->next) {
        arr[i++] = g_strdup ((const gchar*)l->data);
    }
    g_list_free (keys);
    return arr; /* NULL-terminated */
}
