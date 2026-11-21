# The game code. The list is DESCENT_OBJS of the original makefile, without
# the virtual reality headset files (vfx, victor, iglasses), the modem and
# network files, the sound codec assembly, and dumpmine.
set(MAIN_SOURCES
  inferno.c game.c gameseq.c controls.c slew.c config.c args.c text.c
  mission.c playsave.c state.c titles.c credits.c scores.c
  menu.c newmenu.c kconfig.c joydefs.c gauges.c hud.c automap.c kmatrix.c
  endlevel.c terrain.c gamefont.c newdemo.c songs.c digi.c
  bm.c bmread.c piggy.c paging.c texmerge.c gamemine.c gamesave.c polyobj.c
  dtufile.c acm.c
  robot.c vclip.c effects.c weapon.c powerup.c mglobal.c hash.c
  object.c physics.c collide.c fvi.c gameseg.c laser.c fireball.c ai.c
  aipath.c wall.c switch.c fuelcen.c cntrlcen.c hostage.c morph.c
  lighting.c render.c dtugame.c dtudisp.c)

add_library(d_main OBJECT EXCLUDE_FROM_ALL ${MAIN_SOURCES})
target_include_directories(d_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_main PUBLIC
  d_common d_3d d_texmap d_2d d_bios d_iff d_cfile d_mem d_misc d_vecmat d_fix d_div d_platform)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # The original code declares many functions without a return type.
  target_compile_options(d_main PRIVATE
    -Wno-error=implicit-int -Wno-implicit-int
    -Wno-error=return-mismatch -Wno-return-type
    -Wno-unknown-pragmas -Wno-parentheses -Wno-misleading-indentation
    -Wno-unused-function -Wno-comment -Wno-char-subscripts)
endif()

# The executable is not part of the default build until the game code links.
add_executable(ascent EXCLUDE_FROM_ALL $<TARGET_OBJECTS:d_main>)
target_link_libraries(ascent PRIVATE d_main)

add_executable(test_fvi_math test_fvi_math.c)
target_include_directories(test_fvi_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_fvi_math PRIVATE d_common d_fix)
add_test(NAME test_fvi_math COMMAND test_fvi_math)

add_executable(test_data_io test_data_io.c)
target_include_directories(test_data_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_data_io PRIVATE
  d_common d_3d d_2d d_cfile d_misc d_mem d_vecmat d_fix)
add_test(NAME test_data_io COMMAND test_data_io)

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
