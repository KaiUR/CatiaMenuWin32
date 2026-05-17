---
title: Developer Guide — CatiaMenuWin32
description: Build CatiaMenuWin32 from source. Covers prerequisites, project structure, versioning, adding script sources, and the CI/CD release pipeline for this CATIA V5 Python macro launcher.
---

# Developer Guide

## Contents
- [Prerequisites](#prerequisites)
- [Building from Source](#building-from-source)
- [Project Structure](#project-structure)
- [Versioning System](#versioning-system)
- [Adding a New Setting](#adding-a-new-setting)
- [Adding a New Menu Item](#adding-a-new-menu-item)
- [Adding a New Dialog](#adding-a-new-dialog)
- [Adding a New Script Source](#adding-a-new-script-source-type)
- [Filter and Sort System](#filter-and-sort-system)
- [CI/CD Workflow](#cicd-workflow)
- [Releasing a New Version](#releasing-a-new-version)
- [Thread Safety](#thread-safety)
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
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl
cmake --build build
```

The executable is output to `build/CatiaMenuWin32.exe`.

> **Note:** `res/app_icon.ico` must exist before building. It is included in the repository.

### Qt Creator

1. Open `CMakeLists.txt` in Qt Creator
2. Select a **Clang** kit (configured against the MSVC toolchain)
3. Add `-DCMAKE_C_COMPILER=clang-cl` to the CMake arguments
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
│   ├── help.c              In-app help window (TreeView + RichEdit)
│   ├── prefs.c             Favourites, hidden scripts, run counts, notes
│   ├── sources.c           Sources dialog — extra repos and local folders
│   ├── settings.c          Settings load/save, Settings dialog, About dialog
│   ├── updater.c           Update checker — GitHub releases API
│   └── quickbar.c          Floating Quick Launch Bar
├── res/
│   ├── resource.rc.in      Resource script template (CMake substitutes version)
│   ├── version.h.in        Version header template
│   ├── app.manifest        Application manifest (DPI awareness, ComCtl32 v6)
│   └── app_icon.ico        Application icon
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

## Adding a New Menu Item

The hamburger menu is built entirely in `Window_ShowMenu` (`window.c`) and commands are dispatched in `Handle_Command` (`main.c`).

1. **`src/resource.h`** — add a `#define IDM_MY_ACTION <value>` (use the next available number in the `IDM_*` range)
2. **`window.c` `Window_ShowMenu`** — add `AppendMenu(hSubMenu, MF_STRING, IDM_MY_ACTION, L"My Action")` to the appropriate sub-menu
3. **`main.c` `Handle_Command`** — add a `case IDM_MY_ACTION:` branch; keep it short — call a dedicated function if the logic is non-trivial
4. If the item should show a checkmark, pass the current state to `AppendMenu` with `MF_CHECKED` / `MF_UNCHECKED` flags, and toggle `g.cfg.*` + call `Settings_Save` in the handler

> **Tray menu:** if the item should also appear in the system tray right-click menu, add it to `Window_ShowTrayMenu` in the same way.

---

## Adding a New Dialog

All modal dialogs are defined in `res/resource.rc.in` and handled by an `INT_PTR CALLBACK` dialog proc in the appropriate `.c` file.

1. **`src/resource.h`** — add `#define IDD_MY_DIALOG <value>` and any `IDC_*` control IDs
2. **`res/resource.rc.in`** — add the `IDD_MY_DIALOG DIALOGEX` block with controls
3. **Appropriate `.c` file** — implement `MyDlgProc(HWND, UINT, WPARAM, LPARAM)` handling at minimum `WM_INITDIALOG` (populate) and `IDOK` / `IDCANCEL` (read back / dismiss)
4. **`main.h`** — declare `INT_PTR CALLBACK MyDlgProc(HWND, UINT, WPARAM, LPARAM);`
5. **Caller** — open with `DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MY_DIALOG), g.hwnd, MyDlgProc)`

**Theme-aware dialogs:** to match the app theme, handle `WM_ERASEBKGND` and return a brush of `COL_BG()`, and call `Window_ApplyDarkMode(hwnd)` and `Window_ApplyThemeToChildren(hwnd)` in `WM_INITDIALOG`.

---

## Filter and Sort System

### Filter

The search box (`hwnd_search`, `IDC_SEARCH`) posts `EN_CHANGE` notifications to `MainWndProc`. The handler copies the edit text to `g.filter_text` and calls `Tabs_ApplyFilter()`.

`Tabs_ApplyFilter` → `Tabs_RebuildButtons` — which calls `Tabs_ScriptMatchesFilter` for every non-hidden script. A script is shown only when the filter is empty or its `name` / `meta.purpose` contains `g.filter_text` (case-insensitive substring match via `wcsstr`).

Clearing the search box sets `g.filter_text[0] = L'\0'` and rebuilds — all non-hidden scripts reappear.

### Sort

`SortMode` values are stored in `g.cfg.sort_mode` (global default, settable in Settings) and in each `ScriptFolder.sort_mode` (per-tab override, not yet exposed in UI).

`Tabs_ApplySort(fi)` is called after every sync and after each manual sort change. It calls `qsort` on `g.folders[fi].scripts[]` with the matching comparator:

| `SortMode` | Comparator | Notes |
|------------|------------|-------|
| `SORT_ORDER` | (none) | GitHub API / disk order — `qsort` not called |
| `SORT_ALPHA` | `_wcsicmp(a->name, b->name)` | Case-insensitive A-Z |
| `SORT_DATE` | `wcscmp(b->date, a->date)` | Descending: newest first |
| `SORT_MOST_USED` | `b->run_count - a->run_count` | Descending: most runs first |

After `Tabs_ApplySort`, call `Tabs_RebuildButtons` (or `Tabs_Switch`) to reflect the new order in the UI.

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
5. Run `cmake -DCMAKE_C_COMPILER=clang-cl -DVERSION_OVERRIDE=major.minor.patch` (increments `build_number.txt`)
6. Build with Ninja
7. **Code-sign** `CatiaMenuWin32.exe` via `skymatic/code-sign-action@v1` using the certificate stored in GitHub Secrets
8. Commit `build_number.txt` and `CONTRIBUTORS.md` back to `main`
9. Import GPG key and sign the new tag — deletes the base tag (e.g. `v1.2.0`), creates a GPG-signed tag with the full build number (e.g. `v1.2.0.31`)
10. Create GitHub Release with the signed `CatiaMenuWin32.exe`, `README.md`, `CONTRIBUTORS.md`, `LICENSE.txt`

### Code signing secrets

The following secrets must be configured in the repository (Settings → Secrets → Actions):

| Secret | Description |
|--------|-------------|
| `CERTIFICATE` | Base64-encoded PFX file. Generate with: `[System.Convert]::ToBase64String([IO.File]::ReadAllBytes('cert.pfx'))` |
| `PASSWORD` | Password protecting the PFX certificate |
| `CERTHASH` | SHA1 thumbprint of the certificate (from MMC → Certificates) |
| `CERTNAME` | Common name of the certificate |

### GPG signing secrets

The following secrets are used by `crazy-max/ghaction-import-gpg@v6` to sign release tags:

| Secret | Description |
|--------|-------------|
| `GPG_PRIVATE_KEY` | ASCII-armored GPG private key. Export with: `gpg --armor --export-secret-keys KEY_ID` |
| `GPG_PASSPHRASE` | Passphrase protecting the GPG private key |

The imported key is configured as the git signing key; every release tag is created with `git tag -s`, producing a verified tag on GitHub. The committer identity baked into the key must match the `git_committer_name` / `git_committer_email` values in the workflow (`KaiUR` / `kairathjen@yahoo.com`).

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

## Thread Safety

The app uses two background threads (sync and updater) plus the UI thread. Thread-safe communication follows two patterns:

### Posting to the UI thread
Never call Win32 UI functions from a background thread. Use `PostMessage` to marshal work to the UI thread:

```c
// From any thread — allocate a buffer, format, post:
PostStatus(L"Sync done.");  // inline helper in main.h

// Custom messages (defined in main.h):
PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)result, 0);
PostMessage(g.hwnd, WM_SCRIPT_STARTED, 0, 0);
PostMessage(g.hwnd, WM_SCRIPT_STOPPED, 0, 0);
```

`WM_STATUS_SET` frees the heap buffer after displaying it. All other custom messages use simple `wParam`/`lParam` values.

### Protecting shared state
`g.folders[]` and `g.folder_count` are written by the sync thread and read by the UI thread. Guard every access with `g.cs_folders`:

```c
EnterCriticalSection(&g.cs_folders);
// read or write g.folders[] here
LeaveCriticalSection(&g.cs_folders);
```

### Atomic handle ownership (`g.run_process`)
The running-script process handle is shared between `Runner_Thread` (writer) and `Runner_Stop` / `MainWndProc` (readers). Use `InterlockedExchangePointer` for ownership transfer — exactly one caller gets the non-NULL handle:

```c
// Runner_Thread — store a duplicate after CreateProcess:
DuplicateHandle(..., pi.hProcess, ..., &dup, ...);
InterlockedExchangePointer((void **)&g.run_process, dup);

// Runner_Stop — atomically claim it:
HANDLE h = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
if (h) { TerminateProcess(h, 1); CloseHandle(h); }

// Runner_Thread cleanup — take back if Stop hasn't claimed it:
HANDLE old = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
if (old) CloseHandle(old);  // normal completion path
```

This pattern guarantees no double-close and no double-terminate regardless of which side wins the race.

---

## Code Style

- **C11** — designated initialisers, `bool`, `stdbool.h`, `_Static_assert`
- **Unicode throughout** — `WCHAR`, `L""` literals, `_snwprintf_s`, `wcslen` etc.
- **Memory-safe functions only** — always use the `_s` (C11 Annex K) or bounded variants; never use functions flagged by `clang-analyzer-security.insecureAPI`. The full substitution table:

  | Never use | Use instead | Notes |
  |-----------|-------------|-------|
  | `strcpy`, `wcscpy` | `strcpy_s`, `wcscpy_s` | always pass `_countof(dest)` |
  | `strcat`, `wcscat` | `strcat_s`, `wcscat_s` | always pass `_countof(dest)` |
  | `strncpy`, `wcsncpy` | `strncpy_s`, `wcsncpy_s` | `_s` variant guarantees NUL termination |
  | `sprintf`, `swprintf` | `sprintf_s`, `swprintf_s` | pass `_countof(buf)` |
  | `snprintf`, `_snwprintf` | `_snprintf_s`, `_snwprintf_s` | use `_TRUNCATE` as the count argument |
  | `fprintf` | `fprintf_s` | |
  | `vsprintf`, `vswprintf` | `vsprintf_s`, `vswprintf_s` | |
  | `gets` | `fgets` | |
  | `memcpy` | `memcpy_s` | pass `destSize` then `count` |
  | `memmove` | `memmove_s` | pass `destSize` then `count` |
  | `memset` (zeroing secrets) | `SecureZeroMemory` | prevents compiler from eliding the zero |
- **No CRT allocations in WndProcs** where possible — use stack buffers
- **Heap allocations** — use `calloc`/`malloc` + matching `free`; for `ScriptFolder` use the `Folder_Alloc`/`Folder_Free`/`Folder_Push` helpers in `main.h`
- **Double-buffered GDI** — all painting via memory DC, `BitBlt` to screen
- **No globals except `AppState g`** — all state in the single `g` struct
- **Runtime colour functions** — `COL_BG()`, `COL_TEXT()` etc. return dark or light values based on `g.dark_mode`
- **PostStatus()** for cross-thread status bar updates — uses `PostMessage` to marshal to UI thread
- **`Util_Log()`** for debug output — goes to `OutputDebugStringW`, visible in Qt Creator Application Output
