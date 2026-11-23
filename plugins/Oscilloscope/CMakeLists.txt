include(BuildPlugin)

build_plugin(oscilloscope
	Oscilloscope.cpp OscilloscopeControls.cpp OscilloscopeControlDialog.cpp OscilloscopeGraph.cpp
	MOCFILES OscilloscopeControls.h OscilloscopeControlDialog.h OscilloscopeGraph.h
)

target_link_libraries(oscilloscope PRIVATE ringbuffer)
