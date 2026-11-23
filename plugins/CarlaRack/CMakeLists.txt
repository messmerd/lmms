if(LMMS_HAVE_CARLA OR LMMS_HAVE_WEAKCARLA)
	include(BuildPlugin)
	build_plugin(carlarack
		CarlaRack.cpp
		EMBEDDED_RESOURCES logo.png
	)
	target_link_libraries(carlarack PRIVATE carlabase)
	target_compile_definitions(carlarack PRIVATE CARLA_PLUGIN_RACK)
endif()
