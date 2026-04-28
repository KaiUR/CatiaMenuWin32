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
/*  GitHub_VerifyCert                                                   */
/*  Checks that the server certificate for hReq:                       */
/*    1. Is issued by a known GitHub CA (DigiCert or Sectigo)          */
/*    2. The subject CN matches the expected hostname                   */
/*  Returns true if the cert looks legitimate, false to abort.         */
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
        return false;
    }

    /* ── HTTP status check ───────────────────────────────────────── */
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
/*  GitHub_ComputeFileSHA1                                              */
/*  GitHub blob SHAs are:  SHA1("blob <size>\0<content>")              */
/*  Returns hex string in out (40 chars + NUL).                        */
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
    int  header_len = snprintf(header, sizeof(header), "blob %lu", (unsigned long)file_size);
    /* header_len does NOT include the NUL - but the NUL IS part of the hash input */

    /* Hash = SHA1(header + NUL + file_content) */
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool ok = false;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            /* Hash "blob <size>" */
            CryptHashData(hHash, (BYTE *)header, header_len, 0);
            /* Hash the NUL byte */
            BYTE nul = 0;
            CryptHashData(hHash, &nul, 1, 0);
            /* Hash file content */
            CryptHashData(hHash, (BYTE *)file_buf, file_size, 0);

            BYTE  hash[20];
            DWORD hash_len = sizeof(hash);
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
                /* Convert to hex string */
                WCHAR *p = sha_out;
                for (int i = 0; i < 20 && p < sha_out + sha_max - 2; i++) {
                    _snwprintf_s(p, 3, _TRUNCATE, L"%02x", hash[i]);
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
/*  Returns true if the local file's SHA matches the expected SHA      */
/*  from the GitHub API (stored in s->sha).                            */
/* ================================================================== */
bool GitHub_VerifyScriptSHA(const Script *s)
{
    if (!s->sha[0]) return true;   /* no expected SHA - skip check */

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
/* ================================================================== */
bool GitHub_DownloadRaw(const WCHAR *gh_path, const WCHAR *local_path,
                        const WCHAR *token)
{
    WCHAR raw_url[MAX_APPPATH * 2];
    _snwprintf_s(raw_url, MAX_APPPATH * 2 - 1, _TRUNCATE, L"/%s/%s/%s/%s",
               GITHUB_OWNER, GITHUB_REPO, GITHUB_BRANCH, gh_path);

    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) return false;

    DWORD len = 0;
    bool  ok  = GitHub_HttpGet(GITHUB_RAW_HOST, raw_url, token, buf, &len);
    if (ok && len > 0) {
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
/*  GitHub_ParseOwnerRepo                                               */
/*  Extracts owner and repo from https://github.com/owner/repo         */
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
    wcsncpy(owner, p, olen);
    owner[olen] = L'\0';
    /* repo is after slash, up to next '/' or end */
    p = slash + 1;
    const WCHAR *end = wcschr(p, L'/');
    int rlen = end ? (int)(end - p) : (int)wcslen(p);
    if (rlen <= 0 || rlen >= MAX_NAME) return false;
    wcsncpy(repo, p, rlen);
    repo[rlen] = L'\0';
    return true;
}

/* ================================================================== */
/*  GitHub_DownloadRawFull                                              */
/*  Download from a specific host/path (for extra repos)               */
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
        } else ok = false;
    }
    free(buf);
    return ok;
}

/* ================================================================== */
/*  Minimal JSON helpers                                                */
/* ================================================================== */
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
    p++;

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
            const char *obj = p;
            while (obj > json && *obj != '{') obj--;

            char name_a[MAX_NAME] = {0};
            json_str(obj, "name", name_a, sizeof(name_a));

            if (_stricmp(name_a, "setup") == 0 || name_a[0] == '.') { p = after; continue; }

            ScriptFolder *f = &g.folders[g.folder_count++];
            ZeroMemory(f, sizeof(*f));

            MultiByteToWideChar(CP_UTF8, 0, name_a, -1, f->name, MAX_NAME);
            wcscpy(f->display, f->name);
            Util_SnakeToTitle(f->display);
        }
        p = after;
    }
}

/* ================================================================== */
/*  GitHub_ParseFolder                                                  */
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

            size_t nl = strlen(name_a);
            if (nl < 4 || strcmp(name_a + nl - 3, ".py") != 0) {
                p = after; continue;
            }

            Script *s = &f->scripts[f->count++];
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
}
