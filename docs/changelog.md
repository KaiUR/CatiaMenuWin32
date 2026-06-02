---
title: Changelog — CatiaMenuWin32
description: Release history for CatiaMenuWin32 — the CATIA V5 Python script launcher and macro manager.
---

# Changelog

All notable changes to CatiaMenuWin32 are documented here.

---

## v2.5.0 — Virtual environment management, log window controls

### Added
- **Virtual environment management** — The Python Interpreter section in Settings → General now includes four buttons:
  - **Create venv** — Creates an isolated Python environment at `%APPDATA%\CatiaMenuWin32\venv` using the currently configured Python and updates the path field automatically. Click **↓ Deps** afterwards to install script packages into it.
  - **Browse venv...** — Browse for an existing virtual environment folder; the app locates `Scripts\python.exe` inside it and sets the path.
  - **Use global Python** — Probes PATH and well-known install locations for a global Python and fills the path field immediately.
  - **Delete venv** — Removes the default venv directory from disk after confirmation. Clears the path field if it was pointing into the deleted environment.
- **Log window: right-click context menu** — Right-clicking anywhere in the Script Output Log opens a context menu with Copy (grayed when nothing is selected), Select All, and Clear log.
- **Log window: Save log... button** — Saves the full session log buffer to a UTF-8 text file (with BOM) via a Save As dialog.
- **Log window: Clear button** — Clears the log from both the window and the internal session buffer.
- **Log window: dark mode button styling** — Log window buttons now use the same owner-draw style as toolbar buttons, matching dark and light theme correctly.

### Fixed
- Two `LTEXT` controls in the Settings dialog used ID `-1` and were never hidden when switching tabs; replaced with properly-IDed buttons.
- `resource.rc.in`: octal literal `094` replaced with `94` (caused `llvm-rc` parse failure).

---

## v2.4.5 — Extra repo sync infinite loop fix

### Fixed
- **Extra repo sync hangs indefinitely** — `Sync_ExtraRepo` iterated folder-contents JSON using `fnp+1` (after the name value) or `fsp+1` (after the sha value) to advance the inner loop pointer. The GitHub Contents API returns file objects with `"type"` **after** `name`, `sha`, `size`, and `url`, so both advancement positions were behind the `"type"` that had just been found. `strstr` re-found the same `"type"` on every iteration, producing an infinite loop. The fix uses `ftp+1` (past the type value) on all continuation and completion paths, matching the pattern used by `GitHub_ParseFolder`.

---

## v2.4.4 — Empty folder sync fix

### Fixed
- **Sync hangs on extra repos with empty folders** — When an extra GitHub repository contained folders with no `.py` files (e.g. placeholder folders with only a `.gitkeep`), `Sync_ExtraRepo` still called `Sync_MergeFolder` with a zero-script count, creating empty tabs and wasting an API round-trip per empty folder. Empty folders are now silently skipped.

---

## v2.4.3 — Offline refresh fixes and offline tint

### Fixed
- **Scripts cleared on offline Refresh (cached mode)** — When internet is unavailable and *Show cached scripts offline* is enabled, pressing Refresh caused all script buttons to disappear until the app was restarted. `Handle_SyncDone` returned early on `SR_NO_INTERNET` without rebuilding the UI, so any button destruction that occurred during the sync was never recovered. The handler now performs a full UI rebuild from the in-memory cached state, restoring the previously active tab by name.
- **Local scripts hidden after offline Refresh** — Local directory scripts vanished alongside cached repo scripts for the same reason. The rebuild also picks up any local-dir changes merged by `Sync_LocalDir` on the background thread during the sync.
- **Enabling cached scripts while already offline showed no scripts** — If *Show cached scripts offline* was disabled when the network dropped (clearing the in-memory folder list), then enabled while still offline, the next Refresh left the UI empty because the in-memory state was already gone. The handler now calls `Sync_LoadManifest` to reload from the disk cache when the in-memory list is empty, so scripts reappear immediately without a restart.
- **Tooltips disappearing after tab or button rebuild** — After any button rebuild (Refresh, resize, tab switch) the `(i)` tooltip would not reappear on hover until the mouse left the panel and re-entered. `Tabs_DestroyButtons` did not reset `g.tip_btn` or hide the tooltip window, so the "show tooltip" condition (`g.tip_btn != uid`) was never true for the new button. The fix resets `g.tip_btn = -1` and hides the tooltip in `Tabs_DestroyButtons`.

### Added
- **Offline script tinting** — When internet is unavailable and cached scripts are being shown, script buttons are tinted red to indicate they may be out of date. Main-repo scripts use one shade of red; extra-repo scripts use a second, distinct shade. Local scripts are never tinted red regardless of connectivity — they do not require internet. The tint applies in both the main window and the Quick Launch Bar, and respects dark/light theme.

---

## v2.4.2 — Thread-safety fix for Refresh crash

### Fixed
- **App crash on Refresh (especially shortly after pushing scripts)** — A race condition between the sync thread and the main UI thread caused heap corruption. `GitHub_ParseFolder` released `cs_folders` immediately after `Folder_Free`/`Folder_Alloc`, then continued calling `Folder_Push` without the lock. If `Folder_Push` triggered `realloc` (doubling the scripts buffer) while `WM_DRAWITEM` or the tooltip paint was concurrently reading `f->scripts`, the UI thread would dereference the freed pointer. The race was more likely immediately after a push since new/changed scripts caused extra `Folder_Push` calls, increasing the probability of a realloc. Fixes: `cs_folders` is now held for the entire parse loop in `GitHub_ParseFolder`; similarly wrapped in `GitHub_ParseRoot` and `Sync_MergeFolder`. On the UI side, `WM_DRAWITEM`, `Paint_Tooltip`, and the tooltip hover handler in `BtnSubclassProc` now copy the `Script` struct under `cs_folders` before reading it.

---

## v2.4.1 — Local script run fix

### Fixed
- **Local scripts could not be run** — `Runner_Run` always called `Runner_BuildLocalPath`, which reconstructs the script path from `s->gh_path`. For local directory scripts, `gh_path` holds a Windows absolute path (used as a uniqueness key, not a real GitHub path). `wcsrchr` found no forward slash, so the entire Windows path was appended to the cache directory, producing a garbage path. The missing-file check then triggered a `GitHub_DownloadRaw` call with that garbage path, which failed with "Failed to download script from GitHub." Local scripts now bypass `Runner_BuildLocalPath`, download, and SHA verification entirely and execute directly from `s->local`.

---

## v2.4.0 — Improved help window, Open in Editor fix (Windows 10)

### Improved
- **Help window — topic header strip** — the content pane now shows a coloured accent-blue header bar above the text, displaying the current topic name. Makes it immediately clear which topic is open after scrolling.
- **Help window — RTF content styling** — body text increased from 10 pt to 11 pt; main topic titles from 14 pt to 16 pt; section sub-headings now rendered in accent blue bold for clearer visual hierarchy; `Note:` callouts styled with a bold accent label.
- **Help window — minimum size** — bumped from 600 × 420 to 680 × 460 for a more comfortable reading layout.

### Fixed
- **Open in Editor — Windows 10** — the v2.3.0 fix fell back to the shell `open` verb when no `edit` verb was registered, which launches the Python interpreter on Windows 10. The handler now queries the `.txt` file-association's open handler via `AssocQueryStringW` as a proxy for the user's preferred text editor (VS Code, Notepad++, Notepad, etc.), so the correct editor opens on both Windows 10 and Windows 11.

---

## v2.3.2 — Export / Import granular token and path selection

### Improved
- **Export / Import Settings — per-token selection** — instead of a single "Include tokens" toggle, a dedicated **Tokens** list now shows one checkbox per available token: one for the GitHub account token and one for each extra repository that carries a token. Uncheck individual tokens to share a settings file without exposing sensitive credentials.
- **Export / Import Settings — granular path selection** — the former "General settings" checkbox is split into three independent checkboxes: **Python path**, **Cache folder**, and **Options, theme, window & Quick Bar**. This allows sharing a portable settings file (options and preferences only) with other users without transferring machine-specific paths.

---

## v2.3.1 — Export / Import per-item selection fix

### Fixed
- **Export / Import Settings per-item selection** — the initial v2.3.0 release showed three coarse all-or-nothing checkboxes (General / Sources / Tokens) instead of individual checkboxes per repository and local folder as intended. The dialog now lists each extra repository and local folder as a separate checkable item so the user can pick exactly which ones to transfer.

---

## v2.3.0 — Selective Import / Export Settings, Open in Editor fix (Windows 10)

### Added
- **Export Settings** (Settings dialog → Export...) — saves a selective export to any `.ini` file. A dialog lists every extra repository and local folder with individual checkboxes so you can pick exactly which ones to include, plus a **General settings** checkbox and an **Include tokens** toggle. Uncheck Tokens to produce a portable file that can be shared without exposing sensitive data.
- **Import Settings** (Settings dialog → Import...) — after choosing the source file, the same per-item dialog appears showing the repos and folders available in that file. Selected repos and local folders are **appended** to your current sources (nothing is replaced). Duplicate URLs/paths are skipped silently; if a duplicate repo carries a token the app prompts you to keep the existing or imported token. Settings take effect immediately and the dialog closes. `prefs.ini` (favourites, notes, run counts) is not affected.

### Fixed
- **Open in Editor did not work on Windows 10** — `ShellExecuteW` with the `"edit"` verb fails silently on Windows 10 when no application registers an `"edit"` handler for `.py` files (common on Windows 10; less so on Windows 11 where modern IDEs register the verb). The handler now uses `ShellExecuteExW` with `SEE_MASK_FLAG_NO_UI` to suppress any error dialog, then falls back to the `"open"` verb (the user's default handler for the file type) if `"edit"` is unavailable.

---

## v2.2.2 — Extra repository sync fix

### Fixed
- **Extra GitHub repositories were not synced when the main repo was unreachable** — if the main repository failed to connect (no internet, rate limit, certificate error, etc.) the sync thread posted an immediate "no internet" result and returned, skipping extra repo and local dir sync entirely. Extra repos now always run through their own sync pass regardless of the main repo's connectivity.
- **Only the first folder of an extra repository was ever downloaded** — `Sync_ExtraRepo` reused a single buffer for both the root directory listing and each sub-folder contents fetch. After the first folder fetch overwrote the buffer, the outer root-parsing loop's pointers into it became invalid, so every folder after the first was silently skipped. The folder-contents fetch now uses a separate allocation so the root listing stays intact throughout the loop.

---

## v2.2.1 — Open script location, open with default app, open in editor

### Added
- **Open Script Location** (right-click menu) — opens the folder containing the script file in Windows Explorer. Available for all scripts.
- **Open with Default App** (right-click menu) — opens the script file with the Windows default application for its file type (e.g. Python, an IDE). Available for all scripts.
- **Open in Editor** (right-click menu) — opens the script file using the shell `edit` verb, which invokes whatever app is registered as the editor for that file type (e.g. VS Code, Notepad++, IDLE). Shown only for local folder scripts.

---

## v2.2.0 — Script source tinting, scroll flicker fix, script output log

### Added
- **Script source tinting** — script buttons (main window and Quick Launch Bar) can now be tinted based on their origin: a warm rose tint for scripts from local folder sources and a cool green tint for scripts from extra GitHub repositories. Built-in scripts from KaiUR/Pycatia_Scripts remain untinted. Enabled by default.
- **Settings → Window → Tint local and extra-repo script buttons differently** — new checkbox to toggle source tinting. Default: **On**.
- **Script Output Log window** — when scripts run in background (no-console) mode, a **≡ Log** toolbar button opens a persistent modeless log window that displays all stdout and stderr output captured from every script run. Each run is framed by a timestamped header (`=== ScriptName  (HH:MM:SS) ===`) and a completion footer (`--- Finished successfully. ---`, `--- Stopped by user. ---`, or `--- Exited with code N. ---`). The window uses a Consolas monospace font and retains the full session history. Output is buffered internally so nothing is lost even if the window is closed during a run.
- **Log window syntax highlighting** — output lines are colour-coded automatically: header lines in accent blue, completion footers in green/amber/red, and Python runtime output containing keywords such as `Error`, `Exception`, `Traceback`, `Fatal`, or `Warning` highlighted in red or amber respectively. Colours follow the current app theme (dark/light).
- **Log window dark mode and icon** — the log window title bar, background, and text now match the current app theme. The app icon appears in the title bar and taskbar button. Theme changes (including live switching) are applied without reopening the window.

### Fixed
- **Scroll flicker when scrolling quickly** — eliminated white flash artefacts when scrolling through the script panel at high speed. Three-part fix: removed `SW_ERASE` from the `ScrollWindowEx` call so the background is not erased before sibling buttons are repositioned; added `WM_ERASEBKGND` returning 1 to the button subclass procedure to prevent each button from self-erasing; and added `WS_CLIPSIBLINGS` to every script button so overlapping sibling windows cannot overdraw each other during a scroll.

---

## v2.1.6 — Show cached scripts when offline

### Added
- **Show cached scripts when offline** — when the app starts with no internet connection, scripts that were synced during a previous online session can now be displayed immediately from the local cache. An amber warning appears in the status bar: *⚠ Offline — showing N cached folder(s). Scripts may be out of date.*
- **Settings → Sync → Show cached scripts when offline** — a new checkbox controls the behaviour. Default: **Off** (the previous behaviour — no buttons shown when offline). Turn it on to always display the cached scripts when offline.

### Fixed
- **Cached scripts were always hidden when offline** — the sync thread cleared `g.folders[]` before checking for internet connectivity, so `Sync_LoadManifest`'s pre-populated cache was destroyed even when the connection failed. The clear is now deferred until after a successful connection is confirmed.

---

## v2.1.5 — Escape stops script; single green highlight

### Fixed
- **Multiple green buttons when a new script is clicked** — clicking a second script while one was already running left both buttons highlighted green. `Runner_Run` now invalidates the previously highlighted button (and the Quick Bar) before overwriting `g.run_fi`/`g.run_si`, so only the newly launched script's button turns green.
- **Escape did not stop the running script when in repeat mode** — pressing Escape while the Quick Launch Bar had keyboard focus (the normal state after double-clicking a Quick Bar button to start repeat) stopped repeat mode but left the script running. `QuickBarProc WM_KEYDOWN` now calls `Runner_Stop()` in addition to `Repeat_Stop()`, matching the behaviour of the main window's Escape handler.

---

## v2.1.4 — Running script green highlight

### Added
- **Running script highlight** — when a script button is clicked (single click, repeat mode not active) the button turns **green** for the duration of the run: border, left accent bar, and label text all use `COL_SUCCESS`. The highlight clears automatically when the script exits. Applies to both the main window script buttons and the Quick Launch Bar. Only active in background (no-console) mode, where script completion is tracked via `WM_SCRIPT_STARTED` / `WM_SCRIPT_STOPPED`.

---

## v2.1.2 — Auto-update fix

### Fixed
- **Auto-update did not replace the exe or restart the app** — the update batch script was written as UTF-16LE (`ccs=UTF-16LE`), which `cmd.exe` cannot execute; the app closed but nothing was copied or restarted. Fixed by writing a plain ANSI `.bat` file (which `cmd.exe` has always read) and using `GetShortPathNameW` to convert the embedded paths to 8.3 format so the file content is always ASCII-safe, even on systems with non-Latin characters in the user-profile path.

---

## v2.1.1 — Repeat-on-double-click

### Added
- **Repeat script on double-click** — double-clicking any script in the main window or the Quick Launch Bar puts that script into *repeat mode*: it re-runs automatically after each completion until you press **Escape**, single-click the same script, or single-click a different script. The repeating button is highlighted in amber (border, accent bar, label, and loop symbol ↻) so you always know what is looping.
- **Toggle per context** — repeat mode can be enabled or disabled independently for the main window (**☰ Menu → Run → Repeat Script on Double-Click**) and for the Quick Launch Bar (**right-click bar → Repeat on Double-Click** or **☰ Menu → View → Quick Bar → Repeat on Double-Click**). Both options also appear in **Settings → Console** and **Settings → Quick Bar** tabs.
- Defaults: both repeat toggles are **On**.
- **Console-mode note** — repeat is not supported when **Show Python console window** is on (the script process runs attached to a console and completion is not tracked); a status-bar message explains this if you double-click in console mode.

---

## v2.1.0 — Security hardening and Stop Script button

### Added
- **Stop Script button** — a new **■ Stop** toolbar button terminates a running background script instantly. The button is grayed out when idle and turns red when a script is active. Only background (no-console) runs are tracked; console-mode scripts can be stopped by closing the console window directly.

### Fixed
- **Race condition on sync** — the sync thread clearing `g.folders[]` while the UI thread reads it could cause a use-after-free crash; all access to `g.folders[]` and `g.folder_count` is now guarded by a `CRITICAL_SECTION cs_folders` added to `AppState`
- **Shell injection in runner** — paths containing `"`, `^`, or `%` could break out of `cmd.exe` command lines; the `cmd.exe /c` code path is eliminated (Python is now launched directly via `CreateProcess`), and the `cmd.exe /k` keep-open path escapes those characters via a new `EscapeForCmd` helper
- **HTTP indefinite hang** — `GitHub_HttpGet` had no timeouts; a 15-second limit is now set for connect, send, and receive via `InternetSetOption`
- **Batch file encoding for non-ASCII paths** — the auto-updater batch script was written as CP_ACP narrow chars, silently corrupting paths that contain non-Latin characters; switched to `_wfopen_s` with `ccs=UTF-16LE` and `fwprintf_s` wide-char output
- **Mutex handle leak in `wWinMain`** — `hMutex` was never closed; `CloseHandle(hMutex)` added before `App_FreeGDI()`
- **Integer overflow in `cmp_runs` sort comparator** — subtraction-based comparator could overflow for large `run_count` values; replaced with explicit three-way comparison
- **GDI brush leak in `Paint_Tab`** — `CreateSolidBrush` result passed directly to `FrameRect` without storing or deleting the handle; handle is now stored in a local and deleted after use
- **Dead `TipWndProc` function in `paint.c`** — removed an unreachable `TipWndProc` function; the active tooltip window procedure is `TipWndProcInternal` in `window.c`
- **Redundant `SetWindowPos` call in `Window_ApplyAlwaysOnTop`** — duplicate no-op call removed
- **`ParseLatestTag` duplicate whitespace condition** — `while (*p == ' ' || *p == ':' || *p == ' ')` had `' '` duplicated; corrected to `while (*p == ' ' || *p == ':' || *p == '\t')`
- **`wcsncpy` → `wcsncpy_s` throughout** — replaced every remaining `wcsncpy` call across `runner.c`, `tabs.c`, `sync.c`, `github.c`, `prefs.c`, `meta.c`, `sources.c`, `settings.c`, `updater.c`, and `window.c` with C11 Annex K `wcsncpy_s` to match the project safe-string policy and eliminate truncation-without-null-termination risk
- **Removed `_CRT_SECURE_NO_WARNINGS`** — suppressor macro removed from `main.h`; all previously suppressed warnings are now addressed by the `_s` function migration

---

## v2.0.8 — Default settings update

### Changed
- **Default for Start with Windows changed to On** — new installations and **Reset to Defaults** now enable Start with Windows out of the box
- **Default for Quick Launch Bar changed to On** — the Quick Bar is now enabled by default; **Reset to Defaults** also restores it to enabled with `CNEXT.exe` as the target executable
- **Default for Auto-install updates changed to On** — new installations now automatically download and apply updates; **Reset to Defaults** sets it to On

---

## v2.0.7 — Code quality and workflow fixes

### Fixed
- **`resource.h` include guard** — the `#endif` guard closed at line 56 leaving ~160 defines outside the guard; moved to the end of the file
- **Duplicate control ID** — `IDC_STATUS_BAR` and `IDC_BTN_MENU` both had ID `502`; `IDC_STATUS_BAR` reassigned to `507`
- **Duplicate macro definitions** — `WM_TRAYICON` and `TRAY_ID` were defined in both `resource.h` and `main.h`; removed from `resource.h` (canonical definitions remain in `main.h`)
- **`wcsncat` → `wcsncat_s`** — replaced four remaining `wcsncat` calls in `meta.c` and `updater.c` with C11 Annex K `wcsncat_s` to match the project's safe-string policy
- **Dead variable in `meta.c`** — `in_reqs` was set then immediately suppressed with `(void)in_reqs`; variable and workaround comment removed
- **Duplicate step label in `sync.c`** — two sections were both labelled "Step 6"; the result-message-building section corrected to "Step 9"
- **CI: PR builds compiled `main` instead of PR branch** — the "Update Contributors File" step ran on all triggers and executed `git checkout main`, causing subsequent build steps to compile the `main` branch on PR runs; step now gated to tag pushes only
- **CI: build number not incrementing between releases** — the workflow read the incremented `build_number.txt` from the CI workspace but never committed it back; added a commit step (after GPG import, before tag creation) that writes `build_number.txt` and `CONTRIBUTORS.md` back to `main` with `[skip ci]`

---

## v2.0.6 — Tab hiding, About dialog, GitHub Pages link, sync visual fixes

### Added
- **Hidden-tab suppression** — tabs whose every script has been hidden are automatically removed from the tab bar. When the last visible script in a tab is hidden, the tab disappears and the app auto-switches to the first tab that still has visible scripts. Tabs reappear instantly if any script in them is unhidden
- **GitHub Pages link** — `☰ Menu → Help → GitHub Pages` opens the app's GitHub Pages site in the browser
- **Redesigned About dialog** — the About dialog now displays the app icon as a 48×48 logo, a bold title with the current version number, and a **GitHub Pages** button

### Fixed
- **White flash on tab switch** — switching between tabs no longer causes a white flash; the destroy-and-rebuild cycle is wrapped in `WM_SETREDRAW FALSE/TRUE` and the panel is redrawn in a single atomic pass
- **Sync visual glitches** — the "Checking for updates…" placeholder text no longer bleeds through gaps between script buttons during a sync; script buttons retain their name text while the background sync thread temporarily clears folder data

### Changed
- **GitHub token label** — the Settings groupbox now reads "increases API rate limit; required for private repos" (was the ambiguous "raises API rate limit")
- **Quick Bar target process default** — the **Target Exe** field now defaults to `CNEXT.exe` on fresh installs and after **Reset to Defaults**

---

## v2.0.5 — Minimize to tray default, auto-update on manual check, Quick Bar exe browse

### Added
- **Browse button for Quick Bar target exe** — the **Set Target App…** dialog and the **Settings → Quick Bar** tab now include a **Browse…** button next to the "Target process executable name" field, letting users pick the executable via a standard file picker instead of typing the filename manually
- **Auto-update honoured on manual check** — if **Auto-install updates** is enabled in Settings, choosing **Help → Check for Updates…** now also triggers the automatic download-and-replace flow (previously it always fell back to opening the releases page on a manual check)

### Changed
- **Default for Minimize to Tray changed to On** — new installations now start with Minimize to Tray enabled; the **Reset to Defaults** button also sets it to On

---

## v2.0.4 — Quick Bar process executable filter

### Added
- **Target Exe filter for the Quick Bar** — a new optional "Target process executable name" field prevents false positives from other windows that share the same title substring as the target app
  - Set via right-click on the bar → **Set Target App…**, or **Settings → Quick Bar → Target process executable name**
  - When set (e.g. `CNEXT.exe`), a window must match **both** the title substring **and** the process executable name before the bar responds to it
  - Comparison is case-insensitive; only the filename is checked, not the full path
  - Leave the field empty (default) to match any process — identical to the previous behaviour
  - Persisted as `QuickBar\TargetExe` in `settings.ini`

---

## v2.0.3 — Check for Updates, tab restore fix, security fixes

### Added
- **Check for Updates menu item** — `Help → Check for Updates...` performs a manual update check in the background and reports "up to date" in the status bar if no newer version is found; does not wait for the startup delay used by the auto-check

### Fixed
- **Active tab restores by folder name after sync** — after a refresh/sync the active tab now restores by folder name instead of by index, preventing a one-tab-right drift that occurred when the Favourites tab is present
- **C11 security warnings** — replaced all `fprintf` / `memcpy` calls in `updater.c` and `sync.c` with C11 Annex K `_s` variants (`fprintf_s`, `memcpy_s`); replaced `fopen` with `fopen_s` in the auto-updater batch script writer
- **Memory leak in sync** — `old_scripts` buffer allocated per-folder in the sync loop was never freed; `free()` now called after the removal-detection step each iteration

### Changed
- **Help window content reformatted** — all 10 help topics updated: subheadings now have a blank-line gap before their content; bullet descriptions after em-dashes are consistently capitalised; section tab names use title case (General Tab, Sync Tab, Console Tab, Window Tab, Quick Bar Tab); numbered list in Running Scripts replaced with bullets for visual consistency

---

## v2.0.2 — Tabbed Settings dialog, Quick Bar help topic

### Added
- **Tabbed Settings dialog** — the flat Settings dialog has been reorganised into five tabs for easier navigation:
  - **General** — Python interpreter, script cache folder, GitHub token
  - **Sync** — sync on startup, download before run, check for updates, auto-install updates, auto-refresh interval
  - **Console** — show console, keep console open, keep Deps console open
  - **Window** — Always on Top, Minimize to Tray, Start with Windows, Start Minimized, Theme (Dark/Light/System), Sort Scripts (Default/Alphabetical/By Date/Most Used)
  - **Quick Bar** — enable/disable, orientation (Vertical/Horizontal), Stay on Top with Target App, Target App window-title field
- **Quick Launch Bar help topic** — new "Quick Launch Bar" entry in the in-app help TreeView (F1), covering enabling the bar, populating it from Favourites, moving it, orientation, and the target-app tracking behaviour
- **Settings now applies side effects on OK** — changing theme, Always on Top, autorun, sort mode, or Quick Bar settings in the Settings dialog now takes immediate effect without requiring a separate menu toggle

### Changed
- The **Window** section settings (Always on Top, Minimize to Tray, Start with Windows, Start Minimized, Theme) and the **Sort Scripts** setting are now accessible directly from the Settings dialog in addition to the menu, making them discoverable in a single place
- Quick Bar configuration (enable, orientation, target app) is now accessible from the Settings dialog in addition to the right-click context menu and View menu
- **Help → Settings** topic updated to describe the new five-tab layout
- **Help → The Interface** topic updated to mention the Quick Launch Bar

---

## v2.0.1 — Quick Bar hides when target not open

### Changed
- **Quick Launch Bar — hide when target not running** — when a target app is configured the bar now hides whenever the target is not open at all (previously it stayed visible when the app was not running and only hid when all windows were minimised). The bar now shows only when the target has at least one visible non-minimised window. When no target app is configured the bar remains always visible as before.
- Local build number increments by **+1** per CMake configure (same as CI), instead of +2.

### Fixed
- **ISO C pedantic warning** in `main.c` — replaced the double cast `(RtlGetVersion_t)(void *)` on the `GetProcAddress` return value with a union type-pun (`union { FARPROC proc; RtlGetVersion_t fn; }`), which is valid C99/C11 and eliminates the `-Wpedantic` warnings about converting between function pointer and object pointer types.

---

## v2.0.0 — Quick Launch Bar, universal target app

### Added
- **Quick Launch Bar** — a free-floating, always-on-top button bar sourced from the ⭐ Favourites tab
  - Vertical or horizontal orientation (toggle via right-click or **Menu → View → Quick Bar**)
  - Large 52×52 px buttons labelled with the first two uppercase letters of the script name
  - Custom scroll arrows (▲▼ / ◄►) appear automatically when favourites exceed the visible area; mouse wheel also scrolls
  - Hover tooltip shows the script name (bold) and Purpose metadata line
  - Drag anywhere on the bar background to reposition it; position is saved to `settings.ini`
  - Right-click context menu: Enable, Horizontal, Vertical, On Top with Target App, Set Target App…, Reset Position
  - Same options accessible via **☰ Menu → View → Quick Bar**
  - Window class `CMW32QuickBar` uses `WS_EX_TOOLWINDOW` — no taskbar entry or Alt-Tab slot
  - Double-buffered GDI rendering with theme-aware colours

- **Target application tracking** — the bar integrates with the foreground application:
  - Hides automatically when the target app is running but **all** its windows are minimised (detected via `EnumWindows`)
  - Reappears as soon as any target window is restored
  - Stays visible when the target app is not open at all (so you can run start-up scripts)
  - Rises to TOPMOST when the target app is in the foreground; drops to normal z-order otherwise

- **Configurable Target App** — the bar is no longer CATIA-only:
  - Right-click the bar (or **Menu → View → Quick Bar → Set Target App…**) to enter any window-title substring
  - Leave the field empty for no target tracking — bar stays visible at all times when enabled, no topmost behaviour
  - Default value: `CATIA V5` — existing users see no change
  - Stored as `[QuickBar] TargetApp` in `settings.ini`

### Changed
- Menu label **"On Top with CATIA"** renamed to **"On Top with Target App"** and greyed out when no target is configured

---

## v1.3.14 — Tab bar scroll fix

### Fixed
- **Tab bar scroll** — the last tab was unreachable when tabs have unequal widths. The scroll limit (`max_off`) was calculated using `CountFit(offset=0)`, which is not symmetric — a wider last tab may not fit even after scrolling past a narrower one (e.g. "★ Favourites"). The calculation now walks forward until all remaining tabs fit from the computed offset, ensuring the last tab is always reachable via the ◄ ► arrows and mouse wheel.

---

## v1.3.13 — Update Dependencies now upgrades packages

### Fixed
- **Update Dependencies** now upgrades pip itself and all packages — runs `pip install --upgrade pip` then `pip install --upgrade -r requirements.txt`, so both the package manager and all dependencies are brought up to date

---

## v1.3.12 — SxS fix, verified tags, updated certificate

### Fixed
- Side-by-side configuration error on machines without the VC++ redistributable — manifest is now embedded exclusively via the RC file with `/MANIFEST:NO` on the linker, preventing the linker from merging in `Microsoft.VC*.CRT` dependencies
- Release tags now show as **Verified** on GitHub — switched from GitHub API annotated tags to lightweight tags pointing to the verified bot commit

### Changed
- CI compiler switched from `clang` (GNU driver) to `clang-cl` (MSVC driver) — `clang-cl` correctly honours `CMAKE_MSVC_RUNTIME_LIBRARY` for static CRT selection
- Code signing certificate updated — now includes email identity (`kairathjen@yahoo.com`)

---

## v1.3.11 — LLVM toolchain, code signing

### Changed
- CI/CD build toolchain switched from MSYS2/MinGW-w64 (GCC) to LLVM/Clang on `windows-latest`
- MSVC environment (`ilammy/msvc-dev-cmd`) provides Windows SDK, `rc.exe`, and `link.exe`; Clang is the C compiler
- CMakeLists.txt: RC include flag now auto-detects `windres` vs `rc.exe` (`-I` vs `/I`)
- CMakeLists.txt: `UNICODE` and `_UNICODE` explicitly defined for all Windows builds

### Added
- **Code signing** — release binaries are Authenticode-signed via `skymatic/code-sign-action@v1` using a PFX certificate stored in GitHub Secrets (`CERTIFICATE`, `PASSWORD`, `CERTHASH`, `CERTNAME`)

---

## v1.3.10 — Memory safety, unsafe string operations

### Fixed
- Memory leak in `Sync_LocalDir` — heap-allocated `scripts` buffer was never freed when `FindFirstFileW` returns no results, and was not freed after `Sync_MergeFolder` in any code path
- Replaced all `wcscpy`/`wcscat` calls with bounds-safe `wcsncpy`/`wcsncat` across `sync.c`, `github.c`, `updater.c`, `window.c`, `meta.c`, `settings.c`, `sources.c`, and `prefs.c`
- `updater.c` — `wcscat` appending filename suffix into `MAX_APPPATH` buffers after `GetTempPath` could overflow on unusually long temp paths; replaced with `wcsncat` with remaining space

---

## v1.3.9 — Heap-allocated scripts, favourites fixes

### Added
- Scripts per folder now heap-allocated and grow dynamically — no hard limit per folder
- `MAX_SCRIPTS` set to 1024 as default initial capacity

### Fixed
- Crash when adding to favourites — pointer ownership bug after folder shift
- Removing from favourites in the Favourites tab now correctly updates the original script
- Tab no longer shifts after removing one item from a multi-item Favourites tab

---

## v1.3.8 — Wiki links in help menu

### Added
- **Wiki links in help menu** — `☰ Menu → Help` now includes links to the relevant GitHub Wiki pages

---

## v1.3.7 — Report a Bug, Wiki

### Added
- **Report a Bug** — `Menu → Help → Report a Bug...` opens a pre-filled GitHub issue in the browser with app version, Windows version, Python path and theme automatically populated
- GitHub Wiki created with 11 pages: Home, Installation, User Guide, Settings, Script Sources, Script Management, Writing Scripts, Auto Update, Security, Developer Guide, Changelog

---

## v1.3.6 — Auto-update, download buffer

### Fixed
- **Auto-update closes app correctly** — the app now exits cleanly before the updater batch file runs
- **Larger download buffer** — HTTP download buffer increased for more reliable large file transfers

---

## v1.3.5 — SHA verification fix

### Fixed
- **SHA verification after failed downloads** — a failed or partial download no longer leaves a corrupt cached file that passes the SHA check on the next run

---

## v1.3.4 — Auto-update batch file path fix

### Fixed
- Auto-Update option failing — the path generated for the updater batch file was incorrect, causing copy/paste to fail

---

## v1.3.3 — In-app help, bounds-checking functions

### Added
- **In-app help window** — `F1` or `Menu → Help → Help Contents` opens a resizable help window
- TreeView table of contents with 11 topics: Getting Started, The Interface, Running Scripts, Settings, Script Sources, Favourites & Search, Script Details & Notes, Sort & Hide Scripts, Update Dependencies, Keyboard Shortcuts, Troubleshooting
- RichEdit panel with formatted topic content
- Single instance — `F1` when already open brings window to front
- Help icon embedded as resource (`res/help_icon.ico`)
- New `src/help.c` added to project

### Fixed
- Tooltips and Script Details blank on first launch on a new machine — meta retries after sync downloads files
- Auto-update download URL fixed — missing `v` prefix caused download failure
- Search filter rebuilds button list — no gaps between filtered results
- All-caps script names (e.g. IGES) now match correctly in search

---

## v1.3.2 — New machine tooltip fix, default settings

### Changed
- Default settings changed: **Start with Windows** on, **Minimize to Tray** on, **Start Minimized** off
- Replaced all `_snwprintf` with `_snwprintf_s` using `_TRUNCATE` across all source files
- Replaced `memset` with `ZeroMemory` throughout
- Replaced `memmove` with `memmove_s` in `meta.c`
- Replaced `snprintf` with `_snprintf_s` in `github.c`

### Fixed
- Tooltips and Script Details blank on first launch on a new machine — meta retries after sync downloads files
- Auto-update download URL fixed — missing `v` prefix caused download failure
- Two static analysis warnings in `meta.c` — dead code and unused variable
- Suppressed unavoidable `GetProcAddress` cast warning with `#pragma GCC diagnostic`

---

## v1.3.1 — Search fix, update prompt fix

### Fixed
- Search/filter now rebuilds button list instead of hiding buttons — no more gaps between results
- All-caps script names (e.g. IGES) now match correctly in search (case-insensitive via `towlower`)
- Local builds no longer trigger the update prompt
- GitHub Pages homepage improved

---

## v1.3.0 — Script management, search, sorting, favourites, details

### Added
- **Search/filter box** — real-time filter bar below toolbar; filters by script name and purpose
- **Favourites tab** — right-click any script → Add to Favourites; tab appears/disappears automatically; persisted across sessions
- **Script details dialog** — right-click → Script Details; shows all header fields, note, favourite/hidden toggles
- **Hide scripts** — right-click → Hide Script; restore via Menu → File → Manage Hidden Scripts
- **Script notes** — right-click → Add/Edit Note; stored in `prefs.ini`
- **Run with Arguments** — right-click → Run with Arguments
- **Script sorting** — Menu → View → Sort Scripts: Default, Alphabetical, By Date, Most Used
- **Most Used sort** — run count tracked per script in `prefs.ini`
- **Keyboard shortcuts** — `Ctrl+Tab` / `Ctrl+Shift+Tab` to cycle through tabs
- **Open Executable Folder** — Menu → File → Open Executable Folder
- **Auto-refresh interval** — background sync every N hours; default 6 hours; configurable in Settings
- **Auto-update** — download and install new versions automatically; falls back to manual if download fails
- **Prefs system** — new `prefs.ini` stores favourites, hidden scripts, notes, run counts
- **Double `.py` fix** — scripts were cached as `scriptname.py.py`; now correctly `scriptname.py`
- `Code`, `Release`, and multi-line `requirements:` fields now correctly parsed and displayed

### Fixed
- `Tabs_BuildFavourites` removes existing tab before rebuilding — no duplicate tabs
- Settings dialog layout — Auto-install updates no longer overlaps Console Options group
- Sources dialog widened so all columns visible
- `cmd.exe /k` quoting fixed in `Runner_RunWithArgs`

---

## v1.2.6 — Update Deps fix, GitHub Pages

### Added
- GitHub Pages site at `https://kaiur.github.io/CatiaMenuWin32`
- `sitemap.xml` submitted to Google Search Console
- `.github` and dot-folders now skipped — never appear as tabs

### Fixed
- Update Deps crash — `cmd.exe /k` requires inner command wrapped in extra quotes when paths contain spaces

---

## v1.2.5 — Disable main repo fix

### Fixed
- Disabling the main repository and pressing Refresh now immediately clears scripts from the UI
- Ghost buttons no longer appear on hover after all sources are disabled
- `g.folder_count` reset at start of sync so disabled sources never persist after refresh

---

## v1.2.4 — Documentation, reset to defaults, single instance

### Added
- Full `docs/` folder — user guide, developer guide, file reference, changelog
- Screenshots in README and user guide
- `CONTRIBUTING.md` and `SECURITY.md` for both repos
- **Reset to Defaults** button in Settings dialog
- Single instance enforcement — second launch restores existing window

### Fixed
- Update checker `IS_LOCAL_BUILD` guard removed — all builds now check for updates
- Local version detection switched to `git tag --sort=-creatordate`
- Update dialog shows `v` prefix consistently
- Toolbar "Update Deps" renamed to "Deps"
- Tooltip positioning fixed for maximised/multi-monitor
- Sources dialog null pointer dereference fixed
- Sync no longer clears cached tabs on no-internet

---

## v1.2.3 — Certificate validation, SHA verification

### Added
- **Certificate pinning** — every HTTPS request validates server cert subject and issuer
- **SHA verification** — every script verified against GitHub blob SHA before execution

### Fixed
- Raw CDN certificate — added `github.io` to allowed subjects, Let's Encrypt to allowed issuers

---

## v1.2.2 — Sources, scrollable tabs

### Added
- **Script Sources dialog** — manage additional GitHub repositories and local script folders
- Extra GitHub repos sync with same cert validation and SHA verification
- Local script folders — subfolders become tabs, scripts run from disk
- **Enable/Disable** toggle for each source
- **Scrollable tab bar** — natural text width, ◄ ► arrows, mouse wheel scrolling
- Default window width 820px
- `setup/` folder never becomes a tab

### Fixed
- Sync no longer clears cached tabs on no-internet
- Tab text no longer truncated with ellipsis

---

## v1.2.1 — Update checker fix, version info

### Added
- `.exe` file properties populated via `resource.rc.in`
- Dual GitHub menu links — View App and View Scripts

### Fixed
- Update checker version comparison — manual digit parser replaces `swscanf`
- `IS_LOCAL_BUILD` guard removed

---

## v1.2.0 — Dark mode, tooltip, custom tab bar

### Added
- Custom `CMW32TabBar` replaces native `WC_TABCONTROL` — full dark/light mode control
- Tooltip auto-sizes to description length
- Dark popup menus
- `deps_keep_open` setting

### Fixed
- Tab labels readable in both dark and light mode
- Tooltip description no longer cut off
- Status bar and toolbar buttons correctly themed

---

## v1.1.0 — Always on Top, tray, theme, autorun

### Added
- Always on Top toggle
- Minimize to Tray — hide to system tray on minimize/close
- Start with Windows — registry autorun
- Start Minimized option
- Dark / Light / System theme toggle
- Custom `CMW32StatusBar` — owner-drawn, dark mode aware
- Hamburger `☰ Menu` button replaces native menu bar
- Check for updates on startup setting
- GitHub PAT support — raises rate limit from 60 to 5000 req/hr
- Show console / Keep console open options

### Fixed
- Always on Top applied correctly after `ShowWindow`
- Theme reapplied on `WM_SHOWWINDOW` when restoring from tray
- Script descriptions no longer cut off

---

## v1.0.0 — Initial release

### Features
- Native Win32/C application — no frameworks
- Live sync from `KaiUR/Pycatia_Scripts` via GitHub API
- Script buttons organised by folder into tabs
- Script metadata tooltip (Purpose, Author, Version, Date, Description)
- SHA-based delta sync — only downloads changed files
- Offline cache — scripts load immediately from disk on startup
- Python auto-detection
- AppData settings (`settings.ini` and `manifest.ini`)
- Auto-versioning via CMake + `build_number.txt`
- GitHub Actions CI/CD — tag push → automatic release
- Dark mode via `DwmSetWindowAttribute`
- Custom GDI rendering — double-buffered throughout
