cmake_minimum_required(VERSION 3.20)

project(Musica LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build optimisé par défaut : un build Debug rend la FFT et le tap audio
# suffisamment coûteux pour provoquer des micro-coupures (grincements).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Type de build" FORCE)
endif()

include(FetchContent)
find_package(Threads REQUIRED)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.3
)

FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG v2.1.1
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ftxui taglib)

add_executable(musica
    src/main.cpp
    src/AudioEngine.cpp
    src/Track.cpp
    src/FileBrowser.cpp
    src/Queue.cpp
    src/PlaylistStore.cpp
    src/Player.cpp
    src/Spectrum.cpp
    external/miniaudio/miniaudio.c
)

target_include_directories(musica
    PRIVATE
        include
        external/miniaudio
        ${taglib_SOURCE_DIR}
        ${taglib_SOURCE_DIR}/taglib
        ${taglib_SOURCE_DIR}/taglib/toolkit
        ${taglib_BINARY_DIR}
        ${taglib_BINARY_DIR}/taglib
)

target_link_libraries(musica
    PRIVATE
        ftxui::ftxui
        tag
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
)
