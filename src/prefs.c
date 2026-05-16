/*
 * prefs.c  -  User preferences: favourites, hidden scripts, notes, run counts.
 * Stored in %APPDATA%\CatiaMenuWin32\prefs.ini
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

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
    for (WCHAR *p = key; *p; p++) {
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
    return GetPrivateProfileInt(L"Favourites", key, 0, ini) != 0;
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
    return GetPrivateProfileInt(L"Hidden", key, 0, ini) != 0;
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
void Prefs_Load(void)  { /* INI reads are lazy - no bulk load needed */ }
void Prefs_Save(void)  { /* INI writes are immediate - no bulk save needed */ }

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
    for (int fi = 0; fi < g.folder_count; fi++) {
        ScriptFolder *f = &g.folders[fi];
        for (int si = 0; si < f->count; si++) {
            Script *s = &f->scripts[si];
            s->is_favourite = Prefs_IsFavourite(s->gh_path);
            s->is_hidden    = Prefs_IsHidden(s->gh_path);
            s->run_count    = Prefs_GetRunCount(s->gh_path);
            Prefs_GetNote(s->gh_path, s->note, MAX_NOTE_LEN);
        }
    }
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
    for (int fi = 0; fi < g.folder_count; fi++) {
        if (wcscmp(g.folders[fi].name, L"Favourites") == 0) {
            /* Free the favourites scripts first */
            Folder_Free(&g.folders[fi]);
            /* Shift remaining folders left */
            for (int i = fi; i < g.folder_count - 1; i++)
                g.folders[i] = g.folders[i + 1];
            g.folder_count--;
            /* Zero the now-unused last slot so its pointer isn't dangling */
            ZeroMemory(&g.folders[g.folder_count], sizeof(ScriptFolder));
            if (g.active_tab > fi)       g.active_tab--;
            else if (g.active_tab == fi) g.active_tab = 0;
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

    if (fav_count == 0) return;  /* no favourites — no tab */

    /* Shift existing folders right by 1 to make room at index 0 */
    if (g.folder_count >= MAX_FOLDERS) return;
    for (int fi = g.folder_count; fi > 0; fi--)
        g.folders[fi] = g.folders[fi - 1];
    g.folder_count++;
    if (g.active_tab >= 0) g.active_tab++;
    /* Zero slot 0 — g.folders[1] now owns the scripts pointer that was there */
    ZeroMemory(&g.folders[0], sizeof(ScriptFolder));

    /* Build favourites folder at index 0 */
    ScriptFolder *fav = &g.folders[0];
    /* The shift copied g.folders[1]'s scripts pointer into g.folders[0].
       Do NOT free it — g.folders[1] still owns that memory.
       Just zero out this slot and allocate fresh for the favourites. */
    ZeroMemory(fav, sizeof(*fav));
    wcsncpy_s(fav->name,    MAX_NAME, L"Favourites",       _TRUNCATE);
    wcsncpy_s(fav->display, MAX_NAME, L"\u2605 Favourites", _TRUNCATE);
    fav->loaded = true;
    Folder_Alloc(fav, fav_count > 0 ? fav_count : 8);

    /* Copy favourited scripts in — skip hidden */
    for (int fi = 1; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            if (g.folders[fi].scripts[si].is_favourite &&
                !g.folders[fi].scripts[si].is_hidden) {
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
    static Script *s = NULL;
    switch (msg)
    {
    case WM_INITDIALOG:
        s = (Script *)lp;
        if (!s) { EndDialog(hwnd, 0); return FALSE; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s);

        /* Ensure meta is loaded */
        Meta_Parse(s);

        SetDlgItemText(hwnd, IDC_DETAIL_NAME,    s->name);
        SetDlgItemText(hwnd, IDC_DETAIL_PURPOSE, s->meta.purpose);
        SetDlgItemText(hwnd, IDC_DETAIL_AUTHOR,  s->meta.author);
        SetDlgItemText(hwnd, IDC_DETAIL_VERSION, s->meta.version);
        SetDlgItemText(hwnd, IDC_DETAIL_DATE,    s->meta.date);
        SetDlgItemText(hwnd, IDC_DETAIL_CODE,    s->meta.code);
        SetDlgItemText(hwnd, IDC_DETAIL_RELEASE, s->meta.release);
        SetDlgItemText(hwnd, IDC_DETAIL_DESC,    s->meta.description);
        SetDlgItemText(hwnd, IDC_DETAIL_REQS,    s->meta.requirements);
        SetDlgItemText(hwnd, IDC_DETAIL_NOTE,    s->note);
        SetDlgItemText(hwnd, IDC_DETAIL_PATH,    s->local);

        CheckDlgButton(hwnd, IDC_CHK_FAVOURITE,
            s->is_favourite ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHK_HIDDEN,
            s->is_hidden ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        s = (Script *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!s) { EndDialog(hwnd, 0); return TRUE; }

        switch (LOWORD(wp)) {
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
/*  RunWithArgsDlgProc                                                  */
/*  Purpose: Dialog procedure for IDD_RUN_ARGS.  Displays the script   */
/*           name in the title and provides an edit box for extra      */
/*           arguments.  The caller reads IDC_EDIT_ARGS after IDOK.   */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM                                                  */
/*       lp   — LPARAM on WM_INITDIALOG: pointer to the Script         */
/*  Out: INT_PTR — IDOK / IDCANCEL via EndDialog                       */
/* ================================================================== */
INT_PTR CALLBACK RunWithArgsDlgProc(HWND hwnd, UINT msg,
                                     WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        Script *s = (Script *)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        if (s) {
            WCHAR title[MAX_NAME + 20];
            _snwprintf_s(title, MAX_NAME + 19, _TRUNCATE, L"Run: %s", s->name);
            SetWindowText(hwnd, title);
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:
        case IDCANCEL:
            EndDialog(hwnd, LOWORD(wp));
            break;
        }
        return TRUE;
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
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        if (s) SetDlgItemText(hwnd, IDC_EDIT_NOTE, s->note);
        return TRUE;
    }
    case WM_COMMAND:
    {
        Script *s = (Script *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        switch (LOWORD(wp)) {
        case IDOK:
            if (s) {
                GetDlgItemText(hwnd, IDC_EDIT_NOTE, s->note, MAX_NOTE_LEN - 1);
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
        lvc.pszText = L"Script"; lvc.cx = 220;
        ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = L"Folder"; lvc.cx = 150;
        ListView_InsertColumn(hList, 1, &lvc);

        /* Populate with hidden scripts */
        int row = 0;
        for (int fi = 0; fi < g.folder_count; fi++) {
            /* Skip favourites tab */
            if (wcscmp(g.folders[fi].name, L"Favourites") == 0) continue;
            for (int si = 0; si < g.folders[fi].count; si++) {
                Script *s = &g.folders[fi].scripts[si];
                if (!s->is_hidden) continue;
                LVITEM lvi = {0};
                lvi.mask    = LVIF_TEXT | LVIF_PARAM;
                lvi.iItem   = row;
                lvi.pszText = s->name;
                lvi.lParam  = (LPARAM)s;
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
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;
            LVITEM lvi = {0};
            lvi.mask   = LVIF_PARAM;
            lvi.iItem  = sel;
            ListView_GetItem(hList, &lvi);
            Script *s = (Script *)lvi.lParam;
            if (s) {
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
            for (int i = 0; i < count; i++) {
                LVITEM lvi = {0};
                lvi.mask  = LVIF_PARAM;
                lvi.iItem = i;
                ListView_GetItem(hList, &lvi);
                Script *s = (Script *)lvi.lParam;
                if (s) {
                    s->is_hidden = false;
                    Prefs_SetHidden(s->gh_path, false);
                }
            }
            ListView_DeleteAllItems(hList);
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
