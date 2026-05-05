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

/* ------------------------------------------------------------------ */
/*  Simple semver compare: "v1.2.3" vs "v1.2.4"                       */
/*  Returns true if remote > local.                                    */
/* ------------------------------------------------------------------ */
/* Parse "1.2.3.4" into 4 integers - avoids swscanf locale issues on MinGW */
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

/* ------------------------------------------------------------------ */
/*  Parse "tag_name" from the releases/latest JSON response            */
/* ------------------------------------------------------------------ */
static bool ParseLatestTag(const char *json, WCHAR *tag_out, int max)
{
    const char *p = strstr(json, "\"tag_name\"");
    if (!p) return false;
    p += strlen("\"tag_name\"");
    while (*p == ' ' || *p == ':' || *p == ' ') p++;
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
/*  Updater_CheckThread  -  runs in background                         */
/* ================================================================== */
DWORD WINAPI Updater_CheckThread(LPVOID unused)
{
    (void)unused;

    /* Wait for sync thread to finish its API calls first */
    Sleep(3000);

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
        wcsncpy(g.latest_version, display_tag, 31);

        if (g.cfg.auto_update) {
            /* Auto-update: download and install on main thread */
            PostMessage(g.hwnd, WM_UPDATE_AVAIL, 1, 0);  /* wParam=1 means auto */
        } else {
            PostMessage(g.hwnd, WM_UPDATE_AVAIL, 0, 0);
        }
    }

    free(buf);
    return 0;
}

/* ================================================================== */
/*  Updater_PromptAndInstall                                            */
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
    wcscat(temp_path, L"CatiaMenuWin32_update.exe");

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

    /* Write batch script to replace exe and restart */
    WCHAR bat_path[MAX_APPPATH] = {0};
    GetTempPath(MAX_APPPATH - 1, bat_path);
    wcscat(bat_path, L"CatiaMenuWin32_update.bat");

    char temp_path_a[MAX_APPPATH] = {0};
    char exe_path_a[MAX_APPPATH]  = {0};
    char bat_path_a[MAX_APPPATH]  = {0};
    WideCharToMultiByte(CP_ACP, 0, temp_path, -1, temp_path_a, MAX_APPPATH-1, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, exe_path,  -1, exe_path_a,  MAX_APPPATH-1, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, bat_path,  -1, bat_path_a,  MAX_APPPATH-1, NULL, NULL);

    FILE *f = fopen(bat_path_a, "w");
    if (!f) { Updater_PromptAndInstall(latest_tag); return; }
    fprintf(f, "@echo off\n");
    fprintf(f, "timeout /t 2 /nobreak >nul\n");
    fprintf(f, "copy /y \"%s\" \"%s\"\n", temp_path_a, exe_path_a);
    fprintf(f, "if errorlevel 1 goto fail\n");
    fprintf(f, "start \"\" \"%s\"\n", exe_path_a);
    fprintf(f, "del \"%s\"\n", temp_path_a);
    fprintf(f, "del \"%%~f0\"\n");
    fprintf(f, "exit\n");
    fprintf(f, ":fail\n");
    fprintf(f, "echo Update failed - could not copy file.\n");
    fprintf(f, "pause\n");
    fclose(f);

    ShellExecuteW(NULL, L"open", bat_path, NULL, NULL, SW_HIDE);
    Sleep(500); /* brief pause to let batch start before we exit */
    PostMessage(g.hwnd, WM_CLOSE, 1, 0); /* wp=1 = force quit, bypass tray */
}

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
