/*
 * updater.c  -  Check GitHub releases for a newer version and self-update.
 *
 * Checks:  GET https://api.github.com/repos/KaiUR/CatiaMenuWin32/releases/latest
 * Compares tag_name (e.g. "v1.0.0") against VERSION_STRING_W.
 * If newer: prompts user -> downloads installer/zip -> launches it -> exits.
 *
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  ParseVersion  (static)                                             */
/*  Purpose: Splits a "major.minor.patch.build" version string into    */
/*           four integers. Avoids swscanf locale issues on MinGW.    */
/*  In:  s    — version string (leading 'v'/'V' must be stripped first)*/
/*       v[4] — output array filled with [major, minor, patch, build] */
/*  Out: (void — v[] populated; missing parts stay 0)                  */
/* ================================================================== */
static void ParseVersion(const WCHAR *s, int v[4])
{
    v[0] = v[1] = v[2] = v[3] = 0;
    int part = 0;
    for (const WCHAR *p = s; *p && part < 4; p++) {
        if (*p >= L'0' && *p <= L'9') {
            v[part] = v[part] * 10 + (*p - L'0');
        } else if (*p == L'.') {
            part++;
        }
    }
}

/* ================================================================== */
/*  IsNewer  (static)                                                  */
/*  Purpose: Compares a GitHub release tag against VERSION_STRING_W.   */
/*           Only major.minor.patch are compared; build number is      */
/*           ignored so a local development build does not suppress    */
/*           update notifications.                                     */
/*  In:  remote_tag — tag string from GitHub (e.g. "v1.3.10")         */
/*  Out: true if the remote version is strictly newer; false otherwise  */
/* ================================================================== */
static bool IsNewer(const WCHAR *remote_tag)
{
    /* Strip leading 'v' or 'V' */
    const WCHAR *r = remote_tag;
    if (*r == L'v' || *r == L'V') r++;

    int rv[4], lv[4];
    ParseVersion(r, rv);
    ParseVersion(VERSION_STRING_W, lv);

    /* Only compare major.minor.patch (first 3 parts).
       Build number (4th part) is ignored - a local build with a higher
       build number than the latest release should not prompt for update. */
    for (int i = 0; i < 3; i++) {
        if (rv[i] > lv[i]) return true;
        if (rv[i] < lv[i]) return false;
    }
    return false;
}

/* ================================================================== */
/*  ParseLatestTag  (static)                                           */
/*  Purpose: Extracts the "tag_name" value from the GitHub releases    */
/*           /latest JSON response and converts it to a wide string.  */
/*  In:  json    — null-terminated UTF-8 JSON response body           */
/*       tag_out — buffer to receive the wide-string tag              */
/*       max     — capacity of tag_out in WCHARs                      */
/*  Out: true and tag_out filled if "tag_name" was found; false otherwise*/
/* ================================================================== */
static bool ParseLatestTag(const char *json, WCHAR *tag_out, int max)
{
    const char *p = strstr(json, "\"tag_name\"");
    if (!p) return false;
    p += strlen("\"tag_name\"");
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;
    int i = 0;
    char tmp[64] = {0};
    while (*p && *p != '"' && i < 62) tmp[i++] = *p++;
    tmp[i] = '\0';
    MultiByteToWideChar(CP_UTF8, 0, tmp, -1, tag_out, max);
    return (tag_out[0] != L'\0');
}


/* ================================================================== */
/*  Updater_CheckThread                                                 */
/*  Purpose: Background thread that queries the GitHub releases API,   */
/*           compares the latest tag against the running version, and  */
/*           posts WM_UPDATE_AVAIL if a newer release is available.   */
/*           Pass non-NULL lpParam for a manual check: skips the 3 s  */
/*           startup delay and posts "up to date" status if current.  */
/*  In:  unused — NULL for auto-check; non-NULL for manual check       */
/*  Out: 0 on success; 1 on HTTP or parse failure                      */
/* ================================================================== */
DWORD WINAPI Updater_CheckThread(LPVOID unused)
{
    bool manual = (unused != NULL);

    /* Auto-check: wait for sync thread to finish its API calls first */
    if (!manual) Sleep(3000);

    if (manual) PostStatus(L"Checking for updates…");

    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) return 1;

    WCHAR api_path[256];
    _snwprintf_s(api_path, 255, _TRUNCATE, L"/repos/%s/CatiaMenuWin32/releases/latest", GITHUB_OWNER);

    DWORD len = 0;
    bool ok = GitHub_HttpGet(GITHUB_API_HOST, api_path,
                              g.cfg.github_token[0] ? g.cfg.github_token : NULL,
                              buf, &len);

    if (!ok || len == 0) { free(buf); return 1; }

    WCHAR tag[32] = {0};
    if (!ParseLatestTag(buf, tag, 32)) { free(buf); return 1; }

    if (IsNewer(tag)) {
        /* Strip leading 'v' before storing for display */
        const WCHAR *display_tag = tag;
        if (display_tag[0] == L'v' || display_tag[0] == L'V')
            display_tag++;
        wcsncpy_s(g.latest_version, 32, display_tag, _TRUNCATE);

        if (g.cfg.auto_update) {
            /* Auto-update: download and install (honoured for both auto and manual check) */
            PostMessage(g.hwnd, WM_UPDATE_AVAIL, 1, 0);  /* wParam=1 means auto */
        } else {
            PostMessage(g.hwnd, WM_UPDATE_AVAIL, 0, 0);
        }
    } else if (manual) {
        PostStatus(L"App is up to date (v%s).", VERSION_STRING_W);
    }

    free(buf);
    return 0;
}

/* ================================================================== */
/*  Updater_AutoUpdate                                                  */
/*  Purpose: Prompts the user to confirm an automatic update, then     */
/*           downloads the new executable via WinINet, writes a batch  */
/*           script that replaces the running exe and restarts it, and */
/*           exits. Falls back to Updater_PromptAndInstall on failure. */
/*  In:  latest_tag — version string without leading 'v' (e.g. "1.3.10")*/
/*  Out: (void — exits the process on success; falls back otherwise)   */
/* ================================================================== */
void Updater_AutoUpdate(const WCHAR *latest_tag)
{
    /* Download latest release exe and replace self */
    WCHAR url[512];
    _snwprintf_s(url, 511, _TRUNCATE,
        L"https://github.com/KaiUR/CatiaMenuWin32/releases/download/v%s/CatiaMenuWin32.exe",
        latest_tag);

    /* Download to temp file */
    WCHAR temp_path[MAX_APPPATH] = {0};
    GetTempPath(MAX_APPPATH - 1, temp_path);
    wcsncat_s(temp_path, MAX_APPPATH, L"CatiaMenuWin32_update.exe", _TRUNCATE);

    /* Delete any leftover temp file from a previous attempt */
    DeleteFile(temp_path);

    int res = MessageBox(g.hwnd,
        L"A new version is available. Download and install automatically?\n\n"
        L"The application will restart after the update.",
        L"Auto Update", MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);

    if (res != IDYES) {
        Updater_PromptAndInstall(latest_tag);
        return;
    }

    /* Get current exe path before anything else */
    WCHAR exe_path[MAX_APPPATH] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_APPPATH - 1);

    /* Use WinINet directly to download — URLDownloadToFile blocks the
       UI thread on some systems. Use GitHub_HttpGet instead which we
       already trust. */
    PostStatus(L"Downloading update v%s...", latest_tag);

    /* Convert URL to path component for GitHub_HttpGet */
    WCHAR raw_path[512] = {0};
    _snwprintf_s(raw_path, 511, _TRUNCATE,
        L"/KaiUR/CatiaMenuWin32/releases/download/v%s/CatiaMenuWin32.exe",
        latest_tag);

    char *buf = (char *)malloc(8 * 1024 * 1024); /* 8MB for exe */
    if (!buf) { Updater_PromptAndInstall(latest_tag); return; }

    DWORD len = 8 * 1024 * 1024; /* tell HttpGet our buffer size */
    bool ok = GitHub_HttpGet(L"github.com", raw_path,
                             g.cfg.github_token[0] ? g.cfg.github_token : NULL,
                             buf, &len);

    if (!ok || len == 0) {
        free(buf);
        MessageBox(g.hwnd, L"Download failed. Opening releases page instead.",
                   L"Update", MB_ICONWARNING | MB_OK);
        Updater_PromptAndInstall(latest_tag);
        return;
    }

    /* Write downloaded exe to temp file */
    HANDLE hf = CreateFileW(temp_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        free(buf);
        Updater_PromptAndInstall(latest_tag);
        return;
    }
    DWORD written = 0;
    WriteFile(hf, buf, len, &written, NULL);
    CloseHandle(hf);
    free(buf);

    if (written != len || GetFileAttributes(temp_path) == INVALID_FILE_ATTRIBUTES) {
        DeleteFile(temp_path);
        MessageBox(g.hwnd, L"Download incomplete. Opening releases page instead.",
                   L"Update", MB_ICONWARNING | MB_OK);
        Updater_PromptAndInstall(latest_tag);
        return;
    }

    PostStatus(L"Update downloaded. Closing to install...");

    /* Write batch script to replace exe and restart.
       Use _wfopen_s + fwprintf_s with a UTF-16 BOM so cmd.exe can handle
       paths that contain non-ASCII characters (e.g. non-Latin user profiles). */
    WCHAR bat_path[MAX_APPPATH] = {0};
    GetTempPath(MAX_APPPATH - 1, bat_path);
    wcsncat_s(bat_path, MAX_APPPATH, L"CatiaMenuWin32_update.bat", _TRUNCATE);

    FILE *f = NULL;
    if (_wfopen_s(&f, bat_path, L"w,ccs=UTF-16LE") != 0 || !f) {
        Updater_PromptAndInstall(latest_tag); return;
    }
    fwprintf_s(f, L"@echo off\n");
    fwprintf_s(f, L"timeout /t 2 /nobreak >nul\n");
    fwprintf_s(f, L"copy /y \"%s\" \"%s\"\n", temp_path, exe_path);
    fwprintf_s(f, L"if errorlevel 1 goto fail\n");
    fwprintf_s(f, L"start \"\" \"%s\"\n", exe_path);
    fwprintf_s(f, L"del \"%s\"\n", temp_path);
    fwprintf_s(f, L"del \"%%~f0\"\n");
    fwprintf_s(f, L"exit\n");
    fwprintf_s(f, L":fail\n");
    fwprintf_s(f, L"echo Update failed - could not copy file.\n");
    fwprintf_s(f, L"pause\n");
    fclose(f);

    ShellExecuteW(NULL, L"open", bat_path, NULL, NULL, SW_HIDE);
    Sleep(500); /* brief pause to let batch start before we exit */
    PostMessage(g.hwnd, WM_CLOSE, 1, 0); /* wp=1 = force quit, bypass tray */
}

/* ================================================================== */
/*  Updater_PromptAndInstall                                            */
/*  Purpose: Shows a message box informing the user a newer release is */
/*           available and, if confirmed, opens the GitHub releases     */
/*           page in the default browser.                              */
/*  In:  latest_tag — version string without leading 'v' (e.g. "1.3.10")*/
/*  Out: (void — may open browser; no process exit)                    */
/* ================================================================== */
void Updater_PromptAndInstall(const WCHAR *latest_tag)
{
    WCHAR msg[512];
    _snwprintf_s(msg, 511, _TRUNCATE, L"A new version of CatiaMenuWin32 is available!\n\n"
        L"  Current version:  v%s\n"
        L"  Latest version:   v%s\n\n"
        L"Would you like to open the releases page to download the update?",
        VERSION_STRING_W, latest_tag);

    int res = MessageBox(g.hwnd, msg, L"Update Available",
                         MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);

    if (res == IDYES) {
        /* Open the releases page in the browser */
        WCHAR url[256];
        _snwprintf_s(url, 255, _TRUNCATE, L"https://github.com/%s/CatiaMenuWin32/releases/latest",
                   GITHUB_OWNER);
        ShellExecute(NULL, L"open", url, NULL, NULL, SW_SHOW);
    }
}
