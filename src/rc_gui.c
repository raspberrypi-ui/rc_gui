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

/* NOTE raspi-config nonint functions obey sh return codes - 0 is in general success / yes / selected, 1 is failed / no / not selected */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <glib/gstdio.h>

#include "rc_gui.h"

#include "system.h"
#include "display.h"
#include "interface.h"
#include "perform.h"
#include "local.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

GtkWidget *main_dlg, *msg_dlg;
GThread *pthread;
int needs_reboot;
wm_type wm;

#ifdef PLUGIN_NAME
GtkBuilder *builder;
#else
static gulong draw_id;
#endif

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static gboolean ok_clicked (GtkButton *button, gpointer data);
static gboolean close_app (GtkButton *button, gpointer data);
static gboolean close_app_reboot (GtkButton *button, gpointer data);
static gboolean reboot_prompt (gpointer data);
static gpointer process_changes_thread (gpointer ptr);
static gboolean cancel_main (GtkButton *button, gpointer data);
static gboolean ok_main (GtkButton *button, gpointer data);
static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean init_config (gpointer data);
#ifndef PLUGIN_NAME
static gboolean event (GtkWidget *wid, GdkEventWindowState *ev, gpointer data);
static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data);
#endif

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/* Helpers */

int vsystem (const char *fmt, ...)
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

gboolean on_switch (GtkSwitch *btn, gboolean state, const char *cmd)
{
    vsystem (cmd, (1 - state));
    return FALSE;
}

int get_status (char *cmd)
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

char *get_string (char *cmd)
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

char *get_quoted_param (char *path, char *fname, char *toseek)
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

void message (char *msg)
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

void info (char *msg)
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


/* Write the changes to the system when OK is pressed */

static gpointer process_changes_thread (gpointer ptr)
{
    int reboot = needs_reboot;
    
    if (read_system_tab ()) reboot = 1;
    if (read_display_tab ()) reboot = 1;
    if (read_interfacing_tab ()) reboot = 1;
    if (read_performance_tab ()) reboot = 1;

    if (reboot) g_idle_add (reboot_prompt, NULL);
    else gtk_main_quit ();

    return NULL;
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
    pthread = g_thread_new (NULL, process_changes_thread, NULL);
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
    return TRUE;
}

static gboolean init_config (gpointer data)
{
#ifndef PLUGIN_NAME
    GtkBuilder *builder;
#endif
    GtkWidget *wid;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rc_gui.ui");
    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    g_signal_connect (main_dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_cancel");
    g_signal_connect (wid, "clicked", G_CALLBACK (cancel_main), NULL);

    load_system_tab (builder);
    load_display_tab (builder);
    load_interfacing_tab (builder);
    load_performance_tab (builder);
    load_localisation_tab (builder);

#ifndef PLUGIN_NAME
    g_object_unref (builder);

    gtk_widget_show (main_dlg);
    gtk_widget_destroy (msg_dlg);
#endif

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Plugin interface */
/*----------------------------------------------------------------------------*/
#ifdef PLUGIN_NAME

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
    if (needs_reboot) return TRUE;

    if (system_reboot ()) return TRUE;
    if (display_reboot ()) return TRUE;
    if (interfacing_reboot ()) return TRUE;
    if (performance_reboot ()) return TRUE;

    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

#else

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

int main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

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

    message (_("Loading configuration - please wait..."));
    if (wm != WM_OPENBOX) g_signal_connect (msg_dlg, "event", G_CALLBACK (event), NULL);
    else draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    gtk_main ();

    if (main_dlg) gtk_widget_destroy (main_dlg);

    return 0;
}

#endif

/* End of file */
/*----------------------------------------------------------------------------*/
