include(BuildPlugin)

build_plugin(eq
	EqEffect.cpp EqCurve.cpp EqCurve.h EqControls.cpp EqControlsDialog.cpp EqFilter.h EqParameterWidget.cpp EqFader.h EqSpectrumView.h EqSpectrumView.cpp
	MOCFILES EqControls.h EqControlsDialog.h EqCurve.h EqParameterWidget.h EqFader.h EqSpectrumView.h
	EMBEDDED_RESOURCES *.png
)

target_include_directories(eq SYSTEM PRIVATE ${FFTW3F_INCLUDE_DIRS})
target_link_libraries(eq PRIVATE ${FFTW3F_LIBRARIES})
