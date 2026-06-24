default_config_DATA = \
	default/config
default_configdir = $(sysconfdir)/xdg/lxpanel/default

default_config_panel_DATA = \
	default/panels/panel
default_config_paneldir = $(sysconfdir)/xdg/lxpanel/default/panels

two_panels_config_DATA = \
	two_panels/config
two_panels_configdir = $(sysconfdir)/xdg/lxpanel/two_panels

two_panels_config_panel_DATA = \
	two_panels/panels/bottom \
	two_panels/panels/top
two_panels_config_paneldir = $(sysconfdir)/xdg/lxpanel/two_panels/panels

uidir=$(datadir)/lxpanel/ui
ui_in_files= \
	ui/panel-pref.glade \
	ui/launchtaskbar.glade \
	ui/netstatus.glade

ui_DATA = $(ui_in_files:.glade=.ui)

# Purge GtkBuilder UI files
.glade.ui:
	sed 's/<!--.*-->//' < $< | sed ':a;N;$$!ba;s/ *\n *</</g' > $@

CLEANFILES = \
	$(ui_DATA)


lxpanel_images_DATA = \
	images/my-computer.png \
	images/stock_volume.png \
	images/file-manager.png \
	images/gnome-fs-desktop.png \
	images/gnome-logout.png \
	images/gnome-netstatus-0-24.png \
	images/gnome-netstatus-25-49.png \
	images/gnome-netstatus-50-74.png \
	images/gnome-netstatus-75-100.png \
	images/gnome-netstatus-disconn.png \
	images/gnome-netstatus-error.png \
	images/gnome-netstatus-idle.png \
	images/gnome-netstatus-rx.png \
	images/gnome-netstatus-txrx.png \
	images/gnome-netstatus-tx.png \
	images/background.png \
	images/volume.png \
	images/volume-low.png \
	images/volume-medium.png \
	images/volume-high.png \
	images/mute.png \
	images/ns-bothrs.png \
	images/ns-connected.png \
	images/ns-disconnect.png \
	images/ns-problem.png \
	images/ns-recvdata.png \
	images/ns-senddata.png \
	images/ns-lock.png \
	images/capslock-on.png \
	images/capslock-off.png \
	images/numlock-on.png \
	images/numlock-off.png \
	images/scrllock-on.png \
	images/scrllock-off.png \
	images/window-manager.png \
	images/clock.png \
	images/cpufreq-icon.png


lxpanel_imagesdir = $(datadir)/lxpanel/images

EXTRA_DIST = \
	default/panels/panel.in \
	$(lxpanel_DATA) \
	$(lxpanel_images_DATA) \
	$(ui_DATA) \
	$(ui_in_files) \
	$(default_config_DATA) \
	$(two_panels_config_DATA)
