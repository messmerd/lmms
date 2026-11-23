add_library(hiir INTERFACE)
target_include_directories(hiir INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hiir INTERFACE lmmsglobal)
