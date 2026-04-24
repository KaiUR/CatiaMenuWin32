/*
 * paint.c  -  All GDI rendering. Double-buffered throughout.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

static void DrawRoundRect(HDC hdc, int x, int y, int w, int h, int r,
                           COLORREF fill, COLORREF border)
{
    HBRUSH br  = CreateSolidBrush(fill);
    HPEN   pen = (border == fill) ? (HPEN)GetStockObject(NULL_PEN)
                                  : CreatePen(PS_SOLID, 1, border);
    HBRUSH obr = SelectObject(hdc, br);
    HPEN   ope = SelectObject(hdc, pen);
    RoundRect(hdc, x, y, x + w, y + h, r, r);
    SelectObject(hdc, obr); SelectObject(hdc, ope);
    DeleteObject(br);
    if (border != fill) DeleteObject(pen);
}

/* ================================================================== */
/*  Paint_MainWindow                                                    */
/* ================================================================== */
void Paint_MainWindow(HWND hwnd, HDC hdc_paint)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    HDC     mem = CreateCompatibleDC(hdc_paint);
    HBITMAP bmp = CreateCompatibleBitmap(hdc_paint, w, h);
    HBITMAP old = SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(COL_BG());
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    RECT tb = { 0, 0, w, TOOLBAR_H };
    HBRUSH tb_br = CreateSolidBrush(COL_TOOLBAR());
    FillRect(mem, &tb, tb_br);
    DeleteObject(tb_br);

    HPEN dp = CreatePen(PS_SOLID, 1, COL_DIVIDER());
    HPEN op = SelectObject(mem, dp);
    MoveToEx(mem, 0, TOOLBAR_H - 1, NULL);
    LineTo(mem, w, TOOLBAR_H - 1);
    SelectObject(mem, op); DeleteObject(dp);

    SetBkMode(mem, TRANSPARENT);
    HFONT of = SelectObject(mem, g.font_bold);

    /* Build title string with version */
    WCHAR title[128];
    _snwprintf(title, 127, L"%s   v%s", APP_TITLE, VERSION_STRING_W);

    if (g.latest_version[0]) {
        /* Two-line layout: title top half, update badge bottom half */
        SetTextColor(mem, COL_ACCENT);
        SelectObject(mem, g.font_bold);
        RECT tr = { 370, 0, w - 8, TOOLBAR_H / 2 };
        DrawText(mem, title, -1, &tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        WCHAR upd[64];
        _snwprintf(upd, 63, L"\u2B06 Update %s available", g.latest_version);
        SelectObject(mem, g.font_small);
        SetTextColor(mem, COL_WARN);
        RECT ur = { 370, TOOLBAR_H / 2, w - 8, TOOLBAR_H - 2 };
        DrawText(mem, upd, -1, &ur, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else if (g.syncing) {
        SetTextColor(mem, COL_ACCENT);
        RECT tr = { 370, 0, w - 8, TOOLBAR_H / 2 };
        DrawText(mem, title, -1, &tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mem, g.font_small);
        SetTextColor(mem, COL_WARN);
        RECT sr = { 370, TOOLBAR_H / 2, w - 8, TOOLBAR_H - 2 };
        DrawText(mem, L"\u25CF Syncing\u2026", -1, &sr,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(mem, COL_ACCENT);
        RECT tr = { 370, 0, w - 8, TOOLBAR_H };
        DrawText(mem, title, -1, &tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(mem, of);
    BitBlt(hdc_paint, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}

/* ================================================================== */
/*  Paint_ToolbarButton  -  owner-draw for Refresh/Settings/UpdateDeps */
/* ================================================================== */
void Paint_ToolbarButton(DRAWITEMSTRUCT *dis)
{
    HDC  hdc = dis->hDC;
    RECT rc  = dis->rcItem;
    int  w   = rc.right  - rc.left;
    int  h   = rc.bottom - rc.top;

    HDC     mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP old = SelectObject(mem, bmp);

    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot     = (dis->itemState & ODS_HOTLIGHT)  != 0;
    bool focused = (dis->itemState & ODS_FOCUS)      != 0;

    COLORREF bg  = pressed ? COL_BTN_PRESS()
                 : hot     ? COL_BTN_HOT()
                 :            COL_BTN_NORM();
    COLORREF bdr = (hot || focused) ? COL_ACCENT : COL_DIVIDER();

    DrawRoundRect(mem, 0, 0, w, h, 6, bg, bdr);

    /* Button label */
    WCHAR text[64] = {0};
    GetWindowText(dis->hwndItem, text, 63);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, hot || pressed ? COL_ACCENT : COL_TEXT());
    HFONT of = SelectObject(mem, g.font_ui);
    RECT tr = { 0, 0, w, h };
    DrawText(mem, text, -1, &tr,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, of);
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}

/* ================================================================== */
/*  Paint_ScriptButton                                                  */
/* ================================================================== */
void Paint_ScriptButton(HWND hwnd_btn, HDC hdc,
                         bool hot, bool pressed, bool info_hot,
                         const Script *s)
{
    RECT rc; GetClientRect(hwnd_btn, &rc);
    int w = rc.right, h = rc.bottom;
    int main_w = w - INFO_BTN_W;

    HDC     mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP old = SelectObject(mem, bmp);

    COLORREF bg  = pressed ? COL_BTN_PRESS() : hot ? COL_BTN_HOT() : COL_BTN_NORM();
    COLORREF bdr = hot ? COL_ACCENT : COL_DIVIDER();
    DrawRoundRect(mem, 0,      0, main_w,     h, 7, bg, bdr);
    DrawRoundRect(mem, main_w, 0, INFO_BTN_W, h, 7,
                  info_hot ? COL_ACCENT_DIM : COL_INFO_ZONE(), bdr);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, info_hot ? COL_TEXT() : COL_SUBTEXT());
    HFONT of = SelectObject(mem, g.font_bold);
    RECT ir = { main_w, 0, w, h };
    DrawText(mem, L"i", -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (hot || pressed) {
        HBRUSH ab = CreateSolidBrush(pressed ? COL_ACCENT_DIM : COL_ACCENT);
        RECT   ar = { 0, 5, 4, h - 5 };
        FillRect(mem, &ar, ab);
        DeleteObject(ab);
    }

    SetTextColor(mem, hot ? COL_ACCENT : COL_SUBTEXT());
    SelectObject(mem, g.font_ui);
    RECT arr = { 8, 0, 28, h };
    DrawText(mem, L"\u25BA", -1, &arr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const WCHAR *label   = s ? s->name : L"";
    const WCHAR *purpose = (s && s->meta_loaded && s->meta.purpose[0])
                           ? s->meta.purpose : NULL;

    SetTextColor(mem, hot ? COL_ACCENT : COL_TEXT());
    SelectObject(mem, g.font_bold);
    if (purpose) {
        RECT lr = { 30, 3, main_w - 6, h / 2 + 2 };
        DrawText(mem, label, -1, &lr,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(mem, COL_SUBTEXT());
        SelectObject(mem, g.font_small);
        RECT pr = { 30, h / 2, main_w - 6, h - 4 };
        DrawText(mem, purpose, -1, &pr,
                 DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        RECT lr = { 30, 0, main_w - 6, h };
        DrawText(mem, label, -1, &lr,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(mem, of);
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}

/* ================================================================== */
/*  Tip_ComputeHeight  -  measure description word-wrap height         */
/* ================================================================== */
static int Tip_ComputeHeight(const Script *s)
{
    /* Fixed rows: Script + Purpose + Author + Version + Date */
    int h = 8 + TIP_HEADER_ROWS * TIP_ROW_H;

    if (s && s->meta.description[0]) {
        h += 4; /* separator */
        /* Measure wrapped description text height */
        HDC hdc = GetDC(g.hwnd_tip);
        HFONT of = SelectObject(hdc, g.font_small);
        RECT dr = { 8, 0, TIP_W - 12, 0 };
        int text_h = DrawText(hdc, s->meta.description, -1, &dr,
                              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
        SelectObject(hdc, of);
        ReleaseDC(g.hwnd_tip, hdc);
        h += text_h + 8;
    }
    h += 8; /* bottom padding */
    return h;
}

/* ================================================================== */
/*  Paint_Tooltip                                                       */
/* ================================================================== */
void Paint_Tooltip(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP old = SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(COL_TIP_BG());
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    HPEN bp = CreatePen(PS_SOLID, 1, COL_TIP_BORDER);
    HPEN op = SelectObject(mem, bp);
    HBRUSH nb = SelectObject(mem, GetStockObject(NULL_BRUSH));
    Rectangle(mem, 0, 0, w, h);
    SelectObject(mem, op); SelectObject(mem, nb);
    DeleteObject(bp);

    int fi = g.active_tab;
    int si = g.tip_btn - IDC_SCRIPT_BTN_BASE;
    if (fi < 0 || fi >= g.folder_count ||
        si < 0 || si >= g.folders[fi].count) goto done;

    {
        Script *s = &g.folders[fi].scripts[si];

        struct { const WCHAR *lbl; const WCHAR *val; } rows[] = {
            { L"Script:",  s->name },
            { L"Purpose:", s->meta.purpose[0]  ? s->meta.purpose  : L"--" },
            { L"Author:",  s->meta.author[0]   ? s->meta.author   : L"--" },
            { L"Version:", s->meta.version[0]  ? s->meta.version  : L"--" },
            { L"Date:",    s->meta.date[0]     ? s->meta.date     : L"--" },
            { NULL, NULL }
        };

        SetBkMode(mem, TRANSPARENT);
        int y = 8;
        for (int i = 0; rows[i].lbl; i++) {
            RECT lr = { 8, y, 76, y + 16 };
            RECT vr = { 80, y, w - 6, y + 16 };
            SelectObject(mem, g.font_bold);
            SetTextColor(mem, COL_ACCENT);
            DrawText(mem, rows[i].lbl, -1, &lr, DT_LEFT | DT_TOP | DT_SINGLELINE);
            SelectObject(mem, g.font_ui);
            SetTextColor(mem, COL_TEXT());
            DrawText(mem, rows[i].val, -1, &vr,
                     DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += TIP_ROW_H;
        }

        if (s->meta.description[0]) {
            HPEN sep = CreatePen(PS_SOLID, 1, COL_DIVIDER());
            HPEN os  = SelectObject(mem, sep);
            MoveToEx(mem, 8, y, NULL); LineTo(mem, w - 8, y);
            SelectObject(mem, os); DeleteObject(sep);
            y += 5;
            /* Full word-wrapped description - no ellipsis, no clipping */
            RECT dr = { 8, y, w - 6, h - 6 };
            SelectObject(mem, g.font_small);
            SetTextColor(mem, COL_SUBTEXT());
            DrawText(mem, s->meta.description, -1, &dr,
                     DT_LEFT | DT_TOP | DT_WORDBREAK);
        }
    }

done:
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* ================================================================== */
/*  TipWndProc                                                          */
/* ================================================================== */
LRESULT CALLBACK TipWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:      Paint_Tooltip(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  BtnSubclassProc  -  hover + info zone + resize tooltip             */
/* ================================================================== */
LRESULT CALLBACK BtnSubclassProc(HWND hwnd, UINT msg, WPARAM wp,
                                   LPARAM lp, UINT_PTR uid, DWORD_PTR data)
{
    (void)data;
    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        if (g.hot_btn != (int)uid && g.hot_btn >= IDC_SCRIPT_BTN_BASE) {
            HWND hp = GetDlgItem(GetParent(hwnd), g.hot_btn);
            if (hp) InvalidateRect(hp, NULL, FALSE);
        }
        g.hot_btn = (int)uid;
        TRACKMOUSEEVENT tme = { .cbSize=sizeof(tme), .dwFlags=TME_LEAVE,
                                .hwndTrack=hwnd };
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, NULL, FALSE);

        RECT rc; GetClientRect(hwnd, &rc);
        int mx = GET_X_LPARAM(lp);
        bool over_info = (mx >= rc.right - INFO_BTN_W);

        if (over_info && g.tip_btn != (int)uid) {
            g.tip_btn = (int)uid;
            int fi = g.active_tab;
            int si = (int)uid - IDC_SCRIPT_BTN_BASE;
            if (fi >= 0 && fi < g.folder_count &&
                si >= 0 && si < g.folders[fi].count)
                Meta_Parse(&g.folders[fi].scripts[si]);

            /* Compute tooltip height based on description length */
            Script *s = NULL;
            if (fi >= 0 && fi < g.folder_count &&
                si >= 0 && si < g.folders[fi].count)
                s = &g.folders[fi].scripts[si];

            int th = Tip_ComputeHeight(s);
            g.tip_h = th;

            /* Position tooltip */
            POINT pt = { rc.right, 0 };
            ClientToScreen(hwnd, &pt);
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int tx = (pt.x + TIP_W > sw) ? pt.x - rc.right - TIP_W : pt.x;
            int ty = pt.y;
            /* Keep on screen vertically */
            if (ty + th > sh) ty = sh - th - 4;

            SetWindowPos(g.hwnd_tip, HWND_TOPMOST, tx, ty, TIP_W, th,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(g.hwnd_tip, NULL, TRUE);

        } else if (!over_info && g.tip_btn == (int)uid) {
            g.tip_btn = -1;
            ShowWindow(g.hwnd_tip, SW_HIDE);
        }
        break;
    }

    case WM_MOUSELEAVE:
        if (g.hot_btn == (int)uid) { g.hot_btn = -1; InvalidateRect(hwnd, NULL, FALSE); }
        if (g.tip_btn == (int)uid) { g.tip_btn = -1; ShowWindow(g.hwnd_tip, SW_HIDE); }
        break;

    case WM_LBUTTONUP:
    {
        RECT rc; GetClientRect(hwnd, &rc);
        if (GET_X_LPARAM(lp) >= rc.right - INFO_BTN_W) return 0;
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, BtnSubclassProc, uid);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Paint_Tab  -  owner-draw for tab control items                     */
/* ================================================================== */
void Paint_Tab(DRAWITEMSTRUCT *dis)
{
    HDC  hdc = dis->hDC;
    RECT rc  = dis->rcItem;

    bool selected = (dis->itemState & ODS_SELECTED) != 0;

    /* Background */
    COLORREF bg  = selected ? COL_BTN_NORM() : COL_TOOLBAR();
    COLORREF bdr = selected ? COL_ACCENT     : COL_DIVIDER();

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    /* Bottom border: selected tab has no bottom line (merges with content) */
    HPEN pen = CreatePen(PS_SOLID, 1, bdr);
    HPEN op  = SelectObject(hdc, pen);
    if (!selected) {
        MoveToEx(hdc, rc.left,  rc.bottom - 1, NULL);
        LineTo(hdc,   rc.right, rc.bottom - 1);
    }
    /* Top accent line for selected tab */
    if (selected) {
        HPEN ap = CreatePen(PS_SOLID, 2, COL_ACCENT);
        SelectObject(hdc, ap);
        MoveToEx(hdc, rc.left,  rc.top, NULL);
        LineTo(hdc,   rc.right, rc.top);
        SelectObject(hdc, op);
        DeleteObject(ap);
        op = SelectObject(hdc, pen);
    }
    SelectObject(hdc, op);
    DeleteObject(pen);

    /* Tab label text */
    WCHAR text[MAX_NAME] = {0};
    TCITEM ti = { .mask = TCIF_TEXT, .pszText = text, .cchTextMax = MAX_NAME - 1 };
    TabCtrl_GetItem(g.hwnd_tab, dis->itemID, &ti);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, selected ? COL_ACCENT : COL_TEXT());
    HFONT of = SelectObject(hdc, selected ? g.font_bold : g.font_ui);
    DrawText(hdc, text, -1, &rc,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, of);
}
