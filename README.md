# CatiaMenuWin32

A lightweight, native **Win32 API / C** macro launcher for PyCATIA scripts,
built with the same philosophy as [MineSweeperWin32](https://github.com/KaiUR/MineSweeperWin32)
— direct Win32 API, no frameworks, pure C11.

## 🎯 Motivation & Project Goals

* **Win32 UI from scratch** — Tab control, owner-draw buttons, scrollable panel,
  status bar, and DIALOGEX resources, all hand-built with WinAPI.
* **Live GitHub sync** — Scripts are fetched via the GitHub REST API (WinINet).
  SHA comparison detects changed files; new/removed folders update tabs automatically.
* **All state in AppData** — Settings (`settings.ini`) and the sync manifest
  (`manifest.ini`) live in `%APPDATA%\CatiaMenuWin32\`. Nothing is written next
  to the `.exe`.
* **Pure C (C11)** — Zero heavy external dependencies.

## 📂 Script Tabs

Tabs are built dynamically from the current folders in
[KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts):

| Tab label | Repo folder |
|-----------|-------------|
| Any Document Scripts | `Any_Document_Scripts/` |
| Part Document Scripts | `Part_Document_Scripts/` |
| Process Document Scripts | `Process_Document_Scripts/` |
| Product Document Scripts | `Product_Document_Scripts/` |

If folders are added or removed from the repo, tabs update on the next sync.

## 🚀 Features

* **Startup sync** — On launch the app checks the GitHub API, compares blob SHAs,
  downloads changed/new scripts, and deletes removed ones.
* **Dynamic tabs** — Folder additions and removals are detected automatically;
  no recompile needed.
* **Script cache** — Scripts cached to `%APPDATA%\CatiaMenuWin32\scripts\<folder>\`.
* **AppData settings** — `%APPDATA%\CatiaMenuWin32\settings.ini` stores Python path,
  cache dir, GitHub token, and all options.
* **Sync manifest** — `%APPDATA%\CatiaMenuWin32\manifest.ini` tracks the SHA of
  every cached script for efficient delta-sync.
* **Auto-versioning** — CMake auto-increments `build_number.txt` on every configure.

## 🛠️ Built With

* **Language**: C (C11)
* **API**: Win32 API (User32, GDI32, ComCtl32, WinINet, Shell32, Shlwapi)
* **Build system**: CMake 3.16+ / Ninja
* **Compiler**: MinGW-w64 / GCC

## 📦 Building from Source

```bash
git clone https://github.com/KaiUR/CatiaMenuWin32
cd CatiaMenuWin32
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

> **Note**: Add `res/app_icon.ico` before building — the resource script references it.

## 🔢 Versioning System

Identical to MineSweeperWin32:

* `build_number.txt` increments on every `cmake` configure.
* CMake generates `version.h` from `res/version.h.in`, injecting version,
  build number, and date into the binary's `VERSIONINFO` resource.

## How to Create a Release

```bash
git tag v1.0.0
git push origin v1.0.0
```

GitHub Actions builds, packages, and publishes the release automatically.
The build number is appended (e.g. `v1.0.0.12`).

## ⚙️ Settings  (`%APPDATA%\CatiaMenuWin32\settings.ini`)

| Key | Description |
|-----|-------------|
| `Python\Executable` | Full path to `python.exe` (auto-detected if blank) |
| `Scripts\CacheDir` | Where scripts are stored locally |
| `GitHub\Token` | Optional PAT — higher rate limit / private repos |
| `Options\AutoSync` | `1` = sync on startup (default on) |
| `Options\DownloadBeforeRun` | `1` = always re-download before running |
| `Options\ShowConsole` | `1` = show Python console window |

## 📄 License

MIT License — Copyright (c) 2025 Kai-Uwe Rathjen

---

**Author**: [Kai-Uwe Rathjen](https://github.com/KaiUR)
