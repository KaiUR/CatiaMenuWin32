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
- All struct definitions (`AppState`, `Settings`, `Script`, `ScriptFolder`, `ExtraRepo`, `LocalDir`, `ScriptMeta`, `SyncResult`)
- All function prototypes
- Layout constants (`TOOLBAR_H`, `TAB_H`, `STATUS_H` etc.)
- Colour `#define` pairs (dark/light)
- Runtime colour accessor inline functions (`COL_BG()`, `COL_TEXT()` etc.)
- Inline helpers (`PostStatus()`, `Util_Log()`, `Util_SnakeToTitle()`)

### `main.c`
Entry point and main window procedure.

- `wWinMain` — app startup: loads settings, creates window, starts sync and update threads, runs message loop
- `MainWndProc` — handles `WM_PAINT`, `WM_SIZE`, `WM_COMMAND`, `WM_DRAWITEM`, `WM_SETTINGCHANGE`, `WM_TRAYICON`, `WM_SYNC_DONE`, `WM_UPDATE_AVAIL`, `WM_CLOSE`, `WM_DESTROY`
- `Handle_Command` — routes all `WM_COMMAND` messages to appropriate actions
- `Handle_SyncDone` — called on main thread when sync completes; if `SR_NO_INTERNET` sets `g.status_offline` (amber status bar) and optionally clears tabs when `offline_use_cache` is off; otherwise rebuilds tabs and restores the active tab by folder name (via `g.active_folder_name`) to prevent drift when `Tabs_BuildFavourites` shifts indices
- `App_InitGDI` / `App_FreeGDI` / `App_RebuildGDI` — create/destroy/recreate all GDI resources
- `App_BuildAppDataPath` — builds `g.appdata_dir` from `%APPDATA%\CatiaMenuWin32` and creates the directory if it does not exist
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

- `Window_Create` — registers all window classes, creates main window and all child controls (Menu, Refresh, Settings, Deps, Stop); Stop button starts disabled and is enabled/disabled by `WM_SCRIPT_STARTED`/`WM_SCRIPT_STOPPED`
- `Window_OnSize` — repositions tab bar, scroll panel, and status bar on resize
- `Window_ApplyDarkMode` — calls `DwmSetWindowAttribute` for title bar dark mode
- `Window_ApplyAlwaysOnTop` — calls `SetWindowPos` with `HWND_TOPMOST`/`HWND_NOTOPMOST`
- `Window_ShowMenu` — builds and shows the hamburger popup menu via `TrackPopupMenu`
- `Window_AddTrayIcon` / `Window_RemoveTrayIcon` — manage `NOTIFYICONDATA`
- `Window_ShowTrayMenu` — builds and shows the right-click context menu for the system tray icon
- `Window_ApplyThemeToChildren` — resets the status bar visual style and invalidates the tab bar, status bar, and all child windows after a theme change
- `Window_OpenExeFolder` — opens the folder containing the running `.exe` in Windows Explorer via `ShellExecute`
- `Window_ApplyDarkMenu` — placeholder for future dark-mode popup menu colouring (currently a no-op)
- `TabBarProc` — fully custom tab bar `WndProc`; draws tabs at natural text width, handles scroll arrows and mouse wheel
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
- `ScrollPanelProc` — scroll panel `WndProc`; handles `WM_VSCROLL`, `WM_MOUSEWHEEL`, `WM_DRAWITEM` for script buttons

### `paint.c`
All GDI rendering. Every function uses double-buffering (memory DC + `BitBlt`).

- `Paint_MainWindow` — draws toolbar background, title, version, syncing indicator, update badge
- `Paint_ToolbarButton` — owner-draw for `BS_OWNERDRAW` toolbar buttons (Menu, Refresh, Settings, Deps, Stop); renders pressed/hot/normal/disabled states; the Stop button uses a red accent when enabled
- `Paint_ScriptButton` — draws a script button with accent bar, arrow, label, purpose text, and `i` info badge; accepts `bool repeat` and `bool running` — priority: repeat (amber) > running (green) > hot (blue)
- `Paint_Tooltip` — draws the script info tooltip popup
- `Tip_ComputeHeight` — measures tooltip height using `DT_CALCRECT` with correct font
- `BtnSubclassProc` — subclass proc for script buttons; handles hover, `i` badge detection, tooltip show/hide

### `github.c`
All GitHub API communication.

- `GitHub_HttpGet` — opens an HTTPS connection, validates the server certificate, sends a GET request, reads the response
- `GitHub_VerifyCert` — checks cert subject contains `github.com`/`github.io` and issuer is a known CA
- `GitHub_DownloadRaw` — downloads a raw file from `raw.githubusercontent.com`
- `GitHub_DownloadRawFull` — downloads from a specified host/path (used for extra repos)
- `GitHub_ComputeFileSHA1` — computes Git blob SHA1 (`SHA1("blob <size>\0<content>")`) using Windows CryptAPI
- `GitHub_VerifyScriptSHA` — compares computed SHA against `s->sha` from the API
- `GitHub_ParseOwnerRepo` — extracts `owner` and `repo` from a full GitHub URL
- `GitHub_ParseRoot` — parses the contents API JSON to find folder entries
- `GitHub_ParseFolder` — parses a folder's contents JSON to find `.py` script entries

### `sync.c`
GitHub sync thread and local directory scanning.

- `Sync_Thread` — background thread: fetches root, parses folders, downloads changed scripts, syncs extra repos, scans local dirs, saves manifest, posts `WM_SYNC_DONE`; if the GitHub connection fails, posts `SR_NO_INTERNET` without clearing the cache pre-loaded by `Sync_LoadManifest` (when `offline_use_cache` is on), or clears it and posts an offline message (when off)
- `Sync_LoadManifest` — called at startup: scans `cache_dir` on disk to populate `g.folders[]` immediately without internet
- `Sync_SaveManifest` — writes all current SHA values to `manifest.ini`
- `Sync_GetLocalSHA` — reads a script's SHA from `manifest.ini`
- `Sync_MergeFolder` — adds scripts to an existing folder or creates a new one; used for merging sources
- `Sync_ExtraRepo` — syncs one extra GitHub repository
- `Sync_LocalDir` — scans a local folder, treats subfolders as tabs, skips `setup/`
- `DeleteLocalScript` — deletes a cached script file and removes empty parent directories

### `runner.c`
Script execution and dependency management.

- `Runner_Run` — verifies SHA, finds Python, invalidates the previously highlighted button before overwriting `g.run_fi`/`g.run_si` (ensures only one button is green at a time), then launches script in a thread
- `Runner_Thread` — creates process for `python script.py` (with optional `cmd /k` wrapper); for background runs, duplicates the process handle into `g.run_process`, posts `WM_SCRIPT_STARTED`, waits up to 30 minutes, then atomically clears the handle and posts `WM_SCRIPT_STOPPED`
- `Runner_Stop` — atomically claims `g.run_process` via `InterlockedExchangePointer`, calls `TerminateProcess`, and posts `WM_SCRIPT_STOPPED` to disable the Stop button
- `Runner_FindPython` — searches PATH, `cfg.python_exe`, and common install locations
- `Runner_RunWithArgs` — runs a script with additional command-line arguments; uses `EscapeForCmd` to prevent shell injection in the `cmd.exe /k` keep-open path
- `EscapeForCmd` — escapes `^`, `"`, and `%` so user-supplied arguments cannot inject commands into `cmd.exe`
- `Runner_UpdateDeps` — upgrades pip then runs `pip install --upgrade -r requirements.txt` for each source sequentially
- `RunPipInstall` — runs one pip install command and waits for it to complete

### `meta.c`
Parses the metadata header from PyCATIA script files.

- `Meta_Parse` — reads up to 200 lines of a script looking for the dashed header block; extracts `Script name`, `Version`, `Author`, `Date`, `Purpose`, `Description`
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
- `HelpDlgProc` — modeless dialog proc; manages TreeView + RichEdit layout, resizing, and topic switching; paints the app-theme background (`WM_ERASEBKGND`) and a `COL_DIVIDER` vertical line between panes (`WM_PAINT`)
- `Help_GetRTF` — returns RTF-formatted content string for each of the 12 help topics
- `Help_TopicLabel` — returns the display name for each topic
- `Help_LoadTopic` — streams RTF content into the RichEdit control via `EM_STREAMIN`

### `prefs.c`
User preferences persistence and script management dialogs.

- `Prefs_IsFavourite` / `Prefs_SetFavourite` — read/write favourite state to `prefs.ini`
- `Prefs_IsHidden` / `Prefs_SetHidden` — read/write hidden state
- `Prefs_GetRunCount` / `Prefs_IncrementRunCount` — track script run counts for Most Used sort
- `Prefs_GetNote` / `Prefs_SetNote` — per-script user notes
- `Prefs_ApplyToFolders` — stamps all scripts with their prefs after every sync
- `Tabs_BuildFavourites` — builds a `⭐ Favourites` tab at index 0 when any scripts are favourited; removes existing tab first to prevent duplicates
- `ScriptDetailsDlgProc` — full script details dialog with all header fields, note, favourite/hidden toggles
- `RunWithArgsDlgProc` — dialog to enter command line arguments before running
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
- `Settings_ApplyAutorun` — adds/removes the app from `HKCU\...\Run` registry key
- `SettingsDlgProc` — dialog proc for `IDD_SETTINGS`; includes Reset to Defaults
- `AboutDlgProc` — dialog proc for `IDD_ABOUT`

### `updater.c`
Checks GitHub Releases API for newer versions.

- `Updater_CheckThread` — background thread: waits 3 s for sync to finish (skipped for manual checks), calls releases API, compares `major.minor.patch`; pass non-NULL `lpParam` to trigger a manual check that shows "up to date" status if no update is found
- `Updater_AutoUpdate` — downloads the new `.exe` directly via WinINet, writes a batch script to replace the binary, then posts `WM_CLOSE`; falls back to `Updater_PromptAndInstall` on any failure
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

Internal key functions:
- `QuickBarProc` — bar window procedure; handles drag (background = drag handle), click, hover, scroll arrows, mouse wheel, right-click context menu, and `VK_ESCAPE` (calls `Repeat_Stop()` and `Runner_Stop()` so Escape cancels both repeat and the running script)
- `QBarTipProc` — tooltip window procedure; paints script name (bold) and purpose line (small) with double-buffering
- `QB_Paint` — double-buffered render of bar background, scroll arrows (▲▼ / ◄►), and all 2-letter abbreviation buttons with accent bar on hover
- `QB_HitTest` — returns button index (≥0), `HIT_ARROW_PREV/NEXT`, or `HIT_NONE` (background/drag area)
- `QB_UpdateGeometry` — sizes the bar window to fit visible favourites, clamped to 4/5 of the monitor's work area; calls `QB_UpdateScrollMax`
- `QB_CatiaState` — enumerates all top-level windows via `EnumWindows` and returns `CATIA_NONE`, `CATIA_MINIMIZED`, or `CATIA_VISIBLE` based on the tracked title substring
- `QB_UpdateVisibility` — central dispatcher called from both WinEvent hooks; hides bar when target is absent/minimised, sets `HWND_TOPMOST` when target gains focus

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
    volatile HANDLE run_process;  /* duplicated handle of bg script process; NULL when idle */

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

    /* Double-click repeat mode */
    bool   repeat_mode;          /* true while a script is looping */
    int    repeat_fi;            /* folder index of the script to repeat */
    int    repeat_si;            /* script index of the script to repeat */
    bool   suppress_lbuttonup;   /* suppresses extra WM_LBUTTONUP after WM_LBUTTONDBLCLK */

    /* Running state (background mode only) */
    bool   script_running;       /* true while a background script is in flight */
    int    run_fi;               /* folder index of the running script */
    int    run_si;               /* script index of the running script */
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
    bool      repeat_on_dblclick;        /* repeat main-window scripts on double-click */
    bool      qbar_repeat_on_dblclick;   /* repeat Quick Bar scripts on double-click */
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
| `HTTP_BUF_SIZE` | 512 KB | HTTP response read buffer |

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
| `WM_SCRIPT_STARTED` | `WM_USER+13` | Runner → Main | Background script launched; enables the Stop button and turns the running button green |
| `WM_SCRIPT_STOPPED` | `WM_USER+14` | Runner → Main | Background script exited or was terminated; disables the Stop button and clears the green highlight |

### Timer IDs

| Constant | Value | Description |
|----------|-------|-------------|
| `TIMER_AUTO_REFRESH` | 1001 | `SetTimer` ID for the periodic auto-sync interval |
| `TIMER_QBAR` | 1002 | `SetTimer` ID used by the Quick Launch Bar for visibility polling |

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
