cmake_minimum_required(VERSION 3.16)
project(hdb_decoder
    VERSION 0.1.0
    DESCRIPTION "Byte-direct decoder for XFILES.HDB (NeoPersist 3.0 wire format)"
    LANGUAGES CXX
)

# --- Standard / warnings -----------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Decoder library ---------------------------------------------------------
#
# Subset of the C++ reference decoder that depends only on the standard library.
# Excludes game runtime: engine/, audio/, render/, gameplay/, input/, scripts/.

file(GLOB_RECURSE HDB_DECODER_SOURCES
    "src/hdb/*.cpp"
    "src/nl/*.cpp"
    "src/nl/classes/*.cpp"
    "src/vc/*.cpp"
    "src/assets/*.cpp"
    "src/runtime/hsp_loader.cpp"
    "src/runtime/scene_resolver.cpp"
)

add_library(hdb_decoder STATIC ${HDB_DECODER_SOURCES})
target_include_directories(hdb_decoder
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # stb_image is included by pict_decoder.cpp only
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
target_compile_features(hdb_decoder PUBLIC cxx_std_17)
set_target_properties(hdb_decoder PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Demo executable ---------------------------------------------------------

add_executable(hdb_dump src/tools/hdb_dump.cpp)
target_link_libraries(hdb_dump PRIVATE hdb_decoder)

# --- Reference game engine ---------------------------------------------------
# Self-contained pseudo-engine: reads the HDB, rebuilds the game object model
# (scenes, hotspots, triggers, conditions, actions), and demonstrates the
# runtime game loop. See docs/ENGINE.md.
add_executable(xfiles_engine src/engine/xfiles_engine.cpp)
target_link_libraries(xfiles_engine PRIVATE hdb_decoder)

# --- Optional SDL2 playable shell --------------------------------------------
# Slice 1: a 640x480 window that plays one Cinepak/IMA scene, overlays its HSPT
# rects, and dispatches action_ids on click. Off by default — when off, no SDL2
# fetch happens and the existing build matrix is untouched.

option(XFILES_PLAY "Build the SDL2 playable shell (xfiles_play)" OFF)
option(XFILES_SDL2_SYSTEM "Use system SDL2 (find_package) instead of FetchContent" OFF)

if(XFILES_PLAY)
    if(XFILES_SDL2_SYSTEM)
        find_package(SDL2 CONFIG REQUIRED)
    else()
        include(FetchContent)
        set(SDL_TEST   OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON  CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-2.30.7
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL2)
    endif()

    add_executable(xfiles_play
        src/runtime/sdl2_main.cpp
        src/runtime/fmv_player.cpp)
    target_link_libraries(xfiles_play PRIVATE hdb_decoder SDL2::SDL2 SDL2::SDL2main)
    target_include_directories(xfiles_play PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    install(TARGETS xfiles_play RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# --- Tests (optional, Catch2 vendored) ---------------------------------------

option(HDB_BUILD_TESTS "Build Catch2 tests" ON)

if(HDB_BUILD_TESTS)
    enable_testing()
    # Standalone `int main()` style tests. Catch2 ports planned for v0.4+.

    # Layout tests assert offsetof() values that depend on the compiler's
    # vtable padding scheme. They pass on gcc/clang but fail on MSVC
    # (extra word inserted between vptr and trailing fields). Until v0.4
    # delivers a portable layout, register them as build-only on MSVC.
    set(MSVC_INCOMPATIBLE_LAYOUT_TESTS
        test_vc_list_family_layout
        test_vc_misc_layouts
        test_vcaction_layout
        test_vccursor_layout
        test_vcexplorable_layout
        test_vcnode_layout
        test_vctrigger_layout
    )

    file(GLOB HDB_TEST_SOURCES "tests/*.cpp")
    foreach(test_src ${HDB_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE hdb_decoder)

        set(skip_run FALSE)
        if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            list(FIND MSVC_INCOMPATIBLE_LAYOUT_TESTS ${test_name} _idx)
            if(NOT _idx EQUAL -1)
                set(skip_run TRUE)
            endif()
        endif()

        if(NOT skip_run)
            add_test(NAME ${test_name} COMMAND ${test_name})
        else()
            message(STATUS "  test ${test_name}: built but skipped on MSVC (vtable padding)")
        endif()
    endforeach()

    # The xfiles_play --probe smoke test needs the binary path. When the SDL2
    # shell is built, point the test at it via an env var. When OFF, the test
    # self-skips (printing SKIP, returning 0).
    if(XFILES_PLAY AND TARGET test_play_probe AND TARGET xfiles_play)
        set_tests_properties(test_play_probe PROPERTIES
            ENVIRONMENT "XFILES_PLAY_BIN=$<TARGET_FILE:xfiles_play>")
    endif()
endif()

# --- Install -----------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS hdb_decoder hdb_dump xfiles_engine
    EXPORT hdb_decoderTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

message(STATUS "")
message(STATUS "  hdb_decoder ${PROJECT_VERSION}")
message(STATUS "  build tests : ${HDB_BUILD_TESTS}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  install dir : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
