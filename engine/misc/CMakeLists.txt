# Error handling (Error, Warning, Assert, Int3) and the argument parser.
add_library(d_misc STATIC error.c parsarg.c)
target_include_directories(d_misc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_misc PUBLIC d_common)

# The test writes its files in this directory.
set(TEST_MISC_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_misc_tmp)
file(MAKE_DIRECTORY ${TEST_MISC_DIR})
add_executable(test_misc test_misc.c)
target_link_libraries(test_misc PRIVATE d_misc)
add_test(NAME test_misc COMMAND test_misc ${TEST_MISC_DIR})
