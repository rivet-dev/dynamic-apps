AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-taskmanager\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

bin_PROGRAMS =								\
	xfce4-taskmanager

xfce4_taskmanager_CFLAGS =						\
	$(CAIRO_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXMU_CFLAGS)						\
	$(GTK3_CFLAGS)							\
	$(WNCK_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)				\
	$(XFCONF_CFLAGS)						\
	$(NULL)

xfce4_taskmanager_LDADD = 						\
	$(CAIRO_LIBS)							\
	$(LIBX11_LIBS)						\
	$(LIBXMU_LIBS)						\
	$(GTK3_LIBS) 						\
	$(WNCK_LIBS)						\
	$(LIBXFCE4UI_LIBS)			\
	$(XFCONF_LIBS)					\
	$(NULL)

xfce4_taskmanager_SOURCES =						\
	main.c								\
	process-window_ui.h					\
	process-window.c		process-window.h		\
	process-monitor.c		process-monitor.h		\
	process-tree-model.c		process-tree-model.h		\
	process-tree-view.c		process-tree-view.h		\
	process-statusbar.c		process-statusbar.h		\
	settings-dialog_ui.h									\
	settings-dialog.c		settings-dialog.h		\
	settings.c			settings.h			\
	task-manager.c			task-manager.h			\
	$(NULL)

if HAVE_WNCK
xfce4_taskmanager_SOURCES += app-manager.c app-manager.h
endif

if OS_FREEBSD
xfce4_taskmanager_SOURCES += task-manager-freebsd.c
endif
if OS_BSD
xfce4_taskmanager_SOURCES += task-manager-bsd.c
endif
if OS_SOLARIS
xfce4_taskmanager_SOURCES += task-manager-solaris.c
endif
if OS_LINUX
xfce4_taskmanager_SOURCES += task-manager-linux.c
endif
if OS_SKEL
xfce4_taskmanager_SOURCES += task-manager-skel.c
endif

if MAINTAINER_MODE
BUILT_SOURCES = process-window_ui.h settings-dialog_ui.h
process-window_ui.h: process-window.ui
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=process_window_ui $< >$@
settings-dialog_ui.h: settings-dialog.ui
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=settings_dialog_ui $< >$@
endif

EXTRA_DIST = process-window.ui settings-dialog.ui
