#!/bin/bash
export TEXTDOMAIN=rc_gui

. gettext.sh

zenity --password --title "$(gettext "Password Required")"

