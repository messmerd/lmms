include(BuildPlugin)
build_plugin(slewdistortion
	SlewDistortion.cpp SlewDistortionControls.cpp SlewDistortionControlDialog.cpp
	MOCFILES SlewDistortion.h SlewDistortionControls.h SlewDistortionControlDialog.h
	EMBEDDED_RESOURCES *.png
)
target_link_libraries(slewdistortion PRIVATE hiir)
