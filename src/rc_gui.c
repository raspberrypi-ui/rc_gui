/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
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
*/

/* NOTE raspi-config nonint functions obey sh return codes - 0 is in general success / yes / selected, 1 is failed / no / not selected */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <locale.h>
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
#include <crypt.h>

/* Command strings */
#define GET_PREFIX      "raspi-config nonint "
#define SET_PREFIX      "SUDO_ASKPASS=/usr/lib/rc-gui/pwdrcg.sh sudo -A " GET_PREFIX
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
#define GET_OVERSCAN    GET_PREFIX "get_overscan_kms 1"
#define GET_OVERSCAN2   GET_PREFIX "get_overscan_kms 2"
#define SET_OVERSCAN    SET_PREFIX "do_overscan_kms 1 %d"
#define SET_OVERSCAN2   SET_PREFIX "do_overscan_kms 2 %d"
#define GET_CAMERA      GET_PREFIX "get_camera"
#define SET_CAMERA      SET_PREFIX "do_camera %d"
#define GET_SSH         GET_PREFIX "get_ssh"
#define SET_SSH         SET_PREFIX "do_ssh %d"
#define GET_RPC         GET_PREFIX "get_rpi_connect"
#define SET_RPC         SET_PREFIX "do_rpi_connect %d"
#define GET_VNC         GET_PREFIX "get_vnc"
#define SET_VNC         SET_PREFIX "do_vnc %d"
#define GET_SPI         GET_PREFIX "get_spi"
#define SET_SPI         SET_PREFIX "do_spi %d"
#define GET_I2C         GET_PREFIX "get_i2c"
#define SET_I2C         SET_PREFIX "do_i2c %d"
#define GET_SERIALCON   GET_PREFIX "get_serial_cons"
#define GET_SERIALHW    GET_PREFIX "get_serial_hw"
#define SET_SERIALCON   SET_PREFIX "do_serial_cons %d"
#define SET_SERIALHW    SET_PREFIX "do_serial_hw %d"
#define GET_1WIRE       GET_PREFIX "get_onewire"
#define SET_1WIRE       SET_PREFIX "do_onewire %d"
#define GET_RGPIO       GET_PREFIX "get_rgpio"
#define SET_RGPIO       SET_PREFIX "do_rgpio %d"
#define GET_BLANK       GET_PREFIX "get_blanking"
#define SET_BLANK       SET_PREFIX "do_blanking %d"
#define GET_LEDS        GET_PREFIX "get_leds"
#define SET_LEDS        SET_PREFIX "do_leds %d"
#define GET_USBI        GET_PREFIX "get_usb_current"
#define SET_USBI        SET_PREFIX "do_usb_current %d"
#define GET_PI_TYPE     GET_PREFIX "get_pi_type"
#define IS_PI           GET_PREFIX "is_pi"
#define IS_PI4          GET_PREFIX "is_pifour"
#define IS_PI5          GET_PREFIX "is_pifive"
#define IS_64BIT        GET_PREFIX "is_64bit"
#define HAS_ANALOG      GET_PREFIX "has_analog"
#define GET_OVERCLOCK   GET_PREFIX "get_config_var arm_freq /boot/config.txt"
#define SET_OVERCLOCK   SET_PREFIX "do_overclock %s"
#define GET_FAN         GET_PREFIX "get_fan"
#define GET_FAN_GPIO    GET_PREFIX "get_fan_gpio"
#define GET_FAN_TEMP    GET_PREFIX "get_fan_temp"
#define SET_FAN         SET_PREFIX "do_fan %d %d %d"
#define GET_HDMI_GROUP  GET_PREFIX "get_config_var hdmi_group /boot/config.txt"
#define GET_HDMI_MODE   GET_PREFIX "get_config_var hdmi_mode /boot/config.txt"
#define SET_HDMI_GP_MOD SET_PREFIX "do_resolution %d %d"
#define GET_WIFI_CTRY   GET_PREFIX "get_wifi_country"
#define SET_WIFI_CTRY   SET_PREFIX "do_wifi_country %s"
#define SET_PI4_4KH     SET_PREFIX "do_pi4video V1"
#define SET_PI4_ATV     SET_PREFIX "do_pi4video V2"
#define SET_PI4_NONE    SET_PREFIX "do_pi4video V3"
#define GET_PI4_VID     GET_PREFIX "get_pi4video"
#define GET_OVERLAYNOW  GET_PREFIX "get_overlay_now"
#define GET_OVERLAY     GET_PREFIX "get_overlay_conf"
#define GET_BOOTRO      GET_PREFIX "get_bootro_conf"
#define SET_OFS_ON      SET_PREFIX "enable_overlayfs"
#define SET_OFS_OFF     SET_PREFIX "disable_overlayfs"
#define SET_BOOTP_RO    SET_PREFIX "enable_bootro"
#define SET_BOOTP_RW    SET_PREFIX "disable_bootro"
#define CHECK_UNAME     GET_PREFIX "is_uname_current"
#define WLAN_INTERFACES GET_PREFIX "list_wlan_interfaces"
#define RVNC_INSTALLED  GET_PREFIX "is_installed realvnc-vnc-server"
#define WVNC_INSTALLED  GET_PREFIX "is_installed wayvnc"
#define XSCR_INSTALLED  GET_PREFIX "is_installed xscreensaver"
#define GET_VNC_RES     GET_PREFIX "get_vnc_resolution"
#define SET_VNC_RES     SET_PREFIX "do_vnc_resolution %s"
#define CAN_CONFIGURE   GET_PREFIX "can_configure"
#define SET_LOCALE      SET_PREFIX "do_change_locale_rc_gui %s"
#define SET_TIMEZONE    SET_PREFIX "do_change_timezone_rc_gui %s"
#define SET_KEYBOARD    SET_PREFIX "do_change_keyboard_rc_gui %s"
#define GET_BROWSER     GET_PREFIX "get_browser"
#define SET_BROWSER     SET_PREFIX "do_browser %s"
#define FF_INSTALLED    GET_PREFIX "is_installed firefox"
#define CR_INSTALLED    GET_PREFIX "is_installed chromium"
#define RPC_INSTALLED   GET_PREFIX "is_installed rpi-connect"
#define VKBD_INSTALLED  GET_PREFIX "is_installed squeekboard"
#define GET_SQUEEK      GET_PREFIX "get_squeekboard"
#define SET_SQUEEK      SET_PREFIX "do_squeekboard S%d"
#define GET_SQUEEKOUT   GET_PREFIX "get_squeek_output"
#define SET_SQUEEKOUT   SET_PREFIX "do_squeek_output %s"
#define DEFAULT_GPU_MEM "vcgencmd get_mem gpu | cut -d = -f 2 | cut -d M -f 1"
#define CHANGE_PASSWD   "echo $USER:'%s' | SUDO_ASKPASS=/usr/lib/rc-gui/pwdrcg.sh sudo -A chpasswd -e"

#define CONFIG_SWITCH(wid,name,var,cmd) wid = gtk_builder_get_object (builder, name); \
                                        gtk_switch_set_active (GTK_SWITCH (wid), !(var = get_status (cmd)));

#define CONFIG_SET_SWITCH(wid,name,var,getcmd,setcmd) wid = gtk_builder_get_object (builder, name); \
                                        gtk_switch_set_active (GTK_SWITCH (wid), !(var = get_status (getcmd))); \
                                        g_signal_connect (wid, "state-set", G_CALLBACK (on_switch), setcmd);

#define READ_SWITCH(wid,var,cmd,reb)    if (var == gtk_switch_get_active (GTK_SWITCH (wid))) \
                                        { \
                                            vsystem (cmd, (1 - var)); \
                                            if (reb) reboot = 1; \
                                        }

/* Controls */

static GObject *passwd_btn, *hostname_btn, *locale_btn, *timezone_btn, *keyboard_btn, *wifi_btn, *ofs_btn;
static GObject *boot_desktop_rb, *boot_cli_rb, *chromium_rb, *firefox_rb;
static GObject *overscan_sw, *overscan2_sw, *ssh_sw, *rgpio_sw, *vnc_sw;
static GObject *spi_sw, *i2c_sw, *serial_sw, *onewire_sw, *usb_sw, *squeek_cb, *squeekop_cb;
static GObject *alogin_sw, *splash_sw, *scons_sw;
static GObject *blank_sw, *led_actpwr_sw, *fan_sw;
static GObject *overclock_cb, *hostname_tb, *ofs_en_sw, *bp_ro_sw, *ofs_lbl;
static GObject *fan_gpio_sb, *fan_temp_sb, *vnc_res_cb;
static GObject *pwentry1_tb, *pwentry2_tb, *pwok_btn;
static GObject *hostname_tb;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb;
static GObject *keymodel_cb, *keylayout_cb, *keyvar_cb, *keyalayout_cb, *keyavar_cb, *keyshort_cb, *keyled_cb, *keyalt_btn;
static GObject *keybox5, *keybox6, *keybox7, *keybox8;

static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static int orig_boot, orig_overscan, orig_overscan2, orig_ssh, orig_spi, orig_i2c, orig_serial, orig_scons, orig_splash;
static int orig_clock, orig_autolog, orig_onewire, orig_rgpio, orig_vnc, orig_usbi, orig_squeek;
static int orig_ofs, orig_bpro, orig_blank, orig_leds, orig_fan, orig_fan_gpio, orig_fan_temp, orig_vnc_res;
static char *vres, *orig_browser, *orig_sop;

/* Reboot flag set after locale change */

static int needs_reboot, ovfs_rb;

static int alt_keys;

/* Window manager in use */

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;
static wm_type wm;

/* Globals accessed from multiple threads */

static char gbuffer[512];
GThread *pthread;
gulong draw_id;

/* Lists for keyboard setting */

GtkListStore *model_list, *layout_list, *variant_list, *avariant_list, *toggle_list, *led_list;

/* List for locale setting */

GtkListStore *locale_list, *country_list, *charset_list;

#define LOC_NAME   0
#define LOC_LCODE  1
#define LOC_CCODE  2
#define LOC_LCCODE 3

GtkListStore *timezone_list, *tzcity_list;

#define TZ_NAME 0
#define TZ_PATH 1
#define TZ_AREA 2

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

static gboolean on_switch (GtkSwitch *btn, gboolean state, const char *cmd)
{
    vsystem (cmd, (1 - state));
    return FALSE;
}

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char *buf = NULL;
    size_t res = 0;
    int val = 0;

    if (fp == NULL) return 0;
    if (getline (&buf, &res, fp) > 0)
    {
        if (sscanf (buf, "%zu", &res) == 1)
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
    size_t len = 0;
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

static char *get_quoted_param (char *path, char *fname, char *toseek)
{
    char *pathname, *linebuf, *cptr, *dptr, *res;
    size_t len;

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
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

static gboolean ok_clicked (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    if (!main_dlg) gtk_main_quit ();
    return FALSE;
}

static void info (char *msg)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_clicked), NULL);
    gtk_widget_show (wid);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_buttons");
    gtk_widget_show (wid);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

static gboolean close_app (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    gtk_main_quit ();
    return FALSE;
}

static gboolean close_app_reboot (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    gtk_main_quit ();
    vsystem ("reboot");
    return FALSE;
}

static gboolean reboot_prompt (gpointer data)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), _("The changes you have made require the Raspberry Pi to be rebooted to take effect.\n\nWould you like to reboot now? "));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_cancel");
    gtk_button_set_label (GTK_BUTTON (wid), _("_No"));
    g_signal_connect (wid, "clicked", G_CALLBACK (close_app), NULL);
    gtk_widget_show (wid);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
    gtk_button_set_label (GTK_BUTTON (wid), _("_Yes"));
    g_signal_connect (wid, "clicked", G_CALLBACK (close_app_reboot), NULL);
    gtk_widget_show (wid);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_buttons");
    gtk_widget_show (wid);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
    return FALSE;
}


/* Password setting */

static void set_passwd (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb))) && g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (pwentry1_tb)), 
        gtk_entry_get_text (GTK_ENTRY(pwentry2_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "passwddlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    pwentry1_tb = gtk_builder_get_object (builder, "pwentry1");
    pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
    gtk_entry_set_visibility (GTK_ENTRY (pwentry1_tb), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwentry2_tb), FALSE);
    g_signal_connect (pwentry1_tb, "changed", G_CALLBACK (set_passwd), NULL);
    g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (set_passwd), NULL);
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

/* Hostname setting */

static void on_change_hostname (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    int res;
    const char *new_hn, *cptr;
    char *orig_hn;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "hostnamedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
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
                needs_reboot = 1;
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

    gtk_tree_model_get (model, iter, LOC_LCODE, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
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

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

static gpointer locale_thread (gpointer data)
{
    vsystem (SET_LOCALE, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

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
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

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
            needs_reboot = 1;
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

    gtk_tree_model_get (model, iter, TZ_AREA, &str, -1);
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

static gpointer timezone_thread (gpointer data)
{
    vsystem (SET_TIMEZONE, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
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

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

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

/* Wifi country setting */

static void on_set_wifi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char *buffer, *cnow, *cptr;
    FILE *fp;
    int n, found;
    size_t len;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "wcdlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

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

/* Keyboard setting */

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
    gtk_tree_model_get (gtk_combo_box_get_model (cb), &iter, 0, &t1, 1, &t2, -1);

    // reset the list of variants and add the layout name as a default
    variant_list = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (cb2)));
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

    set_init (GTK_TREE_MODEL (variant_list), cb2, 1, NULL);
}

static gpointer keyboard_thread (gpointer ptr)
{
    vsystem (SET_KEYBOARD, gbuffer);
    g_idle_add (close_msg, NULL);
    return NULL;
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

static void on_keyalt_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
        alt_keys = TRUE;
    else
        alt_keys = FALSE;

    gtk_widget_set_visible (GTK_WIDGET (keybox5), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox6), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox7), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox8), alt_keys);
}

static void populate_toggles (void)
{
    GtkTreeIter iter;

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("None"), 1, "", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Alt + Space"), 1, "grp:alt_space_toggle", -1); // overridden by wm

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Ctrl + Alt"), 1, "grp:ctrl_alt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Ctrl + Shift"), 1, "grp:ctrl_shift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Alt"), 1, "grp:lalt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Alt + Caps"), 1, "grp:alt_caps_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Alt + Left Shift"), 1, "grp:lalt_lshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Left Alt + Right Alt"), 1, "grp:alts_toggle", -1); // one way only

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Alt + Shift"), 1, "grp:alt_shift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Ctrl"), 1, "grp:lctrl_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Ctrl + Left Alt"), 1, "grp:lctrl_lalt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Ctrl + Left Shift"), 1, "grp:lctrl_lshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Left Ctrl + Left Win"), 1, "grp:lctrl_lwin_toggle", -1); // triggers menu

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Ctrl + Right Ctrl"), 1, "grp:ctrls_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Shift"), 1, "grp:lshift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Left Shift + Right Shift"), 1, "grp:shifts_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Left Win"), 1, "grp:lwin_toggle", -1); // triggers menu

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Menu"), 1, "grp:menu_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Right Alt"), 1, "grp:toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Right Alt + Right Shift"), 1, "grp:ralt_rshift_toggle", -1); // does nothing

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Right Ctrl"), 1, "grp:rctrl_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Right Ctrl + Right Alt"), 1, "grp:rctrl_ralt_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Right Ctrl + Right Shift"), 1, "grp:rctrl_rshift_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Right Shift"), 1, "grp:rshift_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Right Win"), 1, "grp:rwin_toggle", -1); // can't test...

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Scroll Lock"), 1, "grp:sclk_toggle", -1);

    gtk_list_store_append (toggle_list, &iter);
    gtk_list_store_set (toggle_list, &iter, 0, _("Shift + Caps"), 1, "grp:shift_caps_toggle", -1);

    //gtk_list_store_append (toggle_list, &iter);
    //gtk_list_store_set (toggle_list, &iter, 0, _("Win + Space"), 1, "grp:win_space_toggle", -1); // triggers menu

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, 0, _("None"), 1, "", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, 0, _("Caps"), 1, "grp_led:caps", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, 0, _("Num"), 1, "grp_led:num", -1);

    gtk_list_store_append (led_list, &iter);
    gtk_list_store_set (led_list, &iter, 0, _("Scroll"), 1, "grp_led:scroll", -1);
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GtkCellRenderer *col;
    GtkTreeIter iter;
    char *init_model, *init_layout, *init_variant, *init_alayout, *init_avariant, *init_options;
    char *new_mod, *new_lay, *new_var, *new_alay, *new_avar, *new_opts, *new_opt[2];
    char *cptr;
    int init_alt;

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
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "keyboarddlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

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
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keymodel_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keylayout_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keylayout_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyvar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyvar_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyalayout_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyalayout_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyavar_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyavar_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyshort_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyshort_cb), col, "text", 0);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (keyled_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (keyled_cb), col, "text", 0);

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
    gtk_widget_set_visible (GTK_WIDGET (keybox5), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox6), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox7), alt_keys);
    gtk_widget_set_visible (GTK_WIDGET (keybox8), alt_keys);

    set_init (GTK_TREE_MODEL (model_list), keymodel_cb, 1, init_model);

    g_signal_connect (keylayout_cb, "changed", G_CALLBACK (layout_changed), keyvar_cb);
    set_init (GTK_TREE_MODEL (layout_list), keylayout_cb, 1, init_layout);
    set_init (GTK_TREE_MODEL (variant_list), keyvar_cb, 1, init_variant);

    g_signal_connect (keyalayout_cb, "changed", G_CALLBACK (layout_changed), keyavar_cb);
    set_init (GTK_TREE_MODEL (layout_list), keyalayout_cb, 1, init_alayout);
    set_init (GTK_TREE_MODEL (avariant_list), keyavar_cb, 1, init_avariant);

    set_init_sub (GTK_TREE_MODEL (toggle_list), keyshort_cb, 1, init_options);
    set_init_sub (GTK_TREE_MODEL (led_list), keyled_cb, 1, init_options);

    g_signal_connect (keyalt_btn, "toggled", G_CALLBACK (on_keyalt_toggle), NULL);

    g_object_unref (builder);

    // run the dialog
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keymodel_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (model_list), &iter, 1, &new_mod, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keylayout_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 1, &new_lay, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyvar_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (variant_list), &iter, 1, &new_var, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyalayout_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (layout_list), &iter, 1, &new_alay, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyavar_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (avariant_list), &iter, 1, &new_avar, -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyshort_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (toggle_list), &iter, 1, &new_opt[0], -1);
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (keyled_cb), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (led_list), &iter, 1, &new_opt[1], -1);

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
            if (ptr == NULL) message (_("Setting keyboard - please wait..."));

            if (alt_keys)
                sprintf (gbuffer, "\"%s\" \"%s,%s\" \"%s,%s\" \"%s\"", new_mod, new_lay, new_alay, new_var, new_avar, new_opts ? new_opts : "");
            else
                sprintf (gbuffer, "\"%s\" \"%s\" \"%s\" \"\"", new_mod, new_lay, new_var);

            // launch a thread with the system call to update the keyboard
            pthread = g_thread_new (NULL, keyboard_thread, NULL);

            if (ptr != NULL)
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

/* Overlay file system setting */

static gboolean on_overlay_fs (GtkSwitch *btn, gboolean state, gpointer ptr)
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

static void on_set_ofs (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");

    if (vsystem (CHECK_UNAME))
    {
        info (_("Your system has recently been updated. Please reboot to ensure these updates have loaded before setting up the overlay file system."));
        return;
    }

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "ofsdlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

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

    g_signal_connect (ofs_en_sw, "state-set", G_CALLBACK (on_overlay_fs), NULL);
    g_signal_connect (bp_ro_sw, "state-set", G_CALLBACK (on_overlay_fs), NULL);
    gtk_widget_realize (GTK_WIDGET (ofs_lbl));
    gtk_widget_set_size_request (dlg, gtk_widget_get_allocated_width (dlg), gtk_widget_get_allocated_height (dlg));
    gtk_widget_hide (GTK_WIDGET (ofs_lbl));

    ovfs_rb = 0;
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (ovfs_rb) needs_reboot = 1;
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

/* Button handlers */

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

static gboolean on_alogin_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    config_boot ();
    return FALSE;
}

static void on_boot_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_switch_set_active (GTK_SWITCH (splash_sw), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), FALSE);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), TRUE);
    }
    config_boot ();
}

static gboolean on_serial_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    if (state)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (scons_sw), TRUE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (scons_sw), _("Enable shell and kernel messages on the serial connection"));
    }
    else
    {
        gtk_switch_set_active (GTK_SWITCH (scons_sw), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (scons_sw), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (scons_sw), _("This setting cannot be changed while the serial port is disabled"));
    }
    vsystem (SET_SERIALHW, (1 - state));
    return FALSE;
}

static void fan_config (void)
{
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
}

static void on_fan_value_changed (GtkSpinButton *sb)
{
    fan_config ();
}

static gboolean on_fan_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    gtk_widget_set_sensitive (GTK_WIDGET (fan_gpio_sb), state);
    gtk_widget_set_sensitive (GTK_WIDGET (fan_temp_sb), state);
    if (state)
    {
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_gpio_sb), _("Set the GPIO to which the fan is connected"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_temp_sb), _("Set the temperature in degrees C at which the fan turns on"));
    }
    else
    {
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_gpio_sb), _("This setting cannot be changed unless the fan is enabled"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (fan_temp_sb), _("This setting cannot be changed unless the fan is enabled"));
    }
    fan_config ();
    return FALSE;
}

static void on_browser_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb)))
        vsystem (SET_BROWSER, "chromium");
    else
        vsystem (SET_BROWSER, "firefox");
}

static void on_squeekboard_set (GtkComboBox* cb, gpointer ptr)
{
    vsystem (SET_SQUEEK, gtk_combo_box_get_active (cb) + 1);
}

static void on_squeek_output_set (GtkComboBoxText* cb, gpointer ptr)
{
    char *op = gtk_combo_box_text_get_active_text (cb);
    vsystem (SET_SQUEEKOUT, op);
    g_free (op);
}

static gboolean on_leds_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    vsystem (SET_LEDS, (1 - state));
    return FALSE;
}

static void on_overclock_set (GtkComboBox* cb, gpointer ptr)
{
    switch (get_status (GET_PI_TYPE))
    {
        case 1:
            switch (gtk_combo_box_get_active (cb))
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
            switch (gtk_combo_box_get_active (cb))
            {
                case 0 :    vsystem (SET_OVERCLOCK, "None");
                            break;
                case 1 :    vsystem (SET_OVERCLOCK, "High");
                            break;
            }
            break;
    }
}

static void on_vnc_res_set (GtkComboBox* cb, gpointer ptr)
{
    vres = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (vnc_res_cb));
    vsystem (SET_VNC_RES, vres);
    g_free (vres);
}

/* Write the changes to the system when OK is pressed */

static gpointer process_changes_thread (gpointer ptr)
{
    int reboot = (int) ptr;

    //if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
    //    || orig_autolog == gtk_switch_get_active (GTK_SWITCH (alogin_sw)))
    //{
    //    if (gtk_switch_get_active (GTK_SWITCH (alogin_sw)))
    //    {
    //        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) vsystem (SET_BOOT_GUIA);
    //        else vsystem (SET_BOOT_CLIA);
    //    }
    //    else
    //    {
    //        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) vsystem (SET_BOOT_GUI);
    //        else vsystem (SET_BOOT_CLI);
    //    }
    //}

    //READ_SWITCH (splash_sw, orig_splash, SET_SPLASH, FALSE);
    //READ_SWITCH (ssh_sw, orig_ssh, SET_SSH, FALSE);
    //READ_SWITCH (blank_sw, orig_blank, SET_BLANK, wm == WM_WAYFIRE ? FALSE : TRUE);
    //READ_SWITCH (overscan_sw, orig_overscan, SET_OVERSCAN, FALSE);
    //READ_SWITCH (overscan2_sw, orig_overscan2, SET_OVERSCAN2, FALSE);
    //if (gtk_combo_box_get_active (GTK_COMBO_BOX (squeek_cb)) != orig_squeek)
    //    vsystem (SET_SQUEEK, gtk_combo_box_get_active (GTK_COMBO_BOX (squeek_cb)) + 1);
    //char *sop = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (squeekop_cb));
    //if (sop && orig_sop && g_strcmp0 (orig_sop, sop))
    //    vsystem (SET_SQUEEKOUT, sop);
    //if (sop) g_free (sop);

    //if (strcmp (orig_browser, "chromium") && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb)))
    //    vsystem (SET_BROWSER, "chromium");
    //if (strcmp (orig_browser, "firefox") && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (firefox_rb)))
    //    vsystem (SET_BROWSER, "firefox");

    if (!vsystem (IS_PI))
    {
        //READ_SWITCH (vnc_sw, orig_vnc, SET_VNC, FALSE);
        //READ_SWITCH (spi_sw, orig_spi, SET_SPI, FALSE);
        //READ_SWITCH (i2c_sw, orig_i2c, SET_I2C, FALSE);
        //READ_SWITCH (onewire_sw, orig_onewire, SET_1WIRE, TRUE);
        //READ_SWITCH (rgpio_sw, orig_rgpio, SET_RGPIO, FALSE);
        //READ_SWITCH (usb_sw, orig_usbi, SET_USBI, TRUE);
        //READ_SWITCH (serial_sw, orig_serial, SET_SERIALHW, TRUE);
        //READ_SWITCH (scons_sw, orig_scons, SET_SERIALCON, TRUE);

        //if (orig_leds != -1) READ_SWITCH (led_actpwr_sw, orig_leds, SET_LEDS, FALSE);

        //if (orig_clock != -1 && orig_clock != gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
        //{
        //    switch (get_status (GET_PI_TYPE))
        //    {
        //        case 1:
        //            switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
        //            {
        //                case 0 :    vsystem (SET_OVERCLOCK, "None");
        //                            break;
        //                case 1 :    vsystem (SET_OVERCLOCK, "Modest");
        //                            break;
        //                case 2 :    vsystem (SET_OVERCLOCK, "Medium");
        //                            break;
        //                case 3 :    vsystem (SET_OVERCLOCK, "High");
        //                            break;
        //                case 4 :    vsystem (SET_OVERCLOCK, "Turbo");
        //                            break;
        //            }
        //            reboot = 1;
        //            break;

        //        case 2:
        //            switch (gtk_combo_box_get_active (GTK_COMBO_BOX (overclock_cb)))
        //            {
        //                case 0 :    vsystem (SET_OVERCLOCK, "None");
        //                            break;
        //                case 1 :    vsystem (SET_OVERCLOCK, "High");
        //                            break;
        //            }
        //            reboot = 1;
        //            break;
        //    }
        //}

        //if (wm == WM_OPENBOX)
        //{
        //    if (orig_vnc_res != gtk_combo_box_get_active (GTK_COMBO_BOX (vnc_res_cb)))
        //    {
        //        vres = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (vnc_res_cb));
        //        vsystem (SET_VNC_RES, vres);
        //        g_free (vres);
        //        reboot = 1;
        //    }
        //}

        //if (!vsystem (IS_PI4))
        //{
        //    int fan_gpio = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (fan_gpio_sb));
        //    int fan_temp = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (fan_temp_sb));
        //    if (!gtk_switch_get_active (GTK_SWITCH (fan_sw)))
        //    {
        //        if (orig_fan == 0) vsystem (SET_FAN, 1, 0, 0);
        //    }
        //    else
        //    {
        //        if (orig_fan == 1 || orig_fan_gpio != fan_gpio || orig_fan_temp != fan_temp) vsystem (SET_FAN, 0, fan_gpio, fan_temp);
        //    }
        //}
    }

    if (reboot) g_idle_add (reboot_prompt, NULL);
    else gtk_main_quit ();

    return NULL;
}

/* Status checks */

static int has_wifi (void)
{
    char *res;
    int ret = 0;

    res = get_string (WLAN_INTERFACES);
    if (res && strlen (res) > 0) ret = 1;
    g_free (res);
    return ret;
}

static gboolean cancel_main (GtkButton *button, gpointer data)
{
    if (needs_reboot) reboot_prompt (NULL);
    else gtk_main_quit ();
    return FALSE;
}

static gboolean ok_main (GtkButton *button, gpointer data)
{
    message (_("Updating configuration - please wait..."));
    pthread = g_thread_new (NULL, process_changes_thread, (gpointer) needs_reboot);
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
    return TRUE;
}

static int num_screens (void)
{
    if (wm != WM_OPENBOX)
        return get_status ("wlr-randr | grep -cv '^ '");
    else
        return get_status ("xrandr -q | grep -cw connected");
}

#ifdef PLUGIN_NAME
GtkBuilder *builder;
#endif

static gboolean init_config (gpointer data)
{
#ifndef PLUGIN_NAME
    GtkBuilder *builder;
#endif
    GtkAdjustment *gadj, *tadj;
    GtkWidget *wid;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    g_signal_connect (main_dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_cancel");
    g_signal_connect (wid, "clicked", G_CALLBACK (cancel_main), NULL);

    passwd_btn = gtk_builder_get_object (builder, "button_pw");
    g_signal_connect (passwd_btn, "clicked", G_CALLBACK (on_change_passwd), NULL);

    hostname_btn = gtk_builder_get_object (builder, "button_hn");
    g_signal_connect (hostname_btn, "clicked", G_CALLBACK (on_change_hostname), NULL);

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

    CONFIG_SET_SWITCH (splash_sw, "sw_splash", orig_splash, GET_SPLASH, SET_SPLASH);
    CONFIG_SWITCH (alogin_sw, "sw_alogin", orig_autolog, GET_AUTOLOGIN);
    g_signal_connect (alogin_sw, "state-set", G_CALLBACK (on_alogin_toggle), NULL);
    CONFIG_SET_SWITCH (ssh_sw, "sw_ssh", orig_ssh, GET_SSH, SET_SSH);
    CONFIG_SET_SWITCH (blank_sw, "sw_blank", orig_blank, GET_BLANK, SET_BLANK);

    boot_desktop_rb = gtk_builder_get_object (builder, "rb_desktop");
    boot_cli_rb = gtk_builder_get_object (builder, "rb_cli");
    g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_boot_toggle), NULL);
    if ((orig_boot = get_status (GET_BOOT_CLI)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), FALSE);
    }

    orig_browser = get_string (GET_BROWSER);
    chromium_rb = gtk_builder_get_object (builder, "rb_chromium");
    firefox_rb = gtk_builder_get_object (builder, "rb_firefox");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (firefox_rb), !strcmp (orig_browser, "firefox"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chromium_rb), !strcmp (orig_browser, "chromium"));
    if (vsystem (CR_INSTALLED) || vsystem (FF_INSTALLED))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (chromium_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (firefox_rb), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (chromium_rb), _("Multiple browsers are not installed"));
        gtk_widget_set_tooltip_text (GTK_WIDGET (firefox_rb), _("Multiple browsers are not installed"));
    }
    g_signal_connect (chromium_rb, "toggled", G_CALLBACK (on_browser_toggle), NULL);

    if (!vsystem (XSCR_INSTALLED))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (blank_sw), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (blank_sw), _("This setting is overridden when Xscreensaver is installed"));
    }

    if (wm != WM_OPENBOX)
    {
        char *line, *cptr;
        size_t len;
        FILE *fp;
        int op = 0;

        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox52")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox53")));
        squeek_cb = gtk_builder_get_object (builder, "cb_squeek");
        orig_squeek = get_status (GET_SQUEEK);
        gtk_combo_box_set_active (GTK_COMBO_BOX (squeek_cb), orig_squeek);

        squeekop_cb = gtk_builder_get_object (builder, "cb_squeekout");
        orig_sop = get_string (GET_SQUEEKOUT);
        fp = popen ("wlr-randr", "r");
        if (fp)
        {
            line = NULL;
            len = 0;
            while (getline (&line, &len, fp) != -1)
            {
                if (line[0] != ' ')
                {
                    cptr = line;
                    while (*cptr != ' ') cptr++;
                    *cptr = 0;
                    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (squeekop_cb), line);
                    if (orig_sop && !g_strcmp0 (orig_sop, line))
                    {
                        gtk_combo_box_set_active (GTK_COMBO_BOX (squeekop_cb), op);
                    }
                    op++;
                }
            }
            free (line);
            pclose (fp);
        }

        if (vsystem (VKBD_INSTALLED))
        {
            gtk_widget_set_sensitive (GTK_WIDGET (squeek_cb), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (squeek_cb), _("A virtual keyboard is not installed"));
            gtk_widget_set_sensitive (GTK_WIDGET (squeekop_cb), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (squeekop_cb), _("A virtual keyboard is not installed"));
        }
        g_signal_connect (squeek_cb, "changed", G_CALLBACK (on_squeekboard_set), NULL);
        g_signal_connect (squeekop_cb, "changed", G_CALLBACK (on_squeek_output_set), NULL);
    }
    else
    {
        if (num_screens () != 2) gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox53")));
        else gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "label52")), _("Overscan (HDMI-1):"));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox57")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox58")));
    }

    if (!vsystem (IS_PI))
    {
        CONFIG_SET_SWITCH (overscan_sw, "sw_os1", orig_overscan, GET_OVERSCAN, SET_OVERSCAN);
        CONFIG_SET_SWITCH (overscan2_sw, "sw_os2", orig_overscan2, GET_OVERSCAN2, SET_OVERSCAN2);
        CONFIG_SET_SWITCH (spi_sw, "sw_spi", orig_spi, GET_SPI, SET_SPI);
        CONFIG_SET_SWITCH (i2c_sw, "sw_i2c", orig_i2c, GET_I2C, SET_I2C);
        CONFIG_SET_SWITCH (onewire_sw, "sw_one", orig_onewire, GET_1WIRE, SET_1WIRE);
        CONFIG_SET_SWITCH (rgpio_sw, "sw_rgp", orig_rgpio, GET_RGPIO, SET_RGPIO);
        CONFIG_SET_SWITCH (vnc_sw, "sw_vnc", orig_vnc, GET_VNC, SET_VNC);
        CONFIG_SET_SWITCH (usb_sw, "sw_usb", orig_usbi, GET_USBI, SET_USBI);

        if (!vsystem (IS_PI5))
        {
            CONFIG_SET_SWITCH (scons_sw, "sw_serc", orig_scons, GET_SERIALCON, SET_SERIALCON);
            CONFIG_SWITCH (serial_sw, "sw_ser", orig_serial, GET_SERIALHW);
            g_signal_connect (serial_sw, "state-set", G_CALLBACK (on_serial_toggle), NULL);
        }
        else
        {
            CONFIG_SET_SWITCH (scons_sw, "sw_serc", orig_scons, GET_SERIALCON, SET_SERIALCON);
            serial_sw = gtk_builder_get_object (builder, "sw_ser");
            g_signal_connect (serial_sw, "state-set", G_CALLBACK (on_serial_toggle), NULL);
            if ((orig_serial = get_status (GET_SERIALHW)))
            {
                gtk_switch_set_active (GTK_SWITCH (serial_sw), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET (scons_sw), FALSE);
                gtk_widget_set_tooltip_text (GTK_WIDGET (scons_sw), _("This setting cannot be changed while the serial port is disabled"));
            }
            else
            {
                gtk_switch_set_active (GTK_SWITCH (serial_sw), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (scons_sw), TRUE);
                gtk_widget_set_tooltip_text (GTK_WIDGET (scons_sw), _("Enable shell and kernel messages on the serial connection"));
            }

            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox37")));
        }

        // disable the buttons if VNC isn't installed
        if (vsystem (RVNC_INSTALLED) && vsystem (WVNC_INSTALLED))
        {
            gtk_widget_set_sensitive (GTK_WIDGET (vnc_sw), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (vnc_sw), _("The VNC server is not installed"));
        }

        led_actpwr_sw = gtk_builder_get_object (builder, "sw_led_actpwr");
        orig_leds = get_status (GET_LEDS);
        if (orig_leds == -1) gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox17")));
        else
        {
            gtk_switch_set_active (GTK_SWITCH (led_actpwr_sw), !(orig_leds));
            g_signal_connect (led_actpwr_sw, "state-set", G_CALLBACK (on_leds_toggle), NULL);
        }

        if (vsystem (IS_PI4))
        {
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox34")));
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox35")));
            gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox36")));
        }
        else
        {
            fan_sw = gtk_builder_get_object (builder, "sw_fan");
            fan_gpio_sb = gtk_builder_get_object (builder, "sb_fan_gpio");
            fan_temp_sb = gtk_builder_get_object (builder, "sb_fan_temp");
            if ((orig_fan = get_status (GET_FAN)))
            {
                gtk_switch_set_active (GTK_SWITCH (fan_sw), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET (fan_gpio_sb), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET (fan_temp_sb), FALSE);
                gtk_widget_set_tooltip_text (GTK_WIDGET (fan_gpio_sb), _("This setting cannot be changed unless the fan is enabled"));
                gtk_widget_set_tooltip_text (GTK_WIDGET (fan_temp_sb), _("This setting cannot be changed unless the fan is enabled"));
            }
            else
            {
                gtk_switch_set_active (GTK_SWITCH (fan_sw), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (fan_gpio_sb), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (fan_temp_sb), TRUE);
            }
            g_signal_connect (fan_sw, "state-set", G_CALLBACK (on_fan_toggle), NULL);

            gadj = gtk_adjustment_new (14, 2, 27, 1, 1, 0);
            gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (fan_gpio_sb), GTK_ADJUSTMENT (gadj));
            orig_fan_gpio = get_status (GET_FAN_GPIO);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (fan_gpio_sb), orig_fan_gpio);
            g_signal_connect (fan_gpio_sb, "value_changed", G_CALLBACK (on_fan_value_changed), NULL);

            tadj = gtk_adjustment_new (80, 60, 120, 5, 10, 0);
            gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (fan_temp_sb), GTK_ADJUSTMENT (tadj));
            orig_fan_temp = get_status (GET_FAN_TEMP);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (fan_temp_sb), orig_fan_temp);
            g_signal_connect (fan_temp_sb, "value_changed", G_CALLBACK (on_fan_value_changed), NULL);
        }

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
            g_signal_connect (overclock_cb, "changed", G_CALLBACK (on_overclock_set), NULL);
        }

        ofs_btn = gtk_builder_get_object (builder, "button_ofs");
        g_signal_connect (ofs_btn, "clicked", G_CALLBACK (on_set_ofs), NULL);

        if (wm == WM_OPENBOX)
        {
            vnc_res_cb = gtk_builder_get_object (builder, "combo_res");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "640x480");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "720x480");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "800x600");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "1024x768");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "1280x720");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "1280x1024");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "1600x1200");
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vnc_res_cb), "1920x1080");

            orig_vnc_res = -1;
            vres = get_string (GET_VNC_RES);
            if (!strcmp (vres, "640x480")) orig_vnc_res = 0;
            if (!strcmp (vres, "720x480")) orig_vnc_res = 1;
            if (!strcmp (vres, "800x600")) orig_vnc_res = 2;
            if (!strcmp (vres, "1024x768")) orig_vnc_res = 3;
            if (!strcmp (vres, "1280x720")) orig_vnc_res = 4;
            if (!strcmp (vres, "1280x1024")) orig_vnc_res = 5;
            if (!strcmp (vres, "1600x1200")) orig_vnc_res = 6;
            if (!strcmp (vres, "1920x1080")) orig_vnc_res = 7;
            g_free (vres);

            gtk_combo_box_set_active (GTK_COMBO_BOX (vnc_res_cb), orig_vnc_res);
            g_signal_connect (vnc_res_cb, "changed", G_CALLBACK (on_vnc_res_set), NULL);
        }
        else gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox56")));
    }
    else
    {
        CONFIG_SET_SWITCH (overscan_sw, "sw_os1", orig_overscan, GET_OVERSCAN, SET_OVERSCAN);
        CONFIG_SET_SWITCH (overscan2_sw, "sw_os2", orig_overscan2, GET_OVERSCAN2, SET_OVERSCAN2);

        if (!get_status ("grep -q boot=live /proc/cmdline ; echo $?"))
        {
            gtk_widget_set_sensitive (GTK_WIDGET (splash_sw), FALSE);
        }

        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "vbox30")));

        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox17")));

        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox23")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox24")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox26")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox27")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox28")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox29")));

        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox56")));
    }
#ifndef PLUGIN_NAME
    g_object_unref (builder);

    gtk_widget_show (main_dlg);
    gtk_widget_destroy (msg_dlg);
#endif

    return FALSE;
}

/* Bleagh...
 * We need to start loading config once the message window is properly drawn,
 * which requires it to have focus. So the only way to do this is to hang off
 * the window state event and look for when it indicates that the window has
 * focus. I cannot believe this is actually necessary, but hanging off "show"
 * or "draw" doesn't indicate that the window has fully drawn under Wayland. ..
 */

static gboolean event (GtkWidget *wid, GdkEventWindowState *ev, gpointer data)
{
    if (ev->type == GDK_WINDOW_STATE)
    {
        if (ev->changed_mask == GDK_WINDOW_STATE_FOCUSED
            && ev->new_window_state & GDK_WINDOW_STATE_FOCUSED)
                g_idle_add (init_config, NULL);
    }
    return FALSE;
}

static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data)
{
    g_signal_handler_disconnect (wid, draw_id);
    g_idle_add (init_config, NULL);
    return FALSE;
}

#ifdef PLUGIN_NAME
/*----------------------------------------------------------------------------*/
/* Plugin interface */
/*----------------------------------------------------------------------------*/

void init_plugin (void)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    needs_reboot = 0;
    main_dlg = NULL;

    init_config (NULL);
}

int plugin_tabs (void)
{
    return 5;
}

const char *tab_name (int tab)
{
    switch (tab)
    {
        case 0 : return C_("tab", "System");
        case 1 : return C_("tab", "Display");
        case 2 : return C_("tab", "Interfaces");
        case 3 : return C_("tab", "Performance");
        case 4 : return C_("tab", "Localisation");
        default : return _("No such tab");
    }
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "notebook1");
    switch (tab)
    {
        case 0 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox10");
            break;
        case 1 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox50");
            break;
        case 2 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox20");
            break;
        case 3 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox30");
            break;
        case 4 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox40");
            break;
    }

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

#else

/* The dialog... */

int main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    if (argc == 2 && !g_strcmp0 (argv[1], "-w"))
    {
        on_set_wifi (NULL, NULL);
        return 0;
    }

    if (argc == 2 && !strcmp (argv[1], "-k"))
    {
        pthread = 0;
        on_set_keyboard (NULL, (gpointer) 1);
        if (pthread) g_thread_join (pthread);
        return 0;
    }

    needs_reboot = 0;
    main_dlg = NULL;
    if (vsystem (CAN_CONFIGURE))
    {
        info (_("The Raspberry Pi Configuration application can only modify a standard configuration.\n\nYour configuration appears to have been modified by other tools, and so this application cannot be used on your system.\n\nIn order to use this application, you need to have the latest firmware installed, Device Tree enabled, the default \"pi\" user set up and the lightdm application installed. "));
    }
    else
    {
        message (_("Loading configuration - please wait..."));
        if (wm != WM_OPENBOX) g_signal_connect (msg_dlg, "event", G_CALLBACK (event), NULL);
        else draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);
    }

    gtk_main ();

    if (main_dlg) gtk_widget_destroy (main_dlg);

    return 0;
}

#endif
