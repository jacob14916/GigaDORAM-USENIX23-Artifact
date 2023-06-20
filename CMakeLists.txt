cmake_minimum_required (VERSION 3.0)
project (doram)
set(NAME "doram")
set(CMAKE_BUILD_TYPE Release)

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)#this is what prints

find_package(emp-tool REQUIRED)
include_directories(${EMP-TOOL_INCLUDE_DIRS})
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=bounds")

# Installation: Not yet
# install(FILES cmake/emp-ot-config.cmake DESTINATION cmake/)
# install(DIRECTORY emp-ot DESTINATION include/)

ENABLE_TESTING()
ADD_SUBDIRECTORY(test)