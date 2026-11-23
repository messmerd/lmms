add_subdirectory(3rdparty)

configure_file("lmmsconfig.h.in"        "${CMAKE_BINARY_DIR}/lmmsconfig.h")

# Provide config flags to lmmsversion.h
get_cmake_property(_define_vars VARIABLES)
foreach(_define_var ${_define_vars})
	if(_define_var MATCHES "^WANT|LMMS_(HAVE|DEBUG)" )
		list(APPEND LMMS_BUILD_OPTIONS "${_define_var}='${${_define_var}}'")
	endif()
endforeach()

# Format for readability
string(REPLACE ";" " " LMMS_BUILD_OPTIONS "${LMMS_BUILD_OPTIONS}")

configure_file("lmmsversion.h.in" "${CMAKE_BINARY_DIR}/lmmsversion.h")

set(LMMS_SRCS "")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(tracks)

list(APPEND LMMS_SRCS ${LMMS_COMMON_SRCS})

qt_wrap_ui(LMMS_UI_OUT ${LMMS_UIS})

if(WIN32 AND MSVC)
	set(WINRC "${CMAKE_BINARY_DIR}/lmms.rc")
elseif(WIN32)
	set(WINRC "${CMAKE_BINARY_DIR}/lmmsrc.obj")
	add_custom_command(
		OUTPUT "${WINRC}"
		COMMAND
			"${CMAKE_RC_COMPILER}"
			"-I\"${CMAKE_SOURCE_DIR}\""
			"-o\"${CMAKE_BINARY_DIR}/lmmsrc.obj\""
			"-i\"${CMAKE_BINARY_DIR}/lmms.rc\""
		DEPENDS "${CMAKE_BINARY_DIR}/lmms.rc"
	)
endif()

include(GenQrc)
add_gen_qrc(LMMS_RCC_OUT lmms.qrc
	"${CMAKE_SOURCE_DIR}/doc/AUTHORS"
	"${CMAKE_SOURCE_DIR}/LICENSE.txt"
	"${CONTRIBUTORS}"
)

add_library(lmmsobjs OBJECT
	${LMMS_SRCS}
	${LMMS_INCLUDES}
	${LMMS_RCC_OUT}
)

target_link_libraries(lmmsobjs PUBLIC lmmsglobal)

# Use libraries in non-standard directories (e.g., another version of Qt)
if(LMMS_BUILD_LINUX)
	target_link_options(lmmsobjs INTERFACE -Wl,--enable-new-dtags)
	set_target_properties(lmmsobjs PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()
set_target_properties(lmmsobjs PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

target_include_directories(lmmsobjs PUBLIC
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_BINARY_DIR}"
	"${CMAKE_BINARY_DIR}/include"
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/include"
)

target_include_directories(lmmsobjs SYSTEM PUBLIC
	${QT_INCLUDE_DIRS}
)

target_include_directories(lmmsobjs SYSTEM PRIVATE
	${JACK_INCLUDE_DIRS}
	${SNDIO_INCLUDE_DIRS}
	${FFTW3F_INCLUDE_DIRS}
	$<$<BOOL:${PULSEAUDIO_INCLUDE_DIR}>:${PULSEAUDIO_INCLUDE_DIR}>
	$<$<BOOL:${SUIL_INCLUDE_DIRS}>:${SUIL_INCLUDE_DIRS}>
)

target_compile_options(lmmsobjs PRIVATE ${PULSEAUDIO_DEFINITIONS})

generate_export_header(lmmsobjs
	BASE_NAME lmms
)

# CMake doesn't define target_EXPORTS for OBJECT libraries.
# See the documentation of DEFINE_SYMBOL for details.
# Also add LMMS_STATIC_DEFINE for targets linking against it.
target_compile_definitions(lmmsobjs
	PRIVATE -Dlmmsobjs_EXPORTS
)
target_static_definitions(lmmsobjs LMMS_STATIC_DEFINE)

set(_extra_libraries "")
if(LMMS_HAVE_WINMM)
	set(_extra_libraries "winmm")
endif()

if(LMMS_BUILD_APPLE)
	set(_extra_libraries "-framework CoreMIDI -framework CoreFoundation")
endif()

if(LMMS_BUILD_HAIKU)
	set(_extra_libraries "-lnetwork")
endif()

if(LMMS_HAVE_LIBRT)
	list(APPEND _extra_libraries "rt")
endif()

if(LMMS_HAVE_PORTAUDIO)
	list(APPEND _extra_libraries portaudio::portaudio)
endif()

if(LMMS_HAVE_MP3LAME)
	list(APPEND _extra_libraries mp3lame::mp3lame)
endif()

if(LMMS_HAVE_OGGVORBIS)
	list(APPEND _extra_libraries Vorbis::vorbisenc Vorbis::vorbisfile)
endif()

if(LMMS_HAVE_LV2)
	list(APPEND _extra_libraries Lilv::lilv)
endif()

set(_required_libraries ${_required_libraries}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${ASOUND_LIBRARY}
	${SDL2_LIBRARY}
	${SNDIO_LIBRARIES}
	${PULSEAUDIO_LIBRARIES}
	${JACK_LIBRARIES}
	${SUIL_LIBRARIES}
	${FFTW3F_LIBRARIES}
	SampleRate::samplerate
	SndFile::sndfile
	${_extra_libraries}
)

target_link_libraries(lmmsobjs PRIVATE
	${_required_libraries}
)

target_static_libraries(lmmsobjs PRIVATE ringbuffer)


add_executable(lmms
	core/main.cpp
	"${WINRC}"
)

target_static_libraries(lmms PUBLIC lmmsobjs)
target_include_directories(lmms PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if(EXISTS "${QT_TRANSLATIONS_DIR}")
	target_compile_definitions(lmms PRIVATE "-DQT_TRANSLATIONS_DIR=\"${QT_TRANSLATIONS_DIR}\"")
endif()

# Set Visual Studio startup project to lmms
# https://stackoverflow.com/a/37994396/8166701
if(NOT CMAKE_VERSION VERSION_LESS 3.6)
	set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lmms)
endif()

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LMMS_RCC_OUT} lmmsconfig.h")

set_target_properties(lmms PROPERTIES
	ENABLE_EXPORTS ON
	WIN32_EXECUTABLE $<NOT:$<CONFIG:DEBUG>>
)

set_target_properties(lmmsobjs PROPERTIES AUTOUIC_SEARCH_PATHS "gui/modals")

if(NOT WIN32 AND NOT LMMS_BUILD_APPLE)
	if(CMAKE_INSTALL_MANDIR)
		set(INSTALL_MANDIR ${CMAKE_INSTALL_MANDIR})
	else()
		set(INSTALL_MANDIR share/man)
	endif()
	install(FILES "${CMAKE_SOURCE_DIR}/doc/lmms.1"
		DESTINATION "${INSTALL_MANDIR}/man1/"
		PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
	)
endif()

install(TARGETS lmms RUNTIME DESTINATION "${BIN_DIR}")
