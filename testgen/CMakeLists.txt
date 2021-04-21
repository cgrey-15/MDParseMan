cmake_minimum_required(VERSION 3.8)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CONFIGURATION_TYPES RelWithDebInfo Debug)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

add_executable(markdown_gentest Main.cpp "StringParts.h")
target_compile_features(markdown_gentest PRIVATE cxx_std_17)
target_link_libraries(markdown_gentest PRIVATE nlohmann_json::nlohmann_json fmt::fmt)

add_executable(md_gentest_json "Main2.cpp" "Main2a.cpp")
target_compile_features(md_gentest_json PRIVATE cxx_std_17)
find_path(CTRE_INCLUDE_DIRS "ctll.hpp")
target_include_directories(md_gentest_json PRIVATE ${CTRE_INCLUDE_DIRS})
target_link_libraries(md_gentest_json PRIVATE CLI11::CLI11 nlohmann_json::nlohmann_json)



