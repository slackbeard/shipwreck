cmake_minimum_required(VERSION 2.8)

include_directories("../include")

SET(CMAKE_CXX_COMPILER "/usr/bin/clang++")

SET(CMAKE_CXX_FLAGS "-m32 -Os -mno-sse -DDEBUG -fno-rtti -ffreestanding -nostdlib -std=c++11")
SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic -nostdlib -Wl,--hash-style=sysv")

add_library(system SHARED system.cpp)
