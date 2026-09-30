---
title: CatiaMenuWin32 — Overview
description: Overview of CatiaMenuWin32 — a native Win32 Python script launcher and macro manager for CATIA V5 and PyCATIA automation.
---

# CatiaMenuWin32

A lightweight, native **Win32 API / C** Python script launcher and manager
— built for PyCATIA and CATIA V5 automation, but works as a general-purpose launcher for any Python scripts or application macros. Direct Win32 API, no frameworks, pure C11.

## 📚 Documentation

Full documentation is available in the [`docs/`](docs/) folder:

- [User Guide](user-guide) — installation, settings, sources, running scripts
- [Developer Guide](developer-guide) — building from source, project structure, releasing
- [File Reference](file-reference) — source files, structs, functions, constants
- [Changelog](changelog) — release history

---

## 📸 Screenshots

**Dark Mode**

![CatiaMenuWin32 Dark Mode](images/Main_App_DarkMode_V2.1.0.128.PNG)

**Light Mode**

![CatiaMenuWin32 Light Mode](images/Main_App_LightMode_V2.1.0.128.PNG)

---

## 🎯 What It Does

CatiaMenuWin32 syncs Python scripts from GitHub and presents them as clickable buttons organised into tabs. Click a button — the script runs. No macro editor, no manual path setup, no copy-pasting.

While it ships with the [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts) repository as its built-in source — scripts that automate CATIA V5 via the [PyCATIA](https://github.com/evereux/pycatia) library — the built-in source can be disabled entirely. You can point the app at any GitHub repository or local folder containing `.py` files, making it a general-purpose Python script launcher for any workflow, tool, or application that exposes a Python API.

## 📂 Script Tabs

Tabs are built dynamically from the folders in `KaiUR/Pycatia_Scripts`. If folders are
added or removed from the repo, tabs update automatically on the next sync:

| Tab | Folder |
|-----|--------|
| Any Document Scripts | `Any_Document_Scripts/` |
| Part Document Scripts | `Part_Document_Scripts/` |
| Process Document Scripts | `Process_Document_Scripts/` |
| Product Document Scripts | `Product_Document_Scripts/` |
| Shape Generation Scripts | `Shape_Generation_Scripts/` |

## 🚀 Features

| Feature | Detail |
|---------|--------|
| **Live GitHub sync** | One Git Trees API request lists each repository; an unchanged repository answers "not modified" and the cached listing is reused. Only changed files are downloaded, and scripts deleted from the repository are removed from the cache |
| **Offline cache** | Scripts load from local cache immediately on startup — works without internet |
| **Dynamic tabs** | Folder additions/removals detected automatically; no recompile needed |
| **Favourites tab** | Star any script; a dedicated ⭐ Favourites tab appears automatically |
| **Search/filter** | Real-time filter bar — find scripts by name or purpose instantly |
| **Script info tooltip** | Hover over the `i` badge on any button to see Purpose, Author, Version, Date, full Description and the latest `Change:` entry parsed from the script header |
| **New & updated badges** | A green dot marks new scripts and a blue dot updated ones (tabs get a dot too); cleared when you run the script or open its details, or via View → Mark All Scripts as Seen |
| **Command palette** | **Ctrl+K** opens a search box over every script — type letters in order, Enter runs, Shift+Enter runs with arguments, Ctrl+Enter shows details. System-wide by default, so it works from inside CATIA |
| **Script details** | Right-click → Script Details shows all header fields, the change history, notes, favourite/hidden controls |
| **Hide scripts** | Right-click → Hide Script; restore via Menu → File → Manage Hidden Scripts |
| **Sort scripts** | Sort by Default, Alphabetical, By Date, or Most Used |
| **Run with arguments** | Right-click → Run with Arguments to pass custom CLI arguments; the run is SHA-verified, logged and stoppable like a normal click |
| **Script parameter form** | Scripts that declare an `Args:` header get a Run with Arguments form — text boxes, checkboxes and drop-downs — with the last values remembered per script |
| **Dependency check** | Packages in a script's `dependencies = [...]` list are checked against the Python interpreter before it runs; missing ones can be installed with pip from the prompt |
| **Drag and drop** | Drop a `.py` file on the window to run it once, or a folder to add it as a local script source |
| **Portable mode** | A `settings.ini` next to the exe keeps all settings, cache and data in the exe's folder — runs from a USB stick; never touches the registry |
| **Stop running script** | ■ Stop toolbar button terminates a running background script and every process it started; starting another script stops the running one |
| **Script notes** | Per-script user notes stored locally in `prefs.ini` |
| **Certificate validation** | Every HTTPS connection validates the server certificate subject and issuer — blocks MITM attacks |
| **SHA verification** | Every script is verified against its GitHub blob SHA before running — detects tampered files |
| **Single instance** | Only one instance runs at a time — launching a second brings the existing window to the front |
| **Hidden-tab suppression** | Tabs whose every script has been hidden are removed automatically; reappear when any script is unhidden |
| **Quick Launch Bar** | Floating button bar sourced from your Favourites — drag anywhere, scroll arrows, hover tooltips, always-on-top with the target app |
| **Target app tracking** | Bar hides when the target app is not open or all its windows are minimised; rises to TOPMOST when it gains focus |
| **Always on Top** | Window stays above CATIA so you can click scripts without alt-tabbing |
| **System Tray** | Minimize to tray; restore with double-click |
| **Start with Windows** | Autorun via registry with optional start-minimized flag |
| **Auto-refresh** | Background sync every N hours (default 6); configurable in Settings |
| **Auto-update** | Optionally download and install new versions automatically — only if the download is signed with the same key as the installed version |
| **Update Dependencies** | Upgrades pip then runs `pip install --upgrade -r requirements.txt` for each configured source that has a `setup/requirements.txt` |
| **Dark / Light / System theme** | Follows Windows theme by default; toggle via Menu → View → Theme |
| **Auto-versioning** | CMake increments `build_number.txt` on every configure; CI appends it to the release tag |

## 📁 Script Sources

CatiaMenuWin32 can load scripts from multiple sources simultaneously — the built-in repository, additional GitHub repositories, and local folders on your machine. Open **Menu → File → Sources...** to manage them.

### Built-in Repository

The `KaiUR/Pycatia_Scripts` repository is always the primary source. It can be disabled in the Sources dialog if you only want to use your own scripts.

### Additional GitHub Repositories

Add any public (or private, with a token) GitHub repository that follows the same folder structure:
- Subfolders of the repo root become tabs
- `.py` files inside subfolders become script buttons
- If two repositories have a subfolder with the same name, their scripts are merged into one tab
- Each repo can have its own branch and optional Personal Access Token
- All connections go through the same certificate validation and SHA verification as the built-in repo
- Each repository is listed from its configured branch with a single API request per sync; scripts removed from the repository are also removed from the local cache

**To add a repository:**
1. Open **Menu → File → Sources...**
2. Click **Add...** under "Additional GitHub Repositories"
3. Enter the full GitHub URL: `https://github.com/owner/repo`
4. Enter the branch name (defaults to `main`)
5. Optionally add a Personal Access Token for private repos or higher rate limits
6. Click OK

### Local Script Folders

Add a folder on your local machine. The folder structure mirrors the GitHub repo structure:
- Subfolders of the selected folder become tabs
- `.py` files inside subfolders become script buttons
- Local scripts are not downloaded or SHA-checked — they run directly from disk
- If a local subfolder has the same name as a tab from GitHub, the scripts are merged

**To add a local folder:**
1. Open **Menu → File → Sources...**
2. Click **Add...** under "Local Script Folders"
3. Browse to your folder
4. Click OK

**Example folder structure:**
```
My_Scripts/
├── Any_Document_Scripts/
│   ├── my_custom_script.py
│   └── another_script.py
└── Part_Document_Scripts/
    └── part_tool.py
```

This would add "My Custom Script" and "Another Script" to the "Any Document Scripts" tab alongside the built-in scripts.

### Tab Scrolling

When more tabs exist than can fit in the window width, left (◄) and right (►) arrow buttons appear at the edges of the tab bar. You can also scroll through tabs by hovering over the tab bar and using the **mouse wheel**.

## 🔒 Security

All communication with GitHub is secured at two levels:

**Certificate validation** — every HTTPS request (API calls, script downloads and update downloads) is checked against the server that actually answers, after any redirect: the host must be `github.com` or a subdomain of `github.com` or `githubusercontent.com` (matched exactly, so look-alikes such as `evilgithub.com` fail); the certificate chain must pass Windows' SSL policy check for that exact host name; and the issuing CA's organisation must exactly match one GitHub uses (Sectigo Limited, Let's Encrypt, DigiCert Inc or GlobalSign nv-sa). Connections that fail any check are aborted before any data is read.

**SHA verification** — before any script is executed, its local file SHA is computed using Git's blob SHA format (`SHA1("blob <size>\0<content>")`) and compared against the SHA returned by the GitHub API. If they don't match, a warning is shown and the script is blocked until re-downloaded and verified.

**Signed updates** — before an auto-update is installed, the downloaded exe's Authenticode signature is verified with `WinVerifyTrust` and its signing key must match the key that signed the running exe. A tampered or differently-signed download is deleted and the releases page opens instead; nothing is installed.

## 🛠️ Built With

- **Language**: C (C11)
- **API**: Win32 API — User32, GDI32, ComCtl32, WinINet, Shell32, Shlwapi, DwmApi, Crypt32
- **Build system**: CMake 3.16+ / Ninja
- **Compiler**: LLVM/Clang (with MSVC Windows SDK)
- **Code signing**: PowerShell + `signtool.exe` — release binaries are Authenticode-signed
- **AI Assistance**: Claude (Anthropic) — used to assist with code generation, debugging, and architecture decisions
- **PyCATIA**: Scripts use the [PyCATIA](https://github.com/evereux/pycatia) library by evereux for CATIA V5 automation

## 📦 Building from Source

### Prerequisites
- [LLVM](https://releases.llvm.org/) — Clang 17+
- [Visual Studio](https://visualstudio.microsoft.com/) 2019+ (for Windows SDK and `rc.exe`)
- [CMake 3.16+](https://cmake.org/)
- [Ninja](https://ninja-build.org/)
- Qt Creator (optional, used as IDE)

### Runtime Requirements (for running scripts)
- **Python 3.9+** — required to execute PyCATIA scripts
- **[PyCATIA](https://github.com/evereux/pycatia)** — install via `pip install pycatia`
- **CATIA V5** — must be running for scripts that interact with it

### Steps

Open a **Developer Command Prompt for VS**, then:

```bash
git clone https://github.com/KaiUR/CatiaMenuWin32
cd CatiaMenuWin32
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl
cmake --build build
```

### Qt Creator

1. Open `CMakeLists.txt` in Qt Creator
2. Select a Clang kit configured with the MSVC toolchain
3. Build → Build All

Local builds automatically detect the latest git tag for the version number and show a `(local)` suffix. The update checker is disabled for local builds.

## ⚙️ Settings (`%APPDATA%\CatiaMenuWin32\settings.ini`)

> **Portable mode:** if a `settings.ini` exists next to `CatiaMenuWin32.exe`, that file is used instead and all data (cache, `prefs.ini`, manifest) lives in the exe's folder. Paths inside that folder are stored relative to it, and Start with Windows is unavailable.

| Setting | Default | Description |
|---------|---------|-------------|
| `Python\Executable` | auto-detect | Full path to `python.exe` |
| `Scripts\CacheDir` | `%APPDATA%\CatiaMenuWin32\scripts` | Local script cache |
| `GitHub\Token` | empty | Optional PAT — increases API rate limit from 60 to 5,000 req/hr; required for private repos |
| `Options\AutoSync` | on | Sync scripts from GitHub on startup |
| `Options\DownloadBeforeRun` | off | Always fetch latest before running |
| `Options\ShowConsole` | off | Show Python console window when running a script |
| `Options\ConsoleKeepOpen` | on | Keep console open after script finishes so you can read errors |
| `Options\DepsKeepOpen` | off | Keep Update Deps console open until manually closed |
| `Options\CheckUpdates` | on | Check GitHub Releases for a newer app version on startup |
| `Options\AutoUpdate` | on | Download and install new versions automatically |
| `Window\AlwaysOnTop` | on | Keep window above other windows |
| `Window\MinimizeToTray` | on | Hide to system tray on minimize/close |
| `Window\StartWithWindows` | on | Add to Windows autorun registry key |
| `Window\StartMinimized` | on | Start hidden/minimized |
| `Window\Theme` | 0 (System) | 0 = follow Windows, 1 = dark, 2 = light |
| `QuickBar\Enabled` | on | Show the Quick Launch Bar |
| `QuickBar\Horizontal` | off | Bar orientation: 0 = vertical, 1 = horizontal |
| `QuickBar\TopmostWithCatia` | on | Rise to TOPMOST when the target app is in the foreground |
| `QuickBar\TargetApp` | `CATIA V5` | Window-title substring to track; empty = always visible, no topmost |
| `QuickBar\TargetExe` | `CNEXT.exe` | Process executable name filter; empty = match any process |
| `QuickBar\X` / `QuickBar\Y` | auto | Saved position of the floating bar |
| `QuickBar\HotkeyEnabled` | on | Register the system-wide Quick Bar show/hide hotkey |
| `QuickBar\HotkeyMods` / `HotkeyKey` | 6 (Ctrl+Shift) / 81 (`Q`) | Quick Bar hotkey modifiers (1 = Alt, 2 = Ctrl, 4 = Shift, 8 = Win) and virtual-key code |
| `Options\ShowBadges` | on | Mark new and updated scripts (and their tabs) with a dot |
| `Options\CheckDependencies` | on | Check a script's `dependencies` list against the Python interpreter before running it |
| `Palette\HotkeyEnabled` | on | Register the system-wide command palette hotkey |
| `Palette\HotkeyMods` / `HotkeyKey` | 2 (Ctrl) / 75 (`K`) | Command palette hotkey modifiers and virtual-key code (32 = Space) |

## 🔑 GitHub Token (optional)

The app uses the GitHub REST API to fetch the script list — one request per GitHub source per sync. Without a token, GitHub allows 60 requests per hour per IP. A token increases this to 5,000 req/hr and is also required for private repositories. If you hit the limit, or if you want to add a private repo source, add a **Personal Access Token**:

1. Go to GitHub → Settings → Developer settings → Personal access tokens → Fine-grained tokens
2. Create a token with **read-only** access to public repositories
3. Paste it in **Menu → Settings → Use token**

The token is stored in `settings.ini` and sent as an `Authorization: token` header. It is never transmitted anywhere except GitHub (`api.github.com`, `raw.githubusercontent.com`, and `github.com` for update downloads).

> **Office / shared network users:** GitHub's unauthenticated API limit is 60 requests per hour per public IP address. If multiple people in your organisation use CatiaMenuWin32 on the same network, you may occasionally see a "Connect to internet to sync" message even with a working internet connection. This is the rate limit being hit, not a connectivity issue. Each user should add a Personal Access Token in **Menu → Settings → Use token** to raise their individual limit to 5000 requests per hour.

## 🖥️ Console Window Options

When **Show Python console window** is enabled:
- A console window opens when a script runs
- **Keep console open after script finishes** — wraps the command as `cmd.exe /k python script.py` so the window stays open after the script exits, letting you read any errors or print output

Without **Show console window**, scripts run silently in the background and the status bar shows the exit code.

## 🔄 Update Dependencies

The **Update Deps** button upgrades pip itself then runs `pip install --upgrade -r setup/requirements.txt` for each configured source that has a `setup/requirements.txt` file, using your configured Python interpreter. Enable **Keep Update Deps console open** in Settings to keep the window visible until you close it.

## 🔢 Versioning System

- `build_number.txt` auto-increments on every `cmake` configure (local and CI)
- CMake reads the latest git tag automatically for local builds — no manual version editing needed
- The workflow reads the tag you push (e.g. `v1.1.0`), builds with that version, then appends the build number to create the final release tag (e.g. `v1.1.0.21`)
- The binary version and release tag always match

## 🚀 How to Release

```bash
git tag v1.2.0
git push origin v1.2.0
```

GitHub Actions builds, increments the build number, and publishes automatically. The final tag becomes `v1.2.0.<buildnum>`.

## 📄 License

MIT License — Copyright © 2026 Kai-Uwe Rathjen

Developed with AI assistance from Claude (Anthropic).

---

**Author**: [Kai-Uwe Rathjen](https://github.com/KaiUR)
