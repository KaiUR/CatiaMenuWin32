/*
 * window.c  -  Window creation, layout, dark mode, tray icon.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  Window_ApplyDarkMode                                                */
/* ================================================================== */
void Window_ApplyDarkMode(HWND hwnd)
{
    BOOL dark = g.dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
}

/* ================================================================== */
/*  Window_ApplyThemeToChildren                                        */
/* ================================================================== */
void Window_ApplyThemeToChildren(HWND hwnd)
{
    SetWindowTheme(g.hwnd_status, L"", L"");
    InvalidateRect(g.hwnd_tab,    NULL, TRUE); /* custom tab bar repaints */
    InvalidateRect(g.hwnd_status, NULL, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

/* ================================================================== */
/*  Window_ApplyDarkMenu  -  no-op, menu is now a toolbar button      */
/* ================================================================== */
void Window_ApplyDarkMenu(HWND hwnd) { (void)hwnd; }

/* ================================================================== */
/*  Window_ApplyAlwaysOnTop                                             */
/* ================================================================== */
void Window_ApplyAlwaysOnTop(void)
{
    HWND z = g.cfg.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g.hwnd, z, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd, z, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
}

/* ================================================================== */
/*  DarkenMenu  -  apply dark background brush to a menu recursively   */
/* ================================================================== */
static void DarkenMenu(HMENU hm)
{
    if (!g.dark_mode || !hm) return;
    MENUINFO mi = {0};
    mi.cbSize  = sizeof(mi);
    mi.fMask   = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    mi.hbrBack = CreateSolidBrush(COL_BTN_NORM());
    SetMenuInfo(hm, &mi);
}

/* ================================================================== */
/*  Window_ShowMenu  -  builds and shows popup at the menu button      */
/* ================================================================== */
void Window_ShowMenu(void)
{
    /* Build the full popup menu programmatically */
    HMENU hm  = CreatePopupMenu();
    HMENU hFile = CreatePopupMenu();
    HMENU hRun  = CreatePopupMenu();
    HMENU hView = CreatePopupMenu();
    HMENU hTheme= CreatePopupMenu();
    HMENU hWin  = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();

    /* File */
    AppendMenu(hFile, MF_STRING, IDM_REFRESH,  L"Refresh + Sync\tF5");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, IDM_EXIT,     L"Exit");

    /* Run */
    AppendMenu(hRun, MF_STRING, IDM_RUN_LAST,     L"Run Last Script\tF9");
    AppendMenu(hRun, MF_SEPARATOR, 0, NULL);
    AppendMenu(hRun, MF_STRING, IDM_OPEN_CACHE,   L"Open Cache Folder...");
    AppendMenu(hRun, MF_STRING, IDM_UPDATE_DEPS,  L"Update Dependencies");

    /* Theme submenu */
    AppendMenu(hTheme, MF_STRING, IDM_THEME_DARK,   L"Dark");
    AppendMenu(hTheme, MF_STRING, IDM_THEME_LIGHT,  L"Light");
    AppendMenu(hTheme, MF_STRING, IDM_THEME_SYSTEM, L"System (default)");

    /* View */
    AppendMenu(hView, MF_STRING, IDM_ALWAYS_ON_TOP, L"Always on Top");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    AppendMenu(hView, MF_POPUP, (UINT_PTR)hTheme,   L"Theme");

    /* Window */
    AppendMenu(hWin, MF_STRING, IDM_MINIMIZE_TO_TRAY,   L"Minimize to Tray");
    AppendMenu(hWin, MF_STRING, IDM_START_WITH_WINDOWS, L"Start with Windows");
    AppendMenu(hWin, MF_STRING, IDM_START_MINIMIZED,    L"Start Minimized");

    /* Help */
    AppendMenu(hHelp, MF_STRING, IDM_ABOUT,  L"About");
    AppendMenu(hHelp, MF_STRING, IDM_GITHUB, L"View on GitHub");

    /* Top-level */
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hFile, L"File");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hRun,  L"Run");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hView, L"View");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hWin,  L"Window");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    /* Apply dark background to all menus */
    DarkenMenu(hm);
    DarkenMenu(hFile); DarkenMenu(hRun);  DarkenMenu(hView);
    DarkenMenu(hTheme); DarkenMenu(hWin); DarkenMenu(hHelp);

    /* Apply checkmarks */
    CheckMenuItem(hView, IDM_ALWAYS_ON_TOP,
        g.cfg.always_on_top ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hWin, IDM_MINIMIZE_TO_TRAY,
        g.cfg.minimize_to_tray ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hWin, IDM_START_WITH_WINDOWS,
        g.cfg.start_with_windows ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hWin, IDM_START_MINIMIZED,
        g.cfg.start_minimized ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hTheme, IDM_THEME_DARK,
        g.cfg.theme == THEME_DARK   ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hTheme, IDM_THEME_LIGHT,
        g.cfg.theme == THEME_LIGHT  ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hTheme, IDM_THEME_SYSTEM,
        g.cfg.theme == THEME_SYSTEM ? MF_CHECKED : MF_UNCHECKED);

    /* Show below the menu button */
    HWND hBtn = GetDlgItem(g.hwnd, IDC_BTN_MENU);
    RECT rc; GetWindowRect(hBtn, &rc);
    TrackPopupMenu(hm, TPM_LEFTALIGN | TPM_TOPALIGN,
                   rc.left, rc.bottom, 0, g.hwnd, NULL);

    DestroyMenu(hm); /* destroys submenus too */
}

/* ================================================================== */
/*  Tray                                                                */
/* ================================================================== */
void Window_AddTrayIcon(void)
{
    if (g.tray_icon_added) return;
    NOTIFYICONDATA nid = {0};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = g.hwnd;
    nid.uID              = TRAY_ID;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = LoadIcon(GetModuleHandle(NULL),
                                    MAKEINTRESOURCE(IDI_APP_ICON));
    wcscpy(nid.szTip, APP_TITLE);
    Shell_NotifyIcon(NIM_ADD, &nid);
    g.tray_icon_added = true;
}

void Window_RemoveTrayIcon(void)
{
    if (!g.tray_icon_added) return;
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(nid); nid.hWnd = g.hwnd; nid.uID = TRAY_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    g.tray_icon_added = false;
}

void Window_ShowTrayMenu(void)
{
    HMENU hm = CreatePopupMenu();
    AppendMenu(hm, MF_STRING, IDM_REFRESH, L"Refresh Scripts");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING, IDM_EXIT, L"Exit");
    DarkenMenu(hm);
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g.hwnd);
    TrackPopupMenu(hm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.hwnd, NULL);
    DestroyMenu(hm);
}

/* ================================================================== */
/*  CMW32TabBar  -  fully custom tab bar, replaces native tab control  */
/* ================================================================== */
static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        int n = g.folder_count;

        /* Background */
        HBRUSH bg = CreateSolidBrush(COL_TOOLBAR());
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        if (n == 0) { EndPaint(hwnd, &ps); return 0; }

        int tab_w = (n > 0) ? w / n : w;
        SetBkMode(hdc, TRANSPARENT);

        for (int i = 0; i < n; i++) {
            int x = i * tab_w;
            int tw = (i == n - 1) ? (w - x) : tab_w; /* last tab fills remainder */
            bool sel = (i == g.active_tab);

            /* Tab background */
            COLORREF bg_col = sel ? COL_BTN_NORM() : COL_TOOLBAR();
            HBRUSH tbr = CreateSolidBrush(bg_col);
            RECT tr = { x, 0, x + tw, h };
            FillRect(hdc, &tr, tbr);
            DeleteObject(tbr);

            /* Top accent line for selected tab */
            if (sel) {
                HPEN ap = CreatePen(PS_SOLID, 2, COL_ACCENT);
                HPEN op = SelectObject(hdc, ap);
                MoveToEx(hdc, x, 1, NULL);
                LineTo(hdc, x + tw, 1);
                SelectObject(hdc, op);
                DeleteObject(ap);
            }

            /* Divider between tabs */
            if (i < n - 1) {
                HPEN dp = CreatePen(PS_SOLID, 1, COL_DIVIDER());
                HPEN op = SelectObject(hdc, dp);
                MoveToEx(hdc, x + tw - 1, 3, NULL);
                LineTo(hdc, x + tw - 1, h - 3);
                SelectObject(hdc, op);
                DeleteObject(dp);
            }

            /* Tab label */
            SetTextColor(hdc, sel ? COL_ACCENT : COL_TEXT());
            HFONT of = SelectObject(hdc, sel ? g.font_bold : g.font_ui);
            RECT lr = { x + 4, 0, x + tw - 4, h };
            DrawText(hdc, g.folders[i].display, -1, &lr,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, of);
        }

        /* Bottom border line */
        HPEN bp = CreatePen(PS_SOLID, 1, COL_DIVIDER());
        HPEN op = SelectObject(hdc, bp);
        MoveToEx(hdc, 0, h - 1, NULL);
        LineTo(hdc, w, h - 1);
        SelectObject(hdc, op);
        DeleteObject(bp);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        RECT rc; GetClientRect(hwnd, &rc);
        int n = g.folder_count;
        if (n == 0) break;
        int x = GET_X_LPARAM(lp);
        int tab_w = rc.right / n;
        int idx = x / tab_w;
        if (idx >= n) idx = n - 1;
        if (idx != g.active_tab)
            Tabs_Switch(idx);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Custom status bar                                                   */
/* ================================================================== */
static LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HBRUSH bg = CreateSolidBrush(COL_TOOLBAR());
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        HPEN pen = CreatePen(PS_SOLID, 1, COL_DIVIDER());
        HPEN op  = SelectObject(hdc, pen);
        MoveToEx(hdc, 0, 0, NULL);
        LineTo(hdc, rc.right, 0);
        SelectObject(hdc, op); DeleteObject(pen);

        WCHAR text[256] = {0};
        GetWindowText(hwnd, text, 255);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_SUBTEXT());
        HFONT of = SelectObject(hdc, g.font_small);
        RECT tr = { 6, 0, rc.right - 6, rc.bottom };
        DrawText(hdc, text, -1, &tr,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case SB_SETTEXT:
    {
        const WCHAR *txt = (const WCHAR *)lp;
        if (txt) SetWindowText(hwnd, txt);
        InvalidateRect(hwnd, NULL, FALSE);
        return TRUE;
    }
    case SB_GETTEXT:
        return GetWindowText(hwnd, (WCHAR *)lp, (int)wp);

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Tooltip proc                                                        */
/* ================================================================== */
static LRESULT CALLBACK TipWndProcInternal(HWND hwnd, UINT msg,
                                            WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:      Paint_Tooltip(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Window_Create                                                       */
/* ================================================================== */
void Window_Create(HINSTANCE hInst)
{
    App_InitGDI();

    /* Register custom classes */
    WNDCLASSEX wcsb = { .cbSize=sizeof(wcsb), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=StatusBarProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName=L"CMW32StatusBar" };
    RegisterClassEx(&wcsb);

    WNDCLASSEX wcs = { .cbSize=sizeof(wcs), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=ScrollPanelProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName=L"CMW32ScrollPanel" };
    RegisterClassEx(&wcs);

    WNDCLASSEX wct = { .cbSize=sizeof(wct), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=TipWndProcInternal, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName=L"CMW32Tip" };
    RegisterClassEx(&wct);

    /* Main window class - NO lpszMenuName so there is no native menu bar */
    WNDCLASSEX wc = { .cbSize=sizeof(wc), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=MainWndProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszMenuName=NULL,   /* <-- no native menu bar */
        .lpszClassName=APP_CLASS,
        .hIcon=LoadIcon(hInst,MAKEINTRESOURCE(IDI_APP_ICON)),
        .hIconSm=LoadIcon(hInst,MAKEINTRESOURCE(IDI_APP_ICON)) };
    RegisterClassEx(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 700, wh = 540;

    g.hwnd = CreateWindowEx(
        0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW,
        (sw-ww)/2, (sh-wh)/2, ww, wh,
        NULL, NULL, hInst, NULL);

    Window_ApplyDarkMode(g.hwnd);

    /* ── Toolbar buttons ─────────────────────────────────────────── */
    /* Menu button (hamburger) */
    CreateWindow(L"BUTTON", L"\u2630  Menu",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        6, 5, 90, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_MENU, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u27F3  Refresh",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        102, 5, 100, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2699  Settings",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        208, 5, 100, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_SETTINGS, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2B07  Update Deps",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        314, 5, 120, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_UPDATE_DEPS, hInst, NULL);

    int ids[] = { IDC_BTN_MENU, IDC_BTN_REFRESH,
                  IDC_BTN_SETTINGS, IDC_BTN_UPDATE_DEPS };
    for (int i = 0; i < 4; i++)
        SendDlgItemMessage(g.hwnd, ids[i], WM_SETFONT,
                           (WPARAM)g.font_ui, TRUE);

    /* Register and create fully custom tab bar (no native WC_TABCONTROL) */
    WNDCLASSEX wctb = { .cbSize=sizeof(wctb), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=TabBarProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName=L"CMW32TabBar" };
    RegisterClassEx(&wctb);

    g.hwnd_tab = CreateWindow(L"CMW32TabBar", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, TOOLBAR_H, ww, TAB_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_TAB_CTRL, hInst, NULL);

    /* Scroll panel */
    int ct = TOOLBAR_H + TAB_H;
    int ch = wh - ct - STATUS_H;
    g.hwnd_scroll = CreateWindowEx(0, L"CMW32ScrollPanel", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
        0, ct, ww, ch,
        g.hwnd, (HMENU)(UINT_PTR)IDC_SCROLL_PANEL, hInst, NULL);

    /* Custom status bar */
    g.hwnd_status = CreateWindow(L"CMW32StatusBar",
        L"Checking for updates\u2026",
        WS_CHILD | WS_VISIBLE,
        0, wh - STATUS_H, ww, STATUS_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
    SendMessage(g.hwnd_status, WM_SETFONT, (WPARAM)g.font_small, TRUE);

    /* Tooltip popup */
    g.hwnd_tip = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"CMW32Tip", NULL, WS_POPUP,
        0, 0, 300, 160, g.hwnd, NULL, hInst, NULL);
}

/* ================================================================== */
/*  Window_OnSize                                                       */
/* ================================================================== */
void Window_OnSize(int w, int h)
{
    int ct = TOOLBAR_H + TAB_H;
    int ch = h - ct - STATUS_H;
    if (ch < 0) ch = 0;

    SetWindowPos(g.hwnd_tab,    NULL, 0, TOOLBAR_H, w, TAB_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_scroll, NULL, 0, ct, w, ch,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_status, NULL, 0, h - STATUS_H, w, STATUS_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    Tabs_RebuildButtons();
    InvalidateRect(g.hwnd, NULL, FALSE);
}
