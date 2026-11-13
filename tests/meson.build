test_util = executable('test-util',
  ['test-util.c', '../src/termac-util.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: include_directories('../src'),
)
test('util', test_util)

test_vt = executable('test-vt',
  ['test-vt.c', '../src/termac-vt.c', '../src/termac-util.c'],
  dependencies: [dependency('glib-2.0'), ghostty_vt_dep],
  include_directories: include_directories('../src'),
)
test('vt', test_vt)

test_attachments = executable('test-attachments',
  ['test-attachments.c', '../src/termac-attachments.c', '../src/termac-util.c'],
  dependencies: [dependency('glib-2.0'), dependency('gio-2.0')],
  include_directories: include_directories('../src'),
)
test('attachments', test_attachments)

ui_sources = [
  'test-ui.c',
  '../src/termac-application.c',
  '../src/termac-window.c',
  '../src/termac-terminal.c',
  '../src/termac-vt.c',
  '../src/termac-util.c',
  '../src/termac-attachments.c',
]
ui_sources += gnome.compile_resources('termac-ui-resources',
  '../src/termac.gresource.xml',
  c_name: 'termac',
  source_dir: ['../src', '../assets'],
)
test_ui = executable('test-ui', ui_sources,
  dependencies: [
    dependency('gtk4'),
    dependency('libadwaita-1', version: '>= 1.8'),
    dependency('pangocairo'),
    ghostty_vt_dep,
    cc.find_library('util', required: false),
    dependency('x11'),
    cc.find_library('dl'),
  ],
  include_directories: include_directories('../src'),
)
xvfb_run = find_program('xvfb-run', required: false)
if xvfb_run.found()
  test('ui', xvfb_run, args: ['-a', test_ui], timeout: 180, env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11'])
else
  test('ui', test_ui, timeout: 180, env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11'])
endif
