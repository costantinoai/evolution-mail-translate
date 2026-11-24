/* Private-display integration driver for the real Evolution executable. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>
#include <mail/e-mail-browser.h>
#include <mail/e-mail-reader.h>
#include <mail/e-mail-backend.h>
#include <mail/e-mail-view.h>
#include <libemail-engine/libemail-engine.h>
#include <unistd.h>

static GtkWidget *window;
static EMailReader *reader;
static GtkUIManager *manager;
static gchar *uid;
static gchar *second_uid;
static gint phase, ticks;
static gboolean (*is_translated) (EMailReader *);
static const gchar *mode;

/* Only the test process sees a module directory with this checkout's module. */
GList *e_module_load_all_in_directory (const gchar *directory)
{
    GList *(*original) (const gchar *) = dlsym (RTLD_NEXT, "e_module_load_all_in_directory");
    const gchar *replacement = g_getenv ("TRANSLATE_GUI_MODULE_DIR");
    return original (replacement && g_str_has_suffix (directory, "/evolution/modules") ? replacement : directory);
}

GList *e_module_load_all_in_directory_and_prefixes (const gchar *directory, const gchar *prefix)
{
    GList *(*original) (const gchar *, const gchar *) = dlsym (RTLD_NEXT, "e_module_load_all_in_directory_and_prefixes");
    const gchar *replacement = g_getenv ("TRANSLATE_GUI_MODULE_DIR");
    return original (replacement && g_str_has_suffix (directory, "/evolution/modules") ? replacement : directory, prefix);
}

static void finish_test (gboolean passed, const gchar *message)
{
    g_print ("GUI %s: %s — %s\n", mode, passed ? "PASS" : "FAIL", message);
    fflush (stdout);
    _exit (passed ? 0 : 1);
}

static GtkAction *find_action (const gchar *name)
{
    for (GList *item = gtk_ui_manager_get_action_groups (manager); item; item = item->next) {
        GtkAction *action = gtk_action_group_get_action (item->data, name);
        if (action) return action;
    }
    return NULL;
}

static void screenshot (const gchar *suffix)
{
    const gchar *directory = g_getenv ("TRANSLATE_GUI_EVIDENCE");
    if (!directory || !gtk_widget_get_window (window)) return;
    g_autoptr(GdkPixbuf) pixbuf = gdk_pixbuf_get_from_window (gtk_widget_get_window (window),
        0, 0, gtk_widget_get_allocated_width (window), gtk_widget_get_allocated_height (window));
    g_autofree gchar *filename = g_strdup_printf ("%s/gui-%s-%s.png", directory, mode, suffix);
    if (pixbuf) gdk_pixbuf_save (pixbuf, filename, "png", NULL, NULL);
}

static void text_ready (GObject *source, GAsyncResult *result, gpointer data)
{
    (void) data;
    g_autoptr(GError) error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source), result, &error);
    if (!value) finish_test (FALSE, error ? error->message : "No rendered text");
    g_autofree gchar *text = jsc_value_to_string (value);
    g_object_unref (value);
    g_print ("Rendered synthetic body: %s\n", text);
    if (strstr (text, "__TRANSLATE_SCRIPT_EXECUTED__"))
        finish_test (FALSE, "Message script executed after translation");
    if (phase == 3) {
        gboolean correct = strstr (text, "Good morning") || strstr (text, "GOEDEMORGEN");
        if (!correct) finish_test (FALSE, "Translated body missing from WebKit");
        screenshot ("translated");
        GtkAction *original = find_action ("translate-show-original-action");
        if (!original) finish_test (FALSE, "Show Original action missing");
        if (!gtk_action_is_sensitive (original)) finish_test (FALSE, "Show Original remained disabled after translation");
        gtk_action_activate (original);
        phase = 4;
    } else if (phase == 5) {
        screenshot ("original");
        finish_test (!is_translated (reader) && strstr (text, "Goedemorgen"), "Translate and Show Original actions completed");
    }
}

static void read_rendered_text (void)
{
    const gchar *script =
        "(()=>{const read=d=>{let s=(d.defaultView.__translate_audit_executed?'__TRANSLATE_SCRIPT_EXECUTED__':'')+(d.body?d.body.innerText:'');"
        "for(const f of d.querySelectorAll('iframe')){try{if(f.contentDocument)s+=' '+read(f.contentDocument)}catch(e){}}return s};return read(document)})()";
    webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (e_mail_reader_get_mail_display (reader)),
        script, -1, NULL, NULL, NULL, text_ready, NULL);
}

static gboolean inspect_message_dialog (gboolean fallback)
{
    GList *windows = gtk_window_list_toplevels ();
    gboolean found = FALSE;
    for (GList *item = windows; item; item = item->next) {
        if (!GTK_IS_MESSAGE_DIALOG (item->data) || !gtk_widget_get_visible (item->data)) continue;
        g_autofree gchar *primary = NULL, *secondary = NULL;
        g_object_get (item->data, "text", &primary, "secondary-text", &secondary, NULL);
        if (secondary && strstr (secondary, "HTTP 429") &&
            (!fallback || (primary && strstr (primary, "Argos offline")))) {
            g_print ("Visible dialog: %s — %s\n", primary, secondary);
            gtk_dialog_response (GTK_DIALOG (item->data), GTK_RESPONSE_CLOSE);
            found = TRUE;
            break;
        }
    }
    g_list_free (windows);
    return found;
}

static gboolean edit_settings (gpointer data)
{
    (void) data;
    GList *windows = gtk_window_list_toplevels ();
    for (GList *item = windows; item; item = item->next) {
        if (!GTK_IS_DIALOG (item->data) ||
            g_strcmp0 (gtk_window_get_title (item->data), "Translate Settings")) continue;
        GList *children = gtk_container_get_children (GTK_CONTAINER (gtk_dialog_get_content_area (item->data)));
        GtkWidget *grid = children ? children->data : NULL;
        g_list_free (children);
        children = grid ? gtk_container_get_children (GTK_CONTAINER (grid)) : NULL;
        gboolean field = FALSE, providers = FALSE;
        for (GList *child = children; child; child = child->next) {
            if (GTK_IS_ENTRY (child->data)) {
                field = !g_strcmp0 (gtk_entry_get_text (child->data), "/tmp/existing-venv");
                gtk_entry_set_text (child->data, "/tmp/saved-venv");
            } else if (GTK_IS_COMBO_BOX (child->data)) {
                GtkTreeModel *model = gtk_combo_box_get_model (child->data);
                if (gtk_tree_model_iter_n_children (model, NULL) == 4) {
                    providers = gtk_combo_box_set_active_id (child->data, "mymemory");
                }
            }
        }
        g_list_free (children);
        if (!field || !providers) finish_test (FALSE, "Settings did not load the venv or expose registered providers");
        gtk_dialog_response (item->data, GTK_RESPONSE_OK);
        g_list_free (windows);
        return G_SOURCE_REMOVE;
    }
    g_list_free (windows);
    return G_SOURCE_CONTINUE;
}

static gchar *append_message (CamelFolder *folder, const gchar *body)
{
    g_autoptr(CamelMimeMessage) message = camel_mime_message_new ();
    camel_mime_message_set_subject (message, "Synthetic translation audit");
    camel_mime_part_set_content (CAMEL_MIME_PART (message), body, strlen (body), "text/html; charset=UTF-8");
    gchar *appended_uid = NULL;
    g_autoptr(GError) error = NULL;
    if (!camel_folder_append_message_sync (folder, message, NULL, &appended_uid, NULL, &error))
        finish_test (FALSE, error ? error->message : "Could not append synthetic mail");
    return appended_uid;
}

static gboolean drive (gpointer data)
{
    (void) data;
    if (++ticks > 200) finish_test (FALSE, "Timed out waiting for Evolution");
    if (phase == 0) {
        EShell *shell = e_shell_get_default ();
        if (!shell) return G_SOURCE_CONTINUE;
        EShellBackend *backend = e_shell_get_backend_by_name (shell, "mail");
        if (!backend) return G_SOURCE_CONTINUE;
        e_shell_backend_start (backend);
        EMailSession *session = e_mail_backend_get_session (E_MAIL_BACKEND (backend));
        if (!session) return G_SOURCE_CONTINUE;
        GModule *module = g_module_open (g_getenv ("TRANSLATE_GUI_MODULE"), G_MODULE_BIND_LAZY);
        if (!module || !g_module_symbol (module, "translate_dom_is_translated_reader", (gpointer *) &is_translated))
            finish_test (FALSE, "Could not inspect the test module");
        GSettings *settings = g_settings_new ("org.gnome.evolution.translate");
        g_settings_set_string (settings, "provider-id",
            g_str_equal (mode, "error") || g_str_equal (mode, "fallback") ? "google" : "argos");
        g_settings_set_string (settings, "target-language", "en");
        g_object_unref (settings);
        if (g_str_equal (mode, "browser")) {
            window = e_mail_browser_new (E_MAIL_BACKEND (backend), E_MAIL_FORMATTER_MODE_NORMAL);
            reader = E_MAIL_READER (window);
            manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (window));
        } else {
            window = e_shell_create_shell_window (shell, "mail");
            EShellView *view = e_shell_window_get_shell_view (E_SHELL_WINDOW (window), "mail");
            EMailView *mail_view = NULL;
            g_object_get (e_shell_view_get_shell_content (view), "mail-view", &mail_view, NULL);
            reader = E_MAIL_READER (mail_view);
            g_object_unref (mail_view);
            manager = e_shell_window_get_ui_manager (E_SHELL_WINDOW (window));
        }
        gtk_window_set_default_size (GTK_WINDOW (window), 1050, 760);
        gtk_widget_show_all (window);
        if (g_str_equal (mode, "settings")) {
            GSettings *provider_settings = g_settings_new ("org.gnome.evolution.translate.provider");
            g_settings_set_string (provider_settings, "venv-path", "/tmp/existing-venv");
            GtkAction *action = find_action ("translate-settings-action");
            if (!action) finish_test (FALSE, "Settings action missing");
            g_timeout_add (100, edit_settings, NULL);
            gtk_action_activate (action);
            g_autofree gchar *saved = g_settings_get_string (provider_settings, "venv-path");
            finish_test (!g_strcmp0 (saved, "/tmp/saved-venv"), "Settings loaded and saved the Python environment");
        }
        CamelFolder *folder = e_mail_session_get_local_folder (session, E_MAIL_LOCAL_FOLDER_INBOX);
        if (!folder) finish_test (FALSE, "Isolated local inbox unavailable");
        uid = append_message (folder, "<html><body><p>Goedemorgen, dit is een testbericht.</p><script>window.__translate_audit_executed=true</script></body></html>");
        second_uid = append_message (folder, "<html><body><p>SECOND MESSAGE MUST STAY ORIGINAL</p></body></html>");
        e_mail_reader_set_folder (reader, folder);
        e_mail_reader_set_message (reader, uid);
        phase = 1;
        return G_SOURCE_CONTINUE;
    }
    if (phase == 1) {
        EMailDisplay *display = e_mail_reader_get_mail_display (reader);
        EMailPartList *parts = e_mail_display_get_part_list (display);
        GtkAction *action = find_action ("translate-message-action");
        if (!parts || g_strcmp0 (e_mail_part_list_get_message_uid (parts), uid) ||
            webkit_web_view_is_loading (WEBKIT_WEB_VIEW (display))) return G_SOURCE_CONTINUE;
        if (!action) finish_test (FALSE, "Translate action missing");
        g_print ("Translate action sensitive=%d\n", gtk_action_is_sensitive (action));
        if (!gtk_action_is_sensitive (action)) return G_SOURCE_CONTINUE;
        gtk_action_activate (action);
        if (g_str_equal (mode, "stale")) e_mail_reader_set_message (reader, second_uid);
        if (g_str_equal (mode, "destroy")) { gtk_widget_destroy (window); phase = 7; }
        else phase = 2;
        ticks = 0;
    } else if (phase == 2) {
        if (g_str_equal (mode, "error")) {
            if (inspect_message_dialog (FALSE)) finish_test (TRUE, "Provider failure is visible in the originating window");
        } else if (g_str_equal (mode, "stale")) {
            EMailPartList *parts = e_mail_display_get_part_list (e_mail_reader_get_mail_display (reader));
            if (ticks > 12 && parts && !g_strcmp0 (e_mail_part_list_get_message_uid (parts), second_uid))
                finish_test (!is_translated (reader), "Stale result did not replace the second message");
        } else if (is_translated (reader) && !webkit_web_view_is_loading (WEBKIT_WEB_VIEW (e_mail_reader_get_mail_display (reader)))) {
            if (g_str_equal (mode, "fallback") && !inspect_message_dialog (TRUE))
                finish_test (FALSE, "Offline fallback was not disclosed");
            phase = 3;
            read_rendered_text ();
        }
    } else if (phase == 4 && !webkit_web_view_is_loading (WEBKIT_WEB_VIEW (e_mail_reader_get_mail_display (reader)))) {
        phase = 5;
        read_rendered_text ();
    } else if (phase == 7 && ticks > 12) {
        finish_test (TRUE, "Closing the originating window caused no stale callback or crash");
    }
    return G_SOURCE_CONTINUE;
}

__attribute__((constructor)) static void start_driver (void)
{
    mode = g_getenv ("TRANSLATE_GUI_AUDIT");
    if (!mode) return;
    /* Python/WebKit child processes must not inherit this test driver. */
    g_unsetenv ("LD_PRELOAD");
    g_timeout_add (250, drive, NULL);
}
