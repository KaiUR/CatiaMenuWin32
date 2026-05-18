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

    s->auto_sync           = GetPrivateProfileInt(L"Options", L"AutoSync",          1, ini) != 0;
    s->download_before_run = GetPrivateProfileInt(L"Options", L"DownloadBeforeRun", 0, ini) != 0;
    s->show_console        = GetPrivateProfileInt(L"Options", L"ShowConsole",       0, ini) != 0;
    s->console_keep_open   = GetPrivateProfileInt(L"Options", L"ConsoleKeepOpen",  1, ini) != 0;
    s->check_updates       = GetPrivateProfileInt(L"Options", L"CheckUpdates",      1, ini) != 0;
    s->deps_keep_open      = GetPrivateProfileInt(L"Options", L"DepsKeepOpen",      0, ini) != 0;
    s->auto_update         = GetPrivateProfileInt(L"Options", L"AutoUpdate",         1, ini) != 0;
    s->refresh_interval    = GetPrivateProfileInt(L"Options", L"RefreshInterval",    6, ini);
    s->sort_mode           = (SortMode)GetPrivateProfileInt(L"Options", L"SortMode", 0, ini);
    s->main_repo_enabled   = GetPrivateProfileInt(L"Sources", L"MainRepoEnabled",   1, ini) != 0;

    /* Extra repos */
    s->extra_repo_count = GetPrivateProfileInt(L"Sources", L"ExtraRepoCount", 0, ini);
    if (s->extra_repo_count > MAX_EXTRA_REPOS) s->extra_repo_count = MAX_EXTRA_REPOS;
    for (int i = 0; i < s->extra_repo_count; i++) {
        WCHAR key[32];
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dUrl",     i); GetPrivateProfileString(L"Sources", key, L"", s->extra_repos[i].url,    511, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dBranch",  i); GetPrivateProfileString(L"Sources", key, L"main", s->extra_repos[i].branch, 63, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dToken",   i); GetPrivateProfileString(L"Sources", key, L"", s->extra_repos[i].token,  255, ini);
        _snwprintf_s(key, 31, _TRUNCATE, L"Repo%dEnabled", i); s->extra_repos[i].enabled = GetPrivateProfileInt(L"Sources", key, 1, ini) != 0;
        if (!s->extra_repos[i].branch[0]) wcsncpy_s(s->extra_repos[i].branch, 64, L"main", _TRUNCATE);
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
    if (s->theme < 0 || s->theme > 2) s->theme = THEME_SYSTEM;

    /* Quick Launch Bar */
    s->qbar_enabled            = GetPrivateProfileInt(L"QuickBar", L"Enabled",          1, ini) != 0;
    s->qbar_horizontal         = GetPrivateProfileInt(L"QuickBar", L"Horizontal",       0, ini) != 0;
    s->qbar_topmost_with_catia = GetPrivateProfileInt(L"QuickBar", L"TopmostWithCatia", 1, ini) != 0;
    s->qbar_x                  = GetPrivateProfileInt(L"QuickBar", L"X",                0, ini);
    s->qbar_y                  = GetPrivateProfileInt(L"QuickBar", L"Y",                0, ini);
    GetPrivateProfileString(L"QuickBar", L"TargetApp", L"CATIA V5",
                            s->qbar_target_app, MAX_NAME, ini);
    GetPrivateProfileString(L"QuickBar", L"TargetExe", L"CNEXT.exe",
                            s->qbar_target_exe, MAX_NAME, ini);

    /* Double-click repeat */
    s->repeat_on_dblclick      = GetPrivateProfileInt(L"Options", L"RepeatOnDblClick",    1, ini) != 0;
    s->qbar_repeat_on_dblclick = GetPrivateProfileInt(L"Options", L"QBarRepeatOnDblClick",1, ini) != 0;

    if (!s->cache_dir[0])
        _snwprintf_s(s->cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\scripts", g.appdata_dir);
    SHCreateDirectoryEx(NULL, s->cache_dir, NULL);

    if (!s->python_exe[0])
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

#define WB(sec,key,val) WritePrivateProfileString(sec, key, (val)?L"1":L"0", ini)
    WB(L"Options", L"AutoSync",          s->auto_sync);
    WB(L"Options", L"DownloadBeforeRun", s->download_before_run);
    WB(L"Options", L"ShowConsole",       s->show_console);
    WB(L"Options", L"ConsoleKeepOpen",  s->console_keep_open);
    WB(L"Options", L"CheckUpdates",      s->check_updates);
    WB(L"Options", L"DepsKeepOpen",      s->deps_keep_open);
    WB(L"Options", L"AutoUpdate",         s->auto_update);
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
        WCHAR cmd[MAX_APPPATH + 16];
        if (minimized)
            _snwprintf_s(cmd, MAX_APPPATH + 15, _TRUNCATE, L"\"%s\" /minimized", exe);
        else
            wcsncpy_s(cmd, MAX_APPPATH + 16, exe, _TRUNCATE);

        RegSetValueEx(hk, AUTORUN_NAME, 0, REG_SZ,
                      (BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(WCHAR)));
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
    IDC_GRP_CACHE,  IDC_LBL_CACHE_PATH,  IDC_EDIT_CACHE,  IDC_BTN_BROWSE_DIR,
    IDC_GRP_TOKEN,  IDC_CHK_TOKEN,       IDC_EDIT_TOKEN,
    -1
};
static const int s_stab1[] = {   /* Sync */
    IDC_GRP_SYNC,
    IDC_CHK_AUTOSYNC, IDC_CHK_DOWNLOAD, IDC_CHK_CHECK_UPDATES, IDC_CHK_AUTO_UPDATE,
    IDC_LBL_REFRESH1, IDC_EDIT_REFRESH_INTERVAL, IDC_LBL_REFRESH2,
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
            TCITEM ti = { TCIF_TEXT, 0, 0, NULL, 0, -1, 0 };
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
            bool has_tok = (s->github_token[0] != L'\0');
            CheckDlgButton(hwnd, IDC_CHK_TOKEN, has_tok ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemText(hwnd, IDC_EDIT_TOKEN, s->github_token);
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN), has_tok);
        }

        /* ── Tab 1: Sync ─────────────────────────────────────────── */
        CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,      s->auto_sync           ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,      s->download_before_run ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES, s->check_updates       ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_AUTO_UPDATE,   s->auto_update         ? BST_CHECKED : BST_UNCHECKED);
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
        EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN), s->show_console);

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
        if (nm->idFrom == IDC_TAB_SETTINGS && nm->code == TCN_SELCHANGE) {
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
                SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, slash ? slash + 1 : path);
            }
            break;
        }

        case IDC_CHK_CONSOLE:
            EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE) == BST_CHECKED);
            break;

        case IDC_CHK_TOKEN:
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED);
            break;

        case IDC_CHK_QBAR_ENABLE:
            Settings_QBarEnableControls(hwnd,
                IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_ENABLE) == BST_CHECKED);
            break;

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

            CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,      BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,      BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_AUTO_UPDATE,   BST_CHECKED);
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
            {
                WCHAR ri[8] = {0};
                GetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, ri, 7);
                s->refresh_interval = _wtoi(ri);
                if (s->refresh_interval < 0)   s->refresh_interval = 0;
                if (s->refresh_interval > 168) s->refresh_interval = 168;
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
            else                                                                    s->theme = THEME_SYSTEM;
            if      (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_ALPHA) == BST_CHECKED) s->sort_mode = SORT_ALPHA;
            else if (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_DATE)  == BST_CHECKED) s->sort_mode = SORT_DATE;
            else if (IsDlgButtonChecked(hwnd, IDC_RAD_SORT_USED)  == BST_CHECKED) s->sort_mode = SORT_MOST_USED;
            else                                                                   s->sort_mode = SORT_ORDER;

            /* Quick Bar */
            s->qbar_enabled              = IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_ENABLE) == BST_CHECKED;
            s->qbar_horizontal           = IsDlgButtonChecked(hwnd, IDC_RAD_QBAR_HORIZ)  == BST_CHECKED;
            s->qbar_topmost_with_catia   = IsDlgButtonChecked(hwnd, IDC_CHK_QBAR_TOPMOST)== BST_CHECKED;
            s->qbar_repeat_on_dblclick   = IsDlgButtonChecked(hwnd, IDC_CHK_REPEAT_QBAR) == BST_CHECKED;
            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_S,     s->qbar_target_app, MAX_NAME);
            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE_S, s->qbar_target_exe, MAX_NAME);

            Settings_Save(s);
            SHCreateDirectoryEx(NULL, s->cache_dir, NULL);

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

            if (s->qbar_enabled) {
                if (s->qbar_horizontal != old_qbar_horiz) {
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
        /* App icon at 48×48 */
        s_hIconApp = (HICON)LoadImage(GetModuleHandle(NULL),
                                      MAKEINTRESOURCE(IDI_APP_ICON),
                                      IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
        if (s_hIconApp)
            SendDlgItemMessage(hwnd, IDC_ABOUT_ICON, STM_SETICON,
                               (WPARAM)s_hIconApp, 0);

        /* Bold title font (~14 pt) */
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
            if (s_hFontTitle) { DeleteObject(s_hFontTitle); s_hFontTitle = NULL; }
            if (s_hIconApp)   { DestroyIcon(s_hIconApp);    s_hIconApp   = NULL; }
            EndDialog(hwnd, IDOK);
            break;
        }
        return TRUE;
    }
    return FALSE;
}
