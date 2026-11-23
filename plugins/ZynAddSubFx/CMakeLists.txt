if(NOT FLTK_FOUND)
	return()
endif()

INCLUDE(BuildPlugin)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

IF(MINGW_PREFIX)
	SET(FLTK_FLUID_EXECUTABLE "${MINGW_PREFIX}/bin/fluid")
ENDIF()

IF(NOT EXISTS ${FLTK_FLUID_EXECUTABLE})
    # Try to detect full path anyway
    EXECUTE_PROCESS(COMMAND which fluid OUTPUT_VARIABLE FLTK_FLUID_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)
    IF(NOT EXISTS ${FLTK_FLUID_EXECUTABLE})
        MESSAGE(FATAL_ERROR "Please install the fluid binary which is part of FLTK.")
    ENDIF()
ENDIF()

##################################################
# Settings common to all zynaddsubfx sub-targets #
##################################################

add_library(zynaddsubfx_common INTERFACE)
target_link_libraries(zynaddsubfx_common INTERFACE lmmsglobal)
target_include_directories(zynaddsubfx_common INTERFACE
	"${CMAKE_BINARY_DIR}/src" # lmms_export.h
	"${CMAKE_CURRENT_SOURCE_DIR}" # ThreadShims.h
)

# Work around _WIN32 vs WIN32 differences
target_compile_definitions(zynaddsubfx_common INTERFACE $<$<CXX_COMPILER_ID:MSVC>:WIN32>)

# Full optimizations
target_compile_options(zynaddsubfx_common INTERFACE
	$<$<CXX_COMPILER_ID:MSVC>:/O2>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>
)

# Use asm optimizations when targeting x86 or x86_64
if(NOT MSVC AND (LMMS_HOST_X86 OR LMMS_HOST_X86_64))
	target_compile_definitions(zynaddsubfx_common INTERFACE -DASM_F2I_YES)
endif()

###################
# zynaddsubfx_nio #
###################

add_subdirectory(zynaddsubfx/src/Nio)
target_link_libraries(zynaddsubfx_nio PUBLIC zynaddsubfx_common)

#####################
# zynaddsubfx_synth #
#####################

add_library(zynaddsubfx_synth OBJECT
	zynaddsubfx/src/DSP/AnalogFilter.cpp
	zynaddsubfx/src/DSP/FFTwrapper.cpp
	zynaddsubfx/src/DSP/Filter.cpp
	zynaddsubfx/src/DSP/FormantFilter.cpp
	zynaddsubfx/src/DSP/SVFilter.cpp
	zynaddsubfx/src/DSP/Unison.cpp
	zynaddsubfx/src/Effects/Alienwah.cpp
	zynaddsubfx/src/Effects/Chorus.cpp
	zynaddsubfx/src/Effects/Distorsion.cpp
	zynaddsubfx/src/Effects/DynamicFilter.cpp
	zynaddsubfx/src/Effects/Echo.cpp
	zynaddsubfx/src/Effects/Effect.cpp
	zynaddsubfx/src/Effects/EffectLFO.cpp
	zynaddsubfx/src/Effects/EffectMgr.cpp
	zynaddsubfx/src/Effects/EQ.cpp
	zynaddsubfx/src/Effects/Phaser.cpp
	zynaddsubfx/src/Effects/Reverb.cpp
	zynaddsubfx/src/Misc/Bank.cpp
	zynaddsubfx/src/Misc/Config.cpp
	zynaddsubfx/src/Misc/Dump.cpp
	zynaddsubfx/src/Misc/Master.cpp
	zynaddsubfx/src/Misc/Microtonal.cpp
	zynaddsubfx/src/Misc/Part.cpp
	zynaddsubfx/src/Misc/Util.cpp
	zynaddsubfx/src/Misc/QtXmlWrapper.cpp
	zynaddsubfx/src/Misc/Recorder.cpp
	zynaddsubfx/src/Misc/WavFile.cpp
	zynaddsubfx/src/Misc/WaveShapeSmps.cpp
	zynaddsubfx/src/Params/ADnoteParameters.cpp
	zynaddsubfx/src/Params/Controller.cpp
	zynaddsubfx/src/Params/EnvelopeParams.cpp
	zynaddsubfx/src/Params/FilterParams.cpp
	zynaddsubfx/src/Params/LFOParams.cpp
	zynaddsubfx/src/Params/PADnoteParameters.cpp
	zynaddsubfx/src/Params/Presets.cpp
	zynaddsubfx/src/Params/PresetsArray.cpp
	zynaddsubfx/src/Params/PresetsStore.cpp
	zynaddsubfx/src/Params/SUBnoteParameters.cpp
	zynaddsubfx/src/Synth/SynthNote.cpp
	zynaddsubfx/src/Synth/ADnote.cpp
	zynaddsubfx/src/Synth/Envelope.cpp
	zynaddsubfx/src/Synth/LFO.cpp
	zynaddsubfx/src/Synth/OscilGen.cpp
	zynaddsubfx/src/Synth/PADnote.cpp
	zynaddsubfx/src/Synth/Resonance.cpp
	zynaddsubfx/src/Synth/SUBnote.cpp
)

target_link_libraries(zynaddsubfx_synth PUBLIC zynaddsubfx_common)
target_include_directories(zynaddsubfx_synth SYSTEM PRIVATE ${FFTW3F_INCLUDE_DIRS})

# Unfortunately there's a Qt dependency due to src/Misc/QtXmlWrapper.cpp
target_link_libraries(zynaddsubfx_synth PRIVATE ${QT_LIBRARIES})
target_include_directories(zynaddsubfx_synth SYSTEM PRIVATE ${QT_INCLUDE_DIRS})

####################
# zynaddsubfx_core #
####################

add_library(zynaddsubfx_core STATIC
	LocalZynAddSubFx.cpp
)
target_include_directories(zynaddsubfx_core PUBLIC
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_BINARY_DIR}" # lmmsconfig.h
)
target_include_directories(zynaddsubfx_core SYSTEM PUBLIC
	zynaddsubfx/src
	"${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfx/src"
)
target_static_libraries(zynaddsubfx_core PUBLIC
	zynaddsubfx_nio
	zynaddsubfx_synth
)
target_link_libraries(zynaddsubfx_core PUBLIC
	${FFTW3F_LIBRARIES}
	${QT_LIBRARIES}
	Threads::Threads
	ZLIB::ZLIB
)

target_include_directories(zynaddsubfx_core SYSTEM PUBLIC
	${QT_INCLUDE_DIRS}
)

######################
# zynaddsubfx plugin #
######################

build_plugin(zynaddsubfx
	ZynAddSubFx.cpp ZynAddSubFx.h
	MOCFILES ZynAddSubFx.h
	EMBEDDED_RESOURCES artwork.png logo.png
)

target_static_libraries(zynaddsubfx PRIVATE zynaddsubfx_core)
target_link_directories(zynaddsubfx PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/..")

if(LMMS_BUILD_LINUX)
	target_link_options(zynaddsubfx PRIVATE -Wl,--enable-new-dtags)
	set_target_properties(zynaddsubfx PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN"
	)
else()
	set_target_properties(zynaddsubfx PROPERTIES
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PLUGIN_DIR}"
	)
endif()

if(MSVC)
	set(WINRC "${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfx.rc")
elseif(WIN32)
	SET(WINRC "${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfxrc.obj")
	ADD_CUSTOM_COMMAND(OUTPUT "${WINRC}"
				COMMAND "${CMAKE_RC_COMPILER}"
					"-I\"${CMAKE_CURRENT_SOURCE_DIR}\""
					"-o\"${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfxrc.obj\""
					"-i\"${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfx.rc\""
				DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/zynaddsubfx.rc")
endif()

###################
# zynaddsubfx_gui #
###################

add_subdirectory(zynaddsubfx/src/UI)
target_link_libraries(zynaddsubfx_gui PUBLIC zynaddsubfx_common)
target_include_directories(zynaddsubfx_gui SYSTEM PRIVATE "${FLTK_INCLUDE_DIR}")

# Remove exit confirmation dialogs etc. in MasterUI.fl
target_compile_definitions(zynaddsubfx_gui PRIVATE -DPLUGINVERSION)

if(LMMS_BUILD_LINUX OR LMMS_BUILD_APPLE OR LMMS_BUILD_OPENBSD OR LMMS_BUILD_FREEBSD OR LMMS_BUILD_CYGWIN)
	find_package(X11)
	target_include_directories(zynaddsubfx_gui SYSTEM PRIVATE ${X11_INCLUDE_DIR})
	target_compile_definitions(zynaddsubfx_gui PRIVATE -DOS_LINUX)
else()
	target_compile_definitions(zynaddsubfx_gui PRIVATE -DOS_WINDOWS)
endif()

# Relative include paths don't work automatically for the GUI, because the
# generated C++ files aren't in the source directory. Thus, add the expected
# source directory as an additional include directory.
target_include_directories(zynaddsubfx_gui PUBLIC zynaddsubfx/src/UI)

# Ensure zynaddsubfx_gui has the include directories it needs (#8535)
find_program(FLTK_CONFIG fltk-config)
if(FLTK_CONFIG AND NOT LMMS_BUILD_WIN32)
	execute_process(COMMAND ${FLTK_CONFIG} --cxxflags OUTPUT_VARIABLE _fltk_include_dirs OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE " " ";" _fltk_include_dirs ${_fltk_include_dirs})
	list(REMOVE_DUPLICATES _fltk_include_dirs)
	list(FILTER _fltk_include_dirs INCLUDE REGEX "^-I.+")
	list(TRANSFORM _fltk_include_dirs REPLACE "^-I(.+)" "\\1")
	target_include_directories(zynaddsubfx_gui SYSTEM PUBLIC ${_fltk_include_dirs})
endif()

#####################
# RemoteZynAddSubFx #
#####################

# Use libraries in non-standard directories (e.g., another version of Qt)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_subdirectory("${LMMS_SOURCE_DIR}/src/common" common)

add_executable(RemoteZynAddSubFx
	RemoteZynAddSubFx.cpp
	${LMMS_COMMON_SRCS}
	"${WINRC}"
)

target_static_libraries(RemoteZynAddSubFx PRIVATE zynaddsubfx_core zynaddsubfx_gui)

INSTALL(TARGETS RemoteZynAddSubFx RUNTIME DESTINATION "${PLUGIN_DIR}")

# Needed to deploy dependencies of RemoteZynAddSubFx
SET_PROPERTY(GLOBAL APPEND PROPERTY PLUGINS_BUILT "RemoteZynAddSubFx")

TARGET_COMPILE_DEFINITIONS(RemoteZynAddSubFx PRIVATE BUILD_REMOTE_PLUGIN_CLIENT)

IF(LMMS_BUILD_WIN32)
	target_link_options(RemoteZynAddSubFx PRIVATE "-mwindows")
ENDIF()

# Remove useless dependencies from FLTK. Use fltk-config to avoid static library
# in older environments
SET(FLTK_FILTERED_LDFLAGS ${FLTK_LIBRARIES})
IF(FLTK_CONFIG AND NOT (LMMS_BUILD_APPLE OR LMMS_BUILD_WIN32))
	EXECUTE_PROCESS(COMMAND ${FLTK_CONFIG} --ldflags OUTPUT_VARIABLE FLTK_FILTERED_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REPLACE " " ";" FLTK_FILTERED_LDFLAGS ${FLTK_FILTERED_LDFLAGS})
	LIST(REMOVE_ITEM FLTK_FILTERED_LDFLAGS -lX11)
ENDIF()
target_link_libraries(RemoteZynAddSubFx PRIVATE ${FLTK_FILTERED_LDFLAGS})

if(LMMS_HAVE_LIBRT)
	target_link_libraries(RemoteZynAddSubFx PRIVATE rt)
endif()

# Support qt_version_tag in Qt 5.6
TARGET_LINK_LIBRARIES(RemoteZynAddSubFx PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# link Qt libraries when on win32
IF(LMMS_BUILD_WIN32)
	TARGET_LINK_LIBRARIES(RemoteZynAddSubFx PRIVATE ${QT_LIBRARIES})
ENDIF(LMMS_BUILD_WIN32)


IF(LMMS_BUILD_WIN32)
	add_custom_command(
		TARGET RemoteZynAddSubFx
		POST_BUILD
		COMMAND "${STRIP_COMMAND}" "$<TARGET_FILE:RemoteZynAddSubFx>"
		VERBATIM
		COMMAND_EXPAND_LISTS
	)
ENDIF(LMMS_BUILD_WIN32)
