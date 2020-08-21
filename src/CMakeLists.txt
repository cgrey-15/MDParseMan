# CMakeList.txt : CMake project for MDParseMan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Add source to this project's executable.
add_executable (MDParseMan "MDParseMan.cpp" "MDParseMan.h" "Parser.cpp" "TFrame.cpp")

target_link_libraries(MDParseMan PRIVATE nlohmann_json::nlohmann_json cxxopts::cxxopts fmt::fmt)
target_compile_features(MDParseMan PRIVATE cxx_std_20)

add_custom_target(genexdebug COMMAND ${CMAKE_COMMAND} -E echo "$<IF:${CMAKE_HOST_WIN32},copy /b,cp>")

#message(WARNING $<IF:$<CONFIG:Debug>, hello, goodbye> )
set(COPY_COMMAND "$<IF:${CMAKE_HOST_WIN32},copy,cp>")
#if(CMAKE_HOST_WIN32)
#set(COPY_COMMAND copy /b)
#else()
#set(COPY_COMMAND cp)
#endif()

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/src/test.txt
COMMAND ${COPY_COMMAND} $<SHELL_PATH:${CMAKE_SOURCE_DIR}/rud_test/test.txt> $<SHELL_PATH:${CMAKE_BINARY_DIR}/src>)
add_custom_command(TARGET MDParseMan PRE_BUILD
COMMAND ${COPY_COMMAND} $<SHELL_PATH:${CMAKE_SOURCE_DIR}/rud_test/spec.json> $<SHELL_PATH:${CMAKE_BINARY_DIR}/src>)
add_custom_command(TARGET MDParseMan PRE_BUILD
COMMAND ${COPY_COMMAND} $<SHELL_PATH:${CMAKE_SOURCE_DIR}/rud_test/Testy.txt> $<SHELL_PATH:${CMAKE_BINARY_DIR}/src>)
add_custom_command(TARGET MDParseMan PRE_BUILD
COMMAND ${COPY_COMMAND} $<SHELL_PATH:${CMAKE_SOURCE_DIR}/rud_test/Markdawny.txt> $<SHELL_PATH:${CMAKE_BINARY_DIR}/src>)
add_custom_command(TARGET MDParseMan PRE_BUILD
COMMAND ${COPY_COMMAND} $<SHELL_PATH:${CMAKE_SOURCE_DIR}/rud_test/Markdawny2.txt> $<SHELL_PATH:${CMAKE_BINARY_DIR}/src>)

add_custom_target(TestingTarg DEPENDS ${CMAKE_BINARY_DIR}/src/test.txt)
add_dependencies(MDParseMan TestingTarg)

# TODO: Add tests and install targets if needed.
