cmake_minimum_required(VERSION 2.8)

SET(CMAKE_CXX_COMPILER "/usr/bin/clang++")


add_library(devices OBJECT ps2.cpp vga.cpp apic.cpp terminal.cpp)
