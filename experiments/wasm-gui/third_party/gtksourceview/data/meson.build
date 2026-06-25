if install_glade_catalog
  subdir('glade')
endif

install_subdir('language-specs',
    install_dir: pkgdatadir,
  exclude_files: [ 'Makefile.am',
                   'check-language.sh',
                   'lang_v1_to_v2.xslt',
                   'language-specs.its',
                   'language-specs.pot',
                   'testv1.lang',
                   'ue2gsv.pl',
                   'update-pot.sh' ]
)

install_subdir('styles',
    install_dir: pkgdatadir,
  exclude_files: [ 'Makefile.am',
                   'check-style.sh',
                   'styles.its',
                   'styles.pot',
                   'testdark.xml',
                   'update-pot.sh' ]
)
