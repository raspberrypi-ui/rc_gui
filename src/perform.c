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

#define GET_OVERCLOCK   GET_PREFIX "get_config_var arm_freq /boot/config.txt"
#define SET_OVERCLOCK   SET_PREFIX "do_overclock %s"
#define GET_FAN         GET_PREFIX "get_fan"
#define GET_FAN_GPIO    GET_PREFIX "get_fan_gpio"
#define GET_FAN_TEMP    GET_PREFIX "get_fan_temp"
#define SET_FAN         SET_PREFIX "do_fan %d %d %d"
#define GET_OVERLAYNOW  GET_PREFIX "get_overlay_now"
#define GET_OVERLAY     GET_PREFIX "get_overlay_conf"
#define GET_BOOTRO      GET_PREFIX "get_bootro_conf"
#define SET_OFS_ON      SET_PREFIX "enable_overlayfs"
#define SET_OFS_OFF     SET_PREFIX "disable_overlayfs"
#define SET_BOOTP_RO    SET_PREFIX "enable_bootro"
#define SET_BOOTP_RW    SET_PREFIX "disable_bootro"
#define GET_USBI        GET_PREFIX "get_usb_current"
#define SET_USBI        SET_PREFIX "do_usb_current %d"
#define CHECK_UNAME     GET_PREFIX "is_uname_current"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GObject *overclock_cb, *fan_sw, *fan_gpio_sb, *fan_temp_sb, *usb_sw, *ofs_btn, *ofs_en_sw, *bp_ro_sw, *ofs_lbl;
static int orig_clock, orig_fan, orig_fan_gpio, orig_fan_temp, orig_usbi, orig_ofs, orig_bpro;
static int ovfs_rb;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void on_set_ofs (GtkButton* btn, gpointer ptr);
static gboolean overlay_update (GtkSwitch *btn, gpointer, gpointer);
static gpointer initrd_thread (gpointer data);
static gboolean close_msg (gpointer data);
static void overclock_config (void);
static void fan_config (void);
static void fan_update (void);
static gboolean on_fan_toggle (GtkSwitch *btn, gpointer, gpointer);
#ifdef REALTIME
static void on_overclock_set (GtkComboBox* cb, gpointer ptr);
static gboolean process_oc (gpointer data);
static void on_fan_value_changed (GtkSpinButton *sb);
static gboolean process_fan (gpointer data);
#endif

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Overlay file system dialog                                                 */
/*----------------------------------------------------------------------------*/

static void on_set_ofs (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");

    if (vsystem (CHECK_UNAME))
    {
        info (_("Your system has recently been updated. Please reboot to ensure these updates have loaded before setting up the overlay file system."));
        return;
    }

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "ofsdlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    CONFIG_SWITCH (ofs_en_sw, "sw_ofsen", orig_ofs, GET_OVERLAY);
    CONFIG_SWITCH (bp_ro_sw, "sw_bpro", orig_bpro, GET_BOOTRO);

    ofs_lbl = gtk_builder_get_object (builder, "ofslabel3");

    g_object_unref (builder);

    if (get_status (GET_OVERLAYNOW))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (bp_ro_sw), TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (bp_ro_sw), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (bp_ro_sw), _("The state of the boot partition cannot be changed while an overlay is active"));
    }

    g_signal_connect (ofs_en_sw, "notify::active", G_CALLBACK (overlay_update), NULL);
    g_signal_connect (bp_ro_sw, "notify::active", G_CALLBACK (overlay_update), NULL);
    gtk_widget_realize (GTK_WIDGET (ofs_lbl));
    gtk_widget_set_size_request (dlg, gtk_widget_get_allocated_width (dlg), gtk_widget_get_allocated_height (dlg));
    gtk_widget_hide (GTK_WIDGET (ofs_lbl));

    ovfs_rb = 0;
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (ovfs_rb) needs_reboot = TRUE;
        if (orig_bpro && gtk_switch_get_active (GTK_SWITCH (bp_ro_sw)))
            vsystem (SET_BOOTP_RO);
        if (!orig_bpro && !gtk_switch_get_active (GTK_SWITCH (bp_ro_sw)))
            vsystem (SET_BOOTP_RW);
        if (!orig_ofs && !gtk_switch_get_active (GTK_SWITCH (ofs_en_sw)))
            vsystem (SET_OFS_OFF);
        if (orig_ofs && gtk_switch_get_active (GTK_SWITCH (ofs_en_sw)))
        {
            // warn about a short delay...
            message (_("Setting up overlay - please wait..."));

            // launch a thread with the system call to update the initrd
            g_thread_new (NULL, initrd_thread, NULL);
        }
    }
    gtk_widget_destroy (dlg);
}

static gboolean overlay_update (GtkSwitch *btn, gpointer, gpointer)
{
    ovfs_rb = 0;
    if (orig_ofs == gtk_switch_get_active (GTK_SWITCH (ofs_en_sw))) ovfs_rb = 1;
    if (orig_bpro == gtk_switch_get_active (GTK_SWITCH (bp_ro_sw))) ovfs_rb = 1;
    gtk_widget_set_visible (GTK_WIDGET (ofs_lbl), ovfs_rb);
    return FALSE;
}

static gpointer initrd_thread (gpointer data)
{
    vsystem (SET_OFS_ON);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Control handling                                                           */
/*----------------------------------------------------------------------------*/

static void overclock_config (void)
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
            break;

        case 2:
            switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
            {
                case 0 :    vsystem (SET_OVERCLOCK, "None");
                            break;
                case 1 :    vsystem (SET_OVERCLOCK, "High");
                            break;
            }
            break;
    }
}

static void fan_config (void)
{
#ifdef REALTIME
    set_watch_cursor ();
    g_idle_add (process_fan, NULL);
}

static gboolean process_fan (gpointer data)
{
#endif
    int fan_gpio = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (fan_gpio_sb));
    int fan_temp = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (fan_temp_sb));
    if (!gtk_switch_get_active (GTK_SWITCH (fan_sw)))
    {
        vsystem (SET_FAN, 1, 0, 0);
    }
    else
    {
        vsystem (SET_FAN, 0, fan_gpio, fan_temp);
    }
#ifdef REALTIME
    clear_watch_cursor ();
    return FALSE;
#endif
}

static void fan_update (void)
{
    if (gtk_switch_get_active (GTK_SWITCH (fan_sw)))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (fan_gpio_sb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (fan_temp_sb), TRUE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_gpio_sb), _("Set the GPIO to which the fan is connected"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_temp_sb), _("Set the temperature in degrees C at which the fan turns on"));
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (fan_gpio_sb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (fan_temp_sb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_gpio_sb), _("This setting cannot be changed unless the fan is enabled"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_temp_sb), _("This setting cannot be changed unless the fan is enabled"));
    }
}

static gboolean on_fan_toggle (GtkSwitch *btn, gpointer, gpointer)
{
    fan_update ();

#ifdef REALTIME
    fan_config ();
#endif

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Real-time handlers                                                         */
/*----------------------------------------------------------------------------*/

#ifdef REALTIME

static void on_overclock_set (GtkComboBox* cb, gpointer ptr)
{
    set_watch_cursor ();
    g_idle_add (process_oc, NULL);
}

static gboolean process_oc (gpointer data)
{
    overclock_config ();
    clear_watch_cursor ();
    return FALSE;
}

static void on_fan_value_changed (GtkSpinButton *sb)
{
    fan_config ();
}

#endif

/*----------------------------------------------------------------------------*/
/* Exit processing                                                            */
/*----------------------------------------------------------------------------*/

gboolean read_performance_tab (void)
{
    gboolean reboot = FALSE;

    if (!vsystem (IS_PI))
    {
        READ_SWITCH (usb_sw, orig_usbi, SET_USBI, TRUE);

        if (orig_clock != -1 && orig_clock != gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
        {
            overclock_config ();
            reboot = TRUE;
        }

        if (!vsystem (IS_PI4)) fan_config ();
    }

    return reboot;
}

/*----------------------------------------------------------------------------*/
/* Reboot check                                                               */
/*----------------------------------------------------------------------------*/

gboolean performance_reboot (void)
{
    if (!vsystem (IS_PI))
    {
        CHECK_SWITCH (usb_sw, orig_usbi);

        if (orig_clock != -1 && orig_clock != gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb))) return TRUE;
    }
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Tab setup                                                                  */
/*----------------------------------------------------------------------------*/

void load_performance_tab (GtkBuilder *builder)
{
    GtkAdjustment *gadj, *tadj;

    if (!vsystem (IS_PI))
    {
        /* USB current limit switch */
        CONFIG_SWITCH (usb_sw, "sw_usb", orig_usbi, GET_USBI);
        HANDLE_SWITCH (usb_sw, SET_USBI);
        if (vsystem (IS_PI5))
        {
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox37")));
        }

        /* Fan controls */
        if (!vsystem (IS_PI4))
        {
            CONFIG_SWITCH (fan_sw, "sw_fan", orig_fan, GET_FAN);
            fan_gpio_sb = gtk_builder_get_object (builder, "sb_fan_gpio");
            fan_temp_sb = gtk_builder_get_object (builder, "sb_fan_temp");
            fan_update ();
            g_signal_connect (fan_sw, "notify::active", G_CALLBACK (on_fan_toggle), NULL);

            gadj = gtk_adjustment_new (14, 2, 27, 1, 1, 0);
            gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (fan_gpio_sb), GTK_ADJUSTMENT (gadj));
            orig_fan_gpio = get_status (GET_FAN_GPIO);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (fan_gpio_sb), orig_fan_gpio);
            HANDLE_CONTROL (fan_gpio_sb, "value_changed", on_fan_value_changed);

            tadj = gtk_adjustment_new (80, 60, 120, 5, 10, 0);
            gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (fan_temp_sb), GTK_ADJUSTMENT (tadj));
            orig_fan_temp = get_status (GET_FAN_TEMP);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (fan_temp_sb), orig_fan_temp);
            HANDLE_CONTROL (fan_temp_sb, "value_changed", on_fan_value_changed);
        }
        else
        {
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox34")));
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox35")));
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox36")));
        }

        /* Overclock controls */
        overclock_cb = gtk_builder_get_object (builder, "combo_oc");
        switch (get_status (GET_PI_TYPE))
        {
            case 1 :    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("None (700MHz)"));
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("Modest (800MHz)"));
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("Medium (900MHz)"));
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("High (950MHz)"));
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("Turbo (1000MHz)"));

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
                        break;

            case 2 :    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("None (900MHz)"));
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (overclock_cb), _("High (1000MHz)"));

                        switch (get_status (GET_OVERCLOCK))
                        {
                            case 1000 : orig_clock = 1;
                                        break;
                            default   : orig_clock = 0;
                                        break;
                        }
                        break;

            default :   orig_clock = -1;
                        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox31")));
                        break;
        }
        if (orig_clock != -1)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (overclock_cb), orig_clock);
            HANDLE_CONTROL (overclock_cb, "changed", on_overclock_set);
        }

        /* Overlay file system button */
        ofs_btn = gtk_builder_get_object (builder, "button_ofs");
        g_signal_connect (ofs_btn, "clicked", G_CALLBACK (on_set_ofs), NULL);
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "vbox30")));
    }
}

/* End of file */
/*----------------------------------------------------------------------------*/
