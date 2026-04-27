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
/*  Helpers                                                             */
/* ================================================================== */
static void Prefs_GetPath(WCHAR *out, int max)
{
    _snwprintf(out, max - 1, L"%s\\%s", g.appdata_dir, PREFS_FILE);
}

/* Encode gh_path as an INI key (replace \ and / with _) */
static void PathToKey(const WCHAR *path, WCHAR *key, int max)
{
    wcsncpy(key, path, max - 1);
    for (WCHAR *p = key; *p; p++) {
        if (*p == L'\\' || *p == L'/') *p = L'_';
    }
}

/* ================================================================== */
/*  Prefs_IsFavourite / Prefs_SetFavourite                             */
/* ================================================================== */
bool Prefs_IsFavourite(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"Favourites", key, 0, ini) != 0;
}

void Prefs_SetFavourite(const WCHAR *gh_path, bool fav)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Favourites", key, fav ? L"1" : L"0", ini);
}

/* ================================================================== */
/*  Prefs_IsHidden / Prefs_SetHidden                                   */
/* ================================================================== */
bool Prefs_IsHidden(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"Hidden", key, 0, ini) != 0;
}

void Prefs_SetHidden(const WCHAR *gh_path, bool hidden)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Hidden", key, hidden ? L"1" : L"0", ini);
}

/* ================================================================== */
/*  Prefs_GetRunCount / Prefs_IncrementRunCount                        */
/* ================================================================== */
int Prefs_GetRunCount(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    return GetPrivateProfileInt(L"RunCount", key, 0, ini);
}

void Prefs_IncrementRunCount(const WCHAR *gh_path)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH], val[16];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    int count = GetPrivateProfileInt(L"RunCount", key, 0, ini) + 1;
    _snwprintf(val, 15, L"%d", count);
    WritePrivateProfileString(L"RunCount", key, val, ini);
}

/* ================================================================== */
/*  Prefs_GetNote / Prefs_SetNote                                       */
/* ================================================================== */
void Prefs_GetNote(const WCHAR *gh_path, WCHAR *note, int max)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    GetPrivateProfileString(L"Notes", key, L"", note, max, ini);
}

void Prefs_SetNote(const WCHAR *gh_path, const WCHAR *note)
{
    WCHAR ini[MAX_APPPATH], key[MAX_APPPATH];
    Prefs_GetPath(ini, MAX_APPPATH);
    PathToKey(gh_path, key, MAX_APPPATH);
    WritePrivateProfileString(L"Notes", key, note, ini);
}

/* ================================================================== */
/*  Prefs_Load / Prefs_Save                                             */
/*  These exist for bulk operations - individual reads/writes are       */
/*  done directly via the INI functions above.                          */
/* ================================================================== */
void Prefs_Load(void)  { /* INI reads are lazy - no bulk load needed */ }
void Prefs_Save(void)  { /* INI writes are immediate - no bulk save needed */ }

/* ================================================================== */
/*  Prefs_ApplyToFolders                                                */
/*  Applies favourite, hidden and run_count from prefs.ini to all       */
/*  scripts currently in g.folders[].                                   */
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
/*  Builds a synthetic "Favourites" folder from all favourited scripts. */
/*  Called after Prefs_ApplyToFolders. Inserts at index 0.             */
/* ================================================================== */
void Tabs_BuildFavourites(void)
{
    /* First remove any existing Favourites tab to avoid duplicates */
    for (int fi = 0; fi < g.folder_count; fi++) {
        if (wcscmp(g.folders[fi].name, L"Favourites") == 0) {
            for (int i = fi; i < g.folder_count - 1; i++)
                g.folders[i] = g.folders[i + 1];
            g.folder_count--;
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

    /* Build favourites folder at index 0 */
    ScriptFolder *fav = &g.folders[0];
    memset(fav, 0, sizeof(*fav));
    wcscpy(fav->name,    L"Favourites");
    wcscpy(fav->display, L"\u2605 Favourites");
    fav->loaded = true;

    /* Copy favourited scripts in — skip hidden */
    for (int fi = 1; fi < g.folder_count && fav->count < MAX_SCRIPTS; fi++)
        for (int si = 0; si < g.folders[fi].count && fav->count < MAX_SCRIPTS; si++)
            if (g.folders[fi].scripts[si].is_favourite &&
                !g.folders[fi].scripts[si].is_hidden)
                fav->scripts[fav->count++] = g.folders[fi].scripts[si];
}

/* ================================================================== */
/*  ScriptDetailsDlgProc                                                */
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
            wcsncpy(s->note, note, MAX_NOTE_LEN - 1);
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
            _snwprintf(title, MAX_NAME + 19, L"Run: %s", s->name);
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
/*  Shows a list of all hidden scripts with an Unhide button.           */
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
