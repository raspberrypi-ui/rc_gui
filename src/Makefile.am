bin_PROGRAMS = rc_gui

rc_gui_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/rc-gui"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/rc-gui/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall

rc_gui_SOURCES = rc_gui.c

rc_gui_includedir = $(includedir)/rc-gui

rc_gui_include_HEADERS =

rc_gui_LDFLAGS = $(DYNAMIC_FLAGS) -lcrypt
rc_gui_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

rc_gui_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

EXTRA_DIST =
