include(BuildPlugin)

build_plugin(analyzer
	Analyzer.cpp SaProcessor.cpp SaControls.cpp SaControlsDialog.cpp SaSpectrumView.cpp SaWaterfallView.cpp
	MOCFILES SaProcessor.h SaControls.h SaControlsDialog.h SaSpectrumView.h SaWaterfallView.h DataprocLauncher.h
	EMBEDDED_RESOURCES *.svg
)

target_link_libraries(analyzer PRIVATE ${FFTW3F_LIBRARIES} ringbuffer)
target_include_directories(analyzer SYSTEM PRIVATE ${FFTW3F_INCLUDE_DIRS})
