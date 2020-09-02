# rc_gui

GUI for configuring Raspberry Pi settings. (Including Raspberry Pi Desktop for PC/Mac).

Uses the command-line equivalent, [`raspi-config`](https://github.com/RPi-Distro/raspi-config), internally for some operations.

## Building

Install prerequisites:

```bash
sudo apt update
sudo apt install autoconf libglib2.0-dev autogen libtool intltool libgtk2.0-dev
```

Configure and build:

```bash
./autogen.sh
./configure
make
```

Install data files:

```bash
sudo make install
```
