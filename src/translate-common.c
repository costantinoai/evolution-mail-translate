/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translation requests and activity feedback shared by both mail views. */
#include <glib/gi18n-lib.h>
#include <e-util/e-util.h>
#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>
#include "translate-common.h"
#include "translate-utils.h"
#include "translate-content.h"
#include "translate-dom.h"

#define PENDING_REQUEST_KEY "evolution-translate-pending"

typedef struct {
    GWeakRef reader;
    EMailPartList *parts;
    GCancellable *cancellable;
    gchar *provider;
    gchar *language;
} ReaderRequest;

static void
reader_request_free (ReaderRequest *request)
{
    g_weak_ref_clear (&request->reader);
    g_clear_object (&request->parts);
    g_clear_object (&request->cancellable);
    g_free (request->provider);
    g_free (request->language);
    g_free (request);
}

static void
cancel_and_unref (gpointer data)
{
    g_cancellable_cancel (data);
    g_object_unref (data);
}

static void
cancel_display_request (GtkWidget *display, gpointer user_data)
{
    (void) user_data;
    g_object_set_data (G_OBJECT (display), PENDING_REQUEST_KEY, NULL);
}

static void
display_message_changed (GObject *display, GParamSpec *pspec, gpointer user_data)
{
    (void) pspec;
    (void) user_data;
    cancel_display_request (GTK_WIDGET (display), NULL);
}

void
translate_common_cancel_reader (EMailReader *reader)
{
    EMailDisplay *display = e_mail_reader_get_mail_display (reader);
    if (display)
        cancel_display_request (GTK_WIDGET (display), NULL);
}

static void
refresh_reader_actions (EMailReader *reader)
{
    GtkWindow *window = e_mail_reader_get_window (reader);
    if (E_IS_SHELL_WINDOW (window)) {
        EShellView *view = e_shell_window_get_shell_view (E_SHELL_WINDOW (window), "mail");
        if (view)
            e_shell_view_update_actions (view);
    } else {
        e_mail_reader_update_actions (reader, e_mail_reader_check_state (reader));
    }
}

void
translate_common_restore_reader (EMailReader *reader)
{
    translate_common_cancel_reader (reader);
    translate_dom_restore_original_reader (reader);
    refresh_reader_actions (reader);
}

static void
reader_translation_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void) source;
    ReaderRequest *request = user_data;
    g_autofree gchar *translated = NULL;
    g_autoptr(GError) error = NULL;
    gboolean success = translate_common_translate_finish (result, &translated, &error);
    g_autoptr(GObject) object = g_weak_ref_get (&request->reader);
    EMailReader *reader = object ? E_MAIL_READER (object) : NULL;
    EMailDisplay *display = reader ? e_mail_reader_get_mail_display (reader) : NULL;
    if (display && g_object_get_data (G_OBJECT (display), PENDING_REQUEST_KEY) == request->cancellable) {
        /* Remove the display's ref without cancelling a completed request. */
        g_object_unref (g_object_steal_data (G_OBJECT (display), PENDING_REQUEST_KEY));
        g_autofree gchar *provider = translate_utils_get_provider_id ();
        g_autofree gchar *language = translate_utils_get_target_language ();
        gboolean still_current = e_mail_display_get_part_list (display) == request->parts &&
            g_strcmp0 (provider, request->provider) == 0 &&
            g_strcmp0 (language, request->language) == 0;
        if (still_current) {
            if (success) {
                translate_dom_apply_to_reader (reader, translated);
                refresh_reader_actions (reader);
                const gchar *notice = g_object_get_data (G_OBJECT (result), "translate-fallback-notice");
                if (notice) {
                    GtkWidget *dialog = gtk_message_dialog_new (e_mail_reader_get_window (reader),
                        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
                        "%s", _("Translated with Argos offline"));
                    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", notice);
                    g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
                    gtk_widget_show (dialog);
                }
            } else
                translate_common_show_error (e_mail_reader_get_window (reader), error);
        }
    }
    reader_request_free (request);
}

void
translate_common_translate_reader (EMailReader *reader, EShellBackend *backend)
{
    g_return_if_fail (E_IS_MAIL_READER (reader));
    EMailDisplay *display = e_mail_reader_get_mail_display (reader);
    if (!display)
        return;
    if (g_object_get_data (G_OBJECT (display), PENDING_REQUEST_KEY)) {
        translate_common_cancel_reader (reader);
        return;
    }
    translate_dom_clear_if_message_changed_reader (reader);
    if (translate_dom_is_translated_reader (reader)) {
        translate_common_restore_reader (reader);
        return;
    }
    if (translate_dom_apply_cached_reader (reader)) {
        refresh_reader_actions (reader);
        return;
    }
    EMailPartList *parts = e_mail_display_get_part_list (display);
    g_autofree gchar *body = translate_get_selected_message_body_html_from_reader (reader);
    if (!parts || !body || !*body) {
        g_autoptr(GError) error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            _("No message body is available to translate. Wait for the message to finish loading."));
        translate_common_show_error (e_mail_reader_get_window (reader), error);
        return;
    }
    if (!g_object_get_data (G_OBJECT (display), "evolution-translate-signals")) {
        g_signal_connect (display, "destroy", G_CALLBACK (cancel_display_request), NULL);
        g_signal_connect (display, "notify::part-list", G_CALLBACK (display_message_changed), NULL);
        g_object_set_data (G_OBJECT (display), "evolution-translate-signals", GINT_TO_POINTER (1));
    }
    ReaderRequest *request = g_new0 (ReaderRequest, 1);
    g_weak_ref_init (&request->reader, G_OBJECT (reader));
    request->parts = g_object_ref (parts);
    request->cancellable = g_cancellable_new ();
    request->provider = translate_utils_get_provider_id ();
    request->language = translate_utils_get_target_language ();
    g_object_set_data_full (G_OBJECT (display), PENDING_REQUEST_KEY,
        g_object_ref (request->cancellable), cancel_and_unref);
    translate_common_translate_async_full (body, backend, request->cancellable,
                                           reader_translation_done, request);
}

typedef struct {
    EActivity *activity;
    gchar *body;
    gchar *target;
    gchar *online_error;
    GObject *fallback;
} CommonRequest;

static void
common_request_free (gpointer data)
{
    CommonRequest *request = data;
    g_clear_object (&request->activity);
    g_clear_object (&request->fallback);
    g_free (request->body);
    g_free (request->target);
    g_free (request->online_error);
    g_free (request);
}

static void
provider_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    CommonRequest *request = g_task_get_task_data (task);
    g_autofree gchar *translated = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = translate_provider_translate_finish (
        (TranslateProvider *) source, result, &translated, &error);
    gboolean cancelled = error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    if (!ok && !cancelled && !request->fallback &&
        g_strcmp0 (translate_provider_get_id ((TranslateProvider *) source), "argos") != 0) {
        request->fallback = translate_provider_new_by_id ("argos");
        if (request->fallback) {
            request->online_error = g_strdup (error ? error->message : _("The online provider failed."));
            /* Fallback may use installed models; it must not start a model download. */
            g_object_set_data (request->fallback, "translate-installed-models-only", GINT_TO_POINTER (1));
            if (request->activity)
                e_activity_set_text (request->activity, _("Online translation unavailable; trying Argos offline…"));
            translate_provider_translate_async ((TranslateProvider *) request->fallback,
                request->body, TRUE, NULL, request->target, g_task_get_cancellable (task),
                provider_done, g_object_ref (task));
            return;
        }
    }
    if (request->activity) {
        e_activity_set_text (request->activity, ok
            ? (request->fallback ? _("Translated with Argos offline") : _("Translation complete"))
            : _("Translation failed"));
        e_activity_set_state (request->activity, ok ? E_ACTIVITY_COMPLETED : E_ACTIVITY_CANCELLED);
    }
    if (ok) {
        if (request->fallback)
            g_object_set_data_full (G_OBJECT (task), "translate-fallback-notice",
                g_strdup (request->online_error), g_free);
        g_task_return_pointer (task, g_steal_pointer (&translated), g_free);
    } else if (request->online_error && !cancelled) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
            "%s\n\nOffline fallback also failed: %s", request->online_error,
            error ? error->message : _("No suitable installed Argos model."));
    } else if (error) {
        g_task_return_error (task, g_steal_pointer (&error));
    } else {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Translation failed.");
    }
}

void
translate_common_translate_async_full (const gchar *body_html,
                                        EShellBackend *shell_backend,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    g_autofree gchar *provider_id = translate_utils_get_provider_id ();
    g_autofree gchar *target_lang = translate_utils_get_target_language ();
    g_autoptr(GObject) provider = translate_provider_new_by_id (provider_id);
    GTask *task = g_task_new (provider, cancellable, callback, user_data);
    if (!provider) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                 "Unknown translation provider: %s. Select a provider in Translate Settings.", provider_id);
        g_object_unref (task);
        return;
    }
    if (!body_html || !*body_html) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                 "No message body is available to translate.");
        g_object_unref (task);
        return;
    }
    CommonRequest *request = g_new0 (CommonRequest, 1);
    request->body = g_strdup (body_html);
    request->target = g_strdup (target_lang);
    g_task_set_task_data (task, request, common_request_free);
    if (shell_backend) {
        EActivity *activity = e_activity_new ();
        g_autofree gchar *message = g_strdup_printf (
            _("Requesting translation from %s…"),
            translate_provider_get_name ((TranslateProvider *) provider));
        e_activity_set_text (activity, message);
        e_activity_set_state (activity, E_ACTIVITY_RUNNING);
        e_activity_set_cancellable (activity, cancellable);
        e_shell_backend_add_activity (shell_backend, activity);
        request->activity = activity;
    }
    translate_provider_translate_async ((TranslateProvider *) provider,
        body_html, TRUE, NULL, target_lang, cancellable, provider_done, task);
}

void
translate_common_translate_async (const gchar *body_html,
                                   GAsyncReadyCallback callback, gpointer user_data)
{
    translate_common_translate_async_full (body_html, NULL, NULL, callback, user_data);
}

void
translate_common_translate_async_with_activity (const gchar *body_html,
                                                 EShellBackend *shell_backend,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data)
{
    translate_common_translate_async_full (body_html, shell_backend, NULL, callback, user_data);
}

gboolean
translate_common_translate_finish (GAsyncResult *result, gchar **translated, GError **error)
{
    g_return_val_if_fail (G_IS_TASK (result), FALSE);
    gchar *value = g_task_propagate_pointer (G_TASK (result), error);
    if (!value)
        return FALSE;
    if (translated)
        *translated = value;
    else
        g_free (value);
    return TRUE;
}

void
translate_common_show_error (GtkWindow *parent, const GError *error)
{
    if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
    GtkWidget *dialog = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", _("Translation failed"));
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
        error ? error->message : _("The translation provider returned no result."));
    g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
    gtk_widget_show (dialog);
}
