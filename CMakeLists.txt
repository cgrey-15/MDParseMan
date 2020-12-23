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
add_subdirectory(src)
if(MD_PARSEMAN_ENABLE_TESTING)
 add_subdirectory(test)
endif()





# TODO: Add tests and install targets if needed.
