cmake_minimum_required(VERSION 3.16)
project("discordsync")
add_library(discordsync MODULE "main.cpp" "ReadCFG.cpp" )
set_target_properties(discordsync PROPERTIES PREFIX "")
target_include_directories(discordsync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_BUILD_TYPE Release)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(discordsync PROPERTIES OUTPUT_NAME "discordsync04rel32")
elseif(FORCE_32)
	set_target_properties(discordsync PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
	set_target_properties(discordsync PROPERTIES OUTPUT_NAME "discordsync04rel32")
else()
	set_target_properties(discordsync PROPERTIES OUTPUT_NAME "discordsync04rel64")
endif()

add_custom_command(TARGET discordsync POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:discordsync>"
     "${CMAKE_CURRENT_SOURCE_DIR}/out/binaries/$<TARGET_FILE_NAME:discordsync>" 
  COMMENT "Copying to output directory")


if(WIN32)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
add_compile_definitions(CURL_STATICLIB)
target_link_directories(discordsync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT WIN32)
target_link_libraries(discordsync  ${CURL_LIBRARY} ${OPENSSL}  ${CRYPTO} )
message(STATUS  " " ${CURL_LIBRAY} " " ${OPENSSL} " " ${CRYPTO} )
else()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
target_link_libraries(discordsync ${CURL_LIBRARY} ws2_32.lib wldap32.lib crypt32.lib )
else()
target_link_libraries(discordsync ${CURL_LIBRARY} ws2_32.lib wldap32.lib crypt32.lib advapi32.lib )
message(STATUS ${CURL_LIBRARY})
endif()
endif()