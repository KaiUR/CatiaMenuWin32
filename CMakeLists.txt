cmake_minimum_required(VERSION 3.16)

# --- Determine version from git tag or VERSION_OVERRIDE ---
if(DEFINED VERSION_OVERRIDE)
    # Passed explicitly by the workflow: -DVERSION_OVERRIDE=1.0.6
    set(APP_VERSION ${VERSION_OVERRIDE})
    set(IS_LOCAL_BUILD 0)
else()
    # Get the highest version tag (not nearest ancestor - workflow tags
    # are created on CI commits not on local HEAD)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} tag --sort=-creatordate --list "v[0-9]*"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_ALL_TAGS
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REGEX MATCH "v[0-9][^

]*" GIT_LATEST_TAG "${GIT_ALL_TAGS}")
        string(STRIP "${GIT_LATEST_TAG}" GIT_LATEST_TAG)
    endif()

    if(GIT_LATEST_TAG)
        # Strip leading 'v' and any trailing .buildnum the workflow may have added
        string(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_LATEST_TAG}")
        # Keep only major.minor.patch (first 3 numeric components)
        string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" APP_VERSION "${GIT_VERSION}")
    endif()

    if(NOT APP_VERSION OR APP_VERSION STREQUAL "")
        set(APP_VERSION "1.0.0")
    endif()

    set(IS_LOCAL_BUILD 1)
endif()

message(STATUS "App version: ${APP_VERSION}  (local build: ${IS_LOCAL_BUILD})")

# Ensure patch version is never empty (configure_file fails on empty @VAR@)
if(NOT PROJECT_VERSION_PATCH)
    set(PROJECT_VERSION_PATCH "0")
endif()
if(NOT PROJECT_VERSION_TWEAK)
    set(PROJECT_VERSION_TWEAK "0")
endif()

project(CatiaMenuWin32 VERSION ${APP_VERSION} LANGUAGES C RC)

# Static CRT: prevents lld-link embedding a Microsoft.VC*.CRT SxS dependency
# in the manifest, which would require the VC++ Redistributable on end-user machines.
if(NOT MINGW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --- Build Number ---
# CMake always increments the build number - both locally and in CI.
# The workflow reads the resulting number from build_number.txt for the tag.
if(EXISTS "${CMAKE_SOURCE_DIR}/build_number.txt")
    file(READ "${CMAKE_SOURCE_DIR}/build_number.txt" BUILD_NUMBER)
    string(STRIP "${BUILD_NUMBER}" BUILD_NUMBER)
    if(DEFINED VERSION_OVERRIDE)
        # CI build - increment by 1 to match release tag
        math(EXPR BUILD_NUMBER "${BUILD_NUMBER} + 1")
    else()
        # Local build - increment by 2 so local is always ahead of the release
        math(EXPR BUILD_NUMBER "${BUILD_NUMBER} + 2")
    endif()
else()
    set(BUILD_NUMBER "1")
endif()
file(WRITE "${CMAKE_SOURCE_DIR}/build_number.txt" "${BUILD_NUMBER}")

# --- Timestamps ---
string(TIMESTAMP CURRENT_DATE "%Y-%m-%d")
string(TIMESTAMP CURRENT_YEAR "%Y")

# --- Generate version.h and resource.rc ---
# MinGW/windres: embed the manifest directly in the RC file.
# Clang+lld-link: CMake adds /MANIFEST:EMBED automatically; pass the manifest
# via /MANIFESTINPUT so the RC does not also embed it (avoids duplicate error).
if(MINGW)
    set(RC_MANIFEST_LINE "1 RT_MANIFEST \"app.manifest\"")
else()
    set(RC_MANIFEST_LINE "")
endif()
configure_file(res/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)
configure_file(res/resource.rc.in "${CMAKE_CURRENT_BINARY_DIR}/resource.rc" @ONLY)

# --- Executable ---
add_executable(CatiaMenuWin32 WIN32
    src/main.c
    src/main.h
    src/resource.h
    src/window.c
    src/tabs.c
    src/github.c
    src/sync.c
    src/runner.c
    src/meta.c
    src/updater.c
    src/settings.c
    src/sources.c
    src/prefs.c
    src/help.c
    src/paint.c
    "${CMAKE_CURRENT_BINARY_DIR}/resource.rc"
)

# --- Include Paths ---
target_include_directories(CatiaMenuWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# --- RC include path so the RC compiler finds app_icon.ico and app.manifest ---
# windres (MinGW) uses -I; MSVC rc.exe and llvm-rc use /I
if(CMAKE_RC_COMPILER MATCHES "windres")
    set(RC_INCLUDE_FLAG "-I")
else()
    set(RC_INCLUDE_FLAG "/I")
endif()
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/resource.rc"
    PROPERTIES COMPILE_FLAGS "${RC_INCLUDE_FLAG}${CMAKE_CURRENT_SOURCE_DIR}/res"
)

# --- Link Libraries ---
target_link_libraries(CatiaMenuWin32 PRIVATE
    user32 gdi32 comctl32 comdlg32
    wininet shell32 shlwapi uxtheme dwmapi crypt32
)

# Ensure Unicode entry point is used on all Windows toolchains
target_compile_definitions(CatiaMenuWin32 PRIVATE UNICODE _UNICODE)

if(MINGW)
    # MinGW: -municode sets the Unicode CRT entry and defines UNICODE/_UNICODE
    target_compile_options(CatiaMenuWin32 PRIVATE -municode)
    target_link_options(CatiaMenuWin32 PRIVATE
        -mwindows
        -municode
        $<$<CONFIG:Release>:-s>
    )
elseif(WIN32 AND NOT MINGW)
    # Clang+lld-link: CMake adds /MANIFEST:EMBED; supply our custom manifest
    # via /MANIFESTINPUT. The RC file emits no RT_MANIFEST for this toolchain.
    target_link_options(CatiaMenuWin32 PRIVATE
        -Xlinker /MANIFESTINPUT:${CMAKE_SOURCE_DIR}/res/app.manifest
    )
endif()

if(MSVC)
    target_compile_options(CatiaMenuWin32 PRIVATE /W4)
else()
    target_compile_options(CatiaMenuWin32 PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS CatiaMenuWin32 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES CONTRIBUTORS.md LICENSE.txt README.md DESTINATION ${CMAKE_INSTALL_BINDIR})
