/*
 * palette.c  -  Command palette: type to find any script, Enter to run it.
 * Opened with Ctrl+K inside the app, or an optional system-wide hotkey.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <wctype.h> /* towlower */

#define PAL_CLASS L"CMW32Palette"
#define PAL_W 560 /* palette width (px)                        */
#define PAL_EDIT_H 30 /* search box height (px)                    */
#define PAL_ROW_H 40 /* result row height (px)                    */
#define PAL_ROWS 9 /* result rows visible without scrolling     */
#define PAL_PAD 8 /* inner padding (px)                        */
#define PAL_FOOT_H 22 /* key-hint footer height (px)               */
#define PAL_MAX_RESULTS 300 /* results kept after sorting                */
#define IDC_PAL_EDIT 1 /* child control IDs (palette window only)   */
#define IDC_PAL_LIST 2

/* ================================================================== */
/*  PaletteItem                                                         */
/*  Purpose: One search result.  The script is identified by folder   */
/*           and GitHub path, re-resolved when chosen, because a sync */
/*           may rebuild g.folders[] while the palette is open.        */
/* ================================================================== */
typedef struct
{
    int fi, si; /* position when the list was built             */
    int score; /* match quality; higher is better                */
    int runs; /* run count, breaks ties                          */
    ScriptBadge badge;
    WCHAR name[MAX_NAME];
    WCHAR folder[MAX_NAME]; /* folder name (identity)                 */
    WCHAR tab[MAX_NAME]; /* folder display name (shown)            */
    WCHAR purpose[128];
    WCHAR gh_path[MAX_APPPATH];
} PaletteItem;

/* ================================================================== */
/*  PaletteState                                                        */
/*  Purpose: Per-window state, stored in the palette's GWLP_USERDATA.  */
/* ================================================================== */
typedef struct
{
    HWND edit, list;
    HBRUSH edit_brush; /* background of the search box (theme-aware) */
    HBRUSH list_brush; /* background of the result list below the last row */
    PaletteItem *items;
    int count;
} PaletteState;

/* ================================================================== */
/*  Palette_Score  (static)                                            */
/*  Purpose: Fuzzy-matches a query against a script name: every query  */
/*           character must appear in order.  Rewards matches at word */
/*           starts, consecutive runs, and a matching prefix;          */
/*           penalises gaps.  Spaces in the query are ignored.         */
/*  In:  text — script display name                                   */
/*       q    — query (non-empty)                                      */
/*  Out: score (higher is better), or -1 if not every character matched*/
/* ================================================================== */
static int Palette_Score(const WCHAR *text, const WCHAR *q)
{
    int score = 0, ti = 0, prev = -2;
    for (const WCHAR *qp = q; *qp; qp++)
    {
        WCHAR qc = towlower(*qp);
        if (qc == L' ') continue;
        int found = -1;
        for (int i = ti; text[i]; i++)
        {
            if (towlower(text[i]) == qc)
            {
                found = i;
                break;
            }
        }
        if (found < 0) return -1;
        score += 10;
        if (found == 0 || text[found - 1] == L' ' || text[found - 1] == L'_') score += 15; /* word start */
        if (found == prev + 1) score += 10; /* continues the previous match */
        int gap = found - ti;
        score -= (gap > 5) ? 5 : gap; /* skipped characters cost a little, capped */
        prev = found;
        ti = found + 1;
    }
    if (_wcsnicmp(text, q, wcslen(q)) == 0) score += 30; /* name starts with the query */
    return score;
}

/* ================================================================== */
/*  Palette_Compare  (static)                                          */
/*  Purpose: qsort order — best score first, then most run, then name. */
/* ================================================================== */
static int Palette_Compare(const void *a, const void *b)
{
    const PaletteItem *x = (const PaletteItem *)a, *y = (const PaletteItem *)b;
    if (x->score != y->score) return y->score - x->score;
    if (x->runs != y->runs) return y->runs - x->runs;
    return _wcsicmp(x->name, y->name);
}

/* ================================================================== */
/*  Palette_Refill  (static)                                           */
/*  Purpose: Rebuilds the result list for the current query: every     */
/*           visible script except the Favourites copies, matched on  */
/*           its name (fuzzy) or, failing that, its purpose            */
/*           (substring).  An empty query lists everything, most used */
/*           first.                                                    */
/*  In:  st — palette state                                             */
/*  Out: (void — st->items / st->count and the list box refreshed)     */
/* ================================================================== */
static void Palette_Refill(PaletteState *st)
{
    WCHAR q[MAX_NAME] = {0};
    GetWindowText(st->edit, q, MAX_NAME);

    free(st->items);
    st->items = NULL;
    st->count = 0;

    EnterCriticalSection(&g.cs_folders);
    int total = 0;
    for (int fi = 0; fi < g.folder_count; fi++)
        total += g.folders[fi].count;
    st->items = total ? (PaletteItem *)calloc((size_t)total, sizeof(PaletteItem)) : NULL;
    for (int fi = 0; st->items && fi < g.folder_count; fi++)
    {
        const ScriptFolder *f = &g.folders[fi];
        if (wcscmp(f->name, L"Favourites") == 0) continue; /* copies of scripts listed under their own tab */
        for (int si = 0; si < f->count; si++)
        {
            const Script *s = &f->scripts[si];
            if (s->is_hidden) continue;
            int score = 0;
            if (q[0])
            {
                score = Palette_Score(s->name, q);
                if (score < 0 && s->meta.purpose[0] && StrStrIW(s->meta.purpose, q))
                    score = 1; /* purpose match ranks below any name match */
                if (score < 0) continue;
            }
            PaletteItem *it = &st->items[st->count++];
            it->fi = fi;
            it->si = si;
            it->score = score;
            it->runs = s->run_count;
            it->badge = s->badge;
            wcsncpy_s(it->name, MAX_NAME, s->name, _TRUNCATE);
            wcsncpy_s(it->folder, MAX_NAME, f->name, _TRUNCATE);
            wcsncpy_s(it->tab, MAX_NAME, f->display, _TRUNCATE);
            wcsncpy_s(it->purpose, 128, s->meta.purpose, _TRUNCATE);
            wcsncpy_s(it->gh_path, MAX_APPPATH, s->gh_path, _TRUNCATE);
        }
    }
    LeaveCriticalSection(&g.cs_folders);

    if (st->count > 1) qsort(st->items, (size_t)st->count, sizeof(PaletteItem), Palette_Compare);
    if (st->count > PAL_MAX_RESULTS) st->count = PAL_MAX_RESULTS;

    SendMessage(st->list, WM_SETREDRAW, FALSE, 0);
    SendMessage(st->list, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < st->count; i++)
        SendMessage(st->list, LB_ADDSTRING, 0, (LPARAM)i); /* owner-drawn: the item data is the index */
    if (st->count) SendMessage(st->list, LB_SETCURSEL, 0, 0);
    SendMessage(st->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st->list, NULL, TRUE);
    InvalidateRect(GetParent(st->list), NULL, FALSE); /* footer shows "no match" state */
}

/* ================================================================== */
/*  Palette_Resolve  (static)                                          */
/*  Purpose: Finds a result's script in the current g.folders[]: at    */
/*           its recorded position if still there, else by folder and */
/*           GitHub path (a sync may have rebuilt the folders).        */
/*  In:  it — result                                                    */
/*       fi, si — receive the script position                          */
/*  Out: true if the script still exists                               */
/* ================================================================== */
static bool Palette_Resolve(const PaletteItem *it, int *fi, int *si)
{
    if (it->fi < g.folder_count && it->si < g.folders[it->fi].count &&
        wcscmp(g.folders[it->fi].scripts[it->si].gh_path, it->gh_path) == 0)
    {
        *fi = it->fi;
        *si = it->si;
        return true;
    }
    for (int f = 0; f < g.folder_count; f++)
    {
        if (wcscmp(g.folders[f].name, it->folder) != 0) continue;
        for (int s = 0; s < g.folders[f].count; s++)
        {
            if (wcscmp(g.folders[f].scripts[s].gh_path, it->gh_path) == 0)
            {
                *fi = f;
                *si = s;
                return true;
            }
        }
    }
    return false;
}

/* ================================================================== */
/*  Palette_Activate  (static)                                         */
/*  Purpose: Acts on the selected result and hides the palette.        */
/*  In:  hwnd — palette window                                         */
/*       st   — palette state                                          */
/*       mode — 0 = run, 1 = Run with Arguments, 2 = Script Details    */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Palette_Activate(HWND hwnd, PaletteState *st, int mode)
{
    int sel = (int)SendMessage(st->list, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= st->count) return;
    PaletteItem it = st->items[sel]; /* copy: the list may be rebuilt while a dialog is open */
    ShowWindow(hwnd, SW_HIDE);

    int fi = 0, si = 0;
    if (!Palette_Resolve(&it, &fi, &si))
    {
        PostStatus(L"%s is no longer available.", it.name);
        return;
    }
    if (mode == 0)
    {
        Runner_Run(fi, si);
    }
    else if (mode == 1)
    {
        RunArgsDlgData d = {.script = &g.folders[fi].scripts[si]};
        if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_RUN_ARGS),
                           g.hwnd, RunWithArgsDlgProc, (LPARAM)&d) == IDOK &&
            Palette_Resolve(&it, &fi, &si))
            Runner_RunWithArgs(fi, si, d.args);
    }
    else
    {
        if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SCRIPT_DETAILS),
                           g.hwnd, ScriptDetailsDlgProc, (LPARAM)&g.folders[fi].scripts[si]) == IDOK)
        {
            Tabs_BuildFavourites();
            Tabs_Build();
            Tabs_RebuildButtons();
        }
    }
}

/* ================================================================== */
/*  Palette_EditProc  (static)                                         */
/*  Purpose: Subclass of the search box: arrow and page keys move the  */
/*           selection in the result list, Enter activates it (Shift = */
/*           arguments, Ctrl = details), Escape closes the palette.    */
/* ================================================================== */
static LRESULT CALLBACK Palette_EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR id, DWORD_PTR ref)
{
    (void)id;
    HWND pal = (HWND)ref;
    PaletteState *st = (PaletteState *)GetWindowLongPtr(pal, GWLP_USERDATA);
    if (msg == WM_KEYDOWN && st)
    {
        int sel = (int)SendMessage(st->list, LB_GETCURSEL, 0, 0);
        int move = 0;
        switch (wp)
        {
        case VK_UP:
            move = -1;
            break;
        case VK_DOWN:
            move = 1;
            break;
        case VK_PRIOR:
            move = -PAL_ROWS;
            break;
        case VK_NEXT:
            move = PAL_ROWS;
            break;
        case VK_RETURN:
        {
            int mode = (GetKeyState(VK_CONTROL) & 0x8000) ? 2 : (GetKeyState(VK_SHIFT) & 0x8000) ? 1
                                                                                                 : 0;
            Palette_Activate(pal, st, mode);
            return 0;
        }
        case VK_ESCAPE:
            ShowWindow(pal, SW_HIDE);
            return 0;
        default:
            break;
        }
        if (move && st->count)
        {
            sel += move;
            if (sel < 0) sel = 0;
            if (sel >= st->count) sel = st->count - 1;
            SendMessage(st->list, LB_SETCURSEL, (WPARAM)sel, 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 0x1B)) return 0; /* swallow Enter/Escape: no beep */
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Palette_DrawItem  (static)                                         */
/*  Purpose: Draws one result row: name, tab and purpose, and the      */
/*           new/updated dot, in the current theme.                    */
/* ================================================================== */
static void Palette_DrawItem(const PaletteState *st, const DRAWITEMSTRUCT *dis)
{
    if (dis->itemID == (UINT)-1 || (int)dis->itemID >= st->count) return;
    const PaletteItem *it = &st->items[dis->itemID];
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bg = CreateSolidBrush(sel ? COL_BTN_HOT() : COL_BG());
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    if (sel)
    {
        HBRUSH ab = CreateSolidBrush(COL_ACCENT);
        RECT ar = {rc.left, rc.top + 5, rc.left + 4, rc.bottom - 5}; /* same 4 px accent bar as script buttons */
        FillRect(hdc, &ar, ab);
        DeleteObject(ab);
    }

    int right = rc.right - PAL_PAD;
    if (g.cfg.show_badges && it->badge != BADGE_NONE)
    {
        COLORREF dot = (it->badge == BADGE_NEW) ? COL_SUCCESS : COL_ACCENT;
        HBRUSH db = CreateSolidBrush(dot);
        HPEN dp = CreatePen(PS_SOLID, 1, dot);
        HBRUSH ob = SelectObject(hdc, db);
        HPEN op = SelectObject(hdc, dp);
        int cy = (rc.top + rc.bottom) / 2;
        Ellipse(hdc, right - 8, cy - 4, right, cy + 4);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(db);
        DeleteObject(dp);
        right -= 14;
    }

    SetBkMode(hdc, TRANSPARENT);
    HFONT of = SelectObject(hdc, g.font_bold);
    SetTextColor(hdc, sel ? COL_ACCENT : COL_TEXT());
    RECT nr = {rc.left + 12, rc.top + 3, right, rc.top + PAL_ROW_H / 2 + 2};
    DrawText(hdc, it->name, -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    WCHAR sub[MAX_NAME + 140];
    if (it->purpose[0])
        _snwprintf_s(sub, _countof(sub), _TRUNCATE, L"%s  \x00B7  %s", it->tab, it->purpose);
    else
        wcsncpy_s(sub, _countof(sub), it->tab, _TRUNCATE);
    SelectObject(hdc, g.font_small);
    SetTextColor(hdc, COL_SUBTEXT());
    RECT pr = {rc.left + 12, rc.top + PAL_ROW_H / 2, right, rc.bottom - 3};
    DrawText(hdc, sub, -1, &pr, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, of);
}

/* ================================================================== */
/*  Palette_WndProc  (static)                                          */
/*  Purpose: Window procedure for the palette popup.  Hides itself     */
/*           when it loses activation, so a click anywhere else        */
/*           dismisses it.                                              */
/* ================================================================== */
static LRESULT CALLBACK Palette_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PaletteState *st = (PaletteState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_CREATE:
    {
        st = (PaletteState *)calloc(1, sizeof(PaletteState));
        if (!st) return -1;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        HINSTANCE hi = GetModuleHandle(NULL);
        st->edit = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  PAL_PAD, PAL_PAD, PAL_W - 2 * PAL_PAD, PAL_EDIT_H,
                                  hwnd, (HMENU)(INT_PTR)IDC_PAL_EDIT, hi, NULL);
        st->list = CreateWindowEx(0, L"LISTBOX", NULL,
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED |
                                      LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                  PAL_PAD, PAL_PAD * 2 + PAL_EDIT_H, PAL_W - 2 * PAL_PAD, PAL_ROWS * PAL_ROW_H,
                                  hwnd, (HMENU)(INT_PTR)IDC_PAL_LIST, hi, NULL);
        SendMessage(st->edit, WM_SETFONT, (WPARAM)g.font_ui, TRUE);
        SendMessage(st->edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type to find a script\x2026");
        SetWindowSubclass(st->edit, Palette_EditProc, 1, (DWORD_PTR)hwnd);
        return 0;
    }

    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = PAL_ROW_H;
        return TRUE;

    case WM_DRAWITEM:
        if (st && wp == IDC_PAL_LIST) Palette_DrawItem(st, (DRAWITEMSTRUCT *)lp);
        return TRUE;

    case WM_CTLCOLOREDIT:
        if (!st) break;
        SetTextColor((HDC)wp, COL_TEXT());
        SetBkColor((HDC)wp, COL_BTN_NORM());
        return (LRESULT)st->edit_brush;

    case WM_CTLCOLORLISTBOX:
        if (!st) break;
        return (LRESULT)st->list_brush;

    case WM_COMMAND:
        if (!st) break;
        if (LOWORD(wp) == IDC_PAL_EDIT && HIWORD(wp) == EN_CHANGE)
            Palette_Refill(st);
        else if (LOWORD(wp) == IDC_PAL_LIST && HIWORD(wp) == LBN_DBLCLK)
            Palette_Activate(hwnd, st, 0);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) ShowWindow(hwnd, SW_HIDE); /* clicked elsewhere */
        return 0;

    case WM_ERASEBKGND:
        return 1; /* WM_PAINT fills everything */

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(COL_TOOLBAR());
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        HPEN bp = CreatePen(PS_SOLID, 1, COL_ACCENT);
        HPEN op = SelectObject(hdc, bp);
        HBRUSH nb = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);
        SelectObject(hdc, nb);
        SelectObject(hdc, op);
        DeleteObject(bp);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_SUBTEXT());
        HFONT of = SelectObject(hdc, g.font_small);
        RECT fr = {PAL_PAD, rc.bottom - PAL_FOOT_H, rc.right - PAL_PAD, rc.bottom - 2};
        const WCHAR *hint = (st && st->count == 0)
                                ? L"No matching scripts"
                                : L"Enter run  \x00B7  Shift+Enter run with arguments  \x00B7  Ctrl+Enter details  \x00B7  Esc close";
        DrawText(hdc, hint, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (st)
        {
            if (st->edit_brush) DeleteObject(st->edit_brush);
            if (st->list_brush) DeleteObject(st->list_brush);
            free(st->items);
            free(st);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        g.hwnd_palette = NULL;
        return 0;

    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Palette_Show                                                        */
/*  Purpose: Opens the command palette with an empty query (every      */
/*           script, most used first) and keyboard focus in the search */
/*           box.  Centred over the main window when it is on screen,  */
/*           otherwise on the monitor of the active window — so the    */
/*           system-wide hotkey opens it where the user is working.   */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Palette_Show(void)
{
    HINSTANCE hi = GetModuleHandle(NULL);
    if (!g.hwnd_palette)
    {
        WNDCLASSEX wc = {.cbSize = sizeof(wc),
                         .lpfnWndProc = Palette_WndProc,
                         .hInstance = hi,
                         .hCursor = LoadCursor(NULL, IDC_ARROW),
                         .lpszClassName = PAL_CLASS};
        RegisterClassEx(&wc); /* fails harmlessly if already registered */
        int h = PAL_PAD * 3 + PAL_EDIT_H + PAL_ROWS * PAL_ROW_H + PAL_FOOT_H;
        g.hwnd_palette = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, PAL_CLASS, L"Command Palette",
                                        WS_POPUP, 0, 0, PAL_W, h, g.hwnd, NULL, hi, NULL);
        if (!g.hwnd_palette) return;
    }
    PaletteState *st = (PaletteState *)GetWindowLongPtr(g.hwnd_palette, GWLP_USERDATA);
    if (!st) return;

    /* Theme may have changed since the last opening */
    if (st->edit_brush) DeleteObject(st->edit_brush);
    st->edit_brush = CreateSolidBrush(COL_BTN_NORM());
    if (st->list_brush) DeleteObject(st->list_brush);
    st->list_brush = CreateSolidBrush(COL_BG());
    SetWindowTheme(st->list, g.dark_mode ? L"DarkMode_Explorer" : NULL, NULL);

    /* Position: over the main window if it is visible, else on the active monitor */
    RECT pr;
    GetWindowRect(g.hwnd_palette, &pr);
    int pw = pr.right - pr.left, ph = pr.bottom - pr.top, x, y;
    if (IsWindowVisible(g.hwnd) && !IsIconic(g.hwnd))
    {
        RECT mr;
        GetWindowRect(g.hwnd, &mr);
        x = mr.left + ((mr.right - mr.left) - pw) / 2;
        y = mr.top + 60; /* just below the toolbar */
    }
    else
    {
        HWND fg = GetForegroundWindow();
        HMONITOR mon = MonitorFromWindow(fg ? fg : g.hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {.cbSize = sizeof(mi)};
        GetMonitorInfo(mon, &mi);
        x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - pw) / 2;
        y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - ph) / 4; /* upper quarter, like other launchers */
    }

    SetWindowText(st->edit, L"");
    Palette_Refill(st); /* EN_CHANGE only fires if the box was not already empty */
    SetWindowPos(g.hwnd_palette, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g.hwnd_palette);
    SetFocus(st->edit);
}

/* ================================================================== */
/*  Palette_HotkeyText                                                  */
/*  Purpose: Formats the configured system-wide hotkey for display,    */
/*           e.g. "Ctrl+K", or "(none)" when it is off.                */
/*  In:  buf — output buffer                                            */
/*       len — capacity in WCHARs                                       */
/*  Out: (void)                                                         */
/* ================================================================== */
void Palette_HotkeyText(WCHAR *buf, int len)
{
    if (!buf || len < 1) return;
    buf[0] = L'\0';
    if (!g.cfg.palette_hotkey_enabled || !g.cfg.palette_hotkey_mods)
    {
        wcsncpy_s(buf, len, L"(none)", _TRUNCATE);
        return;
    }
    if (g.cfg.palette_hotkey_mods & MOD_CONTROL) wcsncat_s(buf, len, L"Ctrl+", _TRUNCATE);
    if (g.cfg.palette_hotkey_mods & MOD_ALT) wcsncat_s(buf, len, L"Alt+", _TRUNCATE);
    if (g.cfg.palette_hotkey_mods & MOD_SHIFT) wcsncat_s(buf, len, L"Shift+", _TRUNCATE);
    if (g.cfg.palette_hotkey_mods & MOD_WIN) wcsncat_s(buf, len, L"Win+", _TRUNCATE);

    WCHAR key[8];
    UINT vk = g.cfg.palette_hotkey_vk;
    if (vk == VK_SPACE)
        wcsncpy_s(key, 8, L"Space", _TRUNCATE);
    else if (vk >= VK_F1 && vk <= VK_F12)
        _snwprintf_s(key, 8, _TRUNCATE, L"F%u", vk - VK_F1 + 1);
    else
    { /* letters and digits map straight onto their character */
        key[0] = (WCHAR)vk;
        key[1] = L'\0';
    }
    wcsncat_s(buf, len, key, _TRUNCATE);
}

/* ================================================================== */
/*  Palette_UnregisterHotkey                                            */
/*  Purpose: Releases the system-wide palette hotkey if registered.    */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Palette_UnregisterHotkey(void)
{
    if (!g.palette_hotkey_active) return;
    UnregisterHotKey(g.hwnd, HOTKEY_PALETTE);
    g.palette_hotkey_active = false;
}

/* ================================================================== */
/*  Palette_RegisterHotkey                                              */
/*  Purpose: Registers the system-wide palette hotkey on the main      */
/*           window when enabled (default Ctrl+K).  While registered  */
/*           the combination is taken away from every other            */
/*           application.  Always releases any previous registration   */
/*           first; reports a combination another application owns.   */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Palette_RegisterHotkey(void)
{
    Palette_UnregisterHotkey();
    if (!g.hwnd) return; /* main window not created yet */
    if (!g.cfg.palette_hotkey_enabled || !g.cfg.palette_hotkey_mods) return; /* feature off */

    if (RegisterHotKey(g.hwnd, HOTKEY_PALETTE, g.cfg.palette_hotkey_mods | MOD_NOREPEAT,
                       g.cfg.palette_hotkey_vk))
    {
        g.palette_hotkey_active = true;
        return;
    }
    WCHAR hk[64];
    Palette_HotkeyText(hk, 64);
    PostStatus(L"Command palette hotkey %s is already in use by another application.", hk);
}
