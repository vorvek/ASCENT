# Integration demo: links the real libraries together, with no substitutes.
add_executable(cube cube.c)
target_link_libraries(cube PRIVATE
  d_3d d_texmap d_3d d_2d d_bios d_vecmat d_fix d_div d_mem d_misc d_cfile d_platform d_common)
