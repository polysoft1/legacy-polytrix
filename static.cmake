project(PolyTrixStatic)

include_directories(${POLYCHAT})
set(executable_filename ${CMAKE_SHARED_LIBRARY_PREFIX}PolyTrixStatic${CMAKE_SHARED_LIBRARY_SUFFIX})
set(xml_file ${OUTPUT_DIR}/plugin.xml)

ADD_LIBRARY(PolyTrixStatic STATIC ${src})
set_target_properties(PolyTrixStatic PROPERTIES LINKER_LANGUAGE CXX)

get_target_property(STATIC_INCLUDE_DIRS PolyTrixStatic INCLUDE_DIRECTORIES)
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_BINARY_DIR})

set_target_properties(PolyTrixStatic PROPERTIES INCLUDE_DIRECTORIES "${STATIC_INCLUDE_DIRS}")
target_link_libraries(PolyTrixStatic PUBLIC
	MatrixClient::MatrixClient
	coeurl::coeurl
	OpenSSL::Crypto
	OpenSSL::SSL
	Olm::Olm
	nlohmann_json::nlohmann_json)
