# CMakeList.txt : CMake project for MDParseMan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(pegtl CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 set(CMAKE_VS_JUST_MY_CODE_DEBUGGING 1)
endif()

# Add source to this project's executable.
set(MD_PARSEMAN_SOURCES
MDParseImpl.h 
"BlockLogic.cpp" 
MDParseImpl.cpp
"${CMAKE_SOURCE_DIR}/include/md_parseman/MDParseMan.h"
"${CMAKE_SOURCE_DIR}/include/md_parseman/BlockInfoTags.h"
"${CMAKE_SOURCE_DIR}/include/md_parseman/IntegralTypes.h"
HTMLExport.cpp
"NodeIterator.cpp"
)
add_library (MDParseMan SHARED ${MD_PARSEMAN_SOURCES})#MDParseImpl.h Parser.cpp MDParseImpl.cpp "${CMAKE_SOURCE_DIR}/include/md_parseman/MDParseMan.h" "${CMAKE_SOURCE_DIR}/include/md_parseman/BlockInfoTags.h" "HTMLExport.cpp" "NodeIterator.cpp")
include(GenerateExportHeader)
generate_export_header(MDParseMan)
target_include_directories(MDParseMan PUBLIC "${CMAKE_SOURCE_DIR}/include" $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>)
target_link_libraries(MDParseMan PRIVATE taocpp::pegtl)
target_compile_features(MDParseMan PUBLIC cxx_std_20)

add_executable (mdsprig "mdsprig.cpp" )
target_link_libraries(mdsprig PRIVATE CLI11::CLI11 MDParseMan)
target_include_directories(mdsprig PUBLIC "${CMAKE_SOURCE_DIR}/include" )
target_compile_features(mdsprig PRIVATE cxx_std_20)


if(WIN32 AND USE_OSS_MICROSOFT_STL)
 message(STATUS "Using OSS Microsoft STL library and headers")
 if(NOT MICROSOFT_STL_BINARY_DIR)
  message(SEND_ERROR "Please specify build root with -DMICROSOFT_STL_BINARY_DIR")
 endif()
 if(NOT MICROSOFT_STL_ARCHITECTURE_NAME)
  message(SEND_ERROR "Please specify the -DMICROSOFT_STL_ARCHITECTURE_NAME directory name that holds the library and library binaries (e.g. 'arm64')")
 endif()

 target_include_directories(MDParseMan SYSTEM BEFORE PUBLIC ${MICROSOFT_STL_BINARY_DIR}/out/inc)
 target_include_directories(mdsprig SYSTEM BEFORE PRIVATE ${MICROSOFT_STL_BINARY_DIR}/out/inc)

 add_library(msvcprt SHARED IMPORTED)
 set_target_properties(msvcprt PROPERTIES IMPORTED_IMPLIB ${MICROSOFT_STL_BINARY_DIR}/out/lib/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcprt.lib)
 set_target_properties(msvcprt PROPERTIES IMPORTED_IMPLIB_DEBUG ${MICROSOFT_STL_BINARY_DIR}/out/lib/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcprtd.lib)
 set_target_properties(msvcprt PROPERTIES IMPORTED_LOCATION ${MICROSOFT_STL_BINARY_DIR}/out/bin/${MICROSOFT_STL_ARCHITECTURE_NAME})
 target_link_libraries(MDParseMan PUBLIC msvcprt)
 target_link_libraries(mdsprig PRIVATE msvcprt)
else()
 message(STATUS "Using cxx standard library and headers from default toolset")
endif()

if(NOT CMAKE_HOST_WIN32)
	set(CMAKE_HOST_WIN32 $<NOT:${CMAKE_HOST_UNIX}>)
endif()

set(COPY_CMD "$<IF:${CMAKE_HOST_WIN32},copy,cp>")

add_custom_command(TARGET MDParseMan POST_BUILD
 COMMAND ${COPY_CMD} $<SHELL_PATH:$<TARGET_FILE:MDParseMan>>  $<SHELL_PATH:${CMAKE_BINARY_DIR}/test>#${CMAKE_BINARY_DIR}/test>
)
if(WIN32)
 add_custom_command(TARGET MDParseMan POST_BUILD
  COMMAND ${COPY_CMD} $<SHELL_PATH:$<TARGET_PDB_FILE:MDParseMan>> $<SHELL_PATH:${CMAKE_BINARY_DIR}/test>#${CMAKE_BINARY_DIR}/test>
 )
endif()

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 add_custom_command(TARGET mdsprig POST_BUILD
 COMMAND ${COPY_CMD} $<SHELL_PATH:${MICROSOFT_STL_BINARY_DIR}/out/bin/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcp140$<IF:$<CONFIG:Debug>,d,"">_oss.dll> $<SHELL_PATH:$<TARGET_FILE_DIR:mdsprig>>
)
endif()

# TODO: Add tests and install targets if needed.

