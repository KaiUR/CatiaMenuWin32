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
- `Handle_SyncDone` — called on main thread when sync completes; rebuilds tabs
- `App_InitGDI` / `App_FreeGDI` / `App_RebuildGDI` — create/destroy/recreate all GDI resources
- `App_ResolveTheme` — sets `g.dark_mode` based on `cfg.theme` and Windows registry

### `resource.h`
All `#define` IDs for menus, dialogs, and controls. Grouped by:
- Icon: `IDI_APP_ICON`
- Menu: `IDR_MAINMENU`, `IDM_*`
- Dialogs: `IDD_SETTINGS`, `IDD_ABOUT`, `IDD_SOURCES`, `IDD_REPO_EDIT`
- Controls: `IDC_*`
- Script buttons: `IDC_SCRIPT_BTN_BASE = 1000` (script index added to base)

### `window.c`
Window creation, tab bar, tray icon, and popup menu.

- `Window_Create` — registers all window classes, creates main window and all child controls
- `Window_OnSize` — repositions tab bar, scroll panel, and status bar on resize
- `Window_ApplyDarkMode` — calls `DwmSetWindowAttribute` for title bar dark mode
- `Window_ApplyAlwaysOnTop` — calls `SetWindowPos` with `HWND_TOPMOST`/`HWND_NOTOPMOST`
- `Window_ShowMenu` — builds and shows the hamburger popup menu via `TrackPopupMenu`
- `Window_AddTrayIcon` / `Window_RemoveTrayIcon` — manage `NOTIFYICONDATA`
- `TabBarProc` — fully custom tab bar `WndProc`; draws tabs at natural text width, handles scroll arrows and mouse wheel
- `TabBar_NaturalWidth` — measures tab label width with `GetTextExtentPoint32W`
- `TabBar_NeedsArrows` — returns true when total tab width exceeds bar width
- `StatusBarProc` — custom owner-drawn status bar `WndProc`

### `tabs.c`
Tab switching and script button management.

- `Tabs_Build` — triggers tab bar repaint (tab bar reads `g.folders[]` directly)
- `Tabs_Switch` — sets `g.active_tab`, repaints tab bar, calls `Tabs_RebuildButtons`
- `Tabs_RebuildButtons` — destroys existing script buttons and creates new ones for active tab
- `Tabs_DestroyButtons` — removes all `IDC_SCRIPT_BTN_BASE+n` child windows
- `ScrollPanelProc` — scroll panel `WndProc`; handles `WM_VSCROLL`, `WM_MOUSEWHEEL`, `WM_DRAWITEM` for script buttons

### `paint.c`
All GDI rendering. Every function uses double-buffering (memory DC + `BitBlt`).

- `Paint_MainWindow` — draws toolbar background, title, version, syncing indicator, update badge
- `Paint_ToolbarButton` — owner-draw for `BS_OWNERDRAW` toolbar buttons (Menu, Refresh, Settings, Deps)
- `Paint_ScriptButton` — draws a script button with accent bar, arrow, label, purpose text, and `i` info badge
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

- `Sync_Thread` — background thread: fetches root, parses folders, downloads changed scripts, syncs extra repos, scans local dirs, saves manifest, posts `WM_SYNC_DONE`
- `Sync_LoadManifest` — called at startup: scans `cache_dir` on disk to populate `g.folders[]` immediately without internet
- `Sync_SaveManifest` — writes all current SHA values to `manifest.ini`
- `Sync_GetLocalSHA` — reads a script's SHA from `manifest.ini`
- `Sync_MergeFolder` — adds scripts to an existing folder or creates a new one; used for merging sources
- `Sync_ExtraRepo` — syncs one extra GitHub repository
- `Sync_LocalDir` — scans a local folder, treats subfolders as tabs, skips `setup/`
- `DeleteLocalScript` — deletes a cached script file and removes empty parent directories

### `runner.c`
Script execution and dependency management.

- `Runner_Run` — verifies SHA, finds Python, launches script in a thread
- `Runner_Thread` — creates process for `python script.py` (with optional `cmd /k` wrapper)
- `Runner_FindPython` — searches PATH, `cfg.python_exe`, and common install locations
- `Runner_UpdateDeps` — runs `pip install -r requirements.txt` for each source sequentially
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

- `Updater_CheckThread` — background thread: waits 3s for sync to finish, calls releases API, compares `major.minor.patch`
- `IsNewer` — compares remote and local versions (first 3 parts only, ignores build number)
- `ParseVersion` — splits `"1.2.3.4"` into an `int[4]` without locale-dependent `swscanf`
- `ParseLatestTag` — extracts `tag_name` from releases API JSON
- `Updater_PromptAndInstall` — shows the update dialog and opens the releases page if confirmed

---

## Key Structs

### `AppState` (`g`)
The single global state struct. All state lives here.

```c
typedef struct {
    HWND   hwnd;           /* main window */
    HWND   hwnd_tab;       /* custom tab bar (CMW32TabBar) */
    HWND   hwnd_scroll;    /* scroll panel containing script buttons */
    HWND   hwnd_status;    /* custom status bar (CMW32StatusBar) */
    HWND   hwnd_tip;       /* tooltip popup */

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
    int    hot_btn;        /* currently hovered script button ID */
    int    tip_btn;        /* button whose tooltip is showing */
    WCHAR  latest_version[32];   /* from GitHub releases API */
    bool   tray_icon_added;
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
    ThemeMode theme;          /* THEME_SYSTEM=0, THEME_DARK=1, THEME_LIGHT=2 */
    bool      main_repo_enabled;
    ExtraRepo extra_repos[MAX_EXTRA_REPOS];
    int       extra_repo_count;
    LocalDir  local_dirs[MAX_LOCAL_DIRS];
    int       local_dir_count;
} Settings;
```

### `ScriptFolder`
One tab worth of scripts.

```c
typedef struct {
    WCHAR  name[MAX_NAME];      /* raw folder name e.g. "Any_Document_Scripts" */
    WCHAR  display[MAX_NAME];   /* formatted e.g. "Any Document Scripts" */
    Script scripts[MAX_SCRIPTS];
    int    count;
    bool   loaded;
} ScriptFolder;
```

### `Script`
One script button.

```c
typedef struct {
    WCHAR      name[MAX_NAME];      /* display name */
    WCHAR      gh_path[MAX_APPPATH]; /* GitHub API path e.g. "folder/script.py" */
    WCHAR      sha[MAX_SHA];         /* expected blob SHA from GitHub API */
    WCHAR      local[MAX_APPPATH];   /* local cache path */
    ScriptMeta meta;
    bool       meta_loaded;
} Script;
```

---

## Constants and Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FOLDERS` | 64 | Maximum number of tabs |
| `MAX_SCRIPTS` | 128 | Maximum scripts per folder |
| `MAX_EXTRA_REPOS` | 8 | Maximum extra GitHub repos |
| `MAX_LOCAL_DIRS` | 8 | Maximum local folders |
| `TOOLBAR_H` | 38 | Toolbar height in pixels |
| `TAB_H` | 26 | Tab bar height in pixels |
| `TAB_ARROW_W` | 22 | Tab scroll arrow width |
| `STATUS_H` | 22 | Status bar height |
| `INFO_BTN_W` | 28 | Width of the `i` info badge |
| `TIP_W` | 320 | Tooltip popup width |
| `HTTP_BUF_SIZE` | 512KB | HTTP response buffer |
| `WIN_MIN_W` | 820 | Minimum window width |

---

## Message IDs

| Message | Value | Direction | Description |
|---------|-------|-----------|-------------|
| `WM_SYNC_DONE` | `WM_USER+1` | Thread → Main | Sync completed; `wParam` is `SyncResult*` |
| `WM_STATUS_SET` | `WM_USER+2` | Thread → Main | Update status bar; `lParam` is heap `WCHAR*` |
| `WM_TRAYICON` | `WM_USER+10` | System → Main | Tray icon mouse event |
| `WM_UPDATE_AVAIL` | `WM_USER+11` | Thread → Main | Newer version found |

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

| Function | Dark | Light | Used for |
|----------|------|-------|----------|
| `COL_BG()` | `#1C1C23` | `#F0F0F5` | Main window background |
| `COL_TOOLBAR()` | `#141418` | `#D2D2DC` | Toolbar and tab bar |
| `COL_BTN_NORM()` | `#2C2E40` | `#DCDEED` | Script button normal |
| `COL_BTN_HOT()` | `#3E415A` | `#BEC3DC` | Script button hover |
| `COL_BTN_PRESS()` | `#1C1E30` | `#AAAFCD` | Script button pressed |
| `COL_ACCENT` | `RGB(82,155,245)` | same | Highlights, selected tabs |
| `COL_TEXT()` | `#D2D7F0` | `#1E1E28` | Primary text |
| `COL_SUBTEXT()` | `#6E7494` | `#646478` | Secondary text, purposes |
| `COL_DIVIDER()` | `#2E3042` | `#BEC0D2` | Separators and borders |
| `COL_TIP_BG()` | `#161620` | `#F5F5FC` | Tooltip background |
