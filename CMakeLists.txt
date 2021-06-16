# CMakeList.txt : CMake project for MDParseMan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


project ("MDParseMan")

option(MD_PARSEMAN_ENABLE_TESTING "Use testing for MDParseMan" ON)
option(USE_OSS_MICROSOFT_STL "Using Microsoft from microsoft/STL on GitHub" ON)

# Add source to this project's executable.
if(MD_PARSEMAN_ENABLE_TESTING)
 add_subdirectory(testgen)
endif()

find_package(pegtl CONFIG REQUIRED)

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 set(CMAKE_VS_JUST_MY_CODE_DEBUGGING 1)
endif()

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# Add source to this project's executable.
set(MD_PARSEMAN_SOURCES
src/MDParseImpl.h 
"src/BlockLogic.cpp" 
src/MDParseImpl.cpp
"src/md_parseman/MDParseMan.h"
"src/md_parseman/BlockInfoTags.h"
"src/md_parseman/IntegralTypes.h"
src/HTMLExport.cpp
"src/NodeIterator.cpp"
)
set( MD_PARSEMAN_HEADERS "src/md_parseman/MDParseMan.h"
"src/md_parseman/BlockInfoTags.h"
"src/md_parseman/IntegralTypes.h")

add_library (MDParseMan SHARED ${MD_PARSEMAN_SOURCES} ${MD_PARSEMAN_HEADERS})
include(GenerateExportHeader)
generate_export_header(MDParseMan)

target_include_directories(MDParseMan PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src> 
$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}> $<INSTALL_INTERFACE:include>)
target_link_libraries(MDParseMan PRIVATE taocpp::pegtl)
target_compile_features(MDParseMan PUBLIC cxx_std_20)

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 message(STATUS "Using OSS Microsoft STL library and headers")
 if(NOT MICROSOFT_STL_BINARY_DIR)
  message(SEND_ERROR "Please specify build root with -DMICROSOFT_STL_BINARY_DIR")
 endif()
 if(NOT MICROSOFT_STL_ARCHITECTURE_NAME)
  message(SEND_ERROR "Please specify the -DMICROSOFT_STL_ARCHITECTURE_NAME directory name that holds the library and library binaries (e.g. 'arm64')")
 endif()

 target_include_directories(MDParseMan SYSTEM BEFORE PUBLIC ${MICROSOFT_STL_BINARY_DIR}/out/inc)

 add_library(msvcprt SHARED IMPORTED)
 set_target_properties(msvcprt PROPERTIES IMPORTED_IMPLIB ${MICROSOFT_STL_BINARY_DIR}/out/lib/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcprt.lib)
 set_target_properties(msvcprt PROPERTIES IMPORTED_IMPLIB_DEBUG ${MICROSOFT_STL_BINARY_DIR}/out/lib/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcprtd.lib)
 set_target_properties(msvcprt PROPERTIES IMPORTED_LOCATION ${MICROSOFT_STL_BINARY_DIR}/out/bin/${MICROSOFT_STL_ARCHITECTURE_NAME})
 target_link_libraries(MDParseMan PUBLIC msvcprt)
else()
 message(STATUS "Using cxx standard library and headers from default toolset")
endif()

add_subdirectory(src)
if(MD_PARSEMAN_ENABLE_TESTING)
 enable_testing()
 add_subdirectory(test)
 #add_custom_command(TARGET MDParseMan POST_BUILD
 # COMMAND ${CMAKE_COMMAND} -E copy $<SHELL_PATH:$<TARGET_FILE:MDParseMan>> $<TARGET_FILE_DIR:MDParseManTest> #$<SHELL_PATH:${CMAKE_BINARY_DIR}/test>#${CMAKE_BINARY_DIR}/test>
 #)
endif()





# TODO: Add tests and install targets if needed.
