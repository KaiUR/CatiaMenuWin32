/*
 * github.c  -  WinINet HTTPS GET + JSON parsing for GitHub API v3.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <wincrypt.h>

/* ================================================================== */
/*  GitHub_VerifyCert  (static)                                        */
/*  Purpose: Validates the TLS server certificate after HttpSendRequest */
/*           by checking two conditions:                               */
/*           1. Subject CN contains the expected host or a GitHub alias*/
/*           2. Issuer is a known GitHub CA (DigiCert, Sectigo, etc.)  */
/*           Blocks the request if either check fails.                 */
/*  In:  hReq          — open WinINet request handle after send        */
/*       expected_host — wide string hostname (e.g. L"api.github.com") */
/*  Out: true if certificate passes both checks; false to abort        */
/* ================================================================== */
static bool GitHub_VerifyCert(HINTERNET hReq, const WCHAR *expected_host)
{
    /* INTERNET_CERTIFICATE_INFO string fields are always ANSI even in
       Unicode builds - WinINet fills them with narrow strings regardless
       of the UNICODE define. Cast to char* for correct string ops. */
    INTERNET_CERTIFICATE_INFO ci;
    DWORD ci_size = sizeof(ci);
    if (!InternetQueryOption(hReq, INTERNET_OPTION_SECURITY_CERTIFICATE_STRUCT,
                             &ci, &ci_size))
        return false;

    /* Convert expected_host to narrow for comparison */
    char host_a[256] = {0};
    WideCharToMultiByte(CP_ACP, 0, expected_host, -1,
                        host_a, sizeof(host_a) - 1, NULL, NULL);

    bool ok = false;

    /* ── Check 1: subject must contain the expected host ─────────── */
    if (ci.lpszSubjectInfo) {
        const char *subj = (const char *)ci.lpszSubjectInfo;
        if (strstr(subj, host_a)              ||
            strstr(subj, "github.com")        ||
            strstr(subj, "github.io")         ||
            strstr(subj, "githubusercontent.com")) {
            ok = true;
        }
    }

    if (!ok) {
        Util_Log(L"CertCheck: subject mismatch for %s", expected_host);
        LocalFree(ci.lpszSubjectInfo);
        LocalFree(ci.lpszIssuerInfo);
        LocalFree(ci.lpszProtocolName);
        LocalFree(ci.lpszSignatureAlgName);
        LocalFree(ci.lpszEncryptionAlgName);
        return false;
    }

    /* ── Check 2: issuer must be a known GitHub CA ────────────────── */
    ok = false;
    if (ci.lpszIssuerInfo) {
        const char *issr = (const char *)ci.lpszIssuerInfo;
        if (strstr(issr, "DigiCert")   ||
            strstr(issr, "Sectigo")     ||
            strstr(issr, "GlobalSign")  ||
            strstr(issr, "Encrypt")) {
            ok = true;
        }
        if (!ok)
            Util_Log(L"CertCheck: UNKNOWN CA - BLOCKING");
    }

    LocalFree(ci.lpszSubjectInfo);
    LocalFree(ci.lpszIssuerInfo);
    LocalFree(ci.lpszProtocolName);
    LocalFree(ci.lpszSignatureAlgName);
    LocalFree(ci.lpszEncryptionAlgName);

    return ok;
}

/* ================================================================== */
/*  GitHub_HttpGet                                                      */
/*  Purpose: Performs a secure HTTPS GET request via WinINet, adds the */
/*           JSON Accept header (API calls only) and optional Bearer   */
/*           token, validates the TLS certificate, checks the HTTP     */
/*           status code, and reads the response body into buf.        */
/*  In:  host  — server hostname (e.g. L"api.github.com")              */
/*       path  — request path (e.g. L"/repos/owner/repo/contents/dir") */
/*       token — OAuth token string, or NULL/empty to skip auth header */
/*       buf   — caller-allocated buffer for the response body         */
/*       len   — in: capacity of buf; out: bytes received              */
/*  Out: true if HTTP 200 and at least one byte received; false on err  */
/* ================================================================== */
bool GitHub_HttpGet(const WCHAR *host, const WCHAR *path,
                    const WCHAR *token, char *buf, DWORD *len)
{
    *len = 0;

    HINTERNET hInet = InternetOpen(
        L"CatiaMenuWin32/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) return false;

    /* 15 000 ms = 15 s per operation — prevents sync from hanging on a slow or unresponsive server */
    DWORD timeout_ms = 15000;
    InternetSetOption(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOption(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOption(hInet, INTERNET_OPTION_SEND_TIMEOUT,    &timeout_ms, sizeof(timeout_ms));

    HINTERNET hConn = InternetConnect(
        hInet, host, INTERNET_DEFAULT_HTTPS_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return false; }

    DWORD flags = INTERNET_FLAG_SECURE          /* require HTTPS — plaintext is rejected */
                | INTERNET_FLAG_RELOAD          /* bypass WinINet cache, always fetch fresh */
                | INTERNET_FLAG_NO_CACHE_WRITE; /* don't write the response into the WinINet cache */

    HINTERNET hReq = HttpOpenRequest(
        hConn, L"GET", path, NULL, NULL, NULL, flags, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    /* GitHub REST API requires the v3 JSON accept header; raw file downloads must NOT send it */
    if (wcsstr(host, L"api.github.com")) {
        HttpAddRequestHeaders(hReq,
            L"Accept: application/vnd.github.v3+json\r\n",
            (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD); /* -1 = let WinINet measure the string length */
    }

    if (token && token[0]) {
        WCHAR auth[300];
        _snwprintf_s(auth, 299, _TRUNCATE, L"Authorization: token %s\r\n", token);
        HttpAddRequestHeaders(hReq, auth, (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);
    }

    if (!HttpSendRequest(hReq, NULL, 0, NULL, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    /* ── Certificate validation ──────────────────────────────────── */
    if (!GitHub_VerifyCert(hReq, host)) {
        Util_Log(L"CertCheck: FAILED for %s - aborting", host);
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false; /* refuse to read response from an unverified server */
    }

    /* ── HTTP status check ───────────────────────────────────────── */
    DWORD status = 0, ssz = sizeof(DWORD);
    HttpQueryInfo(hReq,
                  HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                  &status, &ssz, NULL);
    if (status != 200) {
        /* 404 = not found, 401/403 = auth failure, 429 = rate-limited, etc. */
        Util_Log(L"GitHub_HttpGet: HTTP %d for %s%s", status, host, path);
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    DWORD total = 0, read = 0;
    DWORD max_read = (*len > 0) ? *len : HTTP_BUF_SIZE; /* use caller-supplied capacity when provided */
    while (InternetReadFile(hReq, buf + total,
                            max_read - total - 1, &read) && read) { /* -1 reserves space for the null terminator */
        total += read;
        if (total >= max_read - 1) break; /* buffer full — stop reading */
    }
    buf[total] = '\0'; /* null-terminate so callers can use string functions on the response */
    *len = total;

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return total > 0; /* false if the response body was empty even with HTTP 200 */
}

/* ================================================================== */
/*  GitHub_ComputeFileSHA1                                              */
/*  Purpose: Computes the GitHub blob SHA1 for a local file using the  */
/*           same algorithm GitHub uses: SHA1("blob <size>\0<content>")*/
/*           and encodes the result as a 40-character lowercase hex    */
/*           string.  Uses the Win32 CryptoAPI (CALG_SHA1).           */
/*  In:  local_path — full path to the local file                      */
/*       sha_out    — buffer to receive the 40-char hex SHA1 string   */
/*       sha_max    — capacity of sha_out in WCHARs (must be >= 41)   */
/*  Out: true and sha_out filled if successful; false on error          */
/* ================================================================== */
bool GitHub_ComputeFileSHA1(const WCHAR *local_path,
                             WCHAR *sha_out, int sha_max)
{
    sha_out[0] = L'\0';

    HANDLE hf = CreateFile(local_path, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;

    DWORD file_size = GetFileSize(hf, NULL);
    char *file_buf  = (char *)malloc(file_size + 1);
    if (!file_buf) { CloseHandle(hf); return false; }

    DWORD read_bytes = 0;
    ReadFile(hf, file_buf, file_size, &read_bytes, NULL);
    CloseHandle(hf);
    if (read_bytes != file_size) { free(file_buf); return false; }

    /* Build the git blob header: "blob <size>\0" */
    char header[64];
    int  header_len = _snprintf_s(header, sizeof(header), _TRUNCATE, "blob %lu", (unsigned long)file_size);
    /* header_len does NOT include the NUL - but the NUL IS part of the hash input */

    /* Hash = SHA1(header + NUL + file_content) */
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool ok = false;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT)) { /* CRYPT_VERIFYCONTEXT = key-less context, sufficient for hashing */
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) { /* CALG_SHA1 = SHA-1 algorithm ID */
            /* Feed the three-part input to match GitHub's blob SHA formula */
            CryptHashData(hHash, (BYTE *)header, header_len, 0); /* "blob <size>" (no NUL yet) */
            BYTE nul = 0;
            CryptHashData(hHash, &nul, 1, 0);                    /* the mandatory NUL separator */
            CryptHashData(hHash, (BYTE *)file_buf, file_size, 0);/* raw file bytes */

            BYTE  hash[20]; /* SHA-1 produces 160 bits = 20 bytes */
            DWORD hash_len = sizeof(hash);
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
                WCHAR *p = sha_out;
                for (int i = 0; i < 20 && p < sha_out + sha_max - 2; i++) {
                    _snwprintf_s(p, 3, _TRUNCATE, L"%02x", hash[i]); /* 3 = 2 hex chars + null; advance p by 2 */
                    p += 2;
                }
                *p = L'\0';
                ok = true;
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    free(file_buf);
    return ok;
}

/* ================================================================== */
/*  GitHub_VerifyScriptSHA                                              */
/*  Purpose: Verifies the integrity of a locally cached script by      */
/*           computing its blob SHA1 and comparing it to the expected  */
/*           SHA stored in s->sha (from the GitHub API).  If s->sha   */
/*           is empty the check is skipped and true is returned.       */
/*  In:  s — Script whose s->local file and s->sha field are checked   */
/*  Out: true if SHA matches or s->sha is empty; false on mismatch     */
/* ================================================================== */
bool GitHub_VerifyScriptSHA(const Script *s)
{
    if (!s->sha[0]) return true;   /* no SHA in the manifest — skip verification (e.g. local dir scripts) */

    WCHAR computed[MAX_SHA] = {0};
    if (!GitHub_ComputeFileSHA1(s->local, computed, MAX_SHA))
        return false;

    bool match = (wcscmp(computed, s->sha) == 0);
    if (!match)
        Util_Log(L"SHA mismatch for %s: expected %s got %s",
                 s->name, s->sha, computed);
    return match;
}

/* ================================================================== */
/*  GitHub_DownloadRaw                                                  */
/*  Purpose: Downloads a single script from the main GitHub repository  */
/*           (GITHUB_OWNER/GITHUB_REPO/GITHUB_BRANCH) to a local path. */
/*           Retries up to 3 times with increasing delays on failure.  */
/*           Creates the target directory if it does not exist.        */
/*  In:  gh_path    — GitHub-relative path (e.g. "FolderA/script.py") */
/*       local_path — full local destination path                       */
/*       token      — OAuth token, or NULL/empty for unauthenticated    */
/*  Out: true if the file was written successfully; false on all errors */
/* ================================================================== */
bool GitHub_DownloadRaw(const WCHAR *gh_path, const WCHAR *local_path,
                        const WCHAR *token)
{
    WCHAR raw_url[MAX_APPPATH * 2];
    _snwprintf_s(raw_url, MAX_APPPATH * 2 - 1, _TRUNCATE, L"/%s/%s/%s/%s",
               GITHUB_OWNER, GITHUB_REPO, GITHUB_BRANCH, gh_path);

    /* Retry up to 3 times with increasing delays: 0 ms, 1000 ms, 2000 ms */
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) Sleep(1000 * attempt); /* back off: attempt 1 = 1 s, attempt 2 = 2 s */

        char *buf = (char *)malloc(HTTP_BUF_SIZE);
        if (!buf) return false;

        DWORD len = 0;
        bool  ok  = GitHub_HttpGet(GITHUB_RAW_HOST, raw_url, token, buf, &len);
        if (ok && len > 0) {
            WCHAR dir[MAX_APPPATH];
            wcsncpy_s(dir, MAX_APPPATH, local_path, _TRUNCATE);
            PathRemoveFileSpec(dir);
            SHCreateDirectoryEx(NULL, dir, NULL);

            HANDLE hf = CreateFile(local_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hf, buf, len, &written, NULL);
                CloseHandle(hf);
                free(buf);
                if (written == len) return true; /* all bytes written — success */
                /* partial write: fall through to retry */
            } else {
                free(buf); /* couldn't create the file — retry */
            }
        } else {
            free(buf); /* HTTP GET failed — retry */
        }
    }
    return false; /* all 3 attempts exhausted */
}

/* ================================================================== */
/*  GitHub_ParseOwnerRepo                                               */
/*  Purpose: Extracts the owner and repository name from a GitHub URL  */
/*           of the form https://github.com/owner/repo[/...].          */
/*  In:  url   — full GitHub repository URL                            */
/*       owner — buffer to receive the owner name (MAX_NAME chars)     */
/*       repo  — buffer to receive the repository name (MAX_NAME chars)*/
/*  Out: true and owner/repo filled if parsing succeeds; false on err   */
/* ================================================================== */
bool GitHub_ParseOwnerRepo(const WCHAR *url, WCHAR *owner, WCHAR *repo)
{
    owner[0] = repo[0] = L'\0';
    /* Find "github.com/" */
    const WCHAR *p = wcsstr(url, L"github.com/");
    if (!p) return false;
    p += wcslen(L"github.com/");
    /* owner is up to next '/' */
    const WCHAR *slash = wcschr(p, L'/');
    if (!slash) return false;
    int olen = (int)(slash - p);
    if (olen <= 0 || olen >= MAX_NAME) return false;
    wcsncpy_s(owner, MAX_NAME, p, olen);
    /* repo is after slash, up to next '/' or end */
    p = slash + 1;
    const WCHAR *end = wcschr(p, L'/');
    int rlen = end ? (int)(end - p) : (int)wcslen(p);
    if (rlen <= 0 || rlen >= MAX_NAME) return false;
    wcsncpy_s(repo, MAX_NAME, p, rlen);
    return true;
}

/* ================================================================== */
/*  GitHub_DownloadRawFull                                              */
/*  Purpose: Downloads a raw file from an arbitrary host and path to a  */
/*           local path.  Used for extra GitHub repository scripts and  */
/*           requirements files where the host differs from the main   */
/*           repo.  No retry logic; caller retries if needed.          */
/*  In:  host       — server hostname (e.g. L"raw.githubusercontent.com")*/
/*       path       — full request path including owner/repo/branch    */
/*       local_path — full local destination path                       */
/*       token      — OAuth token, or NULL/empty for unauthenticated    */
/*  Out: true if the file was written successfully; false otherwise     */
/* ================================================================== */
bool GitHub_DownloadRawFull(const WCHAR *host, const WCHAR *path,
                             const WCHAR *local_path, const WCHAR *token)
{
    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) return false;

    DWORD len = 0;
    bool  ok  = GitHub_HttpGet(host, path, token, buf, &len);
    if (ok && len > 0) {
        WCHAR dir[MAX_APPPATH];
        wcsncpy_s(dir, MAX_APPPATH, local_path, _TRUNCATE);
        PathRemoveFileSpec(dir);
        SHCreateDirectoryEx(NULL, dir, NULL);

        HANDLE hf = CreateFile(local_path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hf, buf, len, &written, NULL);
            CloseHandle(hf);
            ok = (written == len);
        } else ok = false;
    }
    free(buf);
    return ok;
}

/* ================================================================== */
/*  json_str  (static)                                                 */
/*  Purpose: Minimal single-value JSON string extractor.  Finds the    */
/*           first occurrence of `"key"` after position p, locates    */
/*           the colon-separated value, and copies the unescaped       */
/*           string into out (up to max-1 chars).                      */
/*  In:  p   — search start position in the JSON buffer               */
/*       key — JSON key name to look for (ASCII)                       */
/*       out — buffer to receive the extracted string value            */
/*       max — capacity of out in bytes                                */
/*  Out: pointer past the closing quote of the value on success;       */
/*       NULL if key not found or value is not a string                */
/* ================================================================== */
static const char *json_str(const char *p, const char *key,
                             char *out, int max)
{
    char needle[MAX_NAME];
    _snprintf_s(needle, sizeof(needle), _TRUNCATE, "\"%s\"", key);

    p = strstr(p, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return NULL;
    p++;

    int i = 0;
    while (*p && *p != '"' && i < max - 1) {
        if (*p == '\\') { p++; if (*p) out[i++] = *p++; } /* handle JSON escape: skip '\', copy next char literally */
        else            { out[i++] = *p++; }
    }
    out[i] = '\0';
    return (*p == '"') ? p + 1 : NULL; /* return position after closing quote, or NULL if string was unterminated */
}

/* ================================================================== */
/*  GitHub_ParseRoot                                                    */
/*  Purpose: Parses the GitHub Contents API JSON response for the root */
/*           of the repository and populates g.folders[] with one      */
/*           ScriptFolder per top-level directory (excluding setup/    */
/*           and dot-folders).  Frees any previous folder allocations. */
/*  In:  json — null-terminated UTF-8 JSON buffer from GitHub_HttpGet  */
/*  Out: (void — populates g.folders[] and g.folder_count)             */
/* ================================================================== */
void GitHub_ParseRoot(const char *json)
{
    /* Hold cs_folders for the entire function — we modify g.folder_count and
       g.folders[] struct fields, and Folder_Free is called on existing entries. */
    EnterCriticalSection(&g.cs_folders);

    for (int _fi = 0; _fi < g.folder_count; _fi++) Folder_Free(&g.folders[_fi]);
    g.folder_count = 0;

    const char *p = json;
    while ((p = strstr(p, "\"type\"")) != NULL
           && g.folder_count < MAX_FOLDERS) {

        char type_val[32] = {0};
        const char *after = json_str(p, "type", type_val, sizeof(type_val));
        if (!after) { p++; continue; }

        if (strcmp(type_val, "dir") == 0) {
            /* Walk back to the opening '{' of this JSON object to read other fields */
            const char *obj = p;
            while (obj > json && *obj != '{') obj--;

            char name_a[MAX_NAME] = {0};
            json_str(obj, "name", name_a, sizeof(name_a));

            /* Skip the setup folder (build files, not scripts) and hidden dot-folders */
            if (_stricmp(name_a, "setup") == 0 || name_a[0] == '.') { p = after; continue; }

            ScriptFolder *f = &g.folders[g.folder_count++];
            ZeroMemory(f, sizeof(*f));

            MultiByteToWideChar(CP_UTF8, 0, name_a, -1, f->name, MAX_NAME);
            wcsncpy_s(f->display, MAX_NAME, f->name, _TRUNCATE);
            Util_SnakeToTitle(f->display); /* convert "Part_Document_Scripts" → "Part Document Scripts" */
        }
        p = after;
    }

    LeaveCriticalSection(&g.cs_folders);
}

/* ================================================================== */
/*  GitHub_ParseFolder                                                  */
/*  Purpose: Parses the GitHub Contents API JSON response for a single */
/*           repository folder and populates g.folders[fi].scripts[]  */
/*           with one Script per .py file found.  Strips the .py ext, */
/*           converts snake_case to Title Case, and builds the local  */
/*           cache path for each script.  Frees old scripts first.    */
/*  In:  json — null-terminated UTF-8 JSON from GitHub_HttpGet        */
/*       fi   — index into g.folders[] to populate                    */
/*  Out: (void — populates g.folders[fi].scripts[] and sets loaded)   */
/* ================================================================== */
void GitHub_ParseFolder(const char *json, int fi)
{
    ScriptFolder *f = &g.folders[fi];
    /* Hold cs_folders for the entire parse — Folder_Push may call realloc,
       which moves f->scripts. Releasing before the loop would let a concurrent
       WM_DRAWITEM or tooltip paint dereference the freed pointer. */
    EnterCriticalSection(&g.cs_folders);
    Folder_Free(f);
    Folder_Alloc(f, 64);

    const char *p = json;
    while ((p = strstr(p, "\"type\"")) != NULL) {

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

            size_t nl = strlen(name_a);
            if (nl < 4 || strcmp(name_a + nl - 3, ".py") != 0) {
                /* nl < 4 rejects anything shorter than "x.py"; the strcmp checks the extension */
                p = after; continue;
            }

            Script *s = Folder_Push(f);
            if (!s) break;
            ZeroMemory(s, sizeof(*s));

            MultiByteToWideChar(CP_UTF8, 0, name_a, -1, s->name,    MAX_NAME);
            MultiByteToWideChar(CP_UTF8, 0, path_a, -1, s->gh_path, MAX_APPPATH);
            MultiByteToWideChar(CP_UTF8, 0, sha_a,  -1, s->sha,     MAX_SHA);

            /* Strip .py from display name before building local path */
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);

            /* Build local path from original filename (still has .py) */
            WCHAR fname_w[MAX_NAME] = {0};
            MultiByteToWideChar(CP_UTF8, 0, name_a, -1, fname_w, MAX_NAME);
            _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\%s",
                       g.cfg.cache_dir, g.folders[fi].name, fname_w);
        }
        p = after;
    }
    f->loaded = true;
    LeaveCriticalSection(&g.cs_folders);
}
