# The HOG file reader.
# These files are not built:
#   l.c       revision 1.7 of cfile.c, with the LZW compression that was removed later
#   t.c       a DOS program that extracts all the files from DESCENT.HOG
#   pscomp.c  a DOS test program for the old compressed files (it needs pslib)
add_library(d_cfile STATIC cfile.c)
target_include_directories(d_cfile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_cfile PUBLIC d_common)

# The test writes its files in this directory.
set(TEST_CFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_cfile_tmp)
file(MAKE_DIRECTORY ${TEST_CFILE_DIR})
add_executable(test_cfile test_cfile.c)
target_link_libraries(test_cfile PRIVATE d_cfile)
add_test(NAME test_cfile COMMAND test_cfile ${TEST_CFILE_DIR})
