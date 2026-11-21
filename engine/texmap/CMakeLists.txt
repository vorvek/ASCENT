# Texture mapper. The sources are the files that the original makefile put
# in texmap.lib (ntmap, tmapflat) and the C versions of the assembly files
# of that list. The other files of the directory are old versions and
# experiments. They are not built.
set(TEXMAP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/ntmap.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmapflat.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmap_per.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmap_lin.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmap_ll.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmap_flt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmapfade.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tmap_fl1.c)

# Libraries whose headers or symbols the texture mapper uses.
set(TEXMAP_DEPS fix vecmat misc 2d 3d)

add_library(d_texmap STATIC ${TEXMAP_SOURCES})
target_include_directories(d_texmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_texmap PUBLIC d_common)
foreach(dep ${TEXMAP_DEPS})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../${dep}/CMakeLists.txt)
    target_link_libraries(d_texmap PUBLIC d_${dep})
  else()
    # The library has no build file yet. Use its headers only.
    target_include_directories(d_texmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../${dep})
  endif()
endforeach()

# The test compiles the texture mapper sources again and supplies its own
# small versions of the symbols of the other libraries. It needs their
# headers and not their code.
add_executable(test_texmap test_texmap.c ${TEXMAP_SOURCES})
target_link_libraries(test_texmap PRIVATE d_common)
target_include_directories(test_texmap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
foreach(dep ${TEXMAP_DEPS})
  target_include_directories(test_texmap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../${dep})
endforeach()
target_compile_definitions(test_texmap PRIVATE TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(NOT MSVC)
  target_link_libraries(test_texmap PRIVATE m)
endif()
add_test(NAME test_texmap COMMAND test_texmap)
