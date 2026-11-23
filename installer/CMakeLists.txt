# ASCENT: the installer tools and the install rules (docs/install.md).
#
#   cmake --preset release
#   cmake --build --preset release
#   cmake --install build-rel --prefix <staging>
#
# The staging folder gets ascent.exe, the tools, the runtime DLLs and the
# files that the setup script (ascent.iss) needs.

# The list of the 1.3 patch files, as a C table for ascent-import.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/patch13.txt PATCH13_LINES REGEX "^[0-9a-f]")
set(PATCH13_C "")
set(PATCH13_N 0)
foreach(line ${PATCH13_LINES})
  string(REGEX MATCH "^([0-9a-f]+) ([0-9]+) ([^ ]+)" _ ${line})
  string(APPEND PATCH13_C "\t{ \"${CMAKE_MATCH_3}\", ${CMAKE_MATCH_2}, \"${CMAKE_MATCH_1}\" },\n")
  math(EXPR PATCH13_N "${PATCH13_N} + 1")
endforeach()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/patch13_list.h.tmp
  "// Generated from installer/patch13.txt. Do not edit.\n"
  "typedef struct { const char *name; long long size; const char *sha256; } patch13_file;\n"
  "#define N_PATCH13 ${PATCH13_N}\n"
  "static const patch13_file Patch13[N_PATCH13] = {\n${PATCH13_C}};\n")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/patch13_list.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/patch13_list.h COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/patch13.txt)

add_library(ascent_inst STATIC src/inst.c)
target_include_directories(ascent_inst PUBLIC src ${CMAKE_SOURCE_DIR}/engine/third_party/stb)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ascent_inst PRIVATE -Wall -Wno-unused-function)
endif()

# ASCENT: ascent-import also builds on the other systems (inst.c has the
# file functions of each). The rest of this file is for Windows only.
add_executable(ascent-import src/ascent_import.c)
target_include_directories(ascent-import PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ascent-import PRIVATE ascent_inst)
if(NOT WIN32)
  return()
endif()

# The tools link the MinGW runtime statically: they need no DLL.
set(TOOL_LINK_OPTIONS -static -static-libgcc)
target_sources(ascent-import PRIVATE res/tools.rc)
target_link_options(ascent-import PRIVATE ${TOOL_LINK_OPTIONS})

add_executable(ascent-content WIN32 src/ascent_content.c res/tools.rc)
target_link_libraries(ascent-content PRIVATE ascent_inst winhttp comctl32 shell32)
target_link_options(ascent-content PRIVATE ${TOOL_LINK_OPTIONS})

# The game: the icon and the manifest; in the release build no console.
target_sources(ascent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/res/ascent.rc)
if(ASCENT_RELEASE)
  target_link_options(ascent PRIVATE -mwindows -static-libgcc)
endif()

# The install rules. ascent is not in the default build: build it first
# (the build preset "release" does).
get_filename_component(ASCENT_CC_DIR ${CMAKE_C_COMPILER} DIRECTORY)
install(TARGETS ascent ascent-import ascent-content
  RUNTIME_DEPENDENCIES
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*" ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/.*"
    DIRECTORIES ${ASCENT_CC_DIR}
  RUNTIME DESTINATION .
)
install(FILES content.ini DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/engine/readme.txt DESTINATION . RENAME LICENSE-PARALLAX.txt)
# SDL3.dll of MSYS2 (zlib license): its license text goes with it.
if(EXISTS ${ASCENT_CC_DIR}/../share/licenses/SDL3/LICENSE.txt)
  install(FILES ${ASCENT_CC_DIR}/../share/licenses/SDL3/LICENSE.txt DESTINATION . RENAME LICENSE-SDL3.txt)
endif()
