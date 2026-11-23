if(NOT LMMS_HAVE_VST)
	return()
endif()

include(BuildPlugin)
build_plugin(vestige
	Vestige.cpp Vestige.h
	MOCFILES Vestige.h
	EMBEDDED_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.png;*.svg"
)

target_link_libraries(vestige PRIVATE vstbase)
target_link_directories(vestige PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/..")

if(LMMS_BUILD_LINUX)
	target_link_options(vestige PRIVATE -Wl,--enable-new-dtags)
	set_target_properties(vestige PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN"
	)
else()
	set_target_properties(vestige PROPERTIES
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PLUGIN_DIR}"
	)
endif()

