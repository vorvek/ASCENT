# d_bios: keyboard, mouse, joystick, timer, mono debug text, and the
# DPMI and IPX stubs, on top of the platform layer.
#
# The source list is explicit. The directory also holds DOS test programs
# (testk.c, testm.c, t1.c, demo.c, cdrom.c and others), old versions
# (newkey.c, mousec.c) and the assembly files. They are not built.
add_library(d_bios STATIC
  key.c
  mouse.c
  joy.c
  joyc.c
  timer.c
  mono.c
  dpmi.c
  ipx.c)
target_include_directories(d_bios PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_bios PUBLIC d_common d_fix d_platform)

add_executable(test_timer test_timer.c)
target_link_libraries(test_timer PRIVATE d_bios)
add_test(NAME bios_timer COMMAND test_timer)

add_executable(test_key test_key.c)
target_link_libraries(test_key PRIVATE d_bios)
add_test(NAME bios_key COMMAND test_key)

# An interactive program. It opens a window and prints the input.
# It is not a test. The source is in the platform directory.
add_executable(test_input ${CMAKE_CURRENT_SOURCE_DIR}/../platform/test_input.c)
target_link_libraries(test_input PRIVATE d_bios)
