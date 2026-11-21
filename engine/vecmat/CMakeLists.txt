# Vector and matrix library. vecmat.c replaces vecmat.asm.
add_library(d_vecmat STATIC vecmat.c)
target_include_directories(d_vecmat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_vecmat PUBLIC d_common d_fix)

add_executable(test_vecmat test_vecmat.c)
target_link_libraries(test_vecmat PRIVATE d_vecmat)
if(UNIX)
  target_link_libraries(test_vecmat PRIVATE m)
endif()
add_test(NAME test_vecmat COMMAND test_vecmat)
