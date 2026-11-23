set_directory_properties(PROPERTIES SYSTEM TRUE)

file(GLOB_RECURSE SOURCES cmt/src/*.cpp)
list(SORT SOURCES)

add_library(cmt MODULE ${SOURCES})
target_include_directories(cmt PRIVATE "${CMAKE_SOURCE_DIR}/include")

install(TARGETS cmt LIBRARY DESTINATION "${PLUGIN_DIR}/ladspa")

set_target_properties(cmt PROPERTIES PREFIX "")

if(NOT MSVC)
	target_compile_options(cmt PRIVATE -O3 -fno-strict-aliasing)
endif()

if(LMMS_BUILD_WIN32)
	add_custom_command(
		TARGET cmt
		POST_BUILD
		COMMAND "${STRIP_COMMAND}" "$<TARGET_FILE:cmt>"
		VERBATIM
		COMMAND_EXPAND_LISTS
	)
endif()

if(MSVC)
	target_link_options(cmt PRIVATE "/EXPORT:ladspa_descriptor")
endif()

if(NOT LMMS_BUILD_WIN32)
	target_compile_options(cmt PRIVATE -fPIC)
endif()

if(NOT LMMS_BUILD_APPLE AND NOT LMMS_BUILD_OPENBSD)
	target_link_libraries(cmt PRIVATE -shared)
endif()

