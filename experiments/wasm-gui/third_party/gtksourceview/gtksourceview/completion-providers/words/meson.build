completionwords_c_args = [
  '-DHAVE_CONFIG_H',
  '-DGTK_SOURCE_COMPILATION',
  '-DG_LOG_DOMAIN="GtkSourceView"',
]

completionwords_public_h = [
  'gtksourcecompletionwords.h',
]

completionwords_public_c = [
  'gtksourcecompletionwords.c',
  'gtksourcecompletionwordsbuffer.c',
  'gtksourcecompletionwordslibrary.c',
  'gtksourcecompletionwordsproposal.c',
  'gtksourcecompletionwordsutils.c',
]

install_headers(
  completionwords_public_h,
  install_dir: join_paths(
    pkgincludedir,
    'gtksourceview',
    'completion-providers',
    'words'
  )
)

completionwords_lib = static_library(
  package_string + 'completionwords',
  completionwords_public_c,
  include_directories: rootdir,
  dependencies: core_dep,
  c_args: completionwords_c_args,
  install: false,
)

gtksource_libs += [
  completionwords_lib
]

completionwords_dep = declare_dependency(
  link_with: completionwords_lib,
  include_directories: rootdir,
  dependencies: core_dep,
)

gtksource_deps += completionwords_dep

extra_public_sources += files([
  'gtksourcecompletionwords.c',
  'gtksourcecompletionwords.h',
])
