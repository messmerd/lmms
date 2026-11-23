set_directory_properties(PROPERTIES SYSTEM TRUE)

execute_process(
    COMMAND ${PERL_EXECUTABLE} -MList::MoreUtils -e "1"
    RESULT_VARIABLE MOREUTILS_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if (NOT MOREUTILS_RESULT EQUAL 0)
    message(FATAL_ERROR "Perl module \"List::MoreUtils\" is required for building LadspaEffect/swh.\n")
endif()

# Create blank config.h
FILE(WRITE ladspa/config.h "")

# Common settings
add_library(swh_common INTERFACE)
target_link_libraries(swh_common INTERFACE lmmsglobal)
target_include_directories(swh_common INTERFACE
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/ladspa"
	"${FFTW3F_INCLUDE_DIRS}"
	"${CMAKE_BINARY_DIR}"
)
target_compile_definitions(swh_common INTERFACE -DFFTW3)

# Additional compile flags
target_compile_options(swh_common INTERFACE
	$<$<NOT:$<C_COMPILER_ID:MSVC>>:-O3 -funroll-loops -ffast-math -fno-strict-aliasing>
)


# Loop over every XML file
FILE(GLOB XML_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/*.xml")
LIST(SORT XML_SOURCES)

FOREACH(_item ${XML_SOURCES})
	# Get library name and (soon to be) C file
	GET_FILENAME_COMPONENT(_plugin "${_item}" NAME_WE)
	SET(_out_file "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/${_plugin}.c")

	# Coerce XML source file to C
	ADD_CUSTOM_COMMAND(
		OUTPUT "${_out_file}"
		COMMAND "${PERL_EXECUTABLE}" ./makestub.pl "${_item}" > "${_out_file}"
		DEPENDS "${_item}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ladspa"
		VERBATIM
	)

	# Add a library target for this C file, which depends on success of makestup.pl
	ADD_LIBRARY("${_plugin}" MODULE "${_out_file}")

	target_link_libraries("${_plugin}" PRIVATE swh_common)

	if(MSVC)
		target_link_options("${_plugin}" PRIVATE "/EXPORT:ladspa_descriptor")
	endif()

	# Vocoder does not use fftw
	IF(NOT ("${_plugin}" STREQUAL "vocoder_1337"))
		TARGET_LINK_LIBRARIES("${_plugin}" PRIVATE ${FFTW3F_LIBRARIES})
	ENDIF()
	IF(NOT MSVC)
		TARGET_LINK_LIBRARIES("${_plugin}" PRIVATE m)
	ENDIF()

	if(LMMS_BUILD_WIN32)
		add_custom_command(
			TARGET "${_plugin}"
			POST_BUILD
			COMMAND "${STRIP_COMMAND}" "$<TARGET_FILE:${_plugin}>"
			VERBATIM
			COMMAND_EXPAND_LISTS
		)
	endif(LMMS_BUILD_WIN32)

	SET_TARGET_PROPERTIES("${_plugin}" PROPERTIES PREFIX "")
	target_compile_options("${_plugin}" PRIVATE "${COMPILE_FLAGS}")
	INSTALL(TARGETS "${_plugin}" LIBRARY DESTINATION "${PLUGIN_DIR}/ladspa")
ENDFOREACH()

add_library(iir STATIC ladspa/util/iir.c)
target_link_libraries(iir PRIVATE swh_common)
target_link_libraries(bandpass_a_iir_1893 PRIVATE iir)
target_link_libraries(bandpass_iir_1892   PRIVATE iir)
target_link_libraries(butterworth_1902    PRIVATE iir)
target_link_libraries(highpass_iir_1890   PRIVATE iir)
target_link_libraries(lowpass_iir_1891    PRIVATE iir)
target_link_libraries(notch_iir_1894      PRIVATE iir)

FILE(GLOB GSM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ladspa/gsm/*.c")
LIST(SORT GSM_SOURCES)
add_library(gsm STATIC ${GSM_SOURCES})
target_link_libraries(gsm PRIVATE swh_common)
target_link_libraries(gsm_1215 PRIVATE gsm)

add_library(gverb STATIC ladspa/gverb/gverb.c ladspa/gverb/gverbdsp.c)
target_link_libraries(gverb PRIVATE swh_common)
target_link_libraries(gverb_1216 PRIVATE gverb)

add_library(blo STATIC ladspa/util/blo.c)
target_link_libraries(blo PRIVATE swh_common)
target_link_libraries(analogue_osc_1416  PRIVATE blo)
target_link_libraries(fm_osc_1415        PRIVATE blo)
target_link_libraries(hermes_filter_1200 PRIVATE blo)

add_library(rms STATIC ladspa/util/rms.c)
add_library(db STATIC ladspa/util/db.c)
target_link_libraries(rms PRIVATE swh_common)
target_link_libraries(db PRIVATE swh_common)
target_link_libraries(sc1_1425  PRIVATE rms db)
target_link_libraries(sc2_1426  PRIVATE rms db)
target_link_libraries(sc3_1427  PRIVATE rms db)
target_link_libraries(sc4_1882  PRIVATE rms db)
target_link_libraries(sc4m_1916 PRIVATE rms db)
target_link_libraries(se4_1883  PRIVATE rms db)

add_library(pitchscale STATIC ladspa/util/pitchscale.c)
target_link_libraries(pitchscale PRIVATE
	swh_common
	${FFTW3F_LIBRARIES}
)
target_link_libraries(pitch_scale_1193 PRIVATE pitchscale)
target_link_libraries(pitch_scale_1194 PRIVATE pitchscale)
