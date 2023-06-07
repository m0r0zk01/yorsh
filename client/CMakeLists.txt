link_libraries(http1)
link_libraries(utils)
link_libraries(gigaprotocol)

add_library(client
    src/client.c
    src/communicator.c
)
add_executable(client-bin main.c)
target_link_libraries(client-bin PRIVATE client)
target_include_directories(client PUBLIC inc)
