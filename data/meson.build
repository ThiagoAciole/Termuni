desktop_conf = configuration_data()
desktop_conf.set('application_id', application_id)
desktop_conf.set('application_icon', application_id)
desktop_template = configure_file(
        input: 'thiago.aciole.termac.desktop.in',
       output: 'termac.desktop.in',
 configuration: desktop_conf,
)
desktop_file = i18n.merge_file(
        input: desktop_template,
       output: '@0@.desktop'.format(application_id),
         type: 'desktop',
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'applications'
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif

appstream_conf = configuration_data()
appstream_conf.set('application_id', application_id)
appstream_template = configure_file(
        input: 'thiago.aciole.termac.metainfo.xml.in',
       output: 'termac.metainfo.xml.in',
 configuration: appstream_conf,
)
appstream_file = i18n.merge_file(
        input: appstream_template,
        output: '@0@.metainfo.xml'.format(application_id),
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'metainfo'
)

appstreamcli = find_program('appstreamcli', required: false, disabler: true)
test('Validate appstream file', appstreamcli,
     # This local development project has no published homepage yet.
     args: ['validate', '--no-net', '--explain', '--override=url-homepage-missing=info', appstream_file])

schema_conf = configuration_data()
schema_conf.set('application_id', application_id)
schema_conf.set('settings_path', '/' + application_id.replace('.', '/') + '/')
schema_file = configure_file(
  input: 'thiago.aciole.termac.gschema.xml.in',
  output: '@0@.gschema.xml'.format(application_id),
  configuration: schema_conf,
)
install_data(schema_file,
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas'
)

schema_build = custom_target('termac-gsettings-schema',
  input: schema_file,
  output: 'gschemas.compiled',
  command: [find_program('glib-compile-schemas'), '--strict',
            '--targetdir', meson.current_build_dir(), meson.current_build_dir()],
  build_by_default: true,
)

compile_schemas = find_program('glib-compile-schemas', required: false, disabler: true)
test('Validate schema file',
     compile_schemas,
     args: ['--strict', '--dry-run', meson.current_build_dir()])


service_conf = configuration_data()
service_conf.set('bindir', get_option('prefix') / get_option('bindir'))
service_conf.set('application_id', application_id)
configure_file(
  input: 'thiago.aciole.termac.service.in',
  output: '@0@.service'.format(application_id),
  configuration: service_conf,
  install_dir: get_option('datadir') / 'dbus-1' / 'services'
)

subdir('icons')
