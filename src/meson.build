termuni_sources = [
  'main.c',
  'termuni-application.c',
  'termuni-session-store.c',
  'termuni-settings.c',
  'termuni-window.c',
  'termuni-terminal.c',
  'termuni-vt.c',
  'termuni-util.c',
  'termuni-attachments.c',
  'termuni-agent.c',
]

termuni_deps = [
  dependency('gtk4', version: '>= 4.22'),
  dependency('libadwaita-1', version: '>= 1.8'),
  dependency('pangocairo'),
  ghostty_vt_dep,
  cc.find_library('util', required: false),
]

termuni_sources += gnome.compile_resources('termuni-resources',
  'termuni.gresource.xml',
  c_name: 'termuni',
  source_dir: ['.', '../assets'],
)

executable('termuni', termuni_sources,
  dependencies: termuni_deps,
       install: true,
)
