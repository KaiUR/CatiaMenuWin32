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
/*           keys (auto-sync on, 6-hour refresh, system theme, etc.)  */
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
    s->auto_update         = GetPrivateProfileInt(L"Options", L"AutoUpdate",         0, ini) != 0;
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
        if (!s->extra_repos[i].branch[0]) wcsncpy(s->extra_repos[i].branch, L"main", 63);
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
    s->minimize_to_tray    = GetPrivateProfileInt(L"Window",  L"MinimizeToTray",    0, ini) != 0;
    s->start_with_windows  = GetPrivateProfileInt(L"Window",  L"StartWithWindows",  0, ini) != 0;
    s->start_minimized     = GetPrivateProfileInt(L"Window",  L"StartMinimized",    1, ini) != 0;
    s->theme               = (ThemeMode)GetPrivateProfileInt(L"Window", L"Theme",   0, ini);
    if (s->theme < 0 || s->theme > 2) s->theme = THEME_SYSTEM;

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
#undef WB

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
            wcsncpy(cmd, exe, MAX_APPPATH + 15);

        RegSetValueEx(hk, AUTORUN_NAME, 0, REG_SZ,
                      (BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(WCHAR)));
    } else {
        RegDeleteValue(hk, AUTORUN_NAME);
    }
    RegCloseKey(hk);
}

/* ================================================================== */
/*  SettingsDlgProc                                                     */
/*  Purpose: Dialog procedure for the Settings dialog (IDD_SETTINGS).  */
/*           WM_INITDIALOG populates all controls from g.cfg; IDOK     */
/*           reads them back and calls Settings_Save.  Handles browse  */
/*           buttons for Python and cache folder, the token enable     */
/*           checkbox, and the Reset to Defaults button.               */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID / notification code in LOWORD/HIWORD)*/
/*       lp   — LPARAM (unused)                                        */
/*  Out: INT_PTR — TRUE for handled messages; FALSE otherwise          */
/* ================================================================== */
INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        Settings *s = &g.cfg;
        SetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe);
        SetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir);
        SetDlgItemText(hwnd, IDC_EDIT_TOKEN,  s->github_token);
        CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,  s->auto_sync           ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,  s->download_before_run ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_CONSOLE,       s->show_console        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_KEEP_OPEN,     s->console_keep_open   ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES, s->check_updates       ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_AUTO_UPDATE,    s->auto_update          ? BST_CHECKED : BST_UNCHECKED);

        WCHAR ri[8];
        _snwprintf_s(ri, 7, _TRUNCATE, L"%d", s->refresh_interval);
        SetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, ri);
        CheckDlgButton(hwnd, IDC_CHK_DEPS_KEEP_OPEN, s->deps_keep_open     ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN), s->show_console);
        bool has_tok = (s->github_token[0] != L'\0');
        CheckDlgButton(hwnd, IDC_CHK_TOKEN, has_tok ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN), has_tok);
        return TRUE;
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
        case IDC_CHK_CONSOLE:
            EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE) == BST_CHECKED);
            break;
        case IDC_CHK_TOKEN:
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED);
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

            /* Apply defaults to current settings struct */
            Settings *s = &g.cfg;
            s->python_exe[0]       = L'\0';
            s->github_token[0]     = L'\0';
            s->auto_sync           = true;
            s->download_before_run = false;
            s->show_console        = false;
            s->console_keep_open   = true;
            s->deps_keep_open      = false;
            s->check_updates       = true;
            s->always_on_top       = true;
            s->minimize_to_tray    = true;
            s->start_with_windows  = true;
            s->start_minimized     = false;
            s->theme               = THEME_SYSTEM;

            /* Reset cache dir to default */
            _snwprintf_s(s->cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\scripts", g.appdata_dir);

            /* Auto-detect python */
            Runner_FindPython(s->python_exe, MAX_APPPATH);

            /* Reload dialog controls with new defaults */
            SetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe);
            SetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir);
            SetDlgItemText(hwnd, IDC_EDIT_TOKEN,  L"");
            CheckDlgButton(hwnd, IDC_CHK_AUTOSYNC,      BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_DOWNLOAD,      BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_CONSOLE,       BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_KEEP_OPEN,     BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_DEPS_KEEP_OPEN, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_CHECK_UPDATES, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHK_TOKEN,         BST_UNCHECKED);
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN),  FALSE);
            EnableWindow(GetDlgItem(hwnd, IDC_CHK_KEEP_OPEN), FALSE);
            break;
        }

        case IDOK:
        {
            Settings *s = &g.cfg;
            GetDlgItemText(hwnd, IDC_EDIT_PYTHON, s->python_exe, MAX_APPPATH);
            GetDlgItemText(hwnd, IDC_EDIT_CACHE,  s->cache_dir,  MAX_APPPATH);
            if (IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED)
                GetDlgItemText(hwnd, IDC_EDIT_TOKEN, s->github_token, 256);
            else
                s->github_token[0] = L'\0';
            s->auto_sync           = IsDlgButtonChecked(hwnd, IDC_CHK_AUTOSYNC) == BST_CHECKED;
            s->download_before_run = IsDlgButtonChecked(hwnd, IDC_CHK_DOWNLOAD) == BST_CHECKED;
            s->show_console        = IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE)       == BST_CHECKED;
            s->console_keep_open   = IsDlgButtonChecked(hwnd, IDC_CHK_KEEP_OPEN)     == BST_CHECKED;
            s->check_updates       = IsDlgButtonChecked(hwnd, IDC_CHK_CHECK_UPDATES)  == BST_CHECKED;
            s->auto_update         = IsDlgButtonChecked(hwnd, IDC_CHK_AUTO_UPDATE)     == BST_CHECKED;
            WCHAR ri_buf[8] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_REFRESH_INTERVAL, ri_buf, 7);
            s->refresh_interval = _wtoi(ri_buf);
            if (s->refresh_interval < 0)  s->refresh_interval = 0;
            if (s->refresh_interval > 168) s->refresh_interval = 168;
            s->deps_keep_open      = IsDlgButtonChecked(hwnd, IDC_CHK_DEPS_KEEP_OPEN) == BST_CHECKED;
            Settings_Save(s);
            SHCreateDirectoryEx(NULL, s->cache_dir, NULL);
            EndDialog(hwnd, IDOK);
            break;
        }
        case IDCANCEL: EndDialog(hwnd, IDCANCEL); break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/*  AboutDlgProc                                                        */
/*  Purpose: Dialog procedure for the About dialog (IDD_ABOUT).        */
/*           No dynamic content; dismisses on OK or Cancel.            */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (IDOK / IDCANCEL on WM_COMMAND)                 */
/*       lp   — LPARAM (unused)                                        */
/*  Out: INT_PTR — TRUE for handled messages; FALSE otherwise          */
/* ================================================================== */
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
            EndDialog(hwnd, IDOK);
        return TRUE;
    }
    return FALSE;
}
