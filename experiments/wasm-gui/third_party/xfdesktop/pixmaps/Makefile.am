icons16dir = $(datadir)/icons/hicolor/16x16/apps

icons16_DATA = \
	16x16/org.xfce.xfdesktop.png

icons24dir = $(datadir)/icons/hicolor/24x24/apps

icons24_DATA = \
	24x24/org.xfce.xfdesktop.png

icons32dir = $(datadir)/icons/hicolor/32x32/apps

icons32_DATA = \
	32x32/org.xfce.xfdesktop.png

icons48dir = $(datadir)/icons/hicolor/48x48/apps

icons48_DATA = \
	48x48/org.xfce.xfdesktop.png

icons128dir = $(datadir)/icons/hicolor/128x128/apps

icons128_DATA = \
	128x128/org.xfce.xfdesktop.png

iconsscdir = $(datadir)/icons/hicolor/scalable/apps

iconssc_DATA = \
	scalable/org.xfce.xfdesktop.svg

pixmapsdir = $(datadir)/pixmaps

pixmaps_DATA = \
	xfce4_xicon.png \
	xfce4_xicon1.png \
	xfce4_xicon2.png \
	xfce4_xicon3.png \
	xfce4_xicon4.png

pixmapsxdir = $(datadir)/pixmaps/xfdesktop

pixmapsx_DATA = \
	xfdesktop-fallback-icon.png

EXTRA_DIST = \
	$(icons16_DATA) \
	$(icons24_DATA) \
	$(icons32_DATA) \
	$(icons48_DATA) \
	$(icons128_DATA) \
	$(iconssc_DATA) \
	$(pixmaps_DATA) \
	$(pixmapsx_DATA)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  Remember to run:"; \
		echo "***"; \
		echo "***   $(gtk_update_icon_cache)"; \
		echo "***"; \
	fi
