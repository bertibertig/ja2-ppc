cmake_minimum_required(VERSION 3.10)
project(JA2PPCMac C)

set(JA2_DATA_DIR "" CACHE PATH "JA2 installation directory containing Data/")
set(JA2_LANGUAGE "german" CACHE STRING "JA2 data language (english or german)")
option(JA2_DEBUG_BUILD "Write a detailed runtime debug log" ON)
option(JA2_PROFILE "Build the frame profiler and write JA2PROF.csv" OFF)
set_property(CACHE JA2_LANGUAGE PROPERTY STRINGS english german)

string(TOLOWER "${JA2_LANGUAGE}" JA2_LANGUAGE_LOWER)
if(NOT JA2_LANGUAGE_LOWER STREQUAL "english" AND NOT JA2_LANGUAGE_LOWER STREQUAL "german")
    message(FATAL_ERROR "JA2_LANGUAGE must be english or german")
endif()

# Keep the historical VC6 projects separate from the classic Mac target.
if(NOT CMAKE_SYSTEM_NAME MATCHES "RetroPPC")
    message(FATAL_ERROR
        "Use a Retro68 toolchain: -DCMAKE_TOOLCHAIN_FILE=/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake")
endif()

# Build the vendored laulandn/sdl2macos9 fork as libSDL2.a/libSDL2main.a.
# The JA2PPC target below links it through target_link_sdl2_classic(); see
# retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md for the required link/stub policy.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SDL2Classic.cmake)

# The game: the original JA2 sources at their mapped paths, changed only
# behind JA2_PPC, on a Mac reimplementation of the Standard Gaming Platform.
# See CLAUDE.md "Architecture decision" and AGENTS.md "Port Architecture".
set(JA2_SGP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Standard Gaming Platform")

# Port files of the platform layer (full warnings).
set(JA2_SGP_PORT_SOURCES
    "${JA2_SGP_DIR}/sgp_win32.c"
    "${JA2_SGP_DIR}/sgp_win32_msg.c"
    "${JA2_SGP_DIR}/sgp_wchar.c"
    "${JA2_SGP_DIR}/sgp_ddraw.c"
    "${JA2_SGP_DIR}/sgp_sdl.c"
    "${JA2_SGP_DIR}/sgp_mss.c"
    "${JA2_SGP_DIR}/sgp_smack.c"
)
set(JA2_SGP_ORIGINAL_SOURCES
    "${JA2_SGP_DIR}/DEBUG.C"
    "${JA2_SGP_DIR}/Container.c"
    "${JA2_SGP_DIR}/FileMan.c"
    "${JA2_SGP_DIR}/LibraryDataBase.c"
    "${JA2_SGP_DIR}/Random.c"
    "${JA2_SGP_DIR}/timer.c"
    "${JA2_SGP_DIR}/himage.c"
    "${JA2_SGP_DIR}/STCI.c"
    "${JA2_SGP_DIR}/PCX.C"
    "${JA2_SGP_DIR}/impTGA.c"
    "${JA2_SGP_DIR}/Compression.c"
    "${JA2_SGP_DIR}/shading.c"
    "${JA2_SGP_DIR}/vobject.c"
    "${JA2_SGP_DIR}/vobject_blitters.c"
    "${JA2_SGP_DIR}/vsurface.c"
    "${JA2_SGP_DIR}/video.c"
    "${JA2_SGP_DIR}/DirectDraw Calls.c"
    "${JA2_SGP_DIR}/DirectX Common.c"
    "${JA2_SGP_DIR}/Mutex Manager.c"
    "${JA2_SGP_DIR}/Font.c"
    "${JA2_SGP_DIR}/input.c"
    "${JA2_SGP_DIR}/mousesystem.c"
    "${JA2_SGP_DIR}/Button System.c"
    "${JA2_SGP_DIR}/Button Sound Control.c"
    "${JA2_SGP_DIR}/Cursor Control.c"
    "${JA2_SGP_DIR}/MemMan.c"
    "${JA2_SGP_DIR}/line.c"
    "${JA2_SGP_DIR}/English.c"
    "${JA2_SGP_DIR}/soundman.c"
    "${JA2_SGP_DIR}/sgp.c"
)
# Original game files (ja2/Build/ mapped to retro68/).
set(JA2_GAME_SOURCES
    gameloop.c
    SCREENS.C
    jascreens.c
    Init.c
    "Sys Globals.c"
    GameVersion.c
    GameSettings.c
    "JA2 Splash.c"
    Intro.c
    MainMenuScreen.c
    Credits.c
    GameInitOptionsScreen.c
    "Options Screen.c"
    MessageBoxScreen.c
    "Fade Screen.c"
    Utils/Cinematics.c
    Utils/Cursors.c
    "Utils/Encrypted File.c"
    "Utils/Font Control.c"
    Utils/_GermanText.c
    Utils/_EnglishText.c
    Utils/_Ja25GermanText.c
    Utils/_Ja25EnglishText.c
    Utils/MercTextBox.c
    "Utils/Multi Language Graphic Utils.c"
    "Utils/Music Control.c"
    Utils/Slider.c
    "Utils/Sound Control.c"
    "Utils/Text Input.c"
    "Utils/Text Utils.c"
    "Utils/Timer Control.c"
    Utils/Utilities.c
    Utils/WordWrap.c
    "TileEngine/Render Dirty.c"
    TileEngine/sysutil.c
    # Stand-ins for the original modules not ported yet.
    ja2_ppc_unported.c
)

# The x86 sources spell headers in several cases ("types.h", "DEBUG.H",
# "LAPTOP\files.h"); give each spelling an alias for the case-sensitive
# host file system.
set(JA2_CASE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/case-include")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_case_includes.py"
        "${JA2_CASE_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE JA2_CASE_INCLUDE_RESULT)
if(NOT JA2_CASE_INCLUDE_RESULT EQUAL 0)
    message(FATAL_ERROR "gen_case_includes.py failed")
endif()

# Quoted includes only (-iquote), so Mac headers such as <Timer.h> and
# <Types.h> never resolve to game headers of the same name; the Win32
# stand-in headers (windows.h, tchar.h, ...) come last. -fcommon: the
# original defines some globals in headers, which MSVC merged. The text
# files are Latin-1 and their L"" strings 16-bit. The original files are
# compiled without warnings and accept what MSVC accepted (implicit int,
# pointer/int conversions); the port's own files keep all warnings.
set(JA2_INCLUDE_OPTIONS "-iquote;${JA2_SGP_DIR};-iquote;${CMAKE_CURRENT_SOURCE_DIR}")
foreach(JA2_SUBDIR Utils TileEngine Tactical Strategic TacticalAI Laptop Editor)
    list(APPEND JA2_INCLUDE_OPTIONS "-iquote;${CMAKE_CURRENT_SOURCE_DIR}/${JA2_SUBDIR}")
endforeach()
list(APPEND JA2_INCLUDE_OPTIONS
    "-iquote;${JA2_CASE_INCLUDE_DIR}"
    "-iquote;${CMAKE_CURRENT_SOURCE_DIR}/win32-include"
    "-idirafter;${CMAKE_CURRENT_SOURCE_DIR}/win32-include")
set(JA2_ORIGINAL_OPTIONS
    "${JA2_INCLUDE_OPTIONS};-std=gnu99;-fcommon;-fshort-wchar;-finput-charset=ISO-8859-1;-w"
    "-Wno-error=implicit-int;-Wno-error=incompatible-pointer-types"
    "-Wno-error=int-conversion;-Wno-error=return-mismatch")
string(REPLACE ";;" ";" JA2_ORIGINAL_OPTIONS "${JA2_ORIGINAL_OPTIONS}")
set_source_files_properties(${JA2_SGP_PORT_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "JA2;JA2_PPC"
    COMPILE_OPTIONS "${JA2_INCLUDE_OPTIONS};-std=gnu99;-fcommon;-fshort-wchar")
set_source_files_properties(${JA2_SGP_ORIGINAL_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "JA2;JA2_PPC"
    COMPILE_OPTIONS "${JA2_ORIGINAL_OPTIONS}")
# The game files were built with the precompiled header "JA2 All.h".
set_source_files_properties(${JA2_GAME_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "JA2;JA2_PPC;PRECOMPILEDHEADERS"
    COMPILE_OPTIONS "${JA2_ORIGINAL_OPTIONS}")
set_source_files_properties(ja2_autotest.c PROPERTIES
    COMPILE_OPTIONS "-iquote;${CMAKE_CURRENT_SOURCE_DIR}")
# DEBUG.C, PCX.C and SCREENS.C keep their original upper-case names. CMake
# skips ".C" in a C-only project and gcc would treat it as C++, so set both.
set(JA2_UPPERCASE_C "${JA2_SGP_DIR}/DEBUG.C" "${JA2_SGP_DIR}/PCX.C" SCREENS.C)
set_source_files_properties(${JA2_UPPERCASE_C} PROPERTIES LANGUAGE C)
set_property(SOURCE ${JA2_UPPERCASE_C} APPEND PROPERTY COMPILE_OPTIONS "-x;c")

add_application(JA2PPC
    ${JA2_SGP_PORT_SOURCES}
    ${JA2_SGP_ORIGINAL_SOURCES}
    ${JA2_GAME_SOURCES}
    ja2_autotest.c
    ja2_autotest.h
    ja2_options.c
    ja2_options.h
    ja2_prof.c
    ja2_prof.h
    Smacker/smacker.c
    Smacker/smacker.h
    sdl2_gl_stub.c
    ja2_mac.r
    CREATOR JA2P
)

set_target_properties(JA2PPC PROPERTIES
    COMPILE_OPTIONS "-O2;-ffunction-sections"
    LINK_FLAGS "-Wl,-gc-sections"
)
target_link_sdl2_classic(JA2PPC)
if(JA2_LANGUAGE_LOWER STREQUAL "german")
    target_compile_definitions(JA2PPC PRIVATE JA2_LANGUAGE_GERMAN=1)
else()
    target_compile_definitions(JA2PPC PRIVATE JA2_LANGUAGE_ENGLISH=1)
endif()
if(JA2_DEBUG_BUILD)
    target_compile_definitions(JA2PPC PRIVATE JA2_DEBUG_BUILD=1)
endif()
if(JA2_PROFILE)
    target_compile_definitions(JA2PPC PRIVATE JA2_PROFILE=1)
endif()

# Stage 2 of the SDL2 rendering backend migration: a standalone smoke-test
# APPL, independent of JA2PPC, that only proves SDL2 initializes, opens a
# window, and presents a fixed RGB555 surface on real hardware. See
# retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md ("Implementation Stages", stage 2).
# JA2PPC itself is intentionally untouched by this stage.
add_application(JA2PPC_SDL2_SmokeTest
    sdl2_smoke_test.c
    sdl2_gl_stub.c
    sdl2_smoke_test.r
    CREATOR JA2T
)
target_link_sdl2_classic(JA2PPC_SDL2_SmokeTest)

if(JA2_DATA_DIR)
    get_filename_component(JA2_DATA_DIR "${JA2_DATA_DIR}" ABSOLUTE)
    if(NOT IS_DIRECTORY "${JA2_DATA_DIR}/Data")
        message(FATAL_ERROR "JA2_DATA_DIR must contain a Data directory: ${JA2_DATA_DIR}")
    endif()

    set(JA2_ARCHIVES
        Data.slf Ambient.slf Anims.slf BattleSnds.slf BigItems.slf
        BinaryData.slf Cursors.slf Faces.slf Fonts.slf Interface.slf
        Laptop.slf Maps.slf MercEdt.slf Music.slf Npc_Speech.slf
        NpcData.slf RadarMaps.slf Sounds.slf Speech.slf TileSets.slf
        LoadScreens.slf Intro.slf
    )
    if(JA2_LANGUAGE_LOWER STREQUAL "german")
        list(APPEND JA2_ARCHIVES German.slf)
    endif()

    set(JA2_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/JA2PPC Bundle")
    set(JA2_BUNDLE_DATA_DIR "${JA2_BUNDLE_DIR}/Data")
    set(JA2_BUNDLE_STAMP "${JA2_BUNDLE_DIR}/.bundle-complete")
    file(GLOB JA2_ALL_ARCHIVES
        "${JA2_DATA_DIR}/Data/*.slf"
        "${JA2_DATA_DIR}/Data/*.SLF"
    )

    foreach(JA2_ARCHIVE IN LISTS JA2_ARCHIVES)
        string(TOLOWER "${JA2_ARCHIVE}" JA2_ARCHIVE_LOWER)
        set(JA2_ARCHIVE_SOURCE "")
        foreach(JA2_CANDIDATE IN LISTS JA2_ALL_ARCHIVES)
            get_filename_component(JA2_CANDIDATE_NAME "${JA2_CANDIDATE}" NAME)
            string(TOLOWER "${JA2_CANDIDATE_NAME}" JA2_CANDIDATE_LOWER)
            if(JA2_CANDIDATE_LOWER STREQUAL JA2_ARCHIVE_LOWER)
                set(JA2_ARCHIVE_SOURCE "${JA2_CANDIDATE}")
            endif()
        endforeach()
        if(NOT JA2_ARCHIVE_SOURCE)
            message(FATAL_ERROR "Missing required JA2 archive: ${JA2_ARCHIVE} in ${JA2_DATA_DIR}/Data")
        endif()
    endforeach()

    if(EXISTS "${JA2_DATA_DIR}/Ja2.ini")
        list(APPEND JA2_BUNDLE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${JA2_DATA_DIR}/Ja2.ini" "${JA2_BUNDLE_DIR}/Ja2.ini"
        )
    else()
        message(FATAL_ERROR "Missing Ja2.ini in JA2_DATA_DIR: ${JA2_DATA_DIR}")
    endif()

    add_custom_command(
        OUTPUT "${JA2_BUNDLE_STAMP}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${JA2_BUNDLE_DATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/JA2PPC.APPL" "${JA2_BUNDLE_DIR}/JA2PPC.APPL"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/JA2PPC.bin" "${JA2_BUNDLE_DIR}/JA2PPC.bin"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/options.cfg" "${JA2_BUNDLE_DIR}/options.cfg"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${JA2_DATA_DIR}/Data" "${JA2_BUNDLE_DATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${JA2_DATA_DIR}/Ja2.ini" "${JA2_BUNDLE_DIR}/Ja2.ini"
        COMMAND "${CMAKE_COMMAND}" -E touch "${JA2_BUNDLE_STAMP}"
        DEPENDS JA2PPC_APPL
            "${CMAKE_CURRENT_SOURCE_DIR}/options.cfg"
        VERBATIM
    )
    add_custom_target(JA2PPC_BUNDLE ALL DEPENDS "${JA2_BUNDLE_STAMP}")
    message(STATUS "Bundling ${JA2_LANGUAGE_LOWER} JA2 data from ${JA2_DATA_DIR}")
else()
    message(STATUS "JA2_DATA_DIR not set; building application without game data")
endif()
