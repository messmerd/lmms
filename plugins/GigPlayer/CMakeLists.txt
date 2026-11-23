if(NOT LMMS_HAVE_GIG)
	return()
endif()

set(CMAKE_AUTOUIC ON)

include(BuildPlugin)
build_plugin(gigplayer
	GigPlayer.cpp GigPlayer.h PatchesDialog.cpp PatchesDialog.h PatchesDialog.ui
	MOCFILES GigPlayer.h PatchesDialog.h
	EMBEDDED_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
)
target_link_libraries(gigplayer PRIVATE SampleRate::samplerate)

# Required for not crashing loading files with libgig
target_compile_options(gigplayer PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>)

target_include_directories(gigplayer SYSTEM PRIVATE ${GIG_INCLUDE_DIRS})
target_link_directories(gigplayer PRIVATE ${GIG_LIBRARY_DIRS})
target_link_libraries(gigplayer PRIVATE ${GIG_LIBRARIES})
