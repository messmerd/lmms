include(BuildPlugin)
build_plugin(slicert
	SlicerT.cpp
	SlicerT.h
	SlicerTView.cpp
	SlicerTView.h
	SlicerTWaveform.cpp
	SlicerTWaveform.h
	MOCFILES SlicerT.h SlicerTView.h SlicerTWaveform.h
	EMBEDDED_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
)

target_include_directories(slicert SYSTEM PRIVATE ${FFTW3F_INCLUDE_DIRS})
target_link_libraries(slicert PRIVATE ${FFTW3F_LIBRARIES} SampleRate::samplerate)
