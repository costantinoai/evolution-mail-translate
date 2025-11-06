/* Translate DOM helpers - Manages DOM state during translation
 *
 * This module handles:
 * - Storing original message state before translation
 * - Applying translated HTML to the display
 * - Restoring original messages
 * - Detecting message changes to clear stale translations
 *
 * Note: All duplication has been eliminated using helper functions.
 * Public functions come in pairs (_shell_view and _reader variants)
 * but they all delegate to shared internal helpers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <mail/e-mail-reader.h>
#include <mail/e-mail-view.h>
#include <mail/e-mail-paned-view.h>
#include <mail/e-mail-display.h>
#include <e-util/e-util.h>

#include "translate-dom.h"

/* Internal state structure to track original message */
typedef struct {
    EMailPartList *original_part_list;
    CamelMimeMessage *original_message;
    gchar *original_message_uid;
} DomState;

/* Global state table: EMailDisplay* → DomState* */
static GHashTable *s_states;

/* Free a DomState structure */
static void
free_dom_state (gpointer data)
{
    DomState *st = data;
    if (st) {
        g_clear_object (&st->original_part_list);
        g_clear_object (&st->original_message);
        g_free (st->original_message_uid);
        g_free (st);
    }
}

/* Ensure the global state table exists */
static void
ensure_state_table (void)
{
    if (!s_states) {
        s_states = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_dom_state);
    }
}

/* Extract EMailDisplay from EShellView */
static EMailDisplay *
get_display_from_shell_view (EShellView *shell_view)
{
    EShellContent *shell_content;
    EMailView *mail_view = NULL;
    shell_content = e_shell_view_get_shell_content (shell_view);
    g_object_get (shell_content, "mail-view", &mail_view, NULL);
    if (!mail_view)
        return NULL;
    EMailDisplay *display = e_mail_reader_get_mail_display (E_MAIL_READER (mail_view));
    g_object_unref (mail_view);
    return display;
}

/* Extract EMailDisplay from EMailReader */
static EMailDisplay *
get_display_from_reader (EMailReader *reader)
{
    return e_mail_reader_get_mail_display (reader);
}

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS (DRY - No Duplication)
 * These functions work with EMailDisplay* directly
 * ============================================================================ */

/**
 * apply_translation_internal:
 * @display: The EMailDisplay to apply translation to
 * @translated_html: The translated HTML content
 * @verbose_logging: Whether to log detailed messages
 *
 * Internal helper that applies translated HTML to a display.
 * Handles state management, message change detection, and content loading.
 * This is the single source of truth for apply logic.
 */
static void
apply_translation_internal (EMailDisplay *display,
                            const gchar *translated_html,
                            gboolean verbose_logging)
{
    ensure_state_table ();
    if (!display) return;

    /* Get current message UID */
    EMailPartList *current_part_list = e_mail_display_get_part_list (display);
    const gchar *current_uid = NULL;
    if (current_part_list) {
        current_uid = e_mail_part_list_get_message_uid (current_part_list);
    }

    /* Check if we already have state for this display */
    DomState *existing_state = g_hash_table_lookup (s_states, display);

    /* If state exists but it's for a different message, clear it first */
    if (existing_state) {
        gboolean is_same_message = FALSE;
        if (current_uid && existing_state->original_message_uid) {
            is_same_message = (g_strcmp0 (current_uid, existing_state->original_message_uid) == 0);
        }

        if (!is_same_message) {
            /* Different message - remove old state */
            g_message ("[translate] Clearing old translation state for different message");
            g_hash_table_remove (s_states, display);
            existing_state = NULL;
        }
    }

    /* Create new state if needed */
    if (!existing_state) {
        DomState *st = g_new0 (DomState, 1);
        st->original_part_list = current_part_list;
        if (st->original_part_list) {
            g_object_ref (st->original_part_list);
            if (current_uid) {
                st->original_message_uid = g_strdup (current_uid);
            }
        }
        g_hash_table_insert (s_states, display, st);

        if (verbose_logging) {
            g_message ("[translate] Created new translation state for message UID: %s",
                       current_uid ? current_uid : "(none)");
        }
    }

    /* Load translated HTML directly into the web view */
    e_web_view_load_string (E_WEB_VIEW (display), translated_html ? translated_html : "");

    if (verbose_logging) {
        g_message ("[translate] Applied translated content (%zu bytes) to preview",
                   translated_html ? strlen (translated_html) : 0UL);
    }
}

/**
 * restore_original_internal:
 * @display: The EMailDisplay to restore
 * @reader: Optional EMailReader for reload (can be NULL)
 * @shell_view: Optional EShellView for reload (can be NULL)
 *
 * Internal helper that restores the original message.
 * This is the single source of truth for restore logic.
 * Either reader or shell_view should be provided for reload functionality.
 */
static void
restore_original_internal (EMailDisplay *display,
                           EMailReader *reader,
                           EShellView *shell_view)
{
    ensure_state_table ();
    if (!display) return;

    DomState *st = g_hash_table_lookup (s_states, display);
    if (!st) return;

    /* Force reload of the original message */
    if (st->original_part_list) {
        /* Set the part list back and force a complete reload */
        e_mail_display_set_part_list (display, st->original_part_list);
        e_mail_display_load (display, NULL);

        /* Reload using appropriate method */
        if (reader) {
            /* Direct reader reload */
            e_mail_reader_reload (reader);
        } else if (shell_view) {
            /* Get reader from shell view and reload */
            EShellContent *shell_content = e_shell_view_get_shell_content (shell_view);
            EMailView *mail_view = NULL;
            g_object_get (shell_content, "mail-view", &mail_view, NULL);
            if (mail_view && E_IS_MAIL_READER (mail_view)) {
                e_mail_reader_reload (E_MAIL_READER (mail_view));
            }
            if (mail_view)
                g_object_unref (mail_view);
        }
    }

    g_hash_table_remove (s_states, display);
    g_message ("[translate] Restored original content");
}

/**
 * is_translated_internal:
 * @display: The EMailDisplay to check
 *
 * Internal helper that checks if a display has translation state.
 * This is the single source of truth for translation check logic.
 *
 * Returns: TRUE if the display has active translation state
 */
static gboolean
is_translated_internal (EMailDisplay *display)
{
    ensure_state_table ();
    if (!display) return FALSE;
    return g_hash_table_contains (s_states, display);
}

/**
 * clear_if_message_changed_internal:
 * @display: The EMailDisplay to check
 *
 * Internal helper that clears translation state if the message has changed.
 * This is the single source of truth for message change detection logic.
 */
static void
clear_if_message_changed_internal (EMailDisplay *display)
{
    ensure_state_table ();
    if (!display) return;

    /* Check if we have translation state for this display */
    DomState *existing_state = g_hash_table_lookup (s_states, display);
    if (!existing_state) return;

    /* Get the current message UID from the display */
    EMailPartList *current_part_list = e_mail_display_get_part_list (display);
    const gchar *current_uid = NULL;
    if (current_part_list) {
        current_uid = e_mail_part_list_get_message_uid (current_part_list);
    }

    /* Check if the stored UID matches the current UID */
    gboolean is_same_message = FALSE;
    if (current_uid && existing_state->original_message_uid) {
        is_same_message = (g_strcmp0 (current_uid, existing_state->original_message_uid) == 0);
    }

    /* If the message has changed, clear the stale translation state */
    if (!is_same_message) {
        g_message ("[translate] Message changed (stored: %s, current: %s) - clearing stale translation state",
                   existing_state->original_message_uid ? existing_state->original_message_uid : "(none)",
                   current_uid ? current_uid : "(none)");
        g_hash_table_remove (s_states, display);
    }
}

/* ============================================================================
 * PUBLIC API - SHELL VIEW VARIANTS
 * These are thin wrappers that extract EMailDisplay and call internal helpers
 * ============================================================================ */

/**
 * translate_dom_apply_to_shell_view:
 * @shell_view: The EShellView containing the message
 * @translated_html: The translated HTML to display
 *
 * Applies translated HTML to the mail display in a shell view.
 * Stores state to enable restoration of the original message.
 */
void
translate_dom_apply_to_shell_view (EShellView *shell_view,
                                   const gchar *translated_html)
{
    EMailDisplay *display = get_display_from_shell_view (shell_view);
    apply_translation_internal (display, translated_html, TRUE);
}

/**
 * translate_dom_restore_original:
 * @shell_view: The EShellView containing the message
 *
 * Restores the original message in a shell view, removing translation.
 */
void
translate_dom_restore_original (EShellView *shell_view)
{
    EMailDisplay *display = get_display_from_shell_view (shell_view);
    restore_original_internal (display, NULL, shell_view);
}

/**
 * translate_dom_is_translated:
 * @shell_view: The EShellView to check
 *
 * Checks if the message in a shell view is currently translated.
 *
 * Returns: TRUE if translated, FALSE otherwise
 */
gboolean
translate_dom_is_translated (EShellView *shell_view)
{
    EMailDisplay *display = get_display_from_shell_view (shell_view);
    return is_translated_internal (display);
}

/**
 * translate_dom_clear_if_message_changed:
 * @shell_view: The EShellView to check
 *
 * Clears translation state if the displayed message has changed.
 * This prevents stale translations from persisting.
 */
void
translate_dom_clear_if_message_changed (EShellView *shell_view)
{
    EMailDisplay *display = get_display_from_shell_view (shell_view);
    clear_if_message_changed_internal (display);
}

/* ============================================================================
 * PUBLIC API - READER VARIANTS
 * These are thin wrappers that extract EMailDisplay and call internal helpers
 * ============================================================================ */

/**
 * translate_dom_apply_to_reader:
 * @reader: The EMailReader containing the message
 * @translated_html: The translated HTML to display
 *
 * Applies translated HTML to the mail display in a reader.
 * Stores state to enable restoration of the original message.
 */
void
translate_dom_apply_to_reader (EMailReader *reader,
                                const gchar *translated_html)
{
    EMailDisplay *display = get_display_from_reader (reader);
    apply_translation_internal (display, translated_html, FALSE);
}

/**
 * translate_dom_restore_original_reader:
 * @reader: The EMailReader containing the message
 *
 * Restores the original message in a reader, removing translation.
 */
void
translate_dom_restore_original_reader (EMailReader *reader)
{
    EMailDisplay *display = get_display_from_reader (reader);
    restore_original_internal (display, reader, NULL);
}

/**
 * translate_dom_is_translated_reader:
 * @reader: The EMailReader to check
 *
 * Checks if the message in a reader is currently translated.
 *
 * Returns: TRUE if translated, FALSE otherwise
 */
gboolean
translate_dom_is_translated_reader (EMailReader *reader)
{
    EMailDisplay *display = get_display_from_reader (reader);
    return is_translated_internal (display);
}

/**
 * translate_dom_clear_if_message_changed_reader:
 * @reader: The EMailReader to check
 *
 * Clears translation state if the displayed message has changed.
 * This prevents stale translations from persisting.
 */
void
translate_dom_clear_if_message_changed_reader (EMailReader *reader)
{
    EMailDisplay *display = get_display_from_reader (reader);
    clear_if_message_changed_internal (display);
}
