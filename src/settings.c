/*
 * settings.c  -  Settings load/save, autorun, dialogs.
 * CatiaMenuWin32
 */

#include "main.h"

static void IniPath(WCHAR *out, int max) {
    _snwprintf(out, max-1, L"%s\\%s", g.appdata_dir, SETTINGS_FILE);
}

/* ================================================================== */
/*  Settings_Load                                                       */
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
    s->always_on_top       = GetPrivateProfileInt(L"Window",  L"AlwaysOnTop",       1, ini) != 0;
    s->minimize_to_tray    = GetPrivateProfileInt(L"Window",  L"MinimizeToTray",    0, ini) != 0;
    s->start_with_windows  = GetPrivateProfileInt(L"Window",  L"StartWithWindows",  0, ini) != 0;
    s->start_minimized     = GetPrivateProfileInt(L"Window",  L"StartMinimized",    1, ini) != 0;
    s->theme               = (ThemeMode)GetPrivateProfileInt(L"Window", L"Theme",   0, ini);
    if (s->theme < 0 || s->theme > 2) s->theme = THEME_SYSTEM;

    if (!s->cache_dir[0])
        _snwprintf(s->cache_dir, MAX_APPPATH-1, L"%s\\scripts", g.appdata_dir);
    SHCreateDirectoryEx(NULL, s->cache_dir, NULL);

    if (!s->python_exe[0])
        Runner_FindPython(s->python_exe, MAX_APPPATH);
}

/* ================================================================== */
/*  Settings_Save                                                       */
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
    WB(L"Window",  L"AlwaysOnTop",       s->always_on_top);
    WB(L"Window",  L"MinimizeToTray",    s->minimize_to_tray);
    WB(L"Window",  L"StartWithWindows",  s->start_with_windows);
    WB(L"Window",  L"StartMinimized",    s->start_minimized);
#undef WB

    _snwprintf(tmp, 7, L"%d", (int)s->theme);
    WritePrivateProfileString(L"Window", L"Theme", tmp, ini);
}

/* ================================================================== */
/*  Settings_ApplyAutorun                                               */
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
            _snwprintf(cmd, MAX_APPPATH + 15, L"\"%s\" /minimized", exe);
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
        CheckDlgButton(hwnd, IDC_CHK_CONSOLE,   s->show_console        ? BST_CHECKED : BST_UNCHECKED);
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
        case IDC_CHK_TOKEN:
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_TOKEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_TOKEN) == BST_CHECKED);
            break;
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
            s->show_console        = IsDlgButtonChecked(hwnd, IDC_CHK_CONSOLE)  == BST_CHECKED;
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
