# The IFF (ILBM and PBM) bitmap reader and writer.
add_library(d_iff STATIC iff.c)
target_include_directories(d_iff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# iff.h includes gr.h for the type grs_bitmap. The library uses only types
# from gr.h and calls no function of the 2d library, so it adds the include
# directory and does not link d_2d.
target_include_directories(d_iff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../2d)
target_link_libraries(d_iff PUBLIC d_common d_fix d_mem d_misc d_cfile)

add_executable(test_iff test_iff.c)
target_link_libraries(test_iff PRIVATE d_iff)

# Temporary help for gr.h (engine/2d), which this directory must not edit.
# Remove the block when gr.h compiles with GCC as it is.
#
# 1. gr.h has function declarations without a return type. GCC 14 and later
#    make that an error. The options make it a warning for the files here.
# 2. gr.h ends with the DOS end-of-file character (Ctrl-Z, code 26), which
#    GCC refuses. While gr.h has that character, the files here include a
#    copy without it. The copy is made in the build directory when CMake
#    runs. It is not used when gr.h has no such character.
set(IFF_GR_H ${CMAKE_CURRENT_SOURCE_DIR}/../2d/gr.h)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${IFF_GR_H})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(d_iff PRIVATE -Wno-error=implicit-int -Wno-implicit-int)
  target_compile_options(test_iff PRIVATE -Wno-error=implicit-int -Wno-implicit-int)
endif()
# The search uses the hex form of the file, because a text read on Windows
# stops at the character.
file(READ ${IFF_GR_H} IFF_GR_H_HEX HEX)
if(IFF_GR_H_HEX MATCHES "^(..)*1a")
  string(ASCII 26 IFF_CTRL_Z)
  file(READ ${IFF_GR_H} IFF_GR_H_TEXT)
  string(REPLACE "${IFF_CTRL_Z}" "" IFF_GR_H_TEXT "${IFF_GR_H_TEXT}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gr_h_copy/gr.h "${IFF_GR_H_TEXT}")
  target_include_directories(d_iff BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gr_h_copy)
  target_include_directories(test_iff BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gr_h_copy)
else()
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/gr_h_copy)
endif()

# The test writes its files in this directory.
set(TEST_IFF_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_iff_tmp)
file(MAKE_DIRECTORY ${TEST_IFF_DIR})
add_test(NAME test_iff COMMAND test_iff ${TEST_IFF_DIR})
