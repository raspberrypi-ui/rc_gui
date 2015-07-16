#include <string.h>
#include <math.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

/* Command strings */
#define GET_HOSTNAME    "cat /etc/hostname | tr -d \" \t\n\r\""
#define IS_PI2          "cat /proc/cpuinfo | grep BCM2709"
#define GET_MEM_ARM     "vcgencmd get_mem arm"
#define GET_MEM_GPU     "vcgencmd get_mem gpu"
#define SET_RASTRACK    "curl --data \"name=%s&email=%s\" http://rastrack.co.uk/api.php"
#define CHANGE_PASSWD   "echo pi:%s | sudo chpasswd"
#define GET_SPI         "sudo raspi-config get_spi"
#define GET_I2C         "sudo raspi-config get_i2c"
#define GET_SERIAL      "sudo raspi-config get_serial"
#define GET_SSH         "sudo raspi-config get_ssh"
#define GET_BOOT_GUI    "sudo raspi-config get_boot_to_gui"
#define GET_OVERSCAN    "sudo raspi-config get_config_var disable_overscan /boot/config.txt"
#define GET_CAMERA      "sudo raspi-config get_config_var start_x /boot/config.txt"
#define GET_GPU_MEM     "sudo raspi-config get_config_var gpu_mem /boot/config.txt"
#define GET_OVERCLOCK   "sudo raspi-config get_config_var arm_freq /boot/config.txt"
#define GET_SDRAMF      "sudo raspi-config get_config_var sdram_freq /boot/config.txt"
#define CAN_EXPAND      "sudo raspi-config can_expand_rootfs"
#define EXPAND_FS       "sudo raspi-config do_expand_rootfs"
#define SET_HOSTNAME    "sudo raspi-config do_change_hostname %s"
#define SET_OVERCLOCK   "sudo raspi-config do_overclock %s"
#define SET_GPU_MEM     "sudo raspi-config do_memory_split %d"
#define GET_CAN_CONF    "sudo raspi-config get_can_configure"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *locale_btn, *timezone_btn, *keyboard_btn, *rastrack_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb;
static GObject *overclock_cb, *memsplit_sb, *hostname_tb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;

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

static void get_hostname (char *name)
{
    FILE *fp = popen (GET_HOSTNAME, "r");
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

/* Dialog box "changed" signal handlers */

static void on_set_boot (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	{
	}
	else
	{
	}
}

static void on_set_camera (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_camera 0");
	else
	    system ("sudo raspi-config do_camera 1");
}

static void on_set_overscan (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_overscan 0");
	else
	    system ("sudo raspi-config do_overscan 1");
}

static void on_set_ssh (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_ssh 1");
	else
	    system ("sudo raspi-config do_ssh 0");
}

static void on_set_spi (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_spi 1");
	else
	    system ("sudo raspi-config do_spi 0");
}

static void on_set_i2c (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_i2c 1");
	else
	    system ("sudo raspi-config do_i2c 0");
}

static void on_set_serial (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
	if (gtk_toggle_button_get_active (group->data))
	    system ("sudo raspi-config do_serial 1");
	else
	    system ("sudo raspi-config do_serial 0");
}

static void on_set_memsplit (GtkRadioButton* btn, gpointer ptr)
{
    char buffer[128];
    sprintf (buffer, SET_GPU_MEM, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(memsplit_sb)));
    system (buffer);
}

static void on_set_hostname (GtkEntry *ent, gpointer ptr)
{
    char buffer[128];
    sprintf (buffer, SET_HOSTNAME, gtk_entry_get_text (GTK_ENTRY(hostname_tb)));
    system (buffer);
}

static void on_overclock_set (GtkComboBox* box, gpointer ptr)
{
    char buffer[128];
	if (is_pi2 ())
	{
	    switch (gtk_combo_box_get_active (box))
	    {
		    case 0 :    sprintf (buffer, SET_OVERCLOCK, "Pi2None");
		                break;
		    case 1 :    sprintf (buffer, SET_OVERCLOCK, "Pi2");
		                break;
		}
	}
	else
	{
	    switch (gtk_combo_box_get_active (box))
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
}

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
    system (EXPAND_FS);
}

static void on_set_passwd (GtkEntry *entry, gpointer ptr)
{
	if (strcmp (gtk_entry_get_text (GTK_ENTRY(pwentry1_tb)), gtk_entry_get_text (GTK_ENTRY(pwentry2_tb))))
	    gtk_widget_set_sensitive (GTK_WIDGET(pwok_btn), FALSE);
	else
	    gtk_widget_set_sensitive (GTK_WIDGET(pwok_btn), TRUE);
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
	gtk_widget_set_sensitive (GTK_WIDGET(pwok_btn), FALSE);
	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
	{
	    sprintf (buffer, CHANGE_PASSWD, gtk_entry_get_text (GTK_ENTRY(pwentry1_tb)));
	    system (buffer);
	}
	gtk_widget_destroy (dlg);
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
    system ("lxterminal -e sudo dpkg-reconfigure locales");
}

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
    system ("lxterminal -e sudo dpkg-reconfigure tzdata");
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    system ("lxterminal -e sudo dpkg-reconfigure keyboard-configuration");
}

static void on_rt_change (GtkEntry *entry, gpointer ptr)
{
	if (strlen (gtk_entry_get_text (GTK_ENTRY(rtname_tb))) && strlen (gtk_entry_get_text (GTK_ENTRY(rtemail_tb))))
	    gtk_widget_set_sensitive (GTK_WIDGET(rtok_btn), TRUE);
	else
	    gtk_widget_set_sensitive (GTK_WIDGET(rtok_btn), FALSE);
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
	    sprintf (buffer, SET_RASTRACK, gtk_entry_get_text (GTK_ENTRY(rtname_tb)), gtk_entry_get_text (GTK_ENTRY(rtemail_tb)));
	    system (buffer);
	}
	gtk_widget_destroy (dlg);
}

/* The dialog... */

int main (int argc, char *argv[])
{
	GtkBuilder *builder;
	GObject *item;
	GtkWidget *dlg;
	char hname[128];
	
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
	if (get_status (GET_BOOT_GUI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);
	g_signal_connect (boot_desktop_rb, "toggled", G_CALLBACK (on_set_boot), NULL);
	g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_set_boot), NULL);

	camera_on_rb = gtk_builder_get_object (builder, "radiobutton3");
	camera_off_rb = gtk_builder_get_object (builder, "radiobutton4");
	if (get_status (GET_CAMERA)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (camera_off_rb), TRUE);
	g_signal_connect (camera_on_rb, "toggled", G_CALLBACK (on_set_camera), NULL);
	g_signal_connect (camera_off_rb, "toggled", G_CALLBACK (on_set_camera), NULL);
	
	overscan_on_rb = gtk_builder_get_object (builder, "radiobutton5");
	overscan_off_rb = gtk_builder_get_object (builder, "radiobutton6");
	if (get_status (GET_OVERSCAN)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_off_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (overscan_on_rb), TRUE);
	g_signal_connect (overscan_on_rb, "toggled", G_CALLBACK (on_set_overscan), NULL);
	g_signal_connect (overscan_off_rb, "toggled", G_CALLBACK (on_set_overscan), NULL);
	
	ssh_on_rb = gtk_builder_get_object (builder, "radiobutton7");
	ssh_off_rb = gtk_builder_get_object (builder, "radiobutton8");
	if (get_status (GET_SSH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_off_rb), TRUE);
	g_signal_connect (ssh_on_rb, "toggled", G_CALLBACK (on_set_ssh), NULL);
	g_signal_connect (ssh_off_rb, "toggled", G_CALLBACK (on_set_ssh), NULL);
	
	spi_on_rb = gtk_builder_get_object (builder, "radiobutton11");
	spi_off_rb = gtk_builder_get_object (builder, "radiobutton12");
	if (get_status (GET_SPI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi_off_rb), TRUE);
	g_signal_connect (spi_on_rb, "toggled", G_CALLBACK (on_set_spi), NULL);
	g_signal_connect (spi_off_rb, "toggled", G_CALLBACK (on_set_spi), NULL);
	
	i2c_on_rb = gtk_builder_get_object (builder, "radiobutton13");
	i2c_off_rb = gtk_builder_get_object (builder, "radiobutton14");
	if (get_status (GET_I2C)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c_off_rb), TRUE);
	g_signal_connect (i2c_on_rb, "toggled", G_CALLBACK (on_set_i2c), NULL);
	g_signal_connect (i2c_off_rb, "toggled", G_CALLBACK (on_set_i2c), NULL);
	
	serial_on_rb = gtk_builder_get_object (builder, "radiobutton15");
	serial_off_rb = gtk_builder_get_object (builder, "radiobutton16");
	if (get_status (GET_SERIAL)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
	g_signal_connect (serial_on_rb, "toggled", G_CALLBACK (on_set_serial), NULL);
	g_signal_connect (serial_off_rb, "toggled", G_CALLBACK (on_set_serial), NULL);
	
	if (is_pi2 ())
	{
	    overclock_cb = gtk_builder_get_object (builder, "comboboxtext2");
	    switch (get_status (GET_OVERCLOCK))
	    {
	        case 1000 : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 1);
	                    break;
	        default   : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 0);
	                    break;
	    }
	    gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox7")));
	    gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox8")));
	}
	else
	{
	    overclock_cb = gtk_builder_get_object (builder, "comboboxtext1");
	    switch (get_status (GET_OVERCLOCK))
	    {
	        case 800  : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 1);
	                    break;
	        case 900  : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 2);
	                    break;
	        case 950  : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 3);
	                    break;
	        case 1000 : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 4);
	                    break;
	        default   : gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 0);
	                    break;
        }	
	    gtk_widget_hide_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox8")));
	    gtk_widget_show_all (GTK_WIDGET(gtk_builder_get_object (builder, "hbox7")));
	}	
	g_signal_connect (overclock_cb, "changed", G_CALLBACK (on_overclock_set), NULL);
	
	GtkObject *adj = gtk_adjustment_new (64.0, 16.0, get_total_mem () - 128, 16.0, 64.0, 0);
	memsplit_sb = gtk_builder_get_object (builder, "spinbutton1");
	gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (memsplit_sb), GTK_ADJUSTMENT (adj));
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (memsplit_sb), get_status (GET_GPU_MEM));
	g_signal_connect (memsplit_sb, "changed", G_CALLBACK (on_set_memsplit), NULL);

	hostname_tb = gtk_builder_get_object (builder, "entry1");
	get_hostname (hname);
	gtk_entry_set_text (GTK_ENTRY (hostname_tb), hname);
	g_signal_connect (hostname_tb, "changed", G_CALLBACK (on_set_hostname), NULL);

	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_CANCEL)
	{
	}
	
	gtk_widget_destroy (dlg);

	return 0;
}
