link_libraries(vector)

add_library(http1 src/http1.c)
target_include_directories(http1 PUBLIC inc)

add_executable(http1-test src/test.c)
target_link_libraries(http1-test PUBLIC http1)
