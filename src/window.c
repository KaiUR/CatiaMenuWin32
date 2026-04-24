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
/*  Forces tab control and status bar to repaint with new colours      */
/* ================================================================== */
void Window_ApplyThemeToChildren(HWND hwnd)
{
    /* Remove visual styles from tab control so WM_CTLCOLOR* applies */
    if (g.dark_mode) {
        SetWindowTheme(g.hwnd_tab,    L"", L"");
        SetWindowTheme(g.hwnd_status, L"", L"");
    } else {
        SetWindowTheme(g.hwnd_tab,    NULL, NULL);
        SetWindowTheme(g.hwnd_status, NULL, NULL);
    }
    InvalidateRect(g.hwnd_tab,    NULL, TRUE);
    InvalidateRect(g.hwnd_status, NULL, TRUE);
    InvalidateRect(hwnd, NULL, TRUE);
    /* Redraw toolbar buttons */
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

/* ================================================================== */
/*  Window_ApplyAlwaysOnTop                                             */
/* ================================================================== */
void Window_ApplyAlwaysOnTop(void)
{
    /* SWP_NOACTIVATE keeps focus where it is.
       Call twice - first sets the Z-order flag, second forces it to take. */
    HWND z = g.cfg.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g.hwnd, z, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd, z, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
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
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g.hwnd;
    nid.uID    = TRAY_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    g.tray_icon_added = false;
}

void Window_ShowTrayMenu(void)
{
    HMENU hm = CreatePopupMenu();
    AppendMenu(hm, MF_STRING, IDM_REFRESH, L"Refresh Scripts");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING, IDM_EXIT,    L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g.hwnd);
    TrackPopupMenu(hm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.hwnd, NULL);
    DestroyMenu(hm);
}

/* ================================================================== */
/*  Tooltip window proc                                                 */
/* ================================================================== */
static LRESULT CALLBACK TipWndProcInternal(HWND hwnd, UINT msg,
                                            WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:   Paint_Tooltip(hwnd); return 0;
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

    /* Toolbar buttons */
    CreateWindow(L"BUTTON", L"\u27F3  Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        6, 5, 108, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2699  Settings",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        120, 5, 108, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_SETTINGS, hInst, NULL);

    CreateWindow(L"BUTTON", L"\u2B07  Update Deps",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        234, 5, 120, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_UPDATE_DEPS, hInst, NULL);

    SendDlgItemMessage(g.hwnd, IDC_BTN_REFRESH,     WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    SendDlgItemMessage(g.hwnd, IDC_BTN_SETTINGS,    WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    SendDlgItemMessage(g.hwnd, IDC_BTN_UPDATE_DEPS, WM_SETFONT, (WPARAM)g.font_ui, TRUE);

    /* Apply dark theme to toolbar buttons */
    if (g.dark_mode) {
        SetWindowTheme(GetDlgItem(g.hwnd, IDC_BTN_REFRESH),     L"", L"");
        SetWindowTheme(GetDlgItem(g.hwnd, IDC_BTN_SETTINGS),    L"", L"");
        SetWindowTheme(GetDlgItem(g.hwnd, IDC_BTN_UPDATE_DEPS), L"", L"");
    }

    /* Tab control */
    g.hwnd_tab = CreateWindow(WC_TABCONTROL, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_HOTTRACK,
        0, TOOLBAR_H, ww, TAB_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_TAB_CTRL, hInst, NULL);
    SendMessage(g.hwnd_tab, WM_SETFONT, (WPARAM)g.font_ui, TRUE);
    if (g.dark_mode) SetWindowTheme(g.hwnd_tab, L"", L"");

    /* Scroll panel */
    int ct = TOOLBAR_H + TAB_H;
    int ch = wh - ct - STATUS_H;
    g.hwnd_scroll = CreateWindowEx(
        0, L"CMW32ScrollPanel", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
        0, ct, ww, ch,
        g.hwnd, (HMENU)(UINT_PTR)IDC_SCROLL_PANEL, hInst, NULL);

    /* Status bar */
    g.hwnd_status = CreateWindow(STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        g.hwnd, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
    SendMessage(g.hwnd_status, WM_SETFONT, (WPARAM)g.font_small, TRUE);
    if (g.dark_mode) SetWindowTheme(g.hwnd_status, L"", L"");
    SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                (LPARAM)L"Checking for updates\u2026");

    /* Tooltip popup (hidden) */
    g.hwnd_tip = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"CMW32Tip", NULL, WS_POPUP,
        0, 0, 300, 160,
        g.hwnd, NULL, hInst, NULL);

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
    SendMessage(g.hwnd_status, WM_SIZE, 0, 0);
    RECT sr; GetWindowRect(g.hwnd_status, &sr);
    int sh = sr.bottom - sr.top;

    int ct = TOOLBAR_H + TAB_H;
    int ch = h - ct - sh;
    if (ch < 0) ch = 0;

    SetWindowPos(g.hwnd_tab, NULL, 0, TOOLBAR_H, w, TAB_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_scroll, NULL, 0, ct, w, ch,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    Tabs_RebuildButtons();
    InvalidateRect(g.hwnd, NULL, FALSE);
}
