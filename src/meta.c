/*
 * meta.c  -  Parse script header into ScriptMeta.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <share.h> /* _SH_DENYNO for _wfsopen */
#include <wctype.h> /* iswdigit */

#define DESC_MAX 1023 /* ScriptMeta.description is [1024], so max writable index is 1023 */

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
    while (n > 0 && (s[n - 1] == L'\r' || s[n - 1] == L'\n' ||
                     s[n - 1] == L' ' || s[n - 1] == L'\t'))
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
    while (*p == L' ' || *p == L'\t' || *p == L'#' ||
           *p == L'\'' || *p == L'"' || *p == L'-')
        p++;
    if (p != s) memmove_s(s, (wcslen(p) + 1) * sizeof(WCHAR), p, (wcslen(p) + 1) * sizeof(WCHAR));
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
    while (*p == L' ' || *p == L'\t')
        p++;
    if (*p != L':') return NULL;
    p++;
    while (*p == L' ' || *p == L'\t')
        p++;
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
    if (cur > 0)
    {
        buf[cur++] = L' ';
        buf[cur] = L'\0';
    } /* cur < DESC_MAX guaranteed by guard above */
    wcsncat_s(buf, DESC_MAX + 1, text, _TRUNCATE);
}

/* ================================================================== */
/*  Meta_IsChangeDate  (static)                                        */
/*  Purpose: Reports whether a line starts a new change entry — it     */
/*           begins with a DD.MM.YY date (one or two digits per part). */
/*  In:  s — trimmed line                                               */
/*  Out: true if s starts with a date                                   */
/* ================================================================== */
static bool Meta_IsChangeDate(const WCHAR *s)
{
    for (int part = 0; part < 3; part++)
    {
        int digits = 0;
        while (iswdigit(*s) && digits < 4)
        {
            s++;
            digits++;
        }
        if (digits == 0) return false;
        if (part < 2 && *s++ != L'.') return false;
    }
    return true;
}

/* ================================================================== */
/*  Meta_AddChange  (static)                                           */
/*  Purpose: Adds one line of the Change: block to the metadata.  A    */
/*           line starting with a date begins a new entry, which also  */
/*           becomes last_change (entries are listed oldest first);    */
/*           any other line continues the previous entry.              */
/*  In:  m    — metadata being built                                   */
/*       text — trimmed line text                                       */
/*  Out: (void — m->change_log and m->last_change updated)             */
/* ================================================================== */
static void Meta_AddChange(ScriptMeta *m, const WCHAR *text)
{
    if (!text || !*text) return;
    bool new_entry = Meta_IsChangeDate(text) || !m->last_change[0];
    if (new_entry)
    {
        if (m->change_log[0]) wcsncat_s(m->change_log, _countof(m->change_log), L"\r\n", _TRUNCATE);
        wcsncpy_s(m->last_change, _countof(m->last_change), text, _TRUNCATE);
    }
    else
    {
        wcsncat_s(m->change_log, _countof(m->change_log), L" ", _TRUNCATE);
        wcsncat_s(m->last_change, _countof(m->last_change), L" ", _TRUNCATE);
        wcsncat_s(m->last_change, _countof(m->last_change), text, _TRUNCATE);
    }
    wcsncat_s(m->change_log, _countof(m->change_log), text, _TRUNCATE);
}

/* ================================================================== */
/*  Meta_IsKey  (static)                                               */
/*  Purpose: Reports whether a trimmed line is "<key>:" (optionally    */
/*           with spaces before the colon), whether or not a value     */
/*           follows.                                                   */
/*  In:  line — trimmed line; key — key name (case-insensitive)        */
/*  Out: true if the line starts with that key                          */
/* ================================================================== */
static bool Meta_IsKey(const WCHAR *line, const WCHAR *key)
{
    size_t kl = wcslen(key);
    if (_wcsnicmp(line, key, kl) != 0) return false;
    const WCHAR *p = line + kl;
    while (*p == L' ' || *p == L'\t')
        p++;
    return *p == L':';
}

/* ================================================================== */
/*  Meta_IsKnownKey  (static)                                          */
/*  Purpose: Reports whether a line starts one of the header keys, so  */
/*           an Args: block ends there.  Only the known keys count —   */
/*           an argument line such as "mode:[a|b]" also has a colon.   */
/*  In:  line — trimmed line                                            */
/*  Out: true if the line starts a known header key                     */
/* ================================================================== */
static bool Meta_IsKnownKey(const WCHAR *line)
{
    static const WCHAR *keys[] = {L"Script name", L"Version", L"Code", L"Release", L"Purpose",
                                  L"Author", L"Date", L"Description", L"requirements", L"Args",
                                  L"Change", NULL};
    for (int i = 0; keys[i]; i++)
        if (Meta_IsKey(line, keys[i])) return true;
    return _wcsnicmp(line, L"dependencies", 12) == 0; /* "dependencies = [" has no colon */
}

/* ================================================================== */
/*  Meta_AddLine  (static)                                             */
/*  Purpose: Appends one entry to a newline-separated list buffer.     */
/*  In:  buf — list buffer; max — its capacity; text — entry           */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Meta_AddLine(WCHAR *buf, size_t max, const WCHAR *text)
{
    if (!text || !*text) return;
    if (buf[0]) wcsncat_s(buf, max, L"\n", _TRUNCATE);
    wcsncat_s(buf, max, text, _TRUNCATE);
}

/* ================================================================== */
/*  Meta_AddArg  (static)                                              */
/*  Purpose: Records one Args: parameter line, e.g.                    */
/*           tolerance:float=0.01 "Merge tolerance in mm".  Parsed     */
/*           into form fields by the Run with Arguments dialog.        */
/*  In:  m — metadata being built; text — trimmed parameter line      */
/*  Out: (void — m->args_spec updated)                                 */
/* ================================================================== */
static void Meta_AddArg(ScriptMeta *m, const WCHAR *text)
{
    Meta_AddLine(m->args_spec, _countof(m->args_spec), text);
}

/* ================================================================== */
/*  Meta_AddDeps  (static)                                             */
/*  Purpose: Collects every quoted requirement string ("pycatia",      */
/*           'pywin32>=306') on one line of a dependencies = [...]     */
/*           list into m->dependencies.                                */
/*  In:  m — metadata being built; raw — untrimmed line                */
/*  Out: (void — m->dependencies updated)                              */
/* ================================================================== */
static void Meta_AddDeps(ScriptMeta *m, const WCHAR *raw)
{
    for (const WCHAR *p = raw; *p; p++)
    {
        if (*p != L'"' && *p != L'\'') continue;
        WCHAR q = *p;
        const WCHAR *end = wcschr(p + 1, q);
        if (!end) break;
        WCHAR req[128] = {0};
        size_t n = (size_t)(end - p - 1);
        if (n > 0 && n < _countof(req))
        {
            wcsncpy_s(req, _countof(req), p + 1, n);
            Meta_AddLine(m->dependencies, _countof(m->dependencies), req);
        }
        p = end;
    }
}

/* ================================================================== */
/*  Meta_Parse                                                          */
/*  Purpose: Opens the local cached .py file for script s and parses  */
/*           its header block into s->meta.  The header is bounded by  */
/*           dashed separator lines and contains Key: value pairs for  */
/*           Purpose, Author, Version, Date, Description, Code,       */
/*           Release, and requirements.  The Change: block that       */
/*           follows the header (up to the next separator) fills       */
/*           change_log and last_change.  No-op if already loaded.    */
/*  In:  s — script whose local path points to a downloaded .py file   */
/*  Out: (void — sets s->meta and s->meta_loaded = true on success)   */
/* ================================================================== */
void Meta_Parse(Script *s)
{
    if (s->meta_loaded) return; /* already parsed — nothing to do    */
    if (!s->local[0]) return; /* no local path yet (not downloaded)*/
    if (GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES) return; /* file missing — INVALID_FILE_ATTRIBUTES is the sentinel for "not found" */

    /* _wfsopen with _SH_DENYNO rather than _wfopen_s: _wfopen_s denies sharing, which
       would make a sync download of this same file fail while the header is read */
    FILE *f = _wfsopen(s->local, L"r, ccs=UTF-8", _SH_DENYNO); /* try UTF-8 first (script headers are ASCII-safe) */
    if (!f) f = _wfsopen(s->local, L"r", _SH_DENYNO); /* fall back to system default encoding if UTF-8 open fails */
    if (!f) return; /* file exists but cannot be opened (e.g. locked) */

    ScriptMeta m;
    ZeroMemory(&m, sizeof(m));

    WCHAR raw[1024], line[1024];
    int lineno = 0;
    bool in_header = false;
    bool after_header = false; /* past the header's closing separator: only the Change: block is read */
    bool in_change = false; /* inside the Change: block */
    bool in_deps = false; /* inside a multi-line dependencies = [ ... ] list */
    bool in_args = false; /* inside the Args: block */
    bool in_desc = false;
    bool found_any = false;

    while (lineno < 200 && fgetws(raw, 1024, f)) /* 200-line cap: header is always near the top; avoids reading whole file */
    {
        lineno++;
        TrimRight(raw);
        wcsncpy_s(line, 1024, raw, _TRUNCATE);
        StripLeading(line); /* strip comment chars (#, ', ", -) from the raw line copy */
        TrimRight(line);

        /* Detect dashed separator line — a run of 10+ dashes with no other content */
        bool is_dashes = false;
        {
            int dash_count = 0, other = 0;
            for (WCHAR *p = raw; *p && *p != L'\r' && *p != L'\n'; p++)
            {
                if (*p == L'-')
                    dash_count++;
                else if (*p != L' ' && *p != L'\t' && *p != L'\'' && *p != L'"')
                    other++;
            }
            is_dashes = (dash_count >= 10 && other == 0); /* 10 = minimum dash run to be a separator */
        }

        if (is_dashes)
        {
            if (!in_header)
                in_header = true; /* first separator = start of header block */
            else if (!after_header)
                after_header = true; /* second separator = end of header; the Change: block follows */
            else
                break; /* third separator closes the Change: block; stop */
            in_desc = false;
            continue;
        }

        if (!in_header) continue; /* skip lines before the opening separator */

        /* "Change:" starts the change history — normally just after the header's
           closing separator, but older headers put it inside the header block */
        bool change_key = _wcsnicmp(line, L"Change", 6) == 0; /* 6 = strlen("Change"); a ':' must follow */
        if (change_key)
        {
            const WCHAR *p = line + 6;
            while (*p == L' ' || *p == L'\t')
                p++;
            change_key = (*p == L':');
        }
        if (change_key)
        {
            in_change = true;
            in_desc = false;
            const WCHAR *first = MatchKey(line, L"Change"); /* NULL when "Change:" has nothing on its line */
            if (first) Meta_AddChange(&m, first);
            found_any = true;
            continue;
        }

        /* Stop if we reach actual Python code — the header is over */
        if (_wcsnicmp(line, L"import ", 7) == 0 || /* length includes the trailing space */
            _wcsnicmp(line, L"from ", 5) == 0 ||
            _wcsnicmp(line, L"def ", 4) == 0 ||
            _wcsnicmp(line, L"class ", 6) == 0) break;

        /* Skip blank lines */
        if (line[0] == L'\0') continue;

        /* Inside the Change: block every line is history; after the header but
           outside it, nothing else is read */
        if (in_change)
        {
            Meta_AddChange(&m, line);
            continue;
        }
        if (after_header) continue;

        /* dependencies = [ "pkg", "pkg>=1.2", ] — requirement strings are taken from
           the raw line, because StripLeading would eat a leading quote */
        if (in_deps && Meta_IsKnownKey(line)) in_deps = false; /* list never closed: the next key ends it */
        if (in_deps || _wcsnicmp(line, L"dependencies", 12) == 0) /* 12 = strlen("dependencies") */
        {
            in_deps = true;
            in_desc = false;
            in_args = false;
            Meta_AddDeps(&m, raw);
            if (wcschr(raw, L']')) in_deps = false; /* closing bracket ends the list */
            continue;
        }

        /* Args: — one script parameter per line, value on the key line optional */
        if (Meta_IsKey(line, L"Args"))
        {
            in_args = true;
            in_desc = false;
            const WCHAR *first = MatchKey(line, L"Args"); /* NULL when "Args:" has nothing on its line */
            if (first) Meta_AddArg(&m, first);
            found_any = true;
            continue;
        }
        if (in_args)
        {
            bool indented = (raw[0] == L' ' || raw[0] == L'\t');
            if (indented && !Meta_IsKnownKey(line))
            {
                Meta_AddArg(&m, line);
                continue;
            }
            in_args = false; /* this line is the next key: fall through */
        }

        const WCHAR *val = NULL;

        if (MatchKey(line, L"Script name") != NULL)
        {
            in_desc = false; /* "Script name:" has no display field; recognise it so it doesn't bleed into description */
        }
        else if ((val = MatchKey(line, L"Version")) != NULL)
        {
            wcsncpy_s(m.version, 32, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Author")) != NULL)
        {
            wcsncpy_s(m.author, 64, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Date")) != NULL)
        {
            wcsncpy_s(m.date, 32, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Purpose")) != NULL)
        {
            wcsncpy_s(m.purpose, 128, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Code")) != NULL)
        {
            wcsncpy_s(m.code, 64, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Release")) != NULL)
        {
            wcsncpy_s(m.release, 32, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
        }
        else if ((val = MatchKey(line, L"Description")) != NULL)
        {
            wcsncpy_s(m.description, DESC_MAX + 1, val, _TRUNCATE);
            found_any = true;
            in_desc = true;
        }
        else if (_wcsnicmp(line, L"requirements", 12) == 0)
        {
            /* "requirements:" may have content on the same line or span multiple indented lines */
            val = MatchKey(line, L"requirements");
            if (val) wcsncpy_s(m.requirements, 512, val, _TRUNCATE);
            found_any = true;
            in_desc = false;
            /* Continuation lines are collected in the m.requirements[0] branch below */
        }
        else if (in_desc)
        {
            /* Description continuation: collect indented or comment-prefixed lines */
            bool indented = (raw[0] == L' ' || raw[0] == L'\t' ||
                             raw[0] == L'\'' || raw[0] == L'"');
            if (line[0] == L'[' || line[0] == L']')
            {
                in_desc = false;
                continue;
            } /* INI-style section marker ends description */
            if (indented && line[0])
            {
                AppendDesc(m.description, line);
            }
            else if (!indented)
            {
                in_desc = false; /* unindented non-key line means description block is over */
            }
        }
        else if (m.requirements[0] && !in_desc)
        {
            /* Requirements continuation — collect indented lines after the "requirements:" key */
            bool indented = (raw[0] == L' ' || raw[0] == L'\t');
            if (indented && line[0] &&
                line[0] != L'[' && line[0] != L']')
            {
                int cur = (int)wcslen(m.requirements);
                if (cur < 508)
                { /* 508 = 512 buffer - 4 reserved for \r\n\0 appended below */
                    m.requirements[cur++] = L'\r';
                    m.requirements[cur++] = L'\n';
                    m.requirements[cur] = L'\0';
                    wcsncat_s(m.requirements, 512, line, _TRUNCATE);
                }
            }
        }
    }

    fclose(f);

    if (found_any)
    {
        /* At least one recognised key was parsed — commit the result */
        s->meta = m;
        s->meta_loaded = true;
    }
    /* If found_any is false the file exists but has no recognised header.
       Leaving meta_loaded=false lets Meta_Parse retry on the next tooltip
       hover, which handles the race where the sync thread downloads the
       file after this parse attempt. */
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
