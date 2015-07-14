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

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *locale_btn, *timezone_btn, *keyboard_btn, *rastrack_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *camera_on_rb, *camera_off_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb, *devtree_on_rb, *devtree_off_rb;
static GObject *spi_on_rb, *spi_off_rb, *i2c_on_rb, *i2c_off_rb, *serial_on_rb, *serial_off_rb;
static GObject *audio_auto_rb, *audio_hdmi_rb, *audio_ana_rb;
static GObject *overclock_cb, *memsplit_sb;


/* Dialog box "changed" signal handlers */

static void on_overclock_set (GtkComboBox* box, gpointer ptr)
{
	gint val = gtk_combo_box_get_active (box);
	switch (val)
	{
		case 0 : break;
		case 1 : break;
	}
}

static void on_audio_set (GtkRadioButton* btn, gpointer ptr)
{
	// only respond to the button which is now active
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

	// find out which button in the group is active
	GSList *group = gtk_radio_button_get_group (btn);
    GtkRadioButton *tbtn;
    int nbtn = 0;
    while (group)
    {
    	tbtn = group->data;
    	group = group->next;
    	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (tbtn))) break;
    	nbtn++;
    }
	switch (nbtn)
	{
		case 0: break;
		case 1: break;
		case 2: break;
	}
}

static void on_boot_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_camera_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_overscan_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_ssh_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_devtree_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_spi_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_i2c_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_serial_set (GtkRadioButton* btn, gpointer ptr)
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

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
}

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
}

static void on_set_rastrack (GtkButton* btn, gpointer ptr)
{
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
	dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
	
	expandfs_btn = gtk_builder_get_object (builder, "button3");
	g_signal_connect (expandfs_btn, "clicked", G_CALLBACK (on_expand_fs), NULL);
	
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
	g_signal_connect (boot_desktop_rb, "toggled", G_CALLBACK (on_boot_set), NULL);
	boot_cli_rb = gtk_builder_get_object (builder, "radiobutton2");
	g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_boot_set), NULL);

	camera_on_rb = gtk_builder_get_object (builder, "radiobutton3");
	g_signal_connect (camera_on_rb, "toggled", G_CALLBACK (on_camera_set), NULL);
	camera_off_rb = gtk_builder_get_object (builder, "radiobutton4");
	g_signal_connect (camera_off_rb, "toggled", G_CALLBACK (on_camera_set), NULL);
	
	overscan_on_rb = gtk_builder_get_object (builder, "radiobutton5");
	g_signal_connect (overscan_on_rb, "toggled", G_CALLBACK (on_overscan_set), NULL);
	overscan_off_rb = gtk_builder_get_object (builder, "radiobutton6");
	g_signal_connect (overscan_off_rb, "toggled", G_CALLBACK (on_overscan_set), NULL);
	
	ssh_on_rb = gtk_builder_get_object (builder, "radiobutton7");
	g_signal_connect (ssh_on_rb, "toggled", G_CALLBACK (on_ssh_set), NULL);
	ssh_off_rb = gtk_builder_get_object (builder, "radiobutton8");
	g_signal_connect (ssh_off_rb, "toggled", G_CALLBACK (on_ssh_set), NULL);
	
	devtree_on_rb = gtk_builder_get_object (builder, "radiobutton9");
	g_signal_connect (devtree_on_rb, "toggled", G_CALLBACK (on_devtree_set), NULL);
	devtree_off_rb = gtk_builder_get_object (builder, "radiobutton10");
	g_signal_connect (devtree_off_rb, "toggled", G_CALLBACK (on_devtree_set), NULL);
	
	spi_on_rb = gtk_builder_get_object (builder, "radiobutton11");
	g_signal_connect (spi_on_rb, "toggled", G_CALLBACK (on_spi_set), NULL);
	spi_off_rb = gtk_builder_get_object (builder, "radiobutton12");
	g_signal_connect (spi_off_rb, "toggled", G_CALLBACK (on_spi_set), NULL);
	
	i2c_on_rb = gtk_builder_get_object (builder, "radiobutton13");
	g_signal_connect (i2c_on_rb, "toggled", G_CALLBACK (on_i2c_set), NULL);
	i2c_off_rb = gtk_builder_get_object (builder, "radiobutton14");
	g_signal_connect (i2c_off_rb, "toggled", G_CALLBACK (on_i2c_set), NULL);
	
	serial_on_rb = gtk_builder_get_object (builder, "radiobutton15");
	g_signal_connect (serial_on_rb, "toggled", G_CALLBACK (on_serial_set), NULL);
	serial_off_rb = gtk_builder_get_object (builder, "radiobutton16");
	g_signal_connect (serial_off_rb, "toggled", G_CALLBACK (on_serial_set), NULL);
	
	audio_auto_rb = gtk_builder_get_object (builder, "radiobutton17");
	g_signal_connect (audio_auto_rb, "toggled", G_CALLBACK (on_audio_set), NULL);
	audio_ana_rb = gtk_builder_get_object (builder, "radiobutton18");
	g_signal_connect (audio_ana_rb, "toggled", G_CALLBACK (on_audio_set), NULL);
	audio_hdmi_rb = gtk_builder_get_object (builder, "radiobutton19");
	g_signal_connect (audio_hdmi_rb, "toggled", G_CALLBACK (on_audio_set), NULL);
	
	overclock_cb = gtk_builder_get_object (builder, "comboboxtext1");
	gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), 0);
	g_signal_connect (overclock_cb, "changed", G_CALLBACK (on_overclock_set), NULL);
	
	GtkObject *adj = gtk_adjustment_new (64.0, 16.0, 512.0, 16.0, 64.0, 0);  //!!!!SPL - maximum should be total RAM less 128MB; GPU min is 16MB
	memsplit_sb = gtk_builder_get_object (builder, "spinbutton1");
	gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (memsplit_sb), GTK_ADJUSTMENT (adj));
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (memsplit_sb), 64.0);
/*
	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

	font = gtk_builder_get_object (builder, "fontbutton1");
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (font), desktop_font);
	g_signal_connect (font, "font-set", G_CALLBACK (on_desktop_font_set), NULL);

	dpic = gtk_builder_get_object (builder, "filechooserbutton1");
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), desktop_picture);
	g_signal_connect (dpic, "file-set", G_CALLBACK (on_desktop_picture_set), NULL);
	if (!strcmp (desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
	else gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);

	hcol = gtk_builder_get_object (builder, "colorbutton1");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (hcol), &theme_colour);
	g_signal_connect (hcol, "color-set", G_CALLBACK (on_theme_colour_set), NULL);

	dcol = gtk_builder_get_object (builder, "colorbutton2");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (dcol), &desktop_colour);
	g_signal_connect (dcol, "color-set", G_CALLBACK (on_desktop_colour_set), NULL);

	bcol = gtk_builder_get_object (builder, "colorbutton3");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (bcol), &bar_colour);
	g_signal_connect (bcol, "color-set", G_CALLBACK (on_bar_colour_set), NULL);

	btcol = gtk_builder_get_object (builder, "colorbutton4");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (btcol), &bartext_colour);
	g_signal_connect (btcol, "color-set", G_CALLBACK (on_bartext_colour_set), NULL);

	htcol = gtk_builder_get_object (builder, "colorbutton5");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (htcol), &themetext_colour);
	g_signal_connect (htcol, "color-set", G_CALLBACK (on_themetext_colour_set), NULL);

	dtcol = gtk_builder_get_object (builder, "colorbutton6");
	gtk_color_button_set_color (GTK_COLOR_BUTTON (dtcol), &desktoptext_colour);
	g_signal_connect (dtcol, "color-set", G_CALLBACK (on_desktoptext_colour_set), NULL);

	dmod = gtk_builder_get_object (builder, "comboboxtext1");
	if (!strcmp (desktop_mode, "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 1);
	else if (!strcmp (desktop_mode, "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 2);
	else if (!strcmp (desktop_mode, "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
	else if (!strcmp (desktop_mode, "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 4);
	else if (!strcmp (desktop_mode, "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 5);
	else gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 0);
	g_signal_connect (dmod, "changed", G_CALLBACK (on_desktop_mode_set), gtk_builder_get_object (builder, "filechooserbutton1"));

	item = gtk_builder_get_object (builder, "button3");
	g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), gtk_builder_get_object (builder, "button3"));

	rb1 = gtk_builder_get_object (builder, "radiobutton1");
	g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
	rb2 = gtk_builder_get_object (builder, "radiobutton2");
	g_signal_connect (rb2, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
	if (barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb2), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);

	rb3 = gtk_builder_get_object (builder, "radiobutton3");
	g_signal_connect (rb3, "toggled", G_CALLBACK (on_menu_size_set), NULL);
	rb4 = gtk_builder_get_object (builder, "radiobutton4");
	g_signal_connect (rb4, "toggled", G_CALLBACK (on_menu_size_set), NULL);
	rb5 = gtk_builder_get_object (builder, "radiobutton5");
	g_signal_connect (rb5, "toggled", G_CALLBACK (on_menu_size_set), NULL);
	if (icon_size <= 20) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);
	else if (icon_size <= 28) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb4), TRUE);
	else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb3), TRUE);
*/
	g_object_unref (builder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_CANCEL)
	{
	}
	
	gtk_widget_destroy (dlg);

	return 0;
}
