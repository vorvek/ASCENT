# The debug memory allocator.
# mem1.c is an older copy of mem.c. It is not built.
add_library(d_mem STATIC mem.c)
target_include_directories(d_mem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_mem PUBLIC d_common d_misc)

add_executable(test_mem test_mem.c)
target_link_libraries(test_mem PRIVATE d_mem)
add_test(NAME test_mem COMMAND test_mem)
