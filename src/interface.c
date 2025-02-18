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
#define GET_RGPIO       GET_PREFIX "get_rgpio"
#define SET_RGPIO       SET_PREFIX "do_rgpio %d"
#define RVNC_INSTALLED  GET_PREFIX "is_installed realvnc-vnc-server"
#define WVNC_INSTALLED  GET_PREFIX "is_installed wayvnc"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GObject *ssh_sw, *vnc_sw, *spi_sw, *i2c_sw, *serial_sw, *scons_sw, *onewire_sw, *rgpio_sw;
static int orig_ssh, orig_vnc, orig_spi, orig_i2c, orig_serial, orig_scons, orig_onewire, orig_rgpio;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void serial_update (void);
static gboolean on_serial_toggle (GtkSwitch *btn, gboolean state, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Control handling                                                           */
/*----------------------------------------------------------------------------*/

static void serial_update (void)
{
    if (gtk_switch_get_active (GTK_SWITCH (serial_sw)))
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
}

static gboolean on_serial_toggle (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    serial_update ();

#ifdef REALTIME
    vsystem (SET_SERIALHW, (1 - state));
#endif

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Exit processing                                                            */
/*----------------------------------------------------------------------------*/

gboolean read_interfacing_tab (void)
{
    gboolean reboot = FALSE;

    READ_SWITCH (ssh_sw, orig_ssh, SET_SSH, FALSE);
    READ_SWITCH (vnc_sw, orig_vnc, SET_VNC, FALSE);

    if (!vsystem (IS_PI))
    {
        READ_SWITCH (spi_sw, orig_spi, SET_SPI, FALSE);
        READ_SWITCH (i2c_sw, orig_i2c, SET_I2C, FALSE);
        READ_SWITCH (onewire_sw, orig_onewire, SET_1WIRE, TRUE);
        READ_SWITCH (rgpio_sw, orig_rgpio, SET_RGPIO, FALSE);
        READ_SWITCH (serial_sw, orig_serial, SET_SERIALHW, TRUE);
        READ_SWITCH (scons_sw, orig_scons, SET_SERIALCON, TRUE);
     }

     return reboot;
}

/*----------------------------------------------------------------------------*/
/* Reboot check                                                               */
/*----------------------------------------------------------------------------*/

gboolean interfacing_reboot (void)
{
    if (!vsystem (IS_PI))
    {
        CHECK_SWITCH (onewire_sw, orig_onewire);
        CHECK_SWITCH (serial_sw, orig_serial);
        CHECK_SWITCH (scons_sw, orig_scons);
    }

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Tab setup                                                                  */
/*----------------------------------------------------------------------------*/

void load_interfacing_tab (GtkBuilder *builder)
{
    /* SSH switch */
    CONFIG_SWITCH (ssh_sw, "sw_ssh", orig_ssh, GET_SSH);
    HANDLE_SWITCH (ssh_sw, SET_SSH);

    /* VNC switch */
    CONFIG_SWITCH (vnc_sw, "sw_vnc", orig_vnc, GET_VNC);
    HANDLE_SWITCH (vnc_sw, SET_VNC);
    if (vsystem (RVNC_INSTALLED) && vsystem (WVNC_INSTALLED))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (vnc_sw), FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (vnc_sw), _("The VNC server is not installed"));
    }

    if (!vsystem (IS_PI))
    {
        /* SPI switch */
        CONFIG_SWITCH (spi_sw, "sw_spi", orig_spi, GET_SPI);
        HANDLE_SWITCH (spi_sw, SET_SPI);

        /* I2C switch */
        CONFIG_SWITCH (i2c_sw, "sw_i2c", orig_i2c, GET_I2C);
        HANDLE_SWITCH (i2c_sw, SET_I2C);

        /* 1-wire interface switch */
        CONFIG_SWITCH (onewire_sw, "sw_one", orig_onewire, GET_1WIRE);
        HANDLE_SWITCH (onewire_sw, SET_1WIRE);

        /* Remote GPIO switch */
        CONFIG_SWITCH (rgpio_sw, "sw_rgp", orig_rgpio, GET_RGPIO);
        HANDLE_SWITCH (rgpio_sw, SET_RGPIO);

        /* Serial console switch */
        CONFIG_SWITCH (scons_sw, "sw_serc", orig_scons, GET_SERIALCON);
        HANDLE_SWITCH (scons_sw, SET_SERIALCON);

        /* Serial hardware switch */
        CONFIG_SWITCH (serial_sw, "sw_ser", orig_serial, GET_SERIALHW);
        if (!vsystem (IS_PI5))
        {
            HANDLE_SWITCH (serial_sw, SET_SERIALHW);
        }
        else
        {
            g_signal_connect (serial_sw, "state-set", G_CALLBACK (on_serial_toggle), NULL);
            serial_update ();
        }
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox24")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox26")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox27")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox28")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox29")));
    }
}

/* End of file */
/*----------------------------------------------------------------------------*/
