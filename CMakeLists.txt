cmake_minimum_required(VERSION 3.1)

project(Yorsh)

set(CMAKE_EXPORT_COMPILE_COMMANDS)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(libs)
add_subdirectory(client)
add_subdirectory(server)
