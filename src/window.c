/*
 * window.c  -  Window creation, layout, dark mode, tray icon.
 * CatiaMenuWin32
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
    if (g.dark_mode) {
        SetWindowTheme(g.hwnd_tab,    L"", L"");
        SetWindowTheme(g.hwnd_status, L"", L"");
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_REFRESH),     L"", L"");
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_SETTINGS),    L"", L"");
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_UPDATE_DEPS), L"", L"");
    } else {
        SetWindowTheme(g.hwnd_tab,    NULL, NULL);
        SetWindowTheme(g.hwnd_status, NULL, NULL);
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_REFRESH),     NULL, NULL);
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_SETTINGS),    NULL, NULL);
        SetWindowTheme(GetDlgItem(hwnd, IDC_BTN_UPDATE_DEPS), NULL, NULL);
    }
    InvalidateRect(g.hwnd_tab,    NULL, TRUE);
    InvalidateRect(g.hwnd_status, NULL, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

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
/*  Dark menu bar via owner-draw                                        */
/* ================================================================== */
void Window_ApplyDarkMenu(HWND hwnd)
{
    HMENU hm = GetMenu(hwnd);
    if (!hm) return;

    /* Set menu background colour only.
       Do NOT use MFT_OWNERDRAW - it breaks text rendering.
       On Windows 11 with DwmSetWindowAttribute dark mode, Windows
       automatically renders menu text in the correct colour.
       On Windows 10, we accept the system menu text colour. */
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask  = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    if (g.dark_mode) {
        /* Use toolbar colour for menu background */
        mi.hbrBack = CreateSolidBrush(COL_TOOLBAR());
    } else {
        mi.hbrBack = (HBRUSH)(COLOR_MENU + 1);
    }
    SetMenuInfo(hm, &mi);

    /* Remove any leftover MFT_OWNERDRAW flags from previous calls */
    int count = GetMenuItemCount(hm);
    for (int i = 0; i < count; i++) {
        MENUITEMINFO mii = {0};
        mii.cbSize = sizeof(mii);
        mii.fMask  = MIIM_FTYPE;
        GetMenuItemInfo(hm, i, TRUE, &mii);
        mii.fType &= ~MFT_OWNERDRAW;
        SetMenuItemInfo(hm, i, TRUE, &mii);
    }
    DrawMenuBar(hwnd);
}

/* ================================================================== */
/*  Tray icon                                                           */
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
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g.hwnd);
    TrackPopupMenu(hm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.hwnd, NULL);
    DestroyMenu(hm);
}

/* ================================================================== */
/*  Custom status bar WndProc  (owner-drawn, no native STATUSCLASSNAME)*/
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

        /* Background */
        HBRUSH bg = CreateSolidBrush(COL_TOOLBAR());
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        /* Top divider line */
        HPEN pen = CreatePen(PS_SOLID, 1, COL_DIVIDER());
        HPEN op  = SelectObject(hdc, pen);
        MoveToEx(hdc, 0, 0, NULL);
        LineTo(hdc, rc.right, 0);
        SelectObject(hdc, op); DeleteObject(pen);

        /* Text */
        WCHAR text[256] = {0};
        GetWindowText(hwnd, text, 255);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_SUBTEXT());
        HFONT of = SelectObject(hdc, g.font_small);
        RECT tr = { 6, 0, rc.right - 6, rc.bottom };
        DrawText(hdc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        EndPaint(hwnd, &ps);
        return 0;
    }

    /* Intercept SB_SETTEXT to store and repaint */
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
/*  Tooltip internal proc                                               */
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

    /* Register status bar class */
    WNDCLASSEX wcsb = {
        .cbSize        = sizeof(wcsb),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = StatusBarProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName = L"CMW32StatusBar",
    };
    RegisterClassEx(&wcsb);

    /* Main class */
    WNDCLASSEX wc = {
        .cbSize        = sizeof(wc),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = MainWndProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszMenuName  = MAKEINTRESOURCE(IDR_MAINMENU),
        .lpszClassName = APP_CLASS,
        .hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP_ICON)),
        .hIconSm       = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP_ICON)),
    };
    RegisterClassEx(&wc);

    /* Scroll panel class */
    WNDCLASSEX wcs = {
        .cbSize        = sizeof(wcs),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = ScrollPanelProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName = L"CMW32ScrollPanel",
    };
    RegisterClassEx(&wcs);

    /* Tooltip class */
    WNDCLASSEX wct = {
        .cbSize        = sizeof(wct),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = TipWndProcInternal,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName = L"CMW32Tip",
    };
    RegisterClassEx(&wct);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 700, wh = 540;

    g.hwnd = CreateWindowEx(
        0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW,
        (sw - ww) / 2, (sh - wh) / 2, ww, wh,
        NULL, NULL, hInst, NULL);

    Window_ApplyDarkMode(g.hwnd);

    /* Toolbar buttons - BS_OWNERDRAW so WM_DRAWITEM paints them */
    CreateWindow(L"BUTTON", L"\u27F3  Refresh",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        6, 5, 108, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2699  Settings",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        120, 5, 108, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_SETTINGS, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2B07  Update Deps",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        234, 5, 120, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_UPDATE_DEPS, hInst, NULL);

    SendDlgItemMessage(g.hwnd, IDC_BTN_REFRESH,     WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    SendDlgItemMessage(g.hwnd, IDC_BTN_SETTINGS,    WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    SendDlgItemMessage(g.hwnd, IDC_BTN_UPDATE_DEPS, WM_SETFONT, (WPARAM)g.font_ui, TRUE);

    /* Tab control - TCS_OWNERDRAWFIXED allows us to paint tabs in dark mode */
    g.hwnd_tab = CreateWindow(WC_TABCONTROL, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_HOTTRACK | TCS_OWNERDRAWFIXED,
        0, TOOLBAR_H, ww, TAB_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_TAB_CTRL, hInst, NULL);
    SendMessage(g.hwnd_tab, WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    SetWindowTheme(g.hwnd_tab, L"", L"");

    /* Scroll panel */
    int ct = TOOLBAR_H + TAB_H;
    int ch = wh - ct - STATUS_H;
    g.hwnd_scroll = CreateWindowEx(
        0, L"CMW32ScrollPanel", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
        0, ct, ww, ch,
        g.hwnd, (HMENU)(UINT_PTR)IDC_SCROLL_PANEL, hInst, NULL);

    /* Custom status bar (owner-drawn, no native STATUSCLASSNAME) */
    g.hwnd_status = CreateWindow(
        L"CMW32StatusBar", L"Checking for updates\u2026",
        WS_CHILD | WS_VISIBLE,
        0, wh - STATUS_H, ww, STATUS_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
    SendMessage(g.hwnd_status, WM_SETFONT, (WPARAM)g.font_small, TRUE);

    /* Tooltip popup */
    g.hwnd_tip = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"CMW32Tip", NULL, WS_POPUP,
        0, 0, 300, 160,
        g.hwnd, NULL, hInst, NULL);

    /* Apply dark menu */
    Window_ApplyDarkMenu(g.hwnd);

    /* Set initial menu checkmarks */
    HMENU hm = GetMenu(g.hwnd);
    if (hm) {
        CheckMenuItem(hm, IDM_ALWAYS_ON_TOP,
            g.cfg.always_on_top      ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_MINIMIZE_TO_TRAY,
            g.cfg.minimize_to_tray   ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_START_WITH_WINDOWS,
            g.cfg.start_with_windows ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_START_MINIMIZED,
            g.cfg.start_minimized    ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_THEME_DARK,
            g.cfg.theme == THEME_DARK   ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_THEME_LIGHT,
            g.cfg.theme == THEME_LIGHT  ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hm, IDM_THEME_SYSTEM,
            g.cfg.theme == THEME_SYSTEM ? MF_CHECKED : MF_UNCHECKED);
    }
}

/* ================================================================== */
/*  Window_OnSize                                                       */
/* ================================================================== */
void Window_OnSize(int w, int h)
{
    int ct = TOOLBAR_H + TAB_H;
    int ch = h - ct - STATUS_H;
    if (ch < 0) ch = 0;

    SetWindowPos(g.hwnd_tab, NULL, 0, TOOLBAR_H, w, TAB_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_scroll, NULL, 0, ct, w, ch,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_status, NULL, 0, h - STATUS_H, w, STATUS_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    Tabs_RebuildButtons();
    InvalidateRect(g.hwnd, NULL, FALSE);
}
