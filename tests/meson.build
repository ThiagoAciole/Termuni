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
  '../src/termac-settings.c',
  '../src/termac-window.c',
  '../src/termac-terminal.c',
  '../src/termac-vt.c',
  '../src/termac-util.c',
  '../src/termac-attachments.c',
  '../src/termac-agent.c',
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
  test('ui-hidpi', xvfb_run, depends: schema_build, args: ['-a', test_ui], timeout: 300, env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11', 'GDK_SCALE=2', 'GDK_DPI_SCALE=1.25'])
  test('ui', xvfb_run, depends: schema_build, args: ['-a', test_ui], timeout: 240, env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11'])
else
  test('ui', test_ui, depends: schema_build, timeout: 240, env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11'])
endif

test_settings = executable('test-settings',
  ['test-settings.c', '../src/termac-settings.c'],
  dependencies: dependency('gio-2.0'),
  include_directories: include_directories('../src'),
)
test('settings', test_settings, depends: schema_build, env: ['GSETTINGS_BACKEND=memory'])

test_agent = executable('test-agent',
  ['test-agent.c', '../src/termac-agent.c', '../src/termac-attachments.c', '../src/termac-util.c'],
  dependencies: dependency('gio-2.0'),
  include_directories: include_directories('../src'),
)
test('agent', test_agent)

test('agent-names-contained', find_program('python3'),
  args: [files('../tools/check-agent-boundary.py'), meson.project_source_root() / 'src'],
)
