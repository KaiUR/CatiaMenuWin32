# Developer Guide

## Contents
- [Prerequisites](#prerequisites)
- [Building from Source](#building-from-source)
- [Project Structure](#project-structure)
- [Versioning System](#versioning-system)
- [Adding a New Script Source](#adding-a-new-script-source)
- [Adding a New Setting](#adding-a-new-setting)
- [CI/CD Workflow](#cicd-workflow)
- [Releasing a New Version](#releasing-a-new-version)
- [Code Style](#code-style)

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [LLVM](https://releases.llvm.org/) | 17+ | C compiler (Clang) |
| [CMake](https://cmake.org/) | 3.16+ | Build system |
| [Ninja](https://ninja-build.org/) | any | Build backend |
| [Visual Studio](https://visualstudio.microsoft.com/) | 2019+ | Windows SDK and linker (`rc.exe`, `link.exe`) |
| [Qt Creator](https://www.qt.io/product/development-tools) | any | IDE (optional) |
| [Git](https://git-scm.com/) | any | Version control |

> **Note:** Visual Studio (or Build Tools for Visual Studio) is required to provide `rc.exe` (resource compiler) and the Windows SDK headers. LLVM/Clang is the C compiler; Visual Studio provides the rest of the Windows toolchain.

---

## Building from Source

### Command Line

Open a **Developer Command Prompt for VS** (or run `ilammy/msvc-dev-cmd` equivalent), then:

```bash
git clone https://github.com/KaiUR/CatiaMenuWin32
cd CatiaMenuWin32
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
cmake --build build
```

The executable is output to `build/CatiaMenuWin32.exe`.

> **Note:** `res/app_icon.ico` must exist before building. It is not included in the repository — generate it from the project icon or use any `.ico` file.

### Qt Creator

1. Open `CMakeLists.txt` in Qt Creator
2. Select a **Clang** kit (configured against the MSVC toolchain)
3. Add `-DCMAKE_C_COMPILER=clang` to the CMake arguments
4. Configure the project
5. **Build → Build All** (`Ctrl+B`)

### CMake Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `VERSION_OVERRIDE` | unset | Override version (used by CI workflow) |
| `CMAKE_BUILD_TYPE` | Debug | `Release` strips debug symbols |

When `VERSION_OVERRIDE` is not set, CMake reads the latest git tag via `git tag --sort=-creatordate` and uses it as the base version. The build number is always incremented from `build_number.txt`.

---

## Project Structure

```
CatiaMenuWin32/
├── src/                    Source files
│   ├── main.h              Central header — all structs, types, prototypes
│   ├── main.c              Entry point, WinMain, MainWndProc, message handling
│   ├── resource.h          All #define IDs for controls, menus, dialogs
│   ├── window.c            Window creation, tab bar, tray, menu popup
│   ├── tabs.c              Tab switching, script button creation, scroll panel
│   ├── paint.c             All GDI rendering — toolbar, buttons, tooltip
│   ├── github.c            HTTPS requests, cert validation, SHA computation
│   ├── sync.c              GitHub sync thread, local dir scanning, manifest
│   ├── runner.c            Script execution, Python detection, Update Deps
│   ├── meta.c              Script header parser (Purpose, Author, etc.)
│   ├── sources.c           Sources dialog — extra repos and local folders
│   ├── settings.c          Settings load/save, Settings dialog, About dialog
│   └── updater.c           Update checker — GitHub releases API
├── res/
│   ├── resource.rc.in      Resource script template (CMake substitutes version)
│   ├── version.h.in        Version header template
│   ├── app.manifest        Application manifest (DPI awareness, ComCtl32 v6)
│   └── app_icon.ico        Application icon (not in repo — add before building)
├── docs/                   Documentation
├── .github/
│   └── workflows/
│       └── release.yml     GitHub Actions CI/CD workflow
├── CMakeLists.txt          Build configuration
├── build_number.txt        Auto-incremented build counter
├── README.md               Project overview
├── LICENSE.txt             MIT license
└── CONTRIBUTORS.md         Auto-generated from git history
```

---

## Versioning System

Version format: `major.minor.patch.build` — e.g. `v1.2.0.31`

- **major.minor.patch** — set by the git tag you push (e.g. `v1.2.0`)
- **build** — auto-incremented by CMake from `build_number.txt` on every configure

### Local builds
CMake reads the latest tag via `git tag --sort=-creatordate`, extracts `major.minor.patch`, increments `build_number.txt`, and generates `version.h`. The resulting binary shows e.g. `v1.2.0.32`.

### CI builds
The workflow passes `-DVERSION_OVERRIDE=1.2.0` to CMake (extracted from your tag), CMake increments `build_number.txt`, the workflow commits the new number back to `main`, then creates the final tag `v1.2.0.31` matching the binary exactly.

### `version.h` (generated)
```c
#define VERSION_MAJOR     1
#define VERSION_MINOR     2
#define VERSION_PATCH     0
#define VERSION_BUILD     31
#define IS_LOCAL_BUILD    1   /* 0 in CI builds */
#define VERSION_STRING_W  L"1.2.0.31"
#define VERSION_DISPLAY_W L"1.2.0.31"
```

---

## Adding a New Setting

1. **`main.h`** — add field to the `Settings` struct
2. **`settings.c`** — add `GetPrivateProfileInt/String` in `Settings_Load` and `WritePrivateProfile*` in `Settings_Save`
3. **`res/resource.rc.in`** — add control to the Settings dialog (`IDD_SETTINGS = 300`)
4. **`src/resource.h`** — add `#define IDC_*` for the new control
5. **`settings.c` `SettingsDlgProc`** — handle in `WM_INITDIALOG` (populate) and `IDOK` (read back)
6. Use the setting wherever needed in the relevant `.c` file

---

## Adding a New Script Source Type

Script sources are synced in `sync.c`:

- `Sync_Thread` — main GitHub repo sync (steps 1–5)
- `Sync_ExtraRepo` — extra GitHub repos (step 6)
- `Sync_LocalDir` — local folders (step 7)
- `Sync_MergeFolder` — merges scripts into `g.folders[]` by folder name

To add a new source type:
1. Add fields to `Settings` in `main.h`
2. Add load/save in `settings.c`
3. Add a UI in `sources.c`
4. Add a sync function in `sync.c` following the pattern of `Sync_ExtraRepo`
5. Call it from `Sync_Thread` and `Sync_LoadManifest`

---

## CI/CD Workflow

The workflow (`.github/workflows/release.yml`) runs on `windows-latest` using **LLVM/Clang** as the C compiler with the Visual Studio MSVC toolchain providing the Windows SDK and resource compiler.

It triggers on:
- Push to `main` branch — builds only, no release
- Push of a `v*` tag — full build, code signing, version tagging, and release

### Tag release flow
1. Checkout with full history
2. Set up MSVC environment (`ilammy/msvc-dev-cmd`) and add LLVM to PATH
3. Update `CONTRIBUTORS.md` from git log
4. Extract `major.minor.patch` from the pushed tag
5. Run `cmake -DCMAKE_C_COMPILER=clang -DVERSION_OVERRIDE=major.minor.patch` (increments `build_number.txt`)
6. Build with Ninja
7. **Code-sign** `CatiaMenuWin32.exe` via `skymatic/code-sign-action@v1` using the certificate stored in GitHub Secrets
8. Commit `build_number.txt` and `CONTRIBUTORS.md` back to `main`
9. Delete the base tag (e.g. `v1.2.0`), create new tag with build number (e.g. `v1.2.0.31`)
10. Create GitHub Release with the signed `CatiaMenuWin32.exe`, `README.md`, `CONTRIBUTORS.md`, `LICENSE.txt`

### Code signing secrets

The following secrets must be configured in the repository (Settings → Secrets → Actions):

| Secret | Description |
|--------|-------------|
| `CERTIFICATE` | Base64-encoded PFX file. Generate with: `[System.Convert]::ToBase64String([IO.File]::ReadAllBytes('cert.pfx'))` |
| `PASSWORD` | Password protecting the PFX certificate |
| `CERTHASH` | SHA1 thumbprint of the certificate (from MMC → Certificates) |
| `CERTNAME` | Common name of the certificate |

---

## Releasing a New Version

```bash
git tag v1.3.0
git push origin v1.3.0
```

That's it. The workflow handles everything else automatically. The final release tag will be `v1.3.0.<buildnum>`.

**Increment rules:**
- **Patch** (`v1.2.1`) — bug fixes
- **Minor** (`v1.3.0`) — new features, backward compatible
- **Major** (`v2.0.0`) — breaking changes or major redesign

---

## Code Style

- **C11** — designated initialisers, `bool`, `stdbool.h`, `_Static_assert`
- **Unicode throughout** — `WCHAR`, `L""` literals, `_snwprintf_s`, `wcslen` etc.
- **Bounds-safe string ops** — always `wcsncpy`/`wcsncat` with explicit limits; never `wcscpy`/`wcscat`
- **Safe formatted output** — always `_snwprintf_s` with `_TRUNCATE`; never `_snwprintf` or `swprintf`
- **No CRT allocations in WndProcs** where possible — use stack buffers
- **Heap allocations** — use `calloc`/`malloc` + matching `free`; for `ScriptFolder` use the `Folder_Alloc`/`Folder_Free`/`Folder_Push` helpers in `main.h`
- **Double-buffered GDI** — all painting via memory DC, `BitBlt` to screen
- **No globals except `AppState g`** — all state in the single `g` struct
- **Runtime colour functions** — `COL_BG()`, `COL_TEXT()` etc. return dark or light values based on `g.dark_mode`
- **PostStatus()** for cross-thread status bar updates — uses `PostMessage` to marshal to UI thread
- **`Util_Log()`** for debug output — goes to `OutputDebugStringW`, visible in Qt Creator Application Output
