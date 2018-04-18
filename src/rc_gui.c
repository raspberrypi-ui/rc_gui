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
static GObject *autologin_cb, *netwait_cb, *splash_on_rb, *splash_off_rb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwentry3_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb, *resolution_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb, *keymodel_cb, *keylayout_cb, *keyvar_cb;
static GObject *language_ls, *country_ls;

static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static char orig_hostname[128];
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_spi, orig_i2c, orig_serial, orig_splash;
static int orig_clock, orig_gpumem, orig_autolog, orig_netwait, orig_onewire, orig_rgpio, orig_vnc, orig_pixdub;

/* Reboot flag set after locale change */

static int needs_reboot;

/* Number of items in comboboxes */

static int loc_count, country_count, char_count;

/* Globals accessed from multiple threads */

static char gbuffer[512];
GThread *pthread;

/* Lists for keyboard setting */

GtkListStore *model_list, *layout_list, *variant_list;

/* Helpers */

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];
    int res;

    if (fp == NULL) return 0;
    if (fgets (buf, sizeof (buf) - 1, fp) != NULL)
    {
        sscanf (buf, "%d", &res);
        pclose (fp);
        return res;
    }
    pclose (fp);
    return 0;
}

static void get_string (char *cmd, char *name)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];

    name[0] = 0;
    if (fp == NULL) return;
    if (fgets (buf, sizeof (buf) - 1, fp) != NULL)
    {
        sscanf (buf, "%s", name);
    }
    pclose (fp);
}

static int get_total_mem (void)
{
    FILE *fp;
    char buf[64];
    int arm, gpu;
    
    fp = popen ("vcgencmd get_mem arm", "r");
    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "arm=%dM", &arm);
    pclose (fp);

    fp = popen ("vcgencmd get_mem gpu", "r");
    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "gpu=%dM", &gpu);
    pclose (fp);

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

static int get_quoted_param (char *path, char *fname, char *toseek, char *result)
{
    char buffer[256], *linebuf = NULL, *cptr, *dptr;
    int len = 0;

    sprintf (buffer, "%s/%s", path, fname);
    FILE *fp = fopen (buffer, "rb");
    if (!fp) return 0;

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
            if (dptr) strcpy (result, dptr);
            else result[0] = 0;

            // done
            free (linebuf);
            fclose (fp);
            return 1;
        }
    }

    // end of file with no match
    result[0] = 0;
    free (linebuf);
    fclose (fp);
    return 0;
}

static void get_language (char *instr, char *lang)
{
    char *cptr = lang;
    int count;

    while (count < 4 && instr[count] >= 'a' && instr[count] <= 'z')
    {
        *cptr++ = instr[count++];
    }
    if (count < 2 || count > 3 || (instr[count] != '_' && instr[count] != 0  && instr[count] != '.')) *lang = 0;
    else *cptr = 0;
}

static void get_country (char *instr, char *ctry)
{
    char *cptr = ctry;
    int count;

    while (instr[count] != '_' && instr[count] != 0 && count < 5) count++;

    if (count == 5 || instr[count] == 0) *ctry = 0;
    else
    {
        count++;
        while (instr[count] && instr[count] != '.') *cptr++ = instr[count++];
        *cptr = 0;
    }
}

/* Password setting */

static void set_passwd (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb))) && strcmp (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), 
		gtk_entry_get_text (GTK_ENTRY(pwentry3_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128];
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
        sprintf (buffer, CHANGE_PASSWD, gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), gtk_entry_get_text (GTK_ENTRY (pwentry3_tb)));
        res = system (buffer);
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

static void delay_warning (char *msg)
{
    msg_dlg = (GtkWidget *) gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (msg_dlg), "");
    gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
    gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);
    gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (msg_dlg), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
    GtkWidget *frame = gtk_frame_new (NULL);
    GtkWidget *label = (GtkWidget *) gtk_label_new (msg);
    gtk_misc_set_padding (GTK_MISC (label), 20, 20);
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (msg_dlg))), frame);
    gtk_container_add (GTK_CONTAINER (frame), label);
    gtk_widget_show_all (msg_dlg);
}

static void country_changed (GtkComboBox *cb, char *ptr)
{
    char buffer[1024], cb_lang[64], cb_ctry[64], *cb_ext, init_char[32], *cptr;
    FILE *fp;

    // clear the combo box
    while (char_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (locchar_cb), 0);
    char_count = 0;

    // if an initial setting is supplied at ptr...
    if (ptr)
    {
        // find the line in SUPPORTED that exactly matches the supplied country string
        sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", ptr);
        fp = popen (buffer, "r");
        if (fp == NULL) return;
        while (fgets (buffer, sizeof (buffer) - 1, fp))
        {
            // copy the current character code into cur_char
            strtok (buffer, " ");
            cptr = strtok (NULL, " \n\r");
            strcpy (init_char, cptr);
        }
        pclose (fp);
    }
    else init_char[0] = 0;

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

    // build the grep expression to search the file of supported formats
    if (!cb_ctry[0])
        sprintf (buffer, "grep %s /usr/share/i18n/SUPPORTED", cb_lang);
    else if (!cb_ext[0])
        sprintf (buffer, "grep %s_%s /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry);
    else
        sprintf (buffer, "grep -E '%s_%s.*%s' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext);

    // run the grep and parse the returned lines
    fp = popen (buffer, "r");
    if (fp == NULL) return;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
        // find the second part of the returned line (separated by a space) and add to combo box
        strtok (buffer, " ");
        cptr = strtok (NULL, " \n\r");
        gtk_combo_box_append_text (GTK_COMBO_BOX (locchar_cb), cptr);

        // check to see if it matches the initial string and set active if so
        if (!strcmp (cptr, init_char)) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), char_count);
        char_count++;
    }
    pclose (fp);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
}

static void language_changed (GtkComboBox *cb, char *ptr)
{
    struct dirent **filelist, *dp;
    char buffer[1024], result[128], cb_lang[64], init_ctry[32], file_lang[8], file_ctry[64], *cptr;
    int entries, entry;

    // clear the combo box
    while (country_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (loccount_cb), 0);
    country_count = 0;

    // if an initial setting is supplied at ptr, extract the country code from the supplied string
    if (ptr) get_country (ptr, init_ctry);
    else init_ctry[0] = 0;

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb));
    if (cptr)
    {
        strcpy (cb_lang, cptr);
        strtok (cb_lang, " ");
        g_free (cptr);
    }
    else cb_lang[0] = 0;

    // loop through locale files
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language and country codes from the locale file name
        get_language (dp->d_name, file_lang);
        get_country (dp->d_name, file_ctry);

        // if the country code from the filename is valid,
        // and the language code from the filename matches the one in the combo box...
        if (*file_ctry && !strcmp (cb_lang, file_lang))
        {
            // read the territory description from the file
            get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "territory", result);

            // add country code and description to combo box
            sprintf (buffer, "%s (%s)", file_ctry, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loccount_cb), buffer);

            // check to see if it matches the initial string and set active if so
            if (!strcmp (file_ctry, init_ctry)) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), country_count);
            country_count++;
        }
        free (dp);
    }
    free (filelist);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);

    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);
}

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

static gpointer locale_thread (gpointer data)
{
    char buffer[256];

    system ("locale-gen");
    sprintf (buffer, "update-locale LANG=%s", gbuffer);
    system (buffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    struct dirent **filelist, *dp;
    char buffer[1024], result[128], init_locale[64], file_lang[8], last_lang[8], init_lang[8], *cptr;
    int count, entries, entry;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "loctable");
    loclang_cb = (GObject *) gtk_combo_box_new_text ();
    loccount_cb = (GObject *) gtk_combo_box_new_text ();
    locchar_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (loclang_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (loccount_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (locchar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (loclang_cb));
    gtk_widget_show_all (GTK_WIDGET (loccount_cb));
    gtk_widget_show_all (GTK_WIDGET (locchar_cb));

    // get the current locale setting and save as init_locale
    FILE *fp = popen ("grep LANG /etc/default/locale", "r");
    if (fp == NULL) return;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
        strtok (buffer, "=");
        cptr = strtok (NULL, "\n\r");
    }
    pclose (fp);
    strcpy (init_locale, cptr);

    // parse the initial locale to get the initial language code
    get_language (init_locale, init_lang);

    // loop through locale files
    last_lang[0] = 0;
    count = 0;
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language code from the locale file name
        get_language (dp->d_name, file_lang);

        // if it differs from the last one read, create a new entry
        if (file_lang[0] && strcmp (file_lang, last_lang))
        {
            // check to see if there is a file whose name has the format aa_AA; if not just use the first file we find
            sprintf (buffer, "%s_%c%c%c", file_lang, toupper (file_lang[0]), toupper (file_lang[1]), toupper (file_lang[2]));

            // ...and read the language description from the file
            if (!get_quoted_param ("/usr/share/i18n/locales", buffer, "language", result))
                get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "language", result);

            // add language code and description to combo box
            sprintf (buffer, "%s (%s)", file_lang, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loclang_cb), buffer);

            // make a local copy of the language code for comparisons
            strcpy (last_lang, file_lang);

            // highlight the current language setting...
            if (!strcmp (file_lang, init_lang)) gtk_combo_box_set_active (GTK_COMBO_BOX (loclang_cb), count);
            count++;
        }
        free (dp);
    }
    free (filelist);

    // populate the country and character lists and set the current values
    country_count = char_count = 0;
    language_changed (GTK_COMBO_BOX (loclang_cb), init_locale);
    country_changed (GTK_COMBO_BOX (loclang_cb), init_locale);

    g_signal_connect (loclang_cb, "changed", G_CALLBACK (language_changed), NULL);

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
                sprintf (buffer, "grep %s.*%s$ /usr/share/i18n/SUPPORTED", cb_lang, cptr);
            else if (!cb_ext[0])
                sprintf (buffer, "grep %s_%s.*%s$ /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry, cptr);
            else
                sprintf (buffer, "grep -E '%s_%s.*%s.*%s$' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext, cptr);
            g_free (cptr);

            // run the grep and parse the returned line
            fp = popen (buffer, "r");
            if (fp != NULL)
            {
                fgets (buffer, sizeof (buffer) - 1, fp);
                cptr = strtok (buffer, " ");
                strcpy (gbuffer, cptr);
                pclose (fp);
            }

            if (gbuffer[0] && strcmp (gbuffer, init_locale))
            {
                // look up the current locale setting from init_locale in /etc/locale.gen
                sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", init_locale);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    pclose (fp);
                }

                // use sed to comment that line if uncommented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sed -i 's/^%s/# %s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // look up the new locale setting from gbuffer in /etc/locale.gen
                sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", gbuffer);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    pclose (fp);
                }

                // use sed to uncomment that line if commented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sed -i 's/^# %s/%s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // warn about a short delay...
                delay_warning (_("Setting locale - please wait..."));

                // launch a thread with the system call to update the generated locales
                g_thread_new (NULL, locale_thread, NULL);

                // set reboot flag
                needs_reboot = 1;
            }
        }
    }

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
    char buffer[128], *cptr;
    //DIR *dirp, *sdirp;
    struct dirent **filelist, *dp, **sfilelist, *sdp;
    struct stat st_buf;
    int entries, entry, sentries, sentry;

    while (loc_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (tzloc_cb), 0);
    loc_count = 0;

    cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb));
    sprintf (buffer, "/usr/share/zoneinfo/%s", cptr);
    stat (buffer, &st_buf);

    if (S_ISDIR (st_buf.st_mode))
    {
        entries = scandir (buffer, &filelist, dirfilter, alphasort);
        for (entry = 0; entry < entries; entry++)
        {
            dp = filelist[entry];
            if (dp->d_type == DT_DIR)
            {
                sprintf (buffer, "/usr/share/zoneinfo/%s/%s", cptr, dp->d_name);
                sentries = scandir (buffer, &sfilelist, dirfilter, alphasort);
                for (sentry = 0; sentry < sentries; sentry++)
                {
                    sdp = sfilelist[sentry];
                    sprintf (buffer, "%s/%s", dp->d_name, sdp->d_name);
                    gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), buffer);
                    if (ptr && !strcmp (ptr, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                    loc_count++;
                    free (sdp);
                }
                free (sfilelist);
            }
            else
            {
                gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), dp->d_name);
                if (ptr && !strcmp (ptr, dp->d_name)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                loc_count++;
            }
            free (dp);
        }
        free (filelist);
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
    char buffer[128], before[128], *cptr, *b1ptr, *b2ptr;
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
    get_string ("cat /etc/timezone | tr -d \" \t\n\r\"", buffer);
    strcpy (before, buffer);
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
        if (!strcmp (dp->d_name, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzarea_cb), count);
        count++;
        free (dp);
    }
    free (filelist);
    g_signal_connect (tzarea_cb, "changed", G_CALLBACK (area_changed), NULL);

    // populate the location list and set the current location
    area_changed (GTK_COMBO_BOX (tzarea_cb), cptr);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        b1ptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb));
        b2ptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb));
        if (b2ptr)
            sprintf (buffer, "%s/%s", b1ptr, b2ptr);
        else
            sprintf (buffer, "%s", b1ptr);

        if (strcmp (before, buffer))
        {
            if (b2ptr)
                sprintf (buffer, "echo '%s/%s' | tee /etc/timezone", b1ptr, b2ptr);
            else
                sprintf (buffer, "echo '%s' | tee /etc/timezone", b1ptr);
            system (buffer);

            // warn about a short delay...
            delay_warning (_("Setting timezone - please wait..."));

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }
        g_free (b1ptr);
        if (b2ptr) g_free (b2ptr);
    }
    gtk_widget_destroy (dlg);
}

/* Wifi country setting */

static void on_set_wifi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128], cnow[16], *cptr;
    FILE *fp;
    int n, found;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "wcdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "wctable");
    wccountry_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wccountry_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (wccountry_cb));

    // get the current country setting
    get_string (GET_WIFI_CTRY, cnow);
    if (cnow[0] == 0) sprintf (cnow, "00");

    // populate the combobox
    fp = fopen ("/usr/share/zoneinfo/iso3166.tab", "rb");
    found = 0;
    gtk_combo_box_append_text (GTK_COMBO_BOX (wccountry_cb), _("<not set>"));
    n = 1;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
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

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        // update the wpa_supplicant.conf file
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wccountry_cb));
        if (!strcmp (cptr, _("<not set>")))
        {
            sprintf (buffer, SET_WIFI_CTRY, "00");
            system (buffer);
        }
        else if (strncmp (cnow, cptr, 2))
        {
            strncpy (cnow, cptr, 2);
            cnow[2] = 0;
            sprintf (buffer, SET_WIFI_CTRY, cnow);
            system (buffer);
        }
        if (cptr) g_free (cptr);
    }
    gtk_widget_destroy (dlg);
}

/* Resolution setting */

static void on_set_res (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128], *cptr, entry[128];
    FILE *fp;
    int n, found, hmode, hgroup, mode, x, y, freq, ax, ay, conn;

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
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
		if (!strncmp (buffer, "Nothing", 7)) conn = 0;
    }
    fclose (fp);

    // populate the combobox
	if (conn)
	{
		gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), "Default - preferred monitor settings");
		found = 0;
		n = 1;

		// get valid CEA modes
		fp = popen ("tvservice -m CEA", "r");
		while (fgets (buffer, sizeof (buffer) - 1, fp))
		{
			if (buffer[0] != 0x0A && strstr (buffer, "progressive"))
			{
				sscanf (buffer + 11, "mode %d: %dx%d @ %dHz %d:%d,", &mode, &x, &y, &freq, &ax, &ay);
				if (x <= 1920 && y <= 1200)
				{
					sprintf (entry, "CEA mode %d %dx%d %dHz %d:%d", mode, x, y, freq, ax, ay);
					gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), entry);
					if (hgroup == 1 && hmode == mode) found = n;
					n++;
				}
			}
		}
		fclose (fp);

		// get valid DMT modes
		fp = popen ("tvservice -m DMT", "r");
		while (fgets (buffer, sizeof (buffer) - 1, fp))
		{
			if (buffer[0] != 0x0A && strstr (buffer, "progressive"))
			{
				sscanf (buffer + 11, "mode %d: %dx%d @ %dHz %d:%d,", &mode, &x, &y, &freq, &ax, &ay);
				if (x <= 1920 && y <= 1200)
				{
					sprintf (entry, "DMT mode %d %dx%d %dHz %d:%d", mode, x, y, freq, ax, ay);
					gtk_combo_box_append_text (GTK_COMBO_BOX (resolution_cb), entry);
					if (hgroup == 2 && hmode == mode) found = n;
					n++;
				}
			}
		}
		fclose (fp);
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
				case 4 : 	found = 1;
							break;
				case 9 : 	found = 2;
							break;
				case 16 : 	found = 3;
							break;
				case 85 : 	found = 4;
							break;
				case 35 : 	found = 5;
							break;
				case 51 : 	found = 6;
							break;
				case 82 : 	found = 7;
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
				sprintf (buffer, SET_HDMI_GP_MOD, 0, 0);
				system (buffer);
				needs_reboot = 1;
			}
		}
		else
		{
			// set config vars
			sscanf (cptr, "%s mode %d", buffer, &mode);
			if (hgroup != buffer[0] - 'B' || hmode != mode)
			{
				sprintf (buffer, SET_HDMI_GP_MOD, buffer[0] - 'B', mode);
				system (buffer);
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
    char buffer[32], *cptr, *t1, *t2;
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
    sprintf (buffer, "    '%s'", t2);
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
                        if (init_variant && !strcmp (t2, init_variant)) varn = count;
                    }
                    count++;
                }
            }
            if (!strncmp (buffer, cptr, strlen (buffer))) in_list = 1;
        }
    }
    fclose (fp);
    g_free (cptr);

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
    char init_model[32], init_layout[32], init_variant[32], *cptr, *t1, *t2, *new_mod, *new_lay, *new_var;
    FILE *fp;
    int siz, n, modeln, layoutn, in_list, count;

    // get the current keyboard settings
    *init_model = 0;
    if (fp = popen ("grep XKBMODEL /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        fgets (init_model, sizeof (init_model) - 1, fp);
        pclose (fp);
    }
    init_model[strcspn (init_model, "\r\n")] = 0;
    if (!strlen (init_model)) sprintf (init_model, "pc105");

    *init_layout = 0;
    if (fp = popen ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        fgets (init_layout, sizeof (init_layout) - 1, fp);
        pclose (fp);
    }
    init_layout[strcspn (init_layout, "\r\n")] = 0;
    if (!strlen (init_layout)) sprintf (init_layout, "us");

    *init_variant = 0;
    if (fp = popen ("grep XKBVARIANT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"' | rev | cut -d , -f 1 | rev", "r"))
    {
        fgets (init_variant, sizeof (init_variant) - 1, fp);
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
                        if (!strcmp (t2, init_model)) modeln = count;
                    }
                    if (in_list == 2)
                    {
                        gtk_list_store_append (layout_list, &iter);
                        gtk_list_store_set (layout_list, &iter, 0, t1, 1, t2, -1);
                        if (!strcmp (t2, init_layout)) layoutn = count;
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
            sprintf (gbuffer, "grep -q XKBMODEL /etc/default/keyboard && sed -i 's/XKBMODEL=.*/XKBMODEL=%s/g' /etc/default/keyboard || echo 'XKBMODEL=%s' >> /etc/default/keyboard", new_mod, new_mod);
            system (gbuffer);
            n = 1;
        }
        gtk_tree_path_free (path);

        path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keylayout_cb)), -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (layout_list), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 1, &new_lay, -1);
        if (g_strcmp0 (new_lay, init_layout))
        {
            sprintf (gbuffer, "grep -q XKBLAYOUT /etc/default/keyboard && sed -i 's/XKBLAYOUT=.*/XKBLAYOUT=%s/g' /etc/default/keyboard || echo 'XKBLAYOUT=%s' >> /etc/default/keyboard", new_lay, new_lay);
            system (gbuffer);
            n = 1;
        }
        gtk_tree_path_free (path);

        path = gtk_tree_path_new_from_indices (gtk_combo_box_get_active (GTK_COMBO_BOX (keyvar_cb)), -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (variant_list), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (variant_list), &iter, 1, &new_var, -1);
        if (g_strcmp0 (new_var, init_variant))
        {
            sprintf (gbuffer, "grep -q XKBVARIANT /etc/default/keyboard && sed -i 's/XKBVARIANT=.*/XKBVARIANT=%s/g' /etc/default/keyboard || echo 'XKBVARIANT=%s' >> /etc/default/keyboard", new_var, new_var);
            system (gbuffer);
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
            delay_warning (_("Setting keyboard - please wait..."));

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

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    char buffer[128];
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
        sprintf (buffer, SET_BOOT_WAIT, (1 - orig_netwait));
        system (buffer);
    }

    if (orig_splash != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (splash_off_rb)))
    {
        sprintf (buffer, SET_SPLASH, (1 - orig_splash));
        system (buffer);
    }

    if (orig_camera != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (camera_off_rb)))
    {
        sprintf (buffer, SET_CAMERA, (1 - orig_camera));
        system (buffer);
        reboot = 1;
    }

    if (orig_overscan != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (overscan_off_rb)))
    {
        sprintf (buffer, SET_OVERSCAN, (1 - orig_overscan));
        system (buffer);
        reboot = 1;
    }

    if (orig_pixdub != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pixdub_off_rb)))
    {
        sprintf (buffer, SET_PIXDUB, (1 - orig_pixdub));
        system (buffer);
        reboot = 1;
    }

    if (orig_ssh != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_off_rb)))
    {
        sprintf (buffer, SET_SSH, (1 - orig_ssh));
        system (buffer);
    }

    if (orig_vnc != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vnc_off_rb)))
    {
        sprintf (buffer, SET_VNC, (1 - orig_vnc));
        system (buffer);
    }

    if (orig_spi != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi_off_rb)))
    {
        sprintf (buffer, SET_SPI, (1 - orig_spi));
        system (buffer);
    }

    if (orig_i2c != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c_off_rb)))
    {
        sprintf (buffer, SET_I2C, (1 - orig_i2c));
        system (buffer);
    }

    if (orig_serial != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_off_rb)))
    {
        sprintf (buffer, SET_SERIAL, (1 - orig_serial));
        system (buffer);
        reboot = 1;
    }

    if (orig_onewire != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (onewire_off_rb)))
    {
        sprintf (buffer, SET_1WIRE, (1 - orig_onewire));
        system (buffer);
        reboot = 1;
    }

    if (orig_rgpio != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rgpio_off_rb)))
    {
        sprintf (buffer, SET_RGPIO, (1 - orig_rgpio));
        system (buffer);
    }

    if (strcmp (orig_hostname, gtk_entry_get_text (GTK_ENTRY (hostname_tb))))
    {
        sprintf (buffer, SET_HOSTNAME, gtk_entry_get_text (GTK_ENTRY (hostname_tb)));
        system (buffer);
        reboot = 1;
    }

    if (orig_gpumem != gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (memsplit_sb)))
    {
        sprintf (buffer, SET_GPU_MEM, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (memsplit_sb)));
        system (buffer);
        reboot = 1;
    }

    if (orig_clock != gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
    {
        switch (get_status (GET_PI_TYPE))
        {
            case 1:
                switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
                {
                    case 0 :    sprintf (buffer, SET_OVERCLOCK, "None");
                                break;
                    case 1 :    sprintf (buffer, SET_OVERCLOCK, "Modest");
                                break;
                    case 2 :    sprintf (buffer, SET_OVERCLOCK, "Medium");
                                break;
                    case 3 :    sprintf (buffer, SET_OVERCLOCK, "High");
                                break;
                    case 4 :    sprintf (buffer, SET_OVERCLOCK, "Turbo");
                                break;
                }
                system (buffer);
                reboot = 1;
                break;

            case 2:
                switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
                {
                    case 0 :    sprintf (buffer, SET_OVERCLOCK, "None");
                                break;
                    case 1 :    sprintf (buffer, SET_OVERCLOCK, "High");
                                break;
                }
                system (buffer);
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

    if (argc == 2 && !strcmp (argv[1], "-w"))
    {
        on_set_wifi (NULL, NULL);
        return 0;
    }

    if (argc == 2 && !strcmp (argv[1], "-k"))
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
    get_string (GET_HOSTNAME, orig_hostname);
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
    serial_off_rb = gtk_builder_get_object (builder, "rb_ser_off");
    if (orig_serial = (get_status (GET_SERIAL) | get_status (GET_SERIALHW))) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);

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
