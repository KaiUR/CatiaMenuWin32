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
/*  GitHub_HostAllowed  (static)                                       */
/*  Purpose: Reports whether a host name belongs to GitHub: exactly    */
/*           github.com, or a subdomain of github.com or              */
/*           githubusercontent.com (raw files, release downloads).    */
/*           Suffixes are matched on a dot boundary, so a look-alike   */
/*           such as evilgithub.com is rejected.                       */
/*  In:  host — host name to test                                      */
/*  Out: true if the host is a GitHub host                             */
/* ================================================================== */
static bool GitHub_HostAllowed(const WCHAR *host)
{
    static const WCHAR *suffixes[] = {L".github.com", L".githubusercontent.com", NULL};
    if (_wcsicmp(host, L"github.com") == 0) return true;
    size_t hl = wcslen(host);
    for (int i = 0; suffixes[i]; i++)
    {
        size_t sl = wcslen(suffixes[i]);
        if (hl > sl && _wcsicmp(host + hl - sl, suffixes[i]) == 0) return true;
    }
    return false;
}

/* ================================================================== */
/*  GitHub_VerifyCert  (static)                                        */
/*  Purpose: Validates the TLS server certificate after HttpSendRequest */
/*           by checking three conditions against the server actually  */
/*           answered (after any redirect):                            */
/*           1. Its host is a GitHub host (GitHub_HostAllowed)         */
/*           2. The certificate chain passes the Windows SSL policy    */
/*              for that exact host name (SAN / wildcard rules)        */
/*           3. The issuing CA's organisation exactly matches a CA     */
/*              GitHub uses                                            */
/*           Blocks the request if any check fails.                    */
/*  In:  hReq          — open WinINet request handle after send        */
/*       expected_host — wide string hostname (e.g. L"api.github.com") */
/*  Out: true if the certificate passes all checks; false to abort     */
/* ================================================================== */
static bool GitHub_VerifyCert(HINTERNET hReq, const WCHAR *expected_host)
{
    /* Organisation (O=) of the CAs GitHub issues its certificates from.
       Exact matches — the O= field is stable across the CAs' intermediate
       rotations, unlike the intermediate names themselves. */
    static const WCHAR *trusted_orgs[] = {
        L"Sectigo Limited", /* github.com, api.github.com            */
        L"Let's Encrypt", /* *.githubusercontent.com               */
        L"DigiCert Inc", /* used by GitHub before Sectigo          */
        L"GlobalSign nv-sa", /* used by GitHub's CDN previously        */
        NULL};

    /* ── Check 1: the answering host (after redirects) is GitHub's ─── */
    WCHAR url[2048] = {0};
    DWORD url_size = sizeof(url);
    if (!InternetQueryOption(hReq, INTERNET_OPTION_URL, url, &url_size))
        return false;
    WCHAR host[256] = {0};
    URL_COMPONENTSW uc = {.dwStructSize = sizeof(uc),
                          .lpszHostName = host,
                          .dwHostNameLength = (DWORD)_countof(host)};
    if (!InternetCrackUrlW(url, 0, 0, &uc) || !GitHub_HostAllowed(host))
    {
        Util_Log(L"CertCheck: %s redirected to non-GitHub host %s", expected_host, host);
        return false;
    }

    /* ── Check 2: chain valid for exactly this host ───────────────── */
    PCCERT_CHAIN_CONTEXT chain = NULL;
    DWORD chain_size = sizeof(chain);
    if (!InternetQueryOption(hReq, INTERNET_OPTION_SERVER_CERT_CHAIN_CONTEXT,
                             (LPVOID)&chain, &chain_size) ||
        !chain)
        return false;

    bool ok = false;
    HTTPSPolicyCallbackData https = {.cbStruct = sizeof(https),
                                     .dwAuthType = AUTHTYPE_SERVER,
                                     .pwszServerName = host};
    CERT_CHAIN_POLICY_PARA para = {.cbSize = sizeof(para),
                                   .pvExtraPolicyPara = &https};
    CERT_CHAIN_POLICY_STATUS status = {.cbSize = sizeof(status)};
    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &para, &status) ||
        status.dwError != 0)
    {
        Util_Log(L"CertCheck: SSL policy failed for %s (0x%08lx)", host, status.dwError);
        goto done;
    }

    /* ── Check 3: issuing CA is one GitHub uses ───────────────────── */
    if (chain->cChain < 1 || chain->rgpChain[0]->cElement < 1) goto done;
    PCCERT_CONTEXT leaf = chain->rgpChain[0]->rgpElement[0]->pCertContext;
    WCHAR org[128] = {0};
    CertGetNameStringW(leaf, CERT_NAME_ATTR_TYPE, CERT_NAME_ISSUER_FLAG,
                       (void *)szOID_ORGANIZATION_NAME, org, (DWORD)_countof(org));
    for (int i = 0; trusted_orgs[i]; i++)
    {
        if (wcscmp(org, trusted_orgs[i]) == 0)
        {
            ok = true;
            break;
        }
    }
    if (!ok)
        Util_Log(L"CertCheck: UNKNOWN CA \"%s\" for %s - BLOCKING", org, host);

done:
    CertFreeCertificateChain(chain);
    return ok;
}

/* ================================================================== */
/*  GitHub_HttpGetEx                                                    */
/*  Purpose: Performs a secure HTTPS GET request via WinINet, adds the */
/*           JSON Accept header (API calls only), optional token and   */
/*           optional If-None-Match header, validates the TLS          */
/*           certificate, and reads the whole response body into a    */
/*           heap buffer that grows as needed (up to HTTP_MAX_BODY).  */
/*  In:  host          — server hostname (e.g. L"api.github.com")      */
/*       path          — request path                                  */
/*       token         — OAuth token string, or NULL/empty for none    */
/*       if_none_match — ETag from an earlier 200, or NULL/empty       */
/*       r             — receives status, ETag and body (see Out)      */
/*  Out: true on HTTP 200 (r->body holds the null-terminated body) or  */
/*       HTTP 304 (r->body NULL — the cached copy is still current);   */
/*       false on any other status or error.  The caller frees         */
/*       r->body with free().                                          */
/* ================================================================== */
bool GitHub_HttpGetEx(const WCHAR *host, const WCHAR *path, const WCHAR *token,
                      const char *if_none_match, HttpResponse *r)
{
    ZeroMemory(r, sizeof(*r));

    HINTERNET hInet = InternetOpen(
        L"CatiaMenuWin32/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) return false;

    /* 15 000 ms = 15 s per operation — prevents sync from hanging on a slow or unresponsive server */
    DWORD timeout_ms = 15000;
    InternetSetOption(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOption(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOption(hInet, INTERNET_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    HINTERNET hConn = InternetConnect(
        hInet, host, INTERNET_DEFAULT_HTTPS_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn)
    {
        InternetCloseHandle(hInet);
        return false;
    }

    DWORD flags = INTERNET_FLAG_SECURE /* require HTTPS — plaintext is rejected */
                  | INTERNET_FLAG_RELOAD /* bypass WinINet cache, always fetch fresh */
                  | INTERNET_FLAG_NO_CACHE_WRITE; /* don't write the response into the WinINet cache */

    HINTERNET hReq = HttpOpenRequest(
        hConn, L"GET", path, NULL, NULL, NULL, flags, 0);
    if (!hReq)
    {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    /* GitHub REST API requires the v3 JSON accept header; raw file downloads must NOT send it */
    if (wcsstr(host, L"api.github.com"))
    {
        HttpAddRequestHeaders(hReq,
                              L"Accept: application/vnd.github.v3+json\r\n",
                              (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD); /* -1 = let WinINet measure the string length */
    }

    if (token && token[0])
    {
        WCHAR auth[300];
        _snwprintf_s(auth, 299, _TRUNCATE, L"Authorization: token %s\r\n", token);
        HttpAddRequestHeaders(hReq, auth, (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);
    }

    /* Conditional request: GitHub answers 304 with no body when the resource is unchanged */
    if (if_none_match && if_none_match[0])
    {
        WCHAR inm[200];
        _snwprintf_s(inm, _countof(inm), _TRUNCATE, L"If-None-Match: %S\r\n", if_none_match);
        HttpAddRequestHeaders(hReq, inm, (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);
    }

    if (!HttpSendRequest(hReq, NULL, 0, NULL, 0))
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    /* ── Certificate validation ──────────────────────────────────── */
    if (!GitHub_VerifyCert(hReq, host))
    {
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
    r->status = status;
    if (status != 200)
    {
        /* 304 = unchanged since the ETag we sent; 404 = not found, 401/403 = auth
           failure, 429 = rate-limited, etc. */
        if (status != 304)
            Util_Log(L"GitHub_HttpGetEx: HTTP %d for %s%s", status, host, path);
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return status == 304;
    }

    /* ETag for the next conditional request (absent on some responses) */
    DWORD esz = sizeof(r->etag) - 1;
    if (!HttpQueryInfoA(hReq, HTTP_QUERY_ETAG, r->etag, &esz, NULL))
        r->etag[0] = '\0';

    /* Read the whole body, growing the buffer as needed */
    DWORD cap = 64 * 1024, total = 0, read = 0; /* 64 KB start: most API responses fit */
    char *body = (char *)malloc(cap);
    bool ok = (body != NULL);
    while (ok)
    {
        if (cap - total < 4096) /* keep room for a full read chunk plus the terminator */
        {
            if (cap >= HTTP_MAX_BODY)
            {
                Util_Log(L"GitHub_HttpGetEx: body over %u bytes for %s%s", HTTP_MAX_BODY, host, path);
                ok = false;
                break;
            }
            char *nb = (char *)realloc(body, (size_t)cap * 2);
            if (!nb)
            {
                ok = false;
                break;
            }
            body = nb;
            cap *= 2;
        }
        if (!InternetReadFile(hReq, body + total, cap - total - 1, &read))
        {
            ok = false; /* connection dropped mid-body — never hand back a partial response */
            break;
        }
        if (read == 0) break; /* end of body */
        total += read;
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);

    if (!ok || total == 0) /* an empty 200 body is treated as a failure */
    {
        free(body);
        return false;
    }
    body[total] = '\0'; /* null-terminate so callers can use string functions on the response */
    r->body = body;
    r->len = total;
    return true;
}

/* ================================================================== */
/*  GitHub_HttpGet                                                      */
/*  Purpose: Fixed-buffer convenience wrapper around GitHub_HttpGetEx  */
/*           for callers with a caller-allocated buffer.  Only HTTP    */
/*           200 counts as success.                                     */
/*  In:  host  — server hostname (e.g. L"api.github.com")              */
/*       path  — request path                                           */
/*       token — OAuth token string, or NULL/empty to skip auth header */
/*       buf   — caller-allocated buffer for the response body         */
/*       len   — in: capacity of buf (0 = HTTP_BUF_SIZE); out: bytes   */
/*  Out: true if HTTP 200 and the body fits in buf; false otherwise    */
/* ================================================================== */
bool GitHub_HttpGet(const WCHAR *host, const WCHAR *path,
                    const WCHAR *token, char *buf, DWORD *len)
{
    DWORD cap = (*len > 0) ? *len : HTTP_BUF_SIZE;
    *len = 0;

    HttpResponse r;
    if (!GitHub_HttpGetEx(host, path, token, NULL, &r) || !r.body) return false;
    bool fits = (r.len < cap); /* a body that does not fit would be truncated — fail instead */
    if (fits)
    {
        memcpy_s(buf, cap, r.body, r.len + 1); /* +1 copies the terminator */
        *len = r.len;
    }
    else
        Util_Log(L"GitHub_HttpGet: %u-byte body exceeds %u-byte buffer for %s%s", r.len, cap, host, path);
    free(r.body);
    return fits;
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
    char *file_buf = (char *)malloc(file_size + 1);
    if (!file_buf)
    {
        CloseHandle(hf);
        return false;
    }

    DWORD read_bytes = 0;
    ReadFile(hf, file_buf, file_size, &read_bytes, NULL);
    CloseHandle(hf);
    if (read_bytes != file_size)
    {
        free(file_buf);
        return false;
    }

    /* Build the git blob header: "blob <size>\0" */
    char header[64];
    int header_len = _snprintf_s(header, sizeof(header), _TRUNCATE, "blob %lu", (unsigned long)file_size);
    /* header_len does NOT include the NUL - but the NUL IS part of the hash input */

    /* Hash = SHA1(header + NUL + file_content) */
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool ok = false;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT))
    { /* CRYPT_VERIFYCONTEXT = key-less context, sufficient for hashing */
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
        { /* CALG_SHA1 = SHA-1 algorithm ID */
            /* Feed the three-part input to match GitHub's blob SHA formula */
            CryptHashData(hHash, (BYTE *)header, header_len, 0); /* "blob <size>" (no NUL yet) */
            BYTE nul = 0;
            CryptHashData(hHash, &nul, 1, 0); /* the mandatory NUL separator */
            CryptHashData(hHash, (BYTE *)file_buf, file_size, 0); /* raw file bytes */

            BYTE hash[20]; /* SHA-1 produces 160 bits = 20 bytes */
            DWORD hash_len = sizeof(hash);
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0))
            {
                WCHAR *p = sha_out;
                for (int i = 0; i < 20 && p < sha_out + sha_max - 2; i++)
                {
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
    if (!s->sha[0]) return true; /* no SHA in the manifest — skip verification (e.g. local dir scripts) */

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
    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (attempt > 0) Sleep(1000 * attempt); /* back off: attempt 1 = 1 s, attempt 2 = 2 s */
        if (GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_url, local_path, token)) return true;
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
    HttpResponse r;
    if (!GitHub_HttpGetEx(host, path, token, NULL, &r) || !r.body) return false; /* no ETag sent, so never 304 */
    bool ok = Util_WriteFile(local_path, r.body, r.len);
    free(r.body);
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
    while (*p == ' ' || *p == ':')
        p++;
    if (*p != '"') return NULL;
    p++;

    int i = 0;
    while (*p && *p != '"' && i < max - 1)
    {
        if (*p == '\\')
        {
            p++;
            if (*p) out[i++] = *p++;
        } /* handle JSON escape: skip '\', copy next char literally */
        else
        {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return (*p == '"') ? p + 1 : NULL; /* return position after closing quote, or NULL if string was unterminated */
}

/* ================================================================== */
/*  GitHub_ParseTree                                                    */
/*  Purpose: Parses a Git Trees API response (?recursive=1) into the   */
/*           list of scripts the app shows: every blob whose path is  */
/*           "<folder>/<file>.py" directly inside a top-level folder, */
/*           skipping setup/ and dot-folders.  Also reports the SHA of */
/*           setup/requirements.txt and whether GitHub truncated the  */
/*           listing.                                                  */
/*  In:  json      — null-terminated UTF-8 response body               */
/*       out       — receives a heap array of TreeEntry (caller frees; */
/*                   NULL when there are no scripts)                   */
/*       req_sha   — receives the setup/requirements.txt blob SHA, or  */
/*                   "" if there is none (MAX_SHA WCHARs)              */
/*       truncated — receives true if GitHub cut the listing short     */
/*  Out: number of entries, or -1 if json is not a tree listing        */
/* ================================================================== */
int GitHub_ParseTree(const char *json, TreeEntry **out, WCHAR *req_sha, bool *truncated)
{
    *out = NULL;
    req_sha[0] = L'\0';
    *truncated = false;

    /* The top-level "tree" key precedes the entry array; entry values of
       "tree" (type of a directory) only appear inside that array */
    const char *p = strstr(json, "\"tree\"");
    if (!p) return -1;
    p = strchr(p, '[');
    if (!p) return -1;

    const char *t = strstr(json, "\"truncated\"");
    if (t)
    {
        t += 11; /* strlen("\"truncated\"") */
        while (*t == ' ' || *t == ':')
            t++;
        *truncated = (strncmp(t, "true", 4) == 0);
    }

    TreeEntry *arr = NULL;
    int count = 0, cap = 0;
    while ((p = strchr(p, '{')) != NULL)
    {
        /* Entry objects are flat, so the next '}' closes this one */
        const char *end = strchr(p, '}');
        if (!end) break;

        char path_a[MAX_APPPATH] = {0}, type_a[16] = {0}, sha_a[MAX_SHA] = {0};
        const char *a = json_str(p, "path", path_a, sizeof(path_a));
        const char *b = json_str(p, "type", type_a, sizeof(type_a));
        const char *c = json_str(p, "sha", sha_a, sizeof(sha_a));
        p = end + 1;
        if (!a || a > end || !b || b > end || !c || c > end) continue; /* key missing from this object */
        if (strcmp(type_a, "blob") != 0) continue; /* directories and submodules */

        if (_stricmp(path_a, "setup/requirements.txt") == 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, sha_a, -1, req_sha, MAX_SHA);
            continue;
        }

        /* Only files directly inside a top-level folder */
        char *slash = strchr(path_a, '/');
        if (!slash || strchr(slash + 1, '/')) continue;
        *slash = '\0';
        const char *folder = path_a, *file = slash + 1;
        size_t fl = strlen(file);
        if (fl < 4 || strcmp(file + fl - 3, ".py") != 0) continue; /* fl < 4 rejects anything shorter than "x.py" */
        if (folder[0] == '.' || _stricmp(folder, "setup") == 0) continue; /* hidden folders; setup/ holds build files */

        if (count == cap)
        {
            int ncap = cap ? cap * 2 : 128; /* 128: room for a typical repository in one allocation */
            TreeEntry *na = (TreeEntry *)realloc(arr, (size_t)ncap * sizeof(TreeEntry));
            if (!na) break; /* OOM — return what was parsed so far */
            arr = na;
            cap = ncap;
        }
        TreeEntry *e = &arr[count++];
        ZeroMemory(e, sizeof(*e));
        MultiByteToWideChar(CP_UTF8, 0, folder, -1, e->folder, MAX_NAME);
        MultiByteToWideChar(CP_UTF8, 0, file, -1, e->file, MAX_NAME);
        MultiByteToWideChar(CP_UTF8, 0, sha_a, -1, e->sha, MAX_SHA);
    }
    *out = arr;
    return count;
}

/* ================================================================== */
/*  Util_WriteFile                                                      */
/*  Purpose: Writes a buffer to a file, replacing any existing file,   */
/*           and creates the parent directory if needed.               */
/*  In:  path — destination file                                        */
/*       data — bytes to write                                          */
/*       len  — number of bytes                                         */
/*  Out: true if every byte was written                                 */
/* ================================================================== */
bool Util_WriteFile(const WCHAR *path, const void *data, DWORD len)
{
    WCHAR dir[MAX_APPPATH];
    wcsncpy_s(dir, MAX_APPPATH, path, _TRUNCATE);
    PathRemoveFileSpec(dir);
    SHCreateDirectoryEx(NULL, dir, NULL);

    HANDLE hf = CreateFile(path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hf, data, len, &written, NULL);
    CloseHandle(hf);
    return ok && written == len;
}

/* ================================================================== */
/*  Util_ReadFile                                                       */
/*  Purpose: Reads a whole file into a null-terminated heap buffer.    */
/*  In:  path — file to read                                            */
/*       len  — receives the number of bytes read                       */
/*  Out: heap buffer (caller frees), or NULL if the file is missing,   */
/*       unreadable, or larger than HTTP_MAX_BODY                      */
/* ================================================================== */
char *Util_ReadFile(const WCHAR *path, DWORD *len)
{
    *len = 0;
    HANDLE hf = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;

    DWORD size = GetFileSize(hf, NULL);
    char *buf = (size == INVALID_FILE_SIZE || size >= HTTP_MAX_BODY) ? NULL : (char *)malloc((size_t)size + 1);
    DWORD read = 0;
    if (buf && (!ReadFile(hf, buf, size, &read, NULL) || read != size))
    {
        free(buf);
        buf = NULL;
    }
    CloseHandle(hf);
    if (!buf) return NULL;
    buf[size] = '\0';
    *len = size;
    return buf;
}
