cmake_minimum_required(VERSION 3.1)

project(Yorsh)

set(CMAKE_EXPORT_COMPILE_COMMANDS)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(libs)
add_subdirectory(client)
add_subdirectory(server)
