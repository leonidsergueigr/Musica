cmake_minimum_required(VERSION 3.20)

project(Musica VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MUSICA_SYSTEM_DEPS "Use system-installed FTXUI and TagLib" OFF)
option(MUSICA_BUILD_TESTS "Build the unit tests" OFF)

# Release is the default: a Debug build makes the FFT and the audio tap
# expensive enough to cause dropouts.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

if(MUSICA_SYSTEM_DEPS)
    find_package(ftxui 7 CONFIG REQUIRED)
    find_package(TagLib 2.0 CONFIG QUIET)
    if(NOT TagLib_FOUND)
        find_package(TagLib 2.0 QUIET)
    endif()
    if(NOT TagLib_FOUND)
        message(FATAL_ERROR
            "MUSICA_SYSTEM_DEPS=ON requires TagLib 2.0 or newer, because the "
            "player uses FileRef::complexProperties(). Install libtag/lib taglib "
            "2.x development files, or configure with -DMUSICA_SYSTEM_DEPS=OFF "
            "to fetch TagLib automatically.")
    endif()

    set(MUSICA_FTXUI_TARGET ftxui::ftxui)
    if(TARGET TagLib::TagLib)
        set(MUSICA_TAGLIB_TARGET TagLib::TagLib)
    elseif(TARGET TagLib::tag)
        set(MUSICA_TAGLIB_TARGET TagLib::tag)
    else()
        set(MUSICA_TAGLIB_TARGET ${TagLib_LIBRARIES})
        set(MUSICA_TAGLIB_INCLUDE_DIRS ${TagLib_INCLUDE_DIRS})
    endif()
else()
    include(FetchContent)

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
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(ftxui taglib)

    # TagLib has no option to disable its install rules. Keep its headers,
    # libraries and taglib-config out of the Musica packages.
    set_property(DIRECTORY ${taglib_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL ON)

    set(MUSICA_FTXUI_TARGET ftxui::ftxui)
    set(MUSICA_TAGLIB_TARGET tag)
    set(MUSICA_TAGLIB_INCLUDE_DIRS
        ${taglib_SOURCE_DIR}
        ${taglib_SOURCE_DIR}/taglib
        ${taglib_SOURCE_DIR}/taglib/toolkit
        ${taglib_BINARY_DIR}
        ${taglib_BINARY_DIR}/taglib
    )
endif()

# The project version lives in this file only.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
    cmake/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp
    @ONLY
)

add_executable(musica
    src/main.cpp
    src/I18n.cpp
    src/CoverArt.cpp
    src/StateStore.cpp
    src/AudioEngine.cpp
    src/Track.cpp
    src/FileBrowser.cpp
    src/Queue.cpp
    src/PlaylistStore.cpp
    src/Player.cpp
    src/Spectrum.cpp
    external/miniaudio/miniaudio.c
)

target_compile_options(musica PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>
)

target_include_directories(musica
    PRIVATE
        include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        external/miniaudio
        external/stb
        ${MUSICA_TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(musica
    PRIVATE
        ${MUSICA_FTXUI_TARGET}
        ${MUSICA_TAGLIB_TARGET}
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
)

# Keep the C++ and GCC runtimes out of the runtime dependencies of the
# packages. Some toolchains ship without the static runtime libraries, so
# probe for them instead of failing the build.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LINK_OPTIONS -static-libstdc++ -static-libgcc)
    check_cxx_source_compiles("int main() { return 0; }" MUSICA_STATIC_RUNTIME)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(MUSICA_STATIC_RUNTIME)
        target_link_options(musica PRIVATE
            $<$<CONFIG:Release>:-static-libstdc++>
            $<$<CONFIG:Release>:-static-libgcc>
        )
    else()
        message(STATUS "Static C++ runtime not available; using shared libraries")
    endif()
endif()

if(MUSICA_BUILD_TESTS)
    enable_testing()
    add_executable(musica_tests
        tests/test_core.cpp
        src/Queue.cpp
        src/PlaylistStore.cpp
        src/Spectrum.cpp
    )
    target_include_directories(musica_tests PRIVATE include)
    target_compile_options(musica_tests PRIVATE -Wall -Wextra)
    target_link_libraries(musica_tests PRIVATE Threads::Threads)
    add_test(NAME musica_core COMMAND musica_tests)
endif()

include(GNUInstallDirs)
set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATAROOTDIR}/doc/musica)

install(TARGETS musica RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES man/musica.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)
install(FILES
    LICENSE
    README.md
    README.fr.md
    CHANGELOG.md
    THIRD_PARTY_LICENSES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

set(CPACK_PACKAGE_NAME "musica")
set(CPACK_PACKAGE_VENDOR "Léonid Gr")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Terminal music player with album art and a live spectrum")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "Léonid Gr <leonidgrdev@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/leonidsergueigr/Musica")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# dpkg-shlibdeps is only available where dpkg-dev is installed. When it is
# missing, provide the runtime dependencies and the Debian architecture that
# CPack would otherwise discover itself.
find_program(DPKG_SHLIBDEPS_EXECUTABLE NAMES dpkg-shlibdeps)
if(NOT DPKG_SHLIBDEPS_EXECUTABLE)
    if(NOT CPACK_DEBIAN_PACKAGE_DEPENDS)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libc6, libstdc++6, libgcc-s1, zlib1g")
    endif()
    if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^armv7")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ppc64el)
        else()
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
        endif()
    endif()
endif()
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
include(CPack)
