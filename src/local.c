/*============================================================================
Copyright (c) 2015-2025 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "rc_gui.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define SET_LOCALE      SET_PREFIX "do_change_locale_rc_gui %s"
#define SET_TIMEZONE    SET_PREFIX "do_change_timezone_rc_gui %s"
#define SET_KEYBOARD    SET_PREFIX "do_change_keyboard_rc_gui %s"
#define GET_WIFI_CTRY   GET_PREFIX "get_wifi_country"
#define SET_WIFI_CTRY   SET_PREFIX "do_wifi_country %s"
#define WLAN_INTERFACES GET_PREFIX "list_wlan_interfaces"

#define LOC_NAME   0
#define LOC_LCODE  1
#define LOC_CCODE  2
#define LOC_LCCODE 3

#define TZ_NAME 0
#define TZ_PATH 1
#define TZ_AREA 2

#define KEY_NAME 0
#define KEY_CODE 1

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GObject *locale_btn, *timezone_btn, *keyboard_btn, *wifi_btn;
static GObject *loclang_cb, *loccount_cb, *locchar_cb;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb;
static GObject *keymodel_cb, *keylayout_cb, *keyvar_cb, *keyalayout_cb, *keyavar_cb, *keyshort_cb, *keyled_cb, *keyalt_btn;
static GObject *keybox5, *keybox6, *keybox7, *keybox8;

/* Lists for locale setting */
static GtkListStore *locale_list, *country_list, *charset_list;

/* Lists for timezone setting */
static GtkListStore *timezone_list, *tzcity_list;

/* Lists for keyboard setting */
static GtkListStore *model_list, *layout_list, *variant_list, *avariant_list, *toggle_list, *led_list;

static gboolean alt_keys;

static char gbuffer[512];

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static int has_wifi (void);
static void deunicode (char **str);
static void set_init (GtkTreeModel *model, GObject *cb, int pos, char *init);
static void set_init_sub (GtkTreeModel *model, GObject *cb, int pos, char *init);
static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static gboolean close_msg (gpointer data);
static void on_set_locale (GtkButton* btn, gpointer ptr);
static void read_locales (void);
static void country_changed (GtkComboBox *cb, char *ptr);
static void language_changed (GtkComboBox *cb, char *ptr);
static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static gboolean match_lang (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static gpointer locale_thread (gpointer data);
static void on_set_timezone (GtkButton* btn, gpointer ptr);
static void read_timezones (void);
static int tzfilter (const struct dirent *entry);
static void area_changed (GtkComboBox *cb, gpointer ptr);
static gboolean match_area (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static gpointer timezone_thread (gpointer data);
static void read_keyboards (void);
static void populate_toggles (void);
static void layout_changed (GtkComboBox *cb, GObject *cb2);
static void on_keyalt_toggle (GtkButton *btn, gpointer ptr);
static void keyalt_update (void);
static gpointer keyboard_thread (gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

static int has_wifi (void)
{
    char *res;
    int ret = 0;

    res = get_string (WLAN_INTERFACES);
    if (res && strlen (res) > 0) ret = 1;
    g_free (res);
    return ret;
}

static void deunicode (char **str)
{
    if (*str && strchr (*str, '<'))
    {
        int val;
        char *tmp = g_strdup (*str);
        char *pos = strchr (tmp, '<');

        if (sscanf (pos, "<U00%X>", &val) == 1)
        {
            *pos++ = val >= 0xC0 ? 0xC3 : 0xC2;
            *pos++ = val >= 0xC0 ? val - 0x40 : val;
            sprintf (pos, "%s", strchr (*str, '>') + 1);
            g_free (*str);
            *str = tmp;
        }
    }
}

static void set_init (GtkTreeModel *model, GObject *cb, int pos, char *init)
{
    GtkTreeIter iter;
    char *val;

    gtk_tree_model_get_iter_first (model, &iter);
    if (!init) gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
    else
    {
        while (1)
        {
            gtk_tree_model_get (model, &iter, pos, &val, -1);
            if (!g_strcmp0 (init, val))
            {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
                g_free (val);
                return;
            }
            g_free (val);
            if (!gtk_tree_model_iter_next (model, &iter)) break;
        }
    }

    // couldn't match - just choose the first option - should never happen, but...
    gtk_tree_model_get_iter_first (model, &iter);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
}

static void set_init_sub (GtkTreeModel *model, GObject *cb, int pos, char *init)
{
    GtkTreeIter iter;
    char *val;

    gtk_tree_model_get_iter_first (model, &iter);
    if (!init || *init == 0) gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
    else
    {
        while (1)
        {
            if (!gtk_tree_model_iter_next (model, &iter)) break;
            gtk_tree_model_get (model, &iter, pos, &val, -1);
            if (strstr (init, val))
            {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
                g_free (val);
                return;
            }
            g_free (val);
        }
    }

    // couldn't match - just choose the first option - should never happen, but...
    gtk_tree_model_get_iter_first (model, &iter);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
}

static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkTreeIter next = *iter;
    char *str1, *str2;
    gboolean res;

    if (!gtk_tree_model_iter_next (model, &next)) return TRUE;
    gtk_tree_model_get (model, iter, (intptr_t) data, &str1, -1);
    gtk_tree_model_get (model, &next, (intptr_t) data, &str2, -1);
    if (!g_strcmp0 (str1, str2)) res = FALSE;
    else res = TRUE;
    g_free (str1);
    g_free (str2);
    return res;
}

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Locale dialog                                                              */
/*----------------------------------------------------------------------------*/

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreeModel *model;
    GtkTreeModelSort *slang;
    GtkTreeModelFilter *flang;
    GtkTreeIter iter;
    char *buffer, *init_locale = NULL;

    // create and populate the locale database
    locale_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    country_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    charset_list = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    read_locales ();

    // create the dialog
    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    // create the combo boxes
    loclang_cb = (GObject *) gtk_builder_get_object (builder, "loccblang");
    loccount_cb = (GObject *) gtk_builder_get_object (builder, "loccbcountry");
    locchar_cb = (GObject *) gtk_builder_get_object (builder, "loccbchar");

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (loclang_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (loclang_cb), col, "text", LOC_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (loccount_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (loccount_cb), col, "text", LOC_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (locchar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (locchar_cb), col, "text", LOC_NAME);

    // get the current locale setting and save as init_locale
    init_locale = get_string ("grep LANG= /etc/default/locale | cut -d = -f 2");
    if (init_locale == NULL) init_locale = g_strdup ("en_GB.UTF-8");

    // filter and sort the master database
    slang = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (locale_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (slang), LOC_LCODE, GTK_SORT_ASCENDING);

    flang = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (slang), NULL));
    gtk_tree_model_filter_set_visible_func (flang, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) LOC_LCODE, NULL);

    // set up the language combo box from the sorted and filtered language list
    gtk_combo_box_set_model (GTK_COMBO_BOX (loclang_cb), GTK_TREE_MODEL (flang));

    buffer = g_strdup (init_locale);
    strtok (buffer, "_");
    set_init (GTK_TREE_MODEL (flang), loclang_cb, LOC_LCODE, buffer);
    g_free (buffer);

    // set the other combo boxes accordingly
    language_changed (GTK_COMBO_BOX (loclang_cb), init_locale);
    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);
    g_signal_connect (loclang_cb, "changed", G_CALLBACK (language_changed), NULL);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        // get the current charset code from the combo box
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (locchar_cb));
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (locchar_cb), &iter);
        gtk_tree_model_get (model, &iter, LOC_LCCODE, &buffer, -1);

        gtk_widget_destroy (dlg);

        if (g_strcmp0 (buffer, init_locale))
        {
            // warn about a short delay...
            message (_("Setting locale - please wait..."));

            strcpy (gbuffer, buffer);

            // launch a thread with the system call to update the generated locales
            g_thread_new (NULL, locale_thread, NULL);

            // set reboot flag
            needs_reboot = TRUE;
        }
        g_free (buffer);
    }
    else gtk_widget_destroy (dlg);

    g_free (init_locale);
    g_object_unref (locale_list);
    g_object_unref (country_list);
    g_object_unref (charset_list);
    g_object_unref (flang);
    g_object_unref (slang);
}

static void read_locales (void)
{
    char *cname, *lname, *buffer, *lang, *country, *charset, *loccode, *flname, *fcname;
    GtkTreeIter iter;
    FILE *fp;
    size_t len;

    // populate the locale database
    buffer = NULL;
    len = 0;
    fp = fopen ("/usr/share/i18n/SUPPORTED", "rb");
    while (getline (&buffer, &len, fp) > 0)
    {
        // split into l/c pair and charset
        loccode = strtok (buffer, " ");
        charset = strtok (NULL, " \t\n\r");

        if (loccode && charset)
        {
            // strip any extension
            lang = g_strdup (loccode);
            strtok (lang, ".");

            // lang now holds locale file name - read names from locale file
            cname = get_quoted_param ("/usr/share/i18n/locales", lang, "territory");
            lname = get_quoted_param ("/usr/share/i18n/locales", lang, "language");
            if (!lname && !cname)
            {
                g_free (lang);
                continue;
            }

            // deal with the likes of "malta"...
            if (cname) cname[0] = g_ascii_toupper (cname[0]);
            if (lname) lname[0] = g_ascii_toupper (lname[0]);

            // deal with Curacao and Bokmal
            deunicode (&cname);
            deunicode (&lname);

            // now split to language and country codes
            strtok (lang, "_");
            country = strtok (NULL, " \t\n\r");

            flname = g_strdup_printf ("%s (%s)", lang, lname);
            // purely to deal with esperanto - a language without a country, which is a clue as to just how pointless it is...
            if (country)
                fcname = g_strdup_printf ("%s (%s)", country, cname);
            else
                fcname = NULL;

            gtk_list_store_append (locale_list, &iter);
            gtk_list_store_set (locale_list, &iter, LOC_NAME, flname, LOC_LCODE, lang, -1);
            gtk_list_store_append (country_list, &iter);
            gtk_list_store_set (country_list, &iter, LOC_NAME, fcname, LOC_LCODE, lang, LOC_CCODE, country, -1);
            gtk_list_store_append (charset_list, &iter);
            gtk_list_store_set (charset_list, &iter, LOC_NAME, charset, LOC_LCODE, lang, LOC_CCODE, country, LOC_LCCODE, loccode, -1);

            g_free (cname);
            g_free (lname);
            g_free (lang);
            g_free (flname);
            g_free (fcname);
        }
    }
    fclose (fp);
    g_free (buffer);
}

static void country_changed (GtkComboBox *cb, char *ptr)
{
    GtkTreeModel *model;
    GtkTreeModelFilter *f1, *f2;
    GtkTreeModelSort *schar;
    GtkTreeIter iter;
    char *lstr = NULL, *cstr = NULL;

    // get the current language code from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (loclang_cb));
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (loclang_cb), &iter))
        gtk_tree_model_get (model, &iter, LOC_LCODE, &lstr, -1);

    // get the current country code from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (loccount_cb));
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (loccount_cb), &iter))
        gtk_tree_model_get (model, &iter, LOC_CCODE, &cstr, -1);

    // filter and sort the master database for entries matching this code
    f1 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (charset_list), NULL));
    gtk_tree_model_filter_set_visible_func (f1, (GtkTreeModelFilterVisibleFunc) match_lang, lstr, NULL);

    f2 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (f1), NULL));
    gtk_tree_model_filter_set_visible_func (f2, (GtkTreeModelFilterVisibleFunc) match_country, cstr, NULL);

    schar = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (f2)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (schar), LOC_NAME, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (locchar_cb), GTK_TREE_MODEL (schar));

    if (ptr == NULL) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
    else set_init (GTK_TREE_MODEL (schar), locchar_cb, LOC_LCCODE, ptr);

    g_object_unref (f1);
    g_object_unref (f2);
    g_object_unref (schar);
    g_free (lstr);
    g_free (cstr);
}

static void language_changed (GtkComboBox *cb, char *ptr)
{
    GtkTreeModel *model;
    GtkTreeModelFilter *f1, *f2;
    GtkTreeModelSort *scount;
    GtkTreeIter iter;
    char *lstr = NULL, *init, *init_count;

    // get the current language code from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (loclang_cb));
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (loclang_cb), &iter))
        gtk_tree_model_get (model, &iter, LOC_LCODE, &lstr, -1);

    // filter and sort the master database for entries matching this code
    f1 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (country_list), NULL));
    gtk_tree_model_filter_set_visible_func (f1, (GtkTreeModelFilterVisibleFunc) match_lang, lstr, NULL);

    f2 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (f1), NULL));
    gtk_tree_model_filter_set_visible_func (f2, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) LOC_CCODE, NULL);

    scount = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (f2)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scount), LOC_CCODE, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (loccount_cb), GTK_TREE_MODEL (scount));

    if (ptr == NULL) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);
    else
    {
        // parse the initial locale for a country code
        init = g_strdup (ptr);
        strtok (init, "_");
        init_count = strtok (NULL, " .");
        set_init (GTK_TREE_MODEL (scount), loccount_cb, LOC_CCODE, init_count);
        g_free (init);
    }

    // disable the combo box if it has no entries
    gtk_widget_set_sensitive (GTK_WIDGET (loccount_cb), gtk_tree_model_iter_n_children (GTK_TREE_MODEL (scount), NULL) > 1);

    g_object_unref (f1);
    g_object_unref (f2);
    g_object_unref (scount);
    g_free (lstr);
    country_changed (GTK_COMBO_BOX (loccount_cb), ptr);
}

static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, LOC_CCODE, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static gboolean match_lang (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, LOC_LCODE, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static gpointer locale_thread (gpointer data)
{
    vsystem (SET_LOCALE, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

/*----------------------------------------------------------------------------*/
/* Timezone dialog                                                            */
/*----------------------------------------------------------------------------*/

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreeModel *model;
    GtkTreeModelSort *stz;
    GtkTreeModelFilter *ftz;
    GtkTreeIter iter;
    char *init_tz = NULL, *buffer, *cptr;

    // create and populate the timezone database
    timezone_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    tzcity_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    read_timezones ();

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    tzarea_cb = (GObject *) gtk_builder_get_object (builder, "tzcbarea");
    tzloc_cb = (GObject *) gtk_builder_get_object (builder, "tzcbloc");

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tzarea_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tzarea_cb), col, "text", TZ_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tzloc_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tzloc_cb), col, "text", TZ_NAME);

    // read the current time zone
    init_tz = get_string ("cat /etc/timezone");
    if (init_tz == NULL) init_tz = g_strdup ("Europe/London");

    // filter and sort the master database
    stz = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (timezone_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (stz), TZ_NAME, GTK_SORT_ASCENDING);

    ftz = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (stz), NULL));
    gtk_tree_model_filter_set_visible_func (ftz, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) TZ_NAME, NULL);

    // set up the area combo box from the sorted and filtered timezone list
    gtk_combo_box_set_model (GTK_COMBO_BOX (tzarea_cb), GTK_TREE_MODEL (ftz));

    buffer = g_strdup (init_tz);
    cptr = buffer;
    while (*cptr++);
    while (cptr-- > buffer)
    {
        if (*cptr == '/')
        {
            *cptr = 0;
            break;
        }
    }
    cptr = buffer;
    while (*cptr++) if (*cptr == '_') *cptr = ' ';
    set_init (GTK_TREE_MODEL (ftz), tzarea_cb, TZ_NAME, buffer);
    g_free (buffer);

    // populate the location list and set the current location
    area_changed (GTK_COMBO_BOX (tzarea_cb), init_tz);
    g_signal_connect (tzarea_cb, "changed", G_CALLBACK (area_changed), NULL);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (tzloc_cb));
        if (gtk_tree_model_iter_n_children (model, NULL) > 1) gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tzloc_cb), &iter);
        else
        {
            model = gtk_combo_box_get_model (GTK_COMBO_BOX (tzarea_cb));
            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tzarea_cb), &iter);
        }
        gtk_tree_model_get (model, &iter, TZ_PATH, &buffer, -1);

        gtk_widget_destroy (dlg);

        if (g_strcmp0 (buffer, init_tz))
        {
            // warn about a short delay...
            message (_("Setting timezone - please wait..."));

            strcpy (gbuffer, buffer);

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }

        g_free (buffer);
    }
    else gtk_widget_destroy (dlg);

    g_free (init_tz);
    g_object_unref (timezone_list);
    g_object_unref (tzcity_list);
    g_object_unref (ftz);
    g_object_unref (stz);
}

static void read_timezones (void)
{
    char *buffer, *path, *area, *zone, *cptr;
    GtkTreeIter iter;
    struct dirent **filelist, *dp, **sfilelist, *sdp, **ssfilelist, *ssdp;
    int entries, entry, sentries, sentry, ssentries, ssentry;

    entries = scandir ("/usr/share/zoneinfo", &filelist, tzfilter, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        if (dp->d_type == DT_DIR)
        {
            buffer = g_strdup_printf ("/usr/share/zoneinfo/%s", dp->d_name);
            sentries = scandir (buffer, &sfilelist, tzfilter, alphasort);
            g_free (buffer);
            for (sentry = 0; sentry < sentries; sentry++)
            {
                sdp = sfilelist[sentry];
                if (sdp->d_type == DT_DIR)
                {
                    buffer = g_strdup_printf ("/usr/share/zoneinfo/%s/%s", dp->d_name, sdp->d_name);
                    ssentries = scandir (buffer, &ssfilelist, tzfilter, alphasort);
                    g_free (buffer);
                    for (ssentry = 0; ssentry < ssentries; ssentry++)
                    {
                        ssdp = ssfilelist[ssentry];
                        path = g_strdup_printf ("%s/%s/%s", dp->d_name, sdp->d_name, ssdp->d_name);
                        area = g_strdup_printf ("%s/%s", dp->d_name, sdp->d_name);
                        zone = g_strdup_printf ("%s", ssdp->d_name);
                        cptr = zone;
                        while (*cptr++) if (*cptr == '_') *cptr = ' ';
                        cptr = area;
                        while (*cptr++) if (*cptr == '_') *cptr = ' ';
                        gtk_list_store_append (timezone_list, &iter);
                        gtk_list_store_set (timezone_list, &iter, TZ_PATH, path, TZ_NAME, area, -1);
                        gtk_list_store_append (tzcity_list, &iter);
                        gtk_list_store_set (tzcity_list, &iter, TZ_PATH, path, TZ_AREA, area, TZ_NAME, zone, -1);
                        g_free (path);
                        g_free (area);
                        g_free (zone);
                    }
                }
                else
                {
                    path = g_strdup_printf ("%s/%s", dp->d_name, sdp->d_name);
                    zone = g_strdup_printf ("%s", sdp->d_name);
                    cptr = zone;
                    while (*cptr++) if (*cptr == '_') *cptr = ' ';
                    gtk_list_store_append (timezone_list, &iter);
                    gtk_list_store_set (timezone_list, &iter, TZ_PATH, path, TZ_NAME, dp->d_name, -1);
                    gtk_list_store_append (tzcity_list, &iter);
                    gtk_list_store_set (tzcity_list, &iter, TZ_PATH, path, TZ_AREA, dp->d_name, TZ_NAME, zone, -1);
                    g_free (path);
                    g_free (zone);
                }
            }
        }
        else
        {
            gtk_list_store_append (timezone_list, &iter);
            gtk_list_store_set (timezone_list, &iter, TZ_PATH, dp->d_name, TZ_NAME, dp->d_name, -1);
            gtk_list_store_append (tzcity_list, &iter);
            gtk_list_store_set (tzcity_list, &iter, TZ_PATH, dp->d_name, TZ_AREA, dp->d_name, TZ_NAME, NULL, -1);
        }
    }
}

static int tzfilter (const struct dirent *entry)
{
    if (entry->d_name[0] >= 'A' && entry->d_name[0] <= 'Z') return 1;
    return 0;
}

static void area_changed (GtkComboBox *cb, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeModelFilter *far;
    GtkTreeModelSort *sar;
    GtkTreeIter iter;
    char *str = NULL;

    // get the current area from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (tzarea_cb));
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tzarea_cb), &iter))
        gtk_tree_model_get (model, &iter, TZ_NAME, &str, -1);

    // filter and sort the master database for entries matching this code
    far = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (tzcity_list), NULL));
    gtk_tree_model_filter_set_visible_func (far, (GtkTreeModelFilterVisibleFunc) match_area, str, NULL);

    sar = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (far)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sar), TZ_AREA, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (tzloc_cb), GTK_TREE_MODEL (sar));

    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), 0);
    else set_init (GTK_TREE_MODEL (sar), tzloc_cb, TZ_PATH, ptr);

    // disable the combo box if it has no entries
    gtk_widget_set_sensitive (GTK_WIDGET (tzloc_cb), gtk_tree_model_iter_n_children (GTK_TREE_MODEL (sar), NULL) > 1);

    g_object_unref (far);
    g_object_unref (sar);
    g_free (str);

}

static gboolean match_area (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, TZ_AREA, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static gpointer timezone_thread (gpointer data)
{
    vsystem (SET_TIMEZONE, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

/*----------------------------------------------------------------------------*/
/* Keyboard dialog                                                            */
/*----------------------------------------------------------------------------*/

void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreeIter iter;
    char *init_model, *init_layout, *init_variant, *init_alayout, *init_avariant, *init_options;
    char *new_mod, *new_lay, *new_var, *new_alay, *new_avar, *new_opts, *new_opt[2];
    char *cptr;
    gboolean init_alt;

    init_model = NULL;
    init_layout = NULL;
    init_variant = NULL;
    init_alayout = NULL;
    init_avariant = NULL;
    init_options = NULL;

    // set up list stores for keyboard layouts
    model_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    layout_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    variant_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    avariant_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    toggle_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    led_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    read_keyboards ();
    populate_toggles ();

    // build the dialog and attach the combo boxes
    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "keyboarddlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    keymodel_cb = (GObject *) gtk_builder_get_object (builder, "keycbmodel");
    keylayout_cb = (GObject *) gtk_builder_get_object (builder, "keycblayout");
    keyvar_cb = (GObject *) gtk_builder_get_object (builder, "keycbvar");
    keyalayout_cb = (GObject *) gtk_builder_get_object (builder, "keycbalayout");
    keyavar_cb = (GObject *) gtk_builder_get_object (builder, "keycbavar");
    keyshort_cb = (GObject *) gtk_builder_get_object (builder, "keycbshortcut");
    keyled_cb = (GObject *) gtk_builder_get_object (builder, "keycbled");
    keyalt_btn = (GObject *) gtk_builder_get_object (builder, "keybtnalt");
    gtk_combo_box_set_model (GTK_COMBO_BOX (keymodel_cb), GTK_TREE_MODEL (model_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keylayout_cb), GTK_TREE_MODEL (layout_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keyvar_cb), GTK_TREE_MODEL (variant_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keyalayout_cb), GTK_TREE_MODEL (layout_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keyavar_cb), GTK_TREE_MODEL (avariant_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keyshort_cb), GTK_TREE_MODEL (toggle_list));
    gtk_combo_box_set_model (GTK_COMBO_BOX (keyled_cb), GTK_TREE_MODEL (led_list));
    keybox5 = (GObject *) gtk_builder_get_object (builder, "keyhbox5");
    keybox6 = (GObject *) gtk_builder_get_object (builder, "keyhbox6");
    keybox7 = (GObject *) gtk_builder_get_object (builder, "keyhbox7");
    keybox8 = (GObject *) gtk_builder_get_object (builder, "keyhbox8");

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keymodel_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keymodel_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keylayout_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keylayout_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyvar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyvar_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyalayout_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyalayout_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyavar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyavar_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyshort_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyshort_cb), col, "text", KEY_NAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyled_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyled_cb), col, "text", KEY_NAME);

    // get the current keyboard settings
    init_model = get_string ("grep XKBMODEL /etc/default/keyboard | cut -d = -f 2 | tr -d '\"'");
    if (init_model == NULL) init_model = g_strdup ("pc105");

    init_layout = get_string ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"'");
    if (init_layout == NULL) init_layout = g_strdup ("gb");

    init_variant = get_string ("grep XKBVARIANT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"'");
    if (init_variant == NULL) init_variant = g_strdup ("");

    init_options = get_string ("grep XKBOPTIONS /etc/default/keyboard | cut -d = -f 2 | tr -d '\"'");
    if (init_options == NULL) init_options = g_strdup ("");

    alt_keys = FALSE;
    cptr = strstr (init_layout, ",");
    if (cptr)
    {
        init_alayout = cptr + 1;
        *cptr = 0;
        alt_keys = TRUE;
    }
    else init_alayout = g_strdup (init_layout);

    cptr = strstr (init_variant, ",");
    if (cptr)
    {
        init_avariant = cptr + 1;
        *cptr = 0;
        alt_keys = TRUE;
    }
    else init_avariant = g_strdup (init_variant);
    init_alt = alt_keys;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (keyalt_btn), alt_keys);
    keyalt_update ();

    set_init (GTK_TREE_MODEL (model_list), keymodel_cb, KEY_CODE, init_model);

    g_signal_connect (keylayout_cb, "changed", G_CALLBACK (layout_changed), keyvar_cb);
    set_init (GTK_TREE_MODEL (layout_list), keylayout_cb, KEY_CODE, init_layout);
    set_init (GTK_TREE_MODEL (variant_list), keyvar_cb, KEY_CODE, init_variant);

    g_signal_connect (keyalayout_cb, "changed", G_CALLBACK (layout_changed), keyavar_cb);
    set_init (GTK_TREE_MODEL (layout_list), keyalayout_cb, KEY_CODE, init_alayout);
    set_init (GTK_TREE_MODEL (avariant_list), keyavar_cb, KEY_CODE, init_avariant);

    set_init_sub (GTK_TREE_MODEL (toggle_list), keyshort_cb, KEY_CODE, init_options);
    set_init_sub (GTK_TREE_MODEL (led_list), keyled_cb, KEY_CODE, init_options);

    g_signal_connect (keyalt_btn, "toggled", G_CALLBACK (on_keyalt_toggle), NULL);

    g_object_unref (builder);

    // run the dialog
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keymodel_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (model_list), &iter, KEY_CODE, &new_mod, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keylayout_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, KEY_CODE, &new_lay, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyvar_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (variant_list), &iter, KEY_CODE, &new_var, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyalayout_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, KEY_CODE, &new_alay, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyavar_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (avariant_list), &iter, KEY_CODE, &new_avar, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyshort_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (toggle_list), &iter, KEY_CODE, &new_opt[0], -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyled_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (led_list), &iter, KEY_CODE, &new_opt[1], -1);

        gtk_widget_destroy (dlg);

        char **options = (char **) malloc (sizeof (char *));
        int i, n_opts = 0;

        new_opts = g_strdup (init_options);
        cptr = strtok (new_opts, ",");
        while (cptr)
        {
            if (!strstr (cptr, "grp:") && !strstr (cptr, "grp_led:"))
            {
                options = (char **) realloc (options, (n_opts + 2) * sizeof (char *));
                options[n_opts] = g_strdup (cptr);
                n_opts++;
            }
            cptr = strtok (NULL, ",");
        }
        g_free (new_opts);

        for (i = 0; i < 2; i++)
        {
            if (*new_opt[i])
            {
                options = (char **) realloc (options, (n_opts + 2) * sizeof (char *));
                options[n_opts] = g_strdup (new_opt[i]);
                n_opts++;
            }
        }

        options[n_opts] = NULL;
        new_opts = g_strjoinv (",", options);

        for (i = 0; i < n_opts - 1; i++) g_free (options[i]);
        g_free (options);

        if (g_strcmp0 (init_model, new_mod) || g_strcmp0 (init_layout, new_lay) || g_strcmp0 (init_variant, new_var)
            || init_alt != alt_keys || g_strcmp0 (init_alayout, new_alay) || g_strcmp0 (init_avariant, new_avar)
            || g_strcmp0 (init_options, new_opts))
        {
            // warn about a short delay...
            if (!singledlg) message (_("Setting keyboard - please wait..."));

            if (alt_keys)
                sprintf (gbuffer, "\"%s\" \"%s,%s\" \"%s,%s\" \"%s\"", new_mod, new_lay, new_alay, new_var, new_avar, new_opts ? new_opts : "");
            else
                sprintf (gbuffer, "\"%s\" \"%s\" \"%s\" \"\"", new_mod, new_lay, new_var);

            // launch a thread with the system call to update the keyboard
            pthread = g_thread_new (NULL, keyboard_thread, NULL);

            if (singledlg)
            {
                // if running the standalone keyboard dialog, need a dialog for the message
                GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
                msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal_dlg");
                GtkWidget *lbl = (GtkWidget *) gtk_builder_get_object (builder, "modald_msg");
                g_object_unref (builder);

                gtk_label_set_text (GTK_LABEL (lbl), _("Setting keyboard - please wait..."));
                gtk_widget_show (msg_dlg);
                gtk_dialog_run (GTK_DIALOG (msg_dlg));
            }
        }

        g_free (new_mod);
        g_free (new_lay);
        g_free (new_var);
        g_free (new_opts);
    }
    else gtk_widget_destroy (dlg);

    g_free (init_model);
    g_free (init_layout);
    g_free (init_variant);
    g_free (init_options);
    g_object_unref (model_list);
    g_object_unref (layout_list);
    g_object_unref (variant_list);
    g_object_unref (avariant_list);
    g_object_unref (toggle_list);
    g_object_unref (led_list);
}

static void read_keyboards (void)
{
    FILE *fp;
    char *cptr, *t1, *t2;
    size_t siz;
    int in_list;
    GtkTreeIter iter;

    // loop through lines in KeyboardNames file
    cptr = NULL;
    in_list = 0;
    fp = fopen ("/usr/share/console-setup/KeyboardNames.pl", "rb");
    while (getline (&cptr, &siz, fp) > 0)
    {
        if (in_list)
        {
            if (cptr[0] == ')') in_list = 0;
            else
            {
                strtok (cptr, "'");
                t1 = strtok (NULL, "'");
                strtok (NULL, "'");
                t2 = strtok (NULL, "'");
                strtok (NULL, "'");
                if (strlen (t1) > 50)
                {
                    t1[47] = '.';
                    t1[48] = '.';
                    t1[49] = '.';
                    t1[50] = 0;
                }
                if (in_list == 1)
                {
                    gtk_list_store_append (model_list, &iter);
                    gtk_list_store_set (model_list, &iter, KEY_NAME, t1, KEY_CODE, t2, -1);
                }
                if (in_list == 2)
                {
                    gtk_list_store_append (layout_list, &iter);
                    gtk_list_store_set (layout_list, &iter, KEY_NAME, t1, KEY_CODE, t2, -1);
                }
            }
        }
        if (!strncmp ("%models", cptr, 7)) in_list = 1;
        if (!strncmp ("%layouts", cptr, 8)) in_list = 2;
    }
    fclose (fp);
    g_free (cptr);
}

static void populate_toggles (void)
{
    GtkTreeIter iter;

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("None"), KEY_CODE, "", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Alt + Space"), KEY_CODE, "grp:alt_space_toggle", -1); // overridden by wm

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Ctrl + Alt"), KEY_CODE, "grp:ctrl_alt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Ctrl + Shift"), KEY_CODE, "grp:ctrl_shift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Alt"), KEY_CODE, "grp:lalt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Alt + Caps"), KEY_CODE, "grp:alt_caps_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Alt + Left Shift"), KEY_CODE, "grp:lalt_lshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Alt + Right Alt"), KEY_CODE, "grp:alts_toggle", -1); // one way only

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Alt + Shift"), KEY_CODE, "grp:alt_shift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Ctrl"), KEY_CODE, "grp:lctrl_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Ctrl + Left Alt"), KEY_CODE, "grp:lctrl_lalt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Ctrl + Left Shift"), KEY_CODE, "grp:lctrl_lshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Ctrl + Left Win"), KEY_CODE, "grp:lctrl_lwin_toggle", -1); // triggers menu

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Ctrl + Right Ctrl"), KEY_CODE, "grp:ctrls_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Shift"), KEY_CODE, "grp:lshift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Shift + Right Shift"), KEY_CODE, "grp:shifts_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Left Win"), KEY_CODE, "grp:lwin_toggle", -1); // triggers menu

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Menu"), KEY_CODE, "grp:menu_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Alt"), KEY_CODE, "grp:toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Alt + Right Shift"), KEY_CODE, "grp:ralt_rshift_toggle", -1); // does nothing

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Ctrl"), KEY_CODE, "grp:rctrl_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Ctrl + Right Alt"), KEY_CODE, "grp:rctrl_ralt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Ctrl + Right Shift"), KEY_CODE, "grp:rctrl_rshift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Shift"), KEY_CODE, "grp:rshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Right Win"), KEY_CODE, "grp:rwin_toggle", -1); // can't test...

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Scroll Lock"), KEY_CODE, "grp:sclk_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Shift + Caps"), KEY_CODE, "grp:shift_caps_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, KEY_NAME, _("Win + Space"), KEY_CODE, "grp:win_space_toggle", -1); // triggers menu

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, KEY_NAME, _("None"), KEY_CODE, "", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, KEY_NAME, _("Caps"), KEY_CODE, "grp_led:caps", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, KEY_NAME, _("Num"), KEY_CODE, "grp_led:num", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, KEY_NAME, _("Scroll"), KEY_CODE, "grp_led:scroll", -1);
}

static void layout_changed (GtkComboBox *cb, GObject *cb2)
{
    FILE *fp;
    GtkTreeIter iter;
    GtkListStore *variant_list;
    char *buffer, *cptr, *t1, *t2;
    size_t siz;
    int in_list;

    // get the currently-set layout from the combo box
    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (gtk_combo_box_get_model (cb), &iter, KEY_NAME, &t1, KEY_CODE, &t2, -1);

    // reset the list of variants and add the layout name as a default
    variant_list = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (cb2)));
    gtk_list_store_clear (variant_list);
    gtk_list_store_append (variant_list, &iter);
    gtk_list_store_set (variant_list, &iter, KEY_NAME, t1, KEY_CODE, "", -1);
    buffer = g_strdup_printf ("    '%s'", t2);
    g_free (t1);
    g_free (t2);

    // parse the database file to find variants for this layout
    cptr = NULL;
    in_list = 0;
    fp = fopen ("/usr/share/console-setup/KeyboardNames.pl", "rb");
    while (getline (&cptr, &siz, fp) > 0)
    {
        if (in_list)
        {
            if (cptr[4] == '}') break;
            else
            {
                strtok (cptr, "'");
                t1 = strtok (NULL, "'");
                strtok (NULL, "'");
                t2 = strtok (NULL, "'");
                strtok (NULL, "'");
                if (in_list == 1)
                {
                    gtk_list_store_append (variant_list, &iter);
                    gtk_list_store_set (variant_list, &iter, KEY_NAME, t1, KEY_CODE, t2, -1);
                }
            }
        }
        if (!strncmp (buffer, cptr, strlen (buffer))) in_list = 1;
    }
    fclose (fp);
    g_free (cptr);
    g_free (buffer);

    set_init (GTK_TREE_MODEL (variant_list), cb2, 1, NULL);
}

static void on_keyalt_toggle (GtkButton *btn, gpointer ptr)
{
    alt_keys = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    keyalt_update ();
}

static void keyalt_update (void)
{
    gtk_widget_set_visible (GTK_WIDGET (keybox5), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox6), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox7), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox8), alt_keys);
}

static gpointer keyboard_thread (gpointer ptr)
{
    vsystem (SET_KEYBOARD, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

/*----------------------------------------------------------------------------*/
/* Wi-fi country dialog                                                       */
/*----------------------------------------------------------------------------*/

void on_set_wifi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char *buffer, *cnow, *cptr;
    FILE *fp;
    int n, found;
    size_t len;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "wcdlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    wccountry_cb = (GObject *) gtk_builder_get_object (builder, "wccbcountry");

    // get the current country setting
    cnow = get_string (GET_WIFI_CTRY);
    if (!cnow) cnow = g_strdup_printf ("00");

    // populate the combobox
    fp = fopen ("/usr/share/zoneinfo/iso3166.tab", "rb");
    found = 0;
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (wccountry_cb), _("<not set>"));
    n = 1;
    buffer = NULL;
    len = 0;
    while (getline (&buffer, &len, fp) > 0)
    {
        if (buffer[0] != 0x0A && buffer[0] != '#')
        {
            buffer[strlen(buffer) - 1] = 0;
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (wccountry_cb), buffer);
            if (!strncmp (cnow, buffer, 2)) found = n;
            n++;
        }
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (wccountry_cb), found);
    g_free (buffer);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        // update the wpa_supplicant.conf file
        cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (wccountry_cb));
        if (!g_strcmp0 (cptr, _("<not set>")))
            vsystem (SET_WIFI_CTRY, "00");
        else if (strncmp (cnow, cptr, 2))
        {
            strncpy (cnow, cptr, 2);
            cnow[2] = 0;
            vsystem (SET_WIFI_CTRY, cnow);
        }
        if (cptr) g_free (cptr);
    }
    g_free (cnow);
    gtk_widget_destroy (dlg);
}

/*----------------------------------------------------------------------------*/
/* Tab setup                                                                  */
/*----------------------------------------------------------------------------*/

void load_localisation_tab (GtkBuilder *builder)
{
    locale_btn = gtk_builder_get_object (builder, "button_loc");
    g_signal_connect (locale_btn, "clicked", G_CALLBACK (on_set_locale), NULL);

    timezone_btn = gtk_builder_get_object (builder, "button_tz");
    g_signal_connect (timezone_btn, "clicked", G_CALLBACK (on_set_timezone), NULL);

    textdomain (GETTEXT_PACKAGE);
    keyboard_btn = gtk_builder_get_object (builder, "button_kb");
    g_signal_connect (keyboard_btn, "clicked", G_CALLBACK (on_set_keyboard), NULL);

    wifi_btn = gtk_builder_get_object (builder, "button_wifi");
    g_signal_connect (wifi_btn, "clicked", G_CALLBACK (on_set_wifi), NULL);

    if (has_wifi ()) gtk_widget_set_sensitive (GTK_WIDGET (wifi_btn), TRUE);
    else gtk_widget_set_sensitive (GTK_WIDGET (wifi_btn), FALSE);
}

/* End of file */
/*----------------------------------------------------------------------------*/
