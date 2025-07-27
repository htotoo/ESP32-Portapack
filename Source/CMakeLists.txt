# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)


add_compile_options("-zmuldefs")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32PP)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-zmuldefs")
