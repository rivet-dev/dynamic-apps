# SPDX-FileCopyrightText: 2013-2020 Graeme Gott <graeme@gottcode.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Gettext 0.19.1 REQUIRED)

# Generate translations
file(GLOB _po_files *.po)
foreach(_po_file ${_po_files})
	get_filename_component(_lang ${_po_file} NAME_WE)
	list(APPEND _linguas ${_lang})

	set(_gmo_file ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
	list(APPEND _gmo_files ${_gmo_file})

	add_custom_command(
		OUTPUT ${_gmo_file}
		COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_gmo_file} ${_po_file}
		DEPENDS ${_po_file}
	)
	install(
		FILES ${_gmo_file}
		DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES/
		RENAME xfce4-whiskermenu-plugin.mo
	)
endforeach()
add_custom_target(translations ALL DEPENDS ${_gmo_files})

# Generate desktop file
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS
	COMMAND ${CMAKE_COMMAND} -E echo "${_linguas}" > ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS
	COMMAND_EXPAND_LISTS
	COMMENT "Generating LINGUAS"
)
add_custom_command(
	OUTPUT whiskermenu.desktop
	COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
		--desktop
		--template=${PROJECT_SOURCE_DIR}/panel-plugin/whiskermenu.desktop.in
		-d ${CMAKE_CURRENT_SOURCE_DIR}
		-o whiskermenu.desktop
	DEPENDS ${_po_files} ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS
)
install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/whiskermenu.desktop
	DESTINATION ${CMAKE_INSTALL_DATADIR}/xfce4/panel/plugins
)
add_custom_target(desktopfile ALL DEPENDS whiskermenu.desktop)

# Generate translations template
find_program(XGETTEXT_EXECUTABLE xgettext)
if(XGETTEXT_EXECUTABLE)
	add_custom_target(update_pot
		COMMAND ${XGETTEXT_EXECUTABLE}
			--output=xfce4-whiskermenu-plugin.pot
			--from-code=UTF-8
			--c++ --keyword=_
			--sort-output
			--no-wrap
			--package-name='Whisker Menu'
			--copyright-holder='Graeme Gott'
			../panel-plugin/*.cpp ../panel-plugin/*.h
		COMMAND ${XGETTEXT_EXECUTABLE}
			--output=xfce4-whiskermenu-plugin.pot
			--join-existing
			--from-code=UTF-8
			-k --keyword=Name --keyword=Comment
			--sort-output
			--no-wrap
			--package-name='Whisker Menu'
			--copyright-holder='Graeme Gott'
			../panel-plugin/whiskermenu.desktop.in
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating xfce4-whiskermenu-plugin.pot"
	)
endif()
