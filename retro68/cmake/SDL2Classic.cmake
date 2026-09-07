# Wraps the vendored laulandn/sdl2macos9 fork's own GNU Makefile
# (SDL-main/Makefile.r68ppc) as a CMake external build, since that fork's
# CMakeLists.txt has no RetroPPC/macosclassic platform support.
#
# See retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md ("Build Integration") for the
# rationale. This module only produces libSDL2.a / libSDL2main.a; it does
# not link them into any JA2PPC target. That happens where the target is
# defined, once game code actually needs SDL2.
#
# Defines, after including this module:
#   JA2PPC_SDL2                - custom target that builds the static libs
#   SDL2_CLASSIC_INCLUDE_DIR   - SDL2 public header directory
#   SDL2_CLASSIC_LIBRARY       - path to libSDL2.a
#   SDL2_CLASSIC_MAIN_LIBRARY  - path to libSDL2main.a
#   SDL2::Classic              - IMPORTED STATIC target exposing the above

include(ExternalProject)

set(SDL2_CLASSIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl2macos9")
set(SDL2_CLASSIC_SRC_DIR "${SDL2_CLASSIC_ROOT}/SDL-main")
set(SDL2_CLASSIC_INCLUDE_DIR "${SDL2_CLASSIC_SRC_DIR}/include")
set(SDL2_CLASSIC_LIBRARY "${SDL2_CLASSIC_SRC_DIR}/libSDL2.a")
set(SDL2_CLASSIC_MAIN_LIBRARY "${SDL2_CLASSIC_SRC_DIR}/libSDL2main.a")

if(NOT EXISTS "${SDL2_CLASSIC_SRC_DIR}/Makefile.r68ppc")
    message(FATAL_ERROR
        "retro68/third_party/sdl2macos9 is missing or not checked out. "
        "Run: git submodule update --init --recursive")
endif()

find_program(JA2_MAKE_EXECUTABLE make REQUIRED)

get_filename_component(SDL2_CLASSIC_TOOLCHAIN_BINDIR
    "${CMAKE_C_COMPILER}" DIRECTORY)
set(SDL2_CLASSIC_CURSOR_DEVICES_GLUE
    "${SDL2_CLASSIC_TOOLCHAIN_BINDIR}/../universal/libppc/CursorDevicesGlue.o")

# CPPFLAGS is intentionally minimal: SDL_MACOSCLASSIC_DRAWSPROCKET,
# SDL_MACOSCLASSIC_AGL, and SDL_MACOSCLASSIC_INPUTSPROCKET are left out per
# AGENTS.md's "Rendering Backend (SDL2)" policy. Passing CPPFLAGS on the make
# command line overrides the fork's own Makefile.r68ppc "+=" assignment
# entirely (make command-line variables take precedence over in-makefile
# assignments), so this reliably disables those optional dependencies
# without patching the vendored Makefile.
set(SDL2_CLASSIC_CPPFLAGS
    "-I./include -D__MACOSCLASSIC__ -DMAC_DEBUG=1")

# BUILDING_FOR_PRE9 is required for the OS 8.6-9.2.2 target range (AGENTS.md
# "Toolchain And Target Platform"); it is added once the pre-9 code paths
# have been verified on an 8.6 install per the migration plan's stage 8.
# Until then, keep the default (9.x-shaped) code path so the base library
# integration can be validated first.

ExternalProject_Add(JA2PPC_SDL2
    SOURCE_DIR "${SDL2_CLASSIC_ROOT}"
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${JA2_MAKE_EXECUTABLE} -C SDL-main -f Makefile.r68ppc
        CC=${CMAKE_C_COMPILER}
        AR=powerpc-apple-macos-ar
        RANLIB=powerpc-apple-macos-ranlib
        CPPFLAGS=${SDL2_CLASSIC_CPPFLAGS}
        CURSOR_DEVICES_GLUE=${SDL2_CLASSIC_CURSOR_DEVICES_GLUE}
    BUILD_BYPRODUCTS "${SDL2_CLASSIC_LIBRARY}" "${SDL2_CLASSIC_MAIN_LIBRARY}"
    INSTALL_COMMAND ""
    LOG_BUILD ON
)

add_library(SDL2::Classic STATIC IMPORTED GLOBAL)
set_target_properties(SDL2::Classic PROPERTIES
    IMPORTED_LOCATION "${SDL2_CLASSIC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_CLASSIC_INCLUDE_DIR}"
)

add_library(SDL2::ClassicMain STATIC IMPORTED GLOBAL)
set_target_properties(SDL2::ClassicMain PROPERTIES
    IMPORTED_LOCATION "${SDL2_CLASSIC_MAIN_LIBRARY}"
)
