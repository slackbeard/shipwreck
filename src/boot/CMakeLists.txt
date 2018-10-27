cmake_minimum_required(VERSION 2.8)

add_custom_target(bootsector ALL nasm ${CMAKE_CURRENT_SOURCE_DIR}/bootsector.nasm -o ${CMAKE_CURRENT_BINARY_DIR}/bootsector)

