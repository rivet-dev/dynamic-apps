# SPDX-FileCopyrightText: 2013-2023 Graeme Gott <graeme@gottcode.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

project(whiskermenu VERSION 2.8.3)

# options
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_TOLOWER)
if(CMAKE_BUILD_TYPE_TOLOWER MATCHES "release|minsizerel")
	set(whiskermenu_strip ON)
endif()
if(CMAKE_BUILD_TYPE_TOLOWER MATCHES "release|minsizerel|relwithdebinfo")
	set(whiskermenu_lto ON)
endif()

option(ENABLE_ACCOUNTS_SERVICE "Enable building with AccountsService" ON)
option(ENABLE_GTK_LAYER_SHELL "Enable support for positioning in Wayland" ON)
option(ENABLE_VISIBILITY "Enable ELF visibility attributes" ON)
option(ENABLE_AS_NEEDED "Enable -Wl,--as-needed for the linker" ON)
option(ENABLE_LINKER_OPTIMIZED_HASH_TABLES "Enable -Wl,-O1 for the linker" ON)
option(ENABLE_LINK_TIME_OPTIMIZATION "Enable link time optimization" ${whiskermenu_lto})
option(ENABLE_STRIP "Enable automatic stripping of release builds" ${whiskermenu_strip})
option(ENABLE_DEVELOPER_MODE "Enable strict checks to help with development" OFF)

include(GNUInstallDirs)

add_subdirectory(icons)
add_subdirectory(panel-plugin)
add_subdirectory(po)

# uninstall target
if(NOT TARGET uninstall)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/cmake/uninstall.cmake
		IMMEDIATE @ONLY)
	add_custom_target(uninstall
		${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/uninstall.cmake)
endif()
