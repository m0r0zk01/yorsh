link_libraries(http1)
link_libraries(utils)
link_libraries(gigaprotocol)

add_library(server
    src/server.c
    src/connection.c
)
add_executable(server-bin main.c)
target_link_libraries(server-bin PRIVATE server)
target_include_directories(server PUBLIC inc)
