# Settings that all the engine libraries use.
add_library(d_common INTERFACE)
target_include_directories(d_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # The original compiler (Watcom C) has an unsigned plain char and
  # wraps signed integers on overflow. The code depends on the two properties.
  target_compile_options(d_common INTERFACE
    -funsigned-char -fwrapv -fno-strict-aliasing
    -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-pointer-sign
    -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
  # ASCENT: stricmp, strnicmp, strlwr, strupr and itoa of the Watcom and
  # Windows C libraries for the other systems (includes/portable.h).
  if(NOT WIN32)
    target_compile_options(d_common INTERFACE
      -include ${CMAKE_CURRENT_SOURCE_DIR}/includes/portable.h)
  endif()
endif()

# ASCENT: the release build keeps the debug information in the objects of
# the game, so that ascent-symbols.exe (engine/main/CMakeLists.txt) can
# turn the addresses of a crash report into functions and lines.
if(ASCENT_RELEASE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-g)
endif()

# A library is part of the build when its directory has a CMakeLists.txt file.
# The order is the dependency order.
set(ASCENT_LIBS
  fix vecmat div mem misc cfile iff pslib
  platform bios 2d 3d texmap ui main demo)
foreach(lib ${ASCENT_LIBS})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${lib}/CMakeLists.txt)
    add_subdirectory(${lib})
  endif()
endforeach()
