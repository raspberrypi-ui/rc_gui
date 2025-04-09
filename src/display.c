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
#define VKBD_INSTALLED  GET_PREFIX "is_installed squeekboard"
#define XSCR_INSTALLED  GET_PREFIX "is_installed xscreensaver"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GObject *overscan_sw, *overscan2_sw, *blank_sw, *vnc_res_cb, *squeek_cb, *squeekop_cb;
static int orig_overscan, orig_overscan2, orig_blank, orig_vnc_res, orig_squeek;
static char *orig_sop;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static int num_screens (void);
#ifdef REALTIME
static void on_squeekboard_set (GtkComboBox *cb, gpointer ptr);
static void on_squeek_output_set (GtkComboBoxText *cb, gpointer ptr);
static void on_vnc_res_set (GtkComboBoxText *cb, gpointer ptr);
static gboolean process_cb (gpointer data);
#endif

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

static int num_screens (void)
{
    if (wm != WM_OPENBOX)
        return get_status ("wlr-randr | grep -cv '^ '");
    else
        return get_status ("xrandr -q | grep -cw connected");
}

/*----------------------------------------------------------------------------*/
/* Real-time handlers                                                         */
/*----------------------------------------------------------------------------*/

#ifdef REALTIME

static void on_squeekboard_set (GtkComboBox *cb, gpointer ptr)
{
    char *cmd;
    set_watch_cursor ();
    cmd = g_strdup_printf (SET_SQUEEK, gtk_combo_box_get_active (cb) + 1);
    g_idle_add (process_cb, cmd);
}

static void on_squeek_output_set (GtkComboBoxText *cb, gpointer ptr)
{
    char *cmd, *op;
    set_watch_cursor ();
    op = gtk_combo_box_text_get_active_text (cb);
    cmd = g_strdup_printf (SET_SQUEEKOUT, op);
    g_free (op);
    g_idle_add (process_cb, cmd);
}

static void on_vnc_res_set (GtkComboBoxText *cb, gpointer ptr)
{
    char *cmd, *vres;
    set_watch_cursor ();
    vres = gtk_combo_box_text_get_active_text (cb);
    cmd = g_strdup_printf (SET_VNC_RES, vres);
    g_free (vres);
    g_idle_add (process_cb, cmd);
}

static gboolean process_cb (gpointer data)
{
    char *cmd = (char *) data;
    vsystem (cmd);
    g_free (cmd);
    clear_watch_cursor ();
    return FALSE;
}

#endif

/*----------------------------------------------------------------------------*/
/* Exit processing                                                            */
/*----------------------------------------------------------------------------*/

gboolean read_display_tab (void)
{
    gboolean reboot = FALSE;
    char *cptr;

    READ_SWITCH (blank_sw, orig_blank, SET_BLANK, wm == WM_OPENBOX ? TRUE : FALSE);
    READ_SWITCH (overscan_sw, orig_overscan, SET_OVERSCAN, FALSE);
    READ_SWITCH (overscan2_sw, orig_overscan2, SET_OVERSCAN2, FALSE);

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (squeek_cb)) != orig_squeek)
    {
        vsystem (SET_SQUEEK, gtk_combo_box_get_active (GTK_COMBO_BOX (squeek_cb)) + 1);
    }

    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (squeekop_cb));
    if (cptr)
    {
        if (orig_sop && g_strcmp0 (orig_sop, cptr)) vsystem (SET_SQUEEKOUT, cptr);
        g_free (cptr);
    }

    if (!vsystem (IS_PI))
    {
        if (wm == WM_OPENBOX)
        {
            if (gtk_combo_box_get_active (GTK_COMBO_BOX (vnc_res_cb)) != orig_vnc_res)
            {
                cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (vnc_res_cb));
                vsystem (SET_VNC_RES, cptr);
                g_free (cptr);
                reboot = TRUE;
            }
        }
    }

    return reboot;
}

/*----------------------------------------------------------------------------*/
/* Reboot check                                                               */
/*----------------------------------------------------------------------------*/

gboolean display_reboot (void)
{
    if (wm == WM_OPENBOX)
    {
        CHECK_SWITCH (blank_sw, orig_blank);
        if (!vsystem (IS_PI) && orig_vnc_res != gtk_combo_box_get_active (GTK_COMBO_BOX (vnc_res_cb)))
            return TRUE;
    }

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Tab setup                                                                  */
/*----------------------------------------------------------------------------*/

void load_display_tab (GtkBuilder *builder)
{
    char *line, *cptr;
    size_t len;
    FILE *fp;
    int op;

    /* Blanking switch */
    CONFIG_SWITCH (blank_sw, "sw_blank", orig_blank, GET_BLANK);
    HANDLE_SWITCH (blank_sw, SET_BLANK);
    if (!vsystem (XSCR_INSTALLED))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (blank_sw), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (blank_sw), _("This setting is overridden when Xscreensaver is installed"));
    }

    /* Overscan switches */
    CONFIG_SWITCH (overscan_sw, "sw_os1", orig_overscan, GET_OVERSCAN);
    CONFIG_SWITCH (overscan2_sw, "sw_os2", orig_overscan2, GET_OVERSCAN2);
    HANDLE_SWITCH (overscan_sw, SET_OVERSCAN);
    HANDLE_SWITCH (overscan2_sw, SET_OVERSCAN2);

    if (wm == WM_OPENBOX)
    {
        /* Set overscan switches for number of monitors */
        if (num_screens () != 2) gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox53")));
        else gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "label52")), _("Overscan (HDMI-1):"));

        /* Hide squeekboard */
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox57")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox58")));
    }
    else
    {
        /* Hide overscan */
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox52")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox53")));

        /* Squeekboard enable */
        squeek_cb = gtk_builder_get_object (builder, "cb_squeek");
        orig_squeek = get_status (GET_SQUEEK);
        gtk_combo_box_set_active (GTK_COMBO_BOX (squeek_cb), orig_squeek);
        HANDLE_CONTROL (squeek_cb, "changed", on_squeekboard_set);

        /* Squeekboard output */
        squeekop_cb = gtk_builder_get_object (builder, "cb_squeekout");
        orig_sop = get_string (GET_SQUEEKOUT);
        fp = popen ("wlr-randr", "r");
        if (fp)
        {
            op = 0;
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
        HANDLE_CONTROL (squeekop_cb, "changed", on_squeek_output_set);

        if (vsystem (VKBD_INSTALLED))
        {
            gtk_widget_set_sensitive (GTK_WIDGET (squeek_cb), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (squeek_cb), _("A virtual keyboard is not installed"));
            gtk_widget_set_sensitive (GTK_WIDGET (squeekop_cb), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (squeekop_cb), _("A virtual keyboard is not installed"));
        }
    }

    /* VNC resolution */
    if (!vsystem (IS_PI) && wm == WM_OPENBOX)
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
        cptr = get_string (GET_VNC_RES);
        if (!strcmp (cptr, "640x480")) orig_vnc_res = 0;
        if (!strcmp (cptr, "720x480")) orig_vnc_res = 1;
        if (!strcmp (cptr, "800x600")) orig_vnc_res = 2;
        if (!strcmp (cptr, "1024x768")) orig_vnc_res = 3;
        if (!strcmp (cptr, "1280x720")) orig_vnc_res = 4;
        if (!strcmp (cptr, "1280x1024")) orig_vnc_res = 5;
        if (!strcmp (cptr, "1600x1200")) orig_vnc_res = 6;
        if (!strcmp (cptr, "1920x1080")) orig_vnc_res = 7;
        g_free (cptr);

        gtk_combo_box_set_active (GTK_COMBO_BOX (vnc_res_cb), orig_vnc_res);
        HANDLE_CONTROL (vnc_res_cb, "changed", on_vnc_res_set)
    }
    else gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox56")));
}

/* End of file */
/*----------------------------------------------------------------------------*/
