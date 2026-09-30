---
title: User Guide — CatiaMenuWin32
description: How to install and use CatiaMenuWin32 — the Python script launcher and macro manager for CATIA V5. Covers installation, script sources, settings, and favourites.
---

# User Guide

## Contents
- [Installation](#installation)
- [First Launch](#first-launch)
- [The Interface](#the-interface)
- [Running Scripts](#running-scripts)
- [Favourites](#favourites)
- [Search / Filter](#search--filter)
- [Script Details](#script-details)
- [Hiding Scripts](#hiding-scripts)
- [Script Notes](#script-notes)
- [Run with Arguments](#run-with-arguments)
- [Command Palette](#command-palette)
- [Dependency Check](#dependency-check)
- [Drag and Drop](#drag-and-drop)
- [New & Updated Badges](#new--updated-badges)
- [Sorting Scripts](#sorting-scripts)
- [Settings](#settings)
- [Script Sources](#script-sources)
- [Update Dependencies](#update-dependencies)
- [Quick Launch Bar](#quick-launch-bar)
- [Repeat Script on Double-Click](#repeat-script-on-double-click)
- [System Tray](#system-tray)
- [Themes](#themes)
- [GitHub Token](#github-token)
- [Writing Your Own Scripts](#writing-your-own-scripts)
- [In-App Help](#in-app-help)
- [Keyboard Shortcuts](#keyboard-shortcuts)
- [Troubleshooting](#troubleshooting)

---

## Installation

1. Download the latest `CatiaMenuWin32.exe` from the [releases page](https://github.com/KaiUR/CatiaMenuWin32/releases/latest)
2. Place the `.exe` anywhere on your machine — no installer required
3. Double-click to run

**Requirements:**
- Windows 10 or later
- **Python 3.9+** — install from [python.org](https://www.python.org/downloads/)
- **[PyCATIA](https://github.com/evereux/pycatia)** — install via `pip install pycatia` (or use **↓ Deps** in the app)
- **CATIA V5** — must be running for scripts that interact with it

### Portable mode

To run the app from a USB stick or another folder without using `%APPDATA%`, create an empty file named `settings.ini` **next to `CatiaMenuWin32.exe`**. When that file exists the app keeps everything in the exe's folder — settings, `prefs.ini`, the manifest, the `scripts\` cache, the `trees\` listing cache and an optional `venv\`.

- Paths inside that folder (the Python interpreter of a venv created there, the script cache) are saved **relative** to it, so the folder keeps working when it is moved or the drive letter changes.
- **Start with Windows** is unavailable in portable mode and the app never touches the Windows registry — so it cannot remove the autorun entry of an installed copy on the same machine.
- **Help → About** shows *(portable)* after the version number.
- Only one copy of the app runs at a time; close any installed copy before starting the portable one.

> **Tip:** an exported settings file only switches a copy to portable mode if it is saved as `settings.ini` in the same folder as the exe — keep exports under a different name or in another folder.

---

## First Launch

On first launch the app will:

1. Create `%APPDATA%\CatiaMenuWin32\` to store settings and cached scripts
2. Attempt to auto-detect your Python installation
3. Sync scripts from the built-in `KaiUR/Pycatia_Scripts` repository
4. Display the scripts as clickable buttons organised by tab

If the sync fails with "Connect to internet to sync", check your internet connection. If you are on a corporate network see the [GitHub Token](#github-token) section.

---

## The Interface

```
┌──────────────────────────────────────────────────────────────────┐
│  CATIA Macro Menu                                      -  [ ]  x │
├──────────────────────────────────────────────────────────────────┤
│  [Menu] [Refresh] [Settings] [Deps] [Stop] [Log]    v2.2.0.xxx   │
├──────────────────────────────────────────────────────────────────┤
│  Filter scripts...                                               │
├──────────────────────────────────────────────────────────────────┤
│ <  * Favourites  |  Any Document Scripts  |  Part Doc Scripts  > │
├──────────────────────────────────────────────────────────────────┤
│  >  Rename Hybrid Shapes                                    [i]  │
│     Renames multiple hybrid shapes in one go.                    │
│  >  Batch Isolate Geometric Set                             [i]  │
│     Isolate every element in a geometric set as a datum.         │
│  >  Check Duplicate Names In Geometric Set                  [i]  │
│     Scan a geometric set for elements that share a name.         │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│  Sync complete. +5/-0 folders, +71/-0 scripts, 0 updated.        │
└──────────────────────────────────────────────────────────────────┘
```

**Dark Mode**

![CatiaMenuWin32 Dark Mode](images/Main_App_DarkMode_V2.1.0.128.PNG)

**Light Mode**

![CatiaMenuWin32 Light Mode](images/Main_App_LightMode_V2.1.0.128.PNG)

**Toolbar buttons:**
- **☰ Menu** — access all app functions via dropdown menus
- **↺ Refresh** — re-sync scripts from all sources
- **⚙ Settings** — open the settings dialog
- **↓ Deps** — install/update Python dependencies
- **■ Stop** — terminate the currently running background script (grayed out when idle; red when active)
- **≡ Log** — open the script output log window to view stdout/stderr from background script runs

**Tab bar:**
- Each tab corresponds to a script folder
- Click a tab to switch between script categories
- When there are more tabs than fit, ◄ ► arrows appear — click or use the **mouse wheel** to scroll
- A small blue dot in a tab's top-right corner means it contains [new or updated scripts](#new--updated-badges)

**Script buttons:**
- Click the main area to run the script
- Click the **i** badge on the right to see script information (purpose, author, version, description, latest change)
- A green dot marks a new script, a blue dot an updated one — see [New & Updated Badges](#new--updated-badges)

---

## Running Scripts

Click any script button to run it. The app will:

1. Verify the script's SHA hash against GitHub to confirm it hasn't been tampered with
2. [Check the packages it depends on](#dependency-check)
3. Launch Python with the script path
4. Show "Script launched in console" or the exit code in the status bar

You can also [drop a `.py` file](#drag-and-drop) on the window to run it once.

When a script is running in background mode (no console), the button highlights in **green** — border, left accent bar, and label text — for the duration of the run. The highlight clears automatically when the script exits or is stopped.

**Console options** (configurable in Settings):
- **Show console** — opens a visible Python console window when the script runs
- **Keep console open** — keeps the console window open after the script finishes so you can read output and errors (`cmd /k` mode)

Without **Show console**, scripts run silently in the background.

### Stopping a Running Script

Click the **■ Stop** toolbar button (or press `Escape`) to immediately terminate the running script. The button is grayed out when no script is running and turns red when one is active.

Stop ends the script **and every process it started** — for example a helper tool or a second Python process launched with `subprocess`. Processes a script deliberately leaves running when it finishes normally are not affected.

Only one background script runs at a time: **starting another script stops the one that is running**, so ■ Stop always refers to the script that is actually running. The stopped run's log section ends with `--- Stopped: another script was started. ---`.

There is no time limit — a script can run for as long as it needs, and the button stays green until it finishes or you stop it.

> **Note:** Only background (no-console) runs can be stopped this way. If **Show console** is enabled, close the console window directly, or press `Ctrl+C` inside it.

### Script Output Log

When scripts run in background mode (no console), all stdout and stderr output is captured and shown in the **Script Output Log** window.

- Click **≡ Log** in the toolbar to open the log window at any time — even before a script runs or after it finishes.
- Each run is framed by a **timestamped header** and a **completion footer**:
  ```
  === ScriptName  (HH:MM:SS) ===
  ... script output ...
  --- Finished successfully. ---
  ```
  The footer reads `--- Stopped by user. ---` if the script was terminated via the **■ Stop** button, `--- Stopped: another script was started. ---` if starting another script ended it, or `--- Exited with code N. ---` for a non-zero exit.
- **Syntax highlighting** — output is colour-coded automatically:
  - Header lines (`===`) → accent blue
  - `--- Finished successfully. ---` → green
  - `--- Stopped by user. ---` → amber
  - `--- Exited with code N. ---` → red
  - Lines containing `Error`, `Exception`, `Traceback`, `Fatal` → red
  - Lines containing `Warning` → amber
- Output is **buffered internally** — nothing is lost even if the log window is closed during a run. Open it afterwards to see the full history.
- The log accumulates output across all runs for the session.
- The window is non-blocking and follows the current dark/light theme.

> **Note:** Output is only captured in background mode. When **Show console** is enabled, output appears in the console window instead.

### Script Source Tinting

When **Tint local and extra-repo script buttons differently** is enabled in **Settings → Window** (default: On), script buttons are given a subtle background tint based on their source:

| Tint | Source |
|------|--------|
| Warm rose | Script is from a local folder |
| Cool green | Script is from an extra GitHub repository |
| No tint | Script is from the built-in KaiUR/Pycatia_Scripts repository |

The same tinting applies to Quick Launch Bar buttons. Disable the option in Settings → Window if you prefer a uniform appearance.

When the app is **offline** and **Show cached scripts when offline** is enabled, the tinting changes to indicate stale data:

| Tint | Source |
|------|--------|
| Red | Script is from the built-in KaiUR/Pycatia_Scripts repository |
| Dark orange-red | Script is from an extra GitHub repository |
| Unchanged | Script is from a local folder (local scripts are always current) |

Local scripts are never tinted red because they are read directly from disk and are not affected by connectivity.

---

## ⭐ Favourites

Favourites give you quick access to the scripts you use most often.

**To add a favourite:**
- Right-click any script button → **Add to Favourites**
- Or open **Script Details...** → tick the **Favourite** checkbox → click OK

A dedicated **⭐ Favourites** tab appears at the far left of the tab bar as soon as you have at least one favourite. It disappears automatically when all favourites are removed.

**To remove a favourite:**
- Right-click the script → **Remove from Favourites**
- Or open **Script Details...** → untick the **Favourite** checkbox → click OK

Favourites are also the source for the [Quick Launch Bar](#quick-launch-bar) — every script you favourite automatically appears as a button on the floating bar.

Favourites are stored in `%APPDATA%\CatiaMenuWin32\prefs.ini` and persist across restarts and syncs.

---

## 🔍 Search / Filter

A filter bar sits below the toolbar. Type any text to instantly filter the scripts in the current tab by name or purpose. Clear the box to show all scripts again.

---

## 📋 Script Details

Right-click any script button and select **Script Details...** to open a full details dialog showing:

- Script name, purpose, author, version, date
- Code environment and CATIA release
- Full description and requirements
- **Changes** — the script's change history from its `Change:` block, oldest first
- Local cache path
- Your personal note
- Favourite and Hidden toggles

Changes to the note, favourite, and hidden state are saved when you click OK. Opening the details also clears the script's [new/updated badge](#new--updated-badges).

---

## 🙈 Hiding Scripts

Hiding a script removes it from view without deleting it from the cache.

**To hide a script:**
- Right-click any script → **Hide Script**
- Or open **Script Details...** → tick the **Hidden** checkbox → click OK

Hidden scripts are not deleted — they remain in the cache and will not reappear after a sync. If every script in a tab is hidden, the tab itself disappears from the tab bar automatically.

**To unhide scripts:**
1. Go to **☰ Menu → File → Manage Hidden Scripts**
2. Select one or more scripts from the list
3. Click **Unhide** to restore the selected scripts, or **Unhide All** to restore everything

The tab reappears automatically as soon as any script in it is unhidden.

---

## 📝 Script Notes

Right-click any script → **Add Note...** (or **Edit Note...**) to attach a personal note to a script. Notes are visible in the Script Details dialog and stored in `prefs.ini`.

---

## ▶️ Run with Arguments

Right-click any script → **Run with Arguments...** to pass custom command line arguments when running the script.

Type the arguments exactly as you would after `python script.py` on a command line — for example `--tolerance 0.01 "My Part"`. Quotes group words into one argument. The script then runs just like a normal click: it is SHA-verified, its dependencies are checked, its output goes to the [Script Output Log](#script-output-log), the button turns green, and **■ Stop** ends it.

**Parameter form.** If the script declares its parameters in an `Args:` header block (see [Writing Your Own Scripts](#writing-your-own-scripts)), the dialog shows one labelled field per parameter instead of a single text box:

| Parameter type | Field | Passed to the script as |
|----------------|-------|-------------------------|
| `str` (default) | Text box | `--name value` (omitted when empty) |
| `int` / `float` | Text box — checked when you click **Run**; use `.` as the decimal point | `--name 3` / `--name 0.01` |
| `bool` | Checkbox | `--name` when ticked, nothing when not |
| `[a\|b\|c]` | Drop-down list | `--name b` |

Values containing spaces or quotes are quoted automatically. The free-text box stays below the form as **Additional arguments**, appended unchanged. Each field starts with the value you used last time for that script (stored in `prefs.ini`), or the header's default the first time; scripts without an `Args:` block also remember their last arguments.

---

## Dependency Check

Before a script runs, the packages it lists in its header's `dependencies = [...]` block are checked against the Python interpreter that will run it — including version limits such as `pycatia>=0.8` or `pywin32<310`.

If everything is installed the script simply runs. If something is missing or the wrong version, a prompt lists it and lets you choose:

| Button | Result |
|--------|--------|
| **Yes** | Installs the listed packages with `pip install` in a console window, checks again, then runs the script. The console only stays open if pip fails, so you can read the error. If a package is still missing afterwards, the script does not run. |
| **No** | Runs the script anyway |
| **Cancel** | Does not run the script |

- A check that passed is remembered (for that interpreter and list) until you click **↓ Deps** or save the Settings dialog, so it only costs time the first time a script runs.
- If the check itself cannot run (for example Python fails to start), the script is not blocked.
- Scripts without a `dependencies` block are never checked. Turn the check off in **Settings → Console → Check a script's dependencies before running it**.

---

## Drag and Drop

- **Drop a `.py` (or `.pyw`) file** on the main window to run it once — for example to try a script before adding it to a source. It runs like a script button: its dependencies are checked, its output goes to the [Script Output Log](#script-output-log) and **■ Stop** ends it. There is no download or SHA check, because you chose the file yourself. If several files are dropped, only the first one runs.
- **Drop a folder** to add it as a [local script folder](#local-script-folders): the app asks for confirmation, saves it to your sources and re-syncs, so its subfolders appear as tabs.

---

## Command Palette

Press **Ctrl+K** (or **☰ Menu → Run → Command Palette...**) to find and run any script from the keyboard, without switching tabs.

- Type part of a script's name — the letters only need to appear **in order**, so `expcsv` finds *Export Properties To CSV*. Matches at word starts and runs of consecutive letters rank higher; if nothing matches the name, the purpose line is searched too.
- With an empty search box every script is listed, most-used first; scripts you run often also win ties.
- Each row shows the script name, its tab and its purpose, plus the [new/updated dot](#new--updated-badges).
- Hidden scripts are not listed, and each favourite appears once (under its own tab).

| Key | Action |
|-----|--------|
| `↑` `↓` `Page Up` `Page Down` | Move the selection |
| `Enter` (or double-click) | Run the selected script |
| `Shift+Enter` | [Run with Arguments](#run-with-arguments) |
| `Ctrl+Enter` | Open [Script Details](#script-details) |
| `Esc` (or click elsewhere) | Close the palette |

**Ctrl+K is registered system-wide by default**, so it opens the palette from any application — including CATIA — even when the main window is hidden in the tray. While it is registered, Ctrl+K is captured in every other program too (Word and Outlook use it to insert a link, browsers to focus search, VS Code to start a chord); untick it or choose another combination in [Settings → Command Palette](#command-palette-tab) if you need it elsewhere. Inside this app Ctrl+K always works.

---

## New & Updated Badges

After a sync, scripts that changed since you last looked are marked with a dot at the right end of the button:

| Dot | Meaning |
|-----|---------|
| 🟢 Green | **New** — the script appeared since you last looked |
| 🔵 Blue | **Updated** — the script's content changed since you last looked |

A tab that contains any badged script shows a small blue dot in its top-right corner. The script's tooltip shows the badge and its **latest change** from the `Change:` block, and [Script Details](#script-details) lists the full change history.

A badge clears when you **run** the script or **open its details**. **☰ Menu → View → Mark All Scripts as Seen** clears every badge at once, and **☰ Menu → View → Show New/Updated Badges** (or **Settings → Window**) turns them off entirely.

- The first time the app sees your scripts — a fresh install, or the first start after updating to v3.0.0 — everything counts as already seen, so you are not greeted with a dot on every script.
- Badges compare each script's Git SHA with the one recorded when you last saw it (stored in `prefs.ini`), so they work for the built-in repository and extra GitHub repositories. Local-folder scripts are never badged.

---

## 📂 Opening Scripts

The right-click context menu provides three options for opening script files directly:

| Option | Available for | Action |
|--------|--------------|--------|
| **Open Script Location** | All scripts | Opens the folder containing the script in Windows Explorer |
| **Open with Default App** | All scripts | Opens the script file with the Windows default application for its file type |
| **Open in Editor** | Local folder scripts only | Opens the script in the registered editor for the file type (e.g. VS Code, Notepad++, IDLE) |

> **Note:** For GitHub-synced scripts, edits made to the cached file will be overwritten on the next sync. Use **Open in Editor** on local scripts to safely edit your own scripts.

---

## 🔢 Sorting Scripts

**☰ Menu → View → Sort Scripts** offers four sort modes:

| Mode | Description |
|------|-------------|
| Default Order | Order from GitHub API or disk |
| Alphabetical | A–Z by script name |
| By Date | Most recent scripts first (from script header Date field) |
| Most Used | Scripts you run most often appear first |

The sort mode is saved in Settings and applied to all tabs.

---

## Settings

Open via **☰ Menu → File → Settings...** or the **⚙ Settings** toolbar button.

The Settings dialog is organised into six tabs:

### General tab

| Option | Description |
|--------|-------------|
| **Python Interpreter** | Full path to `python.exe`. Click **Browse...** to locate it. Leave blank to auto-detect from PATH. To use a virtual environment, browse to `python.exe` inside the venv's `Scripts` folder (e.g. `C:\My_Venv\Scripts\python.exe`) — the venv does not need to be activated. After changing this, click **↓ Deps** to install packages into the new environment. Click **Create venv** to automatically create a virtual environment at `%APPDATA%\CatiaMenuWin32\venv` — the path field is updated automatically on success. |
| **Script Cache Folder** | Where downloaded scripts are stored locally. Defaults to `%APPDATA%\CatiaMenuWin32\scripts`. |
| **GitHub Token** | Optional Personal Access Token. Increases the API rate limit from 60 to 5,000 req/hr and is required for private repositories. Tick **Use token** and paste the token. See [GitHub Token](#github-token). |

### Sync tab

| Option | Default | Description |
|--------|---------|-------------|
| Sync scripts automatically on startup | On | Downloads latest scripts when the app starts |
| Always download latest before running | Off | Re-downloads the script every time before running |
| Check for app updates on startup | On | Notifies you when a newer version is available |
| Auto-install updates | On | Downloads and installs new versions automatically; also applies when triggering **Help → Check for Updates…** manually. A download is installed only if it is signed by the same publisher as the installed version — otherwise it is deleted and the releases page opens |
| Auto-refresh every N hours | 6 | Background sync interval in hours; 0 = disabled |
| Show cached scripts when offline | Off | Display previously synced scripts when there is no internet connection; an amber status bar warning is shown. When off, no buttons appear if the sync cannot reach GitHub |

### Console tab

| Option | Default | Description |
|--------|---------|-------------|
| Show Python console window | Off | Opens a visible console when running scripts |
| Keep console open after script finishes | On | Window stays open so you can read output/errors (`cmd /k` mode) |
| Keep Update Deps console open | Off | Keeps the dependency install window open until you close it |
| Repeat script on double-click (main window) | On | Enable [repeat mode](#repeat-script-on-double-click) for scripts in the main window |
| Check a script's dependencies before running it | On | Check the packages in the script's `dependencies` list before each run — see [Dependency Check](#dependency-check) |

### Window tab

| Option | Default | Description |
|--------|---------|-------------|
| Always on Top | On | Keep the main window above other applications |
| Minimize to Tray | On | Hide to system tray instead of taskbar when minimised |
| Start with Windows | On | Launch automatically at login (unavailable in [portable mode](#portable-mode)) |
| Start Minimized | On | Start hidden in the tray |
| Theme | System | Dark, Light, or follow Windows setting |
| Sort Scripts | Default Order | Default, Alphabetical, By Date, or Most Used |
| Tint local and extra-repo script buttons differently | On | Warm tint for local-folder scripts, cool tint for extra-repository scripts |
| Mark new and updated scripts with a dot | On | Show [new/updated badges](#new--updated-badges) on script buttons and tabs |

### Quick Bar tab

| Option | Default | Description |
|--------|---------|-------------|
| Enable Quick Launch Bar | On | Show the floating icon toolbar |
| Orientation | Vertical | Vertical (column) or Horizontal (row) |
| Stay on Top with Target App | On | Auto-elevate bar when target app is in the foreground |
| Target App | `CATIA V5` | Window-title substring to track; leave empty for always-visible bar |
| Target Exe | `CNEXT.exe` | Process executable filename to match alongside **Target App**. Click **Browse…** to pick the `.exe` from a file dialog instead of typing it. Leave empty to match any process. |
| Repeat script on double-click (Quick Bar) | On | Enable [repeat mode](#repeat-script-on-double-click) for Quick Bar buttons |
| Enable global hotkey | On | Register the system-wide [show/hide hotkey](#showhide-hotkey) |
| Hotkey modifiers | Ctrl + Shift | Ctrl, Alt, Shift and/or Win — at least one is required |
| Hotkey key | `Q` | A–Z, 0–9 or F1–F12 |

### Command Palette tab

| Option | Default | Description |
|--------|---------|-------------|
| Enable system-wide hotkey | On | Register a hotkey that opens the [command palette](#command-palette) from any application |
| Hotkey modifiers | Ctrl | Ctrl, Alt, Shift and/or Win — at least one is required |
| Hotkey key | `K` | Space, A–Z, 0–9 or F1–F12 |

While registered, the combination is captured in **every** application — Ctrl+K also inserts a link in Word and Outlook, focuses the search box in browsers, and starts chords in VS Code. Untick the hotkey or choose another combination if you need Ctrl+K elsewhere; **Ctrl+K** inside this app keeps working either way. The combination must differ from the Quick Bar hotkey, and Ctrl+Alt combinations should be avoided on keyboards with an AltGr key.

### Reset to Defaults
The **Reset to Defaults** button at the bottom left resets all settings to their original values. Your script sources (extra repos and local folders) are not affected.

### Export / Import Settings
Two buttons in the bottom button row let you back up and transfer your configuration selectively.

Both open a selection dialog divided into three sections:

**General settings** — three independent checkboxes:
- **Python path** — machine-specific; uncheck when sharing with other users.
- **Cache folder** — machine-specific; uncheck when sharing.
- **Options, theme, window & Quick Bar** — portable preference settings, safe to share.

**Extra Repositories** and **Local Folders** — each item has its own checkbox so you pick exactly which sources to transfer.

**Tokens** — one checkbox per available token (GitHub account token and individual repo tokens). Uncheck any tokens you do not want to include.

**Export...**
1. The selection dialog appears — all items checked by default.
2. Uncheck anything you do not want to export. Uncheck individual tokens to keep sensitive credentials out of the file.
3. Choose a destination `.ini` file.

**Import...**
1. Choose the source `.ini` file.
2. The selection dialog appears showing what is available in that file — all items checked by default.
3. Uncheck anything you do not want to add. Selected repos and folders are **appended** to your current sources — nothing is removed or replaced.
4. If a repo URL already exists in your config and the imported entry carries a token, you are prompted to keep the existing or imported token. Exact duplicate paths with no token conflict are skipped silently.
5. Settings take effect immediately and the dialog closes. Your `prefs.ini` (favourites, notes, run counts) is never affected.

> **Tip:** To share only your portable preferences, uncheck **Python path** and **Cache folder** before exporting. The recipient gets your options, theme, and Quick Bar settings without machine-specific paths.

---

## Script Sources

Open via **☰ Menu → File → Sources...**

CatiaMenuWin32 can load scripts from three types of sources simultaneously:

### Built-in Repository
The `KaiUR/Pycatia_Scripts` repository is the default primary source. If you want to use the app with your own scripts only — or as a general Python script launcher unrelated to CATIA — you can disable it:

1. Open **☰ Menu → File → Sources...**
2. At the top of the dialog, uncheck **Enable built-in repository (KaiUR/Pycatia_Scripts)**
3. Click **OK**
4. Click **↺ Refresh** — the built-in scripts are removed from all tabs immediately

The built-in repository is not deleted from your cache, just hidden. Re-check the box and refresh to restore it at any time.

### Additional GitHub Repositories
Add any GitHub repository that uses the same folder structure (subfolders contain `.py` files):

1. Click **Add...** under "Additional GitHub Repositories"
2. Enter the full URL: `https://github.com/owner/repo`
3. Enter the branch name (defaults to `main`)
4. Optionally add a token for private repos or higher rate limits
5. Click **OK**

If two repositories have a folder with the same name, their scripts are merged into one tab.

To **enable or disable** a repo without removing it, select it and click **Enable/Disable**.

To **remove** a repo, select it and click **Remove** — you will be asked to confirm and given the option to delete its cached files.

### Local Script Folders
Add a folder on your machine that contains subfolders with `.py` files:

```
My_Scripts/
├── Any_Document_Scripts/
│   └── my_script.py
└── Part_Document_Scripts/
    └── another_script.py
```

1. Click **Add...** under "Local Script Folders"
2. Browse to your folder and click OK

Local scripts run directly from disk — no downloading or SHA checking. A `setup/` subfolder is never shown as a tab — it is used for dependencies only.

### Update Dependencies for Sources
If a source has a `setup/requirements.txt` file, clicking **↓ Deps** will run `pip install --upgrade pip && pip install --upgrade -r requirements.txt` for each source separately in order:
1. Main repo requirements
2. Each extra GitHub repo's requirements
3. Each local folder's requirements

---

## Update Dependencies

Click **↓ Deps** (or **☰ Menu → Run → Update Dependencies**) to install Python packages required by the scripts.

The app runs `pip install --upgrade pip && pip install --upgrade -r requirements.txt` for each configured source that has a `setup/requirements.txt` file. Each source runs in its own console window sequentially.

Enable **Keep Update Deps console open** in Settings to keep each window visible until you close it manually.

---

## Quick Launch Bar

The Quick Launch Bar is a small floating button bar that gives you one-click access to your favourite scripts without switching to the main window.

### Enabling the bar

Go to **☰ Menu → View → Quick Bar → Enable Quick Bar**, or right-click the bar itself and tick **Enable Quick Bar**. You can also press the [show/hide hotkey](#showhide-hotkey) — **Ctrl+Shift+Q** by default — from anywhere.

### Show/hide hotkey

A system-wide hotkey toggles the bar without switching away from whatever you are working in. The default is **Ctrl+Shift+Q**, and it works while CATIA (or any other application) has focus.

The hotkey toggles the same setting as the menu item, so the tick in the View menu and the right-click menu always reflects what the hotkey did, and the state is remembered the next time you start the app. The current combination is shown next to **Enable Quick Bar** in both menus.

To change it, go to **Settings → Quick Bar → Show / Hide Hotkey**:

- **Enable global hotkey** — untick to release the hotkey entirely
- **Ctrl / Alt / Shift / Win** — at least one modifier is required. A bare key would be captured in *every* application, not just this one, so the dialog will not accept a combination without a modifier
- **Key** — any letter, digit, or F1–F12

If another application has already claimed the combination, Windows refuses to register it and the status bar tells you so. Pick a different combination in that case.

**Avoid Ctrl+Alt combinations on keyboards with an AltGr key** (German, French, Nordic and others). Windows treats Ctrl+Alt as AltGr, so a Ctrl+Alt hotkey blocks the character on that key in every application — Ctrl+Alt+Q is AltGr+Q, the `@` key on German keyboards. This is why the default changed from **Ctrl+Alt+Q** to **Ctrl+Shift+Q** in v3.0.0. If your hotkey was still the old default, the app moves it to Ctrl+Shift+Q once, on the first start after updating, and tells you so; any other combination you chose is left alone.

> **Note:** If you have set a **Target App** and it is not currently on screen, the bar stays hidden until a visible target window appears — the hotkey still records your choice, and the status bar says so.

### Buttons

Each button represents one script from your ⭐ Favourites tab. The button face shows the first two uppercase letters of the script name. Hover over a button to see the full script name and its Purpose line in a tooltip. Click to run the script.

If you have more favourites than fit in the bar, **▲ ▼** (vertical) or **◄ ►** (horizontal) scroll arrows appear at the edges. You can also scroll with the **mouse wheel**.

### Moving the bar

Drag any empty area of the bar (between buttons) to reposition it anywhere on screen. The position is saved automatically to `settings.ini`.

### Orientation

Switch between vertical and horizontal layouts via **☰ Menu → View → Quick Bar** or the right-click context menu. The bar resizes automatically.

### Always on top with the target app

When **On Top with Target App** is enabled, the bar rises to always-on-top whenever the configured target application gains focus, and drops back to normal z-order when any other window comes to the front. This lets the bar float above your target app without covering unrelated windows.

The bar also tracks the target application's state:
- **Target app visible** — bar shown normally
- **All target windows minimised** — bar hides automatically and reappears when any window is restored
- **Target app not running** — bar hides automatically; it reappears when the app is launched and a visible window is detected

### Setting the target app

By default the target is **CATIA V5**. To use the bar with a different application:

1. Right-click the bar and select **Set Target App…** (or **☰ Menu → View → Quick Bar → Set Target App…**)
2. Enter any substring that appears in the target application's window title (e.g. `Fusion 360`, `Blender`, `SolidWorks`)
3. Optionally enter the **Target Exe** — the process executable filename (e.g. `CNEXT.exe`) — to prevent the bar from responding to other windows whose titles contain the same substring. Click **Browse…** to navigate to the executable with a file picker instead of typing the name manually
4. Click **OK**

To disable target tracking entirely — keeping the bar always visible with no topmost behaviour — clear both fields and click **OK**.

### Right-click menu options

| Option | Description |
|--------|-------------|
| Enable Quick Bar | Toggle the bar on or off — the configured [hotkey](#showhide-hotkey) is shown alongside |
| Horizontal / Vertical | Switch orientation |
| On Top with Target App | Toggle topmost-with-target behaviour (greyed out when no target is set) |
| Set Target App… | Enter the window-title substring to track |
| Reset Position | Move the bar back to its default position (right edge of screen) |
| Repeat on Double-Click | Toggle [repeat mode](#repeat-script-on-double-click) for Quick Bar buttons |

---

## Repeat Script on Double-Click

**Repeat mode** lets a script re-run automatically each time it finishes, without any further input.

### Starting repeat mode

- **Double-click** a script button in the main window, or **double-click** a Quick Bar button.
- The button turns amber — the border, left accent bar, label text, and the loop symbol **↻** are shown in yellow/amber — so you always know which script is looping.
- The status bar shows: *Repeat: \<script name\>  •  Esc or click to stop*

### Stopping repeat mode

| Action | Result |
|--------|--------|
| Press **Escape** | Cancels repeat and stops the currently running background script |
| **Single-click the same script** | Cancels repeat; no extra run is triggered |
| **Single-click a different script** | Cancels repeat and runs the new script once |
| Click **■ Stop** | Cancels repeat and stops the current run |

### Toggling the feature

- **Main window:** **☰ Menu → Run → Repeat Script on Double-Click** (checkmark indicates state), or **Settings → Console → Repeat script on double-click (main window)**
- **Quick Bar:** right-click the bar → **Repeat on Double-Click**, or **☰ Menu → View → Quick Bar → Repeat on Double-Click**, or **Settings → Quick Bar → Repeat script on double-click (Quick Bar)**

Both toggles are independent and default to **On**.

> **Note:** Repeat mode is not available when **Show Python console window** is enabled in Settings, because console-mode scripts run attached to a terminal and their completion is not tracked by the app.

---

## System Tray

Enable **Minimize to Tray** in **☰ Menu → Window** to hide the window to the system tray instead of the taskbar when minimised or closed.

- **Double-click** the tray icon to restore the window
- **Right-click** the tray icon for a quick menu

Enable **Start with Windows** to launch the app automatically at login. Combine with **Start Minimized** to have it start silently in the tray.

---

## Themes

Switch between dark, light, and system-default themes via **☰ Menu → View → Theme**:

- **System (default)** — follows your Windows theme setting automatically
- **Dark** — always dark regardless of Windows setting
- **Light** — always light regardless of Windows setting

---

## GitHub Token

The app uses the GitHub REST API to fetch script lists. Without a token, GitHub allows **60 requests per hour per IP address**.

Each sync lists every GitHub source with **one request** (the Git Trees API), and asks GitHub whether the listing changed since last time — an unchanged repository answers "not modified" and the listing cached in `%APPDATA%\CatiaMenuWin32\trees` is reused. Script files themselves are downloaded from `raw.githubusercontent.com`, which does not count against the API limit. Without a token, a "not modified" reply still counts as one request.

A token increases this to **5,000 requests per hour** and is required for private repositories.

**To create a token:**
1. Go to GitHub → Settings → Developer settings → Personal access tokens → Fine-grained tokens
2. Click **Generate new token**
3. Name it `CatiaMenuWin32`
4. Set expiry as preferred
5. Under Repository access select **Public Repositories (read-only)**
6. Click **Generate token** and copy it

**To add it to the app:**
1. Open **☰ Menu → File → Settings...**
2. Tick **Use token**
3. Paste the token
4. Click **OK**

> **Office / shared network users:** If multiple people share the same public IP address, all their requests count against the same 60/hour limit. Each user should set their own token to avoid seeing "Connect to internet to sync" errors.

---

## Writing Your Own Scripts

CatiaMenuWin32 reads metadata from a structured header block at the top of each `.py` file. This header powers the tooltip shown when you hover over the **i** badge on a script button.

### Header Format

```python
'''
    -----------------------------------------------------------------------------------------------------------------------
    Script name:    My_Script_Name.py
    Version:        1.0
    Code:           Python3.10.4, Pycatia 0.8.3
    Release:        V5R32
    Purpose:        One line summary shown under the script name in the button.
    Author:         Your Name
    Date:           DD.MM.YY
    Description:    Full description of what the script does. This is shown in the
                    tooltip popup. Continuation lines must be indented.
    Args:           tolerance:float=0.01 "Merge tolerance in mm"
                    mode:[fast|full]=fast "Search mode"
                    overwrite:bool=false "Overwrite existing results"
    dependencies = [
                    "pycatia",
                    ]
    requirements:   Python >= 3.10
                    pycatia
                    Catia V5 running with an open document.
    -----------------------------------------------------------------------------------------------------------------------

    Change:         20.05.26 1.1: First change, one entry per version.
                    03.06.26 1.2: Newest change last. Long entries may wrap
                    onto indented continuation lines.

    -----------------------------------------------------------------------------------------------------------------------
'''
```

### Rules
- The header must be inside a triple-quoted string `'''...'''` or `"""..."""` at the top of the file
- The dashed separator lines (`-----...`) mark the start and end of the header block
- Keys are matched case-insensitively: `Script name:`, `Purpose:`, `Author:`, `Date:`, `Version:`, `Description:`, `Args:`, `requirements:`, `Change:` (and the `dependencies = [...]` list)
- **Purpose** — shown as the subtitle line on the script button (keep it short, one line)
- **Description** — shown in the tooltip; continuation lines must be indented with spaces or tabs
- **Args** — optional. One parameter per line: `name:type=default "help text"`. Types are `str` (the default), `int`, `float`, `bool`, or a list of choices `[a|b|c]`; the default and the quoted help text are optional. **Run with Arguments** shows a form with one field per parameter and passes them as `--name value` (a ticked `bool` passes `--name`), so read them with `argparse`
- **dependencies** — optional. The pip requirement strings listed in quotes (`"pycatia"`, `"pywin32>=306"`) are checked against the Python interpreter before the script runs; missing packages can be installed from the prompt
- **Change** — the block after the header's closing separator, up to the next separator. Each entry starts with a `DD.MM.YY` date (usually followed by the version); a line that does not start with a date continues the previous entry. List entries oldest first: the **last** one is shown in the tooltip as the script's latest change, and all of them appear under **Changes** in Script Details
- Header parsing stops at the second dashed separator line (then only the `Change:` block is read, until the third separator), or at `import`, `from`, `def`, or `class`
- If no metadata is found the script still appears as a button — just without tooltip details

### Folder Structure

Scripts must be organised in subfolders — the subfolder name becomes the tab name:

```
Your_Repo/
├── Any_Document_Scripts/
│   └── My_Script.py
├── Part_Document_Scripts/
│   └── Another_Script.py
└── setup/
    └── requirements.txt
```

Folder names use snake_case — the app converts them to title case automatically:
`Any_Document_Scripts` → `Any Document Scripts`

### PyCATIA

Scripts in `KaiUR/Pycatia_Scripts` use the [PyCATIA](https://github.com/evereux/pycatia) library by evereux for automating CATIA V5 via COM. To write your own scripts:

1. Install PyCATIA: `pip install pycatia`
2. See the [PyCATIA documentation](https://pycatia.readthedocs.io/) for the full API reference
3. CATIA V5 must be running before scripts that interact with it are executed

### Setup Folder

If your repository or local folder contains a `setup/` subfolder with a `requirements.txt`, clicking **↓ Deps** will automatically install those dependencies:

```
Your_Repo/
└── setup/
    └── requirements.txt
```

The `setup/` folder is never shown as a tab.

### Persistent Data

> **Never ask users to edit a script to change settings or parameters.** CatiaMenuWin32 verifies the SHA hash of every downloaded script against GitHub before running it. A script that has been locally modified will fail the integrity check and the app will refuse to run it. All user-configurable data must be stored outside the script file.

Store settings in a per-script folder under `%APPDATA%\pycatia_scripts\`:

```
%APPDATA%\pycatia_scripts\<Your_Script_Name>\user_settings.json
```

Use the script filename (without `.py`) as the folder name. This keeps each script's data isolated and easy to locate or clean up.

**Implementation pattern:**

```python
import os
import json

SETTINGS_DIR  = os.path.join(os.environ['APPDATA'], 'pycatia_scripts', 'Your_Script_Name')
SETTINGS_FILE = os.path.join(SETTINGS_DIR, 'user_settings.json')

# --- Load (in your dialog __init__) ---
hardcoded_defaults = {"my_param": "10.0", "another_param": "5.0"}
settings = hardcoded_defaults.copy()
if os.path.exists(SETTINGS_FILE):
    try:
        with open(SETTINGS_FILE, 'r') as f:
            settings.update(json.load(f))
    except:
        pass  # Fall back to hardcoded defaults on corrupt or missing file

# --- Save (after user clicks OK) ---
if not os.path.exists(SETTINGS_DIR):
    os.makedirs(SETTINGS_DIR)
with open(SETTINGS_FILE, 'w') as f:
    json.dump({"my_param": dlg.my_field.GetValue(), "another_param": dlg.other_field.GetValue()}, f, indent=4)
```

**Rules:**
- Always define `hardcoded_defaults` — these are factory defaults when no saved file exists
- Wrap the file read in `try/except` and fall back to defaults if the file is corrupt
- Create the directory before writing: use `os.makedirs()` after checking it does not exist
- Save only on successful completion (OK), not on cancel or error
- Include a **Clear Saved** button in dialogs that saves settings, so users can return to factory defaults without touching any files

---

## ❓ In-App Help

Press **F1** or go to **☰ Menu → Help → Help Contents** to open the built-in help window.

The help window has a topic list on the left and formatted content on the right. A coloured header strip above the content shows the current topic name at a glance. Click any topic to navigate to it. The window is resizable and stays open while you work.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `F1` | Open Help |
| `F5` | Refresh + Sync |
| `F9` | Run last script |
| `Ctrl+K` | Open the [command palette](#command-palette) — works from any application; configurable |
| `Ctrl+Shift+Q` | Show / hide the [Quick Launch Bar](#showhide-hotkey) — works from any application; configurable |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Escape` | Cancel repeat mode and stop running script (when active) |

---

## Troubleshooting

### "Connect to internet to sync"
- Check your internet connection
- You may have hit the GitHub API rate limit — set a token (see [GitHub Token](#github-token))
- Corporate firewalls may block `api.github.com` — contact your IT department

### Script fails with "Python Not Found"
- Open **☰ Menu → File → Settings...**
- Click **Browse...** next to Python Interpreter and locate `python.exe`

### Scripts fail after setting up a virtual environment
- Make sure the Python Interpreter in Settings points to `python.exe` inside the venv's `Scripts` folder, not the global Python executable
- Click **↓ Deps** after pointing to the venv so all required packages are installed into it
- The venv does not need to be activated in a terminal — the app calls the Python executable directly by full path

### SHA mismatch warning
- The local cached script differs from what GitHub expects
- Click **Yes** when prompted to re-download the script
- If it persists, delete the cache folder in Settings and re-sync

### Update prompt on local builds
- Local builds pick up the latest release tag via git
- Pull the latest tags with `git fetch --tags` and rebuild
- The local build number will be one higher than the release and no prompt will appear

### A script has disappeared
- It may have been hidden — check **☰ Menu → File → Manage Hidden Scripts**
- If using a filter, clear the search box

### Scripts don't appear after adding a source
- Click **↺ Refresh** to trigger a sync with the new source
- Check the Sources dialog to confirm the source is enabled
