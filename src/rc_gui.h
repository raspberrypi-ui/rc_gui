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
#define REALTIME
#endif

#define SUDO_PREFIX     "SUDO_ASKPASS=/usr/bin/sudopwd sudo -A "
#define GET_PREFIX      "raspi-config nonint "
#define SET_PREFIX      SUDO_PREFIX GET_PREFIX
#define GET_PI_TYPE     GET_PREFIX "get_pi_type"
#define IS_PI           GET_PREFIX "is_pi"
#define IS_PI4          GET_PREFIX "is_pifour"
#define IS_PI5          GET_PREFIX "is_pifive"

#define CONFIG_SWITCH(wid,name,var,cmd) wid = gtk_builder_get_object (builder, name); \
                                        gtk_switch_set_active (GTK_SWITCH (wid), !(var = get_status (cmd)));

#ifdef REALTIME
#define HANDLE_SWITCH(wid,setcmd)       g_signal_connect (wid, "state-set", G_CALLBACK (on_switch), setcmd);
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
    WM_WAYFIRE,
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
extern int get_status (char *cmd);
extern char *get_quoted_param (char *path, char *fname, char *toseek);
extern gboolean on_switch (GtkSwitch *btn, gboolean state, const char *cmd);
extern void message (char *msg);
extern void info (char *msg);

/* End of file */
/*----------------------------------------------------------------------------*/
