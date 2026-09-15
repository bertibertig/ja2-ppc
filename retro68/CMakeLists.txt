cmake_minimum_required(VERSION 3.9)
project(JA2PPCMac C)

set(JA2_DATA_DIR "" CACHE PATH "JA2 installation directory containing Data/")
set(JA2_LANGUAGE "german" CACHE STRING "JA2 data language (english or german)")
option(JA2_DEBUG_BUILD "Write a detailed runtime debug log" ON)
option(JA2_CONVERT_INTRO_VIDEOS "Convert Smacker intros to QuickTime MOV during the host build" ON)
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

add_application(JA2PPC
    ja2_mac.c
    ja2_mac_internal.h
    ja2_ppc_text.c
    ja2_ppc_text.h
    sdl2_gl_stub.c
    ja2_game_init_options.c
    ja2_game_init_options.h
    ja2_new_game_init.c
    ja2_new_game_init.h
    ja2/Build/Utils/Cinematics.c
    ja2/Build/Utils/Cinematics.h
    ja2/Build/Intro.c
    ja2/Build/Intro.h
    ja2_music.c
    ja2_music.h
    ja2_slf.c
    ja2_slf.h
    ja2_mac.r
    CREATOR JA2P
)

set_target_properties(JA2PPC PROPERTIES
    COMPILE_OPTIONS "-O2;-ffunction-sections"
    LINK_FLAGS "-Wl,-gc-sections"
)
target_link_libraries(JA2PPC QuickTimeLib)
target_link_sdl2_classic(JA2PPC)
if(JA2_LANGUAGE_LOWER STREQUAL "german")
    target_compile_definitions(JA2PPC PRIVATE JA2_LANGUAGE_GERMAN=1)
else()
    target_compile_definitions(JA2PPC PRIVATE JA2_LANGUAGE_ENGLISH=1)
endif()
if(JA2_DEBUG_BUILD)
    target_compile_definitions(JA2PPC PRIVATE JA2_DEBUG_BUILD=1)
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
    set(JA2_INTRO_VIDEO_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/intro-video-cache")
    set(JA2_INTRO_VIDEO_OUTPUT_DIR "${JA2_INTRO_VIDEO_CACHE_DIR}/mpg")
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

    if(JA2_CONVERT_INTRO_VIDEOS)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        find_program(JA2_FFMPEG_EXECUTABLE ffmpeg REQUIRED)
        set(JA2_INTRO_VIDEO_STAMP "${CMAKE_CURRENT_BINARY_DIR}/intro-videos.stamp")
        set(JA2_INTRO_VIDEO_OUTPUTS
            "${JA2_INTRO_VIDEO_OUTPUT_DIR}/REBEL_CR.mpg"
            "${JA2_INTRO_VIDEO_OUTPUT_DIR}/OMERTA.mpg"
            "${JA2_INTRO_VIDEO_OUTPUT_DIR}/PRAGUE_CR.mpg"
            "${JA2_INTRO_VIDEO_OUTPUT_DIR}/PRAGUE.mpg"
        )
        add_custom_command(
            OUTPUT ${JA2_INTRO_VIDEO_STAMP}
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/convert_intro_videos.py"
                --archive "${JA2_DATA_DIR}/Data/Intro.slf"
                --cache-dir "${JA2_INTRO_VIDEO_CACHE_DIR}"
                --output-dir "${JA2_INTRO_VIDEO_OUTPUT_DIR}"
                --ffmpeg "${JA2_FFMPEG_EXECUTABLE}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${JA2_INTRO_VIDEO_STAMP}"
            DEPENDS
                "${JA2_DATA_DIR}/Data/Intro.slf"
                "${CMAKE_CURRENT_SOURCE_DIR}/../tools/convert_intro_videos.py"
            VERBATIM
        )
        add_custom_target(JA2PPC_INTRO_VIDEOS DEPENDS "${JA2_INTRO_VIDEO_STAMP}")
    endif()

    set(JA2_BUNDLE_VIDEO_DEPENDENCY "")
    set(JA2_BUNDLE_VIDEO_COMMAND "")
    if(JA2_CONVERT_INTRO_VIDEOS)
        set(JA2_BUNDLE_VIDEO_DEPENDENCY JA2PPC_INTRO_VIDEOS)
        set(JA2_BUNDLE_VIDEO_COMMAND
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${JA2_INTRO_VIDEO_OUTPUT_DIR}" "${JA2_BUNDLE_DATA_DIR}/Intro")
    endif()

    add_custom_command(
        OUTPUT "${JA2_BUNDLE_STAMP}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${JA2_BUNDLE_DATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/JA2PPC.APPL" "${JA2_BUNDLE_DIR}/JA2PPC.APPL"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/JA2PPC.bin" "${JA2_BUNDLE_DIR}/JA2PPC.bin"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${JA2_DATA_DIR}/Data" "${JA2_BUNDLE_DATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${JA2_BUNDLE_DATA_DIR}/Intro"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${JA2_BUNDLE_DATA_DIR}/Intro"
        ${JA2_BUNDLE_VIDEO_COMMAND}
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${JA2_DATA_DIR}/Ja2.ini" "${JA2_BUNDLE_DIR}/Ja2.ini"
        COMMAND "${CMAKE_COMMAND}" -E touch "${JA2_BUNDLE_STAMP}"
        DEPENDS JA2PPC_APPL ${JA2_BUNDLE_VIDEO_DEPENDENCY}
        VERBATIM
    )
    add_custom_target(JA2PPC_BUNDLE ALL DEPENDS "${JA2_BUNDLE_STAMP}")
    message(STATUS "Bundling ${JA2_LANGUAGE_LOWER} JA2 data from ${JA2_DATA_DIR}")
else()
    message(STATUS "JA2_DATA_DIR not set; building application without game data")
endif()
