if build_standalone
  install_data('rc_gui.ui', install_dir: ui_dir)
  i18n.merge_file(input: 'rc_gui.desktop.in',
    output: 'rc_gui.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: desktop_dir
  )
  install_data('pwdrcg.sh', install_dir: resource_dir)
endif

if build_plugin
  install_data('rc_gui.ui', install_dir: pui_dir)
  install_data('pwdrcg.sh', install_dir: presource_dir)
endif
