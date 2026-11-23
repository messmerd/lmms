if(NOT LMMS_HAVE_LV2)
	return()
endif()

include(BuildPlugin)
build_plugin(lv2instrument
	Lv2Instrument.cpp Lv2Instrument.h
	MOCFILES Lv2Instrument.h
	EMBEDDED_RESOURCES logo.png
)

target_link_libraries(lv2instrument PRIVATE Lilv::lilv)
target_include_directories(lv2instrument PRIVATE ${SUIL_INCLUDE_DIRS})
