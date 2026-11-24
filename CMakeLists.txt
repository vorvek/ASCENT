cmake_minimum_required(VERSION 3.16)
project(ascent C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Keep the asserts in the optimized build. The game code has many of them,
# and the port needs them while the loaders change.
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

# SDL3: use an installed package when one exists. If not, download and build it.
# On Linux the search skips the directories of PATH: in WSL, PATH has the
# Windows directories, and an SDL3 of MSYS2 there is the wrong platform.
if(WIN32)
  find_package(SDL3 CONFIG QUIET)
else()
  find_package(SDL3 CONFIG QUIET NO_SYSTEM_ENVIRONMENT_PATH)
endif()
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.20
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# ASCENT_RELEASE: the build for the installer (installer/README in
# docs/install.md): no developer messages unless -debug, no console window.
option(ASCENT_RELEASE "Build the game for the installer" OFF)
if(ASCENT_RELEASE)
  add_compile_definitions(ASCENT_RELEASE)
  # The asserts stay, as in RelWithDebInfo. The original code also has
  # parts that exist only without NDEBUG.
  string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()
if(WIN32)
  enable_language(RC)		# the icon and the manifest of the executables
endif()

enable_testing()
add_subdirectory(engine)
# The import tool (all systems); on Windows also the content tool and the
# install rules.
add_subdirectory(installer)
