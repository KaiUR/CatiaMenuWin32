/*
 * settings.c  -  Settings load/save, autorun, dialogs.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  IniPath  (static)                                                  */
/*  Purpose: Builds the full path to settings.ini in %APPDATA%\       */
/*           CatiaMenuWin32\ and stores it in out.                     */
/*  In:  out — buffer to receive the path                              */
/*       max — capacity of out in WCHARs                               */
/*  Out: (void — out is populated)                                      */
/* ================================================================== */
static void IniPath(WCHAR *out, int max) {
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\%s", g.appdata_dir, SETTINGS_FILE);
}

/* ================================================================== */
/*  Settings_Load                                                       */
/*  Purpose: Reads all application settings from settings.ini into the */
/*           provided Settings struct.  Applies defaults for missing   */
/*           keys (auto-sync on, auto-update on, start-with-windows on, */
/*           quickbar enabled, CNEXT.exe target, 6-hour refresh, etc.) */
/*           and auto-detects Python if no path is stored.             */
/*  In:  s — pointer to the Settings struct to populate                */
/*  Out: (void — s is fully initialised on return)                     */
/* ================================================================== */
void Settings_Load(Settings *s)
{
    ZeroMemory(s, sizeof(*s));
    WCHAR ini[MAX_APPPATH]; IniPath(ini, MAX_APPPATH);

    GetPrivateProfileString(L"Python",  L"Executable", L"", s->python_exe,   MAX_APPPATH, ini);
    GetPrivateProfileString(L"Scripts", L"CacheDir",   L"", s->cache_dir,    MAX_APPPATH, ini);
    GetPrivateProfileString(L"GitHub",  L"Token",      L"", s->github_token, 256, ini);

    s->auto_sync           = GetPrivateProfileInt(L"Options", L"AutoSync",          1, ini) != 0; /* default 1 = on */
    s->download_before_run = GetPrivateProfileInt(L"Options", L"DownloadBeforeRun", 0, ini) != 0; /* default 0 = off */
    s->show_console        = GetPrivateProfileInt(L"Options", L"ShowConsole",       0, ini) != 0; /* default 0 = hidden */
    s->console_keep_open   = GetPrivateProfileInt(L"Options", L"ConsoleKeepOpen",  1, ini) != 0; /* default 1 = on */
    s->check_updates       = GetPrivateProfileInt(L"Options", L"CheckUpdates",      1, ini) != 0; /* default 1 = on */
    s->deps_keep_open      = GetPrivateProfileInt(L"Options", L"DepsKeepOpen",      0, ini) != 0; /* default 0 = off */
    s->auto_update         = GetPrivateProfileInt(L"Options", L"AutoUpdate",         1, ini) != 0; /* default 1 = on */
    s->offline_use_cache   = GetPrivateProfileInt(L"Options", L"OfflineUseCache",   0, ini) != 0; /* default 0 = off */
    s->refresh_interval    = GetPrivateProfileInt(L"Options", L"RefreshInterval",    6, ini);     /* default 6 hours */
    s->sort_mode           = (SortMode)GetPrivateProfileInt(L"Options", L"SortMode", 0, ini);    /* default 0 = SORT_ORDER */
    s->main_repo_enabled   = GetPrivateProfileInt(L"Sources", L"MainRepoEnabled",   1, ini) != 0; /* default 1 = enabled */

    /* Extra repos */
    s->extra_repo_count = GetPrivateProfileInt(L"Sources", L"ExtraRepoCount", 0, ini);
    if (s->extra_repo_count > MAX_EXTRA_REPOS) s->extra_repo_count = MAX_EXTRA_REPOS;
    for (int i = 0; i < s->extra_repo_count; i++) {
        WCHAR key[32];
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dUrl",     i); GetPrivateProfileString(L"Sources", key, L"", s->extra_repos[i].url,    511, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dBranch",  i); GetPrivateProfileString(L"Sources", key, L"main", s->extra_repos[i].branch, 63, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dToken",   i); GetPrivateProfileString(L"Sources", key, L"", s->extra_repos[i].token,  255, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dEnabled", i); s->extra_repos[i].enabled = GetPrivateProfileInt(L"Sources", key, 1, ini) != 0;
        if (!s->extra_repos[i].branch[0]) wcsncpy_s(s->extra_repos[i].branch, 64, L"main", _TRUNCATE); /* fall back to 'main' if key was missing */
    }

    /* Local dirs */
    s->local_dir_count = GetPrivateProfileInt(L"Sources", L"LocalDirCount", 0, ini);
    if (s->local_dir_count > MAX_LOCAL_DIRS) s->local_dir_count = MAX_LOCAL_DIRS;
    for (int i = 0; i < s->local_dir_count; i++) {
        WCHAR key[32];
        _snwprintf_s(key, 31, _TRUNCATE, L"Local%dPath",    i); GetPrivateProfileString(L"Sources", key, L"", s->local_dirs[i].path, MAX_APPPATH-1, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Local%dEnabled", i); s->local_dirs[i].enabled = GetPrivateProfileInt(L"Sources", key, 1, ini) != 0;
    }
    s->always_on_top       = GetPrivateProfileInt(L"Window",  L"AlwaysOnTop",       1, ini) != 0;
    s->minimize_to_tray    = GetPrivateProfileInt(L"Window",  L"MinimizeToTray",    1, ini) != 0;
    s->start_with_windows  = GetPrivateProfileInt(L"Window",  L"StartWithWindows",  1, ini) != 0;
    s->start_minimized     = GetPrivateProfileInt(L"Window",  L"StartMinimized",    1, ini) != 0;
    s->theme               = (ThemeMode)GetPrivateProfileInt(L"Window", L"Theme",   0, ini);
    if (s->theme < 0 || s->theme > 2) s->theme = THEME_SYSTEM; /* clamp: a corrupt INI could store an out-of-range value */

    /* Quick Launch Bar */
    s->qbar_enabled            = GetPrivateProfileInt(L"QuickBar", L"Enabled",          1, ini) != 0; /* default 1 = shown */
    s->qbar_horizontal         = GetPrivateProfileInt(L"QuickBar", L"Horizontal",       0, ini) != 0; /* default 0 = vertical */
    s->qbar_topmost_with_catia = GetPrivateProfileInt(L"QuickBar", L"TopmostWithCatia", 1, ini) != 0; /* default 1 = topmost when CATIA is foreground */
    s->qbar_x                  = GetPrivateProfileInt(L"QuickBar", L"X",                0, ini);
    s->qbar_y                  = GetPrivateProfileInt(L"QuickBar", L"Y",                0, ini);
    GetPrivateProfileString(L"QuickBar", L"TargetApp", L"CATIA V5",
                            s->qbar_target_app, MAX_NAME, ini);
    GetPrivateProfileString(L"QuickBar", L"TargetExe", L"CNEXT.exe",
                            s->qbar_target_exe, MAX_NAME, ini);

    /* Double-click repeat */
    s->repeat_on_dblclick      = GetPrivateProfileInt(L"Options", L"RepeatOnDblClick",    1, ini) != 0;
    s->qbar_repeat_on_dblclick = GetPrivateProfileInt(L"Options", L"QBarRepeatOnDblClick",1, ini) != 0;

    /* Script display */
    s->tint_script_sources     = GetPrivateProfileInt(L"Options", L"TintScriptSources",   1, ini) != 0; /* default 1 = on */

    if (!s->cache_dir[0]) /* no cache dir stored — default to %APPDATA%\CatiaMenuWin32\scripts */
        _snwprintf_s(s->cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\scripts", g.appdata_dir);
    SHCreateDirectoryEx(NULL, s->cache_dir, NULL); /* create directory if it doesn't exist yet */

    if (!s->python_exe[0]) /* no Python path stored — probe the system */
        Runner_FindPython(s->python_exe, MAX_APPPATH);
}

/* ================================================================== */
/*  Settings_Save                                                       */
/*  Purpose: Writes every field in the Settings struct back to         */
/*           settings.ini using WritePrivateProfileString/Int.         */
/*  In:  s — Settings struct to persist (const, not modified)          */
/*  Out: (void — settings.ini is updated on disk)                      */
/* ================================================================== */
void Settings_Save(const Settings *s)
{
    WCHAR ini[MAX_APPPATH]; IniPath(ini, MAX_APPPATH);
    WCHAR tmp[8];

    WritePrivateProfileString(L"Python",  L"Executable",       s->python_exe,   ini);
    WritePrivateProfileString(L"Scripts", L"CacheDir",         s->cache_dir,    ini);
    WritePrivateProfileString(L"GitHub",  L"Token",            s->github_token, ini);

#define WB(sec,key,val) WritePrivateProfileString(sec, key, (val)?L"1":L"0", ini) /* writes "1" or "0" for each boolean flag */
    WB(L"Options", L"AutoSync",          s->auto_sync);
    WB(L"Options", L"DownloadBeforeRun", s->download_before_run);
    WB(L"Options", L"ShowConsole",       s->show_console);
    WB(L"Options", L"ConsoleKeepOpen",  s->console_keep_open);
    WB(L"Options", L"CheckUpdates",      s->check_updates);
    WB(L"Options", L"DepsKeepOpen",      s->deps_keep_open);
    WB(L"Options", L"AutoUpdate",         s->auto_update);
    WB(L"Options", L"OfflineUseCache",   s->offline_use_cache);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->refresh_interval);
    WritePrivateProfileString(L"Options", L"RefreshInterval", tmp, ini);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", (int)s->sort_mode);
    WritePrivateProfileString(L"Options", L"SortMode", tmp, ini);

    /* Sources */
    WB(L"Sources", L"MainRepoEnabled", s->main_repo_enabled);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->extra_repo_count);
    WritePrivateProfileString(L"Sources", L"ExtraRepoCount", tmp, ini);
    for (int i = 0; i < s->extra_repo_count; i++) {
        WCHAR key[32];
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dUrl",     i); WritePrivateProfileString(L"Sources", key, s->extra_repos[i].url,    ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dBranch",  i); WritePrivateProfileString(L"Sources", key, s->extra_repos[i].branch, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dToken",   i); WritePrivateProfileString(L"Sources", key, s->extra_repos[i].token,  ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dEnabled", i); WritePrivateProfileString(L"Sources", key, s->extra_repos[i].enabled ? L"1" : L"0", ini);
    }
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->local_dir_count);
    WritePrivateProfileString(L"Sources", L"LocalDirCount", tmp, ini);
    for (int i = 0; i < s->local_dir_count; i++) {
        WCHAR key[32];
        _snwprintf_s(key, 31, _TRUNCATE, L"Local%dPath",    i); WritePrivateProfileString(L"Sources", key, s->local_dirs[i].path, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Local%dEnabled", i); WritePrivateProfileString(L"Sources", key, s->local_dirs[i].enabled ? L"1" : L"0", ini);
    }
    WB(L"Window",  L"AlwaysOnTop",       s->always_on_top);
    WB(L"Window",  L"MinimizeToTray",    s->minimize_to_tray);
    WB(L"Window",  L"StartWithWindows",  s->start_with_windows);
    WB(L"Window",  L"StartMinimized",    s->start_minimized);

    /* Quick Launch Bar */
    WB(L"QuickBar", L"Enabled",          s->qbar_enabled);
    WB(L"QuickBar", L"Horizontal",       s->qbar_horizontal);
    WB(L"QuickBar", L"TopmostWithCatia", s->qbar_topmost_with_catia);
#undef WB
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->qbar_x);
    WritePrivateProfileString(L"QuickBar", L"X", tmp, ini);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->qbar_y);
    WritePrivateProfileString(L"QuickBar", L"Y", tmp, ini);
    WritePrivateProfileString(L"QuickBar", L"TargetApp", s->qbar_target_app, ini);
    WritePrivateProfileString(L"QuickBar", L"TargetExe", s->qbar_target_exe, ini);

    /* Double-click repeat */
    WritePrivateProfileString(L"Options", L"RepeatOnDblClick",
                              s->repeat_on_dblclick      ? L"1" : L"0", ini);
    WritePrivateProfileString(L"Options", L"QBarRepeatOnDblClick",
                              s->qbar_repeat_on_dblclick ? L"1" : L"0", ini);

    /* Script display */
    WritePrivateProfileString(L"Options", L"TintScriptSources",
                              s->tint_script_sources ? L"1" : L"0", ini);

    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", (int)s->theme);
    WritePrivateProfileString(L"Window", L"Theme", tmp, ini);
}

/* ================================================================== */
/*  Settings_ApplyAutorun                                               */
/*  Purpose: Adds or removes the application from the Windows startup  */
/*           registry key (HKCU\…\Run).  When enable is true and      */
/*           minimized is true the /minimized flag is appended to the  */
/*           command so wWinMain hides directly to tray.               */
/*  In:  enable    — true to add the autorun entry; false to remove    */
/*       minimized — when true, appends /minimized to the command      */
/*  Out: (void — registry entry is created or deleted)                 */
/* ================================================================== */
void Settings_ApplyAutorun(bool enable, bool minimized)
{
    HKEY hk;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTORUN_KEY, 0,
                     KEY_SET_VALUE, &hk) != ERROR_SUCCESS) return;

    if (enable) {
        WCHAR exe[MAX_APPPATH];
        GetModuleFileName(NULL, exe, MAX_APPPATH);
        WCHAR cmd[MAX_APPPATH + 16]; /* +16 = room for quoted path, " /minimized" flag, and null */
        if (minimized)
            _snwprintf_s(cmd, MAX_APPPATH + 15, _TRUNCATE, L"\"%s\" /minimized", exe); /* quoted so spaces in path work */
        else
            wcsncpy_s(cmd, MAX_APPPATH + 16, exe, _TRUNCATE);

        RegSetValueEx(hk, AUTORUN_NAME, 0, REG_SZ,
                      (BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(WCHAR))); /* byte count must include null terminator */
    } else {
        RegDeleteValue(hk, AUTORUN_NAME);
    }
    RegCloseKey(hk);
}

/* ================================================================== */
/*  Settings_TabControls  (static)                                      */
/*  Purpose: Lists the dialog control IDs that belong to each settings  */
/*           tab page. Used to show/hide entire pages when the tab      */
/*           selection changes. Terminated by -1.                      */
/* ================================================================== */
static const int s_stab0[] = {   /* General */
    IDC_GRP_PYTHON, IDC_LBL_PYTHON_PATH, IDC_EDIT_PYTHON, IDC_BTN_BROWSE_PY,
    IDC_BTN_CREATE_VENV, IDC_BTN_BROWSE_VENV, IDC_BTN_USE_GLOBAL_PY, IDC_BTN_DELETE_VENV,
    IDC_GRP_CACHE,  IDC_LBL_CACHE_PATH,  IDC_EDIT_CACHE,  IDC_BTN_BROWSE_DIR,
    IDC_GRP_TOKEN,  IDC_CHK_TOKEN,       IDC_EDIT_TOKEN,
    -1
};
static const int s_stab1[] = {   /* Sync */
    IDC_GRP_SYNC,
    IDC_CHK_AUTOSYNC, IDC_CHK_DOWNLOAD, IDC_CHK_CHECK_UPDATES, IDC_CHK_AUTO_UPDATE,
    IDC_LBL_REFRESH1, IDC_EDIT_REFRESH_INTERVAL, IDC_LBL_REFRESH2,
    IDC_CHK_OFFLINE_CACHE,
    -1
};
static const int s_stab2[] = {   /* Console */
    IDC_GRP_CONSOLE,
    IDC_CHK_CONSOLE, IDC_CHK_KEEP_OPEN, IDC_CHK_DEPS_KEEP_OPEN,
    IDC_CHK_REPEAT_MAIN,
    -1
};
static const int s_stab3[] = {   /* Window */
    IDC_GRP_WINDOW,
    IDC_CHK_ALWAYS_ON_TOP, IDC_CHK_MINIMIZE_TRAY,
    IDC_CHK_START_WINDOWS,  IDC_CHK_START_MIN,
    IDC_GRP_THEME,
    IDC_RAD_THEME_SYSTEM, IDC_RAD_THEME_DARK, IDC_RAD_THEME_LIGHT,
    IDC_GRP_SORT,
    IDC_RAD_SORT_DEFAULT, IDC_RAD_SORT_ALPHA, IDC_RAD_SORT_DATE, IDC_RAD_SORT_USED,
    IDC_GRP_DISPLAY,
    IDC_CHK_TINT_SOURCES,
    -1
};
static const int s_stab4[] = {   /* Quick Bar */
    IDC_CHK_QBAR_ENABLE,
    IDC_GRP_QBAR_ORI, IDC_RAD_QBAR_VERT, IDC_RAD_QBAR_HORIZ,
    IDC_CHK_QBAR_TOPMOST,
    IDC_LBL_QBAR_TARGET,     IDC_EDIT_QBAR_TARGET_S,     IDC_LBL_QBAR_TIP,
    IDC_LBL_QBAR_TARGET_EXE, IDC_EDIT_QBAR_TARGET_EXE_S, IDC_BTN_BROWSE_QBAR_EXE_S,
    IDC_LBL_QBAR_EXE_TIP,
    IDC_CHK_REPEAT_QBAR,
    -1
};
static const int *s_stabs[5] = {
    s_stab0, s_stab1, s_stab2, s_stab3, s_stab4
};

/* ================================================================== */
/*  Settings_ShowTab  (static)                                          */
/*  Purpose: Shows controls that belong to the selected tab page and   */
/*           hides controls that belong to all other pages.            */
/*  In:  hwnd — settings dialog handle                                 */
/*       tab  — zero-based index of the tab to make visible           */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Settings_ShowTab(HWND hwnd, int tab)
{
    for (int t = 0; t < 5; t++) {
        int sw = (t == tab) ? SW_SHOW : SW_HIDE;
        for (const int *p = s_stabs[t]; *p != -1; p++)
            ShowWindow(GetDlgItem(hwnd, *p), sw);
    }
}

/* ================================================================== */
/*  Settings_QBarEnableControls  (static)                              */
/*  Purpose: Enables or disables the Quick Bar sub-controls based on   */
/*           whether the Enable checkbox is checked.                   */
/*  In:  hwnd    — settings dialog handle                              */
/*       enabled — true to enable sub-controls; false to disable       */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Settings_QBarEnableControls(HWND hwnd, bool enabled)
{
    static const int ids[] = {
        IDC_GRP_QBAR_ORI, IDC_RAD_QBAR_VERT, IDC_RAD_QBAR_HORIZ,
        IDC_CHK_QBAR_TOPMOST,
        IDC_LBL_QBAR_TARGET,     IDC_EDIT_QBAR_TARGET_S,     IDC_LBL_QBAR_TIP,
        IDC_LBL_QBAR_TARGET_EXE, IDC_EDIT_QBAR_TARGET_EXE_S, IDC_BTN_BROWSE_QBAR_EXE_S,
        IDC_LBL_QBAR_EXE_TIP,
        IDC_CHK_REPEAT_QBAR,
        -1
    };
    for (const int *p = ids; *p != -1; p++)
        EnableWindow(GetDlgItem(hwnd, *p), enabled);
}

/* ================================================================== */
/*  SettingsTransferParams  (struct)                                    */
/*  Purpose: Carries all state for the Settings Transfer dialog.       */
/*           The caller sets is_export and (for import) src_path,      */
/*           then passes a pointer as lParam to DialogBoxParam.        */
/*           SettingsTransferDlgProc fills repos[], dirs[], and the    */
/*           incl_* / *_selected[] fields on IDOK.                    */
/* ================================================================== */
typedef struct {
    /* ── Input fields (set by caller before DialogBoxParam) ───────── */
    bool  is_export;                  /* true = Export; false = Import */
    WCHAR src_path[MAX_APPPATH];      /* import only: source INI path  */

    /* ── Items shown in the dialog (filled in WM_INITDIALOG) ──────── */
    /* Export: mirrored from g.cfg.  Import: read from src_path.       */
    ExtraRepo repos[MAX_EXTRA_REPOS];
    int       repo_count;
    LocalDir  dirs[MAX_LOCAL_DIRS];
    int       dir_count;

    /* ── Output fields (populated on IDOK) ────────────────────────── */
    bool incl_python_path;                        /* Python path checkbox          */
    bool incl_cache_dir;                          /* Cache folder checkbox         */
    bool incl_options;                            /* Options/theme/window/QBar     */
    bool main_token_selected;                     /* GitHub account token          */
    bool repo_token_selected[MAX_EXTRA_REPOS];    /* per-repo token                */
    bool repo_selected[MAX_EXTRA_REPOS];          /* true = user ticked this repo  */
    bool dir_selected[MAX_LOCAL_DIRS];            /* true = user ticked this dir   */
} SettingsTransferParams;

/* ================================================================== */
/*  Settings_WriteGeneralTo  (static)                                   */
/*  Purpose: Selectively writes Python path, cache dir, and/or all    */
/*           Options/Window/QuickBar keys to the given INI file path.  */
/*           The GitHub/Token key is NOT written here; the caller      */
/*           writes it separately per-token selection.                 */
/*  In:  s                — settings to write (const)                  */
/*       ini              — destination INI file path                  */
/*       incl_python_path — write [Python] Executable                  */
/*       incl_cache_dir   — write [Scripts] CacheDir                   */
/*       incl_options     — write Options, Window, and QuickBar keys   */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Settings_WriteGeneralTo(const Settings *s, const WCHAR *ini,
                                    bool incl_python_path, bool incl_cache_dir,
                                    bool incl_options)
{
    WCHAR tmp[8];

    if (incl_python_path)
        WritePrivateProfileString(L"Python",  L"Executable", s->python_exe, ini);
    if (incl_cache_dir)
        WritePrivateProfileString(L"Scripts", L"CacheDir",   s->cache_dir,  ini);

    if (!incl_options) return;

#define WB(sec, key, val) WritePrivateProfileString(sec, key, (val) ? L"1" : L"0", ini)
    WB(L"Options", L"AutoSync",           s->auto_sync);
    WB(L"Options", L"DownloadBeforeRun",  s->download_before_run);
    WB(L"Options", L"ShowConsole",        s->show_console);
    WB(L"Options", L"ConsoleKeepOpen",    s->console_keep_open);
    WB(L"Options", L"CheckUpdates",       s->check_updates);
    WB(L"Options", L"DepsKeepOpen",       s->deps_keep_open);
    WB(L"Options", L"AutoUpdate",         s->auto_update);
    WB(L"Options", L"OfflineUseCache",    s->offline_use_cache);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->refresh_interval);
    WritePrivateProfileString(L"Options", L"RefreshInterval", tmp, ini);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", (int)s->sort_mode);
    WritePrivateProfileString(L"Options", L"SortMode", tmp, ini);
    WB(L"Options", L"RepeatOnDblClick",     s->repeat_on_dblclick);
    WB(L"Options", L"QBarRepeatOnDblClick", s->qbar_repeat_on_dblclick);
    WB(L"Options", L"TintScriptSources",    s->tint_script_sources);

    WB(L"Window", L"AlwaysOnTop",      s->always_on_top);
    WB(L"Window", L"MinimizeToTray",   s->minimize_to_tray);
    WB(L"Window", L"StartWithWindows", s->start_with_windows);
    WB(L"Window", L"StartMinimized",   s->start_minimized);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", (int)s->theme);
    WritePrivateProfileString(L"Window", L"Theme", tmp, ini);

    WB(L"QuickBar", L"Enabled",          s->qbar_enabled);
    WB(L"QuickBar", L"Horizontal",       s->qbar_horizontal);
    WB(L"QuickBar", L"TopmostWithCatia", s->qbar_topmost_with_catia);
#undef WB
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->qbar_x);
    WritePrivateProfileString(L"QuickBar", L"X", tmp, ini);
    _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", s->qbar_y);
    WritePrivateProfileString(L"QuickBar", L"Y", tmp, ini);
    WritePrivateProfileString(L"QuickBar", L"TargetApp", s->qbar_target_app, ini);
    WritePrivateProfileString(L"QuickBar", L"TargetExe", s->qbar_target_exe, ini);
}


/* ================================================================== */
/*  Settings_ReadGeneralFrom  (static)                                  */
/*  Purpose: Selectively reads Python path, cache dir, and/or all      */
/*           Options/Window/QuickBar keys from the given INI into s.   */
/*           Applies auto-detect fallbacks only for the selected items. */
/*           Does NOT read the GitHub token; the caller handles that.  */
/*  In:  s                — settings struct to update                  */
/*       ini              — source INI file path                       */
/*       incl_python_path — read [Python] Executable                   */
/*       incl_cache_dir   — read [Scripts] CacheDir                    */
/*       incl_options     — read Options, Window, and QuickBar keys    */
/*  Out: (void — relevant fields in s are overwritten)                 */
/* ================================================================== */
static void Settings_ReadGeneralFrom(Settings *s, const WCHAR *ini,
                                     bool incl_python_path, bool incl_cache_dir,
                                     bool incl_options)
{
    if (incl_python_path)
        GetPrivateProfileString(L"Python",  L"Executable", L"", s->python_exe, MAX_APPPATH, ini);
    if (incl_cache_dir)
        GetPrivateProfileString(L"Scripts", L"CacheDir",   L"", s->cache_dir,  MAX_APPPATH, ini);

    if (incl_options) {
        s->auto_sync           = GetPrivateProfileInt(L"Options", L"AutoSync",           1, ini) != 0;
        s->download_before_run = GetPrivateProfileInt(L"Options", L"DownloadBeforeRun",  0, ini) != 0;
        s->show_console        = GetPrivateProfileInt(L"Options", L"ShowConsole",        0, ini) != 0;
        s->console_keep_open   = GetPrivateProfileInt(L"Options", L"ConsoleKeepOpen",    1, ini) != 0;
        s->check_updates       = GetPrivateProfileInt(L"Options", L"CheckUpdates",       1, ini) != 0;
        s->deps_keep_open      = GetPrivateProfileInt(L"Options", L"DepsKeepOpen",       0, ini) != 0;
        s->auto_update         = GetPrivateProfileInt(L"Options", L"AutoUpdate",         1, ini) != 0;
        s->offline_use_cache   = GetPrivateProfileInt(L"Options", L"OfflineUseCache",    0, ini) != 0;
        s->refresh_interval    = GetPrivateProfileInt(L"Options", L"RefreshInterval",    6, ini);
        s->sort_mode           = (SortMode)GetPrivateProfileInt(L"Options", L"SortMode", 0, ini);
        s->repeat_on_dblclick      = GetPrivateProfileInt(L"Options", L"RepeatOnDblClick",     1, ini) != 0;
        s->qbar_repeat_on_dblclick = GetPrivateProfileInt(L"Options", L"QBarRepeatOnDblClick", 1, ini) != 0;
        s->tint_script_sources     = GetPrivateProfileInt(L"Options", L"TintScriptSources",    1, ini) != 0;

        s->always_on_top      = GetPrivateProfileInt(L"Window", L"AlwaysOnTop",      1, ini) != 0;
        s->minimize_to_tray   = GetPrivateProfileInt(L"Window", L"MinimizeToTray",   1, ini) != 0;
        s->start_with_windows = GetPrivateProfileInt(L"Window", L"StartWithWindows", 1, ini) != 0;
        s->start_minimized    = GetPrivateProfileInt(L"Window", L"StartMinimized",   1, ini) != 0;
        s->theme = (ThemeMode)GetPrivateProfileInt(L"Window", L"Theme", 0, ini);
        if (s->theme < 0 || s->theme > 2) s->theme = THEME_SYSTEM;

        s->qbar_enabled            = GetPrivateProfileInt(L"QuickBar", L"Enabled",          1, ini) != 0;
        s->qbar_horizontal         = GetPrivateProfileInt(L"QuickBar", L"Horizontal",       0, ini) != 0;
        s->qbar_topmost_with_catia = GetPrivateProfileInt(L"QuickBar", L"TopmostWithCatia", 1, ini) != 0;
        s->qbar_x                  = GetPrivateProfileInt(L"QuickBar", L"X",                0, ini);
        s->qbar_y                  = GetPrivateProfileInt(L"QuickBar", L"Y",                0, ini);
        GetPrivateProfileString(L"QuickBar", L"TargetApp", L"CATIA V5",
                                s->qbar_target_app, MAX_NAME, ini);
        GetPrivateProfileString(L"QuickBar", L"TargetExe", L"CNEXT.exe",
                                s->qbar_target_exe, MAX_NAME, ini);
    }

    if (incl_cache_dir && !s->cache_dir[0])
        _snwprintf_s(s->cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\scripts", g.appdata_dir);
    if (incl_python_path && !s->python_exe[0])
        Runner_FindPython(s->python_exe, MAX_APPPATH);
}


/* ================================================================== */
/*  SettingsTransferDlgProc  (static)                                   */
/*  Purpose: Dialog procedure for IDD_SETTINGS_TRANSFER.  Used for    */
/*           both Export and Import.  In WM_INITDIALOG the two        */
/*           ListViews are populated:                                  */
/*             Export — items from g.cfg                               */
/*             Import — items read from p->src_path                   */
/*           Each item starts checked.  On IDOK the check state of    */
/*           every item is read back into p->repo_selected[] and      */
/*           p->dir_selected[], and the incl_* flags are set.         */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID on WM_COMMAND)                      */
/*       lp   — LPARAM (SettingsTransferParams* on WM_INITDIALOG)     */
/*  Out: INT_PTR — TRUE for handled messages; FALSE otherwise          */
/* ================================================================== */
static INT_PTR CALLBACK SettingsTransferDlgProc(HWND hwnd, UINT msg,
                                                WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
    {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lp);
        SettingsTransferParams *p = (SettingsTransferParams *)lp;

        SetWindowText(hwnd, p->is_export ? L"Export Settings" : L"Import Settings");
        if (!p->is_export)
            SetDlgItemText(hwnd, IDOK, L"Import");

        /* ── Populate repos and dirs arrays ─────────────────────── */
        if (p->is_export) {
            p->repo_count = g.cfg.extra_repo_count;
            for (int i = 0; i < p->repo_count; i++)
                p->repos[i] = g.cfg.extra_repos[i];
            p->dir_count = g.cfg.local_dir_count;
            for (int i = 0; i < p->dir_count; i++)
                p->dirs[i] = g.cfg.local_dirs[i];
        } else {
            /* Read sources available in the import file */
            p->repo_count = GetPrivateProfileInt(L"Sources", L"ExtraRepoCount", 0, p->src_path);
            if (p->repo_count > MAX_EXTRA_REPOS) p->repo_count = MAX_EXTRA_REPOS;
            for (int i = 0; i < p->repo_count; i++) {
                WCHAR key[32];
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dUrl",     i);
                GetPrivateProfileString(L"Sources", key, L"", p->repos[i].url,    511, p->src_path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dBranch",  i);
                GetPrivateProfileString(L"Sources", key, L"main", p->repos[i].branch, 63, p->src_path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dToken",   i);
                GetPrivateProfileString(L"Sources", key, L"", p->repos[i].token,  255, p->src_path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dEnabled", i);
                p->repos[i].enabled = GetPrivateProfileInt(L"Sources", key, 1, p->src_path) != 0;
                if (!p->repos[i].branch[0])
                    wcsncpy_s(p->repos[i].branch, 64, L"main", _TRUNCATE);
            }
            p->dir_count = GetPrivateProfileInt(L"Sources", L"LocalDirCount", 0, p->src_path);
            if (p->dir_count > MAX_LOCAL_DIRS) p->dir_count = MAX_LOCAL_DIRS;
            for (int i = 0; i < p->dir_count; i++) {
                WCHAR key[32];
                _snwprintf_s(key, 31, _TRUNCATE, L"Local%dPath",    i);
                GetPrivateProfileString(L"Sources", key, L"", p->dirs[i].path, MAX_APPPATH - 1, p->src_path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Local%dEnabled", i);
                p->dirs[i].enabled = GetPrivateProfileInt(L"Sources", key, 1, p->src_path) != 0;
            }
        }

        /* ── Repos ListView ──────────────────────────────────────── */
        HWND hRepos = GetDlgItem(hwnd, IDC_LST_TRANS_REPOS);
        ListView_SetExtendedListViewStyle(hRepos, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        {
            RECT rc; GetClientRect(hRepos, &rc);
            LVCOLUMN lvc; ZeroMemory(&lvc, sizeof(lvc));
            lvc.mask = LVCF_WIDTH;
            lvc.cx   = rc.right;
            ListView_InsertColumn(hRepos, 0, &lvc);
        }
        for (int i = 0; i < p->repo_count; i++) {
            WCHAR label[600];
            _snwprintf_s(label, 599, _TRUNCATE, L"%s  [%s]",
                         p->repos[i].url, p->repos[i].branch);
            LVITEM lvi; ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask    = LVIF_TEXT;
            lvi.iItem   = i;
            lvi.pszText = label;
            ListView_InsertItem(hRepos, &lvi);
            ListView_SetCheckState(hRepos, i, TRUE);
        }
        if (p->repo_count == 0) {
            ShowWindow(GetDlgItem(hwnd, IDC_LBL_TRANS_REPOS), SW_HIDE);
            ShowWindow(hRepos, SW_HIDE);
        }

        /* ── Dirs ListView ───────────────────────────────────────── */
        HWND hDirs = GetDlgItem(hwnd, IDC_LST_TRANS_DIRS);
        ListView_SetExtendedListViewStyle(hDirs, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        {
            RECT rc; GetClientRect(hDirs, &rc);
            LVCOLUMN lvc; ZeroMemory(&lvc, sizeof(lvc));
            lvc.mask = LVCF_WIDTH;
            lvc.cx   = rc.right;
            ListView_InsertColumn(hDirs, 0, &lvc);
        }
        for (int i = 0; i < p->dir_count; i++) {
            LVITEM lvi; ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask    = LVIF_TEXT;
            lvi.iItem   = i;
            lvi.pszText = p->dirs[i].path;
            ListView_InsertItem(hDirs, &lvi);
            ListView_SetCheckState(hDirs, i, TRUE);
        }
        if (p->dir_count == 0) {
            ShowWindow(GetDlgItem(hwnd, IDC_LBL_TRANS_DIRS), SW_HIDE);
            ShowWindow(hDirs, SW_HIDE);
        }

        /* ── Tokens ListView ─────────────────────────────────────── */
        HWND hTokens = GetDlgItem(hwnd, IDC_LST_TRANS_TOKENS);
        ListView_SetExtendedListViewStyle(hTokens, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        {
            RECT rc; GetClientRect(hTokens, &rc);
            LVCOLUMN lvc; ZeroMemory(&lvc, sizeof(lvc));
            lvc.mask = LVCF_WIDTH;
            lvc.cx   = rc.right;
            ListView_InsertColumn(hTokens, 0, &lvc);
        }
        int tok_row = 0;

        /* Main GitHub account token (lParam = -1) */
        bool has_main_token;
        if (p->is_export) {
            has_main_token = g.cfg.github_token[0] != L'\0';
        } else {
            WCHAR tmp_tok[256] = {0};
            GetPrivateProfileString(L"GitHub", L"Token", L"", tmp_tok, 255, p->src_path);
            has_main_token = tmp_tok[0] != L'\0';
        }
        if (has_main_token) {
            LVITEM lvi; ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask    = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem   = tok_row;
            lvi.pszText = L"GitHub account token";
            lvi.lParam  = (LPARAM)(-1);
            ListView_InsertItem(hTokens, &lvi);
            ListView_SetCheckState(hTokens, tok_row, TRUE);
            tok_row++;
        }

        /* Per-repo tokens (lParam = repo index in p->repos[]) */
        for (int i = 0; i < p->repo_count; i++) {
            if (!p->repos[i].token[0]) continue;
            WCHAR label[600];
            _snwprintf_s(label, 599, _TRUNCATE, L"Token for %s", p->repos[i].url);
            LVITEM lvi; ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask    = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem   = tok_row;
            lvi.pszText = label;
            lvi.lParam  = (LPARAM)(LONG_PTR)i;
            ListView_InsertItem(hTokens, &lvi);
            ListView_SetCheckState(hTokens, tok_row, TRUE);
            tok_row++;
        }
        if (tok_row == 0) {
            ShowWindow(GetDlgItem(hwnd, IDC_LBL_TRANS_TOKENS), SW_HIDE);
            ShowWindow(hTokens, SW_HIDE);
        }

        CheckDlgButton(hwnd, IDC_CHK_TRANS_PYTHON,  BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_TRANS_CACHE,   BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_TRANS_OPTIONS, BST_CHECKED);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:
        {
            SettingsTransferParams *p =
                (SettingsTransferParams *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            p->incl_python_path = IsDlgButtonChecked(hwnd, IDC_CHK_TRANS_PYTHON)  == BST_CHECKED;
            p->incl_cache_dir   = IsDlgButtonChecked(hwnd, IDC_CHK_TRANS_CACHE)   == BST_CHECKED;
            p->incl_options     = IsDlgButtonChecked(hwnd, IDC_CHK_TRANS_OPTIONS) == BST_CHECKED;

            HWND hRepos = GetDlgItem(hwnd, IDC_LST_TRANS_REPOS);
            for (int i = 0; i < p->repo_count; i++)
                p->repo_selected[i] = ListView_GetCheckState(hRepos, i) != 0;

            HWND hDirs = GetDlgItem(hwnd, IDC_LST_TRANS_DIRS);
            for (int i = 0; i < p->dir_count; i++)
                p->dir_selected[i] = ListView_GetCheckState(hDirs, i) != 0;

            /* Read token ListView — lParam identifies which token each row maps to */
            HWND hTokens = GetDlgItem(hwnd, IDC_LST_TRANS_TOKENS);
            int n_tok = ListView_GetItemCount(hTokens);
            for (int row = 0; row < n_tok; row++) {
                LVITEM lvi2; ZeroMemory(&lvi2, sizeof(lvi2));
                lvi2.mask  = LVIF_PARAM;
                lvi2.iItem = row;
                ListView_GetItem(hTokens, &lvi2);
                bool checked = ListView_GetCheckState(hTokens, row) != 0;
                if (lvi2.lParam == (LPARAM)(-1))
                    p->main_token_selected = checked;
                else
                    p->repo_token_selected[(int)lvi2.lParam] = checked;
            }

            /* Require at least one item selected */
            bool any = p->incl_python_path || p->incl_cache_dir || p->incl_options
                    || p->main_token_selected;
            for (int i = 0; i < p->repo_count && !any; i++)
                if (p->repo_selected[i] || p->repo_token_selected[i]) any = true;
            for (int i = 0; i < p->dir_count && !any; i++)
                if (p->dir_selected[i]) any = true;
            if (!any) {
                MessageBox(hwnd, L"Please select at least one item.",
                           p->is_export ? L"Export Settings" : L"Import Settings",
                           MB_ICONWARNING | MB_OK);
                return TRUE;
            }
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

/* ================================================================== */
/*  SettingsDlgProc                                                     */
/*  Purpose: Dialog procedure for the tabbed Settings dialog.          */
/*           Five tabs: General, Sync, Console, Window, Quick Bar.     */
/*           WM_INITDIALOG populates all controls and shows tab 0.    */
/*           TCN_SELCHANGE switches the visible page. IDOK reads all   */
/*           controls, saves settings, and applies side effects.       */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID or notification code)               */
/*       lp   — LPARAM (NMHDR* for WM_NOTIFY)                         */
/*  Out: INT_PTR — TRUE for handled messages; FALSE otherwise          */
/* ================================================================== */
INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        Settings *s = &g.cfg;

        /* ── Build the tab control ──────────────────────────────── */
        HWND hTab = GetDlgItem(hwnd, IDC_TAB_SETTINGS);
        {
            TCITEM ti = { TCIF_TEXT, 0, 0, NULL, 0, -1, 0 }; /* only TCIF_TEXT is set; other members zeroed */
            static const WCHAR *labels[] = {
                L"General", L"Sync", L"Console", L"Window", L"Quick Bar"
            };
            for (int i = 0; i < 5; i++) {
                ti.pszText = (LPWSTR)labels[i];
                TabCtrl_InsertItem(hTab, i, &ti);
            }
        }

        /* ── Tab 0: General ─────────────────────────────────────── */
        SetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe);
        SetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir);
        {
            bool has_tok = (s->github_token[0] != L'\0'); /* token present if string is non-empty */
            CheckDlgButton(hwnd, IDC_CHK_TOKEN, has_tok ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemText(hwnd, IDC_EDIT_TOKEN, s->github_token);
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN), has_tok); /* token field only editable when checkbox is ticked */
        }

        /* ── Tab 1: Sync ─────────────────────────────────────────── */
        CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,      s->auto_sync           ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,      s->download_before_run ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES, s->check_updates       ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_AUTO_UPDATE,    s->auto_update         ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_OFFLINE_CACHE, s->offline_use_cache   ? BST_CHECKED : BST_UNCHECKED);
        {
            WCHAR ri[8];
            _snwprintf_s(ri, 7, _TRUNCATE, L"%d", s->refresh_interval);
            SetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, ri);
        }

        /* ── Tab 2: Console ─────────────────────────────────────── */
        CheckDlgButton(hwnd, IDC_CHK_CONSOLE,       s->show_console        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_KEEP_OPEN,     s->console_keep_open   ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_DEPS_KEEP_OPEN, s->deps_keep_open     ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_REPEAT_MAIN,   s->repeat_on_dblclick  ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN), s->show_console); /* keep-open only meaningful when a console window is visible */

        /* ── Tab 3: Window ──────────────────────────────────────── */
        CheckDlgButton(hwnd, IDC_CHK_ALWAYS_ON_TOP, s->always_on_top     ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_MINIMIZE_TRAY, s->minimize_to_tray  ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_START_WINDOWS, s->start_with_windows? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_START_MIN,     s->start_minimized   ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_THEME_DARK,   s->theme == THEME_DARK   ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_THEME_LIGHT,  s->theme == THEME_LIGHT  ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_THEME_SYSTEM, s->theme == THEME_SYSTEM ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_SORT_DEFAULT, s->sort_mode == SORT_ORDER    ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_SORT_ALPHA,   s->sort_mode == SORT_ALPHA    ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_SORT_DATE,    s->sort_mode == SORT_DATE     ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_SORT_USED,    s->sort_mode == SORT_MOST_USED? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_TINT_SOURCES, s->tint_script_sources        ? BST_CHECKED : BST_UNCHECKED);

        /* ── Tab 4: Quick Bar ───────────────────────────────────── */
        CheckDlgButton(hwnd, IDC_CHK_QBAR_ENABLE,  s->qbar_enabled           ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_QBAR_HORIZ,   s->qbar_horizontal        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_RAD_QBAR_VERT,   !s->qbar_horizontal        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_QBAR_TOPMOST, s->qbar_topmost_with_catia? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_S,     s->qbar_target_app);
        SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, s->qbar_target_exe);
        CheckDlgButton(hwnd, IDC_CHK_REPEAT_QBAR, s->qbar_repeat_on_dblclick ? BST_CHECKED : BST_UNCHECKED);
        Settings_QBarEnableControls(hwnd, s->qbar_enabled);

        /* Show only tab 0, hide the rest */
        Settings_ShowTab(hwnd, 0);
        return TRUE;
    }

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == IDC_TAB_SETTINGS && nm->code == TCN_SELCHANGE) { /* TCN_SELCHANGE fires when the user clicks a tab */
            int sel = TabCtrl_GetCurSel(GetDlgItem(hwnd, IDC_TAB_SETTINGS));
            Settings_ShowTab(hwnd, sel);
        }
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {

        case IDC_BTN_BROWSE_PY:
        {
            WCHAR path[MAX_APPPATH] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_PYTHON, path, MAX_APPPATH);
            OPENFILENAME ofn = {
                .lStructSize=sizeof(ofn), .hwndOwner=hwnd,
                .lpstrFilter=L"Python\0python.exe;python3.exe\0All Files\0*.*\0",
                .lpstrFile=path, .nMaxFile=MAX_APPPATH,
                .Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST,
                .lpstrTitle=L"Select Python Executable"
            };
            if (GetOpenFileName(&ofn)) SetDlgItemText(hwnd, IDC_EDIT_PYTHON, path);
            break;
        }

        case IDC_BTN_BROWSE_DIR:
        {
            WCHAR path[MAX_APPPATH] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_CACHE, path, MAX_APPPATH);
            BROWSEINFO bi = { .hwndOwner=hwnd,
                              .lpszTitle=L"Select Script Cache Folder",
                              .ulFlags=BIF_USENEWUI|BIF_RETURNONLYFSDIRS };
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
            if (pidl) {
                SHGetPathFromIDList(pidl, path);
                SetDlgItemText(hwnd, IDC_EDIT_CACHE, path);
                CoTaskMemFree(pidl);
            }
            break;
        }

        /* ================================================================== */
        /*  IDC_BTN_CREATE_VENV                                               */
        /*  Purpose: Creates a Python virtual environment at                  */
        /*           %APPDATA%\CatiaMenuWin32\venv using the Python           */
        /*           executable currently shown in the dialog's path field.   */
        /*           On success, updates the path field to point at the new   */
        /*           venv's python.exe.                                        */
        /* ================================================================== */
        case IDC_BTN_CREATE_VENV:
        {
            /* ── 1. Resolve Python path from the dialog field ───────────── */
            WCHAR python_path[MAX_APPPATH] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_PYTHON, python_path, MAX_APPPATH);
            if (!python_path[0])
                Runner_FindPython(python_path, MAX_APPPATH);
            if (!python_path[0]) {
                MessageBox(hwnd,
                    L"No Python executable found.\n\n"
                    L"Set the Python interpreter path before creating a virtual environment.",
                    L"Create Virtual Environment", MB_ICONWARNING | MB_OK);
                break;
            }

            /* ── 2. Build venv and venv python.exe paths ────────────────── */
            WCHAR venv_dir[MAX_APPPATH];
            WCHAR venv_python[MAX_APPPATH];
            _snwprintf_s(venv_dir,    MAX_APPPATH, _TRUNCATE,
                         L"%s\\venv",                      g.appdata_dir);
            _snwprintf_s(venv_python, MAX_APPPATH, _TRUNCATE,
                         L"%s\\venv\\Scripts\\python.exe", g.appdata_dir);

            /* ── 3. Confirm ─────────────────────────────────────────────── */
            bool already = GetFileAttributes(venv_python) != INVALID_FILE_ATTRIBUTES;
            WCHAR confirm[MAX_APPPATH * 2 + 64];
            _snwprintf_s(confirm, MAX_APPPATH * 2 + 63, _TRUNCATE,
                already
                ? L"A virtual environment already exists at:\n%s\n\n"
                  L"Recreate it? The existing environment will be replaced.\n\n"
                  L"Using Python:\n%s"
                : L"Create a virtual environment at:\n%s\n\n"
                  L"Using Python:\n%s",
                venv_dir, python_path);
            if (MessageBox(hwnd, confirm, L"Create Virtual Environment",
                           MB_ICONQUESTION | MB_YESNO) != IDYES)
                break;

            /* ── 4. Run python.exe -m venv <dir> in a visible console ───── */
            EnableWindow(GetDlgItem(hwnd, IDC_BTN_CREATE_VENV), FALSE);
            SetDlgItemText(hwnd, IDC_BTN_CREATE_VENV, L"Creating...");

            WCHAR params[MAX_APPPATH * 2 + 32];
            _snwprintf_s(params, MAX_APPPATH * 2 + 31, _TRUNCATE,
                         L"/c \"%s\" -m venv \"%s\"", python_path, venv_dir);

            SHELLEXECUTEINFO sei;
            ZeroMemory(&sei, sizeof(sei));
            sei.cbSize      = sizeof(sei);
            sei.fMask       = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd        = hwnd;
            sei.lpFile      = L"cmd.exe";
            sei.lpParameters = params;
            sei.nShow       = SW_SHOW;

            bool launched = ShellExecuteEx(&sei) && sei.hProcess;
            if (launched) {
                /* Pump messages while waiting so the dialog stays responsive */
                HANDLE h = sei.hProcess;
                while (MsgWaitForMultipleObjects(1, &h, FALSE, INFINITE, QS_ALLINPUT)
                       == (WAIT_OBJECT_0 + 1)) {
                    MSG m;
                    while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&m);
                        DispatchMessage(&m);
                    }
                }
                CloseHandle(sei.hProcess);
            }

            EnableWindow(GetDlgItem(hwnd, IDC_BTN_CREATE_VENV), TRUE);
            SetDlgItemText(hwnd, IDC_BTN_CREATE_VENV, L"Create venv");

            /* ── 5. Verify outcome and update the path field ────────────── */
            if (launched && GetFileAttributes(venv_python) != INVALID_FILE_ATTRIBUTES) {
                SetDlgItemText(hwnd, IDC_EDIT_PYTHON, venv_python);
                MessageBox(hwnd,
                    L"Virtual environment created successfully.\n\n"
                    L"The Python path has been updated to point to the new environment.\n\n"
                    L"Click OK to save settings, then use ↓ Deps to install packages.",
                    L"Create Virtual Environment", MB_ICONINFORMATION | MB_OK);
            } else {
                WCHAR err_msg[MAX_APPPATH + 128];
                _snwprintf_s(err_msg, MAX_APPPATH + 127, _TRUNCATE,
                    L"Virtual environment creation failed or was cancelled.\n\n"
                    L"Expected Python executable not found at:\n%s",
                    venv_python);
                MessageBox(hwnd, err_msg, L"Create Virtual Environment",
                           MB_ICONWARNING | MB_OK);
            }
            break;
        }

        /* ================================================================== */
        /*  IDC_BTN_USE_GLOBAL_PY                                            */
        /*  Purpose: Probes PATH and well-known directories for a global     */
        /*           Python installation (bypassing any venv in g.cfg) and  */
        /*           fills the path field with the result immediately.       */
        /* ================================================================== */
        case IDC_BTN_USE_GLOBAL_PY:
        {
            /* Temporarily clear g.cfg.python_exe so Runner_FindPython skips
               the "user-configured path" step and searches the system instead */
            WCHAR saved[MAX_APPPATH];
            wcsncpy_s(saved, MAX_APPPATH, g.cfg.python_exe, _TRUNCATE);
            g.cfg.python_exe[0] = L'\0';

            WCHAR found[MAX_APPPATH] = {0};
            bool ok = Runner_FindPython(found, MAX_APPPATH);

            /* Restore g.cfg — the dialog has not saved yet */
            wcsncpy_s(g.cfg.python_exe, MAX_APPPATH, saved, _TRUNCATE);

            if (ok) {
                SetDlgItemText(hwnd, IDC_EDIT_PYTHON, found);
            } else {
                SetDlgItemText(hwnd, IDC_EDIT_PYTHON, L"");
                MessageBox(hwnd,
                    L"No Python installation found automatically.\n\n"
                    L"Use Browse... to locate python.exe manually.",
                    L"Auto-detect Python", MB_ICONWARNING | MB_OK);
            }
            break;
        }

        /* ================================================================== */
        /*  IDC_BTN_BROWSE_VENV                                               */
        /*  Purpose: Opens a folder browser so the user can select an        */
        /*           existing virtual environment directory.  Validates that  */
        /*           Scripts\python.exe exists inside it, then updates the   */
        /*           Python Interpreter path field.                           */
        /* ================================================================== */
        case IDC_BTN_BROWSE_VENV:
        {
            WCHAR dir[MAX_APPPATH] = {0};
            BROWSEINFO bi;
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"Select the root folder of a Python virtual environment";
            bi.ulFlags   = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
            if (!pidl) break;
            SHGetPathFromIDList(pidl, dir);
            CoTaskMemFree(pidl);

            WCHAR venv_python[MAX_APPPATH];
            _snwprintf_s(venv_python, MAX_APPPATH, _TRUNCATE,
                         L"%s\\Scripts\\python.exe", dir);

            if (GetFileAttributes(venv_python) != INVALID_FILE_ATTRIBUTES) {
                SetDlgItemText(hwnd, IDC_EDIT_PYTHON, venv_python);
            } else {
                WCHAR msg[MAX_APPPATH + 128];
                _snwprintf_s(msg, MAX_APPPATH + 127, _TRUNCATE,
                    L"No Python executable found at:\n%s\n\n"
                    L"Make sure you selected the root folder of a valid "
                    L"virtual environment (it must contain Scripts\\python.exe).",
                    venv_python);
                MessageBox(hwnd, msg, L"Browse Virtual Environment",
                           MB_ICONWARNING | MB_OK);
            }
            break;
        }

        /* ================================================================== */
        /*  IDC_BTN_DELETE_VENV                                               */
        /*  Purpose: Deletes the virtual environment directory at             */
        /*           %APPDATA%\CatiaMenuWin32\venv after user confirmation.  */
        /*           If the Python path field points into the deleted venv,  */
        /*           the field is cleared so auto-detect runs on next save.  */
        /* ================================================================== */
        case IDC_BTN_DELETE_VENV:
        {
            WCHAR venv_dir[MAX_APPPATH];
            _snwprintf_s(venv_dir, MAX_APPPATH, _TRUNCATE,
                         L"%s\\venv", g.appdata_dir);

            if (GetFileAttributes(venv_dir) == INVALID_FILE_ATTRIBUTES) {
                MessageBox(hwnd,
                    L"No virtual environment found at the default location.",
                    L"Delete Virtual Environment", MB_ICONINFORMATION | MB_OK);
                break;
            }

            WCHAR confirm[MAX_APPPATH + 128];
            _snwprintf_s(confirm, MAX_APPPATH + 127, _TRUNCATE,
                L"Delete the virtual environment at:\n%s\n\n"
                L"All installed packages will be removed. This cannot be undone.",
                venv_dir);
            if (MessageBox(hwnd, confirm, L"Delete Virtual Environment",
                           MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                break;

            /* SHFileOperation requires a double-null-terminated path */
            WCHAR del_path[MAX_APPPATH + 2];
            ZeroMemory(del_path, sizeof(del_path));
            wcsncpy_s(del_path, MAX_APPPATH + 1, venv_dir, _TRUNCATE);

            SHFILEOPSTRUCTW fop;
            ZeroMemory(&fop, sizeof(fop));
            fop.hwnd   = hwnd;
            fop.wFunc  = FO_DELETE;
            fop.pFrom  = del_path;
            fop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;

            int res = SHFileOperationW(&fop);

            if (res == 0 && !fop.fAnyOperationsAborted) {
                /* If the path field was pointing into the deleted venv, clear it */
                WCHAR cur[MAX_APPPATH] = {0};
                GetDlgItemText(hwnd, IDC_EDIT_PYTHON, cur, MAX_APPPATH);
                if (_wcsnicmp(cur, venv_dir, wcslen(venv_dir)) == 0)
                    SetDlgItemText(hwnd, IDC_EDIT_PYTHON, L"");

                MessageBox(hwnd,
                    L"Virtual environment deleted successfully.",
                    L"Delete Virtual Environment", MB_ICONINFORMATION | MB_OK);
            } else {
                MessageBox(hwnd,
                    L"Failed to delete the virtual environment.\n\n"
                    L"Some files may be in use — close any terminals or "
                    L"scripts using this environment and try again.",
                    L"Delete Virtual Environment", MB_ICONERROR | MB_OK);
            }
            break;
        }

        case IDC_BTN_BROWSE_QBAR_EXE_S:
        {
            WCHAR path[MAX_APPPATH] = {0};
            OPENFILENAME ofn = {
                .lStructSize = sizeof(ofn), .hwndOwner = hwnd,
                .lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0",
                .lpstrFile = path, .nMaxFile = MAX_APPPATH,
                .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                .lpstrTitle = L"Select Target Application"
            };
            if (GetOpenFileName(&ofn)) {
                WCHAR *slash = wcsrchr(path, L'\\');
                SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, slash ? slash + 1 : path); /* strip path — show filename only */
            }
            break;
        }

        case IDC_CHK_CONSOLE:
            EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE) == BST_CHECKED); /* grey out Keep Open when Show Console is unchecked */
            break;

        case IDC_CHK_TOKEN:
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED);
            break;

        case IDC_CHK_QBAR_ENABLE:
            Settings_QBarEnableControls(hwnd,
                IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_ENABLE) == BST_CHECKED);
            break;

        /* ================================================================== */
        /*  IDC_BTN_EXPORT_SETTINGS                                           */
        /*  Purpose: Shows IDD_SETTINGS_TRANSFER so the user picks which     */
        /*           general settings, individual repos, and individual local  */
        /*           folders to include, then writes only those items to a    */
        /*           user-chosen file.  Exports g.cfg (last-saved state) —   */
        /*           unsaved dialog changes are not included.                 */
        /*           prefs.ini is never written.                              */
        /* ================================================================== */
        case IDC_BTN_EXPORT_SETTINGS:
        {
            /* Step 1: Show selection dialog */
            SettingsTransferParams tp;
            ZeroMemory(&tp, sizeof(tp));
            tp.is_export = true;
            if (DialogBoxParam(GetModuleHandle(NULL),
                               MAKEINTRESOURCE(IDD_SETTINGS_TRANSFER),
                               hwnd, SettingsTransferDlgProc, (LPARAM)&tp) != IDOK)
                break;

            /* Step 2: Choose destination file */
            WCHAR path[MAX_APPPATH] = {0};
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFilter = L"INI Files\0*.ini\0All Files\0*.*\0";
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_APPPATH;
            ofn.lpstrDefExt = L"ini";
            ofn.lpstrTitle  = L"Export Settings";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (!GetSaveFileName(&ofn)) break;

            /* Step 3: Clear any existing file so stale keys don't persist */
            DeleteFileW(path);

            /* Step 4: Write selectively — Python path, cache dir, options, GitHub token */
            if (tp.incl_python_path || tp.incl_cache_dir || tp.incl_options)
                Settings_WriteGeneralTo(&g.cfg, path,
                                        tp.incl_python_path, tp.incl_cache_dir, tp.incl_options);
            if (tp.main_token_selected)
                WritePrivateProfileString(L"GitHub", L"Token", g.cfg.github_token, path);

            /* Step 5: Write only the individually selected repos */
            WCHAR tmp[8];
            int n_repos = 0;
            for (int i = 0; i < tp.repo_count; i++) {
                if (!tp.repo_selected[i]) continue;
                WCHAR key[32];
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dUrl",     n_repos);
                WritePrivateProfileString(L"Sources", key, tp.repos[i].url, path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dBranch",  n_repos);
                WritePrivateProfileString(L"Sources", key, tp.repos[i].branch, path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dToken",   n_repos);
                WritePrivateProfileString(L"Sources", key,
                                          tp.repo_token_selected[i] ? tp.repos[i].token : L"", path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dEnabled", n_repos);
                WritePrivateProfileString(L"Sources", key,
                                          tp.repos[i].enabled ? L"1" : L"0", path);
                n_repos++;
            }
            _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", n_repos);
            WritePrivateProfileString(L"Sources", L"ExtraRepoCount", tmp, path);

            /* Step 6: Write only the individually selected local dirs */
            int n_dirs = 0;
            for (int i = 0; i < tp.dir_count; i++) {
                if (!tp.dir_selected[i]) continue;
                WCHAR key[32];
                _snwprintf_s(key, 31, _TRUNCATE, L"Local%dPath",    n_dirs);
                WritePrivateProfileString(L"Sources", key, tp.dirs[i].path, path);
                _snwprintf_s(key, 31, _TRUNCATE, L"Local%dEnabled", n_dirs);
                WritePrivateProfileString(L"Sources", key,
                                          tp.dirs[i].enabled ? L"1" : L"0", path);
                n_dirs++;
            }
            _snwprintf_s(tmp, 7, _TRUNCATE, L"%d", n_dirs);
            WritePrivateProfileString(L"Sources", L"LocalDirCount", tmp, path);

            MessageBox(hwnd, L"Settings exported successfully.",
                       L"Export Settings", MB_ICONINFORMATION | MB_OK);
            break;
        }

        /* ================================================================== */
        /*  IDC_BTN_IMPORT_SETTINGS                                           */
        /*  Purpose: Lets the user pick an exported file and then choose      */
        /*           which individual repos, local dirs, and/or general       */
        /*           settings to import.  Selected repos and dirs are         */
        /*           appended to the current config.  Duplicate URLs/paths    */
        /*           are skipped silently; if the duplicate carries a token   */
        /*           the user is prompted to keep the old or new one.         */
        /*           The dialog closes via IDCANCEL so its open controls      */
        /*           do not overwrite the newly imported settings.            */
        /* ================================================================== */
        case IDC_BTN_IMPORT_SETTINGS:
        {
            /* Step 1: Choose source file */
            WCHAR path[MAX_APPPATH] = {0};
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFilter = L"INI Files\0*.ini\0All Files\0*.*\0";
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_APPPATH;
            ofn.lpstrTitle  = L"Import Settings";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (!GetOpenFileName(&ofn)) break;

            /* Step 2: Show selection dialog (reads repos/dirs from file) */
            SettingsTransferParams tp;
            ZeroMemory(&tp, sizeof(tp));
            tp.is_export = false;
            wcsncpy_s(tp.src_path, MAX_APPPATH, path, _TRUNCATE);
            if (DialogBoxParam(GetModuleHandle(NULL),
                               MAKEINTRESOURCE(IDD_SETTINGS_TRANSFER),
                               hwnd, SettingsTransferDlgProc, (LPARAM)&tp) != IDOK)
                break;

            /* Step 3: Capture state for side-effect delta */
            bool old_dark       = g.dark_mode;
            bool old_qbar_en    = g.cfg.qbar_enabled;
            bool old_qbar_horiz = g.cfg.qbar_horizontal;

            /* Step 4: Apply selectively — Python path, cache dir, options, GitHub token */
            if (tp.incl_python_path || tp.incl_cache_dir || tp.incl_options)
                Settings_ReadGeneralFrom(&g.cfg, path,
                                         tp.incl_python_path, tp.incl_cache_dir, tp.incl_options);
            if (tp.main_token_selected)
                GetPrivateProfileString(L"GitHub", L"Token", L"",
                                        g.cfg.github_token, 256, path);

            /* Step 5: Append selected repos; prompt on token conflict for duplicates */
            for (int i = 0; i < tp.repo_count; i++) {
                if (!tp.repo_selected[i]) continue;

                /* Search for an existing entry with the same URL */
                int dup = -1;
                for (int j = 0; j < g.cfg.extra_repo_count; j++) {
                    if (_wcsicmp(g.cfg.extra_repos[j].url, tp.repos[i].url) == 0) {
                        dup = j;
                        break;
                    }
                }

                if (dup >= 0) {
                    /* Duplicate: only prompt when the user selected this repo's token */
                    if (tp.repo_token_selected[i] && tp.repos[i].token[0]) {
                        WCHAR msg[720];
                        _snwprintf_s(msg, 719, _TRUNCATE,
                            L"This repository is already in your sources:\n%s\n\n"
                            L"Which token do you want to keep?\n\n"
                            L"Yes  →  Keep existing token\n"
                            L"No   →  Use imported token",
                            tp.repos[i].url);
                        if (MessageBox(hwnd, msg, L"Duplicate Repository",
                                       MB_ICONQUESTION | MB_YESNO) == IDNO)
                            wcsncpy_s(g.cfg.extra_repos[dup].token, 256,
                                      tp.repos[i].token, _TRUNCATE);
                    }
                    /* else: no token conflict — skip silently */
                } else if (g.cfg.extra_repo_count < MAX_EXTRA_REPOS) {
                    /* New repo — append */
                    g.cfg.extra_repos[g.cfg.extra_repo_count] = tp.repos[i];
                    if (!tp.repo_token_selected[i])
                        g.cfg.extra_repos[g.cfg.extra_repo_count].token[0] = L'\0';
                    g.cfg.extra_repo_count++;
                }
            }

            /* Step 6: Append selected local dirs; duplicates skipped silently */
            for (int i = 0; i < tp.dir_count; i++) {
                if (!tp.dir_selected[i]) continue;

                bool dup = false;
                for (int j = 0; j < g.cfg.local_dir_count; j++) {
                    if (_wcsicmp(g.cfg.local_dirs[j].path, tp.dirs[i].path) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup && g.cfg.local_dir_count < MAX_LOCAL_DIRS) {
                    g.cfg.local_dirs[g.cfg.local_dir_count] = tp.dirs[i];
                    g.cfg.local_dir_count++;
                }
            }

            /* Step 7: Persist merged result and apply side effects */
            Settings_Save(&g.cfg);
            SHCreateDirectoryEx(NULL, g.cfg.cache_dir, NULL);

            App_ResolveTheme();
            if (g.dark_mode != old_dark) {
                App_RebuildGDI();
                Window_ApplyDarkMode(g.hwnd);
                Window_ApplyThemeToChildren(g.hwnd);
                QuickBar_OnThemeChange();
                Tabs_RebuildButtons();
                InvalidateRect(g.hwnd, NULL, TRUE);
            }
            Window_ApplyAlwaysOnTop();
            Settings_ApplyAutorun(g.cfg.start_with_windows, g.cfg.start_minimized);
            Tabs_ApplySort(g.active_tab);
            Tabs_RebuildButtons();
            if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);

            if (g.cfg.qbar_enabled) {
                if (g.cfg.qbar_horizontal != old_qbar_horiz) {
                    QuickBar_Destroy();
                    QuickBar_Register(GetModuleHandle(NULL));
                    QuickBar_Create();
                    QuickBar_Rebuild();
                } else if (!old_qbar_en) {
                    QuickBar_Show(true);
                }
            } else if (old_qbar_en) {
                QuickBar_Show(false);
            }

            MessageBox(hwnd,
                L"Settings imported successfully.\n\n"
                L"The dialog will now close.",
                L"Import Settings", MB_ICONINFORMATION | MB_OK);

            /* Close via IDCANCEL so the open dialog controls do not
               write back over the settings we just imported. */
            EndDialog(hwnd, IDCANCEL);
            break;
        }

        case IDC_BTN_RESET:
        {
            int res = MessageBox(hwnd,
                L"Reset all settings to defaults?\n\n"
                L"This will clear your Python path, cache folder,\n"
                L"GitHub token, and all options.\n\n"
                L"Sources (extra repos and local folders) are not affected.",
                L"Reset to Defaults",
                MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
            if (res != IDYES) break;

            Settings *s = &g.cfg;
            s->python_exe[0]           = L'\0';
            s->github_token[0]         = L'\0';
            s->auto_sync               = true;
            s->download_before_run     = false;
            s->show_console            = false;
            s->console_keep_open       = true;
            s->deps_keep_open          = false;
            s->check_updates           = true;
            s->auto_update             = true;
            s->offline_use_cache       = false;
            s->refresh_interval        = 6;
            s->always_on_top           = true;
            s->minimize_to_tray        = true;
            s->start_with_windows      = true;
            s->start_minimized         = true;
            s->theme                   = THEME_SYSTEM;
            s->sort_mode               = SORT_ORDER;
            s->qbar_enabled            = true;
            s->qbar_horizontal         = false;
            s->qbar_topmost_with_catia = true;
            wcsncpy_s(s->qbar_target_app, MAX_NAME, L"CATIA V5",  _TRUNCATE);
            wcsncpy_s(s->qbar_target_exe, MAX_NAME, L"CNEXT.exe", _TRUNCATE);
            s->repeat_on_dblclick      = true;
            s->qbar_repeat_on_dblclick = true;
            _snwprintf_s(s->cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\scripts", g.appdata_dir);
            Runner_FindPython(s->python_exe, MAX_APPPATH);

            /* Reload all tab controls */
            SetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe);
            SetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir);
            SetDlgItemText(hwnd, IDC_EDIT_TOKEN,  L"");
            CheckDlgButton(hwnd, IDC_CHK_TOKEN,         BST_UNCHECKED);
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN), FALSE);

            CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,       BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,       BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES,  BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_AUTO_UPDATE,    BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_OFFLINE_CACHE,  BST_UNCHECKED);
            SetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, L"6");

            CheckDlgButton(hwnd, IDC_CHK_CONSOLE,        BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_KEEP_OPEN,      BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_DEPS_KEEP_OPEN, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_REPEAT_MAIN,    BST_CHECKED);
            EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN), FALSE);

            CheckDlgButton(hwnd, IDC_CHK_ALWAYS_ON_TOP, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_MINIMIZE_TRAY, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_START_WINDOWS, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_START_MIN,     BST_CHECKED);
            CheckDlgButton(hwnd, IDC_RAD_THEME_SYSTEM,  BST_CHECKED);
            CheckDlgButton(hwnd, IDC_RAD_THEME_DARK,    BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_RAD_THEME_LIGHT,   BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_RAD_SORT_DEFAULT,  BST_CHECKED);
            CheckDlgButton(hwnd, IDC_RAD_SORT_ALPHA,    BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_RAD_SORT_DATE,     BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_RAD_SORT_USED,     BST_UNCHECKED);
            s->tint_script_sources = true;
            CheckDlgButton(hwnd, IDC_CHK_TINT_SOURCES,  BST_CHECKED);

            CheckDlgButton(hwnd, IDC_CHK_QBAR_ENABLE,  BST_CHECKED);
            CheckDlgButton(hwnd, IDC_RAD_QBAR_VERT,    BST_CHECKED);
            CheckDlgButton(hwnd, IDC_RAD_QBAR_HORIZ,   BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_QBAR_TOPMOST, BST_CHECKED);
            SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_S,     L"CATIA V5");
            SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, L"CNEXT.exe");
            CheckDlgButton(hwnd, IDC_CHK_REPEAT_QBAR,  BST_CHECKED);
            Settings_QBarEnableControls(hwnd, true);
            break;
        }

        case IDOK:
        {
            Settings *s = &g.cfg;
            bool old_dark       = g.dark_mode;
            bool old_qbar_horiz = s->qbar_horizontal;
            bool old_qbar_en    = s->qbar_enabled;

            /* General */
            GetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe, MAX_APPPATH);
            GetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir,  MAX_APPPATH);
            if (IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED)
                GetDlgItemText(hwnd, IDC_EDIT_TOKEN, s->github_token, 256);
            else
                s->github_token[0] = L'\0';

            /* Sync */
            s->auto_sync           = IsDlgButtonChecked(hwnd, IDC_CHK_AUTOSYNC)      == BST_CHECKED;
            s->download_before_run = IsDlgButtonChecked(hwnd, IDC_CHK_DOWNLOAD)      == BST_CHECKED;
            s->check_updates       = IsDlgButtonChecked(hwnd, IDC_CHK_CHECK_UPDATES) == BST_CHECKED;
            s->auto_update         = IsDlgButtonChecked(hwnd, IDC_CHK_AUTO_UPDATE)   == BST_CHECKED;
            s->offline_use_cache   = IsDlgButtonChecked(hwnd, IDC_CHK_OFFLINE_CACHE) == BST_CHECKED;
            {
                WCHAR ri[8] = {0};
                GetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, ri, 7);
                s->refresh_interval = _wtoi(ri);
                if (s->refresh_interval < 0)   s->refresh_interval = 0;   /* 0 = no auto-sync */
                if (s->refresh_interval > 168) s->refresh_interval = 168; /* cap at 168 hours (1 week) */
            }

            /* Console */
            s->show_console           = IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE)        == BST_CHECKED;
            s->console_keep_open      = IsDlgButtonChecked(hwnd, IDC_CHK_KEEP_OPEN)      == BST_CHECKED;
            s->deps_keep_open         = IsDlgButtonChecked(hwnd, IDC_CHK_DEPS_KEEP_OPEN) == BST_CHECKED;
            s->repeat_on_dblclick     = IsDlgButtonChecked(hwnd, IDC_CHK_REPEAT_MAIN)    == BST_CHECKED;

            /* Window */
            s->always_on_top      = IsDlgButtonChecked(hwnd, IDC_CHK_ALWAYS_ON_TOP)== BST_CHECKED;
            s->minimize_to_tray   = IsDlgButtonChecked(hwnd, IDC_CHK_MINIMIZE_TRAY)== BST_CHECKED;
            s->start_with_windows = IsDlgButtonChecked(hwnd, IDC_CHK_START_WINDOWS)== BST_CHECKED;
            s->start_minimized    = IsDlgButtonChecked(hwnd, IDC_CHK_START_MIN)    == BST_CHECKED;
            if      (IsDlgButtonChecked(hwnd, IDC_RAD_THEME_DARK)  == BST_CHECKED) s->theme = THEME_DARK;
            else if (IsDlgButtonChecked(hwnd, IDC_RAD_THEME_LIGHT) == BST_CHECKED) s->theme = THEME_LIGHT;
            else                                                                    s->theme = THEME_SYSTEM; /* checked radio wins; fall through to THEME_SYSTEM */
            if      (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_ALPHA) == BST_CHECKED) s->sort_mode = SORT_ALPHA;
            else if (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_DATE)  == BST_CHECKED) s->sort_mode = SORT_DATE;
            else if (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_USED)  == BST_CHECKED) s->sort_mode = SORT_MOST_USED;
            else                                                                   s->sort_mode = SORT_ORDER; /* fall through to SORT_ORDER (GitHub/disk order) */
            s->tint_script_sources = IsDlgButtonChecked(hwnd, IDC_CHK_TINT_SOURCES) == BST_CHECKED;

            /* Quick Bar */
            s->qbar_enabled              = IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_ENABLE) == BST_CHECKED;
            s->qbar_horizontal           = IsDlgButtonChecked(hwnd, IDC_RAD_QBAR_HORIZ)  == BST_CHECKED;
            s->qbar_topmost_with_catia   = IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_TOPMOST)== BST_CHECKED;
            s->qbar_repeat_on_dblclick   = IsDlgButtonChecked(hwnd, IDC_CHK_REPEAT_QBAR) == BST_CHECKED;
            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_S,     s->qbar_target_app, MAX_NAME);
            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, s->qbar_target_exe, MAX_NAME);

            Settings_Save(s);
            SHCreateDirectoryEx(NULL, s->cache_dir, NULL); /* create cache dir if it doesn't exist yet */

            /* ── Apply side effects ─────────────────────────────── */
            App_ResolveTheme();
            if (g.dark_mode != old_dark) {
                App_RebuildGDI();
                Window_ApplyDarkMode(g.hwnd);
                Window_ApplyThemeToChildren(g.hwnd);
                QuickBar_OnThemeChange();
                Tabs_RebuildButtons();
                InvalidateRect(g.hwnd, NULL, TRUE);
            }
            Window_ApplyAlwaysOnTop();
            Settings_ApplyAutorun(s->start_with_windows, s->start_minimized);
            Tabs_ApplySort(g.active_tab);
            Tabs_RebuildButtons();
            if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE); /* repaint quick bar in case tint setting changed */

            if (s->qbar_enabled) {
                if (s->qbar_horizontal != old_qbar_horiz) {
                    /* orientation changed — must destroy and recreate the window */
                    QuickBar_Destroy();
                    QuickBar_Register(GetModuleHandle(NULL));
                    QuickBar_Create();
                    QuickBar_Rebuild();
                } else if (!old_qbar_en) {
                    QuickBar_Show(true); /* was disabled, now enabled — just show it */
                }
            } else if (old_qbar_en) {
                QuickBar_Show(false); /* was enabled, now disabled — hide it */
            }

            EndDialog(hwnd, IDOK);
            break;
        }

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/*  AboutDlgProc                                                        */
/*  Purpose: Dialog procedure for the About dialog (IDD_ABOUT).        */
/*           Loads the app icon, sets a bold title font, fills the     */
/*           version string, and opens the GitHub Pages on button click.*/
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID on WM_COMMAND)                      */
/*       lp   — LPARAM (unused)                                        */
/*  Out: INT_PTR — TRUE for handled messages; FALSE otherwise          */
/* ================================================================== */
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static HFONT s_hFontTitle = NULL;
    static HICON s_hIconApp   = NULL;

    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
    {
        /* App icon at 48×48 — standard large icon size */
        s_hIconApp = (HICON)LoadImage(GetModuleHandle(NULL),
                                      MAKEINTRESOURCE(IDI_APP_ICON),
                                      IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
        if (s_hIconApp)
            SendDlgItemMessage(hwnd, IDC_ABOUT_ICON, STM_SETICON,
                               (WPARAM)s_hIconApp, 0);

        /* Bold title font — -18 logical units ≈ 14 pt at 96 DPI */
        s_hFontTitle = CreateFont(
            -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (s_hFontTitle)
            SendDlgItemMessage(hwnd, IDC_ABOUT_TITLE, WM_SETFONT,
                               (WPARAM)s_hFontTitle, TRUE);

        /* Version line */
        SetDlgItemTextW(hwnd, IDC_ABOUT_VER,
                        L"Version " VERSION_DISPLAY_W);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_ABOUT_GITHUB:
            ShellExecuteW(hwnd, L"open",
                          L"https://kaiur.github.io/CatiaMenuWin32/",
                          NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDOK:
        case IDCANCEL:
            if (s_hFontTitle) { DeleteObject(s_hFontTitle); s_hFontTitle = NULL; } /* free GDI font to avoid leak */
            if (s_hIconApp)   { DestroyIcon(s_hIconApp);    s_hIconApp   = NULL; } /* free icon handle to avoid leak */
            EndDialog(hwnd, IDOK);
            break;
        }
        return TRUE;
    }
    return FALSE;
}
