build_plugin(vstbase
	../vst_base.cpp ../VstPlugin.cpp ../VstPlugin.h ../communication.h
	MOCFILES ../VstPlugin.h
	EXPORT_BASE_NAME vstbase
	LINK SHARED
)

target_compile_definitions(vstbase PUBLIC
	-DREMOTE_VST_PLUGIN_FILEPATH_32="${REMOTE_VST_PLUGIN_FILEPATH_32}"
	-DREMOTE_VST_PLUGIN_FILEPATH_64="${REMOTE_VST_PLUGIN_FILEPATH_64}"
)

if(LMMS_BUILD_LINUX)
	target_compile_definitions(vstbase PUBLIC
		-DNATIVE_LINUX_REMOTE_VST_PLUGIN_FILEPATH_64="${NATIVE_LINUX_REMOTE_VST_PLUGIN_FILEPATH_64}"
	)
endif()

target_include_directories(vstbase PUBLIC
	../
)

if(LMMS_BUILD_LINUX AND NOT WANT_QT6)
	target_link_libraries(vstbase PRIVATE qx11embedcontainer)
endif()
