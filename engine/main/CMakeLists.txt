# The game code. The list is DESCENT_OBJS of the original makefile, without
# the virtual reality headset files (vfx, victor, iglasses), the modem and
# network files, the sound codec assembly, and dumpmine.
set(MAIN_SOURCES
  inferno.c game.c gameseq.c controls.c slew.c config.c args.c text.c
  mission.c playsave.c state.c titles.c credits.c scores.c
  menu.c newmenu.c kconfig.c joydefs.c gauges.c hud.c automap.c kmatrix.c
  endlevel.c terrain.c gamefont.c newdemo.c songs.c digi.c
  bm.c bmread.c piggy.c paging.c texmerge.c gamemine.c gamesave.c polyobj.c
  dtufile.c acm.c dtumusic.c dtuogg.c dtufoot.c dtumiss.c
  robot.c vclip.c effects.c weapon.c powerup.c mglobal.c hash.c
  object.c physics.c collide.c fvi.c gameseg.c laser.c fireball.c ai.c
  aipath.c wall.c switch.c fuelcen.c cntrlcen.c hostage.c morph.c
  lighting.c render.c dtugame.c dtudisp.c dtuinfra.c dtuobj.c dtuobj_test.c dtuobj_item.c dtuobj_setup.c dtuobj_combat.c dtuobj_missile.c dtuobj_move.c dtuobj_scenery.c dtusolid.c dtustep.c dtuobj_aware.c dtuobj_ai.c dtutrig.c dtuchar.c dtuhook.c dtucombat.c dtuatk.c dtuspell.c dtuthief.c
  dtufe.c dtufe_script.c dtufe_char.c dtufe_town.c dtustate.c dtuscript.c dtudlg.c dtuscroll.c dtudesc.c dtuthiefpts.c dtucheat.c dtuopts.c
  dtumovie.c mvedec.c dtusoak.c dtuhdui.c dtulog.c dtulang.c dtulangart.c dtuitemfix.c dtutblfix.c dtuxp.c dtuhold.c)

# ASCENT: the translations of the text of the code (dtulang.c,
# docs/formats/lang.md) are part of the program: each
# engine/lang/<code>_port.catalog becomes the C array Lang_port_<code>.
set(LANG_PORT_C ${CMAKE_CURRENT_BINARY_DIR}/lang_port_es.c)
add_custom_command(OUTPUT ${LANG_PORT_C}
  COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/engine/lang/es_port.catalog -DOUT=${LANG_PORT_C}
    -DNAME=Lang_port_es -P ${CMAKE_SOURCE_DIR}/cmake/embed_text.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/engine/lang/es_port.catalog ${CMAKE_SOURCE_DIR}/cmake/embed_text.cmake
  VERBATIM)
list(APPEND MAIN_SOURCES ${LANG_PORT_C})

add_library(d_main OBJECT EXCLUDE_FROM_ALL ${MAIN_SOURCES})
target_include_directories(d_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ASCENT: the version in the session log (dtulog.c): ascent_version.h,
# made at each build from "git describe" and ASCENT_VERSION (the version
# of the setup program, installer/build.sh gives it).
set(ASCENT_VERSION "" CACHE STRING "The version of the game for the session log (empty: git describe only)")
add_custom_target(ascent_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/ascent_version.h
    -DVERSION=${ASCENT_VERSION} -DBUILD_TYPE=${CMAKE_BUILD_TYPE} -P ${CMAKE_SOURCE_DIR}/cmake/version.cmake
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/ascent_version.h)
add_dependencies(d_main ascent_version)
target_include_directories(d_main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(d_main PUBLIC
  d_common d_3d d_texmap d_2d d_bios d_iff d_cfile d_mem d_misc d_vecmat d_fix d_div d_platform)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # The original code declares many functions without a return type.
  target_compile_options(d_main PRIVATE
    -Wno-error=implicit-int -Wno-implicit-int
    -Wno-return-type
    -Wno-unknown-pragmas -Wno-parentheses -Wno-misleading-indentation
    -Wno-unused-function -Wno-comment -Wno-char-subscripts)
endif()
# -Wreturn-mismatch is an error by default from GCC 14. GCC 13 (Ubuntu 24.04)
# does not have the option and stops at an unknown -Wno-error= name.
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wreturn-mismatch ASCENT_HAS_WRETURN_MISMATCH)
if(ASCENT_HAS_WRETURN_MISMATCH)
  target_compile_options(d_main PRIVATE -Wno-error=return-mismatch)
endif()

# The executable is not part of the default build until the game code links.
add_executable(ascent EXCLUDE_FROM_ALL $<TARGET_OBJECTS:d_main>)
target_link_libraries(ascent PRIVATE d_main)

# ASCENT: the symbols of the release build (docs/debug_logs.md). The build
# folder keeps ascent-symbols.exe (the executable with its debug
# information) and ascent.map; the installed ascent.exe has no debug
# information. The code of the two files is the same, so addr2line with
# ascent-symbols.exe gives the function and the line of an address of a
# crash report.
if(ASCENT_RELEASE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(ascent PRIVATE -Wl,-Map=${CMAKE_BINARY_DIR}/ascent.map)
  add_custom_command(TARGET ascent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ascent> ${CMAKE_BINARY_DIR}/ascent-symbols.exe
    COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:ascent>
    COMMENT "The symbols of ascent.exe: ascent-symbols.exe and ascent.map"
    VERBATIM)
endif()

add_executable(test_fvi_math test_fvi_math.c)
target_include_directories(test_fvi_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_fvi_math PRIVATE d_common d_fix)
add_test(NAME test_fvi_math COMMAND test_fvi_math)

# ASCENT: the attack timing rules of the player (dtuatk.c, combat.md A8)
add_executable(test_dtuatk test_dtuatk.c dtuatk.c)
target_include_directories(test_dtuatk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_dtuatk PRIVATE d_common d_fix)
add_test(NAME test_dtuatk COMMAND test_dtuatk)

add_executable(test_data_io test_data_io.c)
target_include_directories(test_data_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_data_io PRIVATE
  d_common d_3d d_2d d_cfile d_misc d_mem d_vecmat d_fix)
add_test(NAME test_data_io COMMAND test_data_io)

# ASCENT: the item corrections (dtuitemfix.c). test_dtuitemfix checks the
# tables and the lookups without the game data. tools/check_itemfix.py
# checks the Spanish names of the corrected items and, when ASCENT_DATA_DIR
# has YPINN.BIN, compares the tables with the item lines of the game.
# ASCENT: the table fixup of 1.3 (dtutblfix.c, 0x1A344). test_dtutblfix
# applies it to records of the mission tables and checks a sample of each
# group of writes.
add_executable(test_dtutblfix test_dtutblfix.c dtutblfix.c)
target_include_directories(test_dtutblfix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_dtutblfix COMMAND test_dtutblfix)

add_executable(test_dtuitemfix test_dtuitemfix.c dtuitemfix.c)
target_include_directories(test_dtuitemfix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_dtuitemfix COMMAND test_dtuitemfix)
set(ASCENT_DATA_DIR "E:/UNDAMONTAN/DTU" CACHE PATH "Directory of the original game files (for tests)")
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME check_itemfix
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/check_itemfix.py --game "${ASCENT_DATA_DIR}")
  # The table fixup of the port and of tools/tblfix.py on YPINN.BIN
  add_test(NAME check_tblfix
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/check_tblfix.py "${ASCENT_DATA_DIR}/YPINN.BIN"
      --exe $<TARGET_FILE:test_dtutblfix>)
  # A save and a load of the port with a level change (tools/check_saveload.py).
  # It needs the files of the CD too (the level BONES).
  set(ASCENT_CD_DATA_DIR "${CMAKE_SOURCE_DIR}/gamedata/cd/DTU" CACHE PATH "Directory of the files of the CD (for tests)")
  if(EXISTS "${ASCENT_DATA_DIR}" AND EXISTS "${ASCENT_CD_DATA_DIR}")
    add_test(NAME check_saveload
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/check_saveload.py --exe $<TARGET_FILE:ascent>
        --game "${ASCENT_DATA_DIR}" --datadir "${ASCENT_CD_DATA_DIR}")
  endif()
endif()

# Check of the data layer against the game data (see test_dtudata.c). It is
# not in the default build and it is not a registered test: it needs the
# game files. It links the whole game code, which has main() of inferno.c.
# The linker option makes the start code call __wrap_main() of the test.
add_executable(test_dtudata EXCLUDE_FROM_ALL test_dtudata.c $<TARGET_OBJECTS:d_main>)
target_link_libraries(test_dtudata PRIVATE d_main)
target_link_options(test_dtudata PRIVATE -Wl,--wrap=main)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # The game headers declare functions without a return type.
  target_compile_options(test_dtudata PRIVATE
    -Wno-error=implicit-int -Wno-implicit-int -Wno-unknown-pragmas)
endif()

# The MVE decoder. With no argument test_mvedec checks a small movie that
# it builds in memory; with a file name it decodes a movie of the game for
# the compare with ffmpeg (docs/formats/mve.md).
add_executable(test_mvedec test_mvedec.c mvedec.c)
target_link_libraries(test_mvedec PRIVATE d_common)
add_test(NAME test_mvedec COMMAND test_mvedec)
