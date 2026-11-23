include(BuildPlugin)

build_plugin(vectorscope
	Vectorscope.cpp VecControls.cpp VecControlsDialog.cpp VectorView.cpp
	MOCFILES VecControls.h VecControlsDialog.h VectorView.h
)

target_link_libraries(vectorscope PRIVATE ringbuffer)
