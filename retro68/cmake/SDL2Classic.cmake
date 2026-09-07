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

# CPPFLAGS omits SDL_MACOSCLASSIC_DRAWSPROCKET, SDL_MACOSCLASSIC_
# INPUTSPROCKET, and SDL_MACOSCLASSIC_AGL (all three genuinely optional at
# the source level; the .c files that need them are guarded exactly by
# these defines with no other header-level dependency). Passing CPPFLAGS on
# the make command line overrides the fork's own Makefile.r68ppc "+="
# assignment entirely (make command-line variables take precedence over
# in-makefile assignments), so this reliably disables all three without
# patching the vendored Makefile.
#
# SDL_VIDEO_OPENGL itself, however, is NOT controlled by any of the above:
# SDL-main/include/SDL_config_macosclassic.h unconditionally sets it to 1,
# so macvideo.c/macevents.c always reference a fixed set of GL hookup
# functions regardless of SDL_MACOSCLASSIC_AGL. With AGL disabled, macgl.c
# (the only file that implements them) compiles to nothing, which leaves
# those references unresolved at link time (verified). The Retro68
# toolchain has no AGL import library to link against anyway (verified: no
# aglCreateContext symbol in any toolchain .a), and the fork's bundled
# software-GL alternative (mactinygl.c, SDL_MACOSCLASSIC_TINYGL) pulls in a
# full software rasterizer JA2 does not need. Any CMake target that links
# SDL2::Classic must therefore also compile retro68/sdl2_gl_stub.c, which
# provides no-op stand-ins for exactly those hookup functions; JA2 never
# calls the SDL_GL_* API per AGENTS.md's "Rendering Backend (SDL2)" policy.
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

# target_link_sdl2_classic(<target>)
#
# Links a Retro68 add_application() target against the vendored SDL2 fork,
# including the ordering/library fixes this integration needs beyond a
# plain target_link_libraries() call:
#
#  - CMake's toolchain-file ABI detection appends a trailing
#    "--start-group -lgcc -lc -lretrocrt -lInterfaceLib --end-group" after
#    every target's own libraries. -lretrocrt's startup object
#    (ppcstart.c.obj) needs `.main`, which only libSDL2main.a provides
#    (src/main/macosclassic/SDL_main.c defines `main` and calls the game's
#    SDL_main()). Because that startup object is inside the trailing
#    group and libSDL2main.a is not, a plain link leaves `.main`
#    unresolved: ld only pulls an archive member to satisfy a symbol that
#    is undefined *at the time it scans that archive*, and nothing before
#    libSDL2main.a in the link line references `main` (verified: the
#    ExternalProject_Add above's produced .a does contain a `.main`/`main`
#    pair; the failure is purely link-order, not a missing symbol).
#    Passing libSDL2main.a with --whole-archive forces every member
#    (including `main`) to be pulled in unconditionally, so it is already
#    a defined global symbol by the time -lretrocrt's startup object needs
#    it (verified fix).
#  - ThreadsLib/MathLib/m: see the CPPFLAGS comment above for why these are
#    always required, independent of any JA2 feature usage.
#
# The caller must still add retro68/sdl2_gl_stub.c to its own sources (see
# the CPPFLAGS comment above); it is deliberately not folded into
# libSDL2.a so that non-SDL2 targets are unaffected.
function(target_link_sdl2_classic TARGET_NAME)
    add_dependencies(${TARGET_NAME} JA2PPC_SDL2)
    target_link_options(${TARGET_NAME} PRIVATE
        "LINKER:--whole-archive,${SDL2_CLASSIC_MAIN_LIBRARY},--no-whole-archive")
    target_link_libraries(${TARGET_NAME} SDL2::Classic ThreadsLib MathLib m)
endfunction()
