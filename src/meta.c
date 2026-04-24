/*
 * meta.c  -  Parse script header into ScriptMeta.
 *
 * Header format:
 *   -------...
 *   Script name:   Rename_Hybrid_Shapes.py
 *   Version:       1.0
 *   Purpose:       Renames multiple hybrid shapes in one go.
 *   Author:        Kai-Uwe Rathjen
 *   Date:          04.03.26
 *   Description:   This script will ask the user...
 *                  continuation line...
 *   -------...
 *
 * meta_loaded is only set true when at least one field is found,
 * so a failed parse (empty file, not yet downloaded) retries next call.
 *
 * CatiaMenuWin32  |  Author: Kai-Uwe Rathjen  |  License: MIT
 */

#include "main.h"

static void TrimRight(WCHAR *s)
{
    int n = (int)wcslen(s);
    while (n > 0 && (s[n-1]==L'\r'||s[n-1]==L'\n'||
                     s[n-1]==L' ' ||s[n-1]==L'\t'))
        s[--n] = L'\0';
}

/* Strip leading comment/dash/quote chars and whitespace */
static void StripLeading(WCHAR *s)
{
    WCHAR *p = s;
    while (*p==L' '||*p==L'\t'||*p==L'#'||
           *p==L'\''||*p==L'"'||*p==L'-') p++;
    if (p != s) memmove(s, p, (wcslen(p)+1)*sizeof(WCHAR));
}

/* Match "Key: value" case-insensitively. Returns ptr to value or NULL. */
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

static void AppendDesc(WCHAR *buf, int max, const WCHAR *text)
{
    if (!text || !*text) return;
    int cur = (int)wcslen(buf);
    if (cur > 0 && cur < max-2) { buf[cur++]=L' '; buf[cur]=L'\0'; }
    wcsncat(buf, text, max-cur-1);
}

/* ================================================================== */
/*  Meta_Parse                                                          */
/* ================================================================== */
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

    WCHAR raw[512], line[512];
    int   lineno    = 0;
    bool  in_header = false;
    bool  in_desc   = false;
    bool  found_any = false;

    while (lineno < 150 && fgetws(raw, 512, f))
    {
        lineno++;
        TrimRight(raw);
        wcscpy(line, raw);
        StripLeading(line);
        TrimRight(line);

        /* Detect dashed separator line in the raw content */
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
            in_desc = false;
            continue;
        }

        if (!in_header) continue;
        if (line[0] == L'\0') { in_desc = false; continue; }

        /* Stop at Python code */
        if (_wcsnicmp(line, L"import ",  7)==0 ||
            _wcsnicmp(line, L"from ",    5)==0 ||
            _wcsnicmp(line, L"def ",     4)==0 ||
            _wcsnicmp(line, L"class ",   6)==0 ||
            _wcsnicmp(line, L"Change",   6)==0 ||
            _wcsnicmp(line, L"dependencies",12)==0 ||
            _wcsnicmp(line, L"requirements",12)==0) break;

        const WCHAR *val = NULL;

        if ((val = MatchKey(line, L"Script name")) != NULL) {
            /* We already have a formatted name - skip */
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

        } else if ((val = MatchKey(line, L"Description")) != NULL) {
            wcsncpy(m.description, val, 255);
            found_any = true; in_desc = true;

        } else if (in_desc) {
            /* Continuation: original line must be indented */
            bool indented = (raw[0]==L' '||raw[0]==L'\t'||
                             raw[0]==L'\''||raw[0]==L'"');
            if (indented && line[0] &&
                line[0]!=L'['&&line[0]!=L']') {
                AppendDesc(m.description, 255, line);
            } else {
                in_desc = false;
            }
        }
    }

    fclose(f);

    if (found_any) {
        s->meta = m;
        s->meta_loaded = true;
    }
    /* If nothing found, meta_loaded stays false so we retry next hover */
}

/* ================================================================== */
/*  Meta_ParseAll                                                       */
/* ================================================================== */
void Meta_ParseAll(void)
{
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            Meta_Parse(&g.folders[fi].scripts[si]);
}
