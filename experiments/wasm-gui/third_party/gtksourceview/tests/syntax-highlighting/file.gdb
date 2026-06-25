Reading symbols from /home/foo/a.out...done.
Starting program: /home/foo/a.out 

Program received signal SIGSEGV, Segmentation fault.
0x00000000004007e2 in baz ()
Thread 2 (Thread 0x7fffe5fc1700 (LWP 24296)):
#0  0x00007ffff2ac984d in poll () at ../sysdeps/unix/syscall-template.S:81
#1  0x00007ffff3d16ee4 in g_main_context_poll (priority=2147483647, n_fds=1, fds=0x7fffe00010e0, timeout=-1, context=0x8d6320) at /build+thing/buildd/glib2.0-2.42.1/./glib/gmain.c:4076
#2  g_main_context_iterate (context=context@entry=0x8d6320, block=block@entry=1, dispatch=dispatch@entry=1, self=<optimized out>) at /build+thing/buildd/glib2.0-2.42.1/./glib/gmain.c:3776
#3  0x00007ffff3d16ffc in g_main_context_iteration (context=0x8d6320, 
    may_block=1) at /build+thing/buildd/glib2.0-2.42.1/./glib/gmain.c:3842
#4  0x00007fffe5fc927d in ?? () from /usr/lib/x86_64-linux-gnu/gio/modules/libdconfsettings.so
#5  0x00007ffff3d3d925 in g_thread_proxy (data=0x737140) 
    at /build~stuff/buildd/glib2.0-2.42.1/./glib/gthread.c:764
#6  0x00007ffff2da60a5 in start_thread (arg=0x7fffe5fc1700) at pthread_create.c:309
#7  0x00007ffff2ad3cfd in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
Thread 1 (Thread 0x7ffff7fa1a40 (LWP 24267)):
#0  0x00007ffff74007e2 in baz ()
#1  0x00007ffff74009f2 in bar (str=0x7ffff3412d85 "hello")
#2  0x00007ffff74003f2 in func2 () from /tmp/ничего/libnothing1.so
#3  0x00007ffff74013f2 in func1 () from /tmp/何も/libnothing2.so
#4  0x00007ffff74007c4 in main ()

Load new symbol table from "/home/foo/b.out"? (y or n) Reading symbols from /home/foo/b.out...done.
Starting program: /home/foo/b.out 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New process 9943]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New Thread 0x7ffff77f7700 (LWP 9944)]
[Thread 0x7ffff77f7700 (LWP 9944) exited]
[Inferior 1 (process 9943) exited with code 01]
[New Thread 0x7ffff77f7700 (LWP 9947)]

Program received signal SIGABRT, Aborted.
[Switching to Thread 0x7ffff77f7700 (LWP 9947)]
0x00007ffff782ee37 in __GI_raise (sig=sig@entry=6)
    at ../nptl/sysdeps/unix/sysv/linux/raise.c:56
56	../nptl/sysdeps/unix/sysv/linux/raise.c: No such file or directory.

Load new symbol table from "/home/foo/c.out"? (y or n) Reading symbols from /home/foo/c.out...done.
Breakpoint 1 at 0x40086d: file test-gdb.c, line 30.
Starting program: /home/foo/c.out 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".

Breakpoint 1, foo1 () at test-gdb.c:30
warning: Source file is more recent than executable.
30		usleep(2500);

Program received signal SIGTTIN, Stopped (tty input).
foo1 () at test-gdb.c:30
30		usleep(2500);
31		exit(1);
#0  foo1 () at test-gdb.c:31
#1  0x00000000004008bd in main () at test-gdb.c:40
The program being debugged has been started already.
Start it from the beginning? (y or n) Program not restarted.
Continuing.
[Inferior 1 (process 10076) exited with code 01]
quit
