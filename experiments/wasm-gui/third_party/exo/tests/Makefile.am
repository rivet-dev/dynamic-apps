
AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"exo-tests\"

TESTS =									\
	test-exo-noop							\
	test-exo-string

check_PROGRAMS =							\
	test-exo-noop							\
	test-exo-string							\
	test-exo-icon-chooser-dialog

test_exo_noop_SOURCES =							\
	test-exo-noop.c

test_exo_noop_CFLAGS =                                                 \
	$(GTK_CFLAGS)                                                  \
	$(LIBXFCE4UTIL_CFLAGS)

test_exo_noop_DEPENDENCIES =						\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

test_exo_noop_LDADD =							\
	$(GLIB_LIBS)							\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

test_exo_string_SOURCES =						\
	test-exo-string.c

test_exo_string_CFLAGS =                                               \
	$(GTK_CFLAGS)                                                  \
	$(LIBXFCE4UTIL_CFLAGS)

test_exo_string_DEPENDENCIES =						\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

test_exo_string_LDADD =							\
	$(GLIB_LIBS)							\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

test_exo_icon_chooser_dialog_SOURCES =					\
	test-exo-icon-chooser-dialog.c

test_exo_icon_chooser_dialog_DEPENDENCIES =				\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

test_exo_icon_chooser_dialog_CFLAGS =					\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)

test_exo_icon_chooser_dialog_LDADD =					\
	$(GTK_LIBS)							\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
