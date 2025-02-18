# To build standalone, use 'rc_gui'
project ('rc_gui','c')
# To build plugin, use 'rpcc_rc_gui'
#project ('rpcc_rc_gui','c')

share_dir = join_paths(get_option('prefix'), 'share')
if meson.project_name().contains('rpcc')
  resource_dir = join_paths(share_dir, 'rpcc')
else
  resource_dir = join_paths(share_dir, meson.project_name())
endif
ui_dir = join_paths(resource_dir, 'ui')
desktop_dir = join_paths(share_dir, 'applications')

i18n = import('i18n')
gettext_package = meson.project_name()

add_project_arguments('-DGETTEXT_PACKAGE="' + gettext_package + '"', language : 'c' )
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : 'c' )
add_project_arguments('-DPACKAGE_DATA_DIR="' + resource_dir + '"', language : 'c' )
add_project_link_arguments('-lcrypt', language : 'c' )

subdir('po')
subdir('src')
subdir('data')
