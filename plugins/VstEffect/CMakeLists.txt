if(NOT LMMS_HAVE_VST)
	return()
endif()

include(BuildPlugin)
build_plugin(vsteffect
	VstEffect.cpp VstEffectControls.cpp VstEffectControlDialog.cpp VstSubPluginFeatures.cpp
	VstEffect.h VstEffectControls.h VstEffectControlDialog.h VstSubPluginFeatures.h
	MOCFILES VstEffectControlDialog.h VstEffectControls.h
	EMBEDDED_RESOURCES *.png *.svg
)

target_link_libraries(vsteffect PRIVATE vstbase)
target_link_directories(vsteffect PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/..")

if(LMMS_BUILD_LINUX)
	target_link_options(vsteffect PRIVATE -Wl,--enable-new-dtags)
	set_target_properties(vsteffect PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN"
	)
else()
	set_target_properties(vsteffect PROPERTIES
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PLUGIN_DIR}"
	)
endif()
