#include "../src/translate-dom.c"

int main (int argc, char **argv)
{
    gtk_init (&argc, &argv);
    GtkWidget *widget = e_mail_display_new (NULL);
    g_object_ref_sink (widget);
    EMailDisplay *display = E_MAIL_DISPLAY (widget);
    g_autoptr(CamelMimeMessage) message_a = camel_mime_message_new ();
    g_autoptr(CamelMimeMessage) message_b = camel_mime_message_new ();
    EMailPartList *first = e_mail_part_list_new (message_a, "1", NULL);
    EMailPartList *second = e_mail_part_list_new (message_b, "1", NULL);
    e_mail_display_set_part_list (display, first);
    apply_translation_internal (display, "<p>Translated first message</p>", FALSE);
    gboolean applied = is_translated_internal (display);
    e_mail_display_set_part_list (display, second);
    clear_if_message_changed_internal (display);
    gboolean stale = is_translated_internal (display);
    g_print ("first applied=%d; unrelated message translated=%d\n", applied, stale);
    gtk_widget_destroy (widget);
    g_object_unref (widget);
    g_object_unref (first);
    g_object_unref (second);
    return applied && !stale ? 0 : 1;
}
