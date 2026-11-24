/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Display-owned original state and a bounded cache of translated bodies. */
#include <mail/e-mail-reader.h>
#include <mail/e-mail-display.h>
#include "translate-dom.h"
#include "translate-content.h"
#include "translate-utils.h"

#define DOM_STATE_KEY "evolution-translate-original"
#define MAX_CACHED_TRANSLATIONS 64

typedef struct {
    EMailPartList *original_part_list;
} DomState;

static GHashTable *s_translation_cache;

static void
free_dom_state (gpointer data)
{
    DomState *state = data;
    g_clear_object (&state->original_part_list);
    g_free (state);
}

static EMailDisplay *
get_display_from_shell_view (EShellView *shell_view)
{
    EMailReader *reader = translate_ref_reader_from_shell_view (shell_view);
    if (!reader)
        return NULL;
    EMailDisplay *display = e_mail_reader_get_mail_display (reader);
    g_object_unref (reader);
    return display;
}

static EMailDisplay *
get_display_from_reader (EMailReader *reader)
{
    return reader ? e_mail_reader_get_mail_display (reader) : NULL;
}

static gchar *
message_identity (EMailPartList *parts)
{
    if (!parts)
        return NULL;
    CamelFolder *folder = e_mail_part_list_get_folder (parts);
    const gchar *uid = e_mail_part_list_get_message_uid (parts);
    if (!folder || !uid || !*uid)
        return NULL; /* Standalone messages are identified by their part-list object. */
    CamelStore *store = camel_folder_get_parent_store (folder);
    g_autoptr(GVariant) identity = g_variant_ref_sink (g_variant_new ("(sss)",
        camel_service_get_uid (CAMEL_SERVICE (store)), camel_folder_get_full_name (folder), uid));
    return g_variant_print (identity, FALSE);
}

static gboolean
same_message (EMailPartList *first, EMailPartList *second)
{
    if (first == second)
        return first != NULL;
    g_autofree gchar *first_id = message_identity (first);
    g_autofree gchar *second_id = message_identity (second);
    return first_id && second_id && g_str_equal (first_id, second_id);
}

static gchar *
cache_key (EMailDisplay *display)
{
    g_autofree gchar *identity = message_identity (e_mail_display_get_part_list (display));
    if (!identity)
        return NULL;
    g_autofree gchar *provider = translate_utils_get_provider_id ();
    g_autofree gchar *language = translate_utils_get_target_language ();
    g_autoptr(GVariant) key = g_variant_ref_sink (g_variant_new ("(sssb)", identity, provider, language,
        g_settings_get_boolean (translate_utils_get_settings (), "preserve-format")));
    return g_variant_print (key, FALSE);
}

static void
clear_if_message_changed_internal (EMailDisplay *display)
{
    if (!display)
        return;
    DomState *state = g_object_get_data (G_OBJECT (display), DOM_STATE_KEY);
    if (state && !same_message (state->original_part_list,
                                e_mail_display_get_part_list (display)))
        g_object_set_data (G_OBJECT (display), DOM_STATE_KEY, NULL);
}

static gboolean
is_translated_internal (EMailDisplay *display)
{
    clear_if_message_changed_internal (display);
    return display && g_object_get_data (G_OBJECT (display), DOM_STATE_KEY) != NULL;
}

static void
apply_translation_internal (EMailDisplay *display, const gchar *html, gboolean verbose_logging)
{
    (void) verbose_logging;
    if (!display || !html)
        return;
    EMailPartList *parts = e_mail_display_get_part_list (display);
    if (!parts)
        return;
    clear_if_message_changed_internal (display);
    if (!g_object_get_data (G_OBJECT (display), DOM_STATE_KEY)) {
        DomState *state = g_new0 (DomState, 1);
        state->original_part_list = g_object_ref (parts);
        g_object_set_data_full (G_OBJECT (display), DOM_STATE_KEY, state, free_dom_state);
    }
    e_web_view_load_string (E_WEB_VIEW (display), html);
    g_autofree gchar *key = cache_key (display);
    if (key) {
        if (!s_translation_cache)
            s_translation_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
        if (g_hash_table_size (s_translation_cache) >= MAX_CACHED_TRANSLATIONS)
            g_hash_table_remove_all (s_translation_cache);
        g_hash_table_replace (s_translation_cache, g_steal_pointer (&key), g_strdup (html));
    }
}

static void
restore_original_internal (EMailDisplay *display, EMailReader *reader, EShellView *shell_view)
{
    (void) reader;
    (void) shell_view;
    clear_if_message_changed_internal (display);
    if (!display)
        return;
    DomState *state = g_object_get_data (G_OBJECT (display), DOM_STATE_KEY);
    if (!state)
        return;
    g_autofree gchar *key = cache_key (display);
    if (key && s_translation_cache)
        g_hash_table_remove (s_translation_cache, key);
    EMailPartList *original = g_object_ref (state->original_part_list);
    g_object_set_data (G_OBJECT (display), DOM_STATE_KEY, NULL);
    e_mail_display_set_part_list (display, original);
    e_mail_display_load (display, NULL);
    g_object_unref (original);
}

static gboolean
has_cached_translation_internal (EMailDisplay *display)
{
    if (!display || !s_translation_cache)
        return FALSE;
    g_autofree gchar *key = cache_key (display);
    if (!key)
        return FALSE;
    /* Applying may evict cache entries, so own the input before doing so. */
    g_autofree gchar *cached = g_strdup (g_hash_table_lookup (s_translation_cache, key));
    if (!cached)
        return FALSE;
    apply_translation_internal (display, cached, FALSE);
    return TRUE;
}

void translate_dom_apply_to_shell_view (EShellView *view, const gchar *html)
{
    apply_translation_internal (get_display_from_shell_view (view), html, FALSE);
}
void translate_dom_restore_original (EShellView *view)
{
    restore_original_internal (get_display_from_shell_view (view), NULL, view);
}
gboolean translate_dom_is_translated (EShellView *view)
{
    return is_translated_internal (get_display_from_shell_view (view));
}
void translate_dom_clear_if_message_changed (EShellView *view)
{
    clear_if_message_changed_internal (get_display_from_shell_view (view));
}
gboolean translate_dom_apply_cached (EShellView *view)
{
    return has_cached_translation_internal (get_display_from_shell_view (view));
}
void translate_dom_apply_to_reader (EMailReader *reader, const gchar *html)
{
    apply_translation_internal (get_display_from_reader (reader), html, FALSE);
}
void translate_dom_restore_original_reader (EMailReader *reader)
{
    restore_original_internal (get_display_from_reader (reader), reader, NULL);
}
gboolean translate_dom_is_translated_reader (EMailReader *reader)
{
    return is_translated_internal (get_display_from_reader (reader));
}
void translate_dom_clear_if_message_changed_reader (EMailReader *reader)
{
    clear_if_message_changed_internal (get_display_from_reader (reader));
}
gboolean translate_dom_apply_cached_reader (EMailReader *reader)
{
    return has_cached_translation_internal (get_display_from_reader (reader));
}
