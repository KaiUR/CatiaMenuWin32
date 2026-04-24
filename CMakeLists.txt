cmake_minimum_required(VERSION 3.16)

project(CatiaMenuWin32 VERSION 1.0.0 LANGUAGES C RC)

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

# --- Generate version.h for C source files ---
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
    wininet shell32 shlwapi uxtheme dwmapi
)

if(MINGW)
    # -mwindows:  GUI subsystem (no console)
    # -municode:  sets entry point to wWinMain (wide-char) instead of WinMain
    target_compile_options(CatiaMenuWin32 PRIVATE -municode)
    target_link_options(CatiaMenuWin32 PRIVATE
        -mwindows
        -municode
        $<$<CONFIG:Release>:-s>
    )
endif()

# --- Compiler Warnings ---
if(MSVC)
    target_compile_options(CatiaMenuWin32 PRIVATE /W4)
else()
    target_compile_options(CatiaMenuWin32 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Install ---
include(GNUInstallDirs)
install(TARGETS CatiaMenuWin32 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES CONTRIBUTORS.md LICENSE.txt README.md DESTINATION ${CMAKE_INSTALL_BINDIR})
