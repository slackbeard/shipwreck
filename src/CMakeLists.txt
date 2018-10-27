cmake_minimum_required(VERSION 2.8)
project(SHIPWRECK)

# relative paths for linker
cmake_policy(SET CMP0015 OLD)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories("./include")

add_subdirectory(boot)
add_subdirectory(user)
add_subdirectory(system)

# this builds the virtual hard disk vmdk
add_custom_target(disk ALL
	mkdir -p disk
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/system/kernel disk
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/user/shell disk
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/system/api/libsystem.so disk
	COMMAND cd disk && tar -cf ${CMAKE_CURRENT_BINARY_DIR}/filesystem.tar *
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && tar -rf ${CMAKE_CURRENT_BINARY_DIR}/filesystem.tar desktop.tga
	COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/boot/bootsector filesystem.tar > disk-flat.vmdk
	COMMAND truncate -s 16777216 disk-flat.vmdk
	DEPENDS bootsector kernel shell
)
