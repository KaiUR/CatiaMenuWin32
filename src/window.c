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
/*  Purpose: Applies the immersive dark-mode DWM attribute to a window */
/*           so that the title bar, borders, and system controls match  */
/*           the current theme.  Called on window creation and after   */
/*           theme changes.                                            */
/*  In:  hwnd — window to apply dark mode to                           */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_ApplyDarkMode(HWND hwnd)
{
    BOOL dark = g.dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark)); /* attribute 19 = pre-20H1 alias for DWMWA_USE_IMMERSIVE_DARK_MODE */
}

/* ================================================================== */
/*  Window_OpenExeFolder                                                */
/*  Purpose: Opens the folder containing the running executable in the */
/*           Windows Explorer shell.                                   */
/*  In:  (none — uses GetModuleFileNameW to locate the .exe)           */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_OpenExeFolder(void)
{
    WCHAR exe_path[MAX_APPPATH] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_APPPATH - 1);
    PathRemoveFileSpec(exe_path);
    ShellExecute(NULL, L"open", exe_path, NULL, NULL, SW_SHOW);
}

/* ================================================================== */
/*  Window_ApplyThemeToChildren                                         */
/*  Purpose: Propagates the current theme to all child controls by     */
/*           resetting the status bar's visual style and invalidating  */
/*           the tab bar, status bar, and the whole window hierarchy.  */
/*  In:  hwnd — parent window (main window)                            */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_ApplyThemeToChildren(HWND hwnd)
{
    SetWindowTheme(g.hwnd_status, L"", L"");
    InvalidateRect(g.hwnd_tab,    NULL, TRUE);
    InvalidateRect(g.hwnd_status, NULL, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
    Log_OnThemeChange(); /* update log window title bar and RichEdit colours */
}

/* ================================================================== */
/*  Window_ApplyDarkMenu                                                */
/*  Purpose: Placeholder for future dark-mode menu colouring.          */
/*           Win32 popup menus do not support owner-draw colouring on  */
/*           all Windows versions; reserved for future implementation. */
/*  In:  hwnd — (unused)                                               */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_ApplyDarkMenu(HWND hwnd) { (void)hwnd; }

/* ================================================================== */
/*  Window_ApplyAlwaysOnTop                                             */
/*  Purpose: Sets or clears the always-on-top (TOPMOST) window flag    */
/*           based on g.cfg.always_on_top.  Called after settings     */
/*           change and after the window becomes visible.              */
/*  In:  (reads g.cfg.always_on_top and g.hwnd)                        */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_ApplyAlwaysOnTop(void)
{
    HWND z = g.cfg.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g.hwnd, z, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

/* ================================================================== */
/*  Window_ShowMenu                                                     */
/*  Purpose: Builds and displays the full hamburger popup menu aligned  */
/*           below the Menu toolbar button.  Includes File, Run, View, */
/*           Window, and Help sub-menus with current checkmarks.       */
/*  In:  (reads g.cfg, g.hwnd for positioning)                         */
/*  Out: (void — menu is destroyed after TrackPopupMenu returns)       */
/* ================================================================== */
void Window_ShowMenu(void)
{
    HMENU hm     = CreatePopupMenu();
    HMENU hFile  = CreatePopupMenu();
    HMENU hRun   = CreatePopupMenu();
    HMENU hView  = CreatePopupMenu();
    HMENU hTheme = CreatePopupMenu();
    HMENU hWin   = CreatePopupMenu();
    HMENU hHelp  = CreatePopupMenu();

    AppendMenu(hFile, MF_STRING,    IDM_REFRESH,  L"Refresh + Sync\tF5");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING,    IDM_SETTINGS, L"Settings...");
    AppendMenu(hFile, MF_STRING,    IDM_SOURCES,          L"Sources...");
    AppendMenu(hFile, MF_STRING,    IDM_HIDDEN_SCRIPTS,   L"Manage Hidden Scripts...");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING,    IDM_OPEN_EXE_FOLDER,  L"Open Executable Folder");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING,    IDM_EXIT,     L"Exit");

    AppendMenu(hRun, MF_STRING,    IDM_RUN_LAST,    L"Run Last Script\tF9");
    AppendMenu(hRun, MF_SEPARATOR, 0, NULL);
    AppendMenu(hRun, MF_STRING,    IDM_OPEN_CACHE,  L"Open Cache Folder...");
    AppendMenu(hRun, MF_STRING,    IDM_UPDATE_DEPS, L"Update Dependencies");
    AppendMenu(hRun, MF_SEPARATOR, 0, NULL);
    AppendMenu(hRun, MF_STRING,    IDM_REPEAT_MAINAPP, L"Repeat Script on Double-Click");

    AppendMenu(hTheme, MF_STRING, IDM_THEME_DARK,   L"Dark");
    AppendMenu(hTheme, MF_STRING, IDM_THEME_LIGHT,  L"Light");
    AppendMenu(hTheme, MF_STRING, IDM_THEME_SYSTEM, L"System (default)");

    AppendMenu(hView, MF_STRING,    IDM_ALWAYS_ON_TOP, L"Always on Top");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    AppendMenu(hView, MF_POPUP, (UINT_PTR)hTheme, L"Theme");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    HMENU hSort = CreatePopupMenu();
    AppendMenu(hSort, MF_STRING, IDM_SORT_DEFAULT,   L"Default Order");
    AppendMenu(hSort, MF_STRING, IDM_SORT_ALPHA,     L"Alphabetical");
    AppendMenu(hSort, MF_STRING, IDM_SORT_DATE,      L"By Date");
    AppendMenu(hSort, MF_STRING, IDM_SORT_MOST_USED, L"Most Used");
    AppendMenu(hView, MF_POPUP, (UINT_PTR)hSort,     L"Sort Scripts");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    bool qbar_has_target = g.cfg.qbar_target_app[0] != L'\0';
    HMENU hQBar = CreatePopupMenu();
    AppendMenu(hQBar, MF_STRING,    IDM_QBAR_TOGGLE,     L"Enable Quick Bar");
    AppendMenu(hQBar, MF_SEPARATOR, 0, NULL);
    AppendMenu(hQBar, MF_STRING,    IDM_QBAR_HORIZONTAL, L"Horizontal");
    AppendMenu(hQBar, MF_STRING,    IDM_QBAR_VERTICAL,   L"Vertical");
    AppendMenu(hQBar, MF_SEPARATOR, 0, NULL);
    AppendMenu(hQBar, qbar_has_target ? MF_STRING : (MF_STRING | MF_GRAYED),
               IDM_QBAR_TOPMOST, L"On Top with Target App");
    AppendMenu(hQBar, MF_STRING,    IDM_QBAR_SET_TARGET, L"Set Target App...");
    AppendMenu(hQBar, MF_SEPARATOR, 0, NULL);
    AppendMenu(hQBar, MF_STRING,    IDM_QBAR_RESET_POS,  L"Reset Position");
    AppendMenu(hQBar, MF_SEPARATOR, 0, NULL);
    AppendMenu(hQBar, MF_STRING,    IDM_REPEAT_QBAR,     L"Repeat on Double-Click");
    AppendMenu(hView, MF_POPUP, (UINT_PTR)hQBar,         L"Quick Bar");

    AppendMenu(hWin, MF_STRING, IDM_MINIMIZE_TO_TRAY,   L"Minimize to Tray");
    AppendMenu(hWin, MF_STRING, IDM_START_WITH_WINDOWS, L"Start with Windows");
    AppendMenu(hWin, MF_STRING, IDM_START_MINIMIZED,    L"Start Minimized");

    AppendMenu(hHelp, MF_STRING, IDM_HELP_CONTENTS,  L"Help Contents\tF1");
    AppendMenu(hHelp, MF_STRING, IDM_CHECK_UPDATES,  L"Check for Updates...");
    AppendMenu(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenu(hHelp, MF_STRING, IDM_REPORT_BUG,     L"Report a Bug...");
    AppendMenu(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenu(hHelp, MF_STRING, IDM_ABOUT,          L"About");
    AppendMenu(hHelp, MF_STRING, IDM_GITHUB,         L"View App on GitHub");
    AppendMenu(hHelp, MF_STRING, IDM_GITHUB_PAGES,   L"GitHub Pages");
    AppendMenu(hHelp, MF_STRING, IDM_WIKI,           L"App Wiki");
    AppendMenu(hHelp, MF_STRING, IDM_GITHUB_SCRIPTS, L"View Scripts on GitHub");
    AppendMenu(hHelp, MF_STRING, IDM_WIKI_SCRIPTS,   L"Scripts Wiki");

    AppendMenu(hm, MF_POPUP, (UINT_PTR)hFile, L"File");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hRun,  L"Run");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hView, L"View");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hWin,  L"Window");
    AppendMenu(hm, MF_POPUP, (UINT_PTR)hHelp, L"Help");

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
    CheckMenuItem(hQBar, IDM_QBAR_TOGGLE,
        g.cfg.qbar_enabled              ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hQBar, IDM_QBAR_HORIZONTAL,
        g.cfg.qbar_horizontal           ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hQBar, IDM_QBAR_VERTICAL,
        !g.cfg.qbar_horizontal          ? MF_CHECKED : MF_UNCHECKED);
    if (qbar_has_target)
        CheckMenuItem(hQBar, IDM_QBAR_TOPMOST,
            g.cfg.qbar_topmost_with_catia ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hQBar, IDM_REPEAT_QBAR,
        g.cfg.qbar_repeat_on_dblclick  ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hRun,  IDM_REPEAT_MAINAPP,
        g.cfg.repeat_on_dblclick       ? MF_CHECKED : MF_UNCHECKED);

    HWND hBtn = GetDlgItem(g.hwnd, IDC_BTN_MENU);
    RECT rc; GetWindowRect(hBtn, &rc);

    TrackPopupMenu(hm, TPM_LEFTALIGN | TPM_TOPALIGN,
                   rc.left, rc.bottom, 0, g.hwnd, NULL);

    DestroyMenu(hm);
}

/* ================================================================== */
/*  Window_AddTrayIcon                                                  */
/*  Purpose: Adds the application icon to the Windows system tray      */
/*           notification area.  No-op if the icon is already present. */
/*  In:  (reads g.hwnd, g.tray_icon_added)                             */
/*  Out: (void — sets g.tray_icon_added = true on success)             */
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
    wcsncpy_s(nid.szTip, _countof(nid.szTip), APP_TITLE, _TRUNCATE);
    Shell_NotifyIcon(NIM_ADD, &nid);
    g.tray_icon_added = true;
}

/* ================================================================== */
/*  Window_RemoveTrayIcon                                               */
/*  Purpose: Removes the application icon from the system tray.        */
/*           No-op if the icon was not previously added.               */
/*  In:  (reads g.hwnd, g.tray_icon_added)                             */
/*  Out: (void — sets g.tray_icon_added = false)                       */
/* ================================================================== */
void Window_RemoveTrayIcon(void)
{
    if (!g.tray_icon_added) return;
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(nid); nid.hWnd = g.hwnd; nid.uID = TRAY_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    g.tray_icon_added = false;
}

/* ================================================================== */
/*  Window_ShowTrayMenu                                                 */
/*  Purpose: Displays a minimal right-click context menu at the cursor  */
/*           position when the user right-clicks the tray icon.        */
/*  In:  (reads g.hwnd for ownership)                                   */
/*  Out: (void — menu is destroyed after TrackPopupMenu returns)       */
/* ================================================================== */
void Window_ShowTrayMenu(void)
{
    HMENU hm = CreatePopupMenu();
    AppendMenu(hm, MF_STRING,    IDM_REFRESH, L"Refresh Scripts");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING,    IDM_EXIT,    L"Exit");

    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g.hwnd);
    TrackPopupMenu(hm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.hwnd, NULL);
    DestroyMenu(hm);
}

/* ── Tab bar helpers ─────────────────────────────────────────────── */

/* ================================================================== */
/*  TabBar_NaturalWidth  (static)                                      */
/*  Purpose: Measures the pixel width needed to display a tab label    */
/*           at the UI bold font width, plus 14 px padding each side. */
/*  In:  label — tab display string                                     */
/*  Out: pixel width required for the tab                              */
/* ================================================================== */
static int TabBar_NaturalWidth(const WCHAR *label)
{
    /* Use a temporary DC to measure text with the UI font */
    HDC hdc = GetDC(NULL);
    HFONT of = SelectObject(hdc, g.font_bold);
    SIZE sz = {0};
    GetTextExtentPoint32W(hdc, label, (int)wcslen(label), &sz);
    SelectObject(hdc, of);
    ReleaseDC(NULL, hdc);
    return sz.cx + 28; /* +28 = 14 px padding on each side */
}

/* ================================================================== */
/*  TabBar_CountFit  (static)                                          */
/*  Purpose: Counts how many tabs fit in the available bar width when  */
/*           starting from tab_offset, accounting for arrow buttons.  */
/*  In:  bar_w       — total pixel width of the tab bar               */
/*       offset      — first visible tab index                         */
/*       with_arrows — whether scroll arrows consume space             */
/*  Out: number of tabs that fit                                        */
/* ================================================================== */
/* offset is an index into vtabs[], not into g.folders[]. */
static int TabBar_CountFit(int bar_w, const int *vtabs, int vn,
                            int offset, bool with_arrows)
{
    int avail = with_arrows ? bar_w - 2 * TAB_ARROW_W : bar_w;
    int used  = 0;
    int count = 0;
    for (int v = offset; v < vn; v++) {
        int tw = TabBar_NaturalWidth(g.folders[vtabs[v]].display);
        if (used + tw > avail) break;
        used += tw;
        count++;
    }
    return count;
}

/* ================================================================== */
/*  TabBar_NeedsArrows  (static)                                       */
/*  Purpose: Determines whether the tab bar requires scroll arrow      */
/*           buttons by checking if all tabs fit without arrows.       */
/*  In:  bar_w — total pixel width of the tab bar                      */
/*  Out: true if not all tabs fit (arrows needed); false otherwise      */
/* ================================================================== */
static bool TabBar_NeedsArrows(int bar_w, const int *vtabs, int vn)
{
    if (vn <= 0) return false;
    int total = 0;
    for (int v = 0; v < vn; v++)
        total += TabBar_NaturalWidth(g.folders[vtabs[v]].display);
    return total > bar_w;
}

/* ================================================================== */
/*  TabBarProc  (static)                                               */
/*  Purpose: Window procedure for the custom CMW32TabBar control.      */
/*           Handles painting (tabs with accent underline, scroll      */
/*           arrows), left-click tab selection, arrow clicks, and      */
/*           mouse-wheel scrolling.                                    */
/*  In:  hwnd — tab bar window handle                                  */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (wheel delta, mouse coords, etc.)               */
/*       lp   — LPARAM (mouse coords, etc.)                            */
/*  Out: LRESULT — 0 for handled messages; DefWindowProc result        */
/* ================================================================== */
static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1; /* suppress default erase to prevent flicker */

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        HBRUSH bg = CreateSolidBrush(COL_TOOLBAR());
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        /* Build list of tabs that have at least one visible (non-hidden) script */
        int vtabs[MAX_FOLDERS];
        int vn = 0;
        EnterCriticalSection(&g.cs_folders);
        for (int i = 0; i < g.folder_count; i++)
            if (Tabs_FolderHasVisible(i)) vtabs[vn++] = i;
        LeaveCriticalSection(&g.cs_folders);

        if (vn == 0) { EndPaint(hwnd, &ps); return 0; }

        bool need_arrows = TabBar_NeedsArrows(w, vtabs, vn);
        int left_x = 0, right_x = w;

        if (need_arrows) {
            left_x  = TAB_ARROW_W;
            right_x = w - TAB_ARROW_W;

            /* Find the last offset from which at least one page of tabs is reachable */
            int max_off = 0;
            while (max_off < vn - 1 &&
                   max_off + TabBar_CountFit(w, vtabs, vn, max_off, true) < vn)
                max_off++;
            if (g.tab_offset > max_off) g.tab_offset = max_off; /* clamp to valid range */
            if (g.tab_offset < 0)       g.tab_offset = 0;

            bool la_hot = (g.tab_offset > 0); /* left arrow active only when not at the start */
            HBRUSH ab = CreateSolidBrush(la_hot ? COL_BTN_HOT() : COL_TOOLBAR());
            RECT ar = { 0, 0, TAB_ARROW_W, h };
            FillRect(hdc, &ar, ab); DeleteObject(ab);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, la_hot ? COL_ACCENT : COL_SUBTEXT());
            HFONT of2 = SelectObject(hdc, g.font_ui);
            DrawText(hdc, L"◄", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of2);

            int visible_count = TabBar_CountFit(w, vtabs, vn, g.tab_offset, true);
            bool ra_hot = (g.tab_offset + visible_count < vn); /* right arrow active when more tabs are hidden to the right */
            ab = CreateSolidBrush(ra_hot ? COL_BTN_HOT() : COL_TOOLBAR());
            ar = (RECT){ right_x, 0, w, h };
            FillRect(hdc, &ar, ab); DeleteObject(ab);
            SetTextColor(hdc, ra_hot ? COL_ACCENT : COL_SUBTEXT());
            HFONT of3 = SelectObject(hdc, g.font_ui);
            DrawText(hdc, L"►", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of3);
        } else {
            g.tab_offset = 0; /* all tabs fit — no offset needed */
        }

        SetBkMode(hdc, TRANSPARENT);

        int x = left_x;
        for (int v = g.tab_offset; v < vn; v++) {
            int fi = vtabs[v];
            int tw = TabBar_NaturalWidth(g.folders[fi].display);
            if (x + tw > right_x) break;
            bool sel = (fi == g.active_tab);

            HBRUSH tbr = CreateSolidBrush(sel ? COL_BTN_NORM() : COL_TOOLBAR());
            RECT tr = { x, 0, x + tw, h };
            FillRect(hdc, &tr, tbr);
            DeleteObject(tbr);

            if (sel) {
                /* 2-px accent line at the top of the selected tab */
                HPEN ap = CreatePen(PS_SOLID, 2, COL_ACCENT);
                HPEN op = SelectObject(hdc, ap);
                MoveToEx(hdc, x, 1, NULL);
                LineTo(hdc, x + tw, 1);
                SelectObject(hdc, op);
                DeleteObject(ap);
            }

            if (v + 1 < vn) { /* draw vertical divider between tabs, but not after the last visible one */
                HPEN dp = CreatePen(PS_SOLID, 1, COL_DIVIDER());
                HPEN op = SelectObject(hdc, dp);
                MoveToEx(hdc, x + tw - 1, 3, NULL);
                LineTo(hdc, x + tw - 1, h - 3);
                SelectObject(hdc, op);
                DeleteObject(dp);
            }

            SetTextColor(hdc, sel ? COL_ACCENT : COL_TEXT());
            HFONT of = SelectObject(hdc, sel ? g.font_bold : g.font_ui);
            RECT lr = { x + 14, 0, x + tw - 14, h }; /* 14 px text margin matches the 28-total-padding from NaturalWidth */
            DrawText(hdc, g.folders[fi].display, -1, &lr,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of);
            x += tw;
        }

        HPEN bp = CreatePen(PS_SOLID, 1, COL_DIVIDER());
        HPEN op = SelectObject(hdc, bp);
        MoveToEx(hdc, 0, h - 1, NULL); /* h - 1 = bottom edge of tab bar = horizontal divider */
        LineTo(hdc, w, h - 1);
        SelectObject(hdc, op);
        DeleteObject(bp);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right;
        if (g.folder_count == 0) break;

        int vtabs[MAX_FOLDERS];
        int vn = 0;
        for (int i = 0; i < g.folder_count; i++)
            if (Tabs_FolderHasVisible(i)) vtabs[vn++] = i;
        if (vn == 0) break;

        int mx = GET_X_LPARAM(lp);
        bool need_arrows = TabBar_NeedsArrows(w, vtabs, vn);

        if (need_arrows) {
            if (mx < TAB_ARROW_W) { /* click on left arrow — scroll left */
                if (g.tab_offset > 0) { g.tab_offset--; InvalidateRect(hwnd, NULL, FALSE); }
                return 0;
            }
            if (mx >= w - TAB_ARROW_W) { /* click on right arrow — scroll right */
                int vis = TabBar_CountFit(w, vtabs, vn, g.tab_offset, true);
                if (g.tab_offset + vis < vn) { g.tab_offset++; InvalidateRect(hwnd, NULL, FALSE); }
                return 0;
            }
        }

        int left_x = need_arrows ? TAB_ARROW_W : 0;
        int right_x = need_arrows ? w - TAB_ARROW_W : w;
        int x = left_x;
        for (int v = g.tab_offset; v < vn; v++) {
            int fi = vtabs[v];
            int tw = TabBar_NaturalWidth(g.folders[fi].display);
            if (x + tw > right_x) tw = right_x - x; /* clip last visible tab at the arrow zone boundary */
            if (mx >= x && mx < x + tw) {
                if (fi != g.active_tab) Tabs_Switch(fi); /* only switch if a different tab was clicked */
                return 0;
            }
            x += tw;
            if (x >= right_x) break;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        if (g.folder_count == 0) break;
        RECT rc; GetClientRect(hwnd, &rc);

        int vtabs[MAX_FOLDERS];
        int vn = 0;
        for (int i = 0; i < g.folder_count; i++)
            if (Tabs_FolderHasVisible(i)) vtabs[vn++] = i;
        if (!TabBar_NeedsArrows(rc.right, vtabs, vn)) break;

        int vis   = TabBar_CountFit(rc.right, vtabs, vn, g.tab_offset, true);
        int delta = GET_WHEEL_DELTA_WPARAM(wp); /* positive = wheel up = scroll left */
        if (delta < 0 && g.tab_offset + vis < vn) { /* wheel down = scroll right */
            g.tab_offset++; InvalidateRect(hwnd, NULL, FALSE);
        } else if (delta > 0 && g.tab_offset > 0) { /* wheel up = scroll left */
            g.tab_offset--; InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  StatusBarProc  (static)                                            */
/*  Purpose: Window procedure for the custom CMW32StatusBar control.   */
/*           Handles owner-draw painting (toolbar background, divider  */
/*           line, small grey text) and the SB_SETTEXT / SB_GETTEXT   */
/*           messages forwarded from the main window.                  */
/*  In:  hwnd — status bar window handle                               */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM                                                  */
/*       lp   — LPARAM (SB_SETTEXT: pointer to text string)            */
/*  Out: LRESULT — TRUE for SB_SETTEXT; text length for SB_GETTEXT;   */
/*                 0 for paint; DefWindowProc for unhandled messages   */
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
        SetTextColor(hdc, g.status_offline ? COL_WARN : COL_SUBTEXT()); /* amber text when offline */
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
/*  TipWndProcInternal  (static)                                       */
/*  Purpose: Minimal window procedure for the CMW32Tip tooltip popup.  */
/*           Delegates all painting to Paint_Tooltip and suppresses    */
/*           background erase to prevent flicker.                      */
/*  In:  hwnd — tooltip popup window handle                            */
/*       msg  — Windows message                                        */
/*       wp, lp — message parameters (unused for paint/erase)         */
/*  Out: LRESULT — 0 for WM_PAINT / WM_ERASEBKGND; DefWindowProc      */
/*                 for all other messages                              */
/* ================================================================== */
static LRESULT CALLBACK TipWndProcInternal(HWND hwnd, UINT msg,
                                            WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:      Paint_Tooltip(hwnd); return 0;
    case WM_ERASEBKGND: return 1; /* suppress erase to prevent flicker */
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Window_Create                                                       */
/*  Purpose: Registers all custom window classes (CMW32TabBar,         */
/*           CMW32StatusBar, CMW32ScrollPanel, CMW32Tip, main class),  */
/*           creates the main window and all child controls, and       */
/*           performs initial GDI and layout setup.                    */
/*  In:  hInst — application instance handle from wWinMain            */
/*  Out: (void — populates g.hwnd, g.hwnd_tab, g.hwnd_scroll,         */
/*               g.hwnd_status, g.hwnd_search, g.hwnd_tip)            */
/* ================================================================== */
void Window_Create(HINSTANCE hInst)
{
    App_InitGDI();

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

    WNDCLASSEX wctb = { .cbSize=sizeof(wctb), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=TabBarProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName=L"CMW32TabBar" };
    RegisterClassEx(&wctb);

    WNDCLASSEX wc = { .cbSize=sizeof(wc), .style=CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc=MainWndProc, .hInstance=hInst,
        .hCursor=LoadCursor(NULL,IDC_ARROW),
        .hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszMenuName=NULL,
        .lpszClassName=APP_CLASS,
        .hIcon=LoadIcon(hInst,MAKEINTRESOURCE(IDI_APP_ICON)),
        .hIconSm=LoadIcon(hInst,MAKEINTRESOURCE(IDI_APP_ICON)) };
    RegisterClassEx(&wc);

    QuickBar_Register(hInst);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 820, wh = 540; /* initial window dimensions */

    g.hwnd = CreateWindowEx(0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW,
        (sw-ww)/2, (sh-wh)/2, ww, wh, NULL, NULL, hInst, NULL); /* (sw-ww)/2 centres horizontally */

    Window_ApplyDarkMode(g.hwnd);

    CreateWindow(L"BUTTON", L"\u2630  Menu",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        6, 5, 82, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_MENU, hInst, NULL);
    CreateWindow(L"BUTTON", L"\u27F3  Refresh",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        94, 5, 88, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInst, NULL);
    CreateWindow(L"BUTTON", L"\u2699  Settings",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        188, 5, 88, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_SETTINGS, hInst, NULL);
    CreateWindow(L"BUTTON", L"\u2B07  Deps",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        282, 5, 76, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_UPDATE_DEPS, hInst, NULL);
    CreateWindow(L"BUTTON", L"\u25A0  Stop",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_DISABLED,
        364, 5, 70, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_STOP, hInst, NULL);
    CreateWindow(L"BUTTON", L"\u2261  Log",
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        440, 5, 62, 28, g.hwnd,
        (HMENU)(UINT_PTR)IDC_BTN_LOG, hInst, NULL);

    int ids[] = { IDC_BTN_MENU, IDC_BTN_REFRESH,
                  IDC_BTN_SETTINGS, IDC_BTN_UPDATE_DEPS, IDC_BTN_STOP, IDC_BTN_LOG };
    for (int i = 0; i < 6; i++) /* apply UI font to all 6 toolbar buttons */
        SendDlgItemMessage(g.hwnd, ids[i], WM_SETFONT,
                           (WPARAM)g.font_ui, TRUE);

    /* Search/filter box - full client width, positioned below toolbar */
    {
        RECT cr; GetClientRect(g.hwnd, &cr);
        int cw = cr.right;
        g.hwnd_search = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            4, TOOLBAR_H + 2, cw - 8, SEARCH_H - 4, g.hwnd,
            (HMENU)(UINT_PTR)IDC_SEARCH, hInst, NULL);
    }
    SendMessage(g.hwnd_search, EM_SETCUEBANNER, 0,
                (LPARAM)L"  Filter scripts...");
    SendMessage(g.hwnd_search, WM_SETFONT, (WPARAM)g.font_ui, TRUE);

    g.hwnd_tab = CreateWindow(L"CMW32TabBar", NULL,
        WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, TOOLBAR_H + SEARCH_H, ww, TAB_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_TAB_CTRL, hInst, NULL);

    int ct = TOOLBAR_H + SEARCH_H + TAB_H; /* top of scroll panel = toolbar + search + tab bar */
    int ch = wh - ct - STATUS_H; /* scroll panel height = remaining space above status bar */

    /* Search box spans full width */
    if (g.hwnd_search)
        SetWindowPos(g.hwnd_search, NULL, 4, TOOLBAR_H + 2, ww - 8, SEARCH_H - 4,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    g.hwnd_scroll = CreateWindowEx(0, L"CMW32ScrollPanel", NULL,
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_CLIPCHILDREN,
        0, ct, ww, ch,
        g.hwnd, (HMENU)(UINT_PTR)IDC_SCROLL_PANEL, hInst, NULL);

    g.hwnd_status = CreateWindow(L"CMW32StatusBar",
        L"Checking for updates\u2026",
        WS_CHILD|WS_VISIBLE,
        0, wh-STATUS_H, ww, STATUS_H,
        g.hwnd, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
    SendMessage(g.hwnd_status, WM_SETFONT, (WPARAM)g.font_small, TRUE);

    g.hwnd_tip = CreateWindowEx(WS_EX_TOPMOST|WS_EX_NOACTIVATE, /* WS_EX_NOACTIVATE: tooltip never steals focus */
        L"CMW32Tip", NULL, WS_POPUP,
        0, 0, 300, 160, g.hwnd, NULL, hInst, NULL);
}

/* ================================================================== */
/*  Window_OnSize                                                       */
/*  Purpose: Repositions and resizes all child controls to fill the    */
/*           main window when it is resized.  Also triggers a button  */
/*           rebuild so the script grid reflows to the new width.      */
/*  In:  w — new client width in pixels                                */
/*       h — new client height in pixels                               */
/*  Out: (void)                                                         */
/* ================================================================== */
void Window_OnSize(int w, int h)
{
    int tab_y    = TOOLBAR_H + SEARCH_H;
    int ct       = tab_y + TAB_H;
    int ch       = h - ct - STATUS_H;
    if (ch < 0) ch = 0; /* prevent negative height if window is smaller than the fixed chrome */

    /* Search box - full client width with small margin */
    if (g.hwnd_search)
        SetWindowPos(g.hwnd_search, NULL, 4, TOOLBAR_H + 2, w - 8, SEARCH_H - 4,
                     SWP_NOZORDER | SWP_NOACTIVATE);

    SetWindowPos(g.hwnd_tab,    NULL, 0, tab_y,     w, TAB_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_scroll, NULL, 0, ct,        w, ch,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g.hwnd_status, NULL, 0, h-STATUS_H, w, STATUS_H,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    Tabs_RebuildButtons();
    InvalidateRect(g.hwnd, NULL, FALSE);
}
