if(NOT LMMS_HAVE_FLUIDSYNTH)
	return()
endif()

set(CMAKE_AUTOUIC ON)

include(BuildPlugin)
build_plugin(sf2player
	Sf2Player.cpp Sf2Player.h PatchesDialog.cpp PatchesDialog.h PatchesDialog.ui
	MOCFILES Sf2Player.h PatchesDialog.h
	EMBEDDED_RESOURCES *.png
)
target_link_libraries(sf2player PRIVATE fluidsynth SampleRate::samplerate)
