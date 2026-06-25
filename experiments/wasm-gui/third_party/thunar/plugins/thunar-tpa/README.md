Thunar Trash Panel Applet (thunar-tpa)
======================================

Thunar-tpa is an extension for the Xfce Panel, which enables users to add a trash can to their panel, that

 * displays the current state of the trash can
 * can move files to the trash by dropping them on the trash icon
 * can empty the trash can
 * can open the trash can

In order to build and install this plugin, you will need to have the xfce4-panel development headers and libraries installed (the appropriate package is usually called xfce4-panel-dev or xfce4-panel-devel). In addition, you'll need to have D-BUS 0.34 or above installed and Thunar must be built with D-BUS support.


How does it work?
=================

To avoid running several desktop processes that all monitor and manage the trash can by itself, and thereby create an unnecessary maintaince and resource overhead, the trash applet simply connects to Thunar via D-BUS to query the state of the trash and send commands to the Trash.

The trash applet is not limited to Thunar, but can work with any file manager that implements the org.xfce.Trash interface and owns the org.xfce.FileManager service. See the thunar-tpa bindings.xml file for a details about the inter- face.

