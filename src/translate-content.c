/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate content extraction helpers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <camel/camel.h>
#include <mail/e-mail-view.h>
#include <mail/e-mail-paned-view.h>
#include <mail/message-list.h>

#include "translate-content.h"

static gboolean
content_type_is (CamelMimePart *part, const gchar *type, const gchar *subtype)
{
    CamelContentType *ct = camel_mime_part_get_content_type (part);
    return ct && camel_content_type_is (ct, type, subtype);
}

static gboolean
is_attachment (CamelMimePart *part, const CamelContentType *parent_ct)
{
    const CamelContentDisposition *cd = camel_mime_part_get_content_disposition (part);
    CamelContentType *ct = camel_mime_part_get_content_type (part);
    return camel_content_disposition_is_attachment_ex (cd, ct, parent_ct);
}

static void
find_body_parts (CamelMimePart *part,
                 CamelMimePart **best_html,
                 CamelMimePart **best_plain,
                 const CamelContentType *parent_ct)
{
    CamelDataWrapper *dw = camel_medium_get_content (CAMEL_MEDIUM (part));
    if (CAMEL_IS_MULTIPART (dw)) {
        CamelMultipart *mp = (CamelMultipart*)dw;
        gint n = camel_multipart_get_number (mp);
        for (gint i = 0; i < n; i++) {
            CamelMimePart *child = camel_multipart_get_part (mp, i);
            find_body_parts (child, best_html, best_plain,
                             camel_mime_part_get_content_type (part));
        }
        return;
    }

    if (is_attachment (part, parent_ct))
        return;

    if (content_type_is (part, "text", "html")) {
        if (!*best_html)
            *best_html = part;
        return;
    }
    if (content_type_is (part, "text", "plain")) {
        if (!*best_plain)
            *best_plain = part;
        return;
    }
}

static gchar *
decode_part_to_utf8 (CamelMimePart *part)
{
    CamelDataWrapper *dw = camel_medium_get_content (CAMEL_MEDIUM (part));
    g_autoptr(CamelStreamMem) mem = (CamelStreamMem*)camel_stream_mem_new ();
    g_autoptr(GError) error = NULL;
    camel_data_wrapper_decode_to_stream_sync (dw, (CamelStream*)mem, NULL, &error);
    if (error) return NULL;
    GByteArray *arr = camel_stream_mem_get_byte_array (mem);
    if (!arr || !arr->data) return NULL;
    CamelContentType *ct = camel_mime_part_get_content_type (part);
    const gchar *charset = ct ? camel_content_type_param (ct, "charset") : NULL;
    if (charset && *charset && g_ascii_strcasecmp (charset, "utf-8") != 0) {
        g_autoptr(GError) conv_err = NULL;
        gsize written = 0;
        gchar *out = g_convert ((const gchar*)arr->data, arr->len, "UTF-8", charset, NULL, &written, &conv_err);
        if (out)
            return out;
    }
    return g_strndup ((const gchar*)arr->data, arr->len);
}

static gchar *
plain_to_html (const gchar *text)
{
    if (!text) return NULL;
    g_autofree gchar *esc = g_markup_escape_text (text, -1);
    GString *s = g_string_new (NULL);
    for (const gchar *p = esc; *p; p++) {
        if (*p == '\n') g_string_append (s, "<br>");
        else g_string_append_c (s, *p);
    }
    return g_string_free (s, FALSE);
}

gchar *
translate_get_selected_message_body_html_from_reader (EMailReader *reader)
{
    EMailView *mail_view = NULL;
    g_autoptr(CamelFolder) folder = NULL;
    g_autoptr(GPtrArray) selected_uids = NULL;

    g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

    /* For EMailBrowser, reader itself owns selection; for EMailPanedView, get from view */
    if (E_IS_MAIL_PANED_VIEW (reader)) {
        mail_view = E_MAIL_VIEW (reader);
        selected_uids = e_mail_reader_get_selected_uids (reader);
        folder = e_mail_reader_ref_folder (reader);
    } else {
        selected_uids = e_mail_reader_get_selected_uids (reader);
        folder = e_mail_reader_ref_folder (reader);
    }

    if (!folder || !selected_uids || selected_uids->len == 0)
        return NULL;

    const gchar *uid = (const gchar*)g_ptr_array_index (selected_uids, 0);
    g_autoptr(GError) error = NULL;
    CamelMimeMessage *msg = camel_folder_get_message_sync (folder, uid, NULL, &error);
    if (!msg)
        return NULL;

    CamelMimePart *top = CAMEL_MIME_PART (msg);
    CamelMimePart *best_html = NULL, *best_plain = NULL;
    find_body_parts (top, &best_html, &best_plain, camel_mime_part_get_content_type (top));

    if (best_html) {
        return decode_part_to_utf8 (best_html);
    } else if (best_plain) {
        g_autofree gchar *plain = decode_part_to_utf8 (best_plain);
        return plain_to_html (plain);
    }
    return NULL;
}

gchar *
translate_get_selected_message_body_html_from_shell_view (EShellView *shell_view)
{
    EShellContent *shell_content;
    EMailView *mail_view = NULL;
    shell_content = e_shell_view_get_shell_content (shell_view);
    g_object_get (shell_content, "mail-view", &mail_view, NULL);
    if (!mail_view)
        return NULL;
    gchar *html = translate_get_selected_message_body_html_from_reader (E_MAIL_READER (mail_view));
    g_object_unref (mail_view);
    return html;
}
