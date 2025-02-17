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
#include <crypt.h>

#include "rc_gui.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define GET_HOSTNAME    GET_PREFIX "get_hostname"
#define SET_HOSTNAME    SET_PREFIX "do_hostname %s"
#define GET_BOOT_CLI    GET_PREFIX "get_boot_cli"
#define GET_AUTOLOGIN   GET_PREFIX "get_autologin"
#define SET_BOOT_CLI    SET_PREFIX "do_boot_behaviour B1"
#define SET_BOOT_CLIA   SET_PREFIX "do_boot_behaviour B2"
#define SET_BOOT_GUI    SET_PREFIX "do_boot_behaviour B3"
#define SET_BOOT_GUIA   SET_PREFIX "do_boot_behaviour B4"
#define GET_SPLASH      GET_PREFIX "get_boot_splash"
#define SET_SPLASH      SET_PREFIX "do_boot_splash %d"
#define GET_LEDS        GET_PREFIX "get_leds"
#define SET_LEDS        SET_PREFIX "do_leds %d"
#define GET_BROWSER     GET_PREFIX "get_browser"
#define SET_BROWSER     SET_PREFIX "do_browser %s"
#define FF_INSTALLED    GET_PREFIX "is_installed firefox"
#define CR_INSTALLED    GET_PREFIX "is_installed chromium"

#define CHANGE_PASSWD   "echo $USER:'%s' | SUDO_ASKPASS=/usr/lib/rc-gui/pwdrcg.sh sudo -A chpasswd -e"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GObject *passwd_btn, *hostname_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *chromium_rb, *firefox_rb;
static GObject *alogin_sw, *splash_sw, *led_actpwr_sw;
static GObject *pwentry1_tb, *pwentry2_tb, *pwok_btn;
static GObject *hostname_tb;

static int orig_boot, orig_autolog, orig_splash, orig_leds;
static char *orig_browser;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void on_change_passwd (GtkButton* btn, gpointer ptr);
static void passwd_update (GtkEntry *entry, gpointer ptr);
static void on_change_hostname (GtkButton* btn, gpointer ptr);
static void config_boot (void);
static void boot_update (void);
static void on_boot_toggle (GtkButton *btn, gpointer ptr);
#ifdef PLUGIN_NAME
static gboolean on_alogin_toggle (GtkSwitch *btn, gboolean state, gpointer ptr);
static void on_browser_toggle (GtkButton *btn, gpointer ptr);
static gboolean on_leds_toggle (GtkSwitch *btn, gboolean state, gpointer ptr);
#endif

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Password dialog                                                            */
/*----------------------------------------------------------------------------*/

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "passwddlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    pwentry1_tb = gtk_builder_get_object (builder, "pwentry1");
    pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
    gtk_entry_set_visibility (GTK_ENTRY (pwentry1_tb), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwentry2_tb), FALSE);
    g_signal_connect (pwentry1_tb, "changed", G_CALLBACK (passwd_update), NULL);
    g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (passwd_update), NULL);
    pwok_btn = gtk_builder_get_object (builder, "passwdok");
    gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        res = vsystem (CHANGE_PASSWD, crypt (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb)), crypt_gensalt (NULL, 0, NULL, 0)));
        gtk_widget_destroy (dlg);
        if (res)
            info (_("The password change failed.\n\nThis could be because the current password was incorrect, or because the new password was not sufficiently complex or was too similar to the current password."));
        else
            info (_("The password has been changed successfully."));
    }
    else gtk_widget_destroy (dlg);
    g_object_unref (builder);
}

static void passwd_update (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb))) && g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb)), 
        gtk_entry_get_text (GTK_ENTRY(pwentry2_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

/*----------------------------------------------------------------------------*/
/* Hostname dialog                                                            */
/*----------------------------------------------------------------------------*/

static void on_change_hostname (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;
    const char *new_hn, *cptr;
    char *orig_hn;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "hostnamedlg");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    hostname_tb = gtk_builder_get_object (builder, "hnentry1");
    orig_hn = get_string (GET_HOSTNAME);
    gtk_entry_set_text (GTK_ENTRY (hostname_tb), orig_hn);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        new_hn = gtk_entry_get_text (GTK_ENTRY (hostname_tb));
        if (g_strcmp0 (orig_hn, new_hn))
        {
            res = 1;
            cptr = new_hn;
            if (*cptr == 0 || *cptr == '-') res = 0;
            else while (*cptr)
            {
                if (!(*cptr >= '0' && *cptr <= '9') && !(*cptr >= 'a' && *cptr <= 'z') && !(*cptr >= 'A' && *cptr <= 'Z') && *cptr != '-')
                {
                    res = 0;
                    break;
                }
                cptr++;
            }
            if (res && *(cptr - 1) == '-') res = 0;

            if (res)
            {
                vsystem (SET_HOSTNAME, new_hn);
                gtk_widget_destroy (dlg);
                info (_("The hostname has been changed successfully and will take effect on the next reboot."));
                needs_reboot = TRUE;
            }
            else
            {
                gtk_widget_destroy (dlg);
                info (_("The hostname change failed.\n\nThe hostname must only contain the characters A-Z, a-z, 0-9 and hyphen.\nThe first and last character may not be the hyphen."));
            }
        }
        else gtk_widget_destroy (dlg);
    }
    else gtk_widget_destroy (dlg);
    g_free (orig_hn);
    g_object_unref (builder);
}

/*----------------------------------------------------------------------------*/
/* Control handling                                                           */
/*----------------------------------------------------------------------------*/

static void config_boot (void)
{
    if (gtk_switch_get_active (GTK_SWITCH (alogin_sw)))
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

static void boot_update (void)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_cli_rb)))
    {
        gtk_switch_set_active (GTK_SWITCH (splash_sw), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), FALSE);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), TRUE);
    }
}

static void on_boot_toggle (GtkButton *btn, gpointer ptr)
{
    boot_update ();
    
#ifdef PLUGIN_NAME
    config_boot ();
#endif
}

/*----------------------------------------------------------------------------*/
/* Real-time handlers                                                         */
/*----------------------------------------------------------------------------*/

#ifdef PLUGIN_NAME

static gboolean on_alogin_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    config_boot ();

    return FALSE;
}

static void on_browser_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb)))
        vsystem (SET_BROWSER, "chromium");
    else
        vsystem (SET_BROWSER, "firefox");
}

static gboolean on_leds_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    vsystem (SET_LEDS, (1 - state));

    return FALSE;
}

#endif

/*----------------------------------------------------------------------------*/
/* Exit processing                                                            */
/*----------------------------------------------------------------------------*/

gboolean read_system_tab (void)
{
    gboolean reboot = FALSE;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
        || orig_autolog == gtk_switch_get_active (GTK_SWITCH (alogin_sw)))
    {
        config_boot ();
    }

    READ_SWITCH (splash_sw, orig_splash, SET_SPLASH, FALSE);

    if (strcmp (orig_browser, "chromium") && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb)))
        vsystem (SET_BROWSER, "chromium");
    if (strcmp (orig_browser, "firefox") && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (firefox_rb)))
        vsystem (SET_BROWSER, "firefox");

    if (!vsystem (IS_PI))
    {
        if (orig_leds != -1) READ_SWITCH (led_actpwr_sw, orig_leds, SET_LEDS, FALSE);
    }

    return reboot;
}

/*----------------------------------------------------------------------------*/
/* Reboot check                                                               */
/*----------------------------------------------------------------------------*/

gboolean system_reboot (void)
{
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Tab setup                                                                  */
/*----------------------------------------------------------------------------*/

void load_system_tab (GtkBuilder *builder)
{
    /* Change password button */
    passwd_btn = gtk_builder_get_object (builder, "button_pw");
    g_signal_connect (passwd_btn, "clicked", G_CALLBACK (on_change_passwd), NULL);

    /* Change hostname button */
    hostname_btn = gtk_builder_get_object (builder, "button_hn");
    g_signal_connect (hostname_btn, "clicked", G_CALLBACK (on_change_hostname), NULL);

    /* Splash screen switch */
    CONFIG_SWITCH (splash_sw, "sw_splash", orig_splash, GET_SPLASH);
    HANDLE_SWITCH (splash_sw, SET_SPLASH);
    if (!vsystem (IS_PI))
    {
        if (!get_status ("grep -q boot=live /proc/cmdline ; echo $?"))
        {
            gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (splash_sw), _("Splash screen cannot be configured on a live image"));
        }
    }

    /* Autologin switch */
    CONFIG_SWITCH (alogin_sw, "sw_alogin", orig_autolog, GET_AUTOLOGIN);
    HANDLE_CONTROL (alogin_sw, "state-set", on_alogin_toggle);

    /* CLI / desktop radio buttons */
    boot_desktop_rb = gtk_builder_get_object (builder, "rb_desktop");
    boot_cli_rb = gtk_builder_get_object (builder, "rb_cli");
    if ((orig_boot = get_status (GET_BOOT_CLI)))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
    else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);
    g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_boot_toggle), NULL);
    boot_update ();

    /* Browser radio buttons */
    chromium_rb = gtk_builder_get_object (builder, "rb_chromium");
    firefox_rb = gtk_builder_get_object (builder, "rb_firefox");
    orig_browser = get_string (GET_BROWSER);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (firefox_rb), !strcmp (orig_browser, "firefox"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chromium_rb), !strcmp (orig_browser, "chromium"));
    if (vsystem (CR_INSTALLED) || vsystem (FF_INSTALLED))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (chromium_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (firefox_rb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (chromium_rb), _("Multiple browsers are not installed"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (firefox_rb), _("Multiple browsers are not installed"));
    }
    HANDLE_CONTROL (chromium_rb, "toggled", on_browser_toggle);

    /* Power LED switch */
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox17")));
    if (!vsystem (IS_PI))
    {
        led_actpwr_sw = gtk_builder_get_object (builder, "sw_led_actpwr");
        orig_leds = get_status (GET_LEDS);
        if (orig_leds != -1)
        {
            gtk_switch_set_active (GTK_SWITCH (led_actpwr_sw), !(orig_leds));
            gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (builder, "hbox17")));
            HANDLE_CONTROL (led_actpwr_sw, "state-set", on_leds_toggle);
        }
    }
}

/* End of file */
/*----------------------------------------------------------------------------*/
