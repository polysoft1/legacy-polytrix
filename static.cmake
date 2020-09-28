project(PolyTrixStatic)

include_directories(${POLYCHAT})
set(executable_filename ${CMAKE_SHARED_LIBRARY_PREFIX}PolyTrixStatic${CMAKE_SHARED_LIBRARY_SUFFIX})
set(xml_file ${OUTPUT_DIR}/plugin.xml)

ADD_LIBRARY(PolyTrixStatic STATIC ${src})
set_target_properties(PolyTrixStatic PROPERTIES LINKER_LANGUAGE CXX)

get_target_property(STATIC_INCLUDE_DIRS PolyTrixStatic INCLUDE_DIRECTORIES)
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_BINARY_DIR})
#if(DEFINED POLYCHAT)
	#target_include_directories(PolyMostStatic PUBLIC ${POLYCHAT_INCLUDE})
	#list(APPEND STATIC_INCLUDE_DIRS ${POLYCHAT})
	#message("Including ${POLYCHAT}")
	#target_link_libraries(PolyMostStatic ${POLYCHAT}/target/${CMAKE_SHARED_LIBRARY_PREFIX}PolyChat${CMAKE_SHARED_LIBRARY_SUFFIX})
#endif()
set_target_properties(PolyTrixStatic PROPERTIES INCLUDE_DIRECTORIES "${STATIC_INCLUDE_DIRS}")
target_link_libraries(PolyTrixStatic PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(PolyTrixStatic PRIVATE libmatrix-client-static)

target_link_libraries(PolyTrixStatic PRIVATE ${LIBMATRIX_CLIENT})