/*
 * meta.c  -  Parse script header into ScriptMeta.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

#define DESC_MAX 1023   /* must match ScriptMeta.description buffer - 1 */

/* ================================================================== */
/*  TrimRight  (static)                                                */
/*  Purpose: Removes trailing whitespace and line-ending characters    */
/*           (\r, \n, space, tab) from a wide string in-place.         */
/*  In:  s — wide string to trim (modified in-place)                   */
/*  Out: (void)                                                         */
/* ================================================================== */
static void TrimRight(WCHAR *s)
{
    int n = (int)wcslen(s);
    while (n > 0 && (s[n-1]==L'\r'||s[n-1]==L'\n'||
                     s[n-1]==L' ' ||s[n-1]==L'\t'))
        s[--n] = L'\0';
}

/* ================================================================== */
/*  StripLeading  (static)                                             */
/*  Purpose: Removes leading spaces, tabs, and comment delimiter       */
/*           characters (#, ', ", -) from a wide string in-place,     */
/*           shifting the remaining content to the front of the buffer.*/
/*  In:  s — wide string to strip (modified in-place)                  */
/*  Out: (void)                                                         */
/* ================================================================== */
static void StripLeading(WCHAR *s)
{
    WCHAR *p = s;
    while (*p==L' '||*p==L'\t'||*p==L'#'||
           *p==L'\''||*p==L'"'||*p==L'-') p++;
    if (p != s) memmove_s(s, (wcslen(p)+1)*sizeof(WCHAR), p, (wcslen(p)+1)*sizeof(WCHAR));
}

/* ================================================================== */
/*  MatchKey  (static)                                                 */
/*  Purpose: Case-insensitively checks whether `line` begins with      */
/*           `key` followed by optional whitespace and a colon.       */
/*           Returns a pointer to the trimmed value after the colon,  */
/*           or NULL if the pattern does not match or value is empty.  */
/*  In:  line — wide string from the script header                     */
/*       key  — expected key name (e.g. L"Purpose")                   */
/*  Out: pointer into `line` at the start of the value; NULL if no    */
/*       match or value is blank                                        */
/* ================================================================== */
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

/* ================================================================== */
/*  AppendDesc  (static)                                               */
/*  Purpose: Appends a continuation text segment to the description    */
/*           buffer, inserting a space separator before the new text   */
/*           if the buffer is not empty.  Truncates at DESC_MAX chars. */
/*  In:  buf  — destination description buffer (ScriptMeta.description)*/
/*       text — wide string segment to append                          */
/*  Out: (void — buf is modified in-place)                             */
/* ================================================================== */
static void AppendDesc(WCHAR *buf, const WCHAR *text)
{
    if (!text || !*text) return;
    int cur = (int)wcslen(buf);
    if (cur >= DESC_MAX) return;
    /* Add a space separator if buffer not empty */
    if (cur > 0 && cur < DESC_MAX) { buf[cur++]=L' '; buf[cur]=L'\0'; }
    wcsncat_s(buf, DESC_MAX + 1, text, _TRUNCATE);
}

/* ================================================================== */
/*  Meta_Parse                                                          */
/*  Purpose: Opens the local cached .py file for script s and parses  */
/*           its header block into s->meta.  The header is bounded by  */
/*           dashed separator lines and contains Key: value pairs for  */
/*           Purpose, Author, Version, Date, Description, Code,       */
/*           Release, and requirements.  No-op if already loaded.     */
/*  In:  s — script whose local path points to a downloaded .py file   */
/*  Out: (void — sets s->meta and s->meta_loaded = true on success)   */
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
    ZeroMemory(&m, sizeof(m));

    WCHAR raw[1024], line[1024];
    int   lineno    = 0;
    bool  in_header = false;
    bool  in_desc   = false;
    bool  found_any = false;

    while (lineno < 200 && fgetws(raw, 1024, f))
    {
        lineno++;
        TrimRight(raw);
        wcsncpy_s(line, 1024, raw, _TRUNCATE);
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
            else            { break; } /* second dashes = end of header */
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

        if (MatchKey(line, L"Script name") != NULL) {
            /* Script name not displayed — just stop description mode */
            in_desc = false;

        } else if ((val = MatchKey(line, L"Version")) != NULL) {
            wcsncpy_s(m.version, 32, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Author")) != NULL) {
            wcsncpy_s(m.author, 64, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Date")) != NULL) {
            wcsncpy_s(m.date, 32, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Purpose")) != NULL) {
            wcsncpy_s(m.purpose, 128, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Code")) != NULL) {
            wcsncpy_s(m.code, 64, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Release")) != NULL) {
            wcsncpy_s(m.release, 32, val, _TRUNCATE);
            found_any = true; in_desc = false;

        } else if ((val = MatchKey(line, L"Description")) != NULL) {
            wcsncpy_s(m.description, DESC_MAX + 1, val, _TRUNCATE);
            found_any = true; in_desc = true;

        } else if (_wcsnicmp(line, L"requirements", 12) == 0) {
            /* requirements: may have content on same line or next lines */
            val = MatchKey(line, L"requirements");
            if (val) wcsncpy_s(m.requirements, 512, val, _TRUNCATE);
            found_any = true; in_desc = false;
            /* Continuation lines are collected below via m.requirements[0] check */

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
                    wcsncat_s(m.requirements, 512, line, _TRUNCATE);
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

/* ================================================================== */
/*  Meta_ParseAll                                                       */
/*  Purpose: Calls Meta_Parse for every script in every folder in      */
/*           g.folders[].  Used after a sync completes to batch-load   */
/*           all metadata from the freshly downloaded files.           */
/*  In:  (reads g.folders[], g.folder_count)                           */
/*  Out: (void — updates Script.meta and meta_loaded for each script)  */
/* ================================================================== */
void Meta_ParseAll(void)
{
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            Meta_Parse(&g.folders[fi].scripts[si]);
}
