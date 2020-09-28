project(PolyTrixShared)

include_directories(${POLYCHAT})
set(executable_filename ${CMAKE_SHARED_LIBRARY_PREFIX}PolyTrix${CMAKE_SHARED_LIBRARY_SUFFIX})
set(json_file ${OUTPUT_DIR}/plugin.json)

ADD_LIBRARY(PolyTrix SHARED ${src})

get_target_property(DYN_INCLUDE_DIRS PolyTrix INCLUDE_DIRECTORIES)
list(APPEND DYN_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
list(APPEND DYN_INCLUDE_DIRS ${PROJECT_BINARY_DIR})
#if(DEFINED POLYCHAT)
	#target_include_directories(PolyTrix PUBLIC ${POLYCHAT_INCLUDE})
	#list(APPEND DYN_INCLUDE_DIRS ${POLYCHAT})
	#message("Including ${POLYCHAT}")
	#target_link_libraries(PolyTrix ${POLYCHAT}/target/${CMAKE_SHARED_LIBRARY_PREFIX}PolyChat${CMAKE_SHARED_LIBRARY_SUFFIX})
#endif()
set_target_properties(PolyTrix PROPERTIES INCLUDE_DIRECTORIES "${DYN_INCLUDE_DIRS}")

add_custom_target(json_creation)
add_dependencies(json_creation PolyTrix)
add_custom_command(TARGET json_creation
	POST_BUILD
	COMMAND cmake -DHOME="${CMAKE_HOME_DIRECTORY}" -DEXECUTABLE="${executable_filename}" -DOUT_DIR="${OUTPUT_DIR}" -DJSON_FILE="plugin.json" -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" -P ${CMAKE_HOME_DIRECTORY}/jsoncreate.cmake
) 

add_custom_target(create_zip
	ALL COMMAND ${CMAKE_COMMAND} -E tar "cfv" "PolyTrix.zip" --format=zip "${executable_filename}" "${json_file}"
	WORKING_DIRECTORY ${OUTPUT_DIR}
)
add_dependencies(create_zip json_creation PolyTrix)

target_link_libraries(PolyTrix PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(PolyTrix PRIVATE libmatrix-client-static)

target_link_libraries(PolyTrix PRIVATE ${LIBMATRIX_CLIENT})
