SUBDIRS = \
	appearance-settings \
	accessibility-settings \
	keyboard-settings \
	mouse-settings

if HAVE_XRANDR
SUBDIRS += \
	display-settings
endif

if HAVE_GIO_UNIX
SUBDIRS += \
	mime-settings
endif

if HAVE_COLORD
SUBDIRS += \
	color-settings
endif
