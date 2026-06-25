
SUBDIRS = \
	16x16 \
	24x24 \
	32x32 \
	48x48 \
	64x64 \
	96x96 \
	128x128 \
	scalable

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
                echo "Updating Gtk icon cache."; \
                $(gtk_update_icon_cache); \
        else \
                echo "*** Icon cache not updated. Remember to run:"; \
		echo "***"; \
                echo "***   $(gtk_update_icon_cache)"; \
		echo "***"; \
        fi

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
