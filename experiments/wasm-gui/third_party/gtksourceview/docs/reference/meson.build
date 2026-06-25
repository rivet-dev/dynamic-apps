glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

reference_private_h = [
  'config.h',
  'gtksource.h',
  'gtksourcebuffer-private.h',
  'gtksourcebufferinputstream.h',
  'gtksourcebufferinternal.h',
  'gtksourcebufferoutputstream.h',
  'gtksourcecompletioncontainer.h',
  'gtksourcecompletionmodel.h',
  'gtksourcecompletion-private.h',
  'gtksourcecompletionwordsbuffer.h',
  'gtksourcecompletionwordslibrary.h',
  'gtksourcecompletionwordsproposal.h',
  'gtksourcecompletionwordsutils.h',
  'gtksourcecontextengine.h',
  'gtksourceencoding-private.h',
  'gtksourceengine.h',
  'gtksourcegutter-private.h',
  'gtksourcegutterrendererlines.h',
  'gtksourcegutterrenderermarks.h',
  'gtksourcegutterrenderer-private.h',
  'gtksourceiter.h',
  'gtksourcelanguage-private.h',
  'gtksourcemarkssequence.h',
  'gtksourcepixbufhelper.h',
  'gtksourceregex.h',
  'gtksourcestyle-private.h',
  'gtksourcetypes-private.h',
  'gtksourceundomanagerdefault.h',
  'gtksourceutils-private.h',
]

reference_content_files = files([
  'lang-tutorial.xml',
  'porting-guide-3-to-4.xml',
])

reference_sources = [
  join_paths(srcdir, 'gtksourceview'),
  join_paths(builddir, 'gtksourceview'),
]

into_xml = configure_file(
         input: 'intro.xml.in',
         output: 'intro.xml',
  configuration: config_h
)

lang_reference_xml = configure_file(
          input: 'lang-reference.xml.in',
         output: 'lang-reference.xml',
  configuration: config_h
)

style_reference_xml = configure_file(
          input: 'style-reference.xml.in',
         output: 'style-reference.xml',
  configuration: config_h
)

reference_content_files += [
  into_xml,
  lang_reference_xml,
  style_reference_xml,
]

gtksourceview_docs_xml = configure_file(
          input: 'gtksourceview-docs.xml.in',
         output: 'gtksourceview-docs.xml',
  configuration: config_h
)

reference_scan_args = [
  '--rebuild-types',
  '--ignore-decorators=GTK_SOURCE_DEPRECATED\w*\s*\([^)]*\)|GTK_SOURCE_DEPRECATED\w*|GTK_SOURCE_AVAILABLE\w*|_GTK_SOURCE_EXTERN',
  '--deprecated-guards=GTKSOURCEVIEW_DISABLE_DEPRECATED',
  '--ignore-headers=' + ' '.join(reference_private_h),
]

reference_fixxref_args = [
  '--html-dir=@0@'.format(docpath),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
  '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gtk')),
]

gnome.gtkdoc('@0@.0'.format(package_string),
           main_xml: join_paths(builddir, '@0@'.format(gtksourceview_docs_xml)),
  gobject_typesfile: 'gtksourceview-4.0.types',
            src_dir: reference_sources,
       dependencies: gtksource_dep,
      content_files: reference_content_files,
          scan_args: reference_scan_args,
       fixxref_args: reference_fixxref_args,
            install: true
)
