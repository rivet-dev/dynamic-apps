# Note that we do not install in the detected GLADE_CATALOG_DIR
# since we may not have permissions to do it, for instance if
# compiling with a prefix while glade is installed in /usr.
# We use the variable only to find the dtd file.
# If you install gtksourceview and glade in different prefixes
# and want to use the catalog in glade, you will need to set
# the GLADE_CATALOG_SEARCH_PATH environment variable.

catalog_in = configuration_data()
catalog_in.set('GSV_API_VERSION', api_version)

catalog_xml = configure_file(
          input: 'gtksourceview.xml.in',
         output: 'gtksourceview.xml',
  configuration: catalog_in,
    install_dir: join_paths(datadir, 'glade', 'catalogs')
)

# Currently DTD validation does not work with glade's DTD.
# This does not appear to be limited to GtkSourceView.
#if xmllint_prg.found()
#  catalog_dtd = join_paths(
#    gladeui_dep.get_pkgconfig_variable('catalogdir'),
#   'glade-catalog.dtd'
#  )
#
#  test(
#    'Validate Glade catalogue',
#    xmllint_prg,
#    args: [
#      '--dtdvalid',
#      catalog_dtd,
#      catalog_xml,
#      '--noout',
#    ]
#  )
#endif
