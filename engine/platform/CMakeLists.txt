file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/test_[^/]*\\.c$")

add_library(d_platform STATIC ${PLATFORM_SOURCES})
target_include_directories(d_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_platform PRIVATE SDL3::SDL3)

add_executable(test_window test_window.c)
target_link_libraries(test_window PRIVATE d_platform d_fix d_common)

# The mixer test does not open a device.
add_executable(test_audio test_audio.c)
target_link_libraries(test_audio PRIVATE d_platform d_common)
add_test(NAME platform_audio COMMAND test_audio)

# A program for a person: it plays a test pattern on the real device.
# It is not a test.
add_executable(test_audio_play test_audio_play.c)
target_link_libraries(test_audio_play PRIVATE d_platform d_common)

# ASCENT: the gamepad state, with events sent by the test (no device)
add_executable(test_pad test_pad.c)
target_link_libraries(test_pad PRIVATE d_platform d_common)
add_test(NAME platform_pad COMMAND test_pad)
