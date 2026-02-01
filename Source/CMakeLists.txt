# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_compile_definitions(RADIOLIB_EXCLUDE_RF69)
add_compile_definitions(RADIOLIB_EXCLUDE_SX1231)
add_compile_definitions(RADIOLIB_EXCLUDE_AFSK)
add_compile_definitions(RADIOLIB_EXCLUDE_APRS)
add_compile_definitions(RADIOLIB_EXCLUDE_AX25)
add_compile_definitions(RADIOLIB_EXCLUDE_BELL)
add_compile_definitions(RADIOLIB_EXCLUDE_FSK4)    
add_compile_definitions(RADIOLIB_EXCLUDE_HELLSCHREIBER)
add_compile_definitions(RADIOLIB_EXCLUDE_MORSE)
add_compile_definitions(RADIOLIB_EXCLUDE_PAGER)
add_compile_definitions(RADIOLIB_EXCLUDE_RTTY)
add_compile_definitions(RADIOLIB_EXCLUDE_SSTV)

add_compile_options("-zmuldefs")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32PP)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-zmuldefs")
