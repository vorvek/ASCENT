# Divide exception handler. div0.c replaces div0.asm and installs nothing.
# testd.c and testda.asm are the original DOS test. They are not built.
add_library(d_div STATIC div0.c)
target_include_directories(d_div PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_div PUBLIC d_common)
