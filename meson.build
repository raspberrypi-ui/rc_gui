project ('rc_gui','c')

build_standalone = false
build_plugin = true
plugin_name = 'rpcc_' + meson.project_name()

share_dir = join_paths(get_option('prefix'), 'share')
resource_dir = join_paths(share_dir, meson.project_name())
presource_dir = join_paths(share_dir, 'rpcc')
ui_dir = join_paths(resource_dir, 'ui')
pui_dir = join_paths(presource_dir, 'ui')
desktop_dir = join_paths(share_dir, 'applications')

i18n = import('i18n')

add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : 'c' )
add_project_link_arguments('-lcrypt', language : 'c' )

subdir('po')
subdir('src')
subdir('data')
