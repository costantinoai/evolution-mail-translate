/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Configurable shortcut adapted from MoDD0/moddo-evolution-translator,
 * commit 6865134515f7e52458eadfc62b30cb94f867f404 (LGPL-2.1-or-later).
 * Modified by Andrea Costantino on 2026-11-24: use GTK's accelerator editor. */
/* Translate Preferences dialog implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "translate-preferences.h"
#include "translate-utils.h"
#include "providers/translate-provider.h"

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

static void
shortcut_edited (GtkCellRendererAccel *cell, const gchar *path, guint keyval,
                 GdkModifierType modifiers, guint hardware_keycode, GtkListStore *store)
{
    (void) cell;
    (void) hardware_keycode;
    GtkTreeIter iter;
    if (gtk_accelerator_valid (keyval, modifiers) &&
        gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path))
        gtk_list_store_set (store, &iter, 0, keyval, 1, modifiers, -1);
}

static void
shortcut_cleared (GtkCellRendererAccel *cell, const gchar *path, GtkListStore *store)
{
    (void) cell;
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path))
        gtk_list_store_set (store, &iter, 0, 0, 1, 0, -1);
}

static GtkWidget *
shortcut_editor (const gchar *accel)
{
    guint keyval;
    GdkModifierType modifiers;
    gtk_accelerator_parse (accel, &keyval, &modifiers);
    GtkListStore *store = gtk_list_store_new (2, G_TYPE_UINT, G_TYPE_UINT);
    GtkTreeIter iter;
    gtk_list_store_insert_with_values (store, &iter, 0, 0, keyval, 1, modifiers, -1);
    GtkWidget *view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
    GtkCellRenderer *cell = gtk_cell_renderer_accel_new ();
    g_object_set (cell, "editable", TRUE, "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (
        NULL, cell, "accel-key", 0, "accel-mods", 1, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
    g_signal_connect_object (cell, "accel-edited", G_CALLBACK (shortcut_edited), store, 0);
    g_signal_connect_object (cell, "accel-cleared", G_CALLBACK (shortcut_cleared), store, 0);
    g_object_unref (store);
    gtk_widget_set_tooltip_text (view, _("Click to change the shortcut; Backspace clears it."));
    return view;
}

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
    g_auto(GStrv) provider_ids = translate_provider_list_ids ();
    for (guint i = 0; provider_ids && provider_ids[i]; i++) {
        g_autoptr(GObject) provider = translate_provider_new_by_id (provider_ids[i]);
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (provider_combo), provider_ids[i],
            translate_provider_get_name ((TranslateProvider *) provider));
    }
    g_autofree gchar *cur_provider = g_settings_get_string (settings, "provider-id");
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (provider_combo), cur_provider && *cur_provider ? cur_provider : "google");

    GtkWidget *lbl_venv = gtk_label_new (_("Python environment (optional):"));
    gtk_widget_set_halign (lbl_venv, GTK_ALIGN_START);
    GtkWidget *venv_entry = gtk_entry_new ();
    g_autofree gchar *venv_path = g_settings_get_string (provider_settings, "venv-path");
    gtk_entry_set_text (GTK_ENTRY (venv_entry), venv_path);

    GtkWidget *install_on_demand = gtk_check_button_new_with_label (_("Install models on demand"));
    /* Load current install-on-demand setting */
    gboolean current_install_on_demand = g_settings_get_boolean (provider_settings, "install-on-demand");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (install_on_demand), current_install_on_demand);

    g_autofree gchar *current_shortcut = g_settings_get_string (settings, "translate-shortcut");
    GtkWidget *shortcut = shortcut_editor (current_shortcut);
    GtkWidget *lbl_shortcut = gtk_label_new (_("Translate shortcut:"));
    gtk_widget_set_halign (lbl_shortcut, GTK_ALIGN_START);

    gtk_grid_attach (GTK_GRID (grid), lbl_lang, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_provider, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), provider_combo, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_venv, 0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), venv_entry, 1, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), install_on_demand, 1, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_shortcut, 0, 4, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), shortcut, 1, 4, 1, 1);

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

        g_settings_set_string (provider_settings, "venv-path",
                               gtk_entry_get_text (GTK_ENTRY (venv_entry)));
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (shortcut));
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            guint keyval, modifiers;
            gtk_tree_model_get (model, &iter, 0, &keyval, 1, &modifiers, -1);
            g_autofree gchar *accel = keyval ? gtk_accelerator_name (keyval, modifiers) : g_strdup ("");
            g_settings_set_string (settings, "translate-shortcut", accel);
        }
    }
    gtk_widget_destroy (dlg);
}
