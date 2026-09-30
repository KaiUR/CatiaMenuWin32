---
title: File Reference — CatiaMenuWin32
description: Source file descriptions, key data structures, functions, and constants for CatiaMenuWin32 — the native Win32 Python script launcher for CATIA V5 automation.
---

# File Reference

Descriptions of every source file, key data structures, and important functions.

## Contents
- [Source Files](#source-files)
- [Key Structs](#key-structs)
- [Key Functions](#key-functions)
- [Constants and Limits](#constants-and-limits)
- [Message IDs](#message-ids)
- [Colour System](#colour-system)

---

## Source Files

### `main.h`
Central header included by every `.c` file. Contains:
- All `#include` directives for Win32 headers
- All struct definitions (`AppState`, `Settings`, `Script`, `ScriptFolder`, `ExtraRepo`, `LocalDir`, `ScriptMeta`, `SyncResult`, `RunArgsDlgData`, `HttpResponse`, `TreeEntry`) and enums (`SortMode`, `ScriptSource`, `ScriptBadge`, `ThemeMode`, `SyncStatus`)
- `RUN_STOP_LPARAM` / `RUN_STOP_ID` / `RUN_STOP_BY_USER` — pack and unpack the run id and stopped-by-user flag carried by `WM_SCRIPT_STOPPED`
- All function prototypes
- Layout constants (`TOOLBAR_H`, `TAB_H`, `STATUS_H` etc.)
- Colour `#define` pairs (dark/light)
- Runtime colour accessor inline functions (`COL_BG()`, `COL_TEXT()` etc.)
- Inline helpers (`PostStatus()`, `Util_Log()`, `Util_SnakeToTitle()`)

### `main.c`
Entry point and main window procedure.

- `wWinMain` — app startup: loads settings, creates window, starts sync and update threads, runs message loop
- `MainWndProc` — handles `WM_PAINT`, `WM_SIZE`, `WM_COMMAND`, `WM_DRAWITEM`, `WM_SETTINGCHANGE`, `WM_TRAYICON`, `WM_SYNC_DONE`, `WM_UPDATE_AVAIL`, `WM_SCRIPT_STARTED` / `WM_SCRIPT_STOPPED` (ignoring stale run ids), `WM_HOTKEY_MIGRATED`, `WM_CLOSE`, `WM_DESTROY`; Escape stops a running script when `Runner_IsRunning()`
- Message loop — intercepts `Ctrl+K` for any window of the app and calls `Palette_Show` (covers the system-wide palette hotkey being off or set to another combination); `WM_HOTKEY` routes `HOTKEY_PALETTE` to `Palette_Show` and `HOTKEY_QBAR_TOGGLE` to `IDM_QBAR_TOGGLE`; `Handle_Command` handles `IDM_PALETTE`, `IDM_SHOW_BADGES` and `IDM_MARK_ALL_SEEN`
- `Handle_Command` — routes all `WM_COMMAND` messages to appropriate actions
- `Main_OnDropFiles` — `WM_DROPFILES` handler: a dropped `.py`/`.pyw` file is run once with `Runner_RunPath`; a dropped folder goes to `Main_OfferLocalFolder`; only the first item is used
- `Main_OfferLocalFolder` — asks whether to add a dropped folder as a local script folder (skipping duplicates and the `MAX_LOCAL_DIRS` limit), saves the settings and re-syncs via `IDM_REFRESH`
- `Handle_SyncDone` — called on main thread when sync completes; if `SR_NO_INTERNET` sets `g.status_offline` (amber status bar) and optionally clears tabs when `offline_use_cache` is off; otherwise rebuilds tabs and restores the active tab by folder name (via `g.active_folder_name`) to prevent drift when `Tabs_BuildFavourites` shifts indices
- `App_InitGDI` / `App_FreeGDI` / `App_RebuildGDI` — create/destroy/recreate all GDI resources
- `App_BuildAppDataPath` — sets `g.appdata_dir`: the exe's own folder when a `settings.ini` sits next to the exe (portable mode, sets `g.portable`), otherwise `%APPDATA%\CatiaMenuWin32` (created if missing)
- `App_ResolveTheme` — sets `g.dark_mode` based on `cfg.theme` and Windows registry

### `resource.h`
All `#define` IDs for menus, dialogs, and controls. Grouped by:
- Icon: `IDI_APP_ICON`
- Menu: `IDR_MAINMENU`, `IDM_*` (includes `IDM_CHECK_UPDATES = 244` for Help → Check for Updates)
- Dialogs: `IDD_SETTINGS`, `IDD_ABOUT`, `IDD_SOURCES`, `IDD_REPO_EDIT`
- Controls: `IDC_*`
- Script buttons: `IDC_SCRIPT_BTN_BASE = 1000` (script index added to base)

### `window.c`
Window creation, tab bar, tray icon, and popup menu.

- `Window_Create` — registers all window classes, creates main window and all child controls (Menu, Refresh, Settings, Deps, Stop); Stop button starts disabled. Calls `DragAcceptFiles` and allows `WM_DROPFILES` / `WM_COPYDATA` / `WM_COPYGLOBALDATA` through `ChangeWindowMessageFilterEx`, so files dropped from Explorer arrive even when the app runs elevated
- `Window_OnSize` — repositions tab bar, scroll panel, and status bar on resize
- `Window_ApplyDarkMode` — calls `DwmSetWindowAttribute` for title bar dark mode
- `Window_ApplyAlwaysOnTop` — calls `SetWindowPos` with `HWND_TOPMOST`/`HWND_NOTOPMOST`
- `Window_ShowMenu` — builds and shows the hamburger popup menu via `TrackPopupMenu`; includes **Run → Command Palette...** (`IDM_PALETTE`), **View → Show New/Updated Badges** (`IDM_SHOW_BADGES`, checked from `cfg.show_badges`) and **View → Mark All Scripts as Seen** (`IDM_MARK_ALL_SEEN`)
- `Window_AddTrayIcon` / `Window_RemoveTrayIcon` — manage `NOTIFYICONDATA`
- `Window_ShowTrayMenu` — builds and shows the right-click context menu for the system tray icon
- `Window_ApplyThemeToChildren` — resets the status bar visual style and invalidates the tab bar, status bar, and all child windows after a theme change
- `Window_OpenExeFolder` — opens the folder containing the running `.exe` in Windows Explorer via `ShellExecute`
- `Window_ApplyDarkMenu` — placeholder for future dark-mode popup menu colouring (currently a no-op)
- `TabBarProc` — fully custom tab bar `WndProc`; draws tabs at natural text width, handles scroll arrows and mouse wheel; draws a 6 px accent dot in a tab's top-right corner when `cfg.show_badges` and `Badges_FolderHasUnseen(fi)`
- `TabBar_NaturalWidth` — measures tab label width with `GetTextExtentPoint32W`
- `TabBar_NeedsArrows` — returns true when total tab width exceeds bar width
- `StatusBarProc` — custom owner-drawn status bar `WndProc`

### `tabs.c`
Tab switching and script button management.

- `Tabs_Build` — triggers tab bar repaint (tab bar reads `g.folders[]` directly)
- `Tabs_Switch` — sets `g.active_tab` and `g.active_folder_name`, repaints tab bar, calls `Tabs_RebuildButtons`
- `Tabs_RebuildButtons` — destroys existing script buttons and creates new ones for active tab; skips hidden and filtered-out scripts
- `Tabs_DestroyButtons` — removes all `IDC_SCRIPT_BTN_BASE+n` child windows
- `Tabs_ApplyFilter` — rebuilds the button grid for the active tab showing only scripts matching `g.filter_text`; delegates to `Tabs_RebuildButtons`
- `Tabs_ScriptMatchesFilter` — returns true if the script's name or purpose contains `g.filter_text` (case-insensitive); always returns true when filter is empty
- `Tabs_ApplySort` — sorts `folder.scripts[]` in-place via `qsort` using the active `SortMode`; `SORT_ORDER` is a no-op (preserves API/disk order)
- `Tabs_FolderHasVisible` — returns true if the folder contains at least one non-hidden script; used to suppress empty tabs
- `ScrollPanelProc` — scroll panel `WndProc`; handles `WM_VSCROLL`, `WM_MOUSEWHEEL`, `WM_DRAWITEM` for script buttons; `WM_DRAWITEM` copies the `Script` struct under `cs_folders` to prevent concurrent realloc from the sync thread invalidating the pointer during paint

### `paint.c`
All GDI rendering. Every function uses double-buffering (memory DC + `BitBlt`).

- `Paint_MainWindow` — draws toolbar background, title, version, syncing indicator, update badge
- `Paint_ToolbarButton` — owner-draw for `BS_OWNERDRAW` toolbar buttons (Menu, Refresh, Settings, Deps, Stop); renders pressed/hot/normal/disabled states; the Stop button uses a red accent when enabled
- `Paint_ScriptButton` — draws a script button with accent bar, arrow, label, purpose text, and `i` info badge; accepts `bool repeat` and `bool running` (priority repeat > running > hot); when `cfg.show_badges` and `s->badge != BADGE_NONE` draws an 8 px dot at the right end of the run area (`COL_SUCCESS` new, `COL_ACCENT` updated) and shortens the text rectangle to clear it
- `Paint_Tooltip` — draws the script info tooltip popup; copies the `Script` struct under `cs_folders` before painting to prevent use-after-free if the sync thread reallocates; below the description draws the change note from `Tip_ChangeText`
- `Tip_ChangeText` — builds the tooltip change note: badge state ("New script." / "Updated since you last used it." when badges are shown) plus `meta.last_change`
- `Tip_WrappedHeight` / `Tip_ComputeHeight` — measure word-wrapped small-font text / the whole tooltip, including the change note
- `Tip_ComputeHeight` — measures tooltip height using `DT_CALCRECT` with correct font
- `BtnSubclassProc` — subclass proc for script buttons; handles hover, `i` badge detection, tooltip show/hide, and right-click context menu actions including Open in Editor (tries shell `edit` verb; falls back to `.txt` default app via `AssocQueryStringW` on Windows 10 where `.py` has no `edit` handler); tooltip hover acquires `cs_folders` to safely copy the script before calling `Meta_Parse` and `Tip_ComputeHeight`

### `github.c`
All GitHub API communication.

- `GitHub_HttpGetEx` — HTTPS GET via WinINet: JSON `Accept` header for API calls, optional token and `If-None-Match` ETag, certificate validation (`GitHub_VerifyCert`), then reads the whole body into a heap buffer that doubles as needed up to `HTTP_MAX_BODY` (64 MB). Returns true on 200 (body + `ETag` in the `HttpResponse`) or 304 (no body — the cached copy is current); a connection dropped mid-body fails rather than returning a partial response
- `GitHub_HttpGet` — fixed-buffer wrapper around `GitHub_HttpGetEx` (used by the updater's release check); fails instead of truncating when the body does not fit
- `GitHub_VerifyCert` — validates the server that actually answered (after redirects): its host must pass `GitHub_HostAllowed`, the chain from `INTERNET_OPTION_SERVER_CERT_CHAIN_CONTEXT` must pass `CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL)` for that exact host name, and the leaf's issuer organisation (O=) must exactly match a CA GitHub uses (Sectigo Limited, Let's Encrypt, DigiCert Inc, GlobalSign nv-sa)
- `GitHub_HostAllowed` — true for `github.com` and subdomains of `github.com` / `githubusercontent.com`, matched on a dot boundary so look-alikes such as `evilgithub.com` fail
- `GitHub_DownloadRaw` — downloads a main-repo file from `raw.githubusercontent.com`, retrying up to 3 times (0 / 1 / 2 s back-off) via `GitHub_DownloadRawFull`
- `GitHub_DownloadRawFull` — downloads a file from a given host/path with `GitHub_HttpGetEx` (no size limit short of `HTTP_MAX_BODY`) and writes it with `Util_WriteFile`
- `GitHub_ComputeFileSHA1` — computes Git blob SHA1 (`SHA1("blob <size>\0<content>")`) using Windows CryptAPI
- `GitHub_VerifyScriptSHA` — compares computed SHA against `s->sha` from the API
- `GitHub_ParseOwnerRepo` — extracts `owner` and `repo` from a full GitHub URL
- `GitHub_ParseTree` — parses a Git Trees API response (`?recursive=1`) into a heap array of `TreeEntry`: every blob whose path is `<folder>/<file>.py` directly inside a top-level folder, skipping `setup/` and dot-folders; also returns the SHA of `setup/requirements.txt` and whether GitHub truncated the listing. Returns -1 if the JSON is not a tree listing
- `Util_WriteFile` / `Util_ReadFile` — write a buffer to a file (creating the parent folder) / read a whole file into a null-terminated heap buffer

### `sync.c`
GitHub sync thread and local directory scanning.

- `Sync_Thread` — background thread: lists the main repository with `Sync_FetchTree`, rebuilds its folders (`Sync_BuildMainFolders`), downloads changed scripts, refreshes `setup/requirements.txt` when its SHA changed, syncs extra repos (`Sync_ExtraRepo`; a failure marks the result `SR_PARTIAL` unless the main repo is already offline), scans local dirs, saves the manifest and posts `WM_SYNC_DONE`. Before rebuilding the main-repo folders it moves the current `g.folders[]` aside (not freed) and compares the new listing against them, so additions, removals and the result counts are measured against what was known before the sync (the previous sync, or the startup cache scan); removed main-repo scripts and folders are deleted from the cache. If the listing fails, posts `SR_NO_INTERNET` without clearing the cache pre-loaded by `Sync_LoadManifest` (when `offline_use_cache` is on), or clears it and posts an offline message (when off)
- `Sync_LoadManifest` — called at startup: scans `cache_dir` on disk to populate `g.folders[]` immediately without internet
- `Sync_SaveManifest` — writes all current SHA values to `manifest.ini`
- `Sync_GetLocalSHA` — reads a script's SHA from `manifest.ini`
- `Sync_MergeFolder` — adds scripts to an existing folder or creates a new one; used for merging sources; holds `cs_folders` for the entire operation to guard `Folder_Push` reallocs and `g.folder_count` modifications
- `Sync_ExtraRepo` — syncs one extra repository from a single `Sync_FetchTree` listing of its configured branch: downloads each `.py` whose cached copy is missing or whose blob SHA differs (`Sync_LocalMatches` — the file itself is compared because the shared manifest keys by `Folder/file.py`, which can collide with the main repo), merges each folder with `Sync_MergeFolder`, prunes stale cache files (`Sync_PruneRepoCache`) and refreshes `setup/requirements.txt` when its SHA changed (deleting it if the repo no longer has one). Returns false if the listing or any download failed
- `Sync_FetchTree` — one Git Trees API request (`/repos/{o}/{r}/git/trees/{branch}?recursive=1`, `/` in the branch escaped as `%2F`) with the last listing's ETag as `If-None-Match`; a 304 re-parses the cached listing, a 200 is cached with its ETag. A listing that cannot be parsed deletes the cache; a truncated listing fails with a status message rather than syncing from an incomplete list
- `Sync_TreeCachePath` — `%APPDATA%\CatiaMenuWin32\trees\<owner>_<repo>_<branch>.json` / `.etag` (unsafe file-name characters replaced with `_`)
- `Sync_BuildMainFolders` — rebuilds `g.folders[]` from the main repo's listing (one folder per top-level directory holding `.py` files, listing order) under `cs_folders`
- `Sync_FillScript` — initialises a `Script` from a `TreeEntry` (display name, `gh_path`, SHA, cache path, source)
- `Sync_LocalMatches` — true if a cached file exists and its Git blob SHA equals the listed SHA
- `Sync_PruneRepoCache` — deletes an extra repo's cached `.py` files that its complete listing no longer contains, and removes folders left empty
- `Sync_LocalDir` — scans a local folder, treats subfolders as tabs, skips `setup/`
- `DeleteLocalScript` — deletes a cached script file and removes empty parent directories
- `Sync_IsMainFolder` — true if a pre-sync folder holds at least one main-repo script; the synthetic Favourites tab never counts
- `Sync_FindOldMainFolder` — finds the pre-sync main-repo folder with a given name, or `NULL` if the folder is new

### `runner.c`
Script execution and dependency management.

- `Runner_Run` / `Runner_RunWithArgs` — thin wrappers around `Runner_Launch` (no arguments / the arguments from the Run with Arguments dialog), so both paths share the download, SHA check, output log and Stop handling
- `Runner_Launch` — finds Python, then branches on `s->source`: local scripts (`SCRIPT_SRC_LOCAL`) use `s->local` directly and skip download and SHA checks entirely; GitHub-sourced scripts run the synced cache copy `s->local`, with optional download and SHA verification before launch. Then runs `Runner_CheckDependencies`, records the run count, marks the script seen, and hands over to `Runner_Start`
- `Runner_Start` — shared launch tail for button runs and dropped files: ends repeat mode when a different script starts, ends the running background script (`Runner_Terminate(true)`), increments `g.run_seq`, clears the previous highlight, records `g.run_fi` / `g.run_si` / `g.run_name`, and starts `Runner_Thread`
- `Runner_RunPath` — runs a dropped `.py` file (`g.run_fi = g.run_si = -1`): parses its header for dependencies, runs the check, then `Runner_Start`; no download or SHA check
- `Runner_CheckDependencies` — if `cfg.check_deps` and the script declares dependencies: skips lists that already passed with this interpreter (`g.deps_ok`, keyed by `Runner_DepsHash`), otherwise runs `Runner_FindMissingDeps`; on missing packages asks Yes (install with `Runner_PipInstall`, re-check, refuse if still missing) / No (run anyway) / Cancel. A check that cannot run never blocks the script
- `Runner_FindMissingDeps` — writes the `s_dep_probe` Python helper to `<data folder>\dep_check.py` and runs it with the requirement strings as arguments (no window, stdout piped, 15 s limit); the probe uses `importlib.metadata` plus `packaging` (or pip's vendored copy) for specifiers and markers and prints `requirement<TAB>note` for each missing/out-of-range one
- `Runner_PipInstall` — `cmd /c "python -m pip install <reqs> || pause"` in a new console, waited for; the console stays open only when pip fails
- `Runner_DepsHash` / `Runner_ForgetDeps` — FNV-1a key of interpreter path + requirement list / clears the remembered passes (called by `Runner_UpdateDeps` and Settings OK)
- `Runner_Thread` — creates the process for `python script.py [args]` (with optional `cmd /k` wrapper). Background runs are created suspended, assigned to a job object and resumed; the job and process handles are published in `g.run_job` / `g.run_proc` under `g.cs_run`, `WM_SCRIPT_STARTED` is posted with the run id, and the thread waits with no timeout. After the process exits it unpublishes the handles only if they are still its own (a newer run may have replaced them), posts `WM_SCRIPT_STOPPED` (`wParam` = exit code, `lParam` = `RUN_STOP_LPARAM(run_id, by_user)`), and only then closes the handles
- `Runner_Terminate` — under `g.cs_run`: ends the published run with `TerminateJobObject` (every process the script started), or `TerminateProcess` on `python.exe` if there is no job; sets `g.run_stop_requested`; optionally waits up to 5 s for the process to exit. Never closes a handle — the owning thread does
- `Runner_Stop` — `Runner_Terminate(false)`; the owning thread posts `WM_SCRIPT_STOPPED` once the script has exited
- `Runner_IsRunning` — true while a background run is published and not already stopped; used by the Escape handlers
- `Runner_FindPython` — searches PATH, `cfg.python_exe`, and common install locations
- `EscapeForCmd` — caret-escapes every `cmd.exe` metacharacter (`^ " % & | < > ( )`) in user-supplied arguments for the `cmd.exe /k` keep-open path; quotes are escaped too so cmd never enters its quoted state, and `^%` defeats `%VAR%` expansion on a command line
- `Runner_UpdateDeps` — upgrades pip then runs `pip install --upgrade -r requirements.txt` for each source sequentially
- `RunPipInstall` — runs one pip install command and waits for it to complete

### `meta.c`
Parses the metadata header from PyCATIA script files.

- `Meta_Parse` — reads up to 200 lines of a script looking for the dashed header block; extracts `Script name`, `Version`, `Author`, `Date`, `Purpose`, `Description`, `Code`, `Release`, `requirements`. After the header's closing separator it reads the `Change:` block (up to the next separator) into `change_log` / `last_change`; a `Change:` inside the header block is handled the same way. Opens the file with `_wfsopen(..., _SH_DENYNO)` so a sync download of the same file is never blocked while the header is read
- `Meta_IsChangeDate` — true if a line starts with a `DD.MM.YY` date (one or two digits per part), i.e. begins a new change entry
- `Meta_AddChange` — adds a `Change:` line: a dated line starts a new entry (appended to `change_log`, CRLF-separated, and copied to `last_change`); any other line continues the previous entry
- `Meta_IsKey` / `Meta_IsKnownKey` — whether a line starts a given `key:` / any known header key (ends an `Args:` block; argument lines like `mode:[a|b]` also contain a colon, so only known keys count)
- `Meta_AddArg` — appends one `Args:` parameter line to `args_spec`
- `Meta_AddDeps` — collects every quoted requirement string on a line of the `dependencies = [...]` list into `dependencies` (read from the raw line; the list ends at `]` or at the next known key)
- `Meta_ParseAll` — calls `Meta_Parse` on every script in `g.folders[]`

Header format expected (see [Writing Your Own Scripts](user-guide.md#writing-your-own-scripts) in the user guide for full details):

```python
'''
    -------...------
    Script name:    My_Script.py
    Version:        1.0
    Purpose:        One line summary.
    Author:         Your Name
    Date:           DD.MM.YY
    Description:    Full description.
                    Continuation line.
    -------...------
'''
```

Scripts use the [PyCATIA](https://github.com/evereux/pycatia) library for CATIA V5 COM automation.

### `help.c`
In-app help window implementation.

- `Help_Show()` — opens the help window (or brings it to front if already open); called from F1 and Menu → Help → Help Contents
- `HelpDlgProc` — modeless dialog proc; manages TreeView + RichEdit layout, resizing, and topic switching; paints the app-theme background (`WM_ERASEBKGND`), an accent-blue topic header strip above the RichEdit (`WM_PAINT`, `HDR_H = 40 px`, label stored in `s_header_text`), and a `COL_DIVIDER` vertical line between panes
- `Help_GetRTF` — returns RTF-formatted content string for each of the 12 help topics. Long topics (Running Scripts, Settings, Quick Launch Bar) are built from several static arrays concatenated once into a static buffer, keeping every string literal under the ISO C99 4095-character limit
- `Help_TopicLabel` — returns the display name for each topic
- `Help_LoadTopic` — streams RTF content into the RichEdit control via `EM_STREAMIN`

### `prefs.c`
User preferences persistence and script management dialogs.

- `Prefs_IsFavourite` / `Prefs_SetFavourite` — read/write favourite state to `prefs.ini`
- `Prefs_IsHidden` / `Prefs_SetHidden` — read/write hidden state
- `Prefs_GetRunCount` / `Prefs_IncrementRunCount` — track script run counts for Most Used sort
- `Prefs_GetNote` / `Prefs_SetNote` — per-script user notes
- `Prefs_ApplyToFolders` — stamps all scripts with their prefs after every sync, and classifies each repository script's `badge` by comparing its SHA with `[SeenSHA]` in `prefs.ini` (no entry = `BADGE_NEW`, different = `BADGE_UPDATED`). Until `[Badges] Initialized=1` is written, every script present is recorded as seen instead, so a fresh install or upgrade shows no badges
- `Badges_MarkSeen` — records a script's current SHA as seen and clears its badge in every tab (including the Favourites copy); called by `Runner_Launch` and `ScriptDetailsDlgProc`
- `Badges_MarkAllSeen` — clears every badge at once (View → Mark All Scripts as Seen)
- `Badges_FolderHasUnseen` — true if a tab holds a visible badged script (tab-bar dot)
- `Tabs_BuildFavourites` — builds a `⭐ Favourites` tab at index 0 when any scripts are favourited; removes existing tab first to prevent duplicates
- `ScriptDetailsDlgProc` — full script details dialog with all header fields, the change history (`IDC_DETAIL_CHANGES`), note, favourite/hidden toggles; marks the script as seen on open
- `RunWithArgsDlgProc` — Run with Arguments dialog; `lParam` is a `RunArgsDlgData`. On init it parses the script's metadata and builds the `Args:` form (`ArgForm_Build`, stored in `RunArgsDlgData.form`, freed on `WM_DESTROY`) and restores the last additional arguments; `IDOK` builds the argument string into `args` via `ArgForm_Collect` (and stays open on an invalid number)
- `ArgForm_ParseLine` — parses `name:type=default "help"` (types `str`, `int`, `float`, `bool`, `[a|b|c]`) into an `ArgField`
- `ArgForm_Build` — creates a label + field per parameter (edit, checkbox or drop-down list, IDs from `IDC_ARGFORM_BASE`), moves the free-text box and buttons below them and resizes the dialog (layout in dialog units via `MapDialogRect`); fields start with the value remembered in `prefs.ini [Args]`, else the header default
- `ArgForm_Collect` — validates `int` / `float` fields, remembers every value, and builds `--name value` pairs (a ticked `bool` gives `--name`) followed by the additional text
- `ArgForm_AppendArg` — appends one argument, quoted by the MSVC / Python argv rules when it contains spaces or quotes
- `ArgForm_ValueKey` — `[Args]` key `<script key>|<parameter>` (`|~` for the additional text)
- `ScriptNoteDlgProc` — quick note edit dialog
- `HiddenScriptsDlgProc` — manage hidden scripts with Unhide and Unhide All

### `sources.c`
Sources dialog for managing extra repos and local folders.

- `SourcesDlgProc` — dialog proc for `IDD_SOURCES`; manages two list views (repos and local dirs)
- `RepoEditDlgProc` — sub-dialog for adding/editing a GitHub repo (URL, branch, token, enabled)
- `DeleteFolderRecursive` — recursively deletes a directory and all its contents (used when removing a repo)

### `settings.c`
Settings persistence and dialogs.

- `Settings_Load` — reads `settings.ini` using `GetPrivateProfileString/Int`; sets defaults for missing values
- `Settings_Save` — writes all settings to `settings.ini`
- `Settings_PathToStored` / `Settings_PathFromStored` — in portable mode, store the Python and cache paths relative to the exe's folder when they are inside it, and resolve them back on load
- `Settings_ReadExtras` / `Settings_WriteExtras` — read/write `Options\ShowBadges`, `Options\CheckDependencies` and the command palette hotkey (`Palette\HotkeyEnabled`, `HotkeyMods`, `HotkeyKey`; default on, Ctrl+K) for settings.ini and export/import; reading drops unknown modifier bits, falls back to the default key and disables a combination with no modifier
- `Settings_FillPaletteKeyCombo` / `Settings_PaletteVkFromIndex` / `Settings_PaletteIndexFromVk` — the palette key list: Space, then the Quick Bar hotkey list (A–Z, 0–9, F1–F12)
- `Settings_ShowPaletteHotkey` / `Settings_PaletteEnableControls` — load a palette combination into the dialog / grey its controls when switched off
- `Settings_ApplyAutorun` — adds/removes the app from `HKCU\...\Run` registry key; does nothing in portable mode (the entry may belong to an installed copy)
- `Settings_ClampHotkey` — sanitises the Quick Bar hotkey after any INI read: drops unknown modifier bits, rejects out-of-range virtual keys, and disables the hotkey entirely when no modifier remains (a bare key registered system-wide would be swallowed in every application)
- `Settings_MigrateHotkey` — one-time move of the pre-3.0 default Ctrl+Alt+Q (AltGr+Q, the `@` key on German keyboards) to `QBAR_HOTKEY_DEFAULT_MODS`/`_VK` (Ctrl+Shift+Q); only the exact old default is moved, and an INI carrying `QuickBar\HotkeyMigrated=1` is never touched. Called by `Settings_Load` (which persists the result, writes the flag and sets `g.qbar_hotkey_migrated`) and by settings import; export writes the flag
- `SettingsDlgProc` — dialog proc for `IDD_SETTINGS` (six tabs, `SETTINGS_TABS`); includes Reset to Defaults. Both hotkeys are read and validated at the top of the `IDOK` handler — a modifier is required, and the palette hotkey may not equal the Quick Bar hotkey — so a rejected combination leaves `g.cfg` untouched; after saving it re-registers both hotkeys
- `AboutDlgProc` — dialog proc for `IDD_ABOUT`

### `updater.c`
Checks GitHub Releases API for newer versions.

- `Updater_CheckThread` — background thread: waits 3 s for sync to finish (skipped for manual checks), calls releases API, compares `major.minor.patch`; pass non-NULL `lpParam` to trigger a manual check that shows "up to date" status if no update is found
- `Updater_AutoUpdate` — downloads the new `.exe` directly via WinINet, checks it with `Updater_VerifySignature` (deleting it and opening the releases page on failure), writes a batch script to replace the binary, then posts `WM_CLOSE`; falls back to `Updater_PromptAndInstall` on any failure
- `Updater_VerifySignature` — accepts an update only if its Authenticode signature is intact and its signer's public key equals the running exe's (`CertComparePublicKeyInfo`); an unsigned running exe (local build) never accepts
- `Updater_GetSigner` — `WinVerifyTrust` (`WINTRUST_ACTION_GENERIC_VERIFY_V2`, no UI, no revocation check) on a handle that denies writers; accepts `ERROR_SUCCESS` or `CERT_E_UNTRUSTEDROOT` (self-signed release certificate) and returns a duplicate of the signer certificate via `WTHelperProvDataFromStateData` / `WTHelperGetProvSignerFromChain`
- `Updater_PromptAndInstall` — shows the update dialog and opens the releases page if confirmed
- `IsNewer` — compares remote and local versions (first 3 parts only, ignores build number)
- `ParseVersion` — splits `"1.2.3.4"` into an `int[4]` without locale-dependent `swscanf`
- `ParseLatestTag` — extracts `tag_name` from releases API JSON

### `quickbar.c`
Floating Quick Launch Bar — a small always-on-top button bar that mirrors the ⭐ Favourites tab.

Public API (called from `main.c` and `window.c`):
- `QuickBar_Register` — registers `CMW32QuickBar` and `CMW32QBarTip` window classes, creates the shared large-bold font for 2-letter button labels; must be called once during `Window_Create` before `QuickBar_Create`
- `QuickBar_Create` — creates the floating bar window (`WS_POPUP | WS_BORDER`) and its tooltip companion; installs `SetWinEventHook` callbacks for `EVENT_SYSTEM_FOREGROUND` and `EVENT_SYSTEM_MINIMIZESTART/END`
- `QuickBar_Destroy` — destroys bar and tooltip windows, unhooks both WinEvent hooks, deletes the label font
- `QuickBar_Show` — shows or hides the bar; respects `qbar_target_app` visibility rules (only shows when a visible target window exists)
- `QuickBar_Rebuild` — called after the Favourites list changes; resets scroll to 0 and calls `QB_UpdateGeometry` to resize the bar
- `QuickBar_OnThemeChange` — reapplies `DwmSetWindowAttribute` dark mode and repaints both windows on theme change
- `QuickBar_SetTopmost` — sets or clears `HWND_TOPMOST` on the bar window
- `QuickBar_ShowTargetDlg` — opens the `IDD_QBAR_TARGET` dialog to set the window-title tracking substring
- `QuickBar_RegisterHotkey` — registers the system-wide show/hide hotkey (`HOTKEY_QBAR_TOGGLE`) on `g.hwnd` via `RegisterHotKey` with `MOD_NOREPEAT`; always unregisters first, so it is the single entry point for both initial setup and re-registration after a settings change. Posts a status-bar warning when the combination is already owned by another application
- `QuickBar_UnregisterHotkey` — releases the hotkey if `g.qbar_hotkey_active`; safe to call when nothing is registered
- `QuickBar_HotkeyText` — formats the configured combination for display (e.g. `Ctrl+Shift+Q`, or `(none)`); used by the status messages and by the "Enable Quick Bar" label in both menus

Internal key functions:
- `QuickBarProc` — bar window procedure; handles drag (background = drag handle), click, hover, scroll arrows, mouse wheel, right-click context menu, and `VK_ESCAPE` (calls `Repeat_Stop()` and `Runner_Stop()` so Escape cancels both repeat and the running script)
- `QBarTipProc` — tooltip window procedure; paints script name (bold) and purpose line (small) with double-buffering
- `QB_Paint` — double-buffered render of bar background, scroll arrows (▲▼ / ◄►), and all 2-letter abbreviation buttons with accent bar on hover
- `QB_HitTest` — returns button index (≥0), `HIT_ARROW_PREV/NEXT`, or `HIT_NONE` (background/drag area)
- `QB_UpdateGeometry` — sizes the bar window to fit visible favourites, clamped to 4/5 of the monitor's work area; calls `QB_UpdateScrollMax`
- `QB_CatiaState` — enumerates all top-level windows via `EnumWindows` and returns `CATIA_NONE`, `CATIA_MINIMIZED`, or `CATIA_VISIBLE` based on the tracked title substring
- `QB_UpdateVisibility` — central dispatcher called from both WinEvent hooks; hides bar when target is absent/minimised, sets `HWND_TOPMOST` when target gains focus

### `palette.c`
Command palette — a popup that finds any script by fuzzy name match (Ctrl+K, or the optional system-wide hotkey).

- `Palette_Show` — creates the popup on first use (`CMW32Palette` class, `WS_EX_TOPMOST | WS_EX_TOOLWINDOW`, owned by `g.hwnd`), refreshes its theme brushes, positions it over the main window (or on the active monitor's upper quarter when the main window is hidden — e.g. hotkey pressed in CATIA), resets the query and focuses the search box
- `Palette_Refill` — rebuilds the result list for the current query under `cs_folders`: every non-hidden script except the synthetic Favourites copies, scored by `Palette_Score` (purpose substring match scores 1 as a fallback), sorted by `Palette_Compare`, capped at `PAL_MAX_RESULTS`
- `Palette_Score` — fuzzy subsequence match: every query character must appear in order; bonuses for word starts, consecutive runs and a matching prefix, small capped penalty for gaps; spaces in the query ignored
- `Palette_Compare` — score, then run count, then name
- `Palette_Resolve` — finds a result's script again by folder name + `gh_path` (a sync may have rebuilt `g.folders[]` while the palette was open)
- `Palette_Activate` — Enter runs (`Runner_Run`), Shift+Enter opens Run with Arguments, Ctrl+Enter opens Script Details
- `Palette_EditProc` — search-box subclass: arrow/page keys move the list selection, Enter activates, Escape hides; swallows the Enter/Escape `WM_CHAR` to avoid the beep
- `Palette_DrawItem` / `Palette_WndProc` — owner-drawn rows (name, tab · purpose, badge dot) and the popup's window procedure (themed colours, key-hint footer, hides on `WA_INACTIVE`)
- `Palette_RegisterHotkey` / `Palette_UnregisterHotkey` / `Palette_HotkeyText` — system-wide hotkey `HOTKEY_PALETTE` with `MOD_NOREPEAT` (default Ctrl+K, enabled); a combination another application owns is reported in the status bar

---

## Key Structs

### `SortMode` / `ThemeMode` (enums)

```c
typedef enum {
    SORT_ORDER    = 0,  /* order from GitHub API / disk                */
    SORT_ALPHA    = 1,  /* alphabetical A-Z                            */
    SORT_DATE     = 2,  /* by script header Date: field                */
    SORT_MOST_USED = 3  /* by run count descending                     */
} SortMode;

typedef enum { THEME_SYSTEM = 0, THEME_DARK = 1, THEME_LIGHT = 2 } ThemeMode;
```

### `SyncStatus` / `SyncResult`
Communicates the outcome of a background sync from `Sync_Thread` to the main window via `WM_SYNC_DONE`. The `SyncResult` pointer is passed as `wParam`; `Handle_SyncDone` reads it and `free`s it.

```c
typedef enum { SR_OK=0, SR_NO_INTERNET, SR_API_ERROR, SR_PARTIAL } SyncStatus;

typedef struct {
    SyncStatus status;
    int folders_added, folders_removed;
    int scripts_updated, scripts_added, scripts_removed;
    WCHAR message[256];
} SyncResult;
```

### `ExtraRepo`
One user-added GitHub repository script source. Loaded from `settings.ini` and passed to `Sync_ExtraRepo`.

```c
typedef struct {
    WCHAR url[512];
    WCHAR branch[64];
    WCHAR token[256];
    bool  enabled;
} ExtraRepo;
```

### `LocalDir`
One user-added local folder script source. Loaded from `settings.ini` and passed to `Sync_LocalDir`.

```c
typedef struct {
    WCHAR path[MAX_APPPATH];
    bool  enabled;
} LocalDir;
```

### `ScriptBadge` (enum)
`BADGE_NONE`, `BADGE_NEW`, `BADGE_UPDATED` — stored in `Script.badge` by `Prefs_ApplyToFolders`; drawn by `Paint_ScriptButton`, `TabBarProc` and the palette.

### `HttpResponse` / `TreeEntry`
Result of `GitHub_HttpGetEx` and one script from a Git Trees API listing (`GitHub_ParseTree`).

```c
typedef struct {
    DWORD status;       /* 200, or 304 = unchanged since the ETag sent */
    char *body;         /* heap, null-terminated on 200; NULL on 304 — caller frees */
    DWORD len;
    char  etag[128];    /* ETag of a 200 reply, or "" */
} HttpResponse;

typedef struct {
    WCHAR folder[MAX_NAME]; /* top-level folder = tab name */
    WCHAR file[MAX_NAME];   /* file name including ".py" */
    WCHAR sha[MAX_SHA];     /* Git blob SHA */
} TreeEntry;
```

### `ScriptMeta`
Parsed header fields from a script file. Populated by `Meta_Parse`; read by `Paint_Tooltip`, `ScriptDetailsDlgProc`, and `Tabs_ApplySort`.

```c
typedef struct {
    WCHAR purpose[128];
    WCHAR author[64];
    WCHAR version[32];
    WCHAR date[32];
    WCHAR description[1024];
    WCHAR code[64];        /* e.g. "Python3.10.4, Pycatia 0.8.3"       */
    WCHAR release[32];     /* e.g. "V5R32"                              */
    WCHAR requirements[512];
    WCHAR last_change[256]; /* newest "Change:" entry */
    WCHAR change_log[1024]; /* every "Change:" entry, oldest first, CRLF-separated */
    WCHAR dependencies[512]; /* dependencies = [...] requirement strings, '\n'-separated */
    WCHAR args_spec[1024];   /* Args: parameter lines, '\n'-separated */
} ScriptMeta;
```

### `AppState` (`g`)
The single global state struct. All state lives here.

```c
typedef struct {
    HWND   hwnd;           /* main window */
    HWND   hwnd_tab;       /* custom tab bar (CMW32TabBar) */
    HWND   hwnd_scroll;    /* scroll panel containing script buttons */
    HWND   hwnd_status;    /* custom status bar (CMW32StatusBar) */
    HWND   hwnd_tip;       /* tooltip popup */
    HWND   hwnd_search;    /* search/filter edit box */
    HWND   hwnd_details;   /* script details panel */

    ScriptFolder folders[MAX_FOLDERS];  /* all loaded folders/tabs */
    int      folder_count;
    int      active_tab;
    int      tab_offset;   /* first visible tab when scrolling */
    Settings cfg;
    bool     dark_mode;    /* resolved from cfg.theme + system setting */

    HFONT  font_ui;        /* Segoe UI 13px normal */
    HFONT  font_bold;      /* Segoe UI 13px semibold */
    HFONT  font_small;     /* Segoe UI 11px normal */
    HBRUSH br_bg, br_toolbar, br_btn, br_btn_hot, br_accent, br_status;

    bool   syncing;
    bool   status_offline; /* true when showing stale cache due to no internet */
    int    hot_btn;        /* currently hovered script button ID */
    int    tip_btn;        /* button whose tooltip is showing */
    int    tip_h;          /* cached tooltip height */
    WCHAR  last_run_path[MAX_APPPATH];
    WCHAR  appdata_dir[MAX_APPPATH];
    WCHAR  latest_version[32];         /* from GitHub releases API */
    WCHAR  active_folder_name[MAX_NAME]; /* folder name saved by Tabs_Switch for post-sync restoration */
    int    scroll_y, scroll_max;
    bool   tray_icon_added;

    CRITICAL_SECTION cs_folders;  /* guards folders[] and folder_count between sync and UI threads */

    /* Running background script — guarded by cs_run; only the owning
       Runner_Thread closes these handles, after unpublishing them */
    CRITICAL_SECTION cs_run;
    HANDLE run_job;               /* job object holding the script's process tree; NULL if none */
    HANDLE run_proc;              /* the script's python.exe process; NULL when idle */
    bool   run_stop_requested;    /* Runner_Stop has ended the published run */
    LONG   run_seq;               /* UI thread only: id of the most recent launch */

    /* Filter */
    WCHAR  filter_text[MAX_NAME]; /* current search/filter string */

    /* Details panel */
    int    details_script_fi;    /* folder index of shown script */
    int    details_script_si;    /* script index of shown script */
    bool   details_visible;

    /* Quick Launch Bar */
    HWND   hwnd_qbar;            /* floating bar window */
    HWND   hwnd_qbar_tip;        /* bar tooltip popup */
    int    qbar_hot;             /* hovered button index, -1 = none */
    int    qbar_scroll;          /* current scroll offset (px) */
    int    qbar_scroll_max;      /* maximum scroll offset */
    bool   qbar_dragging;
    int    qbar_drag_ox;         /* drag start: cursor offset from left */
    int    qbar_drag_oy;         /* drag start: cursor offset from top */
    int    qbar_tip_idx;         /* button index shown in tip, -1 = none */
    bool   qbar_hotkey_active;   /* show/hide hotkey is currently registered */
    bool   qbar_hotkey_migrated; /* Settings_Load moved the old Ctrl+Alt+Q default this launch */
    HWND   hwnd_palette;         /* command palette popup; NULL until first opened */
    bool   palette_hotkey_active; /* system-wide palette hotkey is registered */

    /* Double-click repeat mode */
    bool   repeat_mode;          /* true while a script is looping */
    int    repeat_fi;            /* folder index of the script to repeat */
    int    repeat_si;            /* script index of the script to repeat */
    bool   suppress_lbuttonup;   /* suppresses extra WM_LBUTTONUP after WM_LBUTTONDBLCLK */

    /* Running state (background mode only) */
    bool   script_running;       /* true while a background script is in flight */
    int    run_fi;               /* folder index of the running script */
    int    run_si;               /* script index of the running script */
    WCHAR  run_name[MAX_NAME];   /* running script's name (log header; also dropped files) */
    DWORD  deps_ok[64];          /* passed dependency checks (Runner_DepsHash) */
    int    deps_ok_count;
    bool   portable;             /* settings.ini next to the exe: data lives in the exe's folder */
} AppState;
```

### `Settings`
All user-configurable options. Loaded from/saved to `settings.ini`.

```c
typedef struct {
    WCHAR     python_exe[MAX_APPPATH];
    WCHAR     cache_dir[MAX_APPPATH];
    WCHAR     github_token[256];
    bool      auto_sync, download_before_run;
    bool      show_console, console_keep_open, deps_keep_open;
    bool      always_on_top, minimize_to_tray;
    bool      start_with_windows, start_minimized;
    bool      check_updates;
    bool      auto_update;       /* auto-download and install updates */
    bool      offline_use_cache; /* show cached scripts when offline (default: false) */
    ThemeMode theme;             /* THEME_SYSTEM=0, THEME_DARK=1, THEME_LIGHT=2 */
    int       refresh_interval;  /* hours, 0 = disabled, default 6 */
    bool      main_repo_enabled;
    ExtraRepo extra_repos[MAX_EXTRA_REPOS];
    int       extra_repo_count;
    LocalDir  local_dirs[MAX_LOCAL_DIRS];
    int       local_dir_count;
    SortMode  sort_mode;         /* global sort mode */
    /* Quick Launch Bar */
    bool      qbar_enabled;
    bool      qbar_horizontal;
    bool      qbar_topmost_with_catia;
    int       qbar_x, qbar_y;
    WCHAR     qbar_target_app[MAX_NAME]; /* window-title substring; empty = no target */
    WCHAR     qbar_target_exe[MAX_NAME]; /* process exe name (e.g. CNEXT.exe); empty = any */
    bool      qbar_hotkey_enabled;       /* register the show/hide hotkey at all */
    UINT      qbar_hotkey_mods;          /* MOD_* flags (default MOD_CONTROL|MOD_SHIFT) */
    UINT      qbar_hotkey_vk;            /* virtual-key code (default 'Q') */
    bool      repeat_on_dblclick;        /* repeat main-window scripts on double-click */
    bool      qbar_repeat_on_dblclick;   /* repeat Quick Bar scripts on double-click */
    bool      tint_script_sources;       /* tint local/extra-repo buttons differently */
    bool      show_badges;               /* new/updated dots (default true) */
    bool      check_deps;                /* check dependencies before running (default true) */
    bool      palette_hotkey_enabled;    /* system-wide palette hotkey (default true) */
    UINT      palette_hotkey_mods;       /* default MOD_CONTROL */
    UINT      palette_hotkey_vk;         /* default 'K' */
} Settings;
```

### `ScriptFolder`
One tab worth of scripts.

```c
typedef struct {
    WCHAR    name[MAX_NAME];    /* raw folder name e.g. "Any_Document_Scripts" */
    WCHAR    display[MAX_NAME]; /* formatted e.g. "Any Document Scripts" */
    Script  *scripts;           /* heap-allocated; use Folder_Alloc / Folder_Free */
    int      count;
    int      capacity;          /* number of allocated slots */
    bool     loaded;
    SortMode sort_mode;
} ScriptFolder;
```

`Folder_Alloc`, `Folder_Free`, and `Folder_Push` are inline helpers in `main.h`. `Folder_Push` doubles capacity automatically when `count == capacity`.

### `Script`
One script button.

```c
typedef struct {
    WCHAR      name[MAX_NAME];       /* display name */
    WCHAR      gh_path[MAX_APPPATH]; /* GitHub API path e.g. "folder/script.py" */
    WCHAR      sha[MAX_SHA];         /* expected blob SHA from GitHub API */
    WCHAR      local[MAX_APPPATH];   /* local cache path */
    ScriptMeta meta;
    bool       meta_loaded;
    bool       is_favourite;
    bool       is_hidden;
    int        run_count;
    WCHAR      note[MAX_NOTE_LEN];
    ScriptSource source;             /* SCRIPT_SRC_MAIN / _EXTRA / _LOCAL */
    ScriptBadge  badge;              /* BADGE_NONE / BADGE_NEW / BADGE_UPDATED */
} Script;
```

---

## Constants and Limits

### String / buffer limits

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FOLDERS` | 64 | Maximum number of tabs |
| `MAX_SCRIPTS` | 1024 | Default initial capacity per folder (heap grows dynamically) |
| `MAX_EXTRA_REPOS` | 8 | Maximum extra GitHub repos |
| `MAX_LOCAL_DIRS` | 8 | Maximum local folders |
| `MAX_FAVOURITES` | 256 | Maximum entries in the favourites list in `prefs.ini` |
| `MAX_HIDDEN` | 256 | Maximum entries in the hidden-scripts list in `prefs.ini` |
| `MAX_NAME` | 128 | Wide character buffer for names, tab labels, filter text |
| `MAX_SHA` | 64 | Wide character buffer for a Git blob SHA1 hex string |
| `MAX_APPPATH` | 520 | Wide character buffer for file-system paths |
| `MAX_NOTE_LEN` | 512 | Wide character buffer for per-script user notes |
| `HTTP_BUF_SIZE` | 512 KB | Default capacity for `GitHub_HttpGet` callers' fixed buffers |
| `HTTP_MAX_BODY` | 64 MB | Upper bound for `GitHub_HttpGetEx`'s growable buffer and `Util_ReadFile` |

### Layout constants

| Constant | Value | Description |
|----------|-------|-------------|
| `WIN_MIN_W` | 820 | Minimum window width in pixels |
| `WIN_MIN_H` | 420 | Minimum window height in pixels |
| `TOOLBAR_H` | 38 | Toolbar height in pixels |
| `TAB_H` | 26 | Tab bar height in pixels |
| `TAB_ARROW_W` | 22 | Tab scroll arrow width |
| `STATUS_H` | 22 | Status bar height |
| `SEARCH_H` | 26 | Search/filter box height |
| `BTN_H` | 40 | Script button height |
| `BTN_GAP` | 6 | Vertical gap between script buttons |
| `BTN_MX` | 12 | Script button horizontal margin |
| `BTN_MY` | 10 | Script button vertical margin |
| `INFO_BTN_W` | 28 | Width of the `i` info badge |
| `STAR_BTN_W` | 28 | Width of the favourite star badge |
| `TIP_W` | 320 | Script tooltip popup width |
| `QBAR_BTN_SIZE` | 52 | Quick bar button face square (px) |
| `QBAR_PAD` | 4 | Margin from bar edge to buttons |
| `QBAR_GAP` | 4 | Gap between adjacent bar buttons |
| `QBAR_ARROW_W` | 18 | Quick bar scroll arrow click area |
| `QBAR_TIP_W` | 240 | Quick bar tooltip popup width |
| `QBAR_TIP_PAD` | 8 | Quick bar tooltip internal padding |
| `QBAR_TIP_ROW` | 18 | Quick bar tooltip row height |

---

## Message IDs

| Message | Value | Direction | Description |
|---------|-------|-----------|-------------|
| `WM_SYNC_DONE` | `WM_USER+1` | Thread → Main | Sync completed; `wParam` is heap `SyncResult*` (freed by handler) |
| `WM_STATUS_SET` | `WM_USER+2` | Thread → Main | Update status bar; `lParam` is heap `WCHAR*` (freed by handler) |
| `WM_TRAYICON` | `WM_USER+10` | System → Main | Tray icon mouse event |
| `WM_UPDATE_AVAIL` | `WM_USER+11` | Thread → Main | Newer version found |
| `WM_AUTO_REFRESH` | `WM_USER+12` | Timer → Main | Auto-refresh interval elapsed |
| `WM_SCRIPT_STARTED` | `WM_USER+13` | Runner → Main | Background script launched (`wParam` = run id); enables the Stop button and turns the running button green. Ignored if the id is not `g.run_seq` (run already replaced) |
| `WM_SCRIPT_STOPPED` | `WM_USER+14` | Runner → Main | Background script exited or was terminated (`wParam` = exit code, `lParam` = `RUN_STOP_LPARAM(run id, stopped-by-user)`); disables the Stop button, clears the green highlight, writes the log footer and status, and triggers repeat. A stale id only writes `--- Stopped: another script was started. ---` |
| `WM_LOG_OUTPUT` | `WM_USER+15` | Log reader → Main | Script output chunk; `lParam` is heap `WCHAR*` (freed by handler) |
| `WM_HOTKEY_MIGRATED` | `WM_USER+16` | Startup → Main | Posted once when `Settings_Load` moved the Quick Bar hotkey off Ctrl+Alt+Q; shows the explanatory message box |

### Timer IDs

| Constant | Value | Description |
|----------|-------|-------------|
| `TIMER_AUTO_REFRESH` | 1001 | `SetTimer` ID for the periodic auto-sync interval |
| `TIMER_QBAR` | 1002 | `SetTimer` ID used by the Quick Launch Bar for visibility polling |

### Hotkey IDs

| Constant | Value | Description |
|----------|-------|-------------|
| `HOTKEY_QBAR_TOGGLE` | 1 | `RegisterHotKey` ID on the main window; `WM_HOTKEY` routes it to `IDM_QBAR_TOGGLE` |
| `HOTKEY_PALETTE` | 2 | `RegisterHotKey` ID for the command palette; `WM_HOTKEY` calls `Palette_Show` |

`QBAR_HOTKEY_DEFAULT_MODS` / `QBAR_HOTKEY_DEFAULT_VK` hold the Quick Bar default, **Ctrl+Shift+Q**; `PALETTE_HOTKEY_DEFAULT_ON` / `_MODS` / `_VK` the command palette default, **Ctrl+K, enabled**. Avoid Ctrl+Alt defaults: Windows treats Ctrl+Alt as AltGr, so Ctrl+Alt+Q is the `@` key on German keyboards.

---

## Colour System

All colours are defined as `#define` pairs (dark/light) and accessed via inline runtime functions that check `g.dark_mode`:

```c
// Defined as macros:
#define COL_BG_DARK    RGB(28,  28,  35)
#define COL_BG_LIGHT   RGB(240, 240, 245)

// Accessed via inline function:
static inline COLORREF COL_BG(void) {
    return g.dark_mode ? COL_BG_DARK : COL_BG_LIGHT;
}
```

Runtime accessor functions (dark/light):

| Function | Dark | Light | Used for |
|----------|------|-------|----------|
| `COL_BG()` | `#1C1C23` | `#F0F0F5` | Main window background |
| `COL_TOOLBAR()` | `#141418` | `#D2D2DC` | Toolbar and tab bar |
| `COL_BTN_NORM()` | `#2C2E40` | `#DCDEED` | Script button normal |
| `COL_BTN_HOT()` | `#3E415A` | `#BEC3DC` | Script button hover |
| `COL_BTN_PRESS()` | `#1C1E30` | `#AAAFCD` | Script button pressed |
| `COL_INFO_ZONE()` | `#24263A` | `#C8CAD8` | Script button info badge zone |
| `COL_TEXT()` | `#D2D7F0` | `#1E1E28` | Primary text |
| `COL_SUBTEXT()` | `#6E7494` | `#646478` | Secondary text, purposes |
| `COL_DIVIDER()` | `#2E3042` | `#BEC0D2` | Separators and borders |
| `COL_TIP_BG()` | `#161620` | `#F5F5FC` | Tooltip background |

Fixed-colour constants (no theme variant):

| Constant | Value | Used for |
|----------|-------|----------|
| `COL_ACCENT` | `RGB(82, 155, 245)` | Highlights, selected tabs, accent bars |
| `COL_ACCENT_DIM` | `RGB(48, 92, 160)` | Dimmed accent for pressed states |
| `COL_SUCCESS` | `RGB(80, 200, 120)` | Success/OK indicators; running script button highlight |
| `COL_WARN` | `RGB(240, 190, 60)` | Warning status indicators |
| `COL_STAR` | `RGB(255, 200, 60)` | Favourite star badge |
| `COL_TIP_BORDER` | `RGB(82, 155, 245)` | Tooltip popup border |
