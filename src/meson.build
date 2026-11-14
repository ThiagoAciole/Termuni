termac_sources = [
  'main.c',
  'termac-application.c',
  'termac-session-store.c',
  'termac-settings.c',
  'termac-window.c',
  'termac-terminal.c',
  'termac-vt.c',
  'termac-util.c',
  'termac-attachments.c',
  'termac-agent.c',
]

termac_deps = [
  dependency('gtk4', version: '>= 4.22'),
  dependency('libadwaita-1', version: '>= 1.8'),
  dependency('pangocairo'),
  ghostty_vt_dep,
  cc.find_library('util', required: false),
]

termac_sources += gnome.compile_resources('termac-resources',
  'termac.gresource.xml',
  c_name: 'termac',
  source_dir: ['.', '../assets'],
)

executable('termac', termac_sources,
  dependencies: termac_deps,
       install: true,
)
