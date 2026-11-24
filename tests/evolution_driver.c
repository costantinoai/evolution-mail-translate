/* Private-display integration driver for the real Evolution executable. */
/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Modified by Andrea Costantino on 2026-11-24: cover both APIs and fork ports. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <gdk/gdkx.h>
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
#ifdef HAVE_EUI_MANAGER
typedef EUIAction TestAction;
static EUIManager *manager;
#else
typedef GtkAction TestAction;
static GtkUIManager *manager;
#endif
static gchar *uid;
static gchar *second_uid;
static gint phase, ticks;
static gboolean (*is_translated) (EMailReader *);
static const gchar *mode;
static gboolean cache_checked;

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

static TestAction *find_action (const gchar *name)
{
#ifdef HAVE_EUI_MANAGER
    return e_ui_manager_get_action (manager, name);
#else
    for (GList *item = gtk_ui_manager_get_action_groups (manager); item; item = item->next) {
        GtkAction *action = gtk_action_group_get_action (item->data, name);
        if (action) return action;
    }
    return NULL;
#endif
}

static gboolean action_is_sensitive (TestAction *action)
{
#ifdef HAVE_EUI_MANAGER
    return g_action_get_enabled (G_ACTION (action));
#else
    return gtk_action_is_sensitive (action);
#endif
}

static void activate_action (TestAction *action)
{
#ifdef HAVE_EUI_MANAGER
    g_action_activate (G_ACTION (action), NULL);
#else
    gtk_action_activate (action);
#endif
}

static gboolean shortcut_matches (TestAction *action, guint keyval, GdkModifierType modifiers)
{
#ifdef HAVE_EUI_MANAGER
    guint current_key = 0;
    GdkModifierType current_modifiers = 0;
    const gchar *accel = e_ui_action_get_accel (action);
    if (accel)
        gtk_accelerator_parse (accel, &current_key, &current_modifiers);
    return current_key == keyval && current_modifiers == modifiers;
#else
    GtkAccelKey key;
    const gchar *path = gtk_action_get_accel_path (action);
    return path && gtk_accel_map_lookup_entry (path, &key) &&
        key.accel_key == keyval && key.accel_mods == modifiers;
#endif
}

static void send_shortcut (guint keyval, GdkModifierType modifiers)
{
    /* XTEST exercises normal keyboard dispatch, including the server's modifier
     * state. This driver is launched only on its own private Xvfb display. */
    void *library = dlopen ("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
    int (*fake_key) (Display *, unsigned int, Bool, unsigned long) =
        library ? dlsym (library, "XTestFakeKeyEvent") : NULL;
    if (!fake_key) finish_test (FALSE, "XTEST keyboard injection is unavailable");
    GdkDisplay *gdk_display = gtk_widget_get_display (window);
    Display *xdisplay = gdk_x11_display_get_xdisplay (gdk_display);
    guint keyvals[] = { GDK_KEY_Control_L, GDK_KEY_Shift_L, keyval };
    guint keycodes[G_N_ELEMENTS (keyvals)];
    g_assert (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK));
    for (guint i = 0; i < G_N_ELEMENTS (keyvals); i++) {
        GdkKeymapKey *keys = NULL;
        gint n_keys = 0;
        if (!gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (gdk_display),
                                               keyvals[i], &keys, &n_keys) || !n_keys)
            finish_test (FALSE, "Shortcut is unavailable in the private display's keymap");
        keycodes[i] = keys[0].keycode;
        g_free (keys);
        fake_key (xdisplay, keycodes[i], TRUE, 0);
    }
    for (gint i = G_N_ELEMENTS (keyvals) - 1; i >= 0; i--)
        fake_key (xdisplay, keycodes[i], FALSE, 0);
    gdk_display_flush (gdk_display);
    dlclose (library);
}

static GtkWidget *find_translate_button (GtkWidget *widget)
{
    if (GTK_IS_BUTTON (widget) || GTK_IS_TOOL_BUTTON (widget)) {
        g_autofree gchar *tooltip = gtk_widget_get_tooltip_text (widget);
        if (tooltip && strstr (tooltip, "Translate the selected message") && gtk_widget_get_visible (widget))
            return widget;
    }
    if (GTK_IS_CONTAINER (widget)) {
        GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
        for (GList *item = children; item; item = item->next) {
            GtkWidget *found = find_translate_button (item->data);
            if (found) { g_list_free (children); return found; }
        }
        g_list_free (children);
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
        if (g_str_equal (mode, "dark") || g_str_equal (mode, "cache")) {
            if (!strstr (text, "BG=rgb(255, 255, 255);FG=rgb(0, 0, 0);STYLES=1"))
                finish_test (FALSE, "Translated document has no readable light defaults");
        } else if (g_str_equal (mode, "styled") || g_str_equal (mode, "declared")) {
            if (!strstr (text, "BG=rgb(10, 20, 30);FG=rgb(240, 240, 240);STYLES=1"))
                finish_test (FALSE, "Styled message lost its colors or document wrapper");
            if (g_str_equal (mode, "declared") && !strstr (text, "SCHEME=dark"))
                finish_test (FALSE, "The message's declared color scheme was replaced");
        }
        screenshot ("translated");
        if (g_str_equal (mode, "cache") && !cache_checked) {
            e_mail_reader_set_message (reader, second_uid);
            phase = 8;
            ticks = 0;
            return;
        }
        if (g_str_equal (mode, "cache")) {
            g_autofree gchar *path = g_build_filename (g_getenv ("TRANSLATE_GUI_EVIDENCE"), "helper-count", NULL);
            g_autofree gchar *count = NULL;
            if (!g_file_get_contents (path, &count, NULL, NULL) || g_strcmp0 (count, "1"))
                finish_test (FALSE, "Cached translation unnecessarily invoked the helper");
        }
        TestAction *original = find_action ("translate-show-original-action");
        if (!original) finish_test (FALSE, "Show Original action missing");
        if (!action_is_sensitive (original)) finish_test (FALSE, "Show Original remained disabled after translation");
        activate_action (original);
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
        "for(const f of d.querySelectorAll('iframe')){try{if(f.contentDocument)s+=' '+read(f.contentDocument)}catch(e){}}return s};"
        "const docs=[];const scan=d=>{docs.push(d);for(const f of d.querySelectorAll('iframe')){try{if(f.contentDocument)scan(f.contentDocument)}catch(e){}}};scan(document);"
        "const d=docs.find(d=>d.body&&/GOEDEMORGEN|Good morning/.test(d.body.innerText))||document;"
        "const c=d.defaultView.getComputedStyle(d.body);let bg=c.backgroundColor;"
        "if(bg==='rgba(0, 0, 0, 0)')bg=d.defaultView.getComputedStyle(d.documentElement).backgroundColor;"
        "return read(document)+' BG='+bg+';FG='+c.color+';STYLES='+d.querySelectorAll('#evolution-translate-colors').length+"
        "';SCHEME='+(d.querySelector('meta[name=color-scheme]')?.content||'')})()";
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
        gboolean field = FALSE, providers = FALSE, shortcut = FALSE;
        for (GList *child = children; child; child = child->next) {
            if (GTK_IS_ENTRY (child->data)) {
                field = !g_strcmp0 (gtk_entry_get_text (child->data), "/tmp/existing-venv");
                gtk_entry_set_text (child->data, "/tmp/saved-venv");
            } else if (GTK_IS_COMBO_BOX (child->data)) {
                GtkTreeModel *model = gtk_combo_box_get_model (child->data);
                if (gtk_tree_model_iter_n_children (model, NULL) == 4) {
                    providers = gtk_combo_box_set_active_id (child->data, "mymemory");
                }
            } else if (GTK_IS_TREE_VIEW (child->data)) {
                GtkTreeViewColumn *column = gtk_tree_view_get_column (child->data, 0);
                GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
                if (cells && GTK_IS_CELL_RENDERER_ACCEL (cells->data)) {
                    g_signal_emit_by_name (cells->data, "accel-edited", "0", GDK_KEY_y,
                                          GDK_CONTROL_MASK | GDK_MOD1_MASK, 0);
                    shortcut = TRUE;
                }
                g_list_free (cells);
            }
        }
        g_list_free (children);
        if (!field || !providers) finish_test (FALSE, "Settings did not load the venv or expose registered providers");
        if (!shortcut) finish_test (FALSE, "Settings have no keyboard shortcut editor");
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
        if (strstr (mode, "shortcut"))
            g_settings_set_string (settings, "translate-shortcut", "<Alt><Shift>y");
        if (g_str_equal (mode, "dark") || g_str_equal (mode, "styled") ||
            g_str_equal (mode, "declared") || g_str_equal (mode, "cache"))
            g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", TRUE, NULL);
        if (g_str_equal (mode, "cache")) {
            g_autofree gchar *counter = g_build_filename (g_getenv ("TRANSLATE_GUI_EVIDENCE"), "helper-count", NULL);
            g_setenv ("TRANSLATE_GUI_HELPER_COUNT", counter, TRUE);
        }
        g_object_unref (settings);
        if (g_str_has_prefix (mode, "browser")) {
            window = e_mail_browser_new (E_MAIL_BACKEND (backend), E_MAIL_FORMATTER_MODE_NORMAL);
            reader = E_MAIL_READER (window);
#ifdef HAVE_EUI_MANAGER
            manager = e_mail_reader_get_ui_manager (reader);
#else
            manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (window));
#endif
        } else {
            window = e_shell_create_shell_window (shell, "mail");
            EShellView *view = e_shell_window_get_shell_view (E_SHELL_WINDOW (window), "mail");
            EMailView *mail_view = NULL;
            g_object_get (e_shell_view_get_shell_content (view), "mail-view", &mail_view, NULL);
            reader = E_MAIL_READER (mail_view);
            g_object_unref (mail_view);
#ifdef HAVE_EUI_MANAGER
            manager = e_shell_view_get_ui_manager (view);
#else
            manager = e_shell_window_get_ui_manager (E_SHELL_WINDOW (window));
#endif
        }
        gtk_window_set_default_size (GTK_WINDOW (window), 1050, 760);
        gtk_widget_show_all (window);
        if (g_str_equal (mode, "settings")) {
            GSettings *provider_settings = g_settings_new ("org.gnome.evolution.translate.provider");
            g_settings_set_string (provider_settings, "venv-path", "/tmp/existing-venv");
            TestAction *action = find_action ("translate-settings-action");
            if (!action) finish_test (FALSE, "Settings action missing");
            g_timeout_add (100, edit_settings, NULL);
            activate_action (action);
            g_autofree gchar *saved = g_settings_get_string (provider_settings, "venv-path");
            GSettings *current = g_settings_new ("org.gnome.evolution.translate");
            g_autofree gchar *shortcut = g_settings_get_string (current, "translate-shortcut");
            guint key;
            GdkModifierType modifiers;
            gtk_accelerator_parse (shortcut, &key, &modifiers);
            finish_test (!g_strcmp0 (saved, "/tmp/saved-venv") && key == GDK_KEY_y &&
                modifiers == (GDK_CONTROL_MASK | GDK_MOD1_MASK), "Settings saved the Python environment and shortcut");
        }
        CamelFolder *folder = e_mail_session_get_local_folder (session, E_MAIL_LOCAL_FOLDER_INBOX);
        if (!folder) finish_test (FALSE, "Isolated local inbox unavailable");
        const gchar *body = g_str_equal (mode, "declared") ?
            "<html><head><meta name='color-scheme' content='dark'><style>body {background-color:rgb(10,20,30);color:rgb(240,240,240)}</style></head>"
            "<body><p>Goedemorgen, dit is een testbericht.</p></body></html>" : g_str_equal (mode, "styled") ?
            "<html><head><style>body {background-color:rgb(10,20,30);color:rgb(240,240,240)}</style></head><body>"
            "<p>Goedemorgen, dit is een testbericht.</p><script>window.__translate_audit_executed=true</script></body></html>" :
            "<html><body><p>Goedemorgen, dit is een testbericht.</p><script>window.__translate_audit_executed=true</script></body></html>";
        uid = append_message (folder, body);
        second_uid = append_message (folder, "<html><body><p>SECOND MESSAGE MUST STAY ORIGINAL</p></body></html>");
        e_mail_reader_set_folder (reader, folder);
        e_mail_reader_set_message (reader, uid);
        phase = 1;
        return G_SOURCE_CONTINUE;
    }
    if (phase == 1) {
        EMailDisplay *display = e_mail_reader_get_mail_display (reader);
        EMailPartList *parts = e_mail_display_get_part_list (display);
        TestAction *action = find_action ("translate-message-action");
        if (!parts || g_strcmp0 (e_mail_part_list_get_message_uid (parts), uid) ||
            webkit_web_view_is_loading (WEBKIT_WEB_VIEW (display))) return G_SOURCE_CONTINUE;
        if (!action) finish_test (FALSE, "Translate action missing");
        if (!action_is_sensitive (action)) return G_SOURCE_CONTINUE;
        if (strstr (mode, "shortcut")) {
            if (!gtk_window_is_active (GTK_WINDOW (window))) {
                g_print ("Keyboard target inactive; presenting the private test window\n");
                gtk_window_present (GTK_WINDOW (window));
                gdk_window_focus (gtk_widget_get_window (window), GDK_CURRENT_TIME);
                return G_SOURCE_CONTINUE;
            }
            if (!shortcut_matches (action, GDK_KEY_y, GDK_MOD1_MASK | GDK_SHIFT_MASK))
                finish_test (FALSE, "Configured shortcut was not loaded");
            GSettings *settings = g_settings_new ("org.gnome.evolution.translate");
            g_settings_set_string (settings, "translate-shortcut", "");
            if (!shortcut_matches (action, 0, 0))
                finish_test (FALSE, "An empty shortcut did not disable the binding");
            g_settings_set_string (settings, "translate-shortcut", "invalid-accelerator");
            if (!shortcut_matches (action, GDK_KEY_t, GDK_CONTROL_MASK | GDK_SHIFT_MASK))
                finish_test (FALSE, "Invalid shortcut did not use the schema default");
            g_settings_set_string (settings, "translate-shortcut", "<Control><Shift>F12");
            g_object_unref (settings);
            if (!shortcut_matches (action, GDK_KEY_F12, GDK_CONTROL_MASK | GDK_SHIFT_MASK))
                finish_test (FALSE, "Shortcut did not update in the open window");
            gtk_widget_grab_focus (g_str_has_prefix (mode, "browser") ? GTK_WIDGET (display) :
                                   e_mail_reader_get_message_list (reader));
            /* GtkWindow rebuilds its accelerator lookup after keys-changed.
             * Let that main-loop work finish before the next user input. */
            phase = 10;
            ticks = 0;
            return G_SOURCE_CONTINUE;
        } else if (strstr (mode, "toolbar")) {
            GtkWidget *button = find_translate_button (window);
            if (!button) {
#ifndef HAVE_EUI_MANAGER
                g_autofree gchar *xml = gtk_ui_manager_get_ui (manager);
                g_print ("UI definition: %s\n", xml);
#endif
                finish_test (FALSE, "Translate toolbar button missing");
            }
            if (!gtk_widget_is_sensitive (button)) finish_test (FALSE, "Translate toolbar action is unresolved or disabled");
            if (GTK_IS_TOOL_BUTTON (button))
                button = gtk_bin_get_child (GTK_BIN (button));
            if (!GTK_IS_BUTTON (button)) finish_test (FALSE, "Toolbar has no native button");
            gtk_button_clicked (GTK_BUTTON (button));
        } else activate_action (action);
        if (g_str_equal (mode, "stale")) e_mail_reader_set_message (reader, second_uid);
        if (g_str_equal (mode, "destroy")) { gtk_widget_destroy (window); phase = 7; }
        else phase = 2;
        ticks = 0;
    } else if (phase == 10) {
        send_shortcut (GDK_KEY_F12, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
        phase = 2;
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
    } else if (phase == 8 || phase == 9) {
        EMailDisplay *display = e_mail_reader_get_mail_display (reader);
        EMailPartList *parts = e_mail_display_get_part_list (display);
        const gchar *expected = phase == 8 ? second_uid : uid;
        if (!parts || g_strcmp0 (e_mail_part_list_get_message_uid (parts), expected) ||
            webkit_web_view_is_loading (WEBKIT_WEB_VIEW (display))) return G_SOURCE_CONTINUE;
        if (phase == 8) {
            if (is_translated (reader)) finish_test (FALSE, "Translation state leaked to another message");
            e_mail_reader_set_message (reader, uid);
            phase = 9;
        } else {
            cache_checked = TRUE;
            activate_action (find_action ("translate-message-action"));
            phase = 2;
        }
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
