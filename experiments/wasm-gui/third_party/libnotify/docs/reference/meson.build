api_version_major = '0'
expand_content_md_files = []

toml_data = configuration_data()
toml_data.set('VERSION', meson.project_version())

libnotify_toml = configure_file(
  input: 'libnotify.toml.in',
  output: 'libnotify.toml',
  configuration: toml_data
)

dependency('gi-docgen', version: '>= 2021.7',
           fallback: ['gi-docgen', 'dummy_dep'],
           native: true,
           required: get_option('gtk_doc'))

gidocgen = find_program('gi-docgen')

docs_dir = get_option('datadir') / 'doc'

custom_target('libnotify-doc',
  input: [ libnotify_toml, libnotify_gir[0] ],
  output: 'libnotify-@0@'.format(api_version_major),
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../libnotify'),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depend_files: [ expand_content_md_files ],
  build_by_default: true,
  install: true,
  install_dir: docs_dir,
)
