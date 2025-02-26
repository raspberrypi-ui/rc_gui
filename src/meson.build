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

if build_plugin
  shared_module(plugin_name, sources, dependencies: deps, install: true,
    install_dir: get_option('libdir') / 'rpcc',
    c_args : [ '-DPACKAGE_DATA_DIR="' + presource_dir + '"', '-DPLUGIN_NAME="' + plugin_name + '"' ],
    link_args : '-Wl,-z,defs'
  )
endif

if build_standalone
  executable (meson.project_name(), sources, dependencies: deps, install: true,
    c_args : '-DPACKAGE_DATA_DIR="' + resource_dir + '"'
  )
endif
