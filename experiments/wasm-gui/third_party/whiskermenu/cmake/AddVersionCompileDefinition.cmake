# SPDX-FileCopyrightText: 2022 Graeme Gott <graeme@gottcode.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

function(add_version_compile_definition versionstr_file versionstr_def)
	find_package(Git QUIET)
	if(Git_FOUND)
		# Find git repository
		execute_process(
			COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			RESULT_VARIABLE git_dir_result
			OUTPUT_VARIABLE git_dir
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if (git_dir_result EQUAL 0)
			# Find version number from git
			execute_process(
				COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*"
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE versionstr
				ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			string(REGEX REPLACE "^v" "" versionstr "${versionstr}")

			# Rerun CMake when git repository changes
			if (EXISTS ${git_dir}/logs/HEAD)
				set_property(
					DIRECTORY
					APPEND
					PROPERTY CMAKE_CONFIGURE_DEPENDS ${git_dir}/logs/HEAD
				)
			endif()
		endif()
	endif()

	# Fall back to project's VERSION
	if ("${versionstr}" STREQUAL "")
		set(versionstr ${PROJECT_VERSION})
	endif()

	# Pass version as compile definition to file
	set_property(
		SOURCE ${versionstr_file}
		APPEND
		PROPERTY COMPILE_DEFINITIONS ${versionstr_def}="${versionstr}"
	)
endfunction()
