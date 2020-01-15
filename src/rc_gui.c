/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:1
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.1
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.1
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
*/

/* NOTE raspi-config nonint functions obey sh return codes - 0 is in general success / yes / selected, 1 is failed / no / not selected */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <libintl.h>

/* Command strings */
#define GET_CAN_EXPAND  "raspi-config nonint get_can_expand"
#define EXPAND_FS       "raspi-config nonint do_expand_rootfs"
#define GET_HOSTNAME    "raspi-config nonint get_hostname"
#define SET_HOSTNAME    "raspi-config nonint do_hostname %s"
#define GET_BOOT_CLI    "raspi-config nonint get_boot_cli"
#define GET_AUTOLOGIN   "raspi-config nonint get_autologin"
#define SET_BOOT_CLI    "raspi-config nonint do_boot_behaviour B1"
#define SET_BOOT_CLIA   "raspi-config nonint do_boot_behaviour B2"
#define SET_BOOT_GUI    "raspi-config nonint do_boot_behaviour B3"
#define SET_BOOT_GUIA   "raspi-config nonint do_boot_behaviour B4"
#define GET_BOOT_WAIT   "raspi-config nonint get_boot_wait"
#define SET_BOOT_WAIT   "raspi-config nonint do_boot_wait %d"
#define GET_SPLASH      "raspi-config nonint get_boot_splash"
#define SET_SPLASH      "raspi-config nonint do_boot_splash %d"
#define GET_OVERSCAN    "raspi-config nonint get_overscan"
#define SET_OVERSCAN    "raspi-config nonint do_overscan %d"
#define GET_PIXDUB      "raspi-config nonint get_pixdub"
#define SET_PIXDUB      "raspi-config nonint do_pixdub %d"
#define GET_CAMERA      "raspi-config nonint get_camera"
#define SET_CAMERA      "raspi-config nonint do_camera %d"
#define GET_SSH         "raspi-config nonint get_ssh"
#define SET_SSH         "raspi-config nonint do_ssh %d"
#define GET_VNC         "raspi-config nonint get_vnc"
#define SET_VNC         "raspi-config nonint do_vnc %d"
#define GET_SPI         "raspi-config nonint get_spi"
#define SET_SPI         "raspi-config nonint do_spi %d"
#define GET_I2C         "raspi-config nonint get_i2c"
#define SET_I2C         "raspi-config nonint do_i2c %d"
#define GET_SERIAL      "raspi-config nonint get_serial"
#define GET_SERIALHW    "raspi-config nonint get_serial_hw"
#define SET_SERIAL      "raspi-config nonint do_serial %d"
#define GET_1WIRE       "raspi-config nonint get_onewire"
#define SET_1WIRE       "raspi-config nonint do_onewire %d"
#define GET_RGPIO       "raspi-config nonint get_rgpio"
#define SET_RGPIO       "raspi-config nonint do_rgpio %d"
#define GET_BLANK       "raspi-config nonint get_blanking"
#define SET_BLANK       "raspi-config nonint do_blanking %d"
#define GET_PI_TYPE     "raspi-config nonint get_pi_type"
#define IS_PI4          "raspi-config nonint is_pifour"
#define GET_FKMS        "raspi-config nonint is_fkms"
#define GET_OVERCLOCK   "raspi-config nonint get_config_var arm_freq /boot/config.txt"
#define SET_OVERCLOCK   "raspi-config nonint do_overclock %s"
#define GET_GPU_MEM     "raspi-config nonint get_config_var gpu_mem /boot/config.txt"
#define GET_GPU_MEM_256 "raspi-config nonint get_config_var gpu_mem_256 /boot/config.txt"
#define GET_GPU_MEM_512 "raspi-config nonint get_config_var gpu_mem_512 /boot/config.txt"
#define GET_GPU_MEM_1K  "raspi-config nonint get_config_var gpu_mem_1024 /boot/config.txt"
#define SET_GPU_MEM     "raspi-config nonint do_memory_split %d"
#define GET_HDMI_GROUP  "raspi-config nonint get_config_var hdmi_group /boot/config.txt"
#define GET_HDMI_MODE   "raspi-config nonint get_config_var hdmi_mode /boot/config.txt"
#define SET_HDMI_GP_MOD "raspi-config nonint do_resolution %d %d"
#define GET_WIFI_CTRY   "raspi-config nonint get_wifi_country"
#define SET_WIFI_CTRY   "raspi-config nonint do_wifi_country %s"
#define SET_PI4_4KH     "raspi-config nonint do_pi4video V1"
#define SET_PI4_ATV     "raspi-config nonint do_pi4video V2"
#define SET_PI4_NONE    "raspi-config nonint do_pi4video V3"
#define GET_PI4_VID     "raspi-config nonint get_pi4video"
#define GET_OVERLAYNOW  "raspi-config nonint get_overlay_now"
#define GET_OVERLAY     "raspi-config nonint get_overlay_conf"
#define GET_BOOTRO      "raspi-config nonint get_bootro_conf"
#define SET_OFS_ON      "raspi-config nonint enable_overlayfs"
#define SET_OFS_OFF     "raspi-config nonint disable_overlayfs"
#define SET_BOOTP_RO    "raspi-config nonint enable_bootro"
#define SET_BOOTP_RW    "raspi-config nonint disable_bootro"
#define WLAN_INTERFACES "raspi-config nonint list_wlan_interfaces"
#define DEFAULT_GPU_MEM "vcgencmd get_mem gpu | cut -d = -f 2 | cut -d M -f 1"
#define CHANGE_PASSWD   "echo \"$SUDO_USER:%s\" | chpasswd"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *res_btn, *locale_btn, *timezone_btn, *keyboard_btn, *wifi_btn, *ofs_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb, *pixdub_on_rb, *pixdub_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb, *rgpio_on_rb, *rgpio_off_rb, *vnc_on_rb, *vnc_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb, *onewire_on_rb, *onewire_off_rb;
static GObject *autologin_cb, *netwait_cb, *splash_on_rb, *splash_off_rb, *scons_on_rb, *scons_off_rb, *h4k_on_rb, *h4k_off_rb;
static GObject *analog_on_rb, *analog_off_rb, *blank_on_rb, *blank_off_rb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb, *ofs_en_rb, *ofs_dis_rb, *bp_ro_rb, *bp_rw_rb, *ofs_lbl;
static GObject *pwentry1_tb, *pwentry2_tb, *pwentry3_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb, *resolution_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb, *keymodel_cb, *keylayout_cb, *keyvar_cb;

static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static char *orig_hostname;
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_spi, orig_i2c, orig_serial, orig_scons, orig_splash;
static int orig_clock, orig_gpumem, orig_autolog, orig_netwait, orig_onewire, orig_rgpio, orig_vnc, orig_pixdub, orig_pi4v;
static int orig_ofs, orig_bpro, orig_blank;

/* Reboot flag set after locale change */

static int needs_reboot, ovfs_rb;

/* Number of items in comboboxes */

static int loc_count;

/* Globals accessed from multiple threads */

static char gbuffer[512];
GThread *pthread;

/* Lists for keyboard setting */

GtkListStore *model_list, *layout_list, *variant_list;

/* List for locale setting */

GtkListStore *locale_list, *timezone_list;

/* Helpers */

static int vsystem (const char *fmt, ...)
{
    char *cmdline;
    int res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = system (cmdline);
    g_free (cmdline);
    return res;
}

static FILE *vpopen (const char *fmt, ...)
{
    char *cmdline;
    FILE *res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = popen (cmdline, "r");
    g_free (cmdline);
    return res;
}

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char *buf = NULL;
    int res = 0, val = 0;

    if (fp == NULL) return 0;
    if (getline (&buf, &res, fp) > 0)
    {
        if (sscanf (buf, "%d", &res) == 1)
        {
            val = res;
        }
    }
    pclose (fp);
    g_free (buf);
    return val;
}

static char *get_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    int len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return NULL;
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res)
        {
            if (g_ascii_isspace (*res)) *res = 0;
            res++;
        }
        res = g_strdup (line);
    }
    pclose (fp);
    g_free (line);
    return res;
}

static int get_total_mem (void)
{
    FILE *fp;
    char *buf;
    int arm, gpu, len;

    fp = popen ("vcgencmd get_mem arm", "r");
    if (fp == NULL) return 0;
    buf = NULL;
    len = 0;
    while (getline (&buf, &len, fp) > 0)
        sscanf (buf, "arm=%dM", &arm);
    pclose (fp);
    g_free (buf);

    fp = popen ("vcgencmd get_mem gpu", "r");
    if (fp == NULL) return 0;
    buf = NULL;
    len = 0;
    while (getline (&buf, &len, fp) > 0)
        sscanf (buf, "gpu=%dM", &gpu);
    pclose (fp);
    g_free (buf);

    return arm + gpu;    
}

static int get_gpu_mem (void)
{
    int mem, tmem = get_total_mem ();
    if (tmem > 512)
        mem = get_status (GET_GPU_MEM_1K);
    else if (tmem > 256)
        mem = get_status (GET_GPU_MEM_512);
    else
        mem = get_status (GET_GPU_MEM_256);

    if (mem == 0) mem = get_status (GET_GPU_MEM);
    if (mem == 0) mem = get_status (DEFAULT_GPU_MEM);
    if (mem == 0) mem = 64;
    return mem;
}

static char *get_quoted_param (char *path, char *fname, char *toseek)
{
    char *pathname, *linebuf, *cptr, *dptr, *res;
    int len;

    pathname = g_strdup_printf ("%s/%s", path, fname);
    FILE *fp = fopen (pathname, "rb");
    g_free (pathname);
    if (!fp) return NULL;

    linebuf = NULL;
    len = 0;
    while (getline (&linebuf, &len, fp) > 0)
    {
        // skip whitespace at line start
        cptr = linebuf;
        while (*cptr == ' ' || *cptr == '\t') cptr++;

        // compare against string to find
        if (!strncmp (cptr, toseek, strlen (toseek)))
        {
            // find string in quotes
            strtok (cptr, "\"");
            dptr = strtok (NULL, "\"\n\r");

            // copy to dest
            if (dptr) res = g_strdup (dptr);
            else res = NULL;

            // done
            g_free (linebuf);
            fclose (fp);
            return res;
        }
    }

    // end of file with no match
    g_free (linebuf);
    fclose (fp);
    return NULL;
}

static void message (char *msg)
{
    GdkColor col;
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "msg");
    gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_eb");
    gdk_color_parse ("#FFFFFF", &col);
    gtk_widget_modify_bg (wid, GTK_STATE_NORMAL, &col);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_lbl");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_bb");

    gtk_widget_show_all (msg_dlg);
    g_object_unref (builder);
}

/* Password setting */

static void set_passwd (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb))) && g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), 
        gtk_entry_get_text (GTK_ENTRY(pwentry3_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

static void escape_passwd (const char *in, char **out)
{
    const char *ip;
    char *op;

    ip = in;
    *out = malloc (2 * strlen (in) + 1);    // allocate for worst case...
    op = *out;
    while (*ip)
    {
        if (*ip == '$' || *ip == '"' || *ip == '\\' || *ip == '`')
            *op++ = '\\';
        *op++ = *ip++;
    }
    *op = 0;
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;
    const char *entry;
    char *pw1, *pw2;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "passwddialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
    pwentry3_tb = gtk_builder_get_object (builder, "pwentry3");
    gtk_entry_set_visibility (GTK_ENTRY (pwentry2_tb), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwentry3_tb), FALSE);
    g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (set_passwd), NULL);
    g_signal_connect (pwentry3_tb, "changed", G_CALLBACK (set_passwd), NULL);
    pwok_btn = gtk_builder_get_object (builder, "passwdok");
    gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        escape_passwd (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), &pw1);
        escape_passwd (gtk_entry_get_text (GTK_ENTRY (pwentry3_tb)), &pw2);
        res = vsystem (CHANGE_PASSWD, pw1);
        g_free (pw1);
        g_free (pw2);
        gtk_widget_destroy (dlg);
        if (res)
            dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwbaddialog");
        else
            dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwokdialog");
        gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
        gtk_dialog_run (GTK_DIALOG (dlg));
        gtk_widget_destroy (dlg);
    }
    else gtk_widget_destroy (dlg);
    g_object_unref (builder);
}

/* Locale setting */

static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkTreeIter next = *iter;
    char *str1, *str2;
    gboolean res;

    if (!gtk_tree_model_iter_next (model, &next)) return TRUE;
    gtk_tree_model_get (model, iter, (int) data, &str1, -1);
    gtk_tree_model_get (model, &next, (int) data, &str2, -1);
    if (!g_strcmp0 (str1, str2)) res = FALSE;
    else res = TRUE;
    g_free (str1);
    g_free (str2);
    return res;
}

static gboolean match_lang (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, 0, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, 1, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
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

static void read_locales (void)
{
    char *cname, *lname, *buffer, *lang, *country, *charset, *loccode, *flname, *fcname;
    GtkTreeIter iter;
    FILE *fp;
    int len;

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

            // deal with the likes of "malta"...
            if (cname) cname[0] = g_ascii_toupper (cname[0]);
            if (lname) lname[0] = g_ascii_toupper (lname[0]);

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
            gtk_list_store_set (locale_list, &iter, 0, lang, 1, country, 2, charset, 3, loccode, 4, fcname, 5, flname, -1);
            g_free (cname);
            g_free (lname);
            g_free (lang);
            g_free (flname);
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
        gtk_tree_model_get (model, &iter, 0, &lstr, -1);

    // get the current country code from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (loccount_cb));
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (loccount_cb), &iter))
        gtk_tree_model_get (model, &iter, 1, &cstr, -1);

    // filter and sort the master database for entries matching this code
    f1 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (locale_list), NULL));
    gtk_tree_model_filter_set_visible_func (f1, (GtkTreeModelFilterVisibleFunc) match_lang, lstr, NULL);

    f2 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (f1), NULL));
    gtk_tree_model_filter_set_visible_func (f2, (GtkTreeModelFilterVisibleFunc) match_country, cstr, NULL);

    schar = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (f2)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (schar), 2, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (locchar_cb), GTK_TREE_MODEL (schar));

    if (ptr == NULL) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
    else set_init (GTK_TREE_MODEL (schar), locchar_cb, 3, ptr);

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
        gtk_tree_model_get (model, &iter, 0, &lstr, -1);

    // filter and sort the master database for entries matching this code
    f1 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (locale_list), NULL));
    gtk_tree_model_filter_set_visible_func (f1, (GtkTreeModelFilterVisibleFunc) match_lang, lstr, NULL);

    f2 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (f1), NULL));
    gtk_tree_model_filter_set_visible_func (f2, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) 1, NULL);

    scount = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (f2)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scount), 1, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (loccount_cb), GTK_TREE_MODEL (scount));

    if (ptr == NULL) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);
    else
    {
        // parse the initial locale for a country code
        init = g_strdup (ptr);
        strtok (init, "_");
        init_count = strtok (NULL, " .");
        set_init (GTK_TREE_MODEL (scount), loccount_cb, 1, init_count);
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

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

static gpointer locale_thread (gpointer data)
{
    vsystem ("locale-gen");
    vsystem ("LC_ALL=%s LANG=%s LANGUAGE=%s update-locale LANG=%s LC_ALL=%s LANGUAGE=%s", gbuffer, gbuffer, gbuffer, gbuffer, gbuffer, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
    FILE *fp;
    GtkBuilder *builder;
    GtkWidget *dlg, *tab;
    GtkCellRenderer *col;
    GtkTreeModel *model;
    GtkTreeModelSort *slang;
    GtkTreeModelFilter *flang;
    GtkTreeIter iter;
    char *str, *buffer, *init_locale = NULL;
    int len;

    // create and populate the locale database
    locale_list = gtk_list_store_new (6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    read_locales ();

    // create the dialog
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    tab = (GtkWidget *) gtk_builder_get_object (builder, "loctable");

    // create the combo boxes
    loclang_cb = (GObject *) gtk_combo_box_new ();
    loccount_cb = (GObject *) gtk_combo_box_new ();
    locchar_cb = (GObject *) gtk_combo_box_new ();
    gtk_table_attach (GTK_TABLE (tab), GTK_WIDGET (loclang_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (tab), GTK_WIDGET (loccount_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (tab), GTK_WIDGET (locchar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (loclang_cb));
    gtk_widget_show_all (GTK_WIDGET (loccount_cb));
    gtk_widget_show_all (GTK_WIDGET (locchar_cb));

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (loclang_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (loclang_cb), col, "text", 5);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (loccount_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (loccount_cb), col, "text", 4);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (locchar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (locchar_cb), col, "text", 2);

    // get the current locale setting and save as init_locale
    init_locale = get_string ("grep LANG= /etc/default/locale | cut -d = -f 2");
    if (init_locale == NULL) init_locale = g_strdup ("en_GB.UTF-8");

    // filter and sort the master database
    slang = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (locale_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (slang), 0, GTK_SORT_ASCENDING);

    flang = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (slang), NULL));
    gtk_tree_model_filter_set_visible_func (flang, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) 0, NULL);

    // set up the language combo box from the sorted and filtered language list
    gtk_combo_box_set_model (GTK_COMBO_BOX (loclang_cb), GTK_TREE_MODEL (flang));

    if (init_locale == NULL) gtk_combo_box_set_active (GTK_COMBO_BOX (loclang_cb), 0);
    else
    {
        str = g_strdup (init_locale);
        strtok (str, "_");
        set_init (GTK_TREE_MODEL (flang), loclang_cb, 0, str);
        g_free (str);
    }

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
        gtk_tree_model_get (model, &iter, 3, &str, -1);
        strcpy (gbuffer, str);
        g_free (str);

        if (gbuffer[0] && g_strcmp0 (gbuffer, init_locale))
        {
            // use sed to comment current setting if uncommented
            if (init_locale)
                vsystem ("sed -i 's/^\\(%s\\s\\)/# \\1/g' /etc/locale.gen", init_locale);

            // use sed to uncomment new setting if commented
            if (gbuffer)
                vsystem ("sed -i 's/^# \\(%s\\s\\)/\\1/g' /etc/locale.gen", gbuffer);

            // warn about a short delay...
            message (_("Setting locale - please wait..."));

            // launch a thread with the system call to update the generated locales
            g_thread_new (NULL, locale_thread, NULL);

            // set reboot flag
            needs_reboot = 1;
        }
    }

    g_free (init_locale);
    g_object_unref (locale_list);
    g_object_unref (flang);
    g_object_unref (slang);

    gtk_widget_destroy (dlg);
}

/* Timezone setting */

int tzfilter (const struct dirent *entry)
{
    if (entry->d_name[0] >= 'A' && entry->d_name[0] <= 'Z') return 1;
    return 0;
}

static gboolean match_area (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, 1, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
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
        gtk_tree_model_get (model, &iter, 1, &str, -1);

    // filter and sort the master database for entries matching this code
    far = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (timezone_list), NULL));
    gtk_tree_model_filter_set_visible_func (far, (GtkTreeModelFilterVisibleFunc) match_area, str, NULL);

    sar = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (far)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sar), 1, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (tzloc_cb), GTK_TREE_MODEL (sar));

    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), 0);
    else set_init (GTK_TREE_MODEL (sar), tzloc_cb, 0, ptr);

    // disable the combo box if it has no entries
    gtk_widget_set_sensitive (GTK_WIDGET (tzloc_cb), gtk_tree_model_iter_n_children (GTK_TREE_MODEL (sar), NULL) > 1);

    g_object_unref (far);
    g_object_unref (sar);
    g_free (str);

}

static gpointer timezone_thread (gpointer data)
{
    vsystem ("rm /etc/localtime");
    vsystem ("dpkg-reconfigure --frontend noninteractive tzdata");
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void read_timezones (void)
{
    char *buffer, *path, *area, *zone, *cptr;
    GtkTreeIter iter;
    FILE *fp;
    struct dirent **filelist, *dp, **sfilelist, *sdp, **ssfilelist, *ssdp;
    struct stat st_buf;
    int len, entries, entry, sentries, sentry, ssentries, ssentry;

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
                        gtk_list_store_set (timezone_list, &iter, 0, path, 1, area, 2, zone, -1);
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
                    gtk_list_store_set (timezone_list, &iter, 0, path, 1, dp->d_name, 2, zone, -1);
                    g_free (path);
                    g_free (zone);
                }
            }
        }
        else
        {
            gtk_list_store_append (timezone_list, &iter);
            gtk_list_store_set (timezone_list, &iter, 0, dp->d_name, 1, dp->d_name, 2, NULL, -1);
        }
    }
}

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
    timezone_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    read_timezones ();

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "tztable");
    tzarea_cb = (GObject *) gtk_combo_box_new ();
    tzloc_cb = (GObject *) gtk_combo_box_new ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzarea_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzloc_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (tzarea_cb));
    gtk_widget_show_all (GTK_WIDGET (tzloc_cb));

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tzarea_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tzarea_cb), col, "text", 1);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tzloc_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tzloc_cb), col, "text", 2);

    // read the current time zone
    init_tz = get_string ("cat /etc/timezone");
    if (init_tz == NULL) init_tz = g_strdup ("Europe/London");

    // filter and sort the master database
    stz = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (timezone_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (stz), 1, GTK_SORT_ASCENDING);

    ftz = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (stz), NULL));
    gtk_tree_model_filter_set_visible_func (ftz, (GtkTreeModelFilterVisibleFunc) unique_rows, (void *) 1, NULL);

    // set up the area combo box from the sorted and filtered timezone list
    gtk_combo_box_set_model (GTK_COMBO_BOX (tzarea_cb), GTK_TREE_MODEL (ftz));

    if (!init_tz) gtk_combo_box_set_active (GTK_COMBO_BOX (tzarea_cb), 0);
    else
    {
        buffer = g_strdup (init_tz);
        char *cptr = buffer;
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
        set_init (GTK_TREE_MODEL (ftz), tzarea_cb, 1, buffer);
        g_free (buffer);
    }

    // populate the location list and set the current location
    area_changed (GTK_COMBO_BOX (tzarea_cb), init_tz);
    g_signal_connect (tzarea_cb, "changed", G_CALLBACK (area_changed), NULL);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (tzloc_cb));
        if (gtk_tree_model_iter_n_children (model, NULL) > 1)
        {
            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tzloc_cb), &iter);
        }
        else
        {
            model = gtk_combo_box_get_model (GTK_COMBO_BOX (tzarea_cb));
            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tzarea_cb), &iter);
        }
        gtk_tree_model_get (model, &iter, 0, &buffer, -1);

        if (g_strcmp0 (init_tz, buffer))
        {
            vsystem ("echo '%s' | tee /etc/timezone", buffer);

            // warn about a short delay...
            message (_("Setting timezone - please wait..."));

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }
        g_free (buffer);
    }

    g_free (init_tz);
    g_object_unref (timezone_list);
    g_object_unref (ftz);
    g_object_unref (stz);

    gtk_widget_destroy (dlg);
}

/* Wifi country setting */

static void on_set_wifi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char *buffer, *cnow, *cptr;
    FILE *fp;
    int n, found, len;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "wcdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "wctable");
    wccountry_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wccountry_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (wccountry_cb));

    // get the current country setting
    cnow = get_string (GET_WIFI_CTRY);
    if (!cnow) cnow = g_strdup_printf ("00");

    // populate the combobox
    fp = fopen ("/usr/share/zoneinfo/iso3166.tab", "rb");
    found = 0;
    gtk_combo_box_append_text (GTK_COMBO_BOX (wccountry_cb), _("<not set>"));
    n = 1;
    buffer = NULL;
    len = 0;
    while (getline (&buffer, &len, fp) > 0)
    {
        if (buffer[0] != 0x0A && buffer[0] != '#')
        {
            buffer[strlen(buffer) - 1] = 0;
            gtk_combo_box_append_text (GTK_COMBO_BOX (wccountry_cb), buffer);
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
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wccountry_cb));
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

/* Resolution setting */

static void on_set_res (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char *buffer, *cptr, *entry, group;
    FILE *fp;
    int n, found, hmode, hgroup, mode, x, y, freq, ax, ay, conn, len;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "resdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "restable");
    resolution_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (resolution_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (resolution_cb));

    // get the current HDMI group and mode
    hgroup = get_status (GET_HDMI_GROUP);
    hmode = get_status (GET_HDMI_MODE);

    // is there a monitor connected?
    conn = 1;
    fp = popen ("tvservice -d /dev/null", "r");
    buffer = NULL;
    len = 0;
    while (getline (&buffer, &len, fp) > 0)
    {
        if (!strncmp (buffer, "Nothing", 7)) conn = 0;
    }
    pclose (fp);
    g_free (buffer);

    // populate the combobox
    if (conn)
    {
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "Default - preferred monitor settings");
        found = 0;
        n = 1;

        // get valid CEA modes
        fp = popen ("tvservice -m CEA", "r");
        buffer = NULL;
        len = 0;
        while (getline (&buffer, &len, fp) > 0)
        {
            if (buffer[0] != 0x0A && strstr (buffer, "progressive"))
            {
                sscanf (buffer + 11, "mode %d: %dx%d @ %dHz %d:%d,", &mode, &x, &y, &freq, &ax, &ay);
                if (x <= 1920 && y <= 1200)
                {
                    entry = g_strdup_printf ("CEA mode %d %dx%d %dHz %d:%d", mode, x, y, freq, ax, ay);
                    gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), entry);
                    g_free (entry);
                    if (hgroup == 1 && hmode == mode) found = n;
                    n++;
                }
            }
        }
        pclose (fp);
        g_free (buffer);

        // get valid DMT modes
        fp = popen ("tvservice -m DMT", "r");
        buffer = NULL;
        len = 0;
        while (getline (&buffer, &len, fp) > 0)
        {
            if (buffer[0] != 0x0A && strstr (buffer, "progressive"))
            {
                sscanf (buffer + 11, "mode %d: %dx%d @ %dHz %d:%d,", &mode, &x, &y, &freq, &ax, &ay);
                if (x <= 1920 && y <= 1200)
                {
                    entry = g_strdup_printf ("DMT mode %d %dx%d %dHz %d:%d", mode, x, y, freq, ax, ay);
                    gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), entry);
                    g_free (entry);
                    if (hgroup == 2 && hmode == mode) found = n;
                    n++;
                }
            }
        }
        pclose (fp);
        g_free (buffer);
    }
    else
    {
        // no connected monitor - offer default modes for VNC
        found = 0;
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "Default 720x480");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 4 640x480 60Hz 4:3");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 9 800x600 60Hz 4:3");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 16 1024x768 60Hz 4:3");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 85 1280x720 60Hz 16:9");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 35 1280x1024 60Hz 5:4");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 51 1600x1200 60Hz 4:3");
        gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "DMT mode 82 1920x1080 60Hz 16:9");
        if (hgroup == 2)
        {
            switch (hmode)
            {
                case 4 :    found = 1;
                            break;
                case 9 :    found = 2;
                            break;
                case 16 :   found = 3;
                            break;
                case 85 :   found = 4;
                            break;
                case 35 :   found = 5;
                            break;
                case 51 :   found = 6;
                            break;
                case 82 :   found = 7;
                            break;
            }
        }
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (resolution_cb), found);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        // set the HDMI variables
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (resolution_cb));
        if (!strncmp (cptr, "Default", 7))
        {
            // clear setting
            if (hmode != 0)
            {
                vsystem (SET_HDMI_GP_MOD, 0, 0);
                needs_reboot = 1;
            }
        }
        else
        {
            // set config vars
            sscanf (cptr, "%c%*s mode %d", &group, &mode);
            if (hgroup != group - 'B' || hmode != mode)
            {
                vsystem (SET_HDMI_GP_MOD, group - 'B', mode);
                needs_reboot = 1;
            }
        }
        g_free (cptr);
    }
    gtk_widget_destroy (dlg);
}

/* Keyboard setting */

static void layout_changed (GtkComboBox *cb, char *init_variant)
{
    FILE *fp;
    GtkTreeIter iter;
    char *buffer, *cptr, *t1, *t2;
    int siz, in_list;

    // get the currently-set layout from the combo box
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keylayout_cb), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 0, &t1, 1, &t2, -1);

    // reset the list of variants and add the layout name as a default
    gtk_list_store_clear (variant_list);
    gtk_list_store_append (variant_list, &iter);
    gtk_list_store_set (variant_list, &iter, 0, t1, 1, "", -1);
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
                    gtk_list_store_set (variant_list, &iter, 0, t1, 1, t2, -1);
                }
            }
        }
        if (!strncmp (buffer, cptr, strlen (buffer))) in_list = 1;
    }
    fclose (fp);
    g_free (cptr);
    g_free (buffer);

    set_init (GTK_TREE_MODEL (variant_list), keyvar_cb, 1, init_variant);
}

static gpointer keyboard_thread (gpointer ptr)
{
    //system ("dpkg-reconfigure -f noninteractive keyboard-configuration");
    vsystem ("invoke-rc.d keyboard-setup start");
    vsystem ("setsid sh -c 'exec setupcon -k --force <> /dev/tty1 >&0 2>&1'");
    vsystem ("udevadm trigger --subsystem-match=input --action=change");
    vsystem ("udevadm settle");
    vsystem (gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void read_keyboards (void)
{
    FILE *fp;
    char *cptr, *t1, *t2;
    int siz, in_list;
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
                    gtk_list_store_set (model_list, &iter, 0, t1, 1, t2, -1);
                }
                if (in_list == 2)
                {
                    gtk_list_store_append (layout_list, &iter);
                    gtk_list_store_set (layout_list, &iter, 0, t1, 1, t2, -1);
                }
            }
        }
        if (!strncmp ("%models", cptr, 7)) in_list = 1;
        if (!strncmp ("%layouts", cptr, 8)) in_list = 2;
    }
    fclose (fp);
    g_free (cptr);
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    FILE *fp;
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreeIter iter;
    char *buffer, *init_model = NULL, *init_layout = NULL, *init_variant = NULL, *new_mod, *new_lay, *new_var;
    int n;

    // set up list stores for keyboard layouts
    model_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    layout_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    variant_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    read_keyboards ();

    // build the dialog and attach the combo boxes
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "keyboarddlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "keytable");
    keymodel_cb = (GObject *) gtk_combo_box_new_with_model (GTK_TREE_MODEL (model_list));
    keylayout_cb = (GObject *) gtk_combo_box_new_with_model (GTK_TREE_MODEL (layout_list));
    keyvar_cb = (GObject *) gtk_combo_box_new_with_model (GTK_TREE_MODEL (variant_list));

    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keymodel_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keymodel_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keylayout_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keylayout_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyvar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyvar_cb), col, "text", 0);

    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (keymodel_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (keylayout_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (keyvar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (keymodel_cb));
    gtk_widget_show_all (GTK_WIDGET (keylayout_cb));
    gtk_widget_show_all (GTK_WIDGET (keyvar_cb));

    // get the current keyboard settings
    init_model = get_string ("grep XKBMODEL /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev");
    if (init_model == NULL) init_model = g_strdup ("pc105");

    init_layout = get_string ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev");
    if (init_layout == NULL) init_layout = g_strdup ("gb");

    init_variant = get_string ("grep XKBVARIANT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev");

    set_init (GTK_TREE_MODEL (model_list), keymodel_cb, 1, init_model);
    set_init (GTK_TREE_MODEL (layout_list), keylayout_cb, 1, init_layout);
    g_signal_connect (keylayout_cb, "changed", G_CALLBACK (layout_changed), NULL);
    layout_changed (GTK_COMBO_BOX (keyvar_cb), init_variant);

    g_object_unref (builder);

    // run the dialog
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        n = 0;
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keymodel_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (model_list), &iter, 1, &new_mod, -1);
        if (g_strcmp0 (new_mod, init_model))
        {
            vsystem ("grep -q XKBMODEL /etc/default/keyboard && sed -i 's/XKBMODEL=.*/XKBMODEL=%s/g' /etc/default/keyboard || echo 'XKBMODEL=%s' >> /etc/default/keyboard", new_mod, new_mod);
            n = 1;
        }

        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keylayout_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 1, &new_lay, -1);
        if (g_strcmp0 (new_lay, init_layout))
        {
            vsystem ("grep -q XKBLAYOUT /etc/default/keyboard && sed -i 's/XKBLAYOUT=.*/XKBLAYOUT=%s/g' /etc/default/keyboard || echo 'XKBLAYOUT=%s' >> /etc/default/keyboard", new_lay, new_lay);
            n = 1;
        }

        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyvar_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (variant_list), &iter, 1, &new_var, -1);
        if (g_strcmp0 (new_var, init_variant))
        {
            vsystem ("grep -q XKBVARIANT /etc/default/keyboard && sed -i 's/XKBVARIANT=.*/XKBVARIANT=%s/g' /etc/default/keyboard || echo 'XKBVARIANT=%s' >> /etc/default/keyboard", new_var, new_var);
            n = 1;
        }

        // this updates the current session when invoked after the udev update
        sprintf (gbuffer, "setxkbmap %s%s%s%s%s", new_lay, new_mod[0] ? " -model " : "", new_mod, new_var[0] ? " -variant " : "", new_var);
        g_free (new_mod);
        g_free (new_lay);
        g_free (new_var);

        if (n)
        {
            // warn about a short delay...
            message (_("Setting keyboard - please wait..."));

            // launch a thread with the system call to update the keyboard
            pthread = g_thread_new (NULL, keyboard_thread, NULL);
            if (ptr != NULL) gtk_dialog_run (GTK_DIALOG (msg_dlg));
        }
    }

    g_free (init_model);
    g_free (init_layout);
    g_free (init_variant);
    g_object_unref (model_list);
    g_object_unref (layout_list);
    g_object_unref (variant_list);

    gtk_widget_destroy (dlg);
}

/* Overlay file system setting */

static void on_overlay_fs (GtkButton* btn, gpointer ptr)
{
    ovfs_rb = 0;
    if (orig_ofs && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ofs_en_rb))) ovfs_rb = 1;
    if (!orig_ofs && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ofs_dis_rb))) ovfs_rb = 1;
    if (orig_bpro && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bp_ro_rb))) ovfs_rb = 1;
    if (!orig_bpro && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bp_rw_rb))) ovfs_rb = 1;

    if (ovfs_rb)
        gtk_label_set_text (GTK_LABEL (ofs_lbl), _("Changes will not take effect until a reboot"));
    else
        gtk_label_set_text (GTK_LABEL (ofs_lbl), "");
}

static gpointer initrd_thread (gpointer data)
{
    vsystem (SET_OFS_ON);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void on_set_ofs (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "ofsdlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    ofs_en_rb = gtk_builder_get_object (builder, "rb_ofsen");
    ofs_dis_rb = gtk_builder_get_object (builder, "rb_ofsdis");
    bp_ro_rb = gtk_builder_get_object (builder, "rb_bpro");
    bp_rw_rb = gtk_builder_get_object (builder, "rb_bprw");
    ofs_lbl = gtk_builder_get_object (builder, "ofslabel3");

    g_object_unref (builder);

    if (orig_ofs = vsystem (GET_OVERLAY)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ofs_dis_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ofs_en_rb), TRUE);

    if (orig_bpro = vsystem (GET_BOOTRO)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bp_rw_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bp_ro_rb), TRUE);

    if (vsystem (GET_OVERLAYNOW))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (bp_rw_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (bp_ro_rb), TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (bp_rw_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (bp_ro_rb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (bp_rw_rb), _("The state of the boot partition cannot be changed while an overlay is active"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (bp_ro_rb), _("The state of the boot partition cannot be changed while an overlay is active"));
    }

    g_signal_connect (ofs_en_rb, "toggled", G_CALLBACK (on_overlay_fs), NULL);
    g_signal_connect (bp_ro_rb, "toggled", G_CALLBACK (on_overlay_fs), NULL);
    gtk_label_set_text (GTK_LABEL (ofs_lbl), "");

    ovfs_rb = 0;
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (ovfs_rb) needs_reboot = 1;
        if (orig_bpro && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bp_ro_rb)))
            vsystem (SET_BOOTP_RO);
        if (!orig_bpro && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bp_rw_rb)))
            vsystem (SET_BOOTP_RW);
        if (!orig_ofs && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ofs_dis_rb)))
            vsystem (SET_OFS_OFF);
        if (orig_ofs && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ofs_en_rb)))
        {
            // warn about a short delay...
            message (_("Setting up overlay - please wait..."));

            // launch a thread with the system call to update the initrd
            g_thread_new (NULL, initrd_thread, NULL);
        }
    }
    gtk_widget_destroy (dlg);
}

/* Button handlers */

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
    vsystem (EXPAND_FS);
    needs_reboot = 1;

    GtkBuilder *builder;
    GtkWidget *dlg;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "fsdonedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    g_object_unref (builder);
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}

static void on_boot_cli (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_off_rb), FALSE);
    }
}

static void on_boot_gui (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (splash_on_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_off_rb), TRUE);
    }
}

static void on_serial_on (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (scons_on_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (scons_off_rb), TRUE);
    }
}

static void on_serial_off (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scons_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (scons_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (scons_off_rb), FALSE);
    }
}

static void on_pi4video (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (h4k_on_rb)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (analog_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (analog_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (analog_off_rb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (analog_on_rb), _("Composite video is not available when 4Kp60 HDMI is enabled"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (analog_off_rb), _("Composite video is not available when 4Kp60 HDMI is enabled"));
    }
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (analog_on_rb)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (h4k_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (h4k_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (h4k_off_rb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (h4k_on_rb), _("4Kp60 HDMI is not available when analogue video is enabled"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (h4k_off_rb), _("4Kp60 HDMI is not available when analogue video is enabled"));
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (analog_on_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (analog_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (h4k_on_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (h4k_off_rb), TRUE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (analog_on_rb), _("Enable analogue composite video on the 3.5mm socket"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (analog_off_rb), _("Disable analogue composite video"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (h4k_on_rb), _("Enable 4Kp60 output on HDMI0 (the port closest to the power socket)"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (h4k_off_rb), _("Disable 4Kp60 output"));
    }
}

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    int reboot = 0;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
        || orig_autolog == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) vsystem (SET_BOOT_GUIA);
            else vsystem (SET_BOOT_CLIA);
        }
        else
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) vsystem (SET_BOOT_GUI);
            else vsystem (SET_BOOT_CLI);
        }
    }
    
    if (orig_netwait == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (netwait_cb)))
    {
        vsystem (SET_BOOT_WAIT, (1 - orig_netwait));
    }

    if (orig_splash != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (splash_off_rb)))
    {
        vsystem (SET_SPLASH, (1 - orig_splash));
    }

    if (orig_pixdub != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pixdub_off_rb)))
    {
        vsystem (SET_PIXDUB, (1 - orig_pixdub));
        reboot = 1;
    }

    if (orig_ssh != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_off_rb)))
    {
        vsystem (SET_SSH, (1 - orig_ssh));
    }

    if (g_strcmp0 (orig_hostname, gtk_entry_get_text (GTK_ENTRY (hostname_tb))))
    {
        vsystem (SET_HOSTNAME, gtk_entry_get_text (GTK_ENTRY (hostname_tb)));
        reboot = 1;
    }

#ifdef __arm__
    if (orig_camera != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (camera_off_rb)))
    {
        vsystem (SET_CAMERA, (1 - orig_camera));
        reboot = 1;
    }

    if (orig_overscan != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (overscan_off_rb)))
    {
        vsystem (SET_OVERSCAN, (1 - orig_overscan));
        reboot = 1;
    }

    if (orig_blank != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (blank_off_rb)))
    {
        vsystem (SET_BLANK, (1 - orig_blank));
    }

    if (orig_vnc != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vnc_off_rb)))
    {
        vsystem (SET_VNC, (1 - orig_vnc));
    }

    if (orig_spi != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi_off_rb)))
    {
        vsystem (SET_SPI, (1 - orig_spi));
    }

    if (orig_i2c != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c_off_rb)))
    {
        vsystem (SET_I2C, (1 - orig_i2c));
    }

    if (orig_serial != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_off_rb)) ||
        orig_scons != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scons_off_rb)))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_off_rb)))
            vsystem (SET_SERIAL, 1);
        else
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scons_off_rb)))
                vsystem (SET_SERIAL, 2);
            else
                vsystem (SET_SERIAL, 0);
        }
        reboot = 1;
    }

    if (orig_onewire != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (onewire_off_rb)))
    {
        vsystem (SET_1WIRE, (1 - orig_onewire));
        reboot = 1;
    }

    if (orig_rgpio != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rgpio_off_rb)))
    {
        vsystem (SET_RGPIO, (1 - orig_rgpio));
    }

    if (orig_gpumem != gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (memsplit_sb)))
    {
        vsystem (SET_GPU_MEM, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (memsplit_sb)));
        reboot = 1;
    }

    if (orig_clock != gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
    {
        switch (get_status (GET_PI_TYPE))
        {
            case 1:
                switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
                {
                    case 0 :    vsystem (SET_OVERCLOCK, "None");
                                break;
                    case 1 :    vsystem (SET_OVERCLOCK, "Modest");
                                break;
                    case 2 :    vsystem (SET_OVERCLOCK, "Medium");
                                break;
                    case 3 :    vsystem (SET_OVERCLOCK, "High");
                                break;
                    case 4 :    vsystem (SET_OVERCLOCK, "Turbo");
                                break;
                }
                reboot = 1;
                break;

            case 2:
                switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
                {
                    case 0 :    vsystem (SET_OVERCLOCK, "None");
                                break;
                    case 1 :    vsystem (SET_OVERCLOCK, "High");
                                break;
                }
                reboot = 1;
                break;
        }
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (h4k_on_rb)))
    {
        if (orig_pi4v != 1)
        {
            vsystem (SET_PI4_4KH);
            reboot = 1;
        }
    }
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (analog_on_rb)))
    {
        if (orig_pi4v != 2)
        {
            vsystem (SET_PI4_ATV);
            reboot = 1;
        }
    }
    else
    {
        if (orig_pi4v != 0)
        {
            vsystem (SET_PI4_NONE);
            reboot = 1;
        }
    }
#endif

    return reboot;
}

/* Status checks */

static int can_configure (void)
{
    struct stat buf;
    FILE *fp;

    // check lightdm is installed
    if (stat ("/etc/init.d/lightdm", &buf)) return 0;

#ifdef __arm__
    // check startx.elf is present
    if (stat ("/boot/start_x.elf", &buf)) return 0;

    // check device tree is enabled
    if (!get_status ("cat /boot/config.txt | grep -q ^device_tree=$ ; echo $?")) return 0;

    // check /boot is mounted
    fp = popen ("mountpoint /boot", "r");
    if (pclose (fp) != 0) return 0;

    // create /boot/config.txt if it doesn't exist
    vsystem ("[ -e /boot/config.txt ] || touch /boot/config.txt");
#endif

    return 1;
}

static int has_wifi (void)
{
    char *res;
    int ret = 0;

    res = get_string (WLAN_INTERFACES);
    if (res && strlen (res) > 0) ret = 1;
    g_free (res);
    return ret;
}


/* The dialog... */

#define SHOW_WIDGET(name) item = gtk_builder_get_object (builder, name); gtk_widget_show (GTK_WIDGET (item));
#define HIDE_WIDGET(name) item = gtk_builder_get_object (builder, name); gtk_widget_hide (GTK_WIDGET (item));

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GObject *item;
    GtkWidget *dlg;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    // GTK setup
    gdk_threads_init ();
    gdk_threads_enter ();
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    if (argc == 2 && !g_strcmp0 (argv[1], "-w"))
    {
        on_set_wifi (NULL, NULL);
        return 0;
    }

    if (argc == 2 && !g_strcmp0 (argv[1], "-k"))
    {
        // set up list stores for keyboard layouts
        model_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        layout_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        variant_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

        pthread = 0;
        on_set_keyboard (NULL, (gpointer) 1);
        if (pthread) g_thread_join (pthread);
        return 0;
    }

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);

    if (!can_configure ())
    {
        dlg = (GtkWidget *) gtk_builder_get_object (builder, "errordialog");
        g_object_unref (builder);
        gtk_dialog_run (GTK_DIALOG (dlg));
        gtk_widget_destroy (dlg);
        return 0;
    }

    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");

    passwd_btn = gtk_builder_get_object (builder, "button_pw");
    g_signal_connect (passwd_btn, "clicked", G_CALLBACK (on_change_passwd), NULL);

    locale_btn = gtk_builder_get_object (builder, "button_loc");
    g_signal_connect (locale_btn, "clicked", G_CALLBACK (on_set_locale), NULL);

    timezone_btn = gtk_builder_get_object (builder, "button_tz");
    g_signal_connect (timezone_btn, "clicked", G_CALLBACK (on_set_timezone), NULL);

    keyboard_btn = gtk_builder_get_object (builder, "button_kb");
    g_signal_connect (keyboard_btn, "clicked", G_CALLBACK (on_set_keyboard), NULL);

    wifi_btn = gtk_builder_get_object (builder, "button_wifi");
    g_signal_connect (wifi_btn, "clicked", G_CALLBACK (on_set_wifi), NULL);
    if (has_wifi ()) gtk_widget_set_sensitive (GTK_WIDGET (wifi_btn), TRUE);
    else gtk_widget_set_sensitive (GTK_WIDGET (wifi_btn), FALSE);

    splash_on_rb = gtk_builder_get_object (builder, "rb_splash_on");
    splash_off_rb = gtk_builder_get_object (builder, "rb_splash_off");
    if (orig_splash = get_status (GET_SPLASH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_on_rb), TRUE);

    boot_desktop_rb = gtk_builder_get_object (builder, "rb_desktop");
    g_signal_connect (boot_desktop_rb, "toggled", G_CALLBACK (on_boot_gui), NULL);
    boot_cli_rb = gtk_builder_get_object (builder, "rb_cli");
    g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_boot_cli), NULL);
    if (orig_boot = get_status (GET_BOOT_CLI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);

    autologin_cb = gtk_builder_get_object (builder, "cb_login");
    if (orig_autolog = get_status (GET_AUTOLOGIN)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), FALSE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), TRUE);

    netwait_cb = gtk_builder_get_object (builder, "cb_network");
    if (orig_netwait = get_status (GET_BOOT_WAIT)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), FALSE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), TRUE);

    ssh_on_rb = gtk_builder_get_object (builder, "rb_ssh_on");
    ssh_off_rb = gtk_builder_get_object (builder, "rb_ssh_off");
    if (orig_ssh = get_status (GET_SSH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_on_rb), TRUE);

    hostname_tb = gtk_builder_get_object (builder, "entry_hn");
    orig_hostname = get_string (GET_HOSTNAME);
    gtk_entry_set_text (GTK_ENTRY (hostname_tb), orig_hostname);

    pixdub_on_rb = gtk_builder_get_object (builder, "rb_pd_on");
    pixdub_off_rb = gtk_builder_get_object (builder, "rb_pd_off");
    if (orig_pixdub = get_status (GET_PIXDUB)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pixdub_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pixdub_on_rb), TRUE);

#ifdef __arm__
    res_btn = gtk_builder_get_object (builder, "button_res");
    g_signal_connect (res_btn, "clicked", G_CALLBACK (on_set_res), NULL);

    camera_on_rb = gtk_builder_get_object (builder, "rb_cam_on");
    camera_off_rb = gtk_builder_get_object (builder, "rb_cam_off");
    if (orig_camera = get_status (GET_CAMERA)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_on_rb), TRUE);

    overscan_on_rb = gtk_builder_get_object (builder, "rb_os_on");
    overscan_off_rb = gtk_builder_get_object (builder, "rb_os_off");
    if (orig_overscan = get_status (GET_OVERSCAN)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_on_rb), TRUE);

    spi_on_rb = gtk_builder_get_object (builder, "rb_spi_on");
    spi_off_rb = gtk_builder_get_object (builder, "rb_spi_off");
    if (orig_spi = get_status (GET_SPI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_on_rb), TRUE);

    i2c_on_rb = gtk_builder_get_object (builder, "rb_i2c_on");
    i2c_off_rb = gtk_builder_get_object (builder, "rb_i2c_off");
    if (orig_i2c = get_status (GET_I2C)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_on_rb), TRUE);

    serial_on_rb = gtk_builder_get_object (builder, "rb_ser_on");
    g_signal_connect (serial_on_rb, "toggled", G_CALLBACK (on_serial_on), NULL);
    serial_off_rb = gtk_builder_get_object (builder, "rb_ser_off");
    g_signal_connect (serial_off_rb, "toggled", G_CALLBACK (on_serial_off), NULL);
    scons_on_rb = gtk_builder_get_object (builder, "rb_serc_on");
    scons_off_rb = gtk_builder_get_object (builder, "rb_serc_off");
    if (orig_serial = get_status (GET_SERIALHW)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);
    if (orig_scons = get_status (GET_SERIAL)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scons_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scons_on_rb), TRUE);

    onewire_on_rb = gtk_builder_get_object (builder, "rb_one_on");
    onewire_off_rb = gtk_builder_get_object (builder, "rb_one_off");
    if (orig_onewire = get_status (GET_1WIRE)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_on_rb), TRUE);

    rgpio_on_rb = gtk_builder_get_object (builder, "rb_rgp_on");
    rgpio_off_rb = gtk_builder_get_object (builder, "rb_rgp_off");
    if (orig_rgpio = get_status (GET_RGPIO)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgpio_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgpio_on_rb), TRUE);

    vnc_on_rb = gtk_builder_get_object (builder, "rb_vnc_on");
    vnc_off_rb = gtk_builder_get_object (builder, "rb_vnc_off");
    if (orig_vnc = get_status (GET_VNC)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vnc_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vnc_on_rb), TRUE);

    blank_on_rb = gtk_builder_get_object (builder, "rb_blank_on");
    blank_off_rb = gtk_builder_get_object (builder, "rb_blank_off");
    if (orig_blank = get_status (GET_BLANK)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (blank_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (blank_on_rb), TRUE);

    // disable the buttons if RealVNC isn't installed
    gboolean enable = TRUE;
    struct stat buf;
    if (stat ("/usr/share/doc/realvnc-vnc-server", &buf)) enable = FALSE;
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_on_rb), enable);
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_off_rb), enable);

    h4k_on_rb = gtk_builder_get_object (builder, "rb_4k_on");
    h4k_off_rb = gtk_builder_get_object (builder, "rb_4k_off");
    analog_on_rb = gtk_builder_get_object (builder, "rb_analog_on");
    analog_off_rb = gtk_builder_get_object (builder, "rb_analog_off");
    gtk_widget_set_sensitive (GTK_WIDGET (analog_on_rb), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (analog_off_rb), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (h4k_on_rb), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (h4k_off_rb), TRUE);
    g_signal_connect (h4k_on_rb, "toggled", G_CALLBACK (on_pi4video), NULL);
    g_signal_connect (analog_on_rb, "toggled", G_CALLBACK (on_pi4video), NULL);
    orig_pi4v = get_status (GET_PI4_VID);
    switch (orig_pi4v)
    {
        case 1 :    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (h4k_on_rb), TRUE);
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (analog_off_rb), TRUE);
                    //gtk_widget_set_sensitive (GTK_WIDGET (analog_on_rb), FALSE);
                    //gtk_widget_set_sensitive (GTK_WIDGET (analog_off_rb), FALSE);
                    break;
        case 2 :    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (analog_on_rb), TRUE);
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (h4k_off_rb), TRUE);
                    //gtk_widget_set_sensitive (GTK_WIDGET (h4k_on_rb), FALSE);
                    //gtk_widget_set_sensitive (GTK_WIDGET (h4k_off_rb), FALSE);
                    break;
        default :   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (h4k_off_rb), TRUE);
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (analog_off_rb), TRUE);
                    break;
    }

    switch (get_status (GET_PI_TYPE))
    {
        case 1:
            overclock_cb = gtk_builder_get_object (builder, "combo_oc_pi1");
            switch (get_status (GET_OVERCLOCK))
            {
                case 800  : orig_clock = 1;
                            break;
                case 900  : orig_clock = 2;
                            break;
                case 950  : orig_clock = 3;
                            break;
                case 1000 : orig_clock = 4;
                            break;
                default   : orig_clock = 0;
                            break;
            }
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), orig_clock);
            gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox31a")));
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox31b")));
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox31c")));
            break;

        case 2 :
            overclock_cb = gtk_builder_get_object (builder, "combo_oc_pi2");
            switch (get_status (GET_OVERCLOCK))
            {
                case 1000 : orig_clock = 1;
                            break;
                default   : orig_clock = 0;
                            break;
            }
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), orig_clock);
            gtk_widget_hide_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31a")));
            gtk_widget_show_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31b")));
            gtk_widget_hide_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31c")));
            break;

        default :
            overclock_cb = gtk_builder_get_object (builder, "combo_oc_pi3");
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 0);
            gtk_widget_hide_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31a")));
            gtk_widget_hide_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31b")));
            gtk_widget_show_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31c")));
            gtk_widget_set_sensitive (GTK_WIDGET (overclock_cb), FALSE);
            break;
    }

    ofs_btn = gtk_builder_get_object (builder, "button_ofs");
    g_signal_connect (ofs_btn, "clicked", G_CALLBACK (on_set_ofs), NULL);

    /*  Video options for various platforms
     *
     *                              FKMS,Pi4    FKMS,Pi3    Leg,Pi4     Leg,Pi3
     * hbox51 - set resolution          -           -           Y           Y
     * hbox52 - overscan                Y           Y           Y           Y
     * hbox53 - pixel doubling          -           -           Y           Y
     * hbox54 - 4kp60                   -           -           -           -
     * hbox55 - composite out           Y           -           Y           -
     * hbox56 - blanking                Y           Y           Y           Y
     * hbox5a - filler                  -           Y           -           Y
     * hbox5b - filler                  Y           Y           Y           Y
     * hbox5c - filler                  Y           Y           -           -
     * hbox5d - filler                  Y           Y           -           -
     */

    SHOW_WIDGET ("hbox52");
    HIDE_WIDGET ("hbox54");
    SHOW_WIDGET ("hbox56");
    if (vsystem (IS_PI4))
    {
        HIDE_WIDGET ("hbox55");
        SHOW_WIDGET ("hbox5a");
    }
    else 
    {
        SHOW_WIDGET ("hbox55");
        HIDE_WIDGET ("hbox5a");
    }

    if (vsystem (GET_FKMS))
    {
        SHOW_WIDGET ("hbox51");
        SHOW_WIDGET ("hbox53");
        SHOW_WIDGET ("hbox5b");
        HIDE_WIDGET ("hbox5c");
        HIDE_WIDGET ("hbox5d");
    }
    else
    {
        HIDE_WIDGET ("hbox51");
        HIDE_WIDGET ("hbox53");
        SHOW_WIDGET ("hbox5b");
        SHOW_WIDGET ("hbox5c");
        SHOW_WIDGET ("hbox5d");
    }

    GtkObject *adj = gtk_adjustment_new (64.0, 16.0, get_total_mem () - 128, 8.0, 64.0, 0);
    memsplit_sb = gtk_builder_get_object (builder, "spin_gpu");
    gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (memsplit_sb), GTK_ADJUSTMENT (adj));
    orig_gpumem = get_gpu_mem ();
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (memsplit_sb), orig_gpumem);
#else
    if (!get_status ("grep -q boot=live /proc/cmdline ; echo $?"))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (splash_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_off_rb), FALSE);
    }

    HIDE_WIDGET ("vbox50");

    HIDE_WIDGET ("hbox21");
    HIDE_WIDGET ("hbox23");
    HIDE_WIDGET ("hbox24");
    HIDE_WIDGET ("hbox25");
    HIDE_WIDGET ("hbox26");
    HIDE_WIDGET ("hbox27");
    HIDE_WIDGET ("hbox28");
    HIDE_WIDGET ("hbox29");
    SHOW_WIDGET ("hbox2a");
    SHOW_WIDGET ("hbox2b");
    SHOW_WIDGET ("hbox2c");
    SHOW_WIDGET ("hbox2d");
    SHOW_WIDGET ("hbox2e");
    SHOW_WIDGET ("hbox2f");
    SHOW_WIDGET ("hbox2g");
    SHOW_WIDGET ("hbox2h");

    HIDE_WIDGET ("vbox30");
#endif

    GdkPixbuf *win_icon = gtk_window_get_icon (GTK_WINDOW (main_dlg));
    GList *list;
    list = g_list_append (list, win_icon);
    gtk_window_set_default_icon_list (list);

    needs_reboot = 0;
    if (gtk_dialog_run (GTK_DIALOG (main_dlg)) == GTK_RESPONSE_OK)
    {
        if (process_changes ()) needs_reboot = 1;
    }
    if (needs_reboot)
    {
        dlg = (GtkWidget *) gtk_builder_get_object (builder, "rebootdlg");
        gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
        if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES)
        {
            vsystem ("reboot");
        }
        gtk_widget_destroy (dlg);
    }

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);
    gdk_threads_leave ();

    return 0;
}
