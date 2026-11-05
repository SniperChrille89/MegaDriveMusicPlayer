cmake_minimum_required(VERSION 3.20)
project(MegaDriveAudio LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Windows executable target requires a Windows compiler")
endif()

add_executable(MegaDriveAudio WIN32
    src/player.cpp
    src/engine.cpp
    src/ym3438.c
)
target_compile_features(MegaDriveAudio PRIVATE cxx_std_17)
target_include_directories(MegaDriveAudio PRIVATE src)
target_compile_definitions(MegaDriveAudio PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MegaDriveAudio PRIVATE winmm comdlg32 user32 gdi32 shell32)
if(MSVC)
    target_compile_options(MegaDriveAudio PRIVATE /W3 /utf-8)
else()
    target_compile_options(MegaDriveAudio PRIVATE -Wall -Wextra)
    target_link_options(MegaDriveAudio PRIVATE -municode)
endif()
set_target_properties(MegaDriveAudio PROPERTIES OUTPUT_NAME "MegaDriveAudio")
