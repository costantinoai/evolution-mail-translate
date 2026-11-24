#include "../src/translate-content.c"

static CamelMimeMessage *
message_from_string (const gchar *raw)
{
    CamelMimeMessage *message = camel_mime_message_new ();
    g_autoptr(CamelStream) stream = camel_stream_mem_new_with_buffer (raw, strlen (raw));
    g_autoptr(GError) error = NULL;
    g_assert_true (camel_data_wrapper_construct_from_stream_sync (
        CAMEL_DATA_WRAPPER (message), stream, NULL, &error));
    g_assert_no_error (error);
    return message;
}

static void
skip_attached_multipart (void)
{
    const gchar *raw =
        "MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=outer\r\n\r\n"
        "--outer\r\nContent-Type: multipart/alternative; boundary=attached\r\n"
        "Content-Disposition: attachment; filename=attached.mime\r\n\r\n"
        "--attached\r\nContent-Type: text/html\r\n\r\n<p>ATTACHMENT</p>\r\n"
        "--attached--\r\n--outer\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n"
        "Message body\r\n--outer--\r\n";
    g_autoptr(CamelMimeMessage) message = message_from_string (raw);
    CamelMimePart *html = NULL, *plain = NULL;
    find_body_parts (CAMEL_MIME_PART (message), &html, &plain, NULL);
    g_assert_null (html);
    g_assert_nonnull (plain);
    g_autofree gchar *body = decode_part_to_utf8 (plain);
    g_assert_nonnull (strstr (body, "Message body"));
}

static void
decode_charset_and_escape_plain (void)
{
    g_autoptr(CamelMimeMessage) message = message_from_string (
        "Content-Type: text/plain; charset=ISO-8859-1\r\n\r\nCaf\xe9 <test>\nnext");
    g_autofree gchar *text = decode_part_to_utf8 (CAMEL_MIME_PART (message));
    g_assert_true (g_utf8_validate (text, -1, NULL));
    g_autofree gchar *html = plain_to_html (text);
    g_assert_nonnull (strstr (html, "Café &lt;test&gt;<br>next"));
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/mime/skip-attached-multipart", skip_attached_multipart);
    g_test_add_func ("/mime/charset-and-escaping", decode_charset_and_escape_plain);
    return g_test_run ();
}
