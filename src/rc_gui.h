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
/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#ifdef PLUGIN_NAME
extern const char *dgetfixt (const char *domain, const char *msgctxid);
#undef _
#define _(a) dgettext(GETTEXT_PACKAGE,a)
#undef C_
#define C_(a,b) dgetfixt(GETTEXT_PACKAGE,a"\004"b)
#endif

#ifdef PLUGIN_NAME
#define REALTIME
#endif

#define SUDO_PREFIX     "SUDO_ASKPASS=/usr/bin/sudopwd sudo -A "
#define GET_PREFIX      "raspi-config nonint "
#define SET_PREFIX      SUDO_PREFIX GET_PREFIX
#define GET_PI_TYPE     GET_PREFIX "get_pi_type"
#define IS_PI           GET_PREFIX "is_pi"
#define IS_PI4          GET_PREFIX "is_pifour"
#define IS_PI5          GET_PREFIX "is_pifive"

/* system tab */
#define GET_HOSTNAME    GET_PREFIX "get_hostname"
#define SET_HOSTNAME    SET_PREFIX "do_hostname %s"
#define GET_BOOT_CLI    GET_PREFIX "get_boot_cli"
#define GET_ALOGIN_CLI  GET_PREFIX "get_autologin_cli"
#define GET_ALOGIN_DESK GET_PREFIX "get_autologin_desktop"
#define SET_ALOGIN      SET_PREFIX "do_autologin %d"
#define SET_BOOT_CLI    SET_PREFIX "do_boot_target B1"
#define SET_BOOT_GUI    SET_PREFIX "do_boot_target B2"
#define GET_SPLASH      GET_PREFIX "get_boot_splash"
#define SET_SPLASH      SET_PREFIX "do_boot_splash %d"
#define GET_LEDS        GET_PREFIX "get_leds"
#define SET_LEDS        SET_PREFIX "do_leds %d"
#define GET_BROWSER     GET_PREFIX "get_browser"
#define SET_BROWSER     SET_PREFIX "do_browser %s"
#define GET_PSUDO       GET_PREFIX "get_sudo_pass"
#define SET_PSUDO       SET_PREFIX "do_sudo_pass %d"
#define FF_INSTALLED    GET_PREFIX "get_installed firefox"
#define FFE_INSTALLED   GET_PREFIX "get_installed firefox-esr"
#define CR_INSTALLED    GET_PREFIX "get_installed chromium"

/* interface tab */
#define GET_SSH         GET_PREFIX "get_ssh"
#define SET_SSH         SET_PREFIX "do_ssh %d"
#define GET_VNC         GET_PREFIX "get_vnc"
#define SET_VNC         SET_PREFIX "do_vnc %d"
#define GET_SPI         GET_PREFIX "get_spi"
#define SET_SPI         SET_PREFIX "do_spi %d"
#define GET_I2C         GET_PREFIX "get_i2c"
#define SET_I2C         SET_PREFIX "do_i2c %d"
#define GET_SERIALCON   GET_PREFIX "get_serial_cons"
#define SET_SERIALCON   SET_PREFIX "do_serial_cons %d"
#define GET_SERIALHW    GET_PREFIX "get_serial_hw"
#define SET_SERIALHW    SET_PREFIX "do_serial_hw %d"
#define GET_1WIRE       GET_PREFIX "get_onewire"
#define SET_1WIRE       SET_PREFIX "do_onewire %d"
#define RVNC_INSTALLED  GET_PREFIX "get_installed realvnc-vnc-server"
#define WVNC_INSTALLED  GET_PREFIX "get_installed wayvnc"

/* display tab */
#define GET_OVERSCAN    GET_PREFIX "get_overscan_kms 1"
#define GET_OVERSCAN2   GET_PREFIX "get_overscan_kms 2"
#define SET_OVERSCAN    SET_PREFIX "do_overscan_kms 1 %d"
#define SET_OVERSCAN2   SET_PREFIX "do_overscan_kms 2 %d"
#define GET_BLANK       GET_PREFIX "get_blanking"
#define SET_BLANK       SET_PREFIX "do_blanking %d"
#define GET_VNC_RES     GET_PREFIX "get_vnc_resolution"
#define SET_VNC_RES     SET_PREFIX "do_vnc_resolution %s"
#define GET_SQUEEK      GET_PREFIX "get_squeekboard"
#define SET_SQUEEK      SET_PREFIX "do_squeekboard S%d"
#define GET_SQUEEKOUT   GET_PREFIX "get_squeek_output"
#define SET_SQUEEKOUT   SET_PREFIX "do_squeek_output %s"
#define VKBD_INSTALLED  GET_PREFIX "get_installed squeekboard"
#define XSCR_INSTALLED  GET_PREFIX "get_installed xscreensaver"

/* performance tab */
#define GET_OVERCLOCK   GET_PREFIX "get_config_var arm_freq /boot/firmware/config.txt"
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

/* localisation tab */
#define SET_LOCALE      SET_PREFIX "do_change_locale_rc_gui %s"
#define SET_TIMEZONE    SET_PREFIX "do_change_timezone_rc_gui %s"
#define SET_KEYBOARD    SET_PREFIX "do_change_keyboard_rc_gui %s"
#define GET_WIFI_CTRY   GET_PREFIX "get_wifi_country"
#define SET_WIFI_CTRY   SET_PREFIX "do_wifi_country %s"
#define WLAN_INTERFACES GET_PREFIX "list_wlan_interfaces"

#define CONFIG_SWITCH(wid,name,var,cmd) wid = gtk_builder_get_object (builder, name); \
                                        gtk_switch_set_active (GTK_SWITCH (wid), !(var = get_status (cmd)));

#ifdef REALTIME
#define HANDLE_SWITCH(wid,setcmd,getcmd)  g_signal_connect (wid, "notify::active", G_CALLBACK (on_switch), g_strdup_printf ("%s;%s",setcmd, getcmd));
#define HANDLE_CONTROL(wid,cmd,cb)      g_signal_connect (wid, cmd, G_CALLBACK(cb), NULL);
#else
#define HANDLE_SWITCH(wid,setcmd)
#define HANDLE_CONTROL(wid,cmd,cb)
#endif

#define READ_SWITCH(wid,var,cmd,reb)    if (var == gtk_switch_get_active (GTK_SWITCH (wid))) \
                                        { \
                                            vsystem (cmd, (1 - var)); \
                                            if (reb) reboot = 1; \
                                        }

#define CHECK_SWITCH(wid,var)           if (var == gtk_switch_get_active (GTK_SWITCH (wid))) \
                                        { \
                                            return TRUE; \
                                        }

typedef enum
{
    WM_OPENBOX,
    WM_LABWC
} wm_type;

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

extern GtkWidget *main_dlg, *msg_dlg;
extern GThread *pthread;
extern gboolean needs_reboot;
extern gboolean singledlg;
extern wm_type wm;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

extern int vsystem (const char *fmt, ...);
extern char *get_string (char *cmd);
extern char *get_string_cached (char *cmd);
extern int get_status (char *cmd);
extern char *get_quoted_param (char *path, char *fname, char *toseek);
extern void batch_get (int n, ...);
extern void batch_free (void);
extern void on_switch (GtkSwitch *btn, gpointer, const char *cmd);
extern void message (char *msg);
extern void info (char *msg);

extern void set_watch_cursor (void);
extern void clear_watch_cursor (void);

/* End of file */
/*----------------------------------------------------------------------------*/
