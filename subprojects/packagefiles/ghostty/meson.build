project('ghostty-vt-build', 'c', meson_version: '>= 1.0.0')

zig = find_program('zig', required: true)
build_script = find_program(meson.current_source_dir() / 'build-ghostty-vt.sh')
ghostty_prefix = meson.current_build_dir() / 'zig-prefix'
run_command('mkdir', '-p', ghostty_prefix / 'include', check: true)
ghostty_stamp = custom_target(
  'libghostty-vt-build',
  output: 'libghostty-vt.stamp',
  command: [build_script, zig, meson.current_source_dir(), ghostty_prefix, meson.current_build_dir() / 'libghostty-vt.stamp'],
  depend_files: files('build.zig', 'build.zig.zon', 'src/lib_vt.zig'),
  build_by_default: true,
)
ghostty_vt_dep = declare_dependency(
  compile_args: ['-I' + ghostty_prefix / 'include'],
  link_args: [ghostty_prefix / 'lib' / 'libghostty-vt.a'],
  sources: ghostty_stamp,
)
