if(NOT LMMS_HAVE_LV2)
	return()
endif()

include(BuildPlugin)
build_plugin(lv2effect
	Lv2Effect.cpp Lv2FxControls.cpp Lv2FxControlDialog.cpp Lv2Effect.h Lv2FxControls.h Lv2FxControlDialog.h
	MOCFILES Lv2Effect.h Lv2FxControls.h Lv2FxControlDialog.h
	EMBEDDED_RESOURCES logo.png
)

target_link_libraries(lv2effect PRIVATE Lilv::lilv)
target_include_directories(lv2effect PRIVATE ${SUIL_INCLUDE_DIRS})
