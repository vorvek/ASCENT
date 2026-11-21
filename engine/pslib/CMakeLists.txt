# pslib is not ported. It is the old library file system with LZW
# compression (cflib.lib) and the DOS tool that builds such a library
# (pslib.c). The makefile of the game does not link cflib.lib, and the game
# calls none of its functions. cfile replaced it.
#
# Some game files still include "cflib.h", so the target supplies the
# include directory and no code.
add_library(d_pslib INTERFACE)
target_include_directories(d_pslib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_pslib INTERFACE d_common)
