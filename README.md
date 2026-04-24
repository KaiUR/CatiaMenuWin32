# CatiaMenuWin32

A lightweight, native **Win32 API / C** macro launcher for PyCATIA scripts,
built with the same philosophy as [MineSweeperWin32](https://github.com/KaiUR/MineSweeperWin32)
— direct Win32 API, no frameworks, pure C11.

## 🎯 What It Does

CatiaMenuWin32 connects directly to the [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts)
GitHub repository and presents every script as a clickable button. Click a button — the script runs.
No CATIA macro editor, no manual path setup, no copy-pasting.

## 📂 Script Tabs

Tabs are built dynamically from the folders in `KaiUR/Pycatia_Scripts`. If folders are
added or removed from the repo, tabs update automatically on the next sync:

| Tab | Folder |
|-----|--------|
| Any Document Scripts | `Any_Document_Scripts/` |
| Part Document Scripts | `Part_Document_Scripts/` |
| Process Document Scripts | `Process_Document_Scripts/` |
| Product Document Scripts | `Product_Document_Scripts/` |

## 🚀 Features

| Feature | Detail |
|---------|--------|
| **Live GitHub sync** | Fetches repo structure and compares SHA hashes — only downloads changed files |
| **Dynamic tabs** | Folder additions/removals detected automatically; no recompile needed |
| **Script info tooltip** | Hover over the `i` badge on any button to see Purpose, Author, Version, Date, and Description parsed from the script header |
| **AppData settings** | All settings in `%APPDATA%\CatiaMenuWin32\settings.ini` |
| **Sync manifest** | `%APPDATA%\CatiaMenuWin32\manifest.ini` tracks SHA per script for efficient delta-sync |
| **Always on Top** | Window stays above CATIA so you can click scripts without alt-tabbing |
| **System Tray** | Minimize to tray; restore with double-click |
| **Start with Windows** | Autorun via registry with optional start-minimized flag |
| **Update checker** | Checks GitHub Releases on startup and notifies if a newer version is available |
| **Update Dependencies** | Runs `setup/update.bat` or falls back to `pip install -r requirements.txt` |
| **Dark / Light / System theme** | Follows Windows theme by default |
| **Auto-versioning** | CMake increments `build_number.txt` on every configure |

## 🛠️ Built With

- **Language**: C (C11)
- **API**: Win32 API — User32, GDI32, ComCtl32, WinINet, Shell32, Shlwapi, DwmApi
- **Build system**: CMake 3.16+ / Ninja
- **Compiler**: MinGW-w64 (GCC 13)
- **AI Assistance**: Claude (Anthropic) — used to assist with code generation, debugging, and architecture decisions

## 📦 Building from Source

### Prerequisites
- [MinGW-w64](https://www.mingw-w64.org/) — GCC 13+
- [CMake 3.16+](https://cmake.org/)
- [Ninja](https://ninja-build.org/)
- Qt Creator (optional, used as IDE)

### Steps

```bash
git clone https://github.com/KaiUR/CatiaMenuWin32
cd CatiaMenuWin32
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

> **Note**: Add `res/app_icon.ico` before building.

### Qt Creator

1. Open `CMakeLists.txt` in Qt Creator
2. Select the MinGW-w64 kit
3. Build → Build All

## ⚙️ Settings  (`%APPDATA%\CatiaMenuWin32\settings.ini`)

| Setting | Default | Description |
|---------|---------|-------------|
| `Python\Executable` | auto-detect | Full path to `python.exe` |
| `Scripts\CacheDir` | `%APPDATA%\CatiaMenuWin32\scripts` | Local script cache |
| `GitHub\Token` | empty | Optional PAT — raises API rate limit from 60 to 5000 req/hr and allows private repo access |
| `Options\AutoSync` | on | Sync scripts from GitHub on startup |
| `Options\DownloadBeforeRun` | off | Always fetch latest before running (slower) |
| `Options\ShowConsole` | off | Show Python console window when running a script |
| `Options\ConsoleKeepOpen` | on | Keep console open after script finishes (use `/k` flag) so you can read errors |
| `Options\CheckUpdates` | on | Check GitHub Releases for a newer app version on startup |
| `Window\AlwaysOnTop` | on | Keep window above other windows |
| `Window\MinimizeToTray` | off | Hide to system tray instead of taskbar on minimize |
| `Window\StartWithWindows` | off | Add to Windows autorun registry key |
| `Window\StartMinimized` | on | Start hidden/minimized (passes `/minimized` flag via autorun) |
| `Window\Theme` | 0 (System) | 0 = follow Windows, 1 = dark, 2 = light |

## 🔑 GitHub Token (optional)

The app uses the GitHub REST API to fetch the script list. Without a token, GitHub
allows 60 requests per hour per IP — usually plenty. If you hit the limit (the
script list fails to load), add a **Personal Access Token**:

1. Go to GitHub → Settings → Developer settings → Personal access tokens → Fine-grained tokens
2. Create a token with **read-only** access to public repositories (no special scopes needed for public repos)
3. Paste the token in **Settings → Use token**

The token is stored in `settings.ini` and sent as an `Authorization: token <tok>` header.
It is never transmitted anywhere except `api.github.com` and `raw.githubusercontent.com`.

## 🖥️ Console Window Options

When **Show Python console window** is enabled:
- A console window opens when a script runs
- If **Keep console open after script finishes** is also enabled, the window stays open after the script exits — equivalent to running `cmd.exe /k python script.py`. This lets you see any error messages or print output before closing.

Without **Show console window**, scripts run silently in the background and the status bar shows the exit code.

## 🔄 Update Dependencies

The **Update Deps** button (and `Run → Update Dependencies` menu item) runs `setup/update.bat` from the cached scripts folder. If that file doesn't exist, the app falls back to running `pip install -r setup/requirements.txt` using your configured Python interpreter.

The console stays open after the update so you can see the pip output.

## 🔢 Versioning System

Same system as MineSweeperWin32:
- `build_number.txt` auto-increments on every `cmake` configure
- CMake generates `version.h` from `res/version.h.in`

## How to Release

```bash
git tag v1.0.1
git push origin v1.0.1
```

GitHub Actions builds and publishes automatically. Build number is appended: `v1.0.1.15`.

## 📄 License

MIT License — Copyright © 2025 Kai-Uwe Rathjen

Developed with AI assistance from Claude (Anthropic).

---

**Author**: [Kai-Uwe Rathjen](https://github.com/KaiUR)
