/*
 * main.c  -  Entry point and main window procedure.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

AppState g = {0};

static void Handle_Command(WPARAM wp);
static void Handle_SyncDone(SyncResult *sr);
static bool SystemIsDark(void);

/* ================================================================== */
/*  GlobalKbdHookProc  (static)                                        */
/*  Permanent low-level keyboard hook active for the lifetime of the  */
/*  application.  Forwards hotkeys to the main window so they work    */
/*  regardless of which application currently has keyboard focus.     */
/*                                                                     */
/*  Only fires when g.hwnd is NOT the foreground window to avoid      */
/*  double-delivery when our own window already handles the key.      */
/*                                                                     */
/*  Keys forwarded:                                                    */
/*    Escape — cancel repeat mode and/or stop running script          */
/*    F9     — re-run last script                                      */
/* ================================================================== */
static LRESULT CALLBACK GlobalKbdHookProc(int nCode, WPARAM wp, LPARAM lp)
{
    if (nCode == HC_ACTION && wp == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lp;
        if (GetForegroundWindow() != g.hwnd) {
            if (kb->vkCode == VK_ESCAPE &&
                (g.repeat_mode || (!g.cfg.show_console && g.run_process)))
                PostMessage(g.hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            if (kb->vkCode == VK_F9)
                PostMessage(g.hwnd, WM_KEYDOWN, VK_F9, 0);
        }
    }
    return CallNextHookEx(g.kbd_repeat_hook, nCode, wp, lp);
}

/* ================================================================== */
/*  Repeat_Start                                                       */
/*  Activates repeat mode for the script at (fi, si) and installs a   */
/*  global low-level keyboard hook so Escape works regardless of      */
/*  which window has focus.                                            */
/* ================================================================== */
void Repeat_Start(int fi, int si)
{
    g.repeat_fi   = fi;
    g.repeat_si   = si;
    g.repeat_mode = true;
}

/* ================================================================== */
/*  Repeat_Stop                                                        */
/*  Cancels repeat mode, removes the keyboard hook, and repaints the  */
/*  button that was highlighted amber.  Safe to call when not in      */
/*  repeat mode (no-op).                                               */
/* ================================================================== */
void Repeat_Stop(void)
{
    if (!g.repeat_mode) return;
    g.repeat_mode = false;
    HWND hBtn = GetDlgItem(g.hwnd_scroll,
                            IDC_SCRIPT_BTN_BASE + g.repeat_si);
    if (hBtn) InvalidateRect(hBtn, NULL, FALSE);
    if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
}

/* ================================================================== */
/*  wWinMain                                                            */
/*  Purpose: Application entry point.  Enforces single-instance via    */
/*           a named mutex, initialises settings, creates the main     */
/*           window, starts the sync and update-checker threads, then  */
/*           runs the Win32 message loop.                              */
/*  In:  hInst — module instance handle                                 */
/*       hPrev — always NULL in Win32                                   */
/*       lpCmd — command-line string (checked for /minimized)          */
/*       nShow — requested show state from the shell                   */
/*  Out: exit code (0 for success or second-instance early exit)       */
/* ================================================================== */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev,
                    LPWSTR lpCmd, int nShow)
{
    (void)hPrev;

    /* ── Single instance check ──────────────────────────────────── */
    /* If another instance is already running, bring it to the front
       (or show it from tray) and exit this new instance. */
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"CatiaMenuWin32_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Find the existing window and restore it */
        HWND hExisting = FindWindow(APP_CLASS, NULL);
        if (hExisting) {
            /* If hidden (minimized to tray), show it */
            if (!IsWindowVisible(hExisting))
                ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    INITCOMMONCONTROLSEX icc = {
        .dwSize = sizeof(icc),
        .dwICC  = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&icc);

    App_BuildAppDataPath();
    Settings_Load(&g.cfg);
    App_ResolveTheme();

    InitializeCriticalSection(&g.cs_folders);

    Window_Create(hInst);

    /* Install permanent global keyboard hook for cross-app hotkeys (Escape, F9) */
    g.kbd_repeat_hook = SetWindowsHookEx(WH_KEYBOARD_LL, GlobalKbdHookProc, NULL, 0);

    /* Check command line for /minimized flag from autorun */
    bool launch_minimized = g.cfg.start_minimized ||
        (lpCmd && wcsstr(lpCmd, L"/minimized"));

    /* Show window first, THEN apply topmost - SetWindowPos needs a visible window */
    if (launch_minimized && g.cfg.minimize_to_tray) {
        ShowWindow(g.hwnd, SW_HIDE);
        Window_AddTrayIcon();
    } else if (launch_minimized) {
        ShowWindow(g.hwnd, SW_SHOWMINIMIZED);
    } else {
        ShowWindow(g.hwnd, nShow);
        UpdateWindow(g.hwnd);
    }

    /* Apply always-on-top AFTER ShowWindow so it sticks */
    Window_ApplyAlwaysOnTop();

    /* Create the Quick Launch Bar (hidden until enabled by user) */
    QuickBar_Create();

    /* Load scripts from local cache immediately so tabs show before
       sync completes and when there is no internet connection */
    Sync_LoadManifest();
    Prefs_Load();
    Prefs_ApplyToFolders();
    Tabs_BuildFavourites();
    if (g.folder_count > 0) {
        Tabs_Build();
        Tabs_Switch(0);
    }

    /* Start auto-refresh timer if configured */
    if (g.cfg.refresh_interval > 0) {
        SetTimer(g.hwnd, TIMER_AUTO_REFRESH,
                 g.cfg.refresh_interval * 3600 * 1000, NULL);
    }

    if (g.cfg.auto_sync) {
        g.syncing = true;
        HANDLE hT = CreateThread(NULL, 0, Sync_Thread, NULL, 0, NULL);
        if (hT) CloseHandle(hT);
    } else {
        SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                    (LPARAM)L"Auto-sync disabled. Press Refresh to sync.");
    }

    /* Check for app updates in background */
    if (g.cfg.check_updates) {
        HANDLE hU = CreateThread(NULL, 0, Updater_CheckThread, NULL, 0, NULL);
        if (hU) CloseHandle(hU);
    } else if (!g.cfg.auto_sync) {
        SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                    (LPARAM)L"Auto-sync disabled. Press Refresh to load scripts.");
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    App_FreeGDI();
    return (int)msg.wParam;
}

/* ================================================================== */
/*  SystemIsDark  (static)                                             */
/*  Purpose: Queries the Windows registry to determine whether the     */
/*           system is currently using the dark app theme.             */
/*  In:  (none)                                                         */
/*  Out: true if the system dark theme is active; false otherwise      */
/* ================================================================== */
static bool SystemIsDark(void)
{
    DWORD val = 1; /* default light */
    DWORD sz  = sizeof(val);
    RegGetValue(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD, NULL, &val, &sz);
    return (val == 0);  /* 0 = dark, 1 = light */
}

/* ================================================================== */
/*  App_ResolveTheme                                                    */
/*  Purpose: Sets g.dark_mode based on g.cfg.theme.  THEME_SYSTEM     */
/*           queries the registry via SystemIsDark(); THEME_DARK and   */
/*           THEME_LIGHT set the flag directly.                        */
/*  In:  (reads g.cfg.theme)                                           */
/*  Out: (writes g.dark_mode)                                          */
/* ================================================================== */
void App_ResolveTheme(void)
{
    switch (g.cfg.theme) {
    case THEME_DARK:   g.dark_mode = true;           break;
    case THEME_LIGHT:  g.dark_mode = false;          break;
    default:           g.dark_mode = SystemIsDark(); break;
    }
}

/* ================================================================== */
/*  App_BuildAppDataPath                                                */
/*  Purpose: Resolves %APPDATA%\CatiaMenuWin32 and stores the full    */
/*           path in g.appdata_dir.  Creates the directory if it does  */
/*           not exist.                                                */
/*  In:  (none)                                                         */
/*  Out: (writes g.appdata_dir; creates directory on disk)             */
/* ================================================================== */
void App_BuildAppDataPath(void)
{
    WCHAR appdata[MAX_APPPATH] = {0};
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata);
    _snwprintf_s(g.appdata_dir, MAX_APPPATH , _TRUNCATE, L"%s\\%s", appdata, APP_APPDATA_DIR);
    SHCreateDirectoryEx(NULL, g.appdata_dir, NULL);
}

/* ================================================================== */
/*  App_InitGDI                                                         */
/*  Purpose: Creates all shared GDI resources (fonts and brushes) for  */
/*           the current theme and stores them in the global g struct. */
/*           Called once after the main window is created.             */
/*  In:  (reads g.dark_mode for theme-appropriate brush colours)       */
/*  Out: (writes g.font_ui, g.font_bold, g.font_small, g.br_* fields) */
/* ================================================================== */
void App_InitGDI(void)
{
    g.font_ui    = CreateFont(-13,0,0,0,FW_NORMAL,  0,0,0,DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    g.font_bold  = CreateFont(-13,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    g.font_small = CreateFont(-11,0,0,0,FW_NORMAL,  0,0,0,DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");

    g.br_bg      = CreateSolidBrush(COL_BG());
    g.br_toolbar = CreateSolidBrush(COL_TOOLBAR());
    g.br_btn     = CreateSolidBrush(COL_BTN_NORM());
    g.br_btn_hot = CreateSolidBrush(COL_BTN_HOT());
    g.br_accent  = CreateSolidBrush(COL_ACCENT);
    g.br_status  = CreateSolidBrush(COL_TOOLBAR());
    g.hot_btn    = -1;
    g.tip_btn    = -1;
}

/* ================================================================== */
/*  App_FreeGDI                                                         */
/*  Purpose: Deletes all GDI objects created by App_InitGDI.           */
/*           Called once on application exit before the message loop   */
/*           returns.                                                   */
/*  In:  (reads g.font_* and g.br_* handles)                           */
/*  Out: (void — all GDI handles are released)                         */
/* ================================================================== */
void App_FreeGDI(void)
{
    DeleteObject(g.font_ui);  DeleteObject(g.font_bold);
    DeleteObject(g.font_small);
    DeleteObject(g.br_bg);    DeleteObject(g.br_toolbar);
    DeleteObject(g.br_btn);   DeleteObject(g.br_btn_hot);
    DeleteObject(g.br_accent); DeleteObject(g.br_status);
}

/* ================================================================== */
/*  App_RebuildGDI                                                      */
/*  Purpose: Recreates only the colour brushes (not fonts) after a     */
/*           theme change.  Fonts are theme-independent and not        */
/*           recreated.  Called after App_ResolveTheme changes         */
/*           g.dark_mode.                                              */
/*  In:  (reads updated g.dark_mode for new brush colours)             */
/*  Out: (replaces g.br_* handles; old handles are deleted first)      */
/* ================================================================== */
void App_RebuildGDI(void)
{
    /* Delete brushes (not fonts) and recreate with new colours */
    DeleteObject(g.br_bg);    DeleteObject(g.br_toolbar);
    DeleteObject(g.br_btn);   DeleteObject(g.br_btn_hot);
    DeleteObject(g.br_accent); DeleteObject(g.br_status);

    g.br_bg      = CreateSolidBrush(COL_BG());
    g.br_toolbar = CreateSolidBrush(COL_TOOLBAR());
    g.br_btn     = CreateSolidBrush(COL_BTN_NORM());
    g.br_btn_hot = CreateSolidBrush(COL_BTN_HOT());
    g.br_accent  = CreateSolidBrush(COL_ACCENT);
    g.br_status  = CreateSolidBrush(COL_TOOLBAR());
}

/* ================================================================== */
/*  MainWndProc                                                         */
/*  Purpose: Window procedure for the main application window.  Routes */
/*           keyboard shortcuts, paint messages, tray events, sync     */
/*           completion, theme/settings changes, and menu commands.    */
/*  In:  hwnd — handle to the main window                              */
/*       msg  — Windows message identifier                             */
/*       wp   — message-specific WPARAM                                */
/*       lp   — message-specific LPARAM                                */
/*  Out: LRESULT — 0 for handled messages; DefWindowProc result        */
/*                 for unhandled messages                              */
/* ================================================================== */
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        /* F1 opens help */
        if (wp == VK_F1) { Help_Show(); return 0; }
        /* Escape cancels repeat mode and/or stops a running background script */
        if (wp == VK_ESCAPE &&
            (g.repeat_mode || (!g.cfg.show_console && g.run_process))) {
            if (g.repeat_mode) {
                Repeat_Stop();
                PostStatus(L"Repeat cancelled.");
            }
            if (!g.cfg.show_console && g.run_process)
                Runner_Stop();
            return 0;
        }
        /* Ctrl+Tab / Ctrl+Shift+Tab to switch tabs */
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wp == VK_TAB) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int next = g.active_tab + (shift ? -1 : 1);
                if (next < 0) next = g.folder_count - 1;
                if (next >= g.folder_count) next = 0;
                Tabs_Switch(next);
                return 0;
            }
        }
        if (wp == VK_F5) { SendMessage(hwnd, WM_COMMAND, IDM_REFRESH, 0); return 0; }
        if (wp == VK_F9) { SendMessage(hwnd, WM_COMMAND, IDM_RUN_LAST, 0); return 0; }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Paint_MainWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED && g.cfg.minimize_to_tray) {
            Window_AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        if (wp != SIZE_MINIMIZED)
            Window_OnSize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = WIN_MIN_W;
        mm->ptMinTrackSize.y = WIN_MIN_H;
        return 0;
    }

    /* System colour/theme change */
    case WM_TIMER:
        if (wp == TIMER_AUTO_REFRESH && !g.syncing) {
            g.syncing = true;
            HANDLE hT = CreateThread(NULL, 0, Sync_Thread, NULL, 0, NULL);
            if (hT) CloseHandle(hT);
        }
        break;

    case WM_SETTINGCHANGE:
        if (g.cfg.theme == THEME_SYSTEM) {
            App_ResolveTheme();
            App_RebuildGDI();
            Window_ApplyDarkMode(hwnd);
            Window_ApplyThemeToChildren(hwnd);
            QuickBar_OnThemeChange();
            Tabs_RebuildButtons();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_SEARCH && HIWORD(wp) == EN_CHANGE) {
            GetWindowText(g.hwnd_search, g.filter_text, MAX_NAME - 1);
            Tabs_ApplyFilter();
            return 0;
        }
        Handle_Command(wp);
        return 0;

    case WM_NOTIFY:
        break;

    /* Owner-draw toolbar buttons */
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlType == ODT_BUTTON) {
            int id = (int)dis->CtlID;
            if (id == IDC_BTN_MENU ||
                id == IDC_BTN_REFRESH ||
                id == IDC_BTN_SETTINGS ||
                id == IDC_BTN_UPDATE_DEPS ||
                id == IDC_BTN_STOP) {
                Paint_ToolbarButton(dis);
                return TRUE;
            }
        }
        break;
    }



    /* Dark-mode aware child colouring */
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT());
        return (LRESULT)g.br_bg;
    }
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT());
        return (LRESULT)g.br_toolbar;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, COL_BTN_NORM());
        SetTextColor(hdc, COL_TEXT());
        return (LRESULT)g.br_btn;
    }

    /* System tray */
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP) {
            Window_ShowTrayMenu();
        }
        return 0;

    case WM_SYNC_DONE:
        Handle_SyncDone((SyncResult *)wp);
        return 0;

    case WM_UPDATE_AVAIL:
        InvalidateRect(hwnd, NULL, FALSE);  /* repaint toolbar to show badge */
        if (wp == 1)
            Updater_AutoUpdate(g.latest_version);
        else
            Updater_PromptAndInstall(g.latest_version);
        return 0;

    case WM_STATUS_SET:
    {
        WCHAR *text = (WCHAR *)lp;
        if (text) {
            SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)text);
            free(text);
        }
        return 0;
    }

    case WM_SCRIPT_STARTED:
        g.script_running = true;
        EnableWindow(GetDlgItem(hwnd, IDC_BTN_STOP), TRUE);
        {
            HWND hBtn = GetDlgItem(g.hwnd_scroll, IDC_SCRIPT_BTN_BASE + g.run_si);
            if (hBtn) InvalidateRect(hBtn, NULL, FALSE);
        }
        if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
        return 0;

    case WM_SCRIPT_STOPPED:
        g.script_running = false;
        EnableWindow(GetDlgItem(hwnd, IDC_BTN_STOP), FALSE);
        {
            HWND hBtn = GetDlgItem(g.hwnd_scroll, IDC_SCRIPT_BTN_BASE + g.run_si);
            if (hBtn) InvalidateRect(hBtn, NULL, FALSE);
        }
        if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
        if (g.repeat_mode) {
            if (wp != 0) {
                /* Non-zero exit code → script failed; stop repeating */
                Repeat_Stop();
                PostStatus(L"Repeat cancelled: script exited with error (code %llu).",
                           (unsigned long long)wp);
            } else {
                Runner_Run(g.repeat_fi, g.repeat_si);
            }
        }
        return 0;

    /* Re-apply always-on-top every time the window becomes visible
       (e.g. restored from tray) so it never gets lost */
    case WM_SHOWWINDOW:
        if (wp) Window_ApplyAlwaysOnTop();
        break;

    case WM_CLOSE:
        if (g.cfg.minimize_to_tray && wp == 0) {
            Window_AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        /* wp != 0 means force quit (e.g. from auto-update) */
        Window_RemoveTrayIcon();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g.kbd_repeat_hook) {
            UnhookWindowsHookEx(g.kbd_repeat_hook);
            g.kbd_repeat_hook = NULL;
        }
        QuickBar_Destroy();
        Window_RemoveTrayIcon();
        Settings_Save(&g.cfg);
        DeleteCriticalSection(&g.cs_folders);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Handle_Command  (static)                                           */
/*  Purpose: Dispatches WM_COMMAND notifications from toolbar buttons, */
/*           menu items, and script buttons.  Handles all IDM_* and   */
/*           IDC_* identifiers including script execution, settings,   */
/*           theme switching, sort, and tray actions.                  */
/*  In:  wp — WPARAM from WM_COMMAND (LOWORD = control/menu ID)        */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Handle_Command(WPARAM wp)
{
    int id = LOWORD(wp);

    if (id >= IDC_SCRIPT_BTN_BASE && id < IDC_SCRIPT_BTN_BASE + MAX_SCRIPTS) {
        int fi = g.active_tab;
        int si = id - IDC_SCRIPT_BTN_BASE;
        if (g.repeat_mode) {
            bool same = (g.repeat_fi == fi && g.repeat_si == si);
            Repeat_Stop();
            if (same) return; /* cancel repeat, don't re-run */
        }
        Runner_Run(fi, si);
        return;
    }

    switch (id)
    {
    case IDC_BTN_MENU:
        Window_ShowMenu();
        break;

    case IDM_REFRESH:
    case IDC_BTN_REFRESH:
        if (!g.syncing) {
            g.syncing = true;
            for (int i = 0; i < g.folder_count; i++)
                g.folders[i].loaded = false;
            HANDLE hT = CreateThread(NULL, 0, Sync_Thread, NULL, 0, NULL);
            if (hT) CloseHandle(hT);
        }
        break;

    case IDM_SETTINGS:
    case IDC_BTN_SETTINGS:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS),
                  g.hwnd, SettingsDlgProc);
        break;

    case IDM_SOURCES:
        if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SOURCES),
                  g.hwnd, SourcesDlgProc) == IDOK) {
            /* Re-sync with new sources */
            if (!g.syncing) {
                g.syncing = true;
                HANDLE hT = CreateThread(NULL, 0, Sync_Thread, NULL, 0, NULL);
                if (hT) CloseHandle(hT);
            }
        }
        break;

    case IDM_ALWAYS_ON_TOP:
        g.cfg.always_on_top = !g.cfg.always_on_top;
        Window_ApplyAlwaysOnTop();
        CheckMenuItem(GetMenu(g.hwnd), IDM_ALWAYS_ON_TOP,
            g.cfg.always_on_top ? MF_CHECKED : MF_UNCHECKED);
        Settings_Save(&g.cfg);
        break;

    case IDM_MINIMIZE_TO_TRAY:
        g.cfg.minimize_to_tray = !g.cfg.minimize_to_tray;
        CheckMenuItem(GetMenu(g.hwnd), IDM_MINIMIZE_TO_TRAY,
            g.cfg.minimize_to_tray ? MF_CHECKED : MF_UNCHECKED);
        Settings_Save(&g.cfg);
        break;

    case IDM_START_WITH_WINDOWS:
        g.cfg.start_with_windows = !g.cfg.start_with_windows;
        CheckMenuItem(GetMenu(g.hwnd), IDM_START_WITH_WINDOWS,
            g.cfg.start_with_windows ? MF_CHECKED : MF_UNCHECKED);
        Settings_ApplyAutorun(g.cfg.start_with_windows, g.cfg.start_minimized);
        Settings_Save(&g.cfg);
        break;

    case IDM_START_MINIMIZED:
        g.cfg.start_minimized = !g.cfg.start_minimized;
        CheckMenuItem(GetMenu(g.hwnd), IDM_START_MINIMIZED,
            g.cfg.start_minimized ? MF_CHECKED : MF_UNCHECKED);
        if (g.cfg.start_with_windows)
            Settings_ApplyAutorun(true, g.cfg.start_minimized);
        Settings_Save(&g.cfg);
        break;

    case IDM_THEME_DARK:
        g.cfg.theme = THEME_DARK;
        goto apply_theme;
    case IDM_THEME_LIGHT:
        g.cfg.theme = THEME_LIGHT;
        goto apply_theme;
    case IDM_THEME_SYSTEM:
        g.cfg.theme = THEME_SYSTEM;
apply_theme:
        App_ResolveTheme();
        App_RebuildGDI();
        Window_ApplyDarkMode(g.hwnd);
        Window_ApplyThemeToChildren(g.hwnd);
        QuickBar_OnThemeChange();
        /* Update menu checkmarks */
        CheckMenuItem(GetMenu(g.hwnd), IDM_THEME_DARK,
            g.cfg.theme == THEME_DARK   ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(GetMenu(g.hwnd), IDM_THEME_LIGHT,
            g.cfg.theme == THEME_LIGHT  ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(GetMenu(g.hwnd), IDM_THEME_SYSTEM,
            g.cfg.theme == THEME_SYSTEM ? MF_CHECKED : MF_UNCHECKED);
        Tabs_RebuildButtons();
        InvalidateRect(g.hwnd, NULL, TRUE);
        Settings_Save(&g.cfg);
        break;

    case IDM_RUN_LAST:
        if (g.last_run_path[0]) {
            for (int fi = 0; fi < g.folder_count; fi++)
                for (int si = 0; si < g.folders[fi].count; si++)
                    if (wcscmp(g.folders[fi].scripts[si].gh_path,
                               g.last_run_path) == 0) {
                        Runner_Run(fi, si);
                        return;
                    }
        }
        break;

    case IDM_OPEN_CACHE:
        ShellExecute(NULL, L"explore", g.cfg.cache_dir, NULL, NULL, SW_SHOW);
        break;

    case IDM_GITHUB:
        ShellExecute(NULL, L"open",
            L"https://github.com/KaiUR/CatiaMenuWin32", NULL, NULL, SW_SHOW);
        break;

    case IDM_GITHUB_PAGES:
        ShellExecute(NULL, L"open",
            L"https://kaiur.github.io/CatiaMenuWin32/", NULL, NULL, SW_SHOW);
        break;

    case IDM_WIKI:
        ShellExecute(NULL, L"open",
            L"https://github.com/KaiUR/CatiaMenuWin32/wiki", NULL, NULL, SW_SHOW);
        break;

    case IDM_WIKI_SCRIPTS:
        ShellExecute(NULL, L"open",
            L"https://github.com/KaiUR/Pycatia_Scripts/wiki", NULL, NULL, SW_SHOW);
        break;

    case IDM_OPEN_EXE_FOLDER:
        Window_OpenExeFolder();
        break;

    case IDM_HIDDEN_SCRIPTS:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_HIDDEN_SCRIPTS),
                  g.hwnd, HiddenScriptsDlgProc);
        Tabs_RebuildButtons();
        break;

    case IDM_SORT_DEFAULT:
        g.cfg.sort_mode = SORT_ORDER;
        Settings_Save(&g.cfg);
        Tabs_RebuildButtons();
        break;
    case IDM_SORT_ALPHA:
        g.cfg.sort_mode = SORT_ALPHA;
        Settings_Save(&g.cfg);
        Tabs_ApplySort(g.active_tab);
        Tabs_RebuildButtons();
        break;
    case IDM_SORT_DATE:
        g.cfg.sort_mode = SORT_DATE;
        Settings_Save(&g.cfg);
        Tabs_ApplySort(g.active_tab);
        Tabs_RebuildButtons();
        break;
    case IDM_SORT_MOST_USED:
        g.cfg.sort_mode = SORT_MOST_USED;
        Settings_Save(&g.cfg);
        Tabs_ApplySort(g.active_tab);
        Tabs_RebuildButtons();
        break;


    /* ── Quick Launch Bar ──────────────────────────────────────────── */
    case IDM_QBAR_TOGGLE:
        g.cfg.qbar_enabled = !g.cfg.qbar_enabled;
        QuickBar_Show(g.cfg.qbar_enabled);
        Settings_Save(&g.cfg);
        break;

    case IDM_QBAR_HORIZONTAL:
        if (!g.cfg.qbar_horizontal) {
            g.cfg.qbar_horizontal = true;
            /* Recreate bar with new orientation */
            QuickBar_Destroy();
            QuickBar_Register(GetModuleHandle(NULL));
            QuickBar_Create();
            QuickBar_Rebuild();
            Settings_Save(&g.cfg);
        }
        break;

    case IDM_QBAR_VERTICAL:
        if (g.cfg.qbar_horizontal) {
            g.cfg.qbar_horizontal = false;
            QuickBar_Destroy();
            QuickBar_Register(GetModuleHandle(NULL));
            QuickBar_Create();
            QuickBar_Rebuild();
            Settings_Save(&g.cfg);
        }
        break;

    case IDM_QBAR_TOPMOST:
        g.cfg.qbar_topmost_with_catia = !g.cfg.qbar_topmost_with_catia;
        if (!g.cfg.qbar_topmost_with_catia)
            QuickBar_SetTopmost(false);
        Settings_Save(&g.cfg);
        break;

    case IDM_QBAR_RESET_POS:
    {
        RECT work; SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
        g.cfg.qbar_x = work.right - (QBAR_BTN_SIZE + 2 * QBAR_PAD + 2) - 20;
        g.cfg.qbar_y = work.top + 60;
        if (g.hwnd_qbar)
            SetWindowPos(g.hwnd_qbar, NULL,
                g.cfg.qbar_x, g.cfg.qbar_y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Settings_Save(&g.cfg);
        break;
    }

    case IDM_QBAR_SET_TARGET:
        QuickBar_ShowTargetDlg();
        break;

    case IDM_REPEAT_MAINAPP:
        g.cfg.repeat_on_dblclick = !g.cfg.repeat_on_dblclick;
        if (!g.cfg.repeat_on_dblclick) Repeat_Stop();
        Settings_Save(&g.cfg);
        break;

    case IDM_REPEAT_QBAR:
        g.cfg.qbar_repeat_on_dblclick = !g.cfg.qbar_repeat_on_dblclick;
        if (!g.cfg.qbar_repeat_on_dblclick) Repeat_Stop();
        Settings_Save(&g.cfg);
        break;

    case IDM_GITHUB_SCRIPTS:
        ShellExecute(NULL, L"open",
            L"https://github.com/KaiUR/Pycatia_Scripts", NULL, NULL, SW_SHOW);
        break;

    case IDM_CHECK_UPDATES:
    {
        HANDLE hU = CreateThread(NULL, 0, Updater_CheckThread, (LPVOID)1, 0, NULL);
        if (hU) CloseHandle(hU);
        break;
    }

    case IDM_HELP_CONTENTS:
        Help_Show();
        break;

    case IDM_REPORT_BUG:
    {
        WCHAR win_ver[64] = {0};
        OSVERSIONINFOEXW osvi = {0};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        typedef LONG (WINAPI *RtlGetVersion_t)(OSVERSIONINFOEXW *);
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            union { FARPROC proc; RtlGetVersion_t fn; } u;
            u.proc = GetProcAddress(hNtdll, "RtlGetVersion");
            if (u.fn) u.fn(&osvi);
        }
        _snwprintf_s(win_ver, 63, _TRUNCATE, L"Windows %lu.%lu build %lu",
                     osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

        WCHAR python[MAX_APPPATH] = {0};
        Runner_FindPython(python, MAX_APPPATH);

        /* Build body - all on one line to avoid newline corruption */
        WCHAR body[2048] = {0};
        _snwprintf_s(body, 2047, _TRUNCATE,
            L"## Description\n"
            L"A clear description of the bug.\n\n"
            L"## Steps to Reproduce\n"
            L"1. \n2. \n3. \n\n"
            L"## Expected Behaviour\n\n"
            L"## Actual Behaviour\n\n"
            L"## Environment\n"
            L"- App version: v%s\n"
            L"- Windows: %s\n"
            L"- Python: %s\n"
            L"- Theme: %s\n\n"
            L"## Additional Context\n",
            VERSION_STRING_W,
            win_ver,
            python[0] ? python : L"Not found",
            g.dark_mode ? L"Dark" : L"Light");

        /* URL-encode the body (percent-encode all chars that are unsafe in a query value) */
        WCHAR encoded[3072] = {0};
        WCHAR *dst = encoded;
        for (const WCHAR *src = body; *src && dst < encoded + 3067; src++) {
            WCHAR ch = *src;
            /* Encode each unsafe character as %XX (using 3 output slots). */
            #define PENC(hi, lo) do { *dst++=L'%'; *dst++=(hi); *dst++=(lo); } while(0)
            if      (ch == L' ')  { *dst++ = L'+'; }
            else if (ch == 13)    { /* skip CR */ }
            else if (ch == 10)    { PENC(L'0',L'A'); }
            else if (ch == L'%')  { PENC(L'2',L'5'); }
            else if (ch == L'"')  { PENC(L'2',L'2'); }
            else if (ch == L'#')  { PENC(L'2',L'3'); }
            else if (ch == L'&')  { PENC(L'2',L'6'); }
            else if (ch == L'+')  { PENC(L'2',L'B'); }
            else if (ch == L'/')  { PENC(L'2',L'F'); }
            else if (ch == L':')  { PENC(L'3',L'A'); }
            else if (ch == L'<')  { PENC(L'3',L'C'); }
            else if (ch == L'=')  { PENC(L'3',L'D'); }
            else if (ch == L'>')  { PENC(L'3',L'E'); }
            else if (ch == L'?')  { PENC(L'3',L'F'); }
            else if (ch == L'[')  { PENC(L'5',L'B'); }
            else if (ch == L']')  { PENC(L'5',L'D'); }
            else if (ch == L'*')  { PENC(L'2',L'A'); }
            else if (ch == L'|')  { PENC(L'7',L'C'); }
            else { *dst++ = ch; }
            #undef PENC
        }
        *dst = L'\0';

        WCHAR url[4096] = {0};
        _snwprintf_s(url, 4095, _TRUNCATE,
            L"https://github.com/KaiUR/CatiaMenuWin32/issues/new"
            L"?template=bug_report.md&title=&body=%s",
            encoded);

        ShellExecute(NULL, L"open", url, NULL, NULL, SW_SHOW);
        break;
    }
    case IDM_ABOUT:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT),
                  g.hwnd, AboutDlgProc);
        break;

    case IDM_UPDATE_DEPS:
    case IDC_BTN_UPDATE_DEPS:
        Runner_UpdateDeps();
        break;

    case IDC_BTN_STOP:
        Repeat_Stop();
        Runner_Stop();
        break;

    case IDM_EXIT:
        Window_RemoveTrayIcon();
        DestroyWindow(g.hwnd);
        break;
    }
}

/* ================================================================== */
/*  Handle_SyncDone  (static)                                          */
/*  Purpose: Processes the WM_SYNC_DONE message posted by Sync_Thread. */
/*           Re-parses metadata, applies user prefs, rebuilds tabs and  */
/*           buttons, then restores the active tab by folder name      */
/*           (g.active_folder_name) to prevent index drift when        */
/*           Tabs_BuildFavourites shifts folder indices.  Updates the  */
/*           status bar and frees the heap-allocated SyncResult.       */
/*  In:  sr — heap-allocated SyncResult from Sync_Thread (freed here)  */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Handle_SyncDone(SyncResult *sr)
{
    g.syncing = false;
    if (!sr) return;

    if (sr->status == SR_NO_INTERNET) {
        /* No connection - keep whatever is already displayed from cache.
           Just update the status bar message. */
        SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)sr->message);
        free(sr);
        return;
    }

    /* Reset so meta re-reads from freshly downloaded files */
    for (int fi = 0; fi < g.folder_count; fi++)
        for (int si = 0; si < g.folders[fi].count; si++)
            g.folders[fi].scripts[si].meta_loaded = false;
    Meta_ParseAll();
    Prefs_ApplyToFolders();
    Tabs_BuildFavourites();
    Tabs_Build();
    if (g.folder_count == 0) {
        Tabs_DestroyButtons();
        InvalidateRect(g.hwnd_tab,    NULL, TRUE);
        InvalidateRect(g.hwnd_scroll, NULL, TRUE);
    } else {
        /* Restore the previously active folder by name so the tab doesn't drift
           when Tabs_BuildFavourites inserts/removes the Favourites tab at index 0. */
        int restore = 0;
        if (g.active_folder_name[0]) {
            for (int fi = 0; fi < g.folder_count; fi++) {
                if (wcscmp(g.folders[fi].name, g.active_folder_name) == 0) {
                    restore = fi;
                    break;
                }
            }
        } else {
            restore = (g.active_tab < g.folder_count) ? g.active_tab : 0;
        }
        Tabs_Switch(restore);
    }
    SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)sr->message);
    free(sr);
    InvalidateRect(g.hwnd, NULL, FALSE);
}
