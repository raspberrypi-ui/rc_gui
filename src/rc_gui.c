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
#define GET_PI_TYPE     "raspi-config nonint get_pi_type"
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
#define CHANGE_PASSWD   "(echo \"%s\" ; echo \"%s\") | passwd $SUDO_USER"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *res_btn, *locale_btn, *timezone_btn, *keyboard_btn, *wifi_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb, *pixdub_on_rb, *pixdub_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb, *rgpio_on_rb, *rgpio_off_rb, *vnc_on_rb, *vnc_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb, *onewire_on_rb, *onewire_off_rb;
static GObject *autologin_cb, *netwait_cb, *splash_on_rb, *splash_off_rb, *scons_on_rb, *scons_off_rb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwentry3_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb, *resolution_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb, *keymodel_cb, *keylayout_cb, *keyvar_cb;
static GObject *language_ls, *country_ls, *loc_table;

static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static char *orig_hostname;
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_spi, orig_i2c, orig_serial, orig_scons, orig_splash;
static int orig_clock, orig_gpumem, orig_autolog, orig_netwait, orig_onewire, orig_rgpio, orig_vnc, orig_pixdub;

/* Reboot flag set after locale change */

static int needs_reboot;

/* Number of items in comboboxes */

static int loc_count;

/* Globals accessed from multiple threads */

static char gbuffer[512];
GThread *pthread;

/* Lists for keyboard setting */

GtkListStore *model_list, *layout_list, *variant_list;

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
    int res = 0;

    if (fp == NULL) return 0;
    if (getline (&buf, &res, fp) > 0)
    {
        sscanf (buf, "%d", &res);
        pclose (fp);
        return res;
    }
    pclose (fp);
    return 0;
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
        while (*res++) if (g_ascii_isspace (*res)) *res = 0;
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

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;

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
        res = vsystem (CHANGE_PASSWD, gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), gtk_entry_get_text (GTK_ENTRY (pwentry3_tb)));
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

static void country_changed (GtkComboBox *cb, char *ptr)
{
    char *buffer, *cb_lang = NULL, *cb_ctry = NULL, *cb_ext, *init_char = NULL, *cptr;
    int len, char_count;
    FILE *fp;

    // clear the combo box
    gtk_widget_destroy (GTK_WIDGET (locchar_cb));
    locchar_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (loc_table), GTK_WIDGET (locchar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (locchar_cb));
    char_count = 0;

    // if an initial setting is supplied at ptr...
    if (ptr)
    {
        // find the line in SUPPORTED that exactly matches the supplied country string
        fp = vpopen ("grep '%s ' /usr/share/i18n/SUPPORTED", ptr);
        if (fp == NULL) return;
        buffer = NULL;
        len = 0;
        while (getline (&buffer, &len, fp) > 0)
        {
            // copy the current character code into cur_char
            strtok (buffer, " ");
            cptr = strtok (NULL, " \n\r");
            init_char = g_strdup (cptr);
        }
        pclose (fp);
        g_free (buffer);
    }

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb));
    if (cptr)
    {
        cb_lang = g_strdup (cptr);
        strtok (cb_lang, " ");
        g_free (cptr);
    }

    // read the country from the combo box and split off code and extension
    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loccount_cb));
    if (cptr)
    {
        cb_ctry = g_strdup (cptr);
        strtok (cb_ctry, "@ ");
        cb_ext = strtok (NULL, "@ ");
        if (*cb_ext == '(') cb_ext = NULL;
        g_free (cptr);
    }

    // build the grep expression to search the file of supported formats
    if (cb_ctry == NULL)
        fp = vpopen ("grep %s /usr/share/i18n/SUPPORTED", cb_lang);
    else if (cb_ext == NULL)
        fp = vpopen ("grep %s_%s /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry);
    else
        fp = vpopen ("grep -E '%s_%s.*%s' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext);

    // run the grep and parse the returned lines
    if (fp == NULL) return;
    buffer = NULL;
    len = 0;
    while (getline (&buffer, &len, fp) > 0)
    {
        // find the second part of the returned line (separated by a space) and add to combo box
        strtok (buffer, " ");
        cptr = strtok (NULL, " \n\r");
        gtk_combo_box_append_text (GTK_COMBO_BOX (locchar_cb), cptr);

        // check to see if it matches the initial string and set active if so
        if (!g_strcmp0 (cptr, init_char)) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), char_count);
        char_count++;
    }
    pclose (fp);
    g_free (buffer);

    g_free (cb_lang);
    g_free (cb_ctry);
    g_free (init_char);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);

    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);
}

static void language_changed (GtkComboBox *cb, char *ptr)
{
    struct dirent **filelist, *dp;
    char *buffer, *result, *cptr, *cb_lang = NULL, *init_ctry = NULL, *init = NULL, *file = NULL, *file_lang = NULL, *file_ctry = NULL;
    int entries, entry, len, country_count;
    FILE *fp;

    // clear the combo box
    gtk_widget_destroy (GTK_WIDGET (loccount_cb));
    loccount_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (loc_table), GTK_WIDGET (loccount_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (loccount_cb));
    country_count = 0;

    // if an initial setting is supplied at ptr, extract the country code from the supplied string
    if (ptr)
    {
        init = g_strdup (ptr);
        strtok (init, "_");
        init_ctry = strtok (NULL, ". \n\t\r");
    }

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb));
    if (cptr)
    {
        cb_lang = g_strdup (cptr);
        strtok (cb_lang, " ");
        g_free (cptr);
    }

    // loop through locale files
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language and country codes from the locale file name
        file = g_strdup (dp->d_name);
        file_lang = strtok (file, "_");
        file_ctry = strtok (NULL, ". \n\t\r");

        // if the country code from the filename is valid,
        // and the language code from the filename matches the one in the combo box...
        if (file_ctry && !g_strcmp0 (cb_lang, file_lang))
        {
            // check that the file is in the SUPPORTED list
            fp = vpopen ("grep %s /usr/share/i18n/SUPPORTED", dp->d_name);
            if (fp == NULL) continue;
            buffer = NULL;
            len = 0;
            if (getline (&buffer, &len, fp) <= 0) continue;

            // read the territory description from the file
            result = get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "territory");

            // add country code and description to combo box
            buffer = g_strdup_printf ("%s (%s)", file_ctry, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loccount_cb), buffer);
            g_free (result);
            g_free (buffer);

            // check to see if it matches the initial string and set active if so
            if (!g_strcmp0 (file_ctry, init_ctry)) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), country_count);
            country_count++;
        }
        g_free (dp);
        g_free (file);
    }
    g_free (filelist);

    g_free (cb_lang);
    g_free (init);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);

    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);
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
    GtkBuilder *builder;
    GtkWidget *dlg;
    struct dirent **filelist, *dp;
    char *buffer, *result, *cptr, *init_locale = NULL, *file_lang = NULL, *last_lang = NULL, *init_lang = NULL;
    int count, entries, entry, len;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    loc_table = (GObject *) gtk_builder_get_object (builder, "loctable");
    loclang_cb = (GObject *) gtk_combo_box_new_text ();
    loccount_cb = (GObject *) gtk_combo_box_new_text ();
    locchar_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (loc_table), GTK_WIDGET (loclang_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (loc_table), GTK_WIDGET (loccount_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (loc_table), GTK_WIDGET (locchar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (loclang_cb));
    gtk_widget_show_all (GTK_WIDGET (loccount_cb));
    gtk_widget_show_all (GTK_WIDGET (locchar_cb));

    // get the current locale setting and save as init_locale
    FILE *fp = popen ("grep LANG /etc/default/locale", "r");
    if (fp == NULL) return;
    buffer = NULL;
    len = 0;
    while (getline (&buffer, &len, fp) > 0)
    {
        strtok (buffer, "=");
        cptr = strtok (NULL, "\n\r");
    }
    pclose (fp);
    init_locale = g_strdup (cptr);
    g_free (buffer);

    // parse the initial locale to get the initial language code
    init_lang = g_strdup (init_locale);
    strtok (init_lang, "_");

    // loop through locale files
    count = 0;
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language code from the locale file name
        file_lang = g_strdup (dp->d_name);
        strtok (file_lang, "_.");

        // if it differs from the last one read, create a new entry
        if (file_lang && g_strcmp0 (file_lang, last_lang))
        {
            // check to see if there is a file whose name has the format aa_AA; if not just use the first file we find
            buffer = g_strdup_printf ("%s_%c%c%c", file_lang, toupper (file_lang[0]), toupper (file_lang[1]), toupper (file_lang[2]));

            // ...and read the language description from the file
            result = get_quoted_param ("/usr/share/i18n/locales", buffer, "language");
            if (!result) result = get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "language");
            g_free (buffer);

            // add language code and description to combo box
            buffer = g_strdup_printf ("%s (%s)", file_lang, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loclang_cb), buffer);
            g_free (result);
            g_free (buffer);

            // make a local copy of the language code for comparisons
            g_free (last_lang);
            last_lang = g_strdup (file_lang);

            // highlight the current language setting...
            if (!g_strcmp0 (file_lang, init_lang)) gtk_combo_box_set_active (GTK_COMBO_BOX (loclang_cb), count);
            count++;
        }
        g_free (dp);

        g_free (file_lang);
    }
    g_free (filelist);
    g_free (last_lang);

    // populate the country and character lists and set the current values
    language_changed (GTK_COMBO_BOX (loclang_cb), init_locale);

    g_signal_connect (loclang_cb, "changed", G_CALLBACK (language_changed), NULL);
    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        char cb_lang[64], cb_ctry[64], *cb_ext;
        // read the language from the combo box and split off the code into lang
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb));
        if (cptr)
        {
            strcpy (cb_lang, cptr);
            strtok (cb_lang, " ");
            g_free (cptr);
        }
        else cb_lang[0] = 0;

        // read the country from the combo box and split off code and extension
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loccount_cb));
        if (cptr)
        {
            strcpy (cb_ctry, cptr);
            strtok (cb_ctry, "@ ");
            cb_ext = strtok (NULL, "@ ");
            if (cb_ext[0] == '(') cb_ext[0] = 0;
            g_free (cptr);
        }
        else cb_ctry[0] = 0;

        // build the relevant grep expression to search the file of supported formats
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (locchar_cb));
        if (cptr)
        {
            if (!cb_ctry[0])
                fp = vpopen ("grep %s.*%s$ /usr/share/i18n/SUPPORTED", cb_lang, cptr);
            else if (!cb_ext[0])
                fp = vpopen ("grep %s_%s.*%s$ /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry, cptr);
            else
                fp = vpopen ("grep -E '%s_%s.*%s.*%s$' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext, cptr);
            g_free (cptr);

            // run the grep and parse the returned line
            if (fp != NULL)
            {
                buffer = NULL;
                len = 0;
                getline (&buffer, &len, fp);
                cptr = strtok (buffer, " ");
                strcpy (gbuffer, cptr);
                pclose (fp);
                g_free (buffer);
            }

            if (gbuffer[0] && g_strcmp0 (gbuffer, init_locale))
            {
                // look up the current locale setting from init_locale in /etc/locale.gen
                fp = vpopen ("grep '%s ' /usr/share/i18n/SUPPORTED", init_locale);
                if (fp != NULL)
                {
                    buffer = NULL;
                    len = 0;
                    getline (&buffer, &len, fp);
                    strtok (buffer, "\n\r");
                    pclose (fp);
                }

                // use sed to comment that line if uncommented
                if (buffer[0])
                    vsystem ("sed -i 's/^%s/# %s/g' /etc/locale.gen", buffer, buffer);
                g_free (buffer);

                // look up the new locale setting from gbuffer in /etc/locale.gen
                fp = vpopen ("grep '%s ' /usr/share/i18n/SUPPORTED", gbuffer);
                if (fp != NULL)
                {
                    buffer = NULL;
                    len = 0;
                    getline (&buffer, &len, fp);
                    strtok (buffer, "\n\r");
                    pclose (fp);
                }

                // use sed to uncomment that line if commented
                if (buffer[0])
                    vsystem ("sed -i 's/^# %s/%s/g' /etc/locale.gen", buffer, buffer);
                g_free (buffer);

                // warn about a short delay...
                message (_("Setting locale - please wait..."));

                // launch a thread with the system call to update the generated locales
                g_thread_new (NULL, locale_thread, NULL);

                // set reboot flag
                needs_reboot = 1;
            }
        }
    }

    g_free (init_locale);
    g_free (init_lang);

    gtk_widget_destroy (dlg);
}

/* Timezone setting */

int dirfilter (const struct dirent *entry)
{
    if (entry->d_name[0] != '.') return 1;
    return 0;
}

static void area_changed (GtkComboBox *cb, gpointer ptr)
{
    char *buffer, *cptr;
    struct dirent **filelist, *dp, **sfilelist, *sdp;
    struct stat st_buf;
    int entries, entry, sentries, sentry;

    while (loc_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (tzloc_cb), 0);
    loc_count = 0;

    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb));
    buffer = g_strdup_printf ("/usr/share/zoneinfo/%s", cptr);
    stat (buffer, &st_buf);

    if (S_ISDIR (st_buf.st_mode))
    {
        entries = scandir (buffer, &filelist, dirfilter, alphasort);
        g_free (buffer);
        for (entry = 0; entry < entries; entry++)
        {
            dp = filelist[entry];
            if (dp->d_type == DT_DIR)
            {
                buffer = g_strdup_printf ("/usr/share/zoneinfo/%s/%s", cptr, dp->d_name);
                sentries = scandir (buffer, &sfilelist, dirfilter, alphasort);
                g_free (buffer);
                for (sentry = 0; sentry < sentries; sentry++)
                {
                    sdp = sfilelist[sentry];
                    buffer = g_strdup_printf ("%s/%s", dp->d_name, sdp->d_name);
                    gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), buffer);
                    if (ptr && !g_strcmp0 (ptr, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                    loc_count++;
                    g_free (buffer);
                    g_free (sdp);
                }
                g_free (sfilelist);
            }
            else
            {
                gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), dp->d_name);
                if (ptr && !g_strcmp0 (ptr, dp->d_name)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                loc_count++;
            }
            g_free (dp);
        }
        g_free (filelist);
        if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), 0);
    }
    g_free (cptr);
}

static gpointer timezone_thread (gpointer data)
{
    system ("rm /etc/localtime");
    system ("dpkg-reconfigure --frontend noninteractive tzdata");
    g_idle_add (close_msg, NULL);
    return NULL;
}

int tzfilter (const struct dirent *entry)
{
    if (entry->d_name[0] >= 'A' && entry->d_name[0] <= 'Z') return 1;
    return 0;
}

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char *buffer, *before, *cptr, *b1ptr, *b2ptr;
    struct dirent **filelist, *dp;
    int entries, entry;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "tztable");
    tzarea_cb = (GObject *) gtk_combo_box_new_text ();
    tzloc_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzarea_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzloc_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (tzarea_cb));
    gtk_widget_show_all (GTK_WIDGET (tzloc_cb));

    // select the current timezone area
    buffer = get_string ("cat /etc/timezone | tr -d \" \t\n\r\"");
    before = g_strdup (buffer);
    strtok (buffer, "/");
    cptr = strtok (NULL, "");

    // populate the area combo box from the timezone database
    loc_count = 0;
    int count = 0;
    entries = scandir ("/usr/share/zoneinfo", &filelist, tzfilter, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        gtk_combo_box_append_text (GTK_COMBO_BOX (tzarea_cb), dp->d_name);
        if (!g_strcmp0 (dp->d_name, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzarea_cb), count);
        count++;
        g_free (dp);
    }
    g_free (filelist);
    g_signal_connect (tzarea_cb, "changed", G_CALLBACK (area_changed), NULL);

    // populate the location list and set the current location
    area_changed (GTK_COMBO_BOX (tzarea_cb), cptr);

    g_object_unref (builder);
    g_free (buffer);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        b1ptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb));
        b2ptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb));
        if (b2ptr)
            buffer = g_strdup_printf ("%s/%s", b1ptr, b2ptr);
        else
            buffer = g_strdup_printf ("%s", b1ptr);

        if (g_strcmp0 (before, buffer))
        {
            if (b2ptr)
                vsystem ("echo '%s/%s' | tee /etc/timezone", b1ptr, b2ptr);
            else
                vsystem ("echo '%s' | tee /etc/timezone", b1ptr);

            // warn about a short delay...
            message (_("Setting timezone - please wait..."));

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }
        g_free (b1ptr);
        if (b2ptr) g_free (b2ptr);
        g_free (buffer);
    }
    g_free (before);
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
    fclose (fp);
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
        fclose (fp);
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
        fclose (fp);
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
    GtkTreePath *path;
    GtkTreeIter iter;
    char *buffer, *cptr, *t1, *t2;
    int siz, n, varn, count, in_list;
    FILE *fp;

    // get the currently-set layout from the combo box
    path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keylayout_cb)), -1);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (layout_list), &iter, path);
    gtk_tree_path_free (path);
    gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 0, &t1, 1, &t2, -1);

    // reset the list of variants and add the layout name as a default
    gtk_list_store_clear (variant_list);
    gtk_list_store_append (variant_list, &iter);
    gtk_list_store_set (variant_list, &iter, 0, t1, 1, "", -1);
    buffer = g_strdup_printf ("    '%s'", t2);
    g_free (t1);
    g_free (t2);
    count = 1;

    // parse the database file to find variants for this layout
    cptr = NULL;
    in_list = 0;
    varn = 0;
    fp = fopen ("/usr/share/console-setup/KeyboardNames.pl", "rb");
    while ((n = getline (&cptr, &siz, fp)) != -1)
    {
        if (n)
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
                        if (init_variant && !g_strcmp0 (t2, init_variant)) varn = count;
                    }
                    count++;
                }
            }
            if (!strncmp (buffer, cptr, strlen (buffer))) in_list = 1;
        }
    }
    fclose (fp);
    g_free (cptr);
    g_free (buffer);

    gtk_combo_box_set_active (GTK_COMBO_BOX (keyvar_cb), varn);
}

static gpointer keyboard_thread (gpointer ptr)
{
    //system ("dpkg-reconfigure -f noninteractive keyboard-configuration");
    system ("invoke-rc.d keyboard-setup start");
    system ("setsid sh -c 'exec setupcon -k --force <> /dev/tty1 >&0 2>&1'");
    system ("udevadm trigger --subsystem-match=input --action=change");
    system ("udevadm settle");
    system (gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreePath *path;
    GtkTreeIter iter;
    char *init_model, *init_layout, *init_variant, *cptr, *t1, *t2, *new_mod, *new_lay, *new_var;
    FILE *fp;
    int siz, n, modeln, layoutn, in_list, count;

    // get the current keyboard settings
    init_model = NULL;
    siz = 0;
    if (fp = popen ("grep XKBMODEL /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        getline (&init_model, &siz, fp);
        pclose (fp);
    }
    init_model[strcspn (init_model, "\r\n")] = 0;
    if (!strlen (init_model)) init_model = g_strdup_printf ("pc105");

    init_layout = NULL;
    siz = 0;
    if (fp = popen ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        getline (&init_layout, &siz, fp);
        pclose (fp);
    }
    init_layout[strcspn (init_layout, "\r\n")] = 0;
    if (!strlen (init_layout)) sprintf (init_layout, "us");

    init_variant = NULL;
    siz = 0;
    if (fp = popen ("grep XKBVARIANT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        getline (&init_variant, &siz, fp);
        pclose (fp);
    }
    init_variant[strcspn (init_variant, "\r\n")] = 0;

    // clear the two lists we initialise here
    gtk_list_store_clear (model_list);
    gtk_list_store_clear (layout_list);

    // loop through lines in KeyboardNames file
    cptr = NULL;
    in_list = 0;
    modeln = 0;
    layoutn = 0;
    count = 0;
    fp = fopen ("/usr/share/console-setup/KeyboardNames.pl", "rb");
    while ((n = getline (&cptr, &siz, fp)) != -1)
    {
        if (n)
        {
            if (in_list)
            {
                if (cptr[0] == ')')
                {
                    in_list = 0;
                    count = 0;
                }
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
                        if (!g_strcmp0 (t2, init_model)) modeln = count;
                    }
                    if (in_list == 2)
                    {
                        gtk_list_store_append (layout_list, &iter);
                        gtk_list_store_set (layout_list, &iter, 0, t1, 1, t2, -1);
                        if (!g_strcmp0 (t2, init_layout)) layoutn = count;
                    }
                    count++;
                }
            }
            if (!strncmp ("%models", cptr, 7)) in_list = 1;
            if (!strncmp ("%layouts", cptr, 8)) in_list = 2;
        }
    }
    fclose (fp);
    g_free (cptr);

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

    gtk_combo_box_set_active (GTK_COMBO_BOX (keymodel_cb), modeln);
    gtk_combo_box_set_active (GTK_COMBO_BOX (keylayout_cb), layoutn);

    g_signal_connect (keylayout_cb, "changed", G_CALLBACK (layout_changed), NULL);
    layout_changed (GTK_COMBO_BOX (keyvar_cb), init_variant);

    g_object_unref (builder);

    // run the dialog
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        n = 0;
        path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keymodel_cb)), -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (model_list), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (model_list), &iter, 1, &new_mod, -1);
        if (g_strcmp0 (new_mod, init_model))
        {
            vsystem ("grep -q XKBMODEL /etc/default/keyboard && sed -i 's/XKBMODEL=.*/XKBMODEL=%s/g' /etc/default/keyboard || echo 'XKBMODEL=%s' >> /etc/default/keyboard", new_mod, new_mod);
            n = 1;
        }
        gtk_tree_path_free (path);

        path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keylayout_cb)), -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (layout_list), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 1, &new_lay, -1);
        if (g_strcmp0 (new_lay, init_layout))
        {
            vsystem ("grep -q XKBLAYOUT /etc/default/keyboard && sed -i 's/XKBLAYOUT=.*/XKBLAYOUT=%s/g' /etc/default/keyboard || echo 'XKBLAYOUT=%s' >> /etc/default/keyboard", new_lay, new_lay);
            n = 1;
        }
        gtk_tree_path_free (path);

        path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keyvar_cb)), -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (variant_list), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (variant_list), &iter, 1, &new_var, -1);
        if (g_strcmp0 (new_var, init_variant))
        {
            vsystem ("grep -q XKBVARIANT /etc/default/keyboard && sed -i 's/XKBVARIANT=.*/XKBVARIANT=%s/g' /etc/default/keyboard || echo 'XKBVARIANT=%s' >> /etc/default/keyboard", new_var, new_var);
            n = 1;
        }
        gtk_tree_path_free (path);

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
    gtk_widget_destroy (dlg);
}

/* Button handlers */

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
    system (EXPAND_FS);
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

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    int reboot = 0;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
        || orig_autolog == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) system (SET_BOOT_GUIA);
            else system (SET_BOOT_CLIA);
        }
        else
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) system (SET_BOOT_GUI);
            else system (SET_BOOT_CLI);
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

    if (orig_pixdub != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pixdub_off_rb)))
    {
        vsystem (SET_PIXDUB, (1 - orig_pixdub));
        reboot = 1;
    }

    if (orig_ssh != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_off_rb)))
    {
        vsystem (SET_SSH, (1 - orig_ssh));
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

    if (g_strcmp0 (orig_hostname, gtk_entry_get_text (GTK_ENTRY (hostname_tb))))
    {
        vsystem (SET_HOSTNAME, gtk_entry_get_text (GTK_ENTRY (hostname_tb)));
        reboot = 1;
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
    system ("[ -e /boot/config.txt ] || touch /boot/config.txt");
#endif

    return 1;
}



/* The dialog... */

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

    // disable the buttons if RealVNC isn't installed
    gboolean enable = TRUE;
    struct stat buf;
    if (stat ("/usr/share/doc/realvnc-vnc-server", &buf)) enable = FALSE;
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_on_rb), enable);
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_off_rb), enable);

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

    GtkObject *adj = gtk_adjustment_new (64.0, 16.0, get_total_mem () - 128, 16.0, 64.0, 0);
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
    item = gtk_builder_get_object (builder, "hbox17");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox18");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox21");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox23");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox24");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox25");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox26");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox27");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox28");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox29");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2a");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2b");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2c");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2d");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2e");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox2f");
    gtk_widget_show (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "vbox30");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox48");
    gtk_widget_hide (GTK_WIDGET (item));
    item = gtk_builder_get_object (builder, "hbox49");
    gtk_widget_hide (GTK_WIDGET (item));
#endif

    GdkPixbuf *win_icon = gtk_window_get_icon (GTK_WINDOW (main_dlg));
    GList *list;
    list = g_list_append (list, win_icon);
    gtk_window_set_default_icon_list (list);

    // set up list stores for keyboard layouts
    model_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    layout_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    variant_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

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
            system ("reboot");
        }
        gtk_widget_destroy (dlg);
    }

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);
    gdk_threads_leave ();

    return 0;
}
