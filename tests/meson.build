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
