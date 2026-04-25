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
static bool IsNewer(const WCHAR *remote_tag)
{
    /* Strip leading 'v' or 'V' */
    const WCHAR *r = remote_tag;
    if (*r == L'v' || *r == L'V') r++;

    /* The workflow produces tags like "v1.0.0.8" (major.minor.patch.build).
       VERSION_STRING_W is also "major.minor.patch.build" e.g. "1.0.0.8".
       Compare all 4 parts so that a higher build number on a release IS
       correctly detected as newer than a locally built older binary. */
    int rv[4] = {0}, lv[4] = {0};
    swscanf(r, L"%d.%d.%d.%d", &rv[0], &rv[1], &rv[2], &rv[3]);
    swscanf(VERSION_STRING_W, L"%d.%d.%d.%d", &lv[0], &lv[1], &lv[2], &lv[3]);

    for (int i = 0; i < 4; i++) {
        if (rv[i] > lv[i]) return true;
        if (rv[i] < lv[i]) return false;
    }
    return false; /* equal */
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

    /* Skip update check for local/dev builds.
       Default to skipping if IS_LOCAL_BUILD not defined in version.h
       (means CMake hasn't regenerated it yet - treat as local build). */
#ifndef IS_LOCAL_BUILD
#define IS_LOCAL_BUILD 1
#endif
    if (IS_LOCAL_BUILD) return 0;


    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) return 1;

    WCHAR api_path[256];
    _snwprintf(api_path, 255,
               L"/repos/%s/CatiaMenuWin32/releases/latest", GITHUB_OWNER);

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
        PostMessage(g.hwnd, WM_UPDATE_AVAIL, 0, 0);
    }

    free(buf);
    return 0;
}

/* ================================================================== */
/*  Updater_PromptAndInstall                                            */
/* ================================================================== */
void Updater_PromptAndInstall(const WCHAR *latest_tag)
{
    WCHAR msg[512];
    _snwprintf(msg, 511,
        L"A new version of CatiaMenuWin32 is available!\n\n"
        L"  Current version:  %s\n"
        L"  Latest version:   v%s\n\n"
        L"Would you like to open the releases page to download the update?",
        VERSION_STRING_W, latest_tag);

    int res = MessageBox(g.hwnd, msg, L"Update Available",
                         MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);

    if (res == IDYES) {
        /* Open the releases page in the browser */
        WCHAR url[256];
        _snwprintf(url, 255,
                   L"https://github.com/%s/CatiaMenuWin32/releases/latest",
                   GITHUB_OWNER);
        ShellExecute(NULL, L"open", url, NULL, NULL, SW_SHOW);
    }
}
