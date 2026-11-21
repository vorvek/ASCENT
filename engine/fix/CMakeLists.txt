file(GLOB FIX_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
list(FILTER FIX_SOURCES EXCLUDE REGEX "/test_[^/]*\\.c$")

if(FIX_SOURCES)
  add_library(d_fix STATIC ${FIX_SOURCES})
  target_include_directories(d_fix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(d_fix PUBLIC d_common)
else()
  add_library(d_fix INTERFACE)
  target_include_directories(d_fix INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(d_fix INTERFACE d_common)
endif()

# test_fix.c compares fix.c with test_fix_data.h, which asm_model.py (an
# integer model of fix.asm) writes.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fix.c)
  add_executable(test_fix test_fix.c)
  target_link_libraries(test_fix PRIVATE d_fix)
  if(UNIX)
    target_link_libraries(test_fix PRIVATE m)
  endif()
  add_test(NAME test_fix COMMAND test_fix)
endif()
