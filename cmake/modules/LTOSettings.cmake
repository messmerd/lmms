# Copyright (c) 2025 Dalton Messmer <messmer.dalton/at/gmail.com>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# This module defines:
#   - lto_settings
#   - STATUS_LTO
#
# `lto_settings` is an interface target which depends on the `LTO_MODE` option and
# can be used to apply the link-time optimization settings to another target of choice.
# `STATUS_LTO` is variable containing the status string.

# TODO: -Wl,--thinlto-cache-dir=${CMAKE_BINARY_DIR}/lto.cache
# TODO: -flto-incremental=${CMAKE_BINARY_DIR}/lto.cache

add_library(lto_settings INTERFACE)

message(STATUS "LTO mode: ${LTO_MODE}")

if(LTO_MODE STREQUAL "off")
	set(STATUS_LTO "Disabled")
	return()
endif()

# Choosing "auto" makes LTO dependent on the build configuration
if(LTO_MODE STREQUAL "auto")
	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		message(STATUS "Using 'full' LTO for Release build")
		set(_lto_mode "full")
	elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		message(STATUS "Using 'thin' LTO for RelWithDebInfo build")
		set(_lto_mode "thin")
	else()
		# Disable LTO for Debug builds
		message(STATUS "Disabling LTO for Debug build")
		set(STATUS_LTO "Disabled")
		return()
	endif()
else()
	set(_lto_mode ${LTO_MODE})
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT _lto_supported OUTPUT _lto_error)

if(NOT _lto_supported)
	set(STATUS_LTO "Not supported by this compiler")
	message(WARNING "LTO is not supported: ${_lto_error}")
	return()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	if(_lto_mode STREQUAL "thin")
		# The GCC equivalent of Clang's ThinLTO is -flto-partition=balanced
		set(STATUS_LTO "Enabled (balanced LTO partition)")
		target_compile_options(lto_settings INTERFACE -flto=auto -flto-partition=balanced)
		target_link_options(lto_settings INTERFACE -flto=auto -flto-partition=balanced)
	elseif(_lto_mode STREQUAL "full")
		set(STATUS_LTO "Enabled (single LTO partition)")
		target_compile_options(lto_settings INTERFACE -flto=auto -flto-partition=one)
		target_link_options(lto_settings INTERFACE -flto=auto -flto-partition=one)
	endif()

	# TODO: -flto-incremental incremental LTO

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	if(APPLE)
		if (NOT CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
			# See: https://clang.llvm.org/docs/CommandGuide/clang.html#cmdoption-flto
			set(STATUS_LTO "Not supported for Debug builds")
			message(WARNING "LTO is not supported for macOS debug builds")
			return()
		endif()
	endif()

	if(_lto_mode STREQUAL "thin")
		set(STATUS_LTO "Enabled (ThinLTO)")
		target_compile_options(lto_settings INTERFACE -flto=thin)
		target_link_options(lto_settings INTERFACE -flto=thin)
	elseif(_lto_mode STREQUAL "full")
		set(STATUS_LTO "Enabled (full)")
		target_compile_options(lto_settings INTERFACE -flto=full)
		target_link_options(lto_settings INTERFACE -flto=full)
	endif()

elseif(MSVC)
	set(STATUS_LTO "Enabled")
	target_compile_options(lto_settings INTERFACE /GL)
	target_link_options(lto_settings INTERFACE /LTCG)

	if(LTO_MODE STREQUAL "thin")
		message(WARNING "MSVC does not support ThinLTO, using /GL + /LTCG instead")
	endif()
endif()

set_property(TARGET lto_settings PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
