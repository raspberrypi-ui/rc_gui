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
#define CHECK_SYSTEMCTL "command -v systemctl > /dev/null ; echo $?"
#define CHECK_SYSTEMD   "systemctl | grep -q '\\-\\.mount' ; echo $?"
#define CHECK_DEVROOT   "[ -h /dev/root ]; echo $?"
#define GET_SYSD_PART   "mount | sed -n 's|^/dev/\\(.*\\) on /.*|\\1|p'"
#define GET_DR_PART     "readlink /dev/root"
#define GET_LAST_PART   "sudo parted /dev/mmcblk0 -ms unit s p | tail -n 1 | cut -f 1 -d:"
#define GET_PIUSER      "id -u pi"
#define GET_DEVICETREE  "cat /boot/config.txt | grep -q ^device_tree=$ ; echo $?"
#define GET_PI_TYPE     "sudo raspi-config nonint get_pi_type"
#define GET_HOSTNAME    "cat /etc/hostname | tr -d \" \t\n\r\""
#define GET_TIMEZONE    "cat /etc/timezone | tr -d \" \t\n\r\""
#define GET_MEM_ARM     "vcgencmd get_mem arm"
#define GET_MEM_GPU     "vcgencmd get_mem gpu"
#define GET_OVERCLOCK   "sudo raspi-config nonint get_config_var arm_freq /boot/config.txt"
#define GET_GPU_MEM     "sudo raspi-config nonint get_config_var gpu_mem /boot/config.txt"
#define GET_OVERSCAN    "sudo raspi-config nonint get_config_var disable_overscan /boot/config.txt"
#define GET_CAMERA      "sudo raspi-config nonint get_config_var start_x /boot/config.txt"
#define GET_SSH         "service ssh status | grep -q inactive ; echo $?"
#define GET_SPI         "cat /boot/config.txt | grep -q -E \"^(device_tree_param|dtparam)=([^,]*,)*spi(=(on|true|yes|1))?(,.*)?$\" ; echo $?"
#define GET_I2C         "cat /boot/config.txt | grep -q -E \"^(device_tree_param|dtparam)=([^,]*,)*i2c(_arm)?(=(on|true|yes|1))?(,.*)?$\" ; echo $?"
#define GET_SERIAL      "cat /boot/cmdline.txt | grep -q -E \"(console=ttyAMA0|console=serial0)\" ; echo $?"
#define GET_1WIRE       "cat /boot/config.txt | grep -q -E \"^dtoverlay=w1-gpio\" ; echo $?"
#define GET_BOOT_GUI    "service lightdm status | grep -q inactive ; echo $?"
#define GET_BOOT_SLOW   "test -e /etc/systemd/system/dhcpcd.service.d/wait.conf ; echo $?"
#define GET_ALOG_SYSD   "cat /etc/systemd/system/getty.target.wants/getty@tty1.service | grep -q autologin ; echo $?"
#define GET_ALOG_INITD  "cat /etc/inittab | grep -q login ; echo $?"
#define GET_ALOG_GUI    "cat /etc/lightdm/lightdm.conf | grep -q \"#autologin-user=\" ; echo $?"
#define SET_HOSTNAME    "sudo raspi-config nonint do_change_hostname %s"
#define SET_OVERCLOCK   "sudo raspi-config nonint do_overclock %s"
#define SET_GPU_MEM     "sudo raspi-config nonint do_memory_split %d"
#define SET_OVERSCAN    "sudo raspi-config nonint do_overscan %d"
#define SET_CAMERA      "sudo raspi-config nonint do_camera %d"
#define SET_SSH         "sudo raspi-config nonint do_ssh %d"
#define SET_SPI         "sudo raspi-config nonint do_spi %d"
#define SET_I2C         "sudo raspi-config nonint do_i2c %d"
#define SET_SERIAL      "sudo raspi-config nonint do_serial %d"
#define SET_1WIRE       "sudo raspi-config nonint do_onewire %d"
#define SET_BOOT_CLI    "sudo raspi-config nonint do_boot_behaviour_new B1"
#define SET_BOOT_CLIA   "sudo raspi-config nonint do_boot_behaviour_new B2"
#define SET_BOOT_GUI    "sudo raspi-config nonint do_boot_behaviour_new B3"
#define SET_BOOT_GUIA   "sudo raspi-config nonint do_boot_behaviour_new B4"
#define SET_BOOT_FAST   "sudo raspi-config nonint do_wait_for_network Fast"
#define SET_BOOT_SLOW   "sudo raspi-config nonint do_wait_for_network Slow"
#define SET_RASTRACK    "curl --data \"name=%s&email=%s\" http://rastrack.co.uk/api.php"
#define CHANGE_PASSWD   "echo pi:%s | sudo chpasswd"
#define EXPAND_FS       "sudo raspi-config nonint do_expand_rootfs"
#define FIND_LOCALE     "grep '%s ' /usr/share/i18n/SUPPORTED"
#define GET_WIFI_CTRY	"sudo grep country= /etc/wpa_supplicant/wpa_supplicant.conf | cut -d \"=\" -f 2"
#define SET_WIFI_CTRY   "sudo raspi-config nonint do_configure_wifi_country %s"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *locale_btn, *timezone_btn, *keyboard_btn, *rastrack_btn, *wifi_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb, *onewire_on_rb, *onewire_off_rb;
static GObject *autologin_cb, *netwait_cb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb;
static GObject *language_ls, *country_ls;

static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static char orig_hostname[128];
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_spi, orig_i2c, orig_serial;
static int orig_clock, orig_gpumem, orig_autolog, orig_netwait, orig_onewire;

/* Reboot flag set after locale change */

static int needs_reboot;

/* Number of items in comboboxes */

static int loc_count, country_count, char_count;

/* Global locale accessed from multiple threads */

static char glocale[64];

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
        return res;
    }
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
        return;
    }
}

static int get_total_mem (void)
{
    FILE *fp;
    char buf[64];
    int arm, gpu;
    
    fp = popen (GET_MEM_ARM, "r");
    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "arm=%dM", &arm);
    
    fp = popen (GET_MEM_GPU, "r");
    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "gpu=%dM", &gpu);
        
    return arm + gpu;    
}

static void get_quoted_param (char *path, char *fname, char *toseek, char *result)
{
    char buffer[256], *linebuf = NULL, *cptr, *dptr;
    int len = 0;

    sprintf (buffer, "%s/%s", path, fname);
    FILE *fp = fopen (buffer, "rb");

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
            return;
        }
    }

    // end of file with no match
    result[0] = 0;
    free (linebuf);
    fclose (fp);
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
	if (strcmp (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb)), gtk_entry_get_text (GTK_ENTRY(pwentry2_tb))))
	    gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
	else
	    gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
	GtkBuilder *builder;
	GtkWidget *dlg;
	char buffer[128];

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
	dlg = (GtkWidget *) gtk_builder_get_object (builder, "passwddialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
	pwentry1_tb = gtk_builder_get_object (builder, "pwentry1");
	pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
	g_signal_connect (pwentry1_tb, "changed", G_CALLBACK (set_passwd), NULL);
	g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (set_passwd), NULL);
	pwok_btn = gtk_builder_get_object (builder, "passwdok");
	gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    sprintf (buffer, CHANGE_PASSWD, gtk_entry_get_text (GTK_ENTRY (pwentry1_tb)));
	    system (buffer);
	}
	gtk_widget_destroy (dlg);
}

/* Locale setting */

static void country_changed (GtkComboBox *cb, gpointer ptr)
{
    char buffer[1024], cb_lang[64], cb_ctry[64], *cb_ext, init_char[32], *cptr;
    FILE *fp;

    // clear the combo box
    while (char_count--) gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (locchar_cb), 0);
    char_count = 0;

    // if an initial setting is supplied at ptr...
    if (ptr)
    {
        // find the line in SUPPORTED that exactly matches the supplied country string
        sprintf (buffer, FIND_LOCALE, ptr);
        fp = popen (buffer, "r");
        if (fp == NULL) return;
        while (fgets (buffer, sizeof (buffer) - 1, fp))
        {
            // copy the current character code into cur_char
            strtok (buffer, " ");
            cptr = strtok (NULL, " \n\r");
            strcpy (init_char, cptr);
        }
        fclose (fp);
    }
    else init_char[0] = 0;

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
    if (cptr)
    {
        strcpy (cb_lang, cptr);
        strtok (cb_lang, " ");
    }
    else cb_lang[0] = 0;

    // read the country from the combo box and split off code and extension
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loccount_cb));
    if (cptr)
    {
        strcpy (cb_ctry, cptr);
        strtok (cb_ctry, "@ ");
        cb_ext = strtok (NULL, "@ ");
        if (cb_ext[0] == '(') cb_ext[0] = 0;
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
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (locchar_cb), cptr);

        // check to see if it matches the initial string and set active if so
        if (!strcmp (cptr, init_char)) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), char_count);
        char_count++;
    }
    fclose (fp);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
}

static void language_changed (GtkComboBox *cb, gpointer ptr)
{
    struct dirent **filelist, *dp;
    char buffer[1024], result[128], cb_lang[64], init_ctry[32], file_lang[8], file_ctry[64], *cptr;
    int entries, entry;

    // clear the combo box
    while (country_count--) gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (loccount_cb), 0);
    country_count = 0;

    // if an initial setting is supplied at ptr, extract the country code from the supplied string
    if (ptr) get_country (ptr, init_ctry);
    else init_ctry[0] = 0;

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
    if (cptr)
    {
        strcpy (cb_lang, cptr);
        strtok (cb_lang, " ");
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

    system ("sudo locale-gen");
    sprintf (buffer, "sudo update-locale LANG=%s", glocale);
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
	loclang_cb = (GObject *) gtk_combo_box_text_new ();
	loccount_cb = (GObject *) gtk_combo_box_text_new ();
	locchar_cb = (GObject *) gtk_combo_box_text_new ();
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
    fclose (fp);
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
            // read the language description from the file
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
        cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
        if (cptr)
        {
            strcpy (cb_lang, cptr);
            strtok (cb_lang, " ");
        }
        else cb_lang[0] = 0;

        // read the country from the combo box and split off code and extension
        cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loccount_cb));
        if (cptr)
        {
            strcpy (cb_ctry, cptr);
            strtok (cb_ctry, "@ ");
            cb_ext = strtok (NULL, "@ ");
            if (cb_ext[0] == '(') cb_ext[0] = 0;
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

            // run the grep and parse the returned line
            fp = popen (buffer, "r");
            if (fp != NULL)
            {
                fgets (buffer, sizeof (buffer) - 1, fp);
                cptr = strtok (buffer, " ");
                strcpy (glocale, cptr);
                fclose (fp);
            }

            if (glocale[0] && strcmp (glocale, init_locale))
            {
                // look up the current locale setting from init_locale in /etc/locale.gen
                sprintf (buffer, FIND_LOCALE, init_locale);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    fclose (fp);
                }

                // use sed to comment that line if uncommented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sudo sed -i 's/^%s/# %s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // look up the new locale setting from glocale in /etc/locale.gen
                sprintf (buffer, FIND_LOCALE, glocale);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    fclose (fp);
                }

                // use sed to uncomment that line if commented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sudo sed -i 's/^# %s/%s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // warn about a short delay...
                msg_dlg = (GtkWidget *) gtk_dialog_new ();
                gtk_window_set_title (GTK_WINDOW (msg_dlg), "");
                gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);
                gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_skip_taskbar_hint (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
                GtkWidget *frame = gtk_frame_new (NULL);
                GtkWidget *label = (GtkWidget *) gtk_label_new (_("Setting locale - please wait..."));
                gtk_misc_set_padding (GTK_MISC (label), 20, 20);
                gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (msg_dlg))), frame);
                gtk_container_add (GTK_CONTAINER (frame), label);
	            gtk_widget_show_all (msg_dlg);

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
	char buffer[128];
    //DIR *dirp, *sdirp;
    struct dirent **filelist, *dp, **sfilelist, *sdp;
    struct stat st_buf;
    int entries, entry, sentries, sentry;

    while (loc_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (tzloc_cb), 0);
    loc_count = 0;

    sprintf (buffer, "/usr/share/zoneinfo/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));
    stat (buffer, &st_buf);

    if (S_ISDIR (st_buf.st_mode))
    {
        entries = scandir (buffer, &filelist, dirfilter, alphasort);
        for (entry = 0; entry < entries; entry++)
        {
            dp = filelist[entry];
            if (dp->d_type == DT_DIR)
            {
                sprintf (buffer, "/usr/share/zoneinfo/%s/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)), dp->d_name);
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
}

static gpointer timezone_thread (gpointer data)
{
    system ("sudo dpkg-reconfigure --frontend noninteractive tzdata");
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
	char buffer[128], before[128], *cptr;
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
	get_string (GET_TIMEZONE, buffer);
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
	    if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
            sprintf (buffer, "%s/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
                gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
        else
            sprintf (buffer, "%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));

        if (strcmp (before, buffer))
        {
            if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
                sprintf (buffer, "echo '%s/%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
                    gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
            else
                sprintf (buffer, "echo '%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));
            system (buffer);

            // warn about a short delay...
            msg_dlg = (GtkWidget *) gtk_dialog_new ();
            gtk_window_set_title (GTK_WINDOW (msg_dlg), "");
            gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);
            gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_skip_taskbar_hint (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
            GtkWidget *frame = gtk_frame_new (NULL);
            GtkWidget *label = (GtkWidget *) gtk_label_new (_("Setting timezone - please wait..."));
            gtk_misc_set_padding (GTK_MISC (label), 20, 20);
            gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (msg_dlg))), frame);
            gtk_container_add (GTK_CONTAINER (frame), label);
            gtk_widget_show_all (msg_dlg);

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }
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

    // populate the combobox
    fp = fopen ("/usr/share/zoneinfo/iso3166.tab", "rb");
    n = 0;
    found = 0;
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
        sprintf (buffer, "%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (wccountry_cb)));
        if (strncmp (cnow, buffer, 2))
        {
            strncpy (cnow, buffer, 2);
            cnow[2] = 0;
            sprintf (buffer, SET_WIFI_CTRY, cnow);
            system (buffer);
            needs_reboot = 1;
        }
    }
    gtk_widget_destroy (dlg);
}

/* Rastrack setting */

static void rt_change (GtkEntry *entry, gpointer ptr)
{
	if (strlen (gtk_entry_get_text (GTK_ENTRY (rtname_tb))) && strlen (gtk_entry_get_text (GTK_ENTRY (rtemail_tb))))
	    gtk_widget_set_sensitive (GTK_WIDGET (rtok_btn), TRUE);
	else
	    gtk_widget_set_sensitive (GTK_WIDGET (rtok_btn), FALSE);
}

static void on_set_rastrack (GtkButton* btn, gpointer ptr)
{
	GtkBuilder *builder;
	GtkWidget *dlg;
	char buffer[128];

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
	dlg = (GtkWidget *) gtk_builder_get_object (builder, "rastrackdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
	rtname_tb = gtk_builder_get_object (builder, "rtentry1");
	rtemail_tb = gtk_builder_get_object (builder, "rtentry2");
	g_signal_connect (rtname_tb, "changed", G_CALLBACK (rt_change), NULL);
	g_signal_connect (rtemail_tb, "changed", G_CALLBACK (rt_change), NULL);
	rtok_btn = gtk_builder_get_object (builder, "rastrackok");
	gtk_widget_set_sensitive (GTK_WIDGET(rtok_btn), FALSE);
	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    sprintf (buffer, SET_RASTRACK, gtk_entry_get_text (GTK_ENTRY (rtname_tb)), gtk_entry_get_text (GTK_ENTRY (rtemail_tb)));
	    system (buffer);
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

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    system ("lxkeymap");
}

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    char buffer[128];
    int reboot = 0;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
        || orig_autolog != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
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
        reboot = 1;
    }
    
    if (orig_netwait != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (netwait_cb)))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (netwait_cb))) system (SET_BOOT_SLOW);
        else system (SET_BOOT_FAST);
    }

    if (orig_camera != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (camera_on_rb)))
    {
	    sprintf (buffer, SET_CAMERA, (1 - orig_camera));
	    system (buffer);
	    reboot = 1;
    }

    if (orig_overscan != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (overscan_off_rb)))
    {
	    sprintf (buffer, SET_OVERSCAN, orig_overscan);
	    system (buffer);
	    reboot = 1;
    }

    if (orig_ssh != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_on_rb)))
    {
	    sprintf (buffer, SET_SSH, orig_ssh);
	    system (buffer);
    }

    if (orig_spi != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi_off_rb)))
    {
	    sprintf (buffer, SET_SPI, (1 - orig_spi));
	    system (buffer);
	    reboot = 1;
    }

    if (orig_i2c != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c_off_rb)))
    {
	    sprintf (buffer, SET_I2C, (1 - orig_i2c));
	    system (buffer);
	    reboot = 1;
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

    // check lightdm is installed
    if (stat ("/etc/init.d/lightdm", &buf)) return 0;

    // check config file exists
    if (stat ("/boot/config.txt", &buf)) return 0;

    // check startx.elf is present
    if (stat ("/boot/start_x.elf", &buf)) return 0;

    // check device tree is enabled
    if (!get_status (GET_DEVICETREE)) return 0;

    // check pi user exists
    if (!get_status (GET_PIUSER)) return 0;
    return 1;
}

static int can_expand_fs (void)
{
    char buffer[128];
    int part_num, last_part_num;

    // first check whether systemd is used
    if (!get_status (CHECK_SYSTEMCTL) && !get_status (CHECK_SYSTEMD))
    {
        // systemd used
        get_string (GET_SYSD_PART, buffer);
    }
    else
    {
        // systemd not used - check that /dev/root is a symlink
        if (!get_status (CHECK_DEVROOT))
        {
            get_string (GET_DR_PART, buffer);
        }
        else return 0;
    }
    if (sscanf (buffer, "mmcblk0p%d", &part_num) != 1) return 0;
    if (part_num != 2) return 0;
    last_part_num = get_status (GET_LAST_PART);
    if (last_part_num != part_num) return 0;
    return 1;
}

/* Auto login and boot options */

static int autologin_enabled (void)
{
    if (get_status (GET_BOOT_GUI))
    {
        /* booting to desktop - check the autologin for lightdm */
        return get_status (GET_ALOG_GUI);
    }
    else
    {
        /* booting to CLI - check the autologin in getty */
        if (!get_status (CHECK_SYSTEMD))
        {
            /* systemd used - check getty */
            if (!get_status (GET_ALOG_SYSD)) return 1;
            else return 0;
        }
        else
        {
            /* systemd not used - check initd */
            if (!get_status (GET_ALOG_INITD)) return 1;
            else return 0;
        }
    }
}

static int netwait_enabled (void)
{
    if (!get_status (GET_BOOT_SLOW)) return 1;
    else return 0;
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

	// build the UI
	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);

	if (!can_configure ())
	{
	    dlg = (GtkWidget *) gtk_builder_get_object (builder, "errordialog");
        gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
	    g_object_unref (builder);
	    gtk_dialog_run (GTK_DIALOG (dlg));
	    gtk_widget_destroy (dlg);
	    return 0;
	}

	main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
	
	expandfs_btn = gtk_builder_get_object (builder, "button3");
	g_signal_connect (expandfs_btn, "clicked", G_CALLBACK (on_expand_fs), NULL);
	if (can_expand_fs ()) gtk_widget_set_sensitive (GTK_WIDGET(expandfs_btn), TRUE);
	else gtk_widget_set_sensitive (GTK_WIDGET(expandfs_btn), FALSE);
	
	passwd_btn = gtk_builder_get_object (builder, "button4");
	g_signal_connect (passwd_btn, "clicked", G_CALLBACK (on_change_passwd), NULL);
	
	locale_btn = gtk_builder_get_object (builder, "button5");
	g_signal_connect (locale_btn, "clicked", G_CALLBACK (on_set_locale), NULL);
	
	timezone_btn = gtk_builder_get_object (builder, "button6");
	g_signal_connect (timezone_btn, "clicked", G_CALLBACK (on_set_timezone), NULL);
	
	keyboard_btn = gtk_builder_get_object (builder, "button7");
	g_signal_connect (keyboard_btn, "clicked", G_CALLBACK (on_set_keyboard), NULL);

	rastrack_btn = gtk_builder_get_object (builder, "button8");
	g_signal_connect (rastrack_btn, "clicked", G_CALLBACK (on_set_rastrack), NULL);

	wifi_btn = gtk_builder_get_object (builder, "button11");
	g_signal_connect (wifi_btn, "clicked", G_CALLBACK (on_set_wifi), NULL);

	boot_desktop_rb = gtk_builder_get_object (builder, "radiobutton1");
	boot_cli_rb = gtk_builder_get_object (builder, "radiobutton2");
	if (orig_boot = get_status (GET_BOOT_GUI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);

	autologin_cb = gtk_builder_get_object (builder, "checkbutton1");
    if (orig_autolog = autologin_enabled ()) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), FALSE);

	netwait_cb = gtk_builder_get_object (builder, "checkbutton2");
    if (orig_netwait = netwait_enabled ()) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), FALSE);

	camera_on_rb = gtk_builder_get_object (builder, "radiobutton3");
	camera_off_rb = gtk_builder_get_object (builder, "radiobutton4");
	if (orig_camera = get_status (GET_CAMERA)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_off_rb), TRUE);
	
	overscan_on_rb = gtk_builder_get_object (builder, "radiobutton5");
	overscan_off_rb = gtk_builder_get_object (builder, "radiobutton6");
	if (orig_overscan = get_status (GET_OVERSCAN)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_on_rb), TRUE);
	
	ssh_on_rb = gtk_builder_get_object (builder, "radiobutton7");
	ssh_off_rb = gtk_builder_get_object (builder, "radiobutton8");
	if (orig_ssh = get_status (GET_SSH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_off_rb), TRUE);
	
	spi_on_rb = gtk_builder_get_object (builder, "radiobutton11");
	spi_off_rb = gtk_builder_get_object (builder, "radiobutton12");
	if (orig_spi = get_status (GET_SPI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_on_rb), TRUE);
	
	i2c_on_rb = gtk_builder_get_object (builder, "radiobutton13");
	i2c_off_rb = gtk_builder_get_object (builder, "radiobutton14");
	if (orig_i2c = get_status (GET_I2C)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_on_rb), TRUE);
	
	serial_on_rb = gtk_builder_get_object (builder, "radiobutton15");
	serial_off_rb = gtk_builder_get_object (builder, "radiobutton16");
	if (orig_serial = get_status (GET_SERIAL)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);
	
	onewire_on_rb = gtk_builder_get_object (builder, "radiobutton17");
	onewire_off_rb = gtk_builder_get_object (builder, "radiobutton18");
	if (orig_onewire = get_status (GET_1WIRE)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_on_rb), TRUE);
	
    switch (get_status (GET_PI_TYPE))
	{
	    case 1:
            overclock_cb = gtk_builder_get_object (builder, "comboboxtext1");
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
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox8")));
            gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox7")));
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox19")));
            break;

	    case 2 :
            overclock_cb = gtk_builder_get_object (builder, "comboboxtext2");
            switch (get_status (GET_OVERCLOCK))
            {
                case 1000 : orig_clock = 1;
                            break;
                default   : orig_clock = 0;
                            break;
            }
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), orig_clock);
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox7")));
            gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox8")));
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox19")));
            break;

        default :
            overclock_cb = gtk_builder_get_object (builder, "comboboxtext3");
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 0);
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox7")));
            gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox8")));
            gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox19")));
            gtk_widget_set_sensitive (GTK_WIDGET(overclock_cb), FALSE);
	        break;
	}

	GtkObject *adj = gtk_adjustment_new (64.0, 16.0, get_total_mem () - 128, 16.0, 64.0, 0);
	memsplit_sb = gtk_builder_get_object (builder, "spinbutton1");
	gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (memsplit_sb), GTK_ADJUSTMENT (adj));
	orig_gpumem = get_status (GET_GPU_MEM);
	if (orig_gpumem == 0) orig_gpumem = 64;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (memsplit_sb), orig_gpumem);

	hostname_tb = gtk_builder_get_object (builder, "entry1");
	get_string (GET_HOSTNAME, orig_hostname);
	gtk_entry_set_text (GTK_ENTRY (hostname_tb), orig_hostname);

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
            system ("sudo reboot");
        }
	    gtk_widget_destroy (dlg);
	}

	g_object_unref (builder);
	gtk_widget_destroy (main_dlg);
	gdk_threads_leave ();

	return 0;
}
