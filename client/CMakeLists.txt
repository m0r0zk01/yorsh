include_directories(inc)

link_libraries(http1)
link_libraries(utils)
link_libraries(gigaprotocol)

add_library(client
    src/client.c
    src/communicator.c
)

add_executable(client-bin main.c)
target_link_libraries(client-bin PRIVATE client)
set_target_properties(client-bin PROPERTIES OUTPUT_NAME client)
