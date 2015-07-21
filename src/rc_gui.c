#include <string.h>
#include <math.h>
#include <ctype.h>
#include <mem.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

/* Command strings */
#define GET_HOSTNAME    "cat /etc/hostname | tr -d \" \t\n\r\""
#define GET_TIMEZONE    "cat /etc/timezone | tr -d \" \t\n\r\""
#define IS_PI2          "cat /proc/cpuinfo | grep BCM2709"
#define GET_MEM_ARM     "vcgencmd get_mem arm"
#define GET_MEM_GPU     "vcgencmd get_mem gpu"
#define SET_RASTRACK    "curl --data \"name=%s&email=%s\" http://rastrack.co.uk/api.php"
#define CHANGE_PASSWD   "echo pi:%s | sudo chpasswd"
#define CAN_EXPAND      "sudo raspi-config nonint can_expand_rootfs"
#define EXPAND_FS       "sudo raspi-config nonint do_expand_rootfs"
#define GET_OVERCLOCK   "sudo raspi-config nonint get_config_var arm_freq /boot/config.txt"
#define GET_GPU_MEM     "sudo raspi-config nonint get_config_var gpu_mem /boot/config.txt"
#define GET_OVERSCAN    "sudo raspi-config nonint get_config_var disable_overscan /boot/config.txt"
#define GET_CAMERA      "sudo raspi-config nonint get_config_var start_x /boot/config.txt"
#define GET_SSH         "sudo raspi-config nonint get_ssh"
#define GET_SPI         "sudo raspi-config nonint get_spi"
#define GET_I2C         "sudo raspi-config nonint get_i2c"
#define GET_SERIAL      "sudo raspi-config nonint get_serial"
#define GET_BOOT_GUI    "sudo raspi-config nonint get_boot_to_gui"
#define GET_CAN_CONF    "sudo raspi-config nonint get_can_configure"
#define SET_HOSTNAME    "sudo raspi-config nonint do_change_hostname %s"
#define SET_OVERCLOCK   "sudo raspi-config nonint do_overclock %s"
#define SET_GPU_MEM     "sudo raspi-config nonint do_memory_split %d"
#define SET_OVERSCAN    "sudo raspi-config nonint do_overscan %d"
#define SET_CAMERA      "sudo raspi-config nonint do_camera %d"
#define SET_SSH         "sudo raspi-config nonint do_ssh %d"
#define SET_SPI         "sudo raspi-config nonint do_spi %d"
#define SET_I2C         "sudo raspi-config nonint do_i2c %d"
#define SET_SERIAL      "sudo raspi-config nonint do_serial %d"
#define SET_BOOT_CLI    "sudo raspi-config nonint do_boot_behaviour Console"
#define SET_BOOT_GUI    "sudo raspi-config nonint do_boot_behaviour Desktop"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *locale_btn, *timezone_btn, *keyboard_btn, *rastrack_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb;
static GObject *language_ls, *country_ls;

/* Initial values */

static char orig_hostname[128];
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_spi, orig_i2c, orig_serial;
static int orig_clock, orig_gpumem;

/* Number of items in location combobox */

static int loc_count, country_count, char_count;

/* Helpers */

static int is_pi2 (void)
{
    FILE *fp = popen (IS_PI2, "r");
    char buf[64];
    int res;

    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        if (buf[0] == 'H') return 1;
    return 0;
}

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];
    int res;

    if (fp == NULL) return 0;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "%d", &res);
    return res;
}

static void get_string (char *cmd, char *name)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];
    int res;

    if (fp == NULL) return;
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "%s", name);
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

static void get_quoted_param (char *fname, char *toseek, char *buffer)
{
    char *linebuf = NULL, *cptr, *dptr;
    int len = 0;
    FILE *fp = fopen (fname, "rb");

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
            dptr = strtok (NULL, "\"");

            // copy to dest
            strcpy (buffer, dptr);

            // done
            free (linebuf);
            fclose (fp);
            return;
        }
    }

    // end of file with no match
    *buffer = 0;
    free (linebuf);
    fclose (fp);
}

static void get_code (char *instr, char *ostr)
{
    char *cptr = ostr;
    int count;

    while (count < 4 && instr[count] >= 'a' && instr[count] <= 'z')
    {
        *cptr++ = instr[count++];
    }
    if (count < 2 || count > 3 || (instr[count] != '_' && instr[count] != 0)) *ostr = 0;
    else *cptr = 0;
}

/* Button handlers */

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
    system (EXPAND_FS);
}

static void on_set_passwd (GtkEntry *entry, gpointer ptr)
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
	pwentry1_tb = gtk_builder_get_object (builder, "pwentry1");
	pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
	g_signal_connect (pwentry1_tb, "changed", G_CALLBACK (on_set_passwd), NULL);
	g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (on_set_passwd), NULL);
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

static void on_country_changed (GtkComboBox *cb, gpointer ptr)
{
    char buffer[1024], country[64], lang[64], *ext, cchar[32];
    FILE *fp;

    while (char_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (locchar_cb), 0);
    char_count = 0;

    if (ptr)
    {
        // need to find the line in SUPPORTED that exactly matches the country string
        // and set the charset to the second half
        sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", ptr);
       fp = popen (buffer, "r");
        if (fp == NULL) return;
        while (fgets (buffer, 1023, fp))
        {
            strtok (buffer, " ");
            ext = strtok (NULL, " \n\r");
            strcpy (cchar, ext);
        }
        fclose (fp);
    }

    // split the country code into code and extension (if any)
    sprintf (lang, gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb)));
    strtok (lang, " ");
    sprintf (country, gtk_combo_box_get_active_text (GTK_COMBO_BOX (loccount_cb)));
    if (!strlen (country)) return;
    strtok (country, "@ ");
    ext = strtok (NULL, "@ ");
    if (ext[0] == '(') ext[0] = 0;

    // build the relevant grep expression to search the file of supported formats
    if (ext[0]) sprintf (buffer, "grep -E '%s_%s.*%s' /usr/share/i18n/SUPPORTED", lang, country, ext);
    else sprintf (buffer, "grep %s_%s /usr/share/i18n/SUPPORTED | grep -v @", lang, country);

    // run the grep and parse the returned lines
    fp = popen (buffer, "r");
    if (fp == NULL) return;
    while (fgets (buffer, 1023, fp))
    {
        strtok (buffer, " ");
        ext = strtok (NULL, " \n\r");
        gtk_combo_box_append_text (GTK_COMBO_BOX (locchar_cb), ext);
        if (!strcmp (ext, cchar)) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), char_count);
        char_count++;
    }
    fclose (fp);
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
}

static void on_language_changed (GtkComboBox *cb, gpointer ptr)
{
    DIR *dirp;
    struct dirent *dp;
    char cc[64], code[8], *cptr, *dptr = 0;
    char buffer[256], text[128];

    while (country_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (loccount_cb), 0);
    country_count = 0;

    if (ptr)
    {
        strtok (ptr, "_");
        dptr = strtok (NULL, " .");
    }
    else dptr = 0;

    // get the country code from the combo box
    sscanf (gtk_combo_box_get_active_text (GTK_COMBO_BOX (loclang_cb)), "%s", cc);
    dirp = opendir ("/usr/share/i18n/locales");
    do
    {
        dp = readdir (dirp);
        if (dp)
        {
            get_code (dp->d_name, code);
            if (*code && !strcmp (cc, code))
            {
                sprintf (buffer, "/usr/share/i18n/locales/%s", dp->d_name);
                get_quoted_param (buffer, "territory", text);
                cptr = dp->d_name;
                while (*cptr++ != '_');
                sprintf (buffer, "%s (%s)", cptr, text);

	            gtk_combo_box_append_text (GTK_COMBO_BOX (loccount_cb), buffer);

	            if (dptr && !strcmp (dptr, cptr))
	                gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), country_count);
	            country_count++;
	        }
	    }
    } while (dp);

	if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);

	g_signal_connect (loccount_cb, "changed", G_CALLBACK (on_country_changed), NULL);
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
	GtkBuilder *builder;
	GtkWidget *dlg;
	char buffer[256], code[8], lastcode[8], text[128], ccode[8], country[32], *cptr;
    DIR *dirp;
    struct dirent *dp;

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
	dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");

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

	// get the current locale setting
    // run the grep and parse the returned line
    FILE *fp = popen ("grep LANG /etc/default/locale", "r");
    if (fp == NULL) return;
    while (fgets (buffer, 255, fp))
    {
        strtok (buffer, "=");
        cptr = strtok (NULL, "\n\r");
    }
    fclose (fp);
    get_code (cptr, ccode);
    strcpy (country, cptr);

    dirp = opendir ("/usr/share/i18n/locales");
    sprintf (lastcode, "");
    country_count = char_count = 0;
    int count = 0;
    do
    {
        dp = readdir (dirp);
        if (dp)
        {
            get_code (dp->d_name, code);
            if (*code && strcmp (lastcode, code))
            {
                sprintf (buffer, "/usr/share/i18n/locales/%s", dp->d_name);
                get_quoted_param (buffer, "language", text);
                sprintf (buffer, "%s (%s)", code, text);

	            gtk_combo_box_append_text (GTK_COMBO_BOX (loclang_cb), buffer);
	            strcpy (lastcode, code);

	            // highlight the current language setting...
	            if (!strcmp (ccode, code)) gtk_combo_box_set_active (GTK_COMBO_BOX (loclang_cb), count);
	            count++;
	        }
	    }
    } while (dp);
	g_signal_connect (loclang_cb, "changed", G_CALLBACK (on_language_changed), NULL);

	// populate the location list and set the current location
	strcpy (buffer, country);  // local copy needed as o_l_c calls strtok and trashes it....
	on_language_changed (GTK_COMBO_BOX (loclang_cb), buffer);
	on_country_changed (GTK_COMBO_BOX (loclang_cb), country);

	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    //if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
        //    sprintf (buffer, "echo '%s/%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
        //        gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
        //else
        //    sprintf (buffer, "echo '%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));

        //system (buffer);
        //system ("sudo dpkg-reconfigure --frontend noninteractive tzdata");
	}
	gtk_widget_destroy (dlg);

//    system ("lxterminal -e sudo dpkg-reconfigure locales");
}

static void on_area_changed (GtkComboBox *cb, gpointer ptr)
{
	char buffer[128];
    DIR *dirp, *sdirp;
    struct dirent *dp, *sdp;
    struct stat st_buf;

    while (loc_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (tzloc_cb), 0);
    loc_count = 0;

    sprintf (buffer, "/usr/share/zoneinfo/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));
    stat (buffer, &st_buf);

    if (S_ISDIR (st_buf.st_mode))
    {
        dirp = opendir (buffer);
        do
        {
            dp = readdir (dirp);
            if (dp && dp->d_name[0] != '.')
            {
                if (dp->d_type == DT_DIR)
                {
                    sprintf (buffer, "/usr/share/zoneinfo/%s/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)), dp->d_name);
                    sdirp = opendir (buffer);
                    do
                    {
                        sdp = readdir (sdirp);
                        if (sdp && sdp->d_name[0] != '.')
                        {
                            sprintf (buffer, "%s/%s", dp->d_name, sdp->d_name);
	                        gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), buffer);
	                        if (ptr && !strcmp (ptr, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
	                        loc_count++;
                        }
                    } while (sdp);
                }
                else
                {
	                gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), dp->d_name);
	                if (ptr && !strcmp (ptr, dp->d_name)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
	                loc_count++;
	            }
	        }
        } while (dp);
        if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), 0);
    }
}

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
	GtkBuilder *builder;
	GtkWidget *dlg;
	char buffer[128], *cptr;
    DIR *dirp;
    struct dirent *dp;

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);
	dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdialog");

	GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "tztable");
	tzarea_cb = (GObject *) gtk_combo_box_new_text ();
	tzloc_cb = (GObject *) gtk_combo_box_new_text ();
	gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzarea_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
	gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzloc_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
	gtk_widget_show_all (GTK_WIDGET (tzarea_cb));
	gtk_widget_show_all (GTK_WIDGET (tzloc_cb));

	// select the current timezone area
	get_string (GET_TIMEZONE, buffer);
	strtok (buffer, "/");
	cptr = strtok (NULL, "");

    // populate the area combo box from the timezone database
    dirp = opendir ("/usr/share/zoneinfo");
    loc_count = 0;
    int count = 0;
    do
    {
        dp = readdir (dirp);
        if (dp && dp->d_name[0] >= 'A' && dp->d_name[0] <= 'Z')
        {
	        gtk_combo_box_append_text (GTK_COMBO_BOX (tzarea_cb), dp->d_name);
	        if (!strcmp (dp->d_name, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzarea_cb), count);
	        count++;
	    }
    } while (dp);
	g_signal_connect (tzarea_cb, "changed", G_CALLBACK (on_area_changed), NULL);

	// populate the location list and set the current location
	on_area_changed (GTK_COMBO_BOX (tzarea_cb), cptr);

	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
            sprintf (buffer, "echo '%s/%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
                gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
        else
            sprintf (buffer, "echo '%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));

        system (buffer);
        system ("sudo dpkg-reconfigure --frontend noninteractive tzdata");
	}
	gtk_widget_destroy (dlg);
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    system ("python -S /usr/local/bin/lxkeymap");
}

static void on_rt_change (GtkEntry *entry, gpointer ptr)
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
	rtname_tb = gtk_builder_get_object (builder, "rtentry1");
	rtemail_tb = gtk_builder_get_object (builder, "rtentry2");
	g_signal_connect (rtname_tb, "changed", G_CALLBACK (on_rt_change), NULL);
	g_signal_connect (rtemail_tb, "changed", G_CALLBACK (on_rt_change), NULL);
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

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    char buffer[128];
    int reboot = 0;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)))
    {
	    if (orig_boot) system (SET_BOOT_CLI);
	    else system (SET_BOOT_GUI);
	    reboot = 1;
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

    if (orig_spi != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi_on_rb)))
    {
	    sprintf (buffer, SET_SPI, orig_spi);
	    system (buffer);
	    reboot = 1;
    }

    if (orig_i2c != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c_on_rb)))
    {
	    sprintf (buffer, SET_I2C, orig_i2c);
	    system (buffer);
	    reboot = 1;
    }

    if (orig_serial != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_on_rb)))
    {
	    sprintf (buffer, SET_SERIAL, orig_serial);
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
        if (is_pi2 ())
        {
            switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
            {
                case 0 :    sprintf (buffer, SET_OVERCLOCK, "Pi2None");
                            break;
                case 1 :    sprintf (buffer, SET_OVERCLOCK, "Pi2");
                            break;
            }
        }
        else
        {
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
        }
        system (buffer);
	    reboot = 1;
	}

	return reboot;
}


/* The dialog... */

int main (int argc, char *argv[])
{
	GtkBuilder *builder;
	GObject *item;
	GtkWidget *dlg;
	
	// GTK setup
	gtk_init (&argc, &argv);
	gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

	// build the UI
	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/rc_gui.ui", NULL);

	if (!get_status (GET_CAN_CONF))
	{
	    dlg = (GtkWidget *) gtk_builder_get_object (builder, "errordialog");
	    g_object_unref (builder);
	    gtk_dialog_run (GTK_DIALOG (dlg));
	    gtk_widget_destroy (dlg);
	    return 0;
	}

	dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
	
	expandfs_btn = gtk_builder_get_object (builder, "button3");
	g_signal_connect (expandfs_btn, "clicked", G_CALLBACK (on_expand_fs), NULL);
	if (get_status (CAN_EXPAND)) gtk_widget_set_sensitive (GTK_WIDGET(expandfs_btn), TRUE);
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
		
	boot_desktop_rb = gtk_builder_get_object (builder, "radiobutton1");
	boot_cli_rb = gtk_builder_get_object (builder, "radiobutton2");
	if (orig_boot = get_status (GET_BOOT_GUI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);

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
	if (orig_spi = get_status (GET_SPI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_off_rb), TRUE);
	
	i2c_on_rb = gtk_builder_get_object (builder, "radiobutton13");
	i2c_off_rb = gtk_builder_get_object (builder, "radiobutton14");
	if (orig_i2c = get_status (GET_I2C)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_off_rb), TRUE);
	
	serial_on_rb = gtk_builder_get_object (builder, "radiobutton15");
	serial_off_rb = gtk_builder_get_object (builder, "radiobutton16");
	if (orig_serial = get_status (GET_SERIAL)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
	
	if (is_pi2 ())
	{
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
	}
	else
	{
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
	}

	GtkObject *adj = gtk_adjustment_new (64.0, 16.0, get_total_mem () - 128, 16.0, 64.0, 0);
	memsplit_sb = gtk_builder_get_object (builder, "spinbutton1");
	gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (memsplit_sb), GTK_ADJUSTMENT (adj));
	orig_gpumem = get_status (GET_GPU_MEM);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (memsplit_sb), orig_gpumem);

	hostname_tb = gtk_builder_get_object (builder, "entry1");
	get_string (GET_HOSTNAME, orig_hostname);
	gtk_entry_set_text (GTK_ENTRY (hostname_tb), orig_hostname);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    if (process_changes ())
	    {
	        dlg = (GtkWidget *) gtk_builder_get_object (builder, "rebootdlg");
	        g_object_unref (builder);
	        if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES)
	        {
                system ("sudo reboot");
            }
	    }
	}

	g_object_unref (builder);
	gtk_widget_destroy (dlg);

	return 0;
}
