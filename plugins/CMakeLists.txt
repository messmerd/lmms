set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_DEBUG_POSTFIX "")

# See cmake/modules/PluginList.cmake
foreach(_plugin ${PLUGIN_LIST})
	add_subdirectory(${_plugin})
endforeach()
