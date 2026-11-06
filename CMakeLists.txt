cmake_minimum_required(VERSION 3.20)
project(MegaDriveAudio LANGUAGES C CXX)

include(CTest)
add_library(mda_audio_bus STATIC src/hardware/audio_bus.cpp src/hardware/vgm_trace.cpp)
target_compile_features(mda_audio_bus PUBLIC cxx_std_17)
target_include_directories(mda_audio_bus PUBLIC src)
if(BUILD_TESTING)
    add_executable(mda_test_bus tests/test_bus.cpp)
    target_link_libraries(mda_test_bus PRIVATE mda_audio_bus)
    add_test(NAME sound_bus COMMAND mda_test_bus)
    add_executable(mda_test_vgm_trace tests/test_vgm_trace.cpp)
    target_link_libraries(mda_test_vgm_trace PRIVATE mda_audio_bus)
    add_test(NAME cycle_trace_to_vgm COMMAND mda_test_vgm_trace)
endif()

# Optional, real Z80 execution module. Requires GitHub access at CMake configure time.
# Kept OUT of the Windows GUI for now; driver-specific Sonic RAM/interrupts are unfinished.
option(MDA_ENABLE_Z80 "Build standalone Z80 driver integration and test" OFF)
if(MDA_ENABLE_Z80)
    include(FetchContent)
    FetchContent_Declare(superzazu_z80 GIT_REPOSITORY https://github.com/superzazu/z80.git GIT_TAG master GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(superzazu_z80)
    add_library(mda_z80 STATIC src/hardware/z80_driver.cpp ${superzazu_z80_SOURCE_DIR}/z80.c)
    target_include_directories(mda_z80 PUBLIC src/hardware ${superzazu_z80_SOURCE_DIR} src)
    target_link_libraries(mda_z80 PUBLIC mda_audio_bus)
    if(BUILD_TESTING)
        add_executable(mda_test_z80 tests/test_z80_driver.cpp)
        target_link_libraries(mda_test_z80 PRIVATE mda_z80)
        add_test(NAME z80_port_writes COMMAND mda_test_z80)
        add_executable(mda_z80_generate_vgm tests/z80_generate_vgm.cpp)
        target_link_libraries(mda_z80_generate_vgm PRIVATE mda_z80)
        add_test(NAME real_z80_to_vgm COMMAND mda_z80_generate_vgm ${CMAKE_CURRENT_BINARY_DIR}/Z80_CPU_Demo.vgm)
    endif()
endif()

# Optional 68000 integration. Requires Musashi code-generator and GitHub access.
# This bridge is experimental, and is not yet enabled in the Windows GUI.
option(MDA_ENABLE_M68K "Build Musashi CPU bridge (experimental)" OFF)
if(MDA_ENABLE_M68K)
    include(FetchContent)
    FetchContent_Declare(musashi_upstream GIT_REPOSITORY https://github.com/kstenerud/Musashi.git GIT_TAG master GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(musashi_upstream)
    add_executable(mda_m68kmake ${musashi_upstream_SOURCE_DIR}/m68kmake.c)
    set(MDA_MUSASHI_GEN ${CMAKE_CURRENT_BINARY_DIR}/musashi_generated)
    file(MAKE_DIRECTORY ${MDA_MUSASHI_GEN})
    add_custom_command(
        OUTPUT ${MDA_MUSASHI_GEN}/m68kops.c ${MDA_MUSASHI_GEN}/m68kops.h
        COMMAND $<TARGET_FILE:mda_m68kmake> ${MDA_MUSASHI_GEN} ${musashi_upstream_SOURCE_DIR}/m68k_in.c
        DEPENDS mda_m68kmake ${musashi_upstream_SOURCE_DIR}/m68k_in.c
        VERBATIM
    )
    add_library(mda_m68k STATIC src/hardware/m68k_driver.cpp
        ${musashi_upstream_SOURCE_DIR}/m68kcpu.c ${MDA_MUSASHI_GEN}/m68kops.c)
    target_include_directories(mda_m68k PUBLIC src ${musashi_upstream_SOURCE_DIR} ${MDA_MUSASHI_GEN})
    target_link_libraries(mda_m68k PUBLIC mda_audio_bus)
endif()

if(NOT WIN32)
    message(STATUS "Non-Windows host: building portable CPU/bus tests only")
    return()
endif()

add_executable(MegaDriveAudio WIN32
    src/player.cpp
    src/engine.cpp
    src/ym3438.c
)
target_compile_features(MegaDriveAudio PRIVATE cxx_std_17)
target_include_directories(MegaDriveAudio PRIVATE src)
target_compile_definitions(MegaDriveAudio PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MegaDriveAudio PRIVATE winmm comdlg32 comctl32 user32 gdi32 shell32)
if(MSVC)
    target_compile_options(MegaDriveAudio PRIVATE /W3 /utf-8)
else()
    target_compile_options(MegaDriveAudio PRIVATE -Wall -Wextra)
    target_link_options(MegaDriveAudio PRIVATE -municode)
endif()
set_target_properties(MegaDriveAudio PROPERTIES OUTPUT_NAME "MegaDriveAudio")
