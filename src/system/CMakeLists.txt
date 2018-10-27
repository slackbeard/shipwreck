cmake_minimum_required(VERSION 2.8)

include_directories("./include")

SET(CMAKE_CXX_COMPILER "/usr/bin/clang++")

SET(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)

enable_language(ASM_NASM)

add_library(boot_stub OBJECT boot_stub.nasm)

SET(CMAKE_CXX_FLAGS "-m32 -Os -mno-red-zone -mno-sse -DKERNEL_CODE -DDEBUG -fno-rtti -ffreestanding -nostdlib -std=c++11")

# bootloader puts the kernel at 0x7E00
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-Ttext,7E00 -nostdlib -Wl,--oformat=binary")

add_subdirectory(devices)
add_subdirectory(api)

add_library(memory OBJECT memory.cpp)
add_library(string OBJECT string.cpp)
add_library(interrupts OBJECT interrupts.cpp)
add_library(events OBJECT events.cpp)
add_library(locks OBJECT locks.cpp)
add_library(scheduler OBJECT scheduler.cpp)
add_library(threads OBJECT threads.cpp)
add_library(process OBJECT process.cpp)
add_library(gui OBJECT gui.cpp)
add_library(syscall OBJECT syscall.cpp)
add_library(sysapi OBJECT api/system.cpp)
add_library(filesystem OBJECT filesystem.cpp)
add_library(main OBJECT main.cpp)
add_executable(kernel $<TARGET_OBJECTS:boot_stub> $<TARGET_OBJECTS:main> $<TARGET_OBJECTS:string> $<TARGET_OBJECTS:memory> $<TARGET_OBJECTS:interrupts> $<TARGET_OBJECTS:locks> $<TARGET_OBJECTS:events> $<TARGET_OBJECTS:events> $<TARGET_OBJECTS:threads> $<TARGET_OBJECTS:process> $<TARGET_OBJECTS:gui> $<TARGET_OBJECTS:syscall> $<TARGET_OBJECTS:sysapi> $<TARGET_OBJECTS:scheduler> $<TARGET_OBJECTS:devices> $<TARGET_OBJECTS:filesystem>)

