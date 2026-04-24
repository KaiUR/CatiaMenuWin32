/*
 * tabs.c  -  Tab control population, script button grid, scroll panel.
 * CatiaMenuWin32
 */

#include "main.h"

/* live button handles for the current tab */
static HWND s_btns[MAX_SCRIPTS] = {0};
static int  s_btn_count = 0;

/* ================================================================== */
/*  Tabs_Build  -  rebuild tab bar from g.folders[]                    */
/* ================================================================== */
void Tabs_Build(void)
{
    TabCtrl_DeleteAllItems(g.hwnd_tab);

    for (int i = 0; i < g.folder_count; i++) {
        TCITEM ti = { .mask = TCIF_TEXT, .pszText = g.folders[i].display };
        TabCtrl_InsertItem(g.hwnd_tab, i, &ti);
    }
}

/* ================================================================== */
/*  Tabs_DestroyButtons                                                 */
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
/* ================================================================== */
void Tabs_Switch(int idx)
{
    if (idx < 0 || idx >= g.folder_count) return;
    g.active_tab = idx;
    TabCtrl_SetCurSel(g.hwnd_tab, idx);
    Tabs_RebuildButtons();
}

/* ================================================================== */
/*  Tabs_RebuildButtons  -  create one owner-draw button per script    */
/* ================================================================== */
void Tabs_RebuildButtons(void)
{
    Tabs_DestroyButtons();

    int fi = g.active_tab;
    if (fi < 0 || fi >= g.folder_count) return;

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
    InvalidateRect(g.hwnd_scroll, NULL, TRUE);
}

/* ================================================================== */
/*  ScrollPanelProc                                                     */
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

        /* "Syncing…" placeholder text */
        if (g.syncing) {
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
