cmake_minimum_required(VERSION 3.16)

# --- Determine version from git tag or VERSION_OVERRIDE ---
if(DEFINED VERSION_OVERRIDE)
    # Passed explicitly by the workflow: -DVERSION_OVERRIDE=1.0.6
    set(APP_VERSION ${VERSION_OVERRIDE})
    set(IS_LOCAL_BUILD 0)
else()
    # Try to get the latest semver tag from git
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v[0-9]*"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_LATEST_TAG
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    if(GIT_LATEST_TAG)
        # Strip leading 'v' and any trailing .buildnum the workflow may have added
        string(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_LATEST_TAG}")
        # Keep only major.minor.patch (first 3 numeric components)
        string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" APP_VERSION "${GIT_VERSION}")
    endif()

    if(NOT APP_VERSION)
        set(APP_VERSION "1.0.0")
    endif()

    set(IS_LOCAL_BUILD 1)
endif()

message(STATUS "App version: ${APP_VERSION}  (local build: ${IS_LOCAL_BUILD})")

project(CatiaMenuWin32 VERSION ${APP_VERSION} LANGUAGES C RC)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --- Build Number ---
if(EXISTS "${CMAKE_SOURCE_DIR}/build_number.txt")
    file(READ "${CMAKE_SOURCE_DIR}/build_number.txt" BUILD_NUMBER)
    string(STRIP "${BUILD_NUMBER}" BUILD_NUMBER)
    math(EXPR BUILD_NUMBER "${BUILD_NUMBER} + 1")
else()
    set(BUILD_NUMBER "1")
endif()
file(WRITE "${CMAKE_SOURCE_DIR}/build_number.txt" "${BUILD_NUMBER}")

# --- Timestamps ---
string(TIMESTAMP CURRENT_DATE "%Y-%m-%d")
string(TIMESTAMP CURRENT_YEAR "%Y")

# --- Generate version.h ---
configure_file(res/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

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
    src/paint.c
    res/resource.rc
)

# --- Include Paths ---
target_include_directories(CatiaMenuWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# --- Link Libraries ---
target_link_libraries(CatiaMenuWin32 PRIVATE
    user32 gdi32 comctl32 comdlg32
    wininet shell32 shlwapi uxtheme dwmapi crypt32
)

if(MINGW)
    target_compile_options(CatiaMenuWin32 PRIVATE -municode)
    target_link_options(CatiaMenuWin32 PRIVATE
        -mwindows
        -municode
        $<$<CONFIG:Release>:-s>
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
