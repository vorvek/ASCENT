# The 2D graphics library (gr.lib in the original makefile).
#
# The source list is explicit. The directory also has DOS test programs
# and tools (testg.c, vesatest.c, mspeed.c, median.c, inv_cmap.c, ...)
# and files that the original makefile did not build (effects.c,
# roller.c). They are not part of the library.
#
# linear.c, lbitblt.c, scalea.c, modex.c and vesa.c replace the assembly
# files with the same names. The .asm files stay as the reference.
set(GR2D_SOURCES
  bitblt.c bitmap.c box.c canvas.c circle.c disc.c font.c gpixel.c gr.c
  ibitblt.c lbitblt.c line.c linear.c modex.c palette.c pcx.c pixel.c
  poly.c rect.c rle.c scale.c scalea.c scanline.c vesa.c)

add_library(d_2d STATIC ${GR2D_SOURCES})
target_include_directories(d_2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_2d PUBLIC d_common)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # The original files keep "#pragma off (unreferenced)" of Watcom C.
  target_compile_options(d_2d PRIVATE -Wno-unknown-pragmas -Wno-comment)
endif()

# The libraries that the 2d code uses. A library that has no CMake target
# yet gives only its header directory.
# bios gives only mono.h (debug text output: the macro mprintf is empty
# with NDEBUG, and calls _mprintf of mono.c without it).
foreach(dep fix mem misc cfile platform bios)
  if(TARGET d_${dep})
    target_link_libraries(d_2d PUBLIC d_${dep})
  else()
    target_include_directories(d_2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../${dep})
  endif()
endforeach()

# Test program. It draws into memory canvases and does not open a window.
#
# With ASCENT_2D_TEST_STUBS the test compiles the 2d sources itself and
# uses small stubs in test_2d.c for the memory, error and file functions,
# so it does not depend on the state of the other libraries. The default
# is ON while one of those libraries has no CMake target.
set(GR2D_STUBS_DEFAULT OFF)
foreach(dep mem misc cfile)
  if(NOT TARGET d_${dep})
    set(GR2D_STUBS_DEFAULT ON)
  endif()
endforeach()
option(ASCENT_2D_TEST_STUBS "test_2d uses local stubs for mem, misc and cfile" ${GR2D_STUBS_DEFAULT})

if(ASCENT_2D_TEST_STUBS)
  add_executable(test_2d test_2d.c ${GR2D_SOURCES})
  target_compile_definitions(test_2d PRIVATE ASCENT_2D_TEST_STUBS)
  target_include_directories(test_2d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fix
    ${CMAKE_CURRENT_SOURCE_DIR}/../mem
    ${CMAKE_CURRENT_SOURCE_DIR}/../misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../cfile
    ${CMAKE_CURRENT_SOURCE_DIR}/../bios
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform)
  target_link_libraries(test_2d PRIVATE d_common)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_2d PRIVATE -Wno-unknown-pragmas -Wno-comment)
  endif()
else()
  add_executable(test_2d test_2d.c)
  target_link_libraries(test_2d PRIVATE d_2d)
endif()

# The data tests need the font files (*.FNT) and PALETTE.256 of the game.
# The test takes the directory from its first argument, or from the
# environment variable ASCENT_DATA_DIR, and skips the data tests when the
# files are absent. The test writes its picture (test_2d.ppm) into the
# build directory.
set(ASCENT_DATA_DIR "E:/UNDAMONTAN/DTU" CACHE PATH "Directory of the original game files (for tests)")
add_test(NAME test_2d
  COMMAND test_2d "${ASCENT_DATA_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test_2d.ppm")
