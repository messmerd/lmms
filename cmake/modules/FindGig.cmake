# Copyright (c) 2024 Kevin Zander
# Copyright (c) 2026 Dalton Messmer <messmer.dalton/at/gmail.com>
# Based on FindPortAudio.cmake, copyright (c) 2023 Dominic Clark
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(ImportedTargetHelpers)

find_package_config_mode_with_fallback(libgig libgig::libgig
	LIBRARY_NAMES "gig"
	INCLUDE_NAMES "libgig/gig.h"
	PKG_CONFIG gig
	PREFIX Gig
)

message(STATUS "~~~~~ 1) Gig_VERSION=${Gig_VERSION}")

if(TARGET libgig::libgig)
	# Try to read the VERSION compile definition from the CMake target
	if("${Gig_VERSION}" STREQUAL "")
		get_target_property(_defs libgig::libgig COMPILE_DEFINITIONS)
		foreach(_def IN LISTS _defs)
			if(_def MATCHES "^VERSION=\"(.*)\"")
				set(Gig_VERSION "${CMAKE_MATCH_1}")
			endif()
		endforeach()
	endif()

	message(STATUS "~~~~~ 2) Gig_VERSION=${Gig_VERSION}")

	# Fall back on retrieving the version by compiling the source
	determine_version_from_source(Gig_VERSION libgig::libgig [[
		#include <iostream>
		#include <libgig/gig.h>

		auto main() -> int
		{
			const auto version = gig::libraryVersion();
			std::cout << version;
		}
	]])

	message(STATUS "~~~~~ 3) Gig_VERSION=${Gig_VERSION}")

	# Set GIG_VERSION_* compile definitions
	if(NOT "${Gig_VERSION}" STREQUAL "")
		string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)"
			_gig_version_match_temp ${Gig_VERSION})
		target_compile_definitions(libgig::libgig INTERFACE
			GIG_VERSION_MAJOR=${CMAKE_MATCH_1}
			GIG_VERSION_MINOR=${CMAKE_MATCH_2}
			GIG_VERSION_BUILD=${CMAKE_MATCH_3}
		)
	endif()

	# Disable deprecated check for MinGW - TODO: Remove later
	if(MINGW)
		target_compile_options(libgig::libgig INTERFACE -Wno-deprecated)
	endif()

	# Required for not crashing loading files with libgig
	if(NOT MSVC)
		target_compile_options(libgig::libgig INTERFACE -fexceptions)
	endif()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Gig
	REQUIRED_VARS Gig_LIBRARY Gig_INCLUDE_DIRS
	VERSION_VAR Gig_VERSION
)
