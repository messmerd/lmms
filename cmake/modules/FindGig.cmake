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


#####################################################

# Inspired from https://stackoverflow.com/questions/32183975/how-to-print-all-the-properties-of-a-target-in-cmake

# Get all propreties that cmake supports
execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)

# Convert command output into a CMake list
STRING(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
STRING(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")

# Prints the given property of the given target.
# Recursive for properties ending with <CONFIG>
# Skips properties that are not set.
function(print_target_property tgt prop)
  string(FIND ${prop} "<CONFIG>" isconfig)
  if (NOT ${isconfig} MATCHES -1)
    # this is a <CONFIG> property

    if (${CMAKE_BUILD_TYPE})
      # Expecting makefile generator

      #try ${CMAKE_BUILD_TYPE}
      string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" propconfig ${prop})
      print_target_property(${tgt} ${propconfig})
    else()
      # Expecting Visual Studio generator

      #try RELEASE
      string(REPLACE "<CONFIG>" "RELEASE" propconfig ${prop})
      print_target_property(${tgt} ${propconfig})

      #try DEBUG
      string(REPLACE "<CONFIG>" "DEBUG" propconfig ${prop})
      print_target_property(${tgt} ${propconfig})
    endif()
  else()
    # message ("Checking ${prop}")
    get_property(propval TARGET ${tgt} PROPERTY ${prop} SET)
    if (propval)
        get_target_property(propval ${tgt} ${prop})
        message ("${tgt}.${prop}='${propval}'")
    endif()
  endif()
endfunction(print_target_property)

# Prints all CMake properties of the given target.
function(print_target_properties tgt)
    if(NOT TARGET ${tgt})
      message("There is no target named '${tgt}'")
      return()
    endif()

    foreach (prop ${CMAKE_PROPERTY_LIST})
        # Fix https://stackoverflow.com/questions/32197663/how-can-i-remove-the-the-location-property-may-not-be-read-from-target-error-i
        if(prop STREQUAL "LOCATION" OR prop MATCHES "^LOCATION_" OR prop MATCHES "_LOCATION$")
            continue()
        endif()

        # Try to print this property.
        print_target_property(${tgt} ${prop})
    endforeach(prop)
endfunction(print_target_properties)

#####################################################





message(STATUS "~~~~~ 1) Gig_VERSION=${Gig_VERSION}")

if(TARGET libgig::libgig)
	# Try to read the VERSION compile definition from the CMake target
	if("${Gig_VERSION}" STREQUAL "")
		message(STATUS "~~~~~~~~ 1a")
		get_target_property(_defs libgig::libgig COMPILE_DEFINITIONS)
		foreach(_def IN LISTS _defs)
			message(STATUS "~~~~~~~~ _def=${_def}")
			if(_def MATCHES "^VERSION=\"(.*)\"")
				set(Gig_VERSION "${CMAKE_MATCH_1}")
				message(STATUS "~~~~~~~~ 1a - found version: ${Gig_VERSION}")
			endif()
		endforeach()
		message(STATUS "~~~~~~~~ 1b")
		get_target_property(_defs2 libgig::libgig INTERFACE_COMPILE_DEFINITIONS)
		foreach(_def2 IN LISTS _defs2)
			message(STATUS "~~~~~~~~ _def2=${_def2}")
			if(_def2 MATCHES "^VERSION=\"(.*)\"")
				set(Gig_VERSION "${CMAKE_MATCH_1}")
				message(STATUS "~~~~~~~~ 1b - found version: ${Gig_VERSION}")
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

message(STATUS "~~~~~~~~ 1c")
print_target_properties(libgig::libgig)
message(STATUS "~~~~~~~~ 1d")
print_target_properties(libgig)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Gig
	REQUIRED_VARS Gig_LIBRARY Gig_INCLUDE_DIRS Gig_VERSION
	VERSION_VAR Gig_VERSION
)
