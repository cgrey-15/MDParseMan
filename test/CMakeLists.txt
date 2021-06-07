cmake_minimum_required (VERSION 3.8)

configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
 RESULT_VARIABLE result
 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )

if(result)
 message(FATAL_ERROR "CMake step foreach googletest failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
 RESULT_VARIABLE result
 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )

if(result)
 message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

if(CMAKE_VERSION VERSION_LESS 2.8.11)
 include_directories("${gtest_SOURCE_DIR}/include")
endif()

find_package(Python3 COMPONENTS Interpreter)

set(MD_PARSEMAN_TEST_NAMES FirstTests,3 IndentedCodeIndentTests,3 SimpleParagraphTests,3 NestedParagraphTests,3 ListItemTest,3 MoreListItemTest,3 ThematicBreakBasic,3
ThematicBreakMore,3 ListBasic,3 ListMore,3 SetextHeadingBasic,3 SetextHeadingMore,3 ATXHeadingBasic,3 ATXHeadingMore,3 FencedCodeBasic,3 FencedCodeMore,3
#set(MD_PARSEMAN_TEST_NAMES ListItemTest,3
     CACHE STRING "default tests search for followed by numbers telling how many digits that name uses (formatted like 'test_name7777.txt')")
set(MD_PARSEMAN_TEST_JSON_LABEL md_parseman_tests_in CACHE STRING "default json name to generate tests")
set(MD_PARSEMAN_TEST_WORKING_DIR ../rud_test/ CACHE PATH "directory where test text files are located")
set(MD_PARSEMAN_TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/rud_test)

list(LENGTH _MDPM_TESTS_GLOB GLOB_COUNT)
list(LENGTH  MD_PARSEMAN_TEST_NAMES NAMES_COUNT)
if((NOT DEFINED CACHE{_MDPM_TESTS_GLOB}) OR (NOT GLOB_COUNT EQUAL NAMES_COUNT) OR 1)
 unset(_MDPM_TESTS_GLOB CACHE)
 foreach(MDPM_EL IN LISTS MD_PARSEMAN_TEST_NAMES)
  string(REGEX REPLACE "^[^,0-9     ]+,([1-9]|(1[0-9]))$" "\\1" NUM ${MDPM_EL})
  string(REGEX REPLACE "^([^,0-9     ]+),([1-9]|(1[0-9]))$" "\\1" GLOB_NAME ${MDPM_EL})
  string(REPEAT "[0-9]" ${NUM} GLOB_NUM)
  string(CONCAT FINAL_GLOB_NAME "${MD_PARSEMAN_TEST_WORKING_DIR}/${GLOB_NAME}${GLOB_NUM}.txt")
  list(APPEND _MDPM_TESTS_GLOB ${FINAL_GLOB_NAME})
 endforeach()
 set(_MDPM_TESTS_GLOB ${_MDPM_TESTS_GLOB} CACHE INTERNAL "file globs cache" FORCE)
endif()

list(GET _MDPM_TESTS_GLOB 0 SOME_GLOB_NAME)
file(GLOB MD_PARSEMAN_TEST_INPUTS LIST_DIRECTORIES true  ${_MDPM_TESTS_GLOB})

foreach(MDPM_EL IN LISTS MD_PARSEMAN_TEST_INPUTS)
 string(REGEX REPLACE "([^/\\]*[/\\])*([^/\\]+)" "${MD_PARSEMAN_TEST_OUTPUT_DIR}/\\2.out" FILE_NAME ${MDPM_EL})
 list(APPEND MD_PARSEMAN_TEST_FILE_OUTPUTS ${FILE_NAME})
endforeach()

if(WIN32)
 set(CMARK_CMD cmark.exe CACHE PATH "cmark command")
else()
 set(CMARK_CMD cmark CACHE PATH "cmark command")
endif()

add_custom_command(OUTPUT ${MD_PARSEMAN_TEST_FILE_OUTPUTS}
 COMMAND ${CMAKE_COMMAND} -E make_directory ${MD_PARSEMAN_TEST_OUTPUT_DIR}
 COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/rud_test/gen_test_data.py" -d ${CMAKE_SOURCE_DIR}/rud_test/ 
 -c ${CMARK_CMD} -o ${MD_PARSEMAN_TEST_OUTPUT_DIR} ${MD_PARSEMAN_TEST_NAMES}
 DEPENDS ${MD_PARSEMAN_TEST_INPUTS}
)

add_custom_command(OUTPUT ${MD_PARSEMAN_TEST_JSON_LABEL}.json
 COMMAND md_gentest_json -s ${CMAKE_BINARY_DIR}/rud_test -o ${MD_PARSEMAN_TEST_JSON_LABEL} -d ./ ${MD_PARSEMAN_TEST_NAMES}
 DEPENDS ${MD_PARSEMAN_TEST_FILE_OUTPUTS}
)

add_custom_command(OUTPUT MarkdownTestGenerated.cpp
COMMAND markdown_gentest ARGS ${MD_PARSEMAN_TEST_JSON_LABEL}.json 
MAIN_DEPENDENCY ${MD_PARSEMAN_TEST_JSON_LABEL}.json DEPENDS markdown_gentest ${MD_PARSEMAN_TEST_FILE_OUTPUTS})

if(WIN32 AND USE_OSS_MICROSOFT_STL)
 set(CMAKE_VS_JUST_MY_CODE_DEBUGGING 1)
endif()

add_executable(MDParseManTest  MarkdownTestGenerated.cpp)
target_compile_features(MDParseManTest PRIVATE cxx_std_20)
target_link_libraries(MDParseManTest PRIVATE MDParseMan gtest_main gmock)

if(NOT CMAKE_HOST_WIN32)
 set(CMAKE_HOST_WIN32 $<NOT:${CMAKE_HOST_UNIX}>)
endif()
set(COPY_CMD "$<IF:${CMAKE_HOST_WIN32},copy,cp>")


if(WIN32 AND USE_OSS_MICROSOFT_STL)
 set(CMAKE_VS_JUST_MY_CODE_DEBUGGING 1)
 target_include_directories(MDParseManTest SYSTEM BEFORE PRIVATE ${MICROSOFT_STL_BINARY_DIR}/out/inc)
 target_link_libraries(MDParseManTest PRIVATE msvcprt)
 add_custom_command(TARGET MDParseManTest POST_BUILD
  COMMAND ${COPY_CMD} $<SHELL_PATH:${MICROSOFT_STL_BINARY_DIR}/out/bin/${MICROSOFT_STL_ARCHITECTURE_NAME}/msvcp140$<IF:$<CONFIG:Debug>,d,"">_oss.dll> $<SHELL_PATH:$<TARGET_FILE_DIR:MDParseManTest>>
 )
endif()


add_test(NAME markdown_test COMMAND MDParseManTest)
