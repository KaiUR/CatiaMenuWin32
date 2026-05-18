#ifndef MAIN_H
#define MAIN_H

/*
 * main.h  -  CatiaMenuWin32
 * Central header.
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <wininet.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "resource.h"
#include "version.h"

/* ------------------------------------------------------------------ */
/*  App strings                                                         */
/* ------------------------------------------------------------------ */
#define APP_CLASS        L"CatiaMenuWin32Class"
#define APP_TITLE        L"CATIA Macro Menu"
#define APP_APPDATA_DIR  L"CatiaMenuWin32"
#define SETTINGS_FILE    L"settings.ini"
#define MANIFEST_FILE    L"manifest.ini"
#define PREFS_FILE       L"prefs.ini"
#define GITHUB_OWNER     L"KaiUR"
#define GITHUB_REPO      L"Pycatia_Scripts"
#define GITHUB_BRANCH    L"main"
#define GITHUB_API_HOST  L"api.github.com"
#define GITHUB_RAW_HOST  L"raw.githubusercontent.com"
#define AUTORUN_KEY      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTORUN_NAME     L"CatiaMenuWin32"

/* ------------------------------------------------------------------ */
/*  Layout                                                              */
/* ------------------------------------------------------------------ */
#define WIN_MIN_W        820
#define WIN_MIN_H        420
#define TOOLBAR_H        38
#define TAB_H            26
#define TAB_ARROW_W      22
#define STATUS_H         22
#define BTN_H            40
#define BTN_GAP          6
#define BTN_MX           12
#define BTN_MY           10
#define SCROLL_STEP      40
#define INFO_BTN_W       28
#define STAR_BTN_W       28   /* width of favourite star badge          */
#define TIP_W            320
#define TIP_ROW_H        18
#define TIP_HEADER_ROWS  5
#define SEARCH_H         26   /* height of search/filter box            */

/* Quick Launch Bar */
#define QBAR_BTN_SIZE    52   /* button face square (px)                 */
#define QBAR_PAD          4   /* margin from bar edge to buttons         */
#define QBAR_GAP          4   /* gap between adjacent buttons            */
#define QBAR_ARROW_W     18   /* scroll arrow click area width/height    */
#define QBAR_TIP_W       240  /* tooltip popup width                     */
#define QBAR_TIP_PAD      8   /* tooltip internal padding                */
#define QBAR_TIP_ROW     18   /* tooltip row height                      */

/* ------------------------------------------------------------------ */
/*  Limits                                                              */
/* ------------------------------------------------------------------ */
#define MAX_FOLDERS       64
#define MAX_SCRIPTS       1024  /* default heap capacity per folder — grows as needed */
#define MAX_NAME          128
#define MAX_SHA           64
#define MAX_APPPATH       520
#define HTTP_BUF_SIZE     (512 * 1024)
#define MAX_EXTRA_REPOS   8
#define MAX_LOCAL_DIRS    8
#define MAX_FAVOURITES    256
#define MAX_HIDDEN        256
#define MAX_NOTE_LEN      512

/* ------------------------------------------------------------------ */
/*  Messages                                                            */
/* ------------------------------------------------------------------ */
#define WM_SYNC_DONE       (WM_USER + 1)
#define WM_STATUS_SET      (WM_USER + 2)
#define WM_TRAYICON        (WM_USER + 10)
#define WM_UPDATE_AVAIL    (WM_USER + 11)
#define WM_AUTO_REFRESH    (WM_USER + 12)
#define WM_SCRIPT_STARTED  (WM_USER + 13)  /* posted by Runner_Thread when a bg script starts */
#define WM_SCRIPT_STOPPED  (WM_USER + 14)  /* posted when bg script exits or is terminated   */
#define TRAY_ID            1
#define TIMER_AUTO_REFRESH 1001
#define TIMER_QBAR         1002

/* ================================================================== */
/*  SortMode                                                            */
/*  Purpose: Enumerates the available script sort orders.              */
/*  In:  (enum values used as g.cfg.sort_mode or folder.sort_mode)    */
/*  Out: (no return — used as a value type)                            */
/* ================================================================== */
typedef enum {
    SORT_ORDER    = 0,  /* order from GitHub API / disk                */
    SORT_ALPHA    = 1,  /* alphabetical A-Z                            */
    SORT_DATE     = 2,  /* by script header Date: field                */
    SORT_MOST_USED = 3  /* by run count descending                     */
} SortMode;

/* ================================================================== */
/*  ThemeMode                                                           */
/*  Purpose: Enumerates the three UI theme choices stored in Settings. */
/*  In:  (enum values used as g.cfg.theme)                             */
/*  Out: (no return — used as a value type)                            */
/* ================================================================== */
typedef enum { THEME_SYSTEM = 0, THEME_DARK = 1, THEME_LIGHT = 2 } ThemeMode;

/* ================================================================== */
/*  SyncStatus / SyncResult                                             */
/*  Purpose: Communicates the outcome of a background sync operation   */
/*           from Sync_Thread to the main window via WM_SYNC_DONE.    */
/*  In:  (populated by Sync_Thread before PostMessage)                 */
/*  Out: (consumed by Handle_SyncDone on the UI thread)                */
/* ================================================================== */
typedef enum { SR_OK=0, SR_NO_INTERNET, SR_API_ERROR, SR_PARTIAL } SyncStatus;
typedef struct {
    SyncStatus status;
    int folders_added, folders_removed;
    int scripts_updated, scripts_added, scripts_removed;
    WCHAR message[256];
} SyncResult;

/* ================================================================== */
/*  ScriptMeta                                                          */
/*  Purpose: Holds the key-value metadata parsed from a script's       */
/*           header block (Purpose:, Author:, Version:, Date:, etc.)  */
/*  In:  (populated by Meta_Parse from the script's local .py file)   */
/*  Out: (read by Paint_Tooltip, ScriptDetailsDlgProc, Tabs_ApplySort)*/
/* ================================================================== */
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

/* ================================================================== */
/*  Script                                                              */
/*  Purpose: Represents a single script file with its identity,        */
/*           cached SHA, local path, parsed metadata, and user prefs. */
/*  In:  (populated by Sync_Thread / Sync_LoadManifest)               */
/*  Out: (consumed by Runner_Run, Meta_Parse, Paint_ScriptButton, etc.)*/
/* ================================================================== */
typedef struct {
    WCHAR      name[MAX_NAME];
    WCHAR      gh_path[MAX_APPPATH];
    WCHAR      sha[MAX_SHA];
    WCHAR      local[MAX_APPPATH];
    ScriptMeta meta;
    bool       meta_loaded;
    bool       is_favourite;
    bool       is_hidden;
    int        run_count;
    WCHAR      note[MAX_NOTE_LEN];
} Script;

/* ================================================================== */
/*  ScriptFolder                                                        */
/*  Purpose: Groups scripts that share a top-level folder name into    */
/*           one tab.  The scripts array is heap-allocated and grows   */
/*           dynamically via Folder_Push.                              */
/*  In:  (populated by Sync_MergeFolder / Sync_LocalDir)              */
/*  Out: (consumed by Tabs_Build, Tabs_Switch, Runner_Run, paint.c)   */
/* ================================================================== */
typedef struct {
    WCHAR   name[MAX_NAME];
    WCHAR   display[MAX_NAME];
    Script *scripts;       /* heap allocated — use Folder_Alloc/Folder_Free */
    int     count;
    int     capacity;      /* allocated slots */
    bool    loaded;
    SortMode sort_mode;
} ScriptFolder;

/* ------------------------------------------------------------------ */
/*  ScriptFolder heap helpers                                           */
/* ------------------------------------------------------------------ */

/* ================================================================== */
/*  Folder_Alloc                                                        */
/*  Purpose: Allocates the scripts array for a ScriptFolder with the   */
/*           requested initial capacity (minimum 64 slots).            */
/*  In:  f        — folder to initialise (must be zero-initialised)   */
/*       capacity — initial slot count; clamped to 64 if <= 0         */
/*  Out: true on success; false if malloc fails (f remains unchanged)  */
/* ================================================================== */
static inline bool Folder_Alloc(ScriptFolder *f, int capacity)
{
    if (capacity <= 0) capacity = 64;
    Script *p = (Script *)malloc((size_t)capacity * sizeof(Script));
    if (!p) return false;
    ZeroMemory(p, (size_t)capacity * sizeof(Script));
    f->scripts  = p;
    f->capacity = capacity;
    f->count    = 0;
    return true;
}

/* ================================================================== */
/*  Folder_Free                                                         */
/*  Purpose: Releases the heap-allocated scripts array of a folder     */
/*           and resets count and capacity to zero.                    */
/*  In:  f — folder whose scripts buffer should be freed               */
/*  Out: (void) — f->scripts is set to NULL after freeing              */
/* ================================================================== */
static inline void Folder_Free(ScriptFolder *f)
{
    if (f->scripts) { free(f->scripts); f->scripts = NULL; }
    f->count    = 0;
    f->capacity = 0;
}

/* ================================================================== */
/*  Folder_Push                                                         */
/*  Purpose: Appends a new zero-initialised Script slot to a folder,   */
/*           auto-allocating or doubling the buffer when full.         */
/*  In:  f — target folder (Folder_Alloc not required beforehand)      */
/*  Out: pointer to the new Script slot on success; NULL on OOM        */
/* ================================================================== */
static inline Script *Folder_Push(ScriptFolder *f)
{
    if (!f->scripts) {
        if (!Folder_Alloc(f, 64)) return NULL;
    }
    if (f->count >= f->capacity) {
        int new_cap = f->capacity * 2;
        Script *p = (Script *)realloc(f->scripts,
                                      (size_t)new_cap * sizeof(Script));
        if (!p) return NULL;
        ZeroMemory(p + f->capacity,
                   (size_t)(new_cap - f->capacity) * sizeof(Script));
        f->scripts  = p;
        f->capacity = new_cap;
    }
    return &f->scripts[f->count++];
}


/* ================================================================== */
/*  ExtraRepo                                                           */
/*  Purpose: Describes one user-added GitHub repository script source. */
/*  In:  (loaded from settings.ini by Settings_Load)                   */
/*  Out: (passed to Sync_ExtraRepo during each sync cycle)             */
/* ================================================================== */
typedef struct {
    WCHAR        url[512];
    WCHAR        branch[64];
    WCHAR        token[256];
    bool         enabled;
} ExtraRepo;

/* ================================================================== */
/*  LocalDir                                                            */
/*  Purpose: Describes one user-added local folder script source.      */
/*  In:  (loaded from settings.ini by Settings_Load)                   */
/*  Out: (passed to Sync_LocalDir during each sync cycle)              */
/* ================================================================== */
typedef struct {
    WCHAR        path[MAX_APPPATH];
    bool         enabled;
} LocalDir;

/* ================================================================== */
/*  Settings                                                            */
/*  Purpose: Holds every user-configurable option for the application. */
/*           Persisted to %APPDATA%\CatiaMenuWin32\settings.ini.      */
/*  In:  (populated by Settings_Load at startup)                       */
/*  Out: (written back by Settings_Save on change or exit)             */
/* ================================================================== */
typedef struct {
    WCHAR     python_exe[MAX_APPPATH];
    WCHAR     cache_dir[MAX_APPPATH];
    WCHAR     github_token[256];
    bool      auto_sync;
    bool      download_before_run;
    bool      show_console;
    bool      console_keep_open;
    bool      deps_keep_open;
    bool      always_on_top;
    bool      minimize_to_tray;
    bool      start_with_windows;
    bool      start_minimized;
    bool      check_updates;
    bool      auto_update;       /* auto-download and install updates   */
    ThemeMode theme;
    int       refresh_interval;  /* hours, 0 = disabled, default 6     */
    /* Sources */
    bool      main_repo_enabled;
    ExtraRepo extra_repos[MAX_EXTRA_REPOS];
    int       extra_repo_count;
    LocalDir  local_dirs[MAX_LOCAL_DIRS];
    int       local_dir_count;
    /* Sort */
    SortMode  sort_mode;         /* global sort mode                    */
    /* Quick Launch Bar */
    bool      qbar_enabled;
    bool      qbar_horizontal;
    bool      qbar_topmost_with_catia;
    int       qbar_x;
    int       qbar_y;
    WCHAR     qbar_target_app[MAX_NAME]; /* window-title substring; empty = no target      */
    WCHAR     qbar_target_exe[MAX_NAME]; /* process exe name (e.g. CNEXT.exe); empty = any */
    /* Double-click repeat */
    bool      repeat_on_dblclick;        /* repeat main-window scripts on double-click (default: true) */
    bool      qbar_repeat_on_dblclick;   /* repeat Quick Bar scripts on double-click (default: true)   */
} Settings;

/* ------------------------------------------------------------------ */
/*  Colours                                                             */
/* ------------------------------------------------------------------ */
#define COL_BG_DARK        RGB(28,  28,  35)
#define COL_BG_LIGHT       RGB(240, 240, 245)
#define COL_TOOLBAR_DARK   RGB(20,  20,  28)
#define COL_TOOLBAR_LIGHT  RGB(210, 210, 220)
#define COL_BTN_NORM_DARK  RGB(44,  46,  64)
#define COL_BTN_NORM_LIGHT RGB(220, 222, 235)
#define COL_BTN_HOT_DARK   RGB(62,  65,  90)
#define COL_BTN_HOT_LIGHT  RGB(190, 195, 220)
#define COL_BTN_PRESS_DARK  RGB(28, 30,  48)
#define COL_BTN_PRESS_LIGHT RGB(170,175,205)
#define COL_INFO_ZONE_DARK  RGB(36, 38,  55)
#define COL_INFO_ZONE_LIGHT RGB(200,202,220)
#define COL_ACCENT         RGB(82, 155, 245)
#define COL_ACCENT_DIM     RGB(48,  92, 160)
#define COL_SUCCESS        RGB(80, 200, 120)
#define COL_WARN           RGB(240, 190,  60)
#define COL_STAR           RGB(255, 200,  60)   /* favourite star      */
#define COL_TEXT_DARK      RGB(210, 215, 240)
#define COL_TEXT_LIGHT     RGB(30,  30,  40)
#define COL_SUBTEXT_DARK   RGB(110, 116, 148)
#define COL_SUBTEXT_LIGHT  RGB(100, 100, 120)
#define COL_DIVIDER_DARK   RGB(46,  48,  66)
#define COL_DIVIDER_LIGHT  RGB(190, 192, 210)
#define COL_TIP_BG_DARK    RGB(22,  22,  32)
#define COL_TIP_BG_LIGHT   RGB(245, 245, 252)
#define COL_TIP_BORDER     RGB(82, 155, 245)

/* ================================================================== */
/*  AppState                                                            */
/*  Purpose: Single global structure holding all runtime state —       */
/*           window handles, folders, GDI resources, and UI state.    */
/*           Declared as extern AppState g; and zero-initialised in   */
/*           main.c.  No other global variables exist.                */
/*  In:  (initialised at startup, updated by all modules)             */
/*  Out: (read by all modules via the global `g` instance)            */
/* ================================================================== */
typedef struct {
    HWND   hwnd;
    HWND   hwnd_tab;
    HWND   hwnd_scroll;
    HWND   hwnd_status;
    HWND   hwnd_tip;
    HWND   hwnd_search;   /* search/filter edit box                    */
    HWND   hwnd_details;  /* script details panel                      */

    ScriptFolder folders[MAX_FOLDERS];
    int      folder_count;
    int      active_tab;
    int      tab_offset;
    Settings cfg;
    bool     dark_mode;

    HFONT  font_ui;
    HFONT  font_bold;
    HFONT  font_small;
    HBRUSH br_bg;
    HBRUSH br_toolbar;
    HBRUSH br_btn;
    HBRUSH br_btn_hot;
    HBRUSH br_accent;
    HBRUSH br_status;

    bool   syncing;
    int    hot_btn;
    int    tip_btn;
    int    tip_h;
    WCHAR  last_run_path[MAX_APPPATH];
    WCHAR  appdata_dir[MAX_APPPATH];
    WCHAR  latest_version[32];
    WCHAR  active_folder_name[MAX_NAME]; /* name of active folder for post-sync restoration */
    int    scroll_y;
    int    scroll_max;
    bool   tray_icon_added;

    CRITICAL_SECTION cs_folders; /* guards g.folders[] and g.folder_count */

    /* Running script — NULL when idle; set/cleared atomically via InterlockedExchangePointer */
    volatile HANDLE run_process;

    /* Filter */
    WCHAR  filter_text[MAX_NAME]; /* current search/filter string       */

    /* Details panel */
    int    details_script_fi;    /* folder index of shown script        */
    int    details_script_si;    /* script index of shown script        */
    bool   details_visible;

    /* Quick Launch Bar */
    HWND   hwnd_qbar;            /* floating bar window                 */
    HWND   hwnd_qbar_tip;        /* bar tooltip popup                   */
    int    qbar_hot;             /* hovered button index, -1 = none     */
    int    qbar_scroll;          /* current scroll offset (px)          */
    int    qbar_scroll_max;      /* maximum scroll offset               */
    bool   qbar_dragging;        /* bar is being dragged                */
    int    qbar_drag_ox;         /* drag start: cursor offset from left  */
    int    qbar_drag_oy;         /* drag start: cursor offset from top   */
    int    qbar_tip_idx;         /* button index shown in tip, -1 = none */

    /* Double-click repeat mode */
    bool   repeat_mode;          /* true = re-run script after each completion  */
    int    repeat_fi;            /* folder index of the script to repeat        */
    int    repeat_si;            /* script index of the script to repeat        */
    bool   suppress_lbuttonup;   /* suppress the extra LBUTTONUP after dblclick */
    HHOOK  kbd_repeat_hook;      /* low-level keyboard hook active during repeat */

} AppState;

extern AppState g;

/* ------------------------------------------------------------------ */
/*  DWM dark mode                                                       */
/* ------------------------------------------------------------------ */
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* ================================================================== */
/*  Runtime colour accessors  (COL_BG … COL_TIP_BG)                   */
/*  Purpose: Return the correct COLORREF for the current theme.        */
/*           All GDI code must call these instead of hardcoding RGB.  */
/*  In:  (none — reads g.dark_mode)                                    */
/*  Out: COLORREF — theme-appropriate colour value                     */
/* ================================================================== */
static inline COLORREF COL_BG(void)        { return g.dark_mode ? COL_BG_DARK        : COL_BG_LIGHT;        }
static inline COLORREF COL_TOOLBAR(void)   { return g.dark_mode ? COL_TOOLBAR_DARK   : COL_TOOLBAR_LIGHT;   }
static inline COLORREF COL_BTN_NORM(void)  { return g.dark_mode ? COL_BTN_NORM_DARK  : COL_BTN_NORM_LIGHT;  }
static inline COLORREF COL_BTN_HOT(void)   { return g.dark_mode ? COL_BTN_HOT_DARK   : COL_BTN_HOT_LIGHT;   }
static inline COLORREF COL_BTN_PRESS(void) { return g.dark_mode ? COL_BTN_PRESS_DARK : COL_BTN_PRESS_LIGHT; }
static inline COLORREF COL_INFO_ZONE(void) { return g.dark_mode ? COL_INFO_ZONE_DARK : COL_INFO_ZONE_LIGHT; }
static inline COLORREF COL_TEXT(void)      { return g.dark_mode ? COL_TEXT_DARK      : COL_TEXT_LIGHT;      }
static inline COLORREF COL_SUBTEXT(void)   { return g.dark_mode ? COL_SUBTEXT_DARK   : COL_SUBTEXT_LIGHT;   }
static inline COLORREF COL_DIVIDER(void)   { return g.dark_mode ? COL_DIVIDER_DARK   : COL_DIVIDER_LIGHT;   }
static inline COLORREF COL_TIP_BG(void)    { return g.dark_mode ? COL_TIP_BG_DARK    : COL_TIP_BG_LIGHT;    }

/* ------------------------------------------------------------------ */
/*  Prototypes                                                          */
/* ------------------------------------------------------------------ */
/* main.c */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
void App_InitGDI(void);
void App_FreeGDI(void);
void App_RebuildGDI(void);
void App_BuildAppDataPath(void);
void App_ResolveTheme(void);

/* window.c */
void Window_Create(HINSTANCE);
void Window_OnSize(int w, int h);
void Window_ApplyDarkMode(HWND hwnd);
void Window_ApplyDarkMenu(HWND hwnd);
void Window_ApplyAlwaysOnTop(void);
void Window_AddTrayIcon(void);
void Window_RemoveTrayIcon(void);
void Window_ShowTrayMenu(void);
void Window_ShowMenu(void);
void Window_ApplyThemeToChildren(HWND hwnd);
void Window_OpenExeFolder(void);

/* tabs.c */
void Tabs_Build(void);
void Tabs_Switch(int idx);
void Tabs_RebuildButtons(void);
void Tabs_DestroyButtons(void);
void Tabs_ApplyFilter(void);
bool Tabs_ScriptMatchesFilter(const Script *s);
void Tabs_ApplySort(int fi);
bool Tabs_FolderHasVisible(int fi);
LRESULT CALLBACK ScrollPanelProc(HWND, UINT, WPARAM, LPARAM);

/* github.c */
bool GitHub_HttpGet(const WCHAR *host, const WCHAR *path,
                    const WCHAR *token, char *buf, DWORD *len);
void GitHub_ParseRoot(const char *json);
void GitHub_ParseFolder(const char *json, int fi);
bool GitHub_DownloadRaw(const WCHAR *gh_path, const WCHAR *local_path,
                        const WCHAR *token);
bool GitHub_DownloadRawFull(const WCHAR *host, const WCHAR *path,
                             const WCHAR *local_path, const WCHAR *token);
bool GitHub_ComputeFileSHA1(const WCHAR *local_path, WCHAR *sha_out, int max);
bool GitHub_VerifyScriptSHA(const Script *s);
bool GitHub_ParseOwnerRepo(const WCHAR *url, WCHAR *owner, WCHAR *repo);

/* sync.c */
DWORD WINAPI Sync_Thread(LPVOID);
void  Sync_LoadManifest(void);
void  Sync_SaveManifest(void);
bool  Sync_GetLocalSHA(const WCHAR *gh_path, WCHAR *sha_out);
void  Sync_MergeFolder(const WCHAR *folder_name, Script *scripts, int count);

/* meta.c */
void Meta_Parse(Script *s);
void Meta_ParseAll(void);

/* runner.c */
bool Runner_Run(int fi, int si);
bool Runner_RunWithArgs(int fi, int si, const WCHAR *args);
bool Runner_FindPython(WCHAR *out, int max);
void Runner_UpdateDeps(void);
void Runner_Stop(void);

/* Repeat-mode helpers (main.c) */
void Repeat_Start(int fi, int si); /* activate repeat + install global ESC hook */
void Repeat_Stop(void);            /* cancel repeat + remove hook + repaint      */

/* updater.c */
DWORD WINAPI Updater_CheckThread(LPVOID);
void  Updater_PromptAndInstall(const WCHAR *latest_tag);
void  Updater_AutoUpdate(const WCHAR *latest_tag);

/* settings.c */
void Settings_Load(Settings *s);
void Settings_Save(const Settings *s);
void Settings_ApplyAutorun(bool enable, bool minimized);
INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);

/* help.c */
void Help_Show(void);

/* sources.c */
INT_PTR CALLBACK SourcesDlgProc(HWND, UINT, WPARAM, LPARAM);

/* prefs.c */
void Prefs_Load(void);
void Prefs_Save(void);
bool Prefs_IsFavourite(const WCHAR *gh_path);
void Prefs_SetFavourite(const WCHAR *gh_path, bool fav);
bool Prefs_IsHidden(const WCHAR *gh_path);
void Prefs_SetHidden(const WCHAR *gh_path, bool hidden);
int  Prefs_GetRunCount(const WCHAR *gh_path);
void Prefs_IncrementRunCount(const WCHAR *gh_path);
void Prefs_GetNote(const WCHAR *gh_path, WCHAR *note, int max);
void Prefs_SetNote(const WCHAR *gh_path, const WCHAR *note);
void Prefs_ApplyToFolders(void);
void Tabs_BuildFavourites(void);
INT_PTR CALLBACK ScriptDetailsDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK RunWithArgsDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ScriptNoteDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK HiddenScriptsDlgProc(HWND, UINT, WPARAM, LPARAM);

/* quickbar.c */
void QuickBar_Register(HINSTANCE hInst);
void QuickBar_Create(void);
void QuickBar_Destroy(void);
void QuickBar_Show(bool show);
void QuickBar_Rebuild(void);
void QuickBar_OnThemeChange(void);
void QuickBar_SetTopmost(bool topmost);
void QuickBar_ShowTargetDlg(void);

/* paint.c */
void Paint_MainWindow(HWND, HDC);
void Paint_ToolbarButton(DRAWITEMSTRUCT *dis);
void Paint_ScriptButton(HWND, HDC, bool hot, bool pressed,
                        bool info_hot, bool repeat, const Script *s);
void Paint_Tooltip(HWND hwnd);
LRESULT CALLBACK BtnSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

/* ------------------------------------------------------------------ */
/*  Inline helpers                                                      */
/* ------------------------------------------------------------------ */

/* ================================================================== */
/*  Util_StripExt                                                       */
/*  Purpose: Removes the file extension from a wide string in-place   */
/*           by replacing the last '.' with a null terminator.        */
/*  In:  s — wide string (modified in-place); no-op if no '.' found   */
/*  Out: (void)                                                         */
/* ================================================================== */
static inline void Util_StripExt(WCHAR *s) {
    WCHAR *d = wcsrchr(s, L'.'); if (d) *d = L'\0';
}

/* ================================================================== */
/*  Util_SnakeToTitle                                                   */
/*  Purpose: Converts a snake_case wide string to Title Case in-place. */
/*           Underscores become spaces; each word's first letter is    */
/*           uppercased.                                               */
/*  In:  s — wide string (modified in-place)                           */
/*  Out: (void)                                                         */
/* ================================================================== */
static inline void Util_SnakeToTitle(WCHAR *s) {
    bool cap = true;
    for (WCHAR *p = s; *p; p++) {
        if (*p == L'_')             { *p = L' '; cap = true; }
        else if (cap && *p != L' ') { *p = (WCHAR)towupper(*p); cap = false; }
    }
}

/* ================================================================== */
/*  Util_Log                                                            */
/*  Purpose: Writes a formatted debug message to OutputDebugStringW.  */
/*           Output is visible in Qt Creator's Application Output pane */
/*           and in DebugView.  Compiled out in Release if desired.   */
/*  In:  fmt — printf-style wide format string                         */
/*       ... — variadic arguments                                       */
/*  Out: (void)                                                         */
/* ================================================================== */
static inline void Util_Log(const WCHAR *fmt, ...) {
    WCHAR buf[512];
    va_list va; va_start(va, fmt);
    _vsnwprintf_s(buf, 512, _TRUNCATE, fmt, va); va_end(va);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
}

/* ================================================================== */
/*  PostStatus                                                          */
/*  Purpose: Thread-safe status bar update.  Allocates a heap buffer,  */
/*           formats the message, then posts WM_STATUS_SET to the main */
/*           window.  The UI thread frees the buffer on receipt.      */
/*  In:  fmt — printf-style wide format string                         */
/*       ... — variadic arguments                                       */
/*  Out: (void) — message is delivered asynchronously                  */
/* ================================================================== */
static inline void PostStatus(const WCHAR *fmt, ...) {
    WCHAR *buf = (WCHAR *)malloc(256 * sizeof(WCHAR));
    if (!buf) return;
    va_list va; va_start(va, fmt);
    _vsnwprintf_s(buf, 256, _TRUNCATE, fmt, va); va_end(va);
    PostMessage(g.hwnd, WM_STATUS_SET, 0, (LPARAM)buf);
}

#endif /* MAIN_H */
