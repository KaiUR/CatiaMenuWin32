/*
 * meta.c  -  Parse script header comments into ScriptMeta.
 *
 * Looks for lines in the cached .py file matching:
 *   # Purpose:     ...
 *   # Author:      ...
 *   # Version:     ...
 *   # Date:        ...
 *   # Description: ...  (may span multiple comment lines)
 *
 * Only reads the first 50 lines so it is fast.
 * CatiaMenuWin32
 */

#include "main.h"

/* ------------------------------------------------------------------ */
static void TrimRight(WCHAR *s)
{
    int n = (int)wcslen(s);
    while (n > 0 && (s[n-1] == L'\r' || s[n-1] == L'\n' ||
                     s[n-1] == L' '  || s[n-1] == L'\t'))
        s[--n] = L'\0';
}

/* Extract value after "# Key: value" */
static bool ExtractField(const WCHAR *line, const WCHAR *key, WCHAR *out, int max)
{
    /* skip leading whitespace and # */
    const WCHAR *p = line;
    while (*p == L' ' || *p == L'\t') p++;
    if (*p != L'#') return false;
    p++;
    while (*p == L' ' || *p == L'\t') p++;

    size_t klen = wcslen(key);
    if (_wcsnicmp(p, key, klen) != 0) return false;
    p += klen;
    while (*p == L' ' || *p == L'\t' || *p == L':') p++;

    wcsncpy(out, p, max - 1);
    out[max - 1] = L'\0';
    TrimRight(out);
    return out[0] != L'\0';
}

/* ================================================================== */
/*  Meta_Parse  -  read the first 50 lines of s->local                */
/* ================================================================== */
void Meta_Parse(Script *s)
{
    if (s->meta_loaded) return;
    memset(&s->meta, 0, sizeof(s->meta));
    s->meta_loaded = true;   /* mark even on failure so we don't retry */

    if (!s->local[0]) return;
    if (GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES) return;

    FILE *f = _wfopen(s->local, L"r, ccs=UTF-8");
    if (!f) {
        /* Try without BOM hint */
        f = _wfopen(s->local, L"r");
        if (!f) return;
    }

    WCHAR line[512];
    int   lineno = 0;
    bool  in_desc = false;
    WCHAR desc_buf[256] = {0};

    while (lineno < 80 && fgetws(line, 512, f)) {
        lineno++;
        TrimRight(line);

        /* Stop at first non-comment, non-blank line after header */
        const WCHAR *p = line;
        while (*p == L' ' || *p == L'\t') p++;
        if (*p == L'\0') { in_desc = false; continue; }
        if (*p != L'#')  break;

        WCHAR val[256];
        if (ExtractField(line, L"Purpose",     val, 256))
            wcsncpy(s->meta.purpose,     val, 127);
        if (ExtractField(line, L"Author",      val, 256))
            wcsncpy(s->meta.author,      val, 63);
        if (ExtractField(line, L"Version",     val, 256))
            wcsncpy(s->meta.version,     val, 31);
        if (ExtractField(line, L"Date",        val, 256))
            wcsncpy(s->meta.date,        val, 31);
        if (ExtractField(line, L"Description", val, 256)) {
            wcsncpy(desc_buf, val, 255);
            in_desc = true;
        } else if (in_desc) {
            /* Continuation line: bare "# more text" */
            p++;
            while (*p == L' ' || *p == L'\t') p++;
            if (*p && *p != L'#') {
                /* append */
                if (desc_buf[0] && wcslen(desc_buf) < 250) {
                    wcsncat(desc_buf, L" ", 255 - wcslen(desc_buf));
                    wcsncat(desc_buf, p,   255 - wcslen(desc_buf));
                }
            } else {
                in_desc = false;
            }
        }
    }
    fclose(f);

    wcsncpy(s->meta.description, desc_buf, 255);
}

/* ================================================================== */
/*  Meta_ParseAll  -  parse all cached scripts                         */
/* ================================================================== */
void Meta_ParseAll(void)
{
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            Meta_Parse(&g.folders[fi].scripts[si]);
}
