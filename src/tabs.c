/*
 * tabs.c  -  Tab control population, script button grid, scroll panel.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* live button handles for the current tab */
static HWND s_btns[MAX_SCRIPTS] = {0};
static int  s_btn_count = 0;

/* ================================================================== */
/*  Tabs_ApplyFilter                                                    */
/*  Purpose: Rebuilds the script button grid for the active tab,       */
/*           showing only scripts that match g.filter_text.  Delegates */
/*           to Tabs_RebuildButtons which calls Tabs_ScriptMatchesFilter*/
/*           per script.                                               */
/*  In:  (reads g.filter_text, g.active_tab)                           */
/*  Out: (void — recreates child button windows in g.hwnd_scroll)      */
/* ================================================================== */
void Tabs_ApplyFilter(void)
{
    /* Rebuild buttons showing only matching ones — no gaps */
    Tabs_RebuildButtons();
}

/* ================================================================== */
/*  Tabs_ScriptMatchesFilter                                            */
/*  Purpose: Returns true if the given script's name or purpose field  */
/*           contains g.filter_text (case-insensitive).  Returns true  */
/*           when the filter is empty (show all).                      */
/*  In:  s — pointer to the Script to test                             */
/*  Out: true if the script matches or filter is empty; false otherwise */
/* ================================================================== */
bool Tabs_ScriptMatchesFilter(const Script *s)
{
    if (g.filter_text[0] == L'\0') return true;

    WCHAR name_lower[MAX_NAME]   = {0};
    WCHAR purp_lower[128]        = {0};
    WCHAR filt_lower[MAX_NAME]   = {0};

    wcsncpy(name_lower, s->name,         MAX_NAME - 1);
    wcsncpy(purp_lower, s->meta.purpose, 127);
    wcsncpy(filt_lower, g.filter_text,   MAX_NAME - 1);

    for (WCHAR *p = name_lower; *p; p++) *p = towlower(*p);
    for (WCHAR *p = purp_lower; *p; p++) *p = towlower(*p);
    for (WCHAR *p = filt_lower; *p; p++) *p = towlower(*p);

    return wcsstr(name_lower, filt_lower) != NULL ||
           wcsstr(purp_lower, filt_lower) != NULL;
}

/* ================================================================== */
/*  cmp_alpha / cmp_date / cmp_runs  (static comparators)             */
/*  Purpose: qsort comparison functions used by Tabs_ApplySort.        */
/*  In:  a, b — pointers to Script elements being compared             */
/*  Out: negative / zero / positive per qsort convention               */
/* ================================================================== */
static int cmp_alpha(const void *a, const void *b) {
    return _wcsicmp(((Script*)a)->name, ((Script*)b)->name);
}
static int cmp_date(const void *a, const void *b) {
    return wcscmp(((Script*)b)->meta.date, ((Script*)a)->meta.date);
}
static int cmp_runs(const void *a, const void *b) {
    return ((Script*)b)->run_count - ((Script*)a)->run_count;
}

/* ================================================================== */
/*  Tabs_FolderHasVisible                                               */
/*  Purpose: Returns true if folder fi contains at least one script     */
/*           that is not hidden.  Used to decide whether a tab should  */
/*           be shown in the tab bar.                                  */
/*  In:  fi — folder index (bounds-checked)                             */
/*  Out: true if any non-hidden script exists; false otherwise          */
/* ================================================================== */
bool Tabs_FolderHasVisible(int fi)
{
    if (fi < 0 || fi >= g.folder_count) return false;
    ScriptFolder *f = &g.folders[fi];
    for (int si = 0; si < f->count; si++)
        if (!f->scripts[si].is_hidden) return true;
    return false;
}

/* ================================================================== */
/*  Tabs_ApplySort                                                      */
/*  Purpose: Sorts the scripts array of folder fi in-place according   */
/*           to the global sort mode (g.cfg.sort_mode).  SORT_ORDER   */
/*           is a no-op (GitHub API / disk order is preserved).        */
/*  In:  fi — folder index to sort (clamped via bounds check)          */
/*  Out: (void — f->scripts reordered in place)                        */
/* ================================================================== */
void Tabs_ApplySort(int fi)
{
    if (fi < 0 || fi >= g.folder_count) return;
    ScriptFolder *f = &g.folders[fi];
    switch (g.cfg.sort_mode) {
    case SORT_ALPHA:     qsort(f->scripts, f->count, sizeof(Script), cmp_alpha); break;
    case SORT_DATE:      qsort(f->scripts, f->count, sizeof(Script), cmp_date);  break;
    case SORT_MOST_USED: qsort(f->scripts, f->count, sizeof(Script), cmp_runs);  break;
    default: break;
    }
}

/* ================================================================== */
/*  Tabs_Build                                                          */
/*  Purpose: Signals the custom tab bar to repaint itself from the     */
/*           current g.folders[] array.  The bar reads folder names   */
/*           directly; no explicit rebuild is required.                */
/*  In:  (reads g.folders[], g.folder_count via TabBarProc on repaint) */
/*  Out: (void — invalidates the tab bar window)                       */
/* ================================================================== */
void Tabs_Build(void)
{
    /* Custom tab bar reads directly from g.folders[] - just repaint */
    InvalidateRect(g.hwnd_tab, NULL, FALSE);
}

/* ================================================================== */
/*  Tabs_DestroyButtons                                                 */
/*  Purpose: Destroys all script button child windows in the scroll    */
/*           panel and resets the button count and scroll state.       */
/*  In:  (reads s_btns[], s_btn_count)                                 */
/*  Out: (void — s_btn_count = 0, g.scroll_y = g.scroll_max = 0)      */
/* ================================================================== */
void Tabs_DestroyButtons(void)
{
    for (int i = 0; i < s_btn_count; i++) {
        if (IsWindow(s_btns[i])) DestroyWindow(s_btns[i]);
        s_btns[i] = NULL;
    }
    s_btn_count    = 0;
    g.scroll_y     = 0;
    g.scroll_max   = 0;
}

/* ================================================================== */
/*  Tabs_Switch                                                         */
/*  Purpose: Activates the tab at index idx, repaints the tab bar to   */
/*           show the new selection, and rebuilds the script button    */
/*           grid for the newly active folder.                         */
/*  In:  idx — folder index to make active (validated before use)      */
/*  Out: (void — sets g.active_tab and g.active_folder_name,            */
/*               triggers repaint and button rebuild)                   */
/* ================================================================== */
void Tabs_Switch(int idx)
{
    if (idx < 0 || idx >= g.folder_count) return;
    g.active_tab = idx;
    wcsncpy(g.active_folder_name, g.folders[idx].name, MAX_NAME - 1);
    InvalidateRect(g.hwnd_tab, NULL, FALSE); /* repaint custom tab bar */
    Tabs_RebuildButtons();
}

/* ================================================================== */
/*  Tabs_RebuildButtons                                                 */
/*  Purpose: Destroys existing script buttons and recreates one owner- */
/*           draw button per visible, non-hidden, filter-matching      */
/*           script in the active folder.  Also recalculates scroll    */
/*           bar range based on total button height.                   */
/*  In:  (reads g.active_tab, g.folders[], g.filter_text, g.syncing)  */
/*  Out: (void — repopulates s_btns[], updates scroll info)            */
/* ================================================================== */
void Tabs_RebuildButtons(void)
{
    SendMessage(g.hwnd_scroll, WM_SETREDRAW, FALSE, 0);
    Tabs_DestroyButtons();

    int fi = g.active_tab;
    if (fi < 0 || fi >= g.folder_count) goto done;

    ScriptFolder *f = &g.folders[fi];

    RECT rc;
    GetClientRect(g.hwnd_scroll, &rc);
    int panel_w = rc.right;
    int btn_w   = panel_w - BTN_MX * 2;
    if (btn_w < 80) btn_w = 80;

    HINSTANCE hInst =
        (HINSTANCE)GetWindowLongPtr(g.hwnd, GWLP_HINSTANCE);

    int y = BTN_MY;

    if (g.syncing && f->count == 0) {
        /* Placeholder while syncing */
        s_btn_count = 0;
        goto update_scroll;
    }

    for (int i = 0; i < f->count; i++) {
        if (f->scripts[i].is_hidden) continue;  /* skip hidden */
        if (!Tabs_ScriptMatchesFilter(&f->scripts[i])) continue; /* skip filtered */
        int id = IDC_SCRIPT_BTN_BASE + i;

        HWND hb = CreateWindow(
            L"BUTTON", f->scripts[i].name,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            BTN_MX, y, btn_w, BTN_H,
            g.hwnd_scroll, (HMENU)(UINT_PTR)id, hInst, NULL);

        SetWindowLongPtr(hb, GWLP_USERDATA, (LONG_PTR)i);
        SetWindowSubclass(hb, BtnSubclassProc, (UINT_PTR)id, (DWORD_PTR)0);
        SendMessage(hb, WM_SETFONT, (WPARAM)g.font_bold, FALSE);

        s_btns[s_btn_count++] = hb;
        y += BTN_H + BTN_GAP;
    }

update_scroll:;
    int total_h = y + BTN_MY;
    GetClientRect(g.hwnd_scroll, &rc);
    int visible    = rc.bottom - rc.top;
    g.scroll_max   = (total_h > visible) ? (total_h - visible) : 0;

    SCROLLINFO si = {
        .cbSize = sizeof(si),
        .fMask  = SIF_RANGE | SIF_PAGE | SIF_POS,
        .nMin   = 0,
        .nMax   = total_h,
        .nPage  = (UINT)visible,
        .nPos   = 0
    };
    SetScrollInfo(g.hwnd_scroll, SB_VERT, &si, TRUE);

done:
    SendMessage(g.hwnd_scroll, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g.hwnd_scroll, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

    /* If active tab has no visible scripts (not a filter effect), switch
       to the first tab that does so the empty tab doesn't stay selected. */
    if (!g.filter_text[0] && !Tabs_FolderHasVisible(g.active_tab)) {
        for (int _fi = 0; _fi < g.folder_count; _fi++) {
            if (Tabs_FolderHasVisible(_fi)) {
                g.active_tab = _fi;
                wcsncpy(g.active_folder_name, g.folders[_fi].name, MAX_NAME - 1);
                InvalidateRect(g.hwnd_tab, NULL, FALSE);
                Tabs_RebuildButtons();
                return;
            }
        }
    }
}

/* ================================================================== */
/*  ScrollPanelProc                                                     */
/*  Purpose: Window procedure for the CMW32ScrollPanel control which   */
/*           hosts all script buttons.  Handles background painting,   */
/*           owner-draw dispatch, vertical scrolling (scroll bar and   */
/*           mouse wheel), button click forwarding to the main window, */
/*           and mouse-leave highlight clearing.                       */
/*  In:  hwnd — scroll panel window handle                             */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (scroll direction, mouse coords, etc.)          */
/*       lp   — LPARAM (DRAWITEMSTRUCT pointer for WM_DRAWITEM)        */
/*  Out: LRESULT — TRUE for WM_DRAWITEM; 0 for handled; DefWindowProc  */
/* ================================================================== */
LRESULT CALLBACK ScrollPanelProc(HWND hwnd, UINT msg,
                                  WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        /* Forward button clicks to the main window */
        SendMessage(g.hwnd, WM_COMMAND, wp, lp);
        return 0;

    case WM_MOUSEMOVE:
        /* Track when mouse leaves the scroll panel so all highlights clear */
        {
            TRACKMOUSEEVENT tme = {
                .cbSize    = sizeof(tme),
                .dwFlags   = TME_LEAVE,
                .hwndTrack = hwnd
            };
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        /* Mouse left the scroll panel entirely - clear any stuck highlight */
        if (g.hot_btn >= IDC_SCRIPT_BTN_BASE) {
            HWND hPrev = GetDlgItem(hwnd, g.hot_btn);
            g.hot_btn = -1;
            if (hPrev) InvalidateRect(hPrev, NULL, FALSE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH _bg = CreateSolidBrush(COL_BG()); FillRect(hdc, &rc, _bg); DeleteObject(_bg);

        /* "Syncing…" placeholder text — only when no buttons are present */
        if (g.syncing && !GetWindow(hwnd, GW_CHILD)) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, COL_SUBTEXT());
            HFONT of = SelectObject(hdc, g.font_ui);
            DrawText(hdc, L"Checking for updates\u2026", -1, &rc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    /* Owner-draw buttons */
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlType != ODT_BUTTON) break;

        int fi  = g.active_tab;
        int idx = (int)GetWindowLongPtr(dis->hwndItem, GWLP_USERDATA);

        const Script *s = (fi >= 0 && fi < g.folder_count &&
                           idx >= 0 && idx < g.folders[fi].count)
                          ? &g.folders[fi].scripts[idx] : NULL;

        bool hot      = (g.hot_btn == (int)(UINT_PTR)dis->CtlID);
        bool pressed  = (dis->itemState & ODS_SELECTED) != 0;
        bool info_hot = (g.tip_btn  == (int)(UINT_PTR)dis->CtlID);
        Paint_ScriptButton(dis->hwndItem, dis->hDC, hot, pressed, info_hot, s);
        return TRUE;
    }

    /* Scroll bar */
    case WM_VSCROLL:
    {
        SCROLLINFO si = { .cbSize = sizeof(si), .fMask = SIF_ALL };
        GetScrollInfo(hwnd, SB_VERT, &si);
        int pos = si.nPos;

        switch (LOWORD(wp)) {
        case SB_LINEUP:        pos -= SCROLL_STEP;                   break;
        case SB_LINEDOWN:      pos += SCROLL_STEP;                   break;
        case SB_PAGEUP:        pos -= (int)si.nPage;                 break;
        case SB_PAGEDOWN:      pos += (int)si.nPage;                 break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = HIWORD(wp);                     break;
        case SB_TOP:           pos = si.nMin;                        break;
        case SB_BOTTOM:        pos = si.nMax;                        break;
        }

        int max_pos = si.nMax - (int)si.nPage + 1;
        if (pos < si.nMin) pos = si.nMin;
        if (pos > max_pos) pos = max_pos;

        if (pos != si.nPos) {
            int delta = si.nPos - pos;
            si.nPos   = pos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            ScrollWindowEx(hwnd, 0, delta, NULL, NULL, NULL, NULL,
                           SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
            g.scroll_y = pos;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        SendMessage(hwnd, WM_VSCROLL,
                    delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        SendMessage(hwnd, WM_VSCROLL,
                    delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        return 0;
    }

    case WM_SIZE:
        Tabs_RebuildButtons();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}
