docbook_docs = get_option('docbook_docs')
if not docbook_docs.disabled()
  xmlto = find_program('xmlto', required: docbook_docs.enabled())

  if xmlto.found()
    spec_files = ['notification-spec']

    foreach spec: spec_files
      custom_target(spec + '.html',
        output: spec + '.html',
        input: spec + '.xml',
        depend_files: 'config.xsl',
        command: [xmlto,
          'xhtml-nochunks',
          '-m', join_paths(meson.current_source_dir(), 'config.xsl'),
          '-o', '@OUTDIR@',
          '@INPUT@'
        ],
        install: true,
        install_dir: join_paths(docdir, 'spec'))
    endforeach
  endif
endif

if get_option('gtk_doc')
  subdir('reference')
endif

if get_option('man')
  manpages = ['notify-send']

  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.xml',
      output: page + '.1',
      command: xsltproc_command,
      install: true,
      install_dir: man1dir)
  endforeach
endif
