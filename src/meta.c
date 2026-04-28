/*
 * meta.c  -  Parse script header into ScriptMeta.
 * CatiaMenuWin32  |  Author: Kai-Uwe Rathjen  |  License: MIT
 */

#include "main.h"

#define DESC_MAX 1023   /* must match ScriptMeta.description buffer - 1 */

static void TrimRight(WCHAR *s)
{
    int n = (int)wcslen(s);
    while (n > 0 && (s[n-1]==L'\r'||s[n-1]==L'\n'||
                     s[n-1]==L' ' ||s[n-1]==L'\t'))
        s[--n] = L'\0';
}

static void StripLeading(WCHAR *s)
{
    WCHAR *p = s;
    while (*p==L' '||*p==L'\t'||*p==L'#'||
           *p==L'\''||*p==L'"'||*p==L'-') p++;
    if (p != s) memmove(s, p, (wcslen(p)+1)*sizeof(WCHAR));
}

static const WCHAR *MatchKey(const WCHAR *line, const WCHAR *key)
{
    size_t klen = wcslen(key);
    if (_wcsnicmp(line, key, klen) != 0) return NULL;
    const WCHAR *p = line + klen;
    while (*p==L' '||*p==L'\t') p++;
    if (*p != L':') return NULL;
    p++;
    while (*p==L' '||*p==L'\t') p++;
    return (*p) ? p : NULL;
}

static void AppendDesc(WCHAR *buf, const WCHAR *text)
{
    if (!text || !*text) return;
    int cur = (int)wcslen(buf);
    if (cur >= DESC_MAX) return;
    /* Add a space separator if buffer not empty */
    if (cur > 0 && cur < DESC_MAX) { buf[cur++]=L' '; buf[cur]=L'\0'; }
    wcsncat(buf, text, DESC_MAX - cur);
}

void Meta_Parse(Script *s)
{
    if (s->meta_loaded) return;
    if (!s->local[0]) return;
    if (GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES) return;

    FILE *f = _wfopen(s->local, L"r, ccs=UTF-8");
    if (!f) f = _wfopen(s->local, L"r");
    if (!f) return;

    ScriptMeta m;
    memset(&m, 0, sizeof(m));

    WCHAR raw[1024], line[1024];
    int   lineno    = 0;
    bool  in_header = false;
    bool  in_desc   = false;
    bool  found_any = false;

    while (lineno < 200 && fgetws(raw, 1024, f))
    {
        lineno++;
        TrimRight(raw);
        wcscpy(line, raw);
        StripLeading(line);
        TrimRight(line);

        /* Detect dashed separator line */
        bool is_dashes = false;
        {
            int dash_count = 0, other = 0;
            for (WCHAR *p = raw; *p && *p!=L'\r' && *p!=L'\n'; p++) {
                if (*p==L'-') dash_count++;
                else if (*p!=L' '&&*p!=L'\t'&&*p!=L'\''&&*p!=L'"') other++;
            }
            is_dashes = (dash_count >= 10 && other == 0);
        }

        if (is_dashes) {
            if (!in_header) { in_header = true; }
            else            { in_desc = false; break; } /* second dashes = end */
            continue;
        }

        if (!in_header) continue;

        /* Stop at Python code */
        if (_wcsnicmp(line, L"import ",      7)==0 ||
            _wcsnicmp(line, L"from ",        5)==0 ||
            _wcsnicmp(line, L"def ",         4)==0 ||
            _wcsnicmp(line, L"class ",       6)==0 ||
            _wcsnicmp(line, L"Change",       6)==0) break;

        /* Skip blank lines */
        if (line[0] == L'\0') continue;

        const WCHAR *val = NULL;
        bool in_reqs = false;  /* local flag for this line */

        if ((val = MatchKey(line, L"Script name")) != NULL) {
            in_desc = false;

        } else if ((val = MatchKey(line, L"Version")) != NULL) {
            wcsncpy(m.version, val, 31);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Author")) != NULL) {
            wcsncpy(m.author, val, 63);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Date")) != NULL) {
            wcsncpy(m.date, val, 31);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Purpose")) != NULL) {
            wcsncpy(m.purpose, val, 127);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Code")) != NULL) {
            wcsncpy(m.code, val, 63);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Release")) != NULL) {
            wcsncpy(m.release, val, 31);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Description")) != NULL) {
            wcsncpy(m.description, val, DESC_MAX);
            found_any = true; in_desc = true; in_reqs = false;

        } else if (_wcsnicmp(line, L"requirements", 12) == 0) {
            /* requirements: may have content on same line or next lines */
            val = MatchKey(line, L"requirements");
            if (val) wcsncpy(m.requirements, val, 511);
            found_any = true; in_desc = false; in_reqs = true;
            (void)in_reqs; /* suppress unused warning - used as state below */
            /* Switch from desc mode to reqs mode */
            /* We reuse in_desc=false and handle continuation below */

        } else if (_wcsnicmp(line, L"dependencies", 12) == 0) {
            in_desc = false;
            /* Skip dependencies block */

        } else if (in_desc) {
            /* Description continuation line */
            bool indented = (raw[0]==L' '||raw[0]==L'\t'||
                             raw[0]==L'\''||raw[0]==L'"');
            if (line[0]==L'['||line[0]==L']') { in_desc = false; continue; }
            if (indented && line[0]) {
                AppendDesc(m.description, line);
            } else if (!indented) {
                in_desc = false;
            }
        } else if (m.requirements[0] && !in_desc) {
            /* Requirements continuation — collect indented lines */
            bool indented = (raw[0]==L' '||raw[0]==L'\t');
            if (indented && line[0] &&
                line[0] != L'[' && line[0] != L']') {
                /* Append to requirements */
                int cur = (int)wcslen(m.requirements);
                if (cur < 508) {
                    m.requirements[cur++] = L'\r';
                    m.requirements[cur++] = L'\n';
                    m.requirements[cur]   = L'\0';
                    wcsncat(m.requirements, line, 511 - cur);
                }
            }
        }
    }

    fclose(f);

    if (found_any) {
        s->meta        = m;
        s->meta_loaded = true;
    }
    /* If found_any is false (file missing or no header found),
       leave meta_loaded=false so Meta_Parse retries next hover
       after the sync thread has downloaded the file. */
}

void Meta_ParseAll(void)
{
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            Meta_Parse(&g.folders[fi].scripts[si]);
}
