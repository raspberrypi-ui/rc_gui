This is an application which allows the configuration of Raspberry Pi system settings.

# How to build #
## 1. Install dependencies ##

The dependencies of any Debian project are listed in the [“Build-Depends”](debian/control#L5)
section of the file [`debian/control`](debian/control) of this project.

If the project has already been released into apt, then the build dependencies
can be automatically installed using the command `sudo apt build-dep <package-name>`.

## 2. Prepare project ##
To create the `configure` file and prepare the project directory, use the command
`./autogen.sh` in the top directory of the project.

If this file is not present, then instead use the command `autoreconf -i -f`
followed by the command `intltoolize -c --automake --force`, both in the top directory
of the project.

## 3. Configure ##
To configure the make system, use the command `./configure` in the top directory of
the project. This will by default set the project for installation in the `/usr/local` tree,
which will not overwrite a version which has been installed from apt.

If you wish to overwrite a preinstalled version in the `/usr` tree, supply the arguments
`./configure --prefix=/usr --libdir=/usr/lib/<library-location>` to the configure command.
On a 32-bit system, <library-location> should be `arm-linux-gnueabihf`.
On a 64-bit system, <library-location> should be `aarch64-linux-gnu`.

## 4. Build ##
To build the application, use the command `make` in the top directory of the project.

## 5. Install ##
To install the application and all required data files, use the command `sudo make install`
in the top directory of the project.
