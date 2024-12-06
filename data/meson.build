install_data('rc_gui.ui', install_dir: ui_dir)
if not meson.project_name().contains('rpcc')
  i18n.merge_file(input: 'rc_gui.desktop.in',
      output: 'rc_gui.desktop',
      type: 'desktop',
      po_dir: '../po',
      install: true,
      install_dir: desktop_dir
    )
endif
install_data('pwdrcg.sh', install_dir: resource_dir)
