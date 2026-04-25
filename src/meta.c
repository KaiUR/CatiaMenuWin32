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

        /* Stop at Python code or known section keywords */
        if (_wcsnicmp(line, L"import ",      7)==0 ||
            _wcsnicmp(line, L"from ",        5)==0 ||
            _wcsnicmp(line, L"def ",         4)==0 ||
            _wcsnicmp(line, L"class ",       6)==0 ||
            _wcsnicmp(line, L"Change",       6)==0 ||
            _wcsnicmp(line, L"dependencies", 12)==0 ||
            _wcsnicmp(line, L"requirements", 12)==0) break;

        /* Skip blank lines but do NOT stop description continuation */
        if (line[0] == L'\0') continue;

        const WCHAR *val = NULL;

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
            if (!m.version[0]) { wcsncpy(m.version, val, 31); found_any = true; }
            in_desc = false;

        } else if ((val = MatchKey(line, L"Release")) != NULL) {
            /* Skip - not displayed */
            in_desc = false;

        } else if ((val = MatchKey(line, L"Description")) != NULL) {
            wcsncpy(m.description, val, DESC_MAX);
            found_any = true; in_desc = true;

        } else if (in_desc) {
            /* Continuation line - must be indented in raw form */
            bool indented = (raw[0]==L' '||raw[0]==L'\t'||
                             raw[0]==L'\''||raw[0]==L'"');
            /* Stop if we hit a new key like "dependencies" or "[" */
            if (line[0]==L'['||line[0]==L']') { in_desc = false; continue; }
            if (indented && line[0]) {
                AppendDesc(m.description, line);
            } else if (!indented) {
                /* Unindented non-key line ends description */
                in_desc = false;
            }
        }
    }

    fclose(f);

    if (found_any) {
        s->meta        = m;
        s->meta_loaded = true;
    }
}

void Meta_ParseAll(void)
{
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            Meta_Parse(&g.folders[fi].scripts[si]);
}
