/*
 * prefs.c  -  User preferences: favourites, hidden scripts, notes, run counts.
 * Stored in %APPDATA%\CatiaMenuWin32\prefs.ini
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <wctype.h> /* iswalnum, iswspace */

/* ================================================================== */
/*  Prefs_GetPath  (static)                                            */
/*  Purpose: Builds the full path to prefs.ini in %APPDATA%\           */
/*           CatiaMenuWin32\ and stores it in out.                     */
/*  In:  out — buffer to receive the path                              */
/*       max — capacity of out in WCHARs                               */
/*  Out: (void — out is populated)                                      */
/* ================================================================== */
static void Prefs_GetPath(WCHAR *out, int max)
{
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\%s", g.appdata_dir, PREFS_FILE);
}

/* ================================================================== */
/*  PathToKey  (static)                                                */
/*  Purpose: Converts a GitHub path string into a safe INI key by      */
/*           replacing backslashes and forward slashes with underscores.*/
/*  In:  path — GitHub-relative path (e.g. "FolderA/script.py")       */
/*       key  — buffer to receive the sanitised key                    */
/*       max  — capacity of key in WCHARs                              */
/*  Out: (void — key is populated)                                      */
/* ================================================================== */
static void PathToKey(const WCHAR *path, WCHAR *key, int max)
{
    wcsncpy_s(key, max, path, _TRUNCATE);
    for (WCHAR *p = key; *p; p++)
    {
        if (*p == L'\\' || *p == L'/') *p = L'_';
    }
}

/* ================================================================== */
/*  Prefs_IsFavourite                                                   */
/*  Purpose: Returns true if the script identified by gh_path is       */
/*           marked as a favourite in prefs.ini.                       */
/*  In:  gh_path — GitHub-relative script path used as identity key    */
/*  Out: true if favourited; false otherwise                            */
/* ================================================================== */
bool Prefs_IsFavourite(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"Favourites", key, 0, ini) != 0; /* default 0 = not favourited; any non-zero = favourited */
}

/* ================================================================== */
/*  Prefs_SetFavourite                                                  */
/*  Purpose: Writes the favourite flag for the given script to         */
/*           prefs.ini immediately.                                     */
/*  In:  gh_path — GitHub-relative script path                         */
/*       fav     — true to mark as favourite; false to unmark          */
/*  Out: (void)                                                         */
/* ================================================================== */
void Prefs_SetFavourite(const WCHAR *gh_path, bool fav)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Favourites", key, fav ? L"1" : L"0", ini);
}

/* ================================================================== */
/*  Prefs_IsHidden                                                      */
/*  Purpose: Returns true if the script identified by gh_path is       */
/*           marked as hidden in prefs.ini.                            */
/*  In:  gh_path — GitHub-relative script path                         */
/*  Out: true if hidden; false otherwise                                */
/* ================================================================== */
bool Prefs_IsHidden(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"Hidden", key, 0, ini) != 0; /* default 0 = visible; any non-zero = hidden */
}

/* ================================================================== */
/*  Prefs_SetHidden                                                     */
/*  Purpose: Writes the hidden flag for the given script to prefs.ini. */
/*  In:  gh_path — GitHub-relative script path                         */
/*       hidden  — true to hide; false to unhide                       */
/*  Out: (void)                                                         */
/* ================================================================== */
void Prefs_SetHidden(const WCHAR *gh_path, bool hidden)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Hidden", key, hidden ? L"1" : L"0", ini);
}

/* ================================================================== */
/*  Prefs_GetRunCount                                                   */
/*  Purpose: Returns the number of times the script has been run,      */
/*           as stored in prefs.ini under the [RunCount] section.      */
/*  In:  gh_path — GitHub-relative script path                         */
/*  Out: run count (0 if not yet stored)                                */
/* ================================================================== */
int Prefs_GetRunCount(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"RunCount", key, 0, ini);
}

/* ================================================================== */
/*  Prefs_IncrementRunCount                                             */
/*  Purpose: Reads the current run count for a script from prefs.ini,  */
/*           increments it by one, and writes it back immediately.     */
/*  In:  gh_path — GitHub-relative script path                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Prefs_IncrementRunCount(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH], val[16];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    int count = GetPrivateProfileInt(L"RunCount", key, 0, ini) + 1;
    _snwprintf_s(val, 15, _TRUNCATE, L"%d", count);
    WritePrivateProfileString(L"RunCount", key, val, ini);
}

/* ================================================================== */
/*  Prefs_GetNote                                                       */
/*  Purpose: Reads the user-written note for a script from prefs.ini  */
/*           into the provided buffer.                                  */
/*  In:  gh_path — GitHub-relative script path                         */
/*       note    — buffer to receive the note text                     */
/*       max     — capacity of note in WCHARs                          */
/*  Out: (void — note is populated; empty string if no note stored)    */
/* ================================================================== */
void Prefs_GetNote(const WCHAR *gh_path, WCHAR *note, int max)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    GetPrivateProfileString(L"Notes", key, L"", note, max, ini);
}

/* ================================================================== */
/*  Prefs_SetNote                                                       */
/*  Purpose: Writes a user note for the given script to prefs.ini.    */
/*  In:  gh_path — GitHub-relative script path                         */
/*       note    — note text to store                                   */
/*  Out: (void)                                                         */
/* ================================================================== */
void Prefs_SetNote(const WCHAR *gh_path, const WCHAR *note)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Notes", key, note, ini);
}

/* ================================================================== */
/*  Prefs_Load / Prefs_Save                                             */
/*  Purpose: Exist for API symmetry.  All prefs reads are lazy (called */
/*           per script on demand) and all writes are immediate.       */
/*           No bulk load or save is needed.                           */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Prefs_Load(void)
{ /* INI reads are lazy - no bulk load needed */
}
void Prefs_Save(void)
{ /* INI writes are immediate - no bulk save needed */
}

/* ================================================================== */
/*  Prefs_ApplyToFolders                                                */
/*  Purpose: Reads favourite, hidden, run_count, and note from         */
/*           prefs.ini for every script in g.folders[] and writes the  */
/*           values into the Script struct fields.  Called after a     */
/*           sync or on startup to merge persisted prefs with the      */
/*           in-memory script list.                                    */
/*  In:  (reads g.folders[], g.folder_count)                           */
/*  Out: (void — updates is_favourite, is_hidden, run_count, note      */
/*               fields in each Script)                                */
/* ================================================================== */
void Prefs_ApplyToFolders(void)
{
    WCHAR ini[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);

    /* The first time any repository script is known, everything already there
       counts as seen — a fresh install or upgrade must not badge every script */
    bool initialised = GetPrivateProfileInt(L"Badges", L"Initialized", 0, ini) != 0;
    bool any_repo_script = false;

    for (int fi = 0; fi < g.folder_count; fi++)
    {
        ScriptFolder *f = &g.folders[fi];
        for (int si = 0; si < f->count; si++)
        {
            Script *s = &f->scripts[si];
            s->is_favourite = Prefs_IsFavourite(s->gh_path);
            s->is_hidden = Prefs_IsHidden(s->gh_path);
            s->run_count = Prefs_GetRunCount(s->gh_path);
            Prefs_GetNote(s->gh_path, s->note, MAX_NOTE_LEN);

            /* New/updated badge: compare the current SHA with the one last seen.
               Local scripts have no SHA and are never badged. */
            s->badge = BADGE_NONE;
            if (s->source == SCRIPT_SRC_LOCAL || !s->sha[0]) continue;
            any_repo_script = true;
            WCHAR key[MAX_APPPATH], seen[MAX_SHA] = {0};
            PathToKey(s->gh_path, key, MAX_APPPATH);
            if (!initialised)
            {
                WritePrivateProfileString(L"SeenSHA", key, s->sha, ini);
                continue;
            }
            GetPrivateProfileString(L"SeenSHA", key, L"", seen, MAX_SHA, ini);
            if (!seen[0])
                s->badge = BADGE_NEW;
            else if (_wcsicmp(seen, s->sha) != 0)
                s->badge = BADGE_UPDATED;
        }
    }
    if (!initialised && any_repo_script)
        WritePrivateProfileString(L"Badges", L"Initialized", L"1", ini);
}

/* ================================================================== */
/*  Badges_Repaint  (static)                                           */
/*  Purpose: Repaints everything that shows badges: the tab bar, the  */
/*           script buttons and the Quick Bar.                         */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Badges_Repaint(void)
{
    if (g.hwnd_tab) InvalidateRect(g.hwnd_tab, NULL, FALSE);
    if (g.hwnd_scroll) InvalidateRect(g.hwnd_scroll, NULL, FALSE);
    for (HWND b = g.hwnd_scroll ? GetWindow(g.hwnd_scroll, GW_CHILD) : NULL; b; b = GetWindow(b, GW_HWNDNEXT))
        InvalidateRect(b, NULL, FALSE);
    if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
}

/* ================================================================== */
/*  Badges_MarkSeen                                                     */
/*  Purpose: Records that the user has seen a script in its current   */
/*           version and clears its badge in every tab showing it     */
/*           (including the Favourites copy).  Called when a script   */
/*           is run or its details are opened.                        */
/*  In:  gh_path — script identity                                     */
/*       sha     — current SHA (empty for local scripts: no-op)        */
/*  Out: (void)                                                         */
/* ================================================================== */
void Badges_MarkSeen(const WCHAR *gh_path, const WCHAR *sha)
{
    if (!sha || !sha[0]) return;
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"SeenSHA", key, sha, ini);

    bool changed = false;
    EnterCriticalSection(&g.cs_folders);
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
        {
            Script *s = &g.folders[fi].scripts[si];
            if (s->badge != BADGE_NONE && wcscmp(s->gh_path, gh_path) == 0)
            {
                s->badge = BADGE_NONE;
                changed = true;
            }
        }
    LeaveCriticalSection(&g.cs_folders);
    if (changed) Badges_Repaint();
}

/* ================================================================== */
/*  Badges_MarkAllSeen                                                  */
/*  Purpose: Clears every new/updated badge at once (View menu).       */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Badges_MarkAllSeen(void)
{
    WCHAR ini[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    EnterCriticalSection(&g.cs_folders);
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
        {
            Script *s = &g.folders[fi].scripts[si];
            if (s->badge == BADGE_NONE) continue;
            WCHAR key[MAX_APPPATH];
            PathToKey(s->gh_path, key, MAX_APPPATH);
            WritePrivateProfileString(L"SeenSHA", key, s->sha, ini);
            s->badge = BADGE_NONE;
        }
    LeaveCriticalSection(&g.cs_folders);
    Badges_Repaint();
}

/* ================================================================== */
/*  Badges_FolderHasUnseen                                              */
/*  Purpose: Reports whether a tab holds any visible badged script,    */
/*           so the tab bar can mark it.                               */
/*  In:  fi — folder index                                              */
/*  Out: true if at least one non-hidden script has a badge            */
/* ================================================================== */
bool Badges_FolderHasUnseen(int fi)
{
    if (fi < 0 || fi >= g.folder_count) return false;
    const ScriptFolder *f = &g.folders[fi];
    for (int si = 0; si < f->count; si++)
        if (f->scripts[si].badge != BADGE_NONE && !f->scripts[si].is_hidden) return true;
    return false;
}

/* ================================================================== */
/*  Tabs_BuildFavourites                                                */
/*  Purpose: Builds (or rebuilds) the synthetic "Favourites" folder at */
/*           index 0 of g.folders[] from all non-hidden, favourited   */
/*           scripts across the other folders.  Removes any existing  */
/*           Favourites tab first to avoid duplicates.  No-op if no   */
/*           scripts are favourited.                                   */
/*  In:  (reads g.folders[], g.folder_count; writes g.folders[0])      */
/*  Out: (void — may increment g.folder_count by 1)                    */
/* ================================================================== */
void Tabs_BuildFavourites(void)
{
    /* First remove any existing Favourites tab to avoid duplicates */
    for (int fi = 0; fi < g.folder_count; fi++)
    {
        if (wcscmp(g.folders[fi].name, L"Favourites") == 0)
        {
            Folder_Free(&g.folders[fi]);
            /* Shift remaining folders left to fill the gap */
            for (int i = fi; i < g.folder_count - 1; i++)
                g.folders[i] = g.folders[i + 1];
            g.folder_count--;
            ZeroMemory(&g.folders[g.folder_count], sizeof(ScriptFolder)); /* clear now-unused last slot to avoid dangling pointer */
            if (g.active_tab > fi)
                g.active_tab--; /* shift active index to compensate for removed slot */
            else if (g.active_tab == fi)
                g.active_tab = 0; /* active tab was the favourites tab — reset to first */
            break;
        }
    }

    /* Count favourites — skip hidden */
    int fav_count = 0;
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            if (g.folders[fi].scripts[si].is_favourite &&
                !g.folders[fi].scripts[si].is_hidden)
                fav_count++;

    if (fav_count == 0) return; /* no favourited scripts — don't create an empty tab */

    if (g.folder_count >= MAX_FOLDERS) return; /* no room to insert a new tab */
    /* Shift all existing folders one position right to free slot 0 */
    for (int fi = g.folder_count; fi > 0; fi--)
        g.folders[fi] = g.folders[fi - 1];
    g.folder_count++;
    if (g.active_tab >= 0) g.active_tab++; /* compensate for the new slot 0 so the active tab stays correct */
    /* Zero slot 0 — g.folders[1] now owns the scripts pointer that was there */
    ZeroMemory(&g.folders[0], sizeof(ScriptFolder));

    /* Build favourites folder at index 0 */
    ScriptFolder *fav = &g.folders[0];
    /* The shift copied g.folders[1]'s scripts pointer into g.folders[0].
       Do NOT free it — g.folders[1] still owns that memory.
       Just zero out this slot and allocate fresh for the favourites. */
    ZeroMemory(fav, sizeof(*fav));
    wcsncpy_s(fav->name, MAX_NAME, L"Favourites", _TRUNCATE);
    wcsncpy_s(fav->display, MAX_NAME, L"\u2605 Favourites", _TRUNCATE);
    fav->loaded = true;
    Folder_Alloc(fav, fav_count > 0 ? fav_count : 8);

    /* Copy favourited scripts in — skip hidden */
    for (int fi = 1; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            if (g.folders[fi].scripts[si].is_favourite &&
                !g.folders[fi].scripts[si].is_hidden)
            {
                Script *dst = Folder_Push(fav);
                if (dst) *dst = g.folders[fi].scripts[si];
            }

    /* Refresh the Quick Launch Bar whenever favourites change */
    QuickBar_Rebuild();
}

/* ================================================================== */
/*  ScriptDetailsDlgProc                                                */
/*  Purpose: Dialog procedure for IDD_SCRIPT_DETAILS.  Displays all   */
/*           metadata fields, note, local path, and favourite/hidden   */
/*           checkboxes for a script.  IDOK saves note, favourite, and */
/*           hidden changes to prefs.ini and to the Script struct.     */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID on WM_COMMAND)                      */
/*       lp   — LPARAM on WM_INITDIALOG: pointer to the Script         */
/*  Out: INT_PTR — IDOK / IDCANCEL via EndDialog                       */
/* ================================================================== */
INT_PTR CALLBACK ScriptDetailsDlgProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    static Script *s = NULL; /* static: persists across messages for this dialog instance */
    switch (msg)
    {
    case WM_INITDIALOG:
        s = (Script *)lp;
        if (!s)
        {
            EndDialog(hwnd, 0);
            return FALSE;
        } /* safety: should never be NULL */
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s); /* stash for WM_COMMAND */

        /* Ensure meta is loaded */
        Meta_Parse(s);

        SetDlgItemText(hwnd, IDC_DETAIL_NAME, s->name);
        SetDlgItemText(hwnd, IDC_DETAIL_PURPOSE, s->meta.purpose);
        SetDlgItemText(hwnd, IDC_DETAIL_AUTHOR, s->meta.author);
        SetDlgItemText(hwnd, IDC_DETAIL_VERSION, s->meta.version);
        SetDlgItemText(hwnd, IDC_DETAIL_DATE, s->meta.date);
        SetDlgItemText(hwnd, IDC_DETAIL_CODE, s->meta.code);
        SetDlgItemText(hwnd, IDC_DETAIL_RELEASE, s->meta.release);
        SetDlgItemText(hwnd, IDC_DETAIL_DESC, s->meta.description);
        SetDlgItemText(hwnd, IDC_DETAIL_REQS, s->meta.requirements);
        SetDlgItemText(hwnd, IDC_DETAIL_CHANGES, s->meta.change_log[0] ? s->meta.change_log : L"(none listed)");
        SetDlgItemText(hwnd, IDC_DETAIL_NOTE, s->note);

        /* Opening the details counts as having seen this version */
        Badges_MarkSeen(s->gh_path, s->sha);
        SetDlgItemText(hwnd, IDC_DETAIL_PATH, s->local);

        CheckDlgButton(hwnd, IDC_CHK_FAVOURITE,
                       s->is_favourite ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_HIDDEN,
                       s->is_hidden ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        s = (Script *)GetWindowLongPtr(hwnd, GWLP_USERDATA); /* retrieve Script pointer stored in WM_INITDIALOG */
        if (!s)
        {
            EndDialog(hwnd, 0);
            return TRUE;
        } /* defensive: close if pointer was lost */

        switch (LOWORD(wp))
        {
        case IDOK:
        {
            /* Save note */
            WCHAR note[MAX_NOTE_LEN] = {0};
            GetDlgItemText(hwnd, IDC_DETAIL_NOTE, note, MAX_NOTE_LEN - 1);
            wcsncpy_s(s->note, MAX_NOTE_LEN, note, _TRUNCATE);
            Prefs_SetNote(s->gh_path, note);

            /* Save favourite */
            bool fav = IsDlgButtonChecked(hwnd, IDC_CHK_FAVOURITE) == BST_CHECKED;
            s->is_favourite = fav;
            Prefs_SetFavourite(s->gh_path, fav);

            /* Save hidden */
            bool hidden = IsDlgButtonChecked(hwnd, IDC_CHK_HIDDEN) == BST_CHECKED;
            s->is_hidden = hidden;
            Prefs_SetHidden(s->gh_path, hidden);

            EndDialog(hwnd, IDOK);
            break;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/*  Args: parameter form                                               */
/*  A script can describe its command-line parameters in its header:   */
/*      Args:   tolerance:float=0.01 "Merge tolerance in mm"           */
/*              mode:[fast|full]=fast "Search mode"                    */
/*              overwrite:bool=false                                   */
/*  Run with Arguments then shows one labelled field per parameter     */
/*  and passes --name value (a bool passes --name when ticked).        */
/*  Types: str (default), int, float, bool, or [a|b|c] choices.        */
/* ================================================================== */
#define ARGF_MAX 16 /* parameters shown in one form */

typedef enum
{
    ARGT_STR = 0,
    ARGT_INT,
    ARGT_FLOAT,
    ARGT_BOOL,
    ARGT_CHOICE
} ArgType;

typedef struct
{
    WCHAR name[64];
    ArgType type;
    WCHAR def[128]; /* default value from the header */
    WCHAR help[128]; /* quoted help text, shown as the label */
    WCHAR choices[256]; /* ARGT_CHOICE: '|'-separated values */
} ArgField;

typedef struct
{
    ArgField f[ARGF_MAX];
    int count;
} ArgForm;

/* ================================================================== */
/*  ArgForm_ParseLine  (static)                                        */
/*  Purpose: Parses one Args: line — name:type=default "help" — into  */
/*           a field.  Lines without a valid name are skipped.         */
/*  In:  line — parameter line; out — field to fill                    */
/*  Out: true if out holds a parameter                                 */
/* ================================================================== */
static bool ArgForm_ParseLine(const WCHAR *line, ArgField *out)
{
    ZeroMemory(out, sizeof(*out));
    WCHAR buf[512];
    wcsncpy_s(buf, _countof(buf), line, _TRUNCATE);

    /* "help text" — everything between the first and the last double quote */
    WCHAR *q1 = wcschr(buf, L'"');
    if (q1)
    {
        WCHAR *q2 = wcsrchr(buf, L'"');
        if (q2 > q1)
        {
            *q2 = L'\0';
            wcsncpy_s(out->help, _countof(out->help), q1 + 1, _TRUNCATE);
        }
        *q1 = L'\0';
    }

    /* name — letters, digits, '_' and '-' up to the ':' */
    WCHAR *p = buf;
    while (*p == L' ' || *p == L'\t')
        p++;
    int n = 0;
    while (*p && (iswalnum(*p) || *p == L'_' || *p == L'-') && n < (int)_countof(out->name) - 1)
        out->name[n++] = *p++;
    out->name[n] = L'\0';
    if (!n) return false;
    while (*p == L' ' || *p == L'\t')
        p++;
    if (*p == L':') p++;
    while (*p == L' ' || *p == L'\t')
        p++;

    /* type — [a|b|c] choices, or a type name; "=default" may follow */
    WCHAR *eq = NULL;
    if (*p == L'[')
    {
        WCHAR *close = wcschr(p, L']');
        if (close)
        {
            *close = L'\0';
            wcsncpy_s(out->choices, _countof(out->choices), p + 1, _TRUNCATE);
            out->type = ARGT_CHOICE;
            eq = wcschr(close + 1, L'=');
        }
    }
    else
    {
        eq = wcschr(p, L'=');
        if (eq) *eq = L'\0';
        WCHAR type[16] = {0};
        int t = 0;
        while (*p && !iswspace(*p) && t < (int)_countof(type) - 1)
            type[t++] = *p++;
        if (_wcsicmp(type, L"int") == 0 || _wcsicmp(type, L"integer") == 0)
            out->type = ARGT_INT;
        else if (_wcsicmp(type, L"float") == 0 || _wcsicmp(type, L"number") == 0)
            out->type = ARGT_FLOAT;
        else if (_wcsicmp(type, L"bool") == 0 || _wcsicmp(type, L"flag") == 0)
            out->type = ARGT_BOOL;
        else
            out->type = ARGT_STR; /* str, string, or anything unknown */
    }
    if (eq)
    {
        WCHAR *d = eq + 1;
        while (*d == L' ' || *d == L'\t')
            d++;
        wcsncpy_s(out->def, _countof(out->def), d, _TRUNCATE);
        for (size_t k = wcslen(out->def); k > 0 && iswspace(out->def[k - 1]); k--)
            out->def[k - 1] = L'\0';
    }
    return true;
}

/* ================================================================== */
/*  ArgForm_ValueKey  (static)                                         */
/*  Purpose: prefs.ini [Args] key for a script's remembered value:     */
/*           "<script key>|<parameter>", or "<script key>|~" for the   */
/*           free-text additional arguments.                           */
/* ================================================================== */
static void ArgForm_ValueKey(const Script *s, const WCHAR *param, WCHAR *key, int max)
{
    WCHAR sk[MAX_APPPATH];
    PathToKey(s->gh_path[0] ? s->gh_path : s->local, sk, MAX_APPPATH);
    _snwprintf_s(key, max, _TRUNCATE, L"%s|%s", sk, param);
}

/* ================================================================== */
/*  ArgForm_AppendArg  (static)                                        */
/*  Purpose: Appends one argument to a command line, quoted the way    */
/*           Python's argv parser expects when it contains spaces or  */
/*           quotes (backslashes before a quote are doubled).          */
/*  In:  cmd — command line being built; max — capacity; arg — text   */
/*  Out: (void)                                                         */
/* ================================================================== */
static void ArgForm_AppendArg(WCHAR *cmd, int max, const WCHAR *arg)
{
    if (cmd[0]) wcsncat_s(cmd, max, L" ", _TRUNCATE);
    if (arg[0] && !wcspbrk(arg, L" \t\""))
    {
        wcsncat_s(cmd, max, arg, _TRUNCATE);
        return;
    }
    WCHAR q[MAX_APPPATH * 2];
    int n = 0, cap = (int)_countof(q) - 3;
    q[n++] = L'"';
    for (const WCHAR *p = arg; *p && n < cap; p++)
    {
        int slashes = 0;
        while (*p == L'\\')
        {
            slashes++;
            p++;
        }
        if (*p == L'"' || *p == L'\0')
            slashes *= 2; /* backslashes before a quote (or the closing quote) are doubled */
        for (int k = 0; k < slashes && n < cap; k++)
            q[n++] = L'\\';
        if (*p == L'\0') break;
        if (*p == L'"' && n < cap) q[n++] = L'\\';
        if (n < cap) q[n++] = *p;
    }
    q[n++] = L'"';
    q[n] = L'\0';
    wcsncat_s(cmd, max, q, _TRUNCATE);
}

/* ================================================================== */
/*  ArgForm_Build  (static)                                            */
/*  Purpose: Parses the script's Args: lines and lays the form out:   */
/*           one row per parameter (label + field), then the free-text */
/*           "Additional arguments" row, then the buttons.  Fields     */
/*           start with the value remembered for this script, else    */
/*           the header's default.                                     */
/*  In:  hwnd — Run with Arguments dialog; s — script                  */
/*  Out: heap ArgForm (the dialog frees it), or NULL if the script has */
/*       no Args: parameters                                           */
/* ================================================================== */
static ArgForm *ArgForm_Build(HWND hwnd, const Script *s)
{
    if (!s->meta.args_spec[0]) return NULL;
    ArgForm *form = (ArgForm *)calloc(1, sizeof(ArgForm));
    if (!form) return NULL;
    WCHAR spec[_countof(s->meta.args_spec)];
    wcsncpy_s(spec, _countof(spec), s->meta.args_spec, _TRUNCATE);
    WCHAR *ctx = NULL;
    for (WCHAR *line = wcstok_s(spec, L"\n", &ctx); line && form->count < ARGF_MAX;
         line = wcstok_s(NULL, L"\n", &ctx))
        if (ArgForm_ParseLine(line, &form->f[form->count])) form->count++;
    if (!form->count)
    {
        free(form);
        return NULL;
    }

    WCHAR ini[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    HFONT font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    HINSTANCE hi = GetModuleHandle(NULL);

    /* Layout in dialog units, converted to pixels with MapDialogRect */
    const int row = 20, label_w = 150, field_x = 162, field_w = 190, dlg_w = 360; /* DLU */
    for (int i = 0; i < form->count; i++)
    {
        ArgField *f = &form->f[i];
        int y = 8 + i * row;
        WCHAR label[200];
        if (f->help[0])
            _snwprintf_s(label, _countof(label), _TRUNCATE, L"%s (--%s):", f->help, f->name);
        else
            _snwprintf_s(label, _countof(label), _TRUNCATE, L"--%s:", f->name);
        RECT lr = {7, y + 2, 7 + label_w, y + 2 + 10};
        MapDialogRect(hwnd, &lr);
        HWND hl = CreateWindow(L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
                               lr.left, lr.top, lr.right - lr.left, lr.bottom - lr.top,
                               hwnd, (HMENU)(INT_PTR)(IDC_ARGFORM_BASE + 2 * i), hi, NULL);
        SendMessage(hl, WM_SETFONT, (WPARAM)font, FALSE);

        /* Remembered value for this script, else the header default */
        WCHAR key[MAX_APPPATH + 80], val[MAX_APPPATH];
        ArgForm_ValueKey(s, f->name, key, (int)_countof(key));
        GetPrivateProfileString(L"Args", key, f->def, val, MAX_APPPATH, ini);

        int id = IDC_ARGFORM_BASE + 2 * i + 1;
        RECT fr = {field_x, y, field_x + field_w, y + (f->type == ARGT_CHOICE ? 100 : 14)}; /* 100 = dropped-down list height */
        MapDialogRect(hwnd, &fr);
        HWND hf = NULL;
        if (f->type == ARGT_BOOL)
        {
            hf = CreateWindow(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                              fr.left, fr.top, fr.right - fr.left, fr.bottom - fr.top,
                              hwnd, (HMENU)(INT_PTR)id, hi, NULL);
            bool on = _wcsicmp(val, L"true") == 0 || _wcsicmp(val, L"1") == 0 || _wcsicmp(val, L"yes") == 0;
            SendMessage(hf, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        else if (f->type == ARGT_CHOICE)
        {
            hf = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                              fr.left, fr.top, fr.right - fr.left, fr.bottom - fr.top,
                              hwnd, (HMENU)(INT_PTR)id, hi, NULL);
            WCHAR ch[256];
            wcsncpy_s(ch, _countof(ch), f->choices, _TRUNCATE);
            WCHAR *c2 = NULL;
            for (WCHAR *c = wcstok_s(ch, L"|", &c2); c; c = wcstok_s(NULL, L"|", &c2))
                SendMessage(hf, CB_ADDSTRING, 0, (LPARAM)c);
            LRESULT sel = SendMessage(hf, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)val);
            SendMessage(hf, CB_SETCURSEL, (WPARAM)(sel == CB_ERR ? 0 : sel), 0); /* unknown value: first choice */
        }
        else
        {
            hf = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", val, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                fr.left, fr.top, fr.right - fr.left, fr.bottom - fr.top,
                                hwnd, (HMENU)(INT_PTR)id, hi, NULL);
        }
        SendMessage(hf, WM_SETFONT, (WPARAM)font, FALSE);
    }

    /* Free-text row, then the buttons, below the parameters */
    int y = 8 + form->count * row + 4;
    RECT r = {7, y + 2, 7 + label_w, y + 12};
    MapDialogRect(hwnd, &r);
    SetDlgItemText(hwnd, IDC_LBL_RUN_ARGS, L"Additional arguments:");
    SetWindowPos(GetDlgItem(hwnd, IDC_LBL_RUN_ARGS), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
    r = (RECT){field_x, y, field_x + field_w, y + 14};
    MapDialogRect(hwnd, &r);
    SetWindowPos(GetDlgItem(hwnd, IDC_EDIT_RUN_ARGS), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
    y += 26;
    r = (RECT){dlg_w - 116, y, dlg_w - 66, y + 14};
    MapDialogRect(hwnd, &r);
    SetWindowPos(GetDlgItem(hwnd, IDOK), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
    r = (RECT){dlg_w - 60, y, dlg_w - 10, y + 14};
    MapDialogRect(hwnd, &r);
    SetWindowPos(GetDlgItem(hwnd, IDCANCEL), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);

    /* Grow the dialog to fit, keeping it centred where it was */
    RECT client = {0, 0, dlg_w, y + 22};
    MapDialogRect(hwnd, &client);
    AdjustWindowRectEx(&client, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int w = client.right - client.left, h = client.bottom - client.top;
    SetWindowPos(hwnd, NULL, wr.left - (w - (wr.right - wr.left)) / 2, wr.top - (h - (wr.bottom - wr.top)) / 2,
                 w, h, SWP_NOZORDER);
    return form;
}

/* ================================================================== */
/*  ArgForm_Collect  (static)                                          */
/*  Purpose: Validates the form and builds the argument string:        */
/*           --name value for each filled field (--name alone for a   */
/*           ticked bool), then the additional free text.  Remembers  */
/*           the values for this script.                               */
/*  In:  hwnd — dialog; form — fields; s — script; out/max — result   */
/*  Out: false (with the offending field focused) if a number is      */
/*       invalid                                                       */
/* ================================================================== */
static bool ArgForm_Collect(HWND hwnd, const ArgForm *form, const Script *s, WCHAR *out, int max)
{
    WCHAR ini[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    out[0] = L'\0';
    for (int i = 0; form && i < form->count; i++)
    {
        const ArgField *f = &form->f[i];
        HWND hf = GetDlgItem(hwnd, IDC_ARGFORM_BASE + 2 * i + 1);
        WCHAR val[MAX_APPPATH] = {0};
        if (f->type == ARGT_BOOL)
            wcsncpy_s(val, MAX_APPPATH, SendMessage(hf, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"true" : L"false", _TRUNCATE);
        else
            GetWindowText(hf, val, MAX_APPPATH);

        if ((f->type == ARGT_INT || f->type == ARGT_FLOAT) && val[0])
        {
            WCHAR *end = NULL;
            if (f->type == ARGT_INT)
                (void)wcstol(val, &end, 10);
            else
                (void)wcstod(val, &end);
            while (end && iswspace(*end))
                end++;
            if (!end || *end)
            {
                WCHAR msg[200];
                _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"--%s must be %s.", f->name,
                             f->type == ARGT_INT ? L"a whole number" : L"a number (use . as the decimal point)");
                MessageBox(hwnd, msg, L"Run with Arguments", MB_ICONWARNING | MB_OK);
                SetFocus(hf);
                return false;
            }
        }

        WCHAR key[MAX_APPPATH + 80];
        ArgForm_ValueKey(s, f->name, key, (int)_countof(key));
        WritePrivateProfileString(L"Args", key, val, ini);

        WCHAR flag[80];
        _snwprintf_s(flag, _countof(flag), _TRUNCATE, L"--%s", f->name);
        if (f->type == ARGT_BOOL)
        {
            if (wcscmp(val, L"true") == 0) ArgForm_AppendArg(out, max, flag);
        }
        else if (val[0])
        {
            ArgForm_AppendArg(out, max, flag);
            ArgForm_AppendArg(out, max, val);
        }
    }

    WCHAR extra[MAX_APPPATH] = {0};
    GetDlgItemText(hwnd, IDC_EDIT_RUN_ARGS, extra, MAX_APPPATH);
    WCHAR key[MAX_APPPATH + 80];
    ArgForm_ValueKey(s, L"~", key, (int)_countof(key));
    WritePrivateProfileString(L"Args", key, extra, ini);
    if (extra[0])
    {
        if (out[0]) wcsncat_s(out, max, L" ", _TRUNCATE);
        wcsncat_s(out, max, extra, _TRUNCATE); /* typed as on a command line: passed through unchanged */
    }
    return true;
}

/* ================================================================== */
/*  RunWithArgsDlgProc                                                  */
/*  Purpose: Dialog procedure for IDD_RUN_ARGS.  Shows a field for     */
/*           each parameter the script declares in its Args: header   */
/*           (see ArgForm_Build) plus a free-text box for additional  */
/*           arguments, both starting with the values last used for   */
/*           this script.  IDOK builds the argument string into the   */
/*           caller's RunArgsDlgData.                                  */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM                                                  */
/*       lp   — LPARAM on WM_INITDIALOG: pointer to a RunArgsDlgData   */
/*  Out: INT_PTR — IDOK / IDCANCEL via EndDialog                       */
/* ================================================================== */
INT_PTR CALLBACK RunWithArgsDlgProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        RunArgsDlgData *d = (RunArgsDlgData *)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)d); /* stash for WM_COMMAND */
        SendDlgItemMessage(hwnd, IDC_EDIT_RUN_ARGS, EM_LIMITTEXT, MAX_APPPATH - 1, 0);
        if (d && d->script)
        {
            WCHAR title[MAX_NAME + 20]; /* +20 = "Run: " prefix (5) + null terminator + headroom */
            _snwprintf_s(title, MAX_NAME + 19, _TRUNCATE, L"Run: %s", d->script->name);
            SetWindowText(hwnd, title);

            /* The script lives in g.folders; parsing fills its metadata in place */
            Meta_Parse((Script *)d->script);
            d->form = ArgForm_Build(hwnd, d->script);

            WCHAR ini[MAX_APPPATH], key[MAX_APPPATH + 80], extra[MAX_APPPATH] = {0};
            Prefs_GetPath(ini, MAX_APPPATH);
            ArgForm_ValueKey(d->script, L"~", key, (int)_countof(key));
            GetPrivateProfileString(L"Args", key, L"", extra, MAX_APPPATH, ini);
            SetDlgItemText(hwnd, IDC_EDIT_RUN_ARGS, extra);
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:
        {
            /* Build the arguments now — the controls are destroyed with the dialog */
            RunArgsDlgData *d = (RunArgsDlgData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (d && d->script &&
                !ArgForm_Collect(hwnd, (const ArgForm *)d->form, d->script, d->args, (int)_countof(d->args)))
                return TRUE; /* invalid number: keep the dialog open */
            EndDialog(hwnd, IDOK);
            break;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    case WM_DESTROY:
    {
        RunArgsDlgData *d = (RunArgsDlgData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (d)
        {
            free(d->form);
            d->form = NULL;
        }
        return FALSE;
    }
    }
    return FALSE;
}

/* ================================================================== */
/*  ScriptNoteDlgProc                                                   */
/*  Purpose: Dialog procedure for IDD_SCRIPT_NOTE.  Populates the note */
/*           edit box on WM_INITDIALOG; IDOK saves the note back to   */
/*           the Script struct and to prefs.ini.                       */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM                                                  */
/*       lp   — LPARAM on WM_INITDIALOG: pointer to the Script         */
/*  Out: INT_PTR — IDOK / IDCANCEL via EndDialog                       */
/* ================================================================== */
INT_PTR CALLBACK ScriptNoteDlgProc(HWND hwnd, UINT msg,
                                   WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        Script *s = (Script *)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s); /* stash for WM_COMMAND */
        if (s) SetDlgItemText(hwnd, IDC_EDIT_NOTE, s->note); /* populate edit box from existing note if any */
        return TRUE;
    }
    case WM_COMMAND:
    {
        Script *s = (Script *)GetWindowLongPtr(hwnd, GWLP_USERDATA); /* retrieve Script pointer stashed in WM_INITDIALOG */
        switch (LOWORD(wp))
        {
        case IDOK:
            if (s)
            {
                GetDlgItemText(hwnd, IDC_EDIT_NOTE, s->note, MAX_NOTE_LEN - 1); /* -1 leaves room for null terminator */
                Prefs_SetNote(s->gh_path, s->note);
            }
            EndDialog(hwnd, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    }
    }
    return FALSE;
}

/* ================================================================== */
/*  HiddenScriptsDlgProc                                                */
/*  Purpose: Dialog procedure for IDD_HIDDEN_SCRIPTS.  Populates a    */
/*           ListView with all currently hidden scripts, along with    */
/*           their folder name.  The Unhide and Unhide All buttons     */
/*           clear the hidden flag in the Script struct and in         */
/*           prefs.ini and remove the row from the list.               */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (button ID on WM_COMMAND)                       */
/*       lp   — LPARAM (unused)                                        */
/*  Out: INT_PTR — IDOK / IDCANCEL via EndDialog                       */
/* ================================================================== */
INT_PTR CALLBACK HiddenScriptsDlgProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND hList = GetDlgItem(hwnd, IDC_LST_HIDDEN);
        LVCOLUMN lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = L"Script";
        lvc.cx = 220;
        ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = L"Folder";
        lvc.cx = 150;
        ListView_InsertColumn(hList, 1, &lvc);

        /* Populate with hidden scripts */
        int row = 0;
        for (int fi = 0; fi < g.folder_count; fi++)
        {
            if (wcscmp(g.folders[fi].name, L"Favourites") == 0) continue; /* skip synthetic Favourites tab; its scripts live in real folders */
            for (int si = 0; si < g.folders[fi].count; si++)
            {
                Script *s = &g.folders[fi].scripts[si];
                if (!s->is_hidden) continue;
                LVITEM lvi = {0};
                lvi.mask = LVIF_TEXT | LVIF_PARAM;
                lvi.iItem = row;
                lvi.pszText = s->name;
                lvi.lParam = (LPARAM)s; /* store Script pointer so Unhide handler can retrieve it via GetItem */
                ListView_InsertItem(hList, &lvi);
                ListView_SetItemText(hList, row, 1, g.folders[fi].display);
                row++;
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BTN_UNHIDE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_HIDDEN);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED); /* -1 starts from beginning; returns -1 if nothing selected */
            if (sel < 0) break; /* nothing selected — nothing to do */
            LVITEM lvi = {0};
            lvi.mask = LVIF_PARAM;
            lvi.iItem = sel;
            ListView_GetItem(hList, &lvi);
            Script *s = (Script *)lvi.lParam;
            if (s)
            {
                s->is_hidden = false;
                Prefs_SetHidden(s->gh_path, false);
                ListView_DeleteItem(hList, sel);
            }
            break;
        }

        case IDC_BTN_UNHIDE_ALL:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_HIDDEN);
            int count = ListView_GetItemCount(hList);
            for (int i = 0; i < count; i++)
            {
                LVITEM lvi = {0};
                lvi.mask = LVIF_PARAM;
                lvi.iItem = i;
                ListView_GetItem(hList, &lvi);
                Script *s = (Script *)lvi.lParam;
                if (s)
                {
                    s->is_hidden = false;
                    Prefs_SetHidden(s->gh_path, false);
                }
            }
            ListView_DeleteAllItems(hList); /* clear list display after unhiding all */
            break;
        }
        case IDOK:
        case IDCANCEL:
            EndDialog(hwnd, LOWORD(wp));
            break;
        }
        return TRUE;
    }
    return FALSE;
}
