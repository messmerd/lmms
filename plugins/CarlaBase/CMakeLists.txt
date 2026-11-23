# If Carla was not provided by the system, make a dummy library instead
if(LMMS_HAVE_WEAKCARLA)
  SET(CARLA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/carla/source
    ${CMAKE_CURRENT_SOURCE_DIR}/carla/source/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/carla/source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/carla/source/backend
  )

  # force "lib" prefix
  IF(LMMS_BUILD_WIN32)
      SET(CMAKE_SHARED_LIBRARY_PREFIX "lib")
  ENDIF()

  SET(CARLA_NATIVE_LIB carla_native-plugin)

  ADD_LIBRARY(${CARLA_NATIVE_LIB} SHARED DummyCarla.cpp)
  target_include_directories(${CARLA_NATIVE_LIB} SYSTEM PUBLIC ${CARLA_INCLUDE_DIRS})
  INSTALL(TARGETS ${CARLA_NATIVE_LIB}
    LIBRARY DESTINATION "${PLUGIN_DIR}/optional"
    RUNTIME DESTINATION "${PLUGIN_DIR}/optional"
  )
  SET(CARLA_LIBRARIES ${CARLA_NATIVE_LIB})
  # Set parent scope variables so carlarack and carlapatchbay can see them
  SET(CARLA_LIBRARIES ${CARLA_LIBRARIES} PARENT_SCOPE)
endif()

if(LMMS_HAVE_CARLA OR LMMS_HAVE_WEAKCARLA)
	# Mimic the missing "config.h"
	set(CARLA_INCLUDE_DIRS ${CARLA_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/CarlaConfig)

	include(BuildPlugin)
	build_plugin(carlabase Carla.cpp Carla.h
		MOCFILES Carla.h
		EMBEDDED_RESOURCES artwork-patchbay.png artwork-rack.png
		EXPORT_BASE_NAME carlabase
		LINK SHARED
	)

	target_include_directories(carlabase SYSTEM PUBLIC
		${CARLA_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_directories(carlabase PUBLIC
		${CARLA_LIBRARY_DIRS}
		${CMAKE_CURRENT_BINARY_DIR}
	)
	target_link_libraries(carlabase PUBLIC
		${CARLA_LIBRARIES}
	)
	target_compile_definitions(carlabase PUBLIC
		-DCARLA_PLUGIN_SYNTH
	)

	SET_TARGET_PROPERTIES(carlabase
		PROPERTIES SKIP_BUILD_RPATH TRUE
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH_USE_LINK_PATH TRUE
		INSTALL_RPATH "${CARLA_RPATH}"
	)
	if(LMMS_HAVE_WEAKCARLA)
		add_dependencies(carlabase ${CARLA_NATIVE_LIB})
	endif()
endif()
