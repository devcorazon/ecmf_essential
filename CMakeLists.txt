# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.0.1")

# Extract major, minor, and patch numbers from PROJECT_VER
string(REPLACE "." ";" VERSION_LIST ${PROJECT_VER})
list(GET VERSION_LIST 0 ecmf_essential_PROJECT_VER_MAJOR)
list(GET VERSION_LIST 1 ecmf_essential_PROJECT_VER_MINOR)
list(GET VERSION_LIST 2 ecmf_essential_PROJECT_VER_PATCH)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ecmf_essential)
