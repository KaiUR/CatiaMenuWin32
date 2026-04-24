/*
 * github.c  -  WinINet HTTPS GET + JSON parsing for GitHub API v3.
 * CatiaMenuWin32
 */

#include "main.h"

/* ================================================================== */
/*  GitHub_HttpGet                                                      */
/*  HTTPS GET https://<host><path>  -> buf (NUL-terminated)            */
/* ================================================================== */
bool GitHub_HttpGet(const WCHAR *host, const WCHAR *path,
                    const WCHAR *token, char *buf, DWORD *len)
{
    *len = 0;

    HINTERNET hInet = InternetOpen(
        L"CatiaMenuWin32/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) return false;

    HINTERNET hConn = InternetConnect(
        hInet, host, INTERNET_DEFAULT_HTTPS_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return false; }

    DWORD flags = INTERNET_FLAG_SECURE
                | INTERNET_FLAG_RELOAD
                | INTERNET_FLAG_NO_CACHE_WRITE;

    HINTERNET hReq = HttpOpenRequest(
        hConn, L"GET", path, NULL, NULL, NULL, flags, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    HttpAddRequestHeaders(hReq,
        L"Accept: application/vnd.github.v3+json\r\n",
        (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);

    if (token && token[0]) {
        WCHAR auth[300];
        _snwprintf(auth, 299, L"Authorization: token %s\r\n", token);
        HttpAddRequestHeaders(hReq, auth, (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);
    }

    if (!HttpSendRequest(hReq, NULL, 0, NULL, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    /* Check HTTP status code */
    DWORD status = 0, ssz = sizeof(DWORD);
    HttpQueryInfo(hReq,
                  HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                  &status, &ssz, NULL);
    if (status != 200) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    DWORD total = 0, read = 0;
    while (InternetReadFile(hReq, buf + total,
                            HTTP_BUF_SIZE - total - 1, &read) && read) {
        total += read;
        if (total >= HTTP_BUF_SIZE - 1) break;
    }
    buf[total] = '\0';
    *len = total;

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return total > 0;
}

/* ================================================================== */
/*  GitHub_DownloadRaw                                                  */
/*  Download a raw file from raw.githubusercontent.com to local_path.  */
/* ================================================================== */
bool GitHub_DownloadRaw(const WCHAR *gh_path, const WCHAR *local_path,
                        const WCHAR *token)
{
    WCHAR raw_url[MAX_APPPATH * 2];
    _snwprintf(raw_url, MAX_APPPATH * 2 - 1,
               L"/%s/%s/%s/%s",
               GITHUB_OWNER, GITHUB_REPO, GITHUB_BRANCH, gh_path);

    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) return false;

    DWORD len = 0;
    bool  ok  = GitHub_HttpGet(GITHUB_RAW_HOST, raw_url,
                               token, buf, &len);
    if (ok && len > 0) {
        /* Ensure parent directory exists */
        WCHAR dir[MAX_APPPATH];
        wcsncpy(dir, local_path, MAX_APPPATH - 1);
        PathRemoveFileSpec(dir);
        SHCreateDirectoryEx(NULL, dir, NULL);

        HANDLE hf = CreateFile(local_path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hf, buf, len, &written, NULL);
            CloseHandle(hf);
            ok = (written == len);
        } else {
            ok = false;
        }
    }

    free(buf);
    return ok;
}

/* ================================================================== */
/*  Minimal JSON helpers (no external dependency)                      */
/* ================================================================== */

/*
 * json_str: find  "key":"value"  in json, copy value into out.
 * Returns pointer past the closing quote, or NULL.
 */
static const char *json_str(const char *p, const char *key,
                             char *out, int max)
{
    char needle[MAX_NAME];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    p = strstr(p, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return NULL;
    p++;   /* skip opening quote */

    int i = 0;
    while (*p && *p != '"' && i < max - 1) {
        if (*p == '\\') { p++; if (*p) out[i++] = *p++; }
        else            { out[i++] = *p++; }
    }
    out[i] = '\0';
    return (*p == '"') ? p + 1 : NULL;
}

/* ================================================================== */
/*  GitHub_ParseRoot                                                    */
/*  Populate g.folders[] from the top-level /contents/ response.       */
/* ================================================================== */
void GitHub_ParseRoot(const char *json)
{
    g.folder_count = 0;

    const char *p = json;
    while ((p = strstr(p, "\"type\"")) != NULL
           && g.folder_count < MAX_FOLDERS) {

        char type_val[32] = {0};
        const char *after = json_str(p, "type", type_val, sizeof(type_val));
        if (!after) { p++; continue; }

        if (strcmp(type_val, "dir") == 0) {
            /* Walk back to the opening '{' of this object */
            const char *obj = p;
            while (obj > json && *obj != '{') obj--;

            char name_a[MAX_NAME] = {0};
            json_str(obj, "name", name_a, sizeof(name_a));

            /* Skip the setup folder */
            if (_stricmp(name_a, "setup") == 0) { p = after; continue; }

            ScriptFolder *f = &g.folders[g.folder_count++];
            memset(f, 0, sizeof(*f));

            MultiByteToWideChar(CP_UTF8, 0,
                                name_a, -1, f->name, MAX_NAME);
            wcscpy(f->display, f->name);
            Util_SnakeToTitle(f->display);
        }
        p = after;
    }
}

/* ================================================================== */
/*  GitHub_ParseFolder                                                  */
/*  Populate f->scripts[] from a folder /contents/ response.           */
/*  Stores the GitHub blob SHA for each file.                           */
/* ================================================================== */
void GitHub_ParseFolder(const char *json, int fi)
{
    ScriptFolder *f = &g.folders[fi];
    f->count  = 0;

    const char *p = json;
    while ((p = strstr(p, "\"type\"")) != NULL
           && f->count < MAX_SCRIPTS) {

        char type_val[32] = {0};
        const char *after = json_str(p, "type", type_val, sizeof(type_val));
        if (!after) { p++; continue; }

        if (strcmp(type_val, "file") == 0) {
            const char *obj = p;
            while (obj > json && *obj != '{') obj--;

            char name_a[MAX_NAME]     = {0};
            char path_a[MAX_APPPATH]  = {0};
            char sha_a[MAX_SHA]       = {0};
            json_str(obj, "name", name_a,  sizeof(name_a));
            json_str(obj, "path", path_a,  sizeof(path_a));
            json_str(obj, "sha",  sha_a,   sizeof(sha_a));

            /* .py files only */
            size_t nl = strlen(name_a);
            if (nl < 4 || strcmp(name_a + nl - 3, ".py") != 0) {
                p = after; continue;
            }

            Script *s = &f->scripts[f->count++];
            memset(s, 0, sizeof(*s));

            MultiByteToWideChar(CP_UTF8, 0, name_a, -1, s->name,    MAX_NAME);
            MultiByteToWideChar(CP_UTF8, 0, path_a, -1, s->gh_path, MAX_APPPATH);
            MultiByteToWideChar(CP_UTF8, 0, sha_a,  -1, s->sha,     MAX_SHA);

            /* Build local cached path */
            _snwprintf(s->local, MAX_APPPATH - 1, L"%s\\%s\\%s",
                       g.cfg.cache_dir, g.folders[fi].name, s->name);
            /* .name still has .py at this point; strip for display */
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            /* local path needs the .py extension back */
            wcscat(s->local, L".py");
        }
        p = after;
    }
    f->loaded = true;
}
