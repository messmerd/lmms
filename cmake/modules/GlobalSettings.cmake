# Copyright (c) 2026 Dalton Messmer <messmer.dalton/at/gmail.com>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# The lmmsglobal interface target contains all the common compile/link options meant
# to be applied to the lmms, lmmsobj, plugin, test, and 3rd-party targets.
# This target is an alternative to include_directories(), link_libraries(),
# and other such discouraged CMake functions with a global scope.

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Debug-related macros
target_compile_definitions(lmmsglobal INTERFACE $<$<CONFIG:Debug>:LMMS_DEBUG>)
target_compile_definitions(lmmsglobal INTERFACE $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# Enable Gprof
target_compile_options(lmmsglobal INTERFACE $<$<BOOL:${WANT_DEBUG_GPROF}>:-pg>)
target_link_options(lmmsglobal INTERFACE $<$<BOOL:${WANT_DEBUG_GPROF}>:-pg>)

# Set MSVC-specific options
# - /options:strict:      Raises an error on unrecognised compiler options
# - /utf-8:               Sets UTF-8 as the source and execution character set
# - /permissive-:         Opts in to a variety of Standard C++ conforming behavior
# - /Zc:__cplusplus:      Sets the __cplusplus macro to the correct value
# - /Zc:checkGwOdr:       Enforce Standard C++ ODR violations under /Gw
# - /Zc:enumTypes:        Makes the underlying types of enums C++ Standard-conforming
# - /Zc:inline:           All inline functions must be defined at the point of declaration
# - /Zc:preprocessor:     Uses a Standard-conforming C preprocessor
# - /Zc:templateScope:    Template variable names are not allowed to shadow
# - /Zc:throwingNew:      Assumes the new operator throws std::bad_alloc on failure rather than
#                         returning nullptr. This eliminates unnecessary null checks for better performance.
# - /Zc:u8EscapeEncoding: Uses Standard-conforming u8 string literals
target_compile_options(lmmsglobal INTERFACE
	$<$<CXX_COMPILER_ID:MSVC>:/options:strict /utf-8 /permissive- /Zc:__cplusplus /Zc:checkGwOdr /Zc:enumTypes /Zc:inline /Zc:preprocessor /Zc:templateScope /Zc:throwingNew /Zc:u8EscapeEncoding>
)

# Disable min/max macros in windows.h
target_compile_definitions(lmmsglobal INTERFACE $<$<BOOL:${WIN32}>:NOMINMAX>)

# Enable position-independent code on non-Windows
# TODO: Enable on Windows too?
target_compile_options(lmmsglobal INTERFACE $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-fPIC>)
target_compile_definitions(lmmsglobal INTERFACE $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:PIC>)

# Use libc++ on macOS
target_compile_options(lmmsglobal INTERFACE $<$<AND:$<BOOL:${APPLE}>,$<CXX_COMPILER_ID:Clang,AppleClang>>:-stdlib=libc++>)

# Fix linking on macOS 10.14+. See issue #4762 on GitHub
target_link_directories(lmmsglobal INTERFACE $<$<BOOL:${APPLE}>:"${APPLE_PREFIX}/lib">)

# Paths relative to lmms executable
file(RELATIVE_PATH _lib_dir_relative "/${BIN_DIR}" "/${LIB_DIR}")
file(RELATIVE_PATH _plugin_dir_relative "/${BIN_DIR}" "/${PLUGIN_DIR}")

# Set LIB_DIR and PLUGIN_DIR macros
target_compile_definitions(lmmsglobal INTERFACE
	-DLIB_DIR="${_lib_dir_relative}"
	-DPLUGIN_DIR="${_plugin_dir_relative}"
)

# Set the target microarchitecture
if(TARGET_UARCH_FLAGS)
	if(TARGET_UARCH AND NOT TARGET_UARCH STREQUAL "custom")
		message(WARNING "Ignoring TARGET_UARCH since TARGET_UARCH_FLAGS was set")
	endif()

	set(_default_target_uarch "custom")
	message(STATUS "Setting target microarchitecture to 'custom'")
	message(STATUS "Target microarchitecture flags: ${TARGET_UARCH_FLAGS}")
elseif(TARGET_UARCH)
	message(STATUS "Setting target microarchitecture to '${TARGET_UARCH}'")
else()
	set(_default_target_uarch "none")
	message(STATUS "Setting target microarchitecture to 'none' since TARGET_UARCH was not set")
endif()

if(NOT TARGET_UARCH)
	set(TARGET_UARCH ${_default_target_uarch} CACHE STRING "Choose the target microarchitecture" FORCE)
	# Accepted options:
	# - "none":     No microarchitecture is set. Uses the compiler default.
	# - "official": Targets the baseline microarchitecture of official LMMS releases.
	# - "custom":   Targets the microarchitecture specified by TARGET_UARCH_FLAGS.
	# - "native":   Targets the highest microarchitecture level supported by the build machine.
	set_property(CACHE TARGET_UARCH PROPERTY STRINGS "none" "official" "custom" "native")
endif()

if(TARGET_UARCH STREQUAL "custom" OR TARGET_UARCH_FLAGS)
	target_compile_options(lmmsglobal INTERFACE ${TARGET_UARCH_FLAGS})
elseif(TARGET_UARCH STREQUAL "none")
	# Do nothing, use the compiler default
elseif(TARGET_UARCH STREQUAL "official")
	if(LMMS_HOST_X86_64)
		if(MSVC)
			# MSVC doesn't have /arch:x86-64-v2, but this is pretty close
			target_compile_options(lmmsglobal INTERFACE /arch:SSE4.2)
		else()
			target_compile_options(lmmsglobal INTERFACE -march=x86-64-v2)
		endif()
	elseif(LMMS_HOST_ARM64)
		if(LMMS_BUILD_WIN64)
			# Using Windows 11 minimum requirements
			# https://learn.microsoft.com/en-us/windows-hardware/design/minimum/minimum-hardware-requirements-overview
			if(MSVC)
				target_compile_options(lmmsglobal INTERFACE /arch:armv8.1)
			else()
				target_compile_options(lmmsglobal INTERFACE -march=armv8.1-a)
			endif()
		elseif(LMMS_BUILD_APPLE)
			# Chosen for Apple M1 support
			target_compile_options(lmmsglobal INTERFACE -march=armv8.4-a)
		else()
			# Chosen for Raspberry Pi 4 support
			target_compile_options(lmmsglobal INTERFACE -march=armv8-a)
		endif()
	else()
		message(WARNING "There are no official releases for the target architecture, so there is no official baseline microarchitecture")
	endif()
elseif(TARGET_UARCH STREQUAL "native")
	if(CMAKE_CROSSCOMPILING)
		message(FATAL_ERROR "Cannot target native microarchitecture while cross-compiling")
	endif()

	if(MSVC)
		include(CheckCPUFeatures)
		determine_msvc_native_arch(_arch_option)
		if(NOT "${_arch_option}" STREQUAL "")
			target_compile_options(lmmsglobal INTERFACE ${_arch_option})
		endif()
	else()
		target_compile_options(lmmsglobal INTERFACE -march=native -mtune=native)
	endif()
endif()
