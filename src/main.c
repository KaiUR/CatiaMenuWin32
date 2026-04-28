/*
 * main.c  -  Entry point and main window procedure.
 * CatiaMenuWin32  |  Author: Kai-Uwe Rathjen  |  License: MIT
 */

#include "main.h"

AppState g = {0};

static void Handle_Command(WPARAM wp);
static void Handle_SyncDone(SyncResult *sr);
static bool SystemIsDark(void);

/* ================================================================== */
/*  wWinMain                                                            */
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

    Window_Create(hInst);

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

    App_FreeGDI();
    return (int)msg.wParam;
}

/* ================================================================== */
/*  App_ResolveTheme                                                    */
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
/* ================================================================== */
void App_BuildAppDataPath(void)
{
    WCHAR appdata[MAX_APPPATH] = {0};
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata);
    _snwprintf_s(g.appdata_dir, MAX_APPPATH , _TRUNCATE, L"%s\\%s", appdata, APP_APPDATA_DIR);
    SHCreateDirectoryEx(NULL, g.appdata_dir, NULL);
}

/* ================================================================== */
/*  App_InitGDI / App_FreeGDI / App_RebuildGDI                        */
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

void App_FreeGDI(void)
{
    DeleteObject(g.font_ui);  DeleteObject(g.font_bold);
    DeleteObject(g.font_small);
    DeleteObject(g.br_bg);    DeleteObject(g.br_toolbar);
    DeleteObject(g.br_btn);   DeleteObject(g.br_btn_hot);
    DeleteObject(g.br_accent); DeleteObject(g.br_status);
}

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
/* ================================================================== */
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        /* F1 opens help */
        if (wp == VK_F1) { Help_Show(); return 0; }
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
                id == IDC_BTN_UPDATE_DEPS) {
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

    /* Re-apply always-on-top every time the window becomes visible
       (e.g. restored from tray) so it never gets lost */
    case WM_SHOWWINDOW:
        if (wp) Window_ApplyAlwaysOnTop();
        break;

    case WM_CLOSE:
        if (g.cfg.minimize_to_tray) {
            Window_AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        Window_RemoveTrayIcon();
        Settings_Save(&g.cfg);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Handle_Command                                                      */
/* ================================================================== */
static void Handle_Command(WPARAM wp)
{
    int id = LOWORD(wp);

    if (id >= IDC_SCRIPT_BTN_BASE && id < IDC_SCRIPT_BTN_BASE + MAX_SCRIPTS) {
        Runner_Run(g.active_tab, id - IDC_SCRIPT_BTN_BASE);
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


    case IDM_GITHUB_SCRIPTS:
        ShellExecute(NULL, L"open",
            L"https://github.com/KaiUR/Pycatia_Scripts", NULL, NULL, SW_SHOW);
        break;

    case IDM_HELP_CONTENTS:
        Help_Show();
        break;

    case IDM_ABOUT:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT),
                  g.hwnd, AboutDlgProc);
        break;

    case IDM_UPDATE_DEPS:
    case IDC_BTN_UPDATE_DEPS:
        Runner_UpdateDeps();
        break;

    case IDM_EXIT:
        Window_RemoveTrayIcon();
        DestroyWindow(g.hwnd);
        break;
    }
}

/* ================================================================== */
/*  Handle_SyncDone                                                     */
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
        Tabs_Switch(g.active_tab < g.folder_count ? g.active_tab : 0);
    }
    SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)sr->message);
    free(sr);
    InvalidateRect(g.hwnd, NULL, FALSE);
}
