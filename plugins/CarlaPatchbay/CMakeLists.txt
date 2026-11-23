if(LMMS_HAVE_CARLA OR LMMS_HAVE_WEAKCARLA)
	include(BuildPlugin)
	build_plugin(carlapatchbay
		CarlaPatchbay.cpp
		EMBEDDED_RESOURCES logo.png
	)
	target_link_libraries(carlapatchbay PRIVATE carlabase)
	target_compile_definitions(carlapatchbay PRIVATE CARLA_PLUGIN_PATCHBAY)
endif()
