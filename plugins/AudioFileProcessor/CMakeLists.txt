include(BuildPlugin)

build_plugin(audiofileprocessor
	AudioFileProcessor.cpp AudioFileProcessor.h AudioFileProcessorView.cpp AudioFileProcessorView.h AudioFileProcessorWaveView.cpp AudioFileProcessorWaveView.h
	MOCFILES AudioFileProcessor.h AudioFileProcessorView.h AudioFileProcessorWaveView.h
	EMBEDDED_RESOURCES *.png
)
