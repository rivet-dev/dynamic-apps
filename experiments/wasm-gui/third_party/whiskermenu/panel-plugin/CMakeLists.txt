# SPDX-FileCopyrightText: 2013-2022 Graeme Gott <graeme@gottcode.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include(../cmake/AddVersionCompileDefinition.cmake)
add_version_compile_definition(plugin.cpp PACKAGE_VERSION)
add_version_compile_definition(xfce4-popup-whiskermenu.cpp PACKAGE_VERSION)

find_package(PkgConfig REQUIRED)
pkg_check_modules(gio REQUIRED IMPORTED_TARGET gio-2.0)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)
pkg_check_modules(exo REQUIRED IMPORTED_TARGET exo-2>=0.12)
pkg_check_modules(garcon REQUIRED IMPORTED_TARGET garcon-1>=0.6.4)
pkg_check_modules(libxfce4panel REQUIRED IMPORTED_TARGET libxfce4panel-2.0>=4.14)
pkg_check_modules(libxfce4ui REQUIRED IMPORTED_TARGET libxfce4ui-2>=4.14)
pkg_check_modules(libxfce4util REQUIRED IMPORTED_TARGET libxfce4util-1.0>=4.14)
pkg_check_modules(libxfconf REQUIRED IMPORTED_TARGET libxfconf-0>=4.14)

# plugin
add_library(whiskermenu MODULE
	applications-page.cpp
	category.cpp
	category-button.cpp
	command.cpp
	command-edit.cpp
	element.cpp
	favorites-page.cpp
	icon-renderer.cpp
	icon-size.cpp
	image-menu-item.h
	launcher.cpp
	launcher-icon-view.cpp
	launcher-tree-view.cpp
	page.cpp
	plugin.cpp
	profile.cpp
	query.cpp
	recent-page.cpp
	register-plugin.c
	run-action.cpp
	search-action.cpp
	search-page.cpp
	settings.cpp
	settings-dialog.cpp
	slot.h
	window.cpp)

target_link_libraries(whiskermenu PRIVATE
	PkgConfig::GTK3
	PkgConfig::exo
	PkgConfig::garcon
	PkgConfig::libxfce4panel
	PkgConfig::libxfce4ui
	PkgConfig::libxfce4util
	PkgConfig::libxfconf)

target_compile_options(whiskermenu PRIVATE -Wall -Wextra)

target_compile_definitions(whiskermenu PRIVATE
	GETTEXT_PACKAGE="xfce4-whiskermenu-plugin"
	PACKAGE_LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
	PACKAGE_NAME="xfce4-whiskermenu-plugin"
	BINDIR="${CMAKE_INSTALL_FULL_BINDIR}"
	DATADIR="${CMAKE_INSTALL_FULL_DATADIR}"
	SETTINGS_MENUFILE="${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/menus/xfce-settings-manager.menu"
	G_LOG_DOMAIN="whiskermenu"
	GSEAL_ENABLE
	GTK_MULTIHEAD_SAFE
	$<$<NOT:$<CONFIG:Debug>>:G_DISABLE_ASSERT G_DISABLE_CAST_CHECKS>)

target_compile_features(whiskermenu PRIVATE cxx_std_11)

if(ENABLE_ACCOUNTS_SERVICE)
	pkg_check_modules(accountsservice REQUIRED IMPORTED_TARGET accountsservice>=0.6.45)
	target_link_libraries(whiskermenu PRIVATE PkgConfig::accountsservice)
	target_compile_definitions(whiskermenu PRIVATE HAVE_ACCOUNTS_SERVICE)
endif()

if(ENABLE_GTK_LAYER_SHELL)
	pkg_check_modules(gtklayershell REQUIRED IMPORTED_TARGET gtk-layer-shell-0>=0.7)
	target_link_libraries(whiskermenu PRIVATE PkgConfig::gtklayershell)
	target_compile_definitions(whiskermenu PRIVATE HAVE_GTK_LAYER_SHELL)
endif()

# popup
add_executable(xfce4-popup-whiskermenu xfce4-popup-whiskermenu.cpp)

target_link_libraries(xfce4-popup-whiskermenu PRIVATE
	PkgConfig::gio
	PkgConfig::libxfconf)

target_compile_options(xfce4-popup-whiskermenu PRIVATE -Wall -Wextra)

target_compile_definitions(xfce4-popup-whiskermenu PRIVATE
	GETTEXT_PACKAGE="xfce4-whiskermenu-plugin"
	PACKAGE_LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
	PACKAGE_NAME="xfce4-popup-whiskermenu"
	G_LOG_DOMAIN="whiskermenu"
	GSEAL_ENABLE
	$<$<CONFIG:Release>:G_DISABLE_ASSERT G_DISABLE_CAST_CHECKS>
	$<$<CONFIG:MinSizeRel>:G_DISABLE_ASSERT G_DISABLE_CAST_CHECKS>
	$<$<CONFIG:RelWithDebInfo>:G_DISABLE_ASSERT G_DISABLE_CAST_CHECKS>)

# options
if(ENABLE_VISIBILITY)
	set_target_properties(whiskermenu PROPERTIES CXX_VISIBILITY_PRESET hidden)
endif()

if(ENABLE_AS_NEEDED)
	target_link_options(whiskermenu PRIVATE "LINKER:--as-needed")
endif()

if(ENABLE_LINKER_OPTIMIZED_HASH_TABLES)
	target_link_options(whiskermenu PRIVATE "LINKER:-O1")
endif()

if(ENABLE_DEVELOPER_MODE)
	target_link_options(whiskermenu PRIVATE "LINKER:-z,defs")
endif()

if(ENABLE_DEVELOPER_MODE)
	target_compile_options(whiskermenu PRIVATE -Werror)
	target_compile_definitions(whiskermenu PRIVATE
		XFCE_DISABLE_DEPRECATED
		G_DISABLE_DEPRECATED
		GDK_DISABLE_DEPRECATED
		GDK_PIXBUF_DISABLE_DEPRECATED
		GDK_DISABLE_SINGLE_INCLUDES
		GDK_PIXBUF_DISABLE_SINGLE_INCLUDES
		GTK_DISABLE_SINGLE_INCLUDES)

	target_compile_options(xfce4-popup-whiskermenu PRIVATE -Werror)
	target_compile_definitions(xfce4-popup-whiskermenu PRIVATE
		G_DISABLE_DEPRECATED)
endif()

if(ENABLE_LINK_TIME_OPTIMIZATION)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT result)
	if(result)
		set_target_properties(whiskermenu PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

if(ENABLE_STRIP)
	add_custom_command(TARGET whiskermenu POST_BUILD
		COMMAND ${CMAKE_STRIP} $<TARGET_FILE:whiskermenu>)
	add_custom_command(TARGET xfce4-popup-whiskermenu POST_BUILD
		COMMAND ${CMAKE_STRIP} $<TARGET_FILE:xfce4-popup-whiskermenu>)
endif()

# install plugin
install(TARGETS whiskermenu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/xfce4/panel/plugins)

# install popup
install(TARGETS xfce4-popup-whiskermenu RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES xfce4-popup-whiskermenu.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT doc)
