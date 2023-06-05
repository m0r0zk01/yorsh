link_libraries(http1)
link_libraries(utils)

add_library(vector src/vector.c)
target_include_directories(vector PUBLIC inc)
