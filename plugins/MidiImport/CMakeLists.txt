include(BuildPlugin)

# For now, portsmf files are included directly in the plugin
build_plugin(midiimport
	MidiImport.cpp MidiImport.h
	portsmf/include/allegro.h portsmf/src/allegro.cpp
	portsmf/include/mfmidi.h portsmf/src/mfmidi.cpp
	portsmf/include/strparse.h portsmf/src/strparse.cpp
	# portsmf/src/allegrosmfwr.cpp portsmf/src/allegrowr.cpp
	portsmf/src/allegrosmfrd.cpp portsmf/src/allegrord.cpp
	portsmf/src/algsmfrd_internal.h portsmf/src/algsmfrd_internal.h
	MOCFILES MidiImport.h
)

target_include_directories(midiimport PRIVATE "portsmf/include")

if(NOT MSVC)
	# portsmf raises these warnings, which will prevent compilation with -Werror
	target_compile_options(midiimport PRIVATE
		-Wno-error=unknown-pragmas
		-Wno-error=maybe-uninitialized
		-Wno-error=restrict
	)
endif()
