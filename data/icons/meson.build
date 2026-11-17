scalable_dir = 'hicolor' / 'scalable' / 'apps'
# Install a square canvas while keeping the original SVG artwork intact.
configure_file(
  input: scalable_dir / ('@0@.svg').format(application_id),
  output: application_id + '.svg',
  command: [find_program('python3'), files('../../tools/prepare-icon.py'), '@INPUT@', '@OUTPUT@'],
  install_dir: get_option('datadir') / 'icons' / scalable_dir,
)

symbolic_dir = 'hicolor' / 'symbolic' / 'apps'
install_data(
  symbolic_dir / ('@0@-symbolic.svg').format(application_id),
  install_dir: get_option('datadir') / 'icons' / symbolic_dir
)
