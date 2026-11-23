include(BuildPlugin)

build_plugin(amplifier
	Amplifier.cpp AmplifierControls.cpp AmplifierControlDialog.cpp
	MOCFILES AmplifierControls.h AmplifierControlDialog.h
	EMBEDDED_RESOURCES artwork.svg
)
