progs = ['notify-send']

foreach prog: progs
  executable(prog, [prog + '.c'],
    dependencies: libnotify_dep,
    install: true)
endforeach
