sources = files (
    'rc_gui.c',
    'system.c',
    'display.c',
    'interface.c',
    'perform.c',
    'local.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
deps = [ gtk ]

shared_module(plugin_name, sources, dependencies: deps, install: true,
  install_dir: get_option('libdir') / 'rpcc',
  c_args : [ '-DPACKAGE_DATA_DIR="' + presource_dir + '"', '-DGETTEXT_PACKAGE="' + plugin_name + '"', '-DPLUGIN_NAME="' + plugin_name + '"' ]
)
