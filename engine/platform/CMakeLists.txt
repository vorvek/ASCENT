file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/test_[^/]*\\.c$")

add_library(d_platform STATIC ${PLATFORM_SOURCES})
target_include_directories(d_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d_platform PRIVATE SDL3::SDL3)

add_executable(test_window test_window.c)
target_link_libraries(test_window PRIVATE d_platform d_fix d_common)
