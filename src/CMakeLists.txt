# CMakeList.txt : CMake project for MDParseMan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(CLI11 CONFIG REQUIRED)

add_executable (mdsprig "mdsprig.cpp" )
target_link_libraries(mdsprig PRIVATE CLI11::CLI11 MDParseMan)
target_compile_features(mdsprig PRIVATE cxx_std_20)

if(WIN32 AND USE_OSS_MICROST_STL)
 target_include_directories(mdsprig SYSTEM BEFORE PRIVATE ${MICROSOFT_STL_BINARY_DIR}/out/inc)
 target_link_libraries(mdsprig PRIVATE msvcprt)
endif()

if(NOT CMAKE_HOST_WIN32)
 set(CMAKE_HOST_WIN32 $<NOT:${CMAKE_HOST_UNIX}>)
endif()

set(COPY_CMD "$<IF:${CMAKE_HOST_WIN32},copy,cp>")

#add_custom_command(TARGET MDParseMan POST_BUILD
# COMMAND ${COPY_CMD} $<SHELL_PATH:$<TARGET_FILE:MDParseMan>>  $<SHELL_PATH:${CMAKE_BINARY_DIR}/test>#${CMAKE_BINARY_DIR}/test>
#)
#if(WIN32)
# add_custom_command(TARGET MDParseMan POST_BUILD
#  COMMAND ${COPY_CMD} $<SHELL_PATH:$<TARGET_PDB_FILE:MDParseMan>> $<SHELL_PATH:${CMAKE_BINARY_DIR}/test>#${CMAKE_BINARY_DIR}/test>
# )
#endif()

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 add_custom_command(TARGET mdsprig POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy $<SHELL_PATH:${MICROSOFT_STL_BINARY_DIR}/out/bin/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcp140$<IF:$<CONFIG:Debug>,d,"">_oss.dll> $<SHELL_PATH:$<TARGET_FILE_DIR:mdsprig>>
)
endif()

# TODO: Add tests and install targets if needed.


