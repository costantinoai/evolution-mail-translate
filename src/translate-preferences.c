/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Translate Preferences dialog implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "translate-preferences.h"
#include "translate-utils.h"

typedef struct {
    const char *code;
    const char *name;
} Lang;

static const Lang k_langs[] = {
    {"en", N_("English")}, {"es", N_("Spanish")}, {"fr", N_("French")},
    {"de", N_("German")}, {"it", N_("Italian")}, {"pt", N_("Portuguese")},
    {"nl", N_("Dutch")}, {"sv", N_("Swedish")}, {"da", N_("Danish")},
    {"no", N_("Norwegian")}, {"fi", N_("Finnish")}, {"pl", N_("Polish")},
    {"ru", N_("Russian")}, {"uk", N_("Ukrainian")}, {"cs", N_("Czech")},
    {"sk", N_("Slovak")}, {"hu", N_("Hungarian")}, {"ro", N_("Romanian")},
    {"bg", N_("Bulgarian")}, {"el", N_("Greek")}, {"tr", N_("Turkish")},
    {"ar", N_("Arabic")}, {"he", N_("Hebrew")}, {"hi", N_("Hindi")},
    {"ja", N_("Japanese")}, {"ko", N_("Korean")}, {"zh", N_("Chinese")},
};

void
translate_preferences_show (GtkWindow *parent)
{
    g_autoptr(GSettings) settings = g_settings_new ("org.gnome.evolution.translate");
    GSettings *provider_settings = translate_utils_get_provider_settings ();

    GtkWidget *dlg = gtk_dialog_new_with_buttons (_("Translate Settings"), parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
        _("Cancel"), GTK_RESPONSE_CANCEL,
        _("Save"), GTK_RESPONSE_OK,
        NULL);

    GtkWidget *area = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
    GtkWidget *grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
    gtk_container_add (GTK_CONTAINER (area), grid);

    GtkWidget *lbl_lang = gtk_label_new (_("Target language:"));
    gtk_widget_set_halign (lbl_lang, GTK_ALIGN_START);
    GtkWidget *combo = gtk_combo_box_text_new ();
    for (guint i = 0; i < G_N_ELEMENTS (k_langs); i++)
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), k_langs[i].code, k_langs[i].name);
    g_autofree gchar *cur = g_settings_get_string (settings, "target-language");
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), cur && *cur ? cur : "en");

    GtkWidget *lbl_provider = gtk_label_new (_("Provider:"));
    gtk_widget_set_halign (lbl_provider, GTK_ALIGN_START);
    GtkWidget *provider_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (provider_combo), "argos", "Argos Translate (offline, privacy-focused)");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (provider_combo), "google", "Google Translate (online, free, recommended)");
    g_autofree gchar *cur_provider = g_settings_get_string (settings, "provider-id");
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (provider_combo), cur_provider && *cur_provider ? cur_provider : "google");

    GtkWidget *lbl_venv = gtk_label_new (_("Argos venv path (optional):"));
    gtk_widget_set_halign (lbl_venv, GTK_ALIGN_START);
    GtkWidget *venv_entry = gtk_entry_new ();

    GtkWidget *install_on_demand = gtk_check_button_new_with_label (_("Install models on demand"));
    /* Load current install-on-demand setting */
    gboolean current_install_on_demand = g_settings_get_boolean (provider_settings, "install-on-demand");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (install_on_demand), current_install_on_demand);

    gtk_grid_attach (GTK_GRID (grid), lbl_lang, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_provider, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), provider_combo, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_venv, 0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), venv_entry, 1, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), install_on_demand, 1, 3, 1, 1);

    gtk_widget_show_all (dlg);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK) {
        /* Save target language setting */
        const gchar *sel = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo));
        if (sel && *sel)
            g_settings_set_string (settings, "target-language", sel);

        /* Save provider selection */
        const gchar *sel_provider = gtk_combo_box_get_active_id (GTK_COMBO_BOX (provider_combo));
        if (sel_provider && *sel_provider)
            g_settings_set_string (settings, "provider-id", sel_provider);

        /* Save install-on-demand setting */
        gboolean install_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (install_on_demand));
        g_settings_set_boolean (provider_settings, "install-on-demand", install_enabled);

        /* venv_entry not yet implemented */
        (void)venv_entry;
    }
    gtk_widget_destroy (dlg);
}
