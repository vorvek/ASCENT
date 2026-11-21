# The 3d library. The C files replace the assembly files of the same name.
# objects.c, upvecs.c and vecview.c are not a part of the library, the same
# as in the original makefile (see the comment in objects.c).
set(D3D_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/globvars.c
  ${CMAKE_CURRENT_SOURCE_DIR}/setup.c
  ${CMAKE_CURRENT_SOURCE_DIR}/matrix.c
  ${CMAKE_CURRENT_SOURCE_DIR}/points.c
  ${CMAKE_CURRENT_SOURCE_DIR}/draw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/clipper.c
  ${CMAKE_CURRENT_SOURCE_DIR}/horizon.c
  ${CMAKE_CURRENT_SOURCE_DIR}/instance.c
  ${CMAKE_CURRENT_SOURCE_DIR}/rod.c
  ${CMAKE_CURRENT_SOURCE_DIR}/interp.c)

# The 3d library uses the fix and vecmat libraries, and the headers of the
# 2d library (gr.h). The 2d library gives its include directory through the
# link when it has a target. If not, the directory is added by path.
# The texture mapper (draw_tmap, gr_upoly_tmap,
# init_interface_vars_to_assembler) is not in the link list: it comes after
# the 3d library in the build order, and it links the 3d library.
# gpu3d.c (ASCENT) gives the polygons to the GPU renderer of the platform
# layer. It is not in D3D_SOURCES: the test program has no platform layer.
add_library(d_3d STATIC ${D3D_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/gpu3d.c)
target_include_directories(d_3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_3d PUBLIC d_common d_fix d_vecmat d_platform)
if(TARGET d_2d)
  target_link_libraries(d_3d PUBLIC d_2d)
else()
  target_include_directories(d_3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../2d)
endif()

# The test program. It compiles the sources of the library again, and has
# its own small versions of the 2d and texture mapper functions that the
# library calls, so it does not link the 2d library or the texture mapper.
add_executable(test_3d test_3d.c ${D3D_SOURCES})
target_include_directories(test_3d PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../2d)
target_link_libraries(test_3d PRIVATE d_common d_fix d_vecmat)

add_test(NAME test_3d COMMAND test_3d)
