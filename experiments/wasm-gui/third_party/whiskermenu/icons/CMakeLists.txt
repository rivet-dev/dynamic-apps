# SPDX-FileCopyrightText: 2013 Graeme Gott <graeme@gottcode.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INSTALL_ICONDIR ${CMAKE_INSTALL_DATADIR}/icons/hicolor)

install(FILES hi16-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/16x16/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi22-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/22x22/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi24-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/24x24/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi32-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/32x32/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi48-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/48x48/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi64-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/64x64/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi128-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/128x128/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hi256-app-whiskermenu.png
        DESTINATION ${INSTALL_ICONDIR}/256x256/apps
        RENAME org.xfce.panel.whiskermenu.png)

install(FILES hisc-app-whiskermenu.svg
        DESTINATION ${INSTALL_ICONDIR}/scalable/apps
        RENAME org.xfce.panel.whiskermenu.svg)
