cmake_minimum_required(VERSION 2.8)

SET(CMAKE_CXX_COMPILER "/usr/bin/clang++")

# user progs
SET(CMAKE_CXX_FLAGS "-m32 -std=c++11 -fno-rtti")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-m,elf_i386 -Wl,--hash-style=sysv")

add_executable(shell shell.cpp ../system/string.cpp)
target_link_libraries(shell system)
set_target_properties(shell PROPERTIES LINK_FLAGS -nostdlib)

