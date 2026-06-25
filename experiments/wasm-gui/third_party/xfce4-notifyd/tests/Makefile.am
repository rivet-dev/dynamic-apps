noinst_PROGRAMS = \
	test-text \
	test-positioning \
	test-actions \
	test-icons

tests_cflags = \
	-I$(top_srcdir) \
	$(LIBNOTIFY_CFLAGS)

tests_ldadd = \
	$(LIBNOTIFY_LIBS)

icon_tests_cflags = \
	$(GTK_CFLAGS) \
	$(tests_cflags)

icon_tests_ldadd = \
	$(GTK_LIBS) \
	$(tests_ldadd)

test_actions_SOURCES = test-actions.c
test_actions_CFLAGS = $(tests_cflags)
test_actions_LDADD = $(tests_ldadd)

test_positioning_SOURCES = test-positioning.c
test_positioning_CFLAGS = $(tests_cflags)
test_positioning_LDADD = $(tests_ldadd)

test_text_SOURCES = test-text.c
test_text_CFLAGS = $(tests_cflags)
test_text_LDADD = $(tests_ldadd)

test_icons_SOURCES = test-icons.c
test_icons_CFLAGS = $(icon_tests_cflags)
test_icons_LDADD = $(icon_tests_ldadd)
