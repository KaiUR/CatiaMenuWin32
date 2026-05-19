/*
 * quickbar.c  -  Quick Launch Bar
 * Floating button bar showing favourite scripts as large icon buttons.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ------------------------------------------------------------------ */
/*  Internal constants                                                  */
/* ------------------------------------------------------------------ */
#define QBAR_WNDCLASS   L"CMW32QuickBar"
#define QBAR_TIPCLASS   L"CMW32QBarTip"
#define HIT_NONE        (-1)   /* background / drag area               */
#define HIT_ARROW_PREV  (-2)   /* scroll-back arrow (▲ or ◄)          */
#define HIT_ARROW_NEXT  (-3)   /* scroll-forward arrow (▼ or ►)       */

/* ------------------------------------------------------------------ */
/*  Module-level state                                                  */
/* ------------------------------------------------------------------ */
static HWINEVENTHOOK s_event_hook;    /* EVENT_SYSTEM_FOREGROUND hook    */
static HWINEVENTHOOK s_minimize_hook; /* MINIMIZESTART..END hook         */
static HFONT         s_font_label;   /* large bold font for 2-letter abbrev */

/* ------------------------------------------------------------------ */
/*  CatiaState                                                          */
/*  Describes the aggregate visibility of all CATIA V5 windows.        */
/* ------------------------------------------------------------------ */
typedef enum {
    CATIA_NONE      = 0,  /* no CATIA window found at all               */
    CATIA_MINIMIZED = 1,  /* CATIA is running but all windows minimized  */
    CATIA_VISIBLE   = 2   /* at least one CATIA window is visible        */
} CatiaState;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK QuickBarProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK QBarTipProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK QB_TargetAppDlgProc(HWND, UINT, WPARAM, LPARAM);
static void QB_Paint(HWND hwnd, HDC hdc);
static int  QB_HitTest(HWND hwnd, int x, int y);
static void QB_BtnRect(HWND hwnd, int idx, RECT *r);
static int  QB_FavCount(void);
static Script *QB_GetFav(int idx, int *fi_out, int *si_out);
static void QB_UpdateGeometry(void);
static void QB_UpdateScrollMax(HWND hwnd);
static void QB_ShowTip(HWND hwnd, int idx);
static void QB_HideTip(void);
static void QB_ShowContextMenu(HWND hwnd);
static void QB_SavePos(void);
static bool QB_IsTarget(HWND hwnd);
static CatiaState QB_CatiaState(void);
static void QB_UpdateVisibility(HWND fg_hwnd);
static void CALLBACK QB_WinEventProc(HWINEVENTHOOK, DWORD, HWND,
                                      LONG, LONG, DWORD, DWORD);

/* ================================================================== */
/*  QB_FavCount                                                        */
/*  Returns count of visible scripts in the Favourites folder (idx 0) */
/* ================================================================== */
static int QB_FavCount(void)
{
    if (g.folder_count == 0) return 0;
    if (wcscmp(g.folders[0].name, L"Favourites") != 0) return 0;
    ScriptFolder *f = &g.folders[0];
    int n = 0;
    for (int i = 0; i < f->count; i++)
        if (!f->scripts[i].is_hidden) n++;
    return n;
}

/* ================================================================== */
/*  QB_GetFav                                                          */
/*  Returns a pointer to the n-th visible favourite script.           */
/* ================================================================== */
static Script *QB_GetFav(int idx, int *fi_out, int *si_out)
{
    if (g.folder_count == 0) return NULL;
    if (wcscmp(g.folders[0].name, L"Favourites") != 0) return NULL;
    ScriptFolder *f = &g.folders[0];
    int n = 0;
    for (int i = 0; i < f->count; i++) {
        if (f->scripts[i].is_hidden) continue;
        if (n == idx) {
            if (fi_out) *fi_out = 0;
            if (si_out) *si_out = i;
            return &f->scripts[i];
        }
        n++;
    }
    return NULL;
}

/* ================================================================== */
/*  QB_GetProcessExeName                                               */
/*  Retrieves the filename (not full path) of the process that owns   */
/*  hwnd into buf.  Returns true on success.                          */
/*  Uses PROCESS_QUERY_LIMITED_INFORMATION so it works across UAC     */
/*  boundaries without requiring elevated privileges.                 */
/* ================================================================== */
static bool QB_GetProcessExeName(HWND hwnd, WCHAR *buf, int len)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hp) return false;
    WCHAR path[MAX_PATH] = {0};
    DWORD pathlen = MAX_PATH;
    bool ok = QueryFullProcessImageNameW(hp, 0, path, &pathlen) != 0;
    CloseHandle(hp);
    if (!ok) return false;
    /* Extract just the filename portion after the last backslash */
    WCHAR *slash = wcsrchr(path, L'\\');
    wcsncpy_s(buf, len, slash ? slash + 1 : path, _TRUNCATE);
    return true;
}

/* ================================================================== */
/*  QB_IsTarget                                                        */
/*  Returns true if hwnd passes both the title-substring filter and   */
/*  the optional process-executable filter.                           */
/*  - Title filter (qbar_target_app): must be non-empty for any match */
/*  - Exe filter   (qbar_target_exe): when non-empty the owning       */
/*    process filename must match (case-insensitive).                  */
/* ================================================================== */
static bool QB_IsTarget(HWND hwnd)
{
    if (!hwnd || !g.cfg.qbar_target_app[0]) return false;
    WCHAR title[256] = {0};
    GetWindowTextW(hwnd, title, 255);
    if (!wcsstr(title, g.cfg.qbar_target_app)) return false;
    if (!g.cfg.qbar_target_exe[0]) return true;
    WCHAR exe[MAX_NAME] = {0};
    if (!QB_GetProcessExeName(hwnd, exe, MAX_NAME)) return false;
    return _wcsicmp(exe, g.cfg.qbar_target_exe) == 0;
}

/* ================================================================== */
/*  CatiaCheckProc  (EnumWindows callback)                            */
/*  Categorises all top-level windows into CATIA_MINIMIZED /          */
/*  CATIA_VISIBLE.  Stops early as soon as a visible window is found. */
/*  Applies both the title-substring filter (qbar_target_app) and     */
/*  the optional process-executable filter (qbar_target_exe).         */
/* ================================================================== */
static BOOL CALLBACK CatiaCheckProc(HWND hwnd, LPARAM lp)
{
    CatiaState *result = (CatiaState *)lp;
    /* Skip windows without WS_VISIBLE (e.g. background system windows) */
    if (!IsWindowVisible(hwnd)) return TRUE;
    WCHAR title[256] = {0};
    GetWindowTextW(hwnd, title, 255);
    if (!wcsstr(title, g.cfg.qbar_target_app)) return TRUE;
    /* Optional exe filter — skip window if it belongs to a different process */
    if (g.cfg.qbar_target_exe[0]) {
        WCHAR exe[MAX_NAME] = {0};
        if (!QB_GetProcessExeName(hwnd, exe, MAX_NAME)) return TRUE;
        if (_wcsicmp(exe, g.cfg.qbar_target_exe) != 0) return TRUE;
    }
    /* Passed all filters — categorise by window state */
    if (IsIconic(hwnd)) {
        /* Minimised — mark as at-least-running if not already visible */
        if (*result < CATIA_MINIMIZED) *result = CATIA_MINIMIZED;
    } else {
        *result = CATIA_VISIBLE;
        return FALSE; /* found a visible one — no need to keep looking */
    }
    return TRUE;
}

/* ================================================================== */
/*  QB_CatiaState                                                      */
/*  Scans all top-level windows and returns the aggregate CATIA state. */
/* ================================================================== */
static CatiaState QB_CatiaState(void)
{
    CatiaState state = CATIA_NONE;
    EnumWindows(CatiaCheckProc, (LPARAM)&state);
    return state;
}

/* ================================================================== */
/*  QB_UpdateVisibility                                                */
/*  Central function called from all WinEvent callbacks.              */
/*  fg_hwnd: the new foreground window (NULL when called from a       */
/*            minimize/restore event where fg is not meaningful).     */
/* ================================================================== */
static void QB_UpdateVisibility(HWND fg_hwnd)
{
    if (!g.hwnd_qbar || !IsWindow(g.hwnd_qbar)) return;

    /* No target app configured: bar is always visible when enabled,
       no topmost behaviour. */
    if (!g.cfg.qbar_target_app[0]) {
        if (g.cfg.qbar_enabled && !IsWindowVisible(g.hwnd_qbar))
            ShowWindow(g.hwnd_qbar, SW_SHOWNOACTIVATE);
        return;
    }

    CatiaState state = QB_CatiaState();

    /* Hide the bar when the target app is not open or is fully minimised. */
    if (state != CATIA_VISIBLE) {
        QB_HideTip();
        ShowWindow(g.hwnd_qbar, SW_HIDE);
        return;
    }

    /* Target has a visible window — show if enabled */
    if (g.cfg.qbar_enabled && !IsWindowVisible(g.hwnd_qbar))
        ShowWindow(g.hwnd_qbar, SW_SHOWNOACTIVATE);

    /* Update always-on-top only when we have a foreground-change event */
    if (!g.cfg.qbar_topmost_with_catia || !fg_hwnd) return;
    if (fg_hwnd == g.hwnd_qbar || fg_hwnd == g.hwnd) return; /* our own windows */

    HWND ins = (state == CATIA_VISIBLE && QB_IsTarget(fg_hwnd))
               ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g.hwnd_qbar, ins, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

/* ================================================================== */
/*  QB_WinEventProc                                                    */
/*  Shared WinEvent callback for foreground and minimize/restore       */
/*  events.  Runs in the main thread via WINEVENT_OUTOFCONTEXT.       */
/* ================================================================== */
static void CALLBACK QB_WinEventProc(HWINEVENTHOOK hook, DWORD event,
    HWND hwnd, LONG idObj, LONG idChild, DWORD thread, DWORD time)
{
    (void)hook; (void)idObj; (void)idChild; (void)thread; (void)time;
    /* Pass hwnd only for foreground events so QB_UpdateVisibility can
       use it for the topmost decision.  Minimize/restore events carry
       the window being minimised, not the new foreground window. */
    HWND fg = (event == EVENT_SYSTEM_FOREGROUND) ? hwnd : NULL;
    QB_UpdateVisibility(fg);
}

/* ================================================================== */
/*  QB_BtnRect                                                         */
/*  Fills r with the client-coordinate rect of button idx, accounting */
/*  for scroll offset and arrow areas when scroll is active.          */
/* ================================================================== */
static void QB_BtnRect(HWND hwnd, int idx, RECT *r)
{
    (void)hwnd;
    bool horiz = g.cfg.qbar_horizontal;
    bool has_arrows = (g.qbar_scroll_max > 0);
    int  arrow_off  = has_arrows ? QBAR_ARROW_W : 0;
    int  step       = QBAR_BTN_SIZE + QBAR_GAP;

    if (horiz) {
        int x = QBAR_PAD + arrow_off + idx * step - g.qbar_scroll;
        r->left   = x;
        r->top    = QBAR_PAD;
        r->right  = x + QBAR_BTN_SIZE;
        r->bottom = QBAR_PAD + QBAR_BTN_SIZE;
    } else {
        int y = QBAR_PAD + arrow_off + idx * step - g.qbar_scroll;
        r->left   = QBAR_PAD;
        r->top    = y;
        r->right  = QBAR_PAD + QBAR_BTN_SIZE;
        r->bottom = y + QBAR_BTN_SIZE;
    }
}

/* ================================================================== */
/*  QB_HitTest                                                         */
/*  Returns button index (>=0), HIT_ARROW_PREV/NEXT, or HIT_NONE.    */
/* ================================================================== */
static int QB_HitTest(HWND hwnd, int x, int y)
{
    RECT rc; GetClientRect(hwnd, &rc);
    bool horiz      = g.cfg.qbar_horizontal;
    bool has_arrows = (g.qbar_scroll_max > 0);

    if (has_arrows) {
        if (horiz) {
            if (x < QBAR_ARROW_W)              return HIT_ARROW_PREV;
            if (x > rc.right - QBAR_ARROW_W)   return HIT_ARROW_NEXT;
        } else {
            if (y < QBAR_ARROW_W)              return HIT_ARROW_PREV;
            if (y > rc.bottom - QBAR_ARROW_W)  return HIT_ARROW_NEXT;
        }
    }

    int n = QB_FavCount();
    for (int i = 0; i < n; i++) {
        RECT br; QB_BtnRect(hwnd, i, &br);
        if (x >= br.left && x < br.right && y >= br.top && y < br.bottom)
            return i;
    }
    return HIT_NONE;
}

/* ================================================================== */
/*  QB_UpdateScrollMax                                                 */
/*  Recomputes qbar_scroll_max from content vs visible area.          */
/* ================================================================== */
static void QB_UpdateScrollMax(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    bool horiz = g.cfg.qbar_horizontal;
    int  n     = QB_FavCount();
    int  step  = QBAR_BTN_SIZE + QBAR_GAP;

    /* Total logical content size */
    int content = 2 * QBAR_PAD + (n > 0 ? (n * step - QBAR_GAP) : QBAR_BTN_SIZE);

    /* Visible client dimension in scroll direction */
    int visible = horiz ? (rc.right - rc.left) : (rc.bottom - rc.top);

    /* Arrow areas eat into the drawable space */
    bool need_arrows = content > visible;
    int  drawable    = need_arrows ? visible - 2 * QBAR_ARROW_W : visible;
    if (drawable < 0) drawable = 0;

    g.qbar_scroll_max = max(0, content - drawable);
    if (g.qbar_scroll > g.qbar_scroll_max) g.qbar_scroll = g.qbar_scroll_max;
}

/* ================================================================== */
/*  QB_UpdateGeometry                                                  */
/*  Sizes and repositions the bar window to fit current favourites.   */
/* ================================================================== */
static void QB_UpdateGeometry(void)
{
    if (!g.hwnd_qbar) return;

    MONITORINFO mi = {0}; mi.cbSize = sizeof(mi);
    HMONITOR hm = MonitorFromWindow(g.hwnd_qbar, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hm, &mi);
    RECT work = mi.rcWork;

    int n    = QB_FavCount();
    int step = QBAR_BTN_SIZE + QBAR_GAP;
    bool horiz = g.cfg.qbar_horizontal;

    int content = 2 * QBAR_PAD + (n > 0 ? (n * step - QBAR_GAP) : QBAR_BTN_SIZE);
    int max_vis = horiz
        ? (work.right  - work.left) * 4 / 5
        : (work.bottom - work.top)  * 4 / 5;
    if (max_vis < QBAR_BTN_SIZE + 2 * QBAR_PAD)
        max_vis = QBAR_BTN_SIZE + 2 * QBAR_PAD;

    int vis       = min(content, max_vis);   /* client length in scroll dir */
    int fixed_dim = QBAR_BTN_SIZE + 2 * QBAR_PAD;  /* client in fixed dir */

    /* WS_BORDER adds 1px each side — window = client + 2 */
    int window_w = horiz ? (vis + 2) : (fixed_dim + 2);
    int window_h = horiz ? (fixed_dim + 2) : (vis + 2);

    /* Clamp saved position to monitor work area */
    int x = g.cfg.qbar_x, y = g.cfg.qbar_y;
    if (x + window_w > work.right)  x = work.right  - window_w;
    if (y + window_h > work.bottom) y = work.bottom - window_h;
    if (x < work.left) x = work.left;
    if (y < work.top)  y = work.top;

    SetWindowPos(g.hwnd_qbar, NULL, x, y, window_w, window_h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    QB_UpdateScrollMax(g.hwnd_qbar);
    InvalidateRect(g.hwnd_qbar, NULL, FALSE);
}

/* ================================================================== */
/*  QB_SavePos                                                         */
/* ================================================================== */
static void QB_SavePos(void)
{
    if (!g.hwnd_qbar) return;
    RECT rc; GetWindowRect(g.hwnd_qbar, &rc);
    if (rc.left == g.cfg.qbar_x && rc.top == g.cfg.qbar_y) return;
    g.cfg.qbar_x = rc.left;
    g.cfg.qbar_y = rc.top;
    Settings_Save(&g.cfg);
}

/* ================================================================== */
/*  QB_Paint                                                           */
/*  Renders the bar background, scroll arrows, and all buttons.       */
/* ================================================================== */
static void QB_Paint(HWND hwnd, HDC hdc)
{
    RECT rc; GetClientRect(hwnd, &rc);
    bool horiz      = g.cfg.qbar_horizontal;
    bool has_arrows = (g.qbar_scroll_max > 0);
    int  n          = QB_FavCount();

    /* ── Background ──────────────────────────────────────────────── */
    HBRUSH br_bg = CreateSolidBrush(COL_TOOLBAR());
    FillRect(hdc, &rc, br_bg);
    DeleteObject(br_bg);

    /* ── Border highlight ─────────────────────────────────────────── */
    HPEN pen_brd = CreatePen(PS_SOLID, 1, COL_DIVIDER());
    HPEN old_pen = SelectObject(hdc, pen_brd);
    HBRUSH old_br = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_br);
    DeleteObject(pen_brd);

    /* ── Empty state hint ─────────────────────────────────────────── */
    if (n == 0) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_SUBTEXT());
        HFONT of = SelectObject(hdc, g.font_small);
        RECT tr = rc;
        DrawTextW(hdc, L"No\nFavs", -1, &tr, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        SelectObject(hdc, of);
        return;
    }

    /* ── Scroll arrows ─────────────────────────────────────────────── */
    if (has_arrows) {
        COLORREF arrow_col = COL_SUBTEXT();
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, arrow_col);
        HFONT of = SelectObject(hdc, g.font_bold);

        RECT ar;
        if (!horiz) {
            /* ▲ at top */
            ar.left = rc.left; ar.right = rc.right;
            ar.top  = 0;       ar.bottom = QBAR_ARROW_W;
            if (g.qbar_scroll > 0) {
                SetTextColor(hdc, COL_ACCENT);
                DrawTextW(hdc, L"▲", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SetTextColor(hdc, arrow_col);
            }
            /* ▼ at bottom */
            ar.top = rc.bottom - QBAR_ARROW_W; ar.bottom = rc.bottom;
            if (g.qbar_scroll < g.qbar_scroll_max) {
                SetTextColor(hdc, COL_ACCENT);
                DrawTextW(hdc, L"▼", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SetTextColor(hdc, arrow_col);
            }
        } else {
            /* ◄ at left */
            ar.left = 0; ar.right = QBAR_ARROW_W;
            ar.top  = rc.top; ar.bottom = rc.bottom;
            if (g.qbar_scroll > 0) {
                SetTextColor(hdc, COL_ACCENT);
                DrawTextW(hdc, L"◄", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SetTextColor(hdc, arrow_col);
            }
            /* ► at right */
            ar.left = rc.right - QBAR_ARROW_W; ar.right = rc.right;
            if (g.qbar_scroll < g.qbar_scroll_max) {
                SetTextColor(hdc, COL_ACCENT);
                DrawTextW(hdc, L"►", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        SelectObject(hdc, of);
    }

    /* ── Buttons ──────────────────────────────────────────────────── */
    /* Clip to avoid drawing into arrow areas */
    HRGN clip_rgn = NULL;
    if (has_arrows) {
        RECT clip = rc;
        if (!horiz) {
            clip.top    = QBAR_ARROW_W;
            clip.bottom = rc.bottom - QBAR_ARROW_W;
        } else {
            clip.left  = QBAR_ARROW_W;
            clip.right = rc.right - QBAR_ARROW_W;
        }
        clip_rgn = CreateRectRgnIndirect(&clip);
        SelectClipRgn(hdc, clip_rgn);
    }

    for (int i = 0; i < n; i++) {
        RECT br; QB_BtnRect(hwnd, i, &br);

        /* Skip fully off-screen buttons */
        if (br.right < 0 || br.bottom < 0) continue;
        if (!horiz && (br.top > rc.bottom || br.bottom < 0)) continue;
        if ( horiz && (br.left > rc.right  || br.right  < 0)) continue;

        bool hot = (i == g.qbar_hot);

        /* Check if this button is the currently repeating script */
        int fi_btn = 0, si_btn = 0;
        QB_GetFav(i, &fi_btn, &si_btn);
        bool rep = g.repeat_mode && g.repeat_fi == fi_btn && g.repeat_si == si_btn;
        bool run = g.script_running && !g.repeat_mode && g.run_fi == fi_btn && g.run_si == si_btn;

        /* Button background (rounded rect) */
        COLORREF bg  = hot ? COL_BTN_HOT() : COL_BTN_NORM();
        COLORREF brd = rep ? COL_WARN : run ? COL_SUCCESS : (hot ? COL_ACCENT : COL_DIVIDER());
        HBRUSH br_btn = CreateSolidBrush(bg);
        HPEN   pen    = CreatePen(PS_SOLID, (rep || run) ? 2 : 1, brd);
        HPEN   op     = SelectObject(hdc, pen);
        HBRUSH ob     = SelectObject(hdc, br_btn);
        RoundRect(hdc, br.left, br.top, br.right, br.bottom, 10, 10);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(br_btn);

        /* Accent edge bar when hot, repeating, or running */
        if (hot || rep || run) {
            HBRUSH ba = CreateSolidBrush(rep ? COL_WARN : run ? COL_SUCCESS : COL_ACCENT);
            RECT   acc;
            if (!horiz) {
                acc.left   = br.left;
                acc.top    = br.top    + 8;
                acc.right  = br.left   + 3;
                acc.bottom = br.bottom - 8;
            } else {
                acc.left   = br.left   + 8;
                acc.top    = br.top;
                acc.right  = br.right  - 8;
                acc.bottom = br.top    + 3;
            }
            FillRect(hdc, &acc, ba);
            DeleteObject(ba);
        }

        /* 2-letter abbreviation from script name */
        Script *s = QB_GetFav(i, NULL, NULL);
        if (!s) continue;

        WCHAR tmp[MAX_NAME];
        wcsncpy_s(tmp, MAX_NAME, s->name, _TRUNCATE);
        Util_StripExt(tmp);

        WCHAR label[3] = {0};
        int   li = 0;
        for (int j = 0; tmp[j] && li < 2; j++) {
            WCHAR ch = tmp[j];
            if (ch != L'_' && ch != L' ' && ch != L'-')
                label[li++] = (WCHAR)towupper(ch);
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, rep ? COL_WARN : run ? COL_SUCCESS : (hot ? COL_ACCENT : COL_TEXT()));
        HFONT of = SelectObject(hdc, s_font_label);
        DrawTextW(hdc, label, -1, &br, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, of);
    }

    if (clip_rgn) {
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip_rgn);
    }
}

/* ================================================================== */
/*  QB_ShowTip                                                         */
/* ================================================================== */
static void QB_ShowTip(HWND hwnd, int idx)
{
    if (!g.hwnd_qbar_tip) return;
    Script *s = QB_GetFav(idx, NULL, NULL);
    if (!s) return;

    g.qbar_tip_idx = idx;
    if (!s->meta_loaded) Meta_Parse(s);

    int rows   = 1 + (s->meta.purpose[0] ? 1 : 0);
    int tip_h  = 2 * QBAR_TIP_PAD + rows * QBAR_TIP_ROW;
    int tip_w  = QBAR_TIP_W;

    RECT br; QB_BtnRect(hwnd, idx, &br);
    POINT tl = {br.left, br.top};
    POINT tr = {br.right, br.bottom};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &tr);

    MONITORINFO mi = {0}; mi.cbSize = sizeof(mi);
    HMONITOR hm = MonitorFromPoint(tl, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hm, &mi);
    RECT work = mi.rcWork;

    int x, y;
    if (!g.cfg.qbar_horizontal) {
        /* Vertical bar: show to the right, fallback left */
        x = tr.x + 4;
        y = tl.y;
        if (x + tip_w > work.right) x = tl.x - tip_w - 4;
        if (y + tip_h > work.bottom) y = work.bottom - tip_h;
        if (y < work.top) y = work.top;
    } else {
        /* Horizontal bar: show below, fallback above */
        x = tl.x;
        y = tr.y + 4;
        if (y + tip_h > work.bottom) y = tl.y - tip_h - 4;
        if (x + tip_w > work.right)  x = work.right - tip_w;
        if (x < work.left) x = work.left;
    }

    SetWindowPos(g.hwnd_qbar_tip, HWND_TOPMOST, x, y, tip_w, tip_h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g.hwnd_qbar_tip, NULL, FALSE);
}

/* ================================================================== */
/*  QB_HideTip                                                         */
/* ================================================================== */
static void QB_HideTip(void)
{
    g.qbar_tip_idx = -1;
    if (g.hwnd_qbar_tip) ShowWindow(g.hwnd_qbar_tip, SW_HIDE);
}

/* ================================================================== */
/*  QB_Scroll                                                          */
/*  Scrolls the bar by delta px (positive = forward/down).            */
/* ================================================================== */
static void QB_Scroll(HWND hwnd, int delta)
{
    g.qbar_scroll += delta;
    if (g.qbar_scroll < 0)                    g.qbar_scroll = 0;
    if (g.qbar_scroll > g.qbar_scroll_max)    g.qbar_scroll = g.qbar_scroll_max;
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ================================================================== */
/*  QB_ShowContextMenu                                                 */
/* ================================================================== */
static void QB_ShowContextMenu(HWND hwnd)
{
    bool has_target = g.cfg.qbar_target_app[0] != L'\0';

    HMENU hm = CreatePopupMenu();
    AppendMenu(hm, MF_STRING,    IDM_QBAR_TOGGLE,     L"Enable Quick Bar");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING,    IDM_QBAR_HORIZONTAL, L"Horizontal");
    AppendMenu(hm, MF_STRING,    IDM_QBAR_VERTICAL,   L"Vertical");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, has_target ? MF_STRING : (MF_STRING | MF_GRAYED),
               IDM_QBAR_TOPMOST, L"On Top with Target App");
    AppendMenu(hm, MF_STRING,    IDM_QBAR_SET_TARGET, L"Set Target App...");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING,    IDM_QBAR_RESET_POS,  L"Reset Position");
    AppendMenu(hm, MF_SEPARATOR, 0, NULL);
    AppendMenu(hm, MF_STRING,    IDM_REPEAT_QBAR,     L"Repeat on Double-Click");

    CheckMenuItem(hm, IDM_QBAR_TOGGLE,
        g.cfg.qbar_enabled            ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hm, IDM_QBAR_HORIZONTAL,
        g.cfg.qbar_horizontal         ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hm, IDM_QBAR_VERTICAL,
        !g.cfg.qbar_horizontal        ? MF_CHECKED : MF_UNCHECKED);
    if (has_target)
        CheckMenuItem(hm, IDM_QBAR_TOPMOST,
            g.cfg.qbar_topmost_with_catia ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hm, IDM_REPEAT_QBAR,
        g.cfg.qbar_repeat_on_dblclick ? MF_CHECKED : MF_UNCHECKED);

    POINT pt; GetCursorPos(&pt);
    /* SetForegroundWindow ensures the menu closes properly */
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hm, TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, g.hwnd, NULL);
    DestroyMenu(hm);
}

/* ================================================================== */
/*  QBarTipProc                                                        */
/*  Tooltip popup window procedure.                                   */
/* ================================================================== */
static LRESULT CALLBACK QBarTipProc(HWND hwnd, UINT msg,
                                     WPARAM wp, LPARAM lp)
{
    (void)wp; (void)lp;
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
        HBRUSH br = CreateSolidBrush(COL_TIP_BG());
        FillRect(hdc, &rc, br);
        DeleteObject(br);

        /* Border */
        HPEN pen = CreatePen(PS_SOLID, 1, COL_TIP_BORDER);
        HPEN op  = SelectObject(hdc, pen);
        HBRUSH ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);

        if (g.qbar_tip_idx < 0) { EndPaint(hwnd, &ps); return 0; }
        Script *s = QB_GetFav(g.qbar_tip_idx, NULL, NULL);
        if (!s)                  { EndPaint(hwnd, &ps); return 0; }

        SetBkMode(hdc, TRANSPARENT);
        int y = QBAR_TIP_PAD;
        RECT tr;

        /* Script name in bold (snake_case → Title Case, no extension) */
        WCHAR disp[MAX_NAME];
        wcsncpy_s(disp, MAX_NAME, s->name, _TRUNCATE);
        Util_StripExt(disp);
        Util_SnakeToTitle(disp);

        tr.left   = QBAR_TIP_PAD;
        tr.right  = rc.right - QBAR_TIP_PAD;
        tr.top    = y;
        tr.bottom = y + QBAR_TIP_ROW;
        SetTextColor(hdc, COL_TEXT());
        HFONT of = SelectObject(hdc, g.font_bold);
        DrawTextW(hdc, disp, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        y += QBAR_TIP_ROW;

        /* Purpose line in small text */
        if (s->meta.purpose[0]) {
            tr.top    = y;
            tr.bottom = y + QBAR_TIP_ROW;
            SetTextColor(hdc, COL_SUBTEXT());
            of = SelectObject(hdc, g.font_small);
            DrawTextW(hdc, s->meta.purpose, -1, &tr,
                      DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, of);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  QuickBarProc                                                       */
/*  Main window procedure for the floating quick bar.                 */
/*  VK_ESCAPE calls Repeat_Stop() and Runner_Stop() so that Escape    */
/*  cancels both repeat mode and any running background script.       */
/* ================================================================== */
static LRESULT CALLBACK QuickBarProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        g.qbar_hot     = -1;
        g.qbar_tip_idx = -1;
        g.qbar_dragging = false;
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc_raw = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        /* Double-buffered to prevent flicker */
        HDC     hdc = CreateCompatibleDC(hdc_raw);
        HBITMAP bmp = CreateCompatibleBitmap(hdc_raw, rc.right, rc.bottom);
        HBITMAP old = SelectObject(hdc, bmp);

        QB_Paint(hwnd, hdc);

        BitBlt(hdc_raw, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, old);
        DeleteObject(bmp);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    /* Make the background area act as a drag handle */
    case WM_SETCURSOR:
    {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        if (QB_HitTest(hwnd, pt.x, pt.y) == HIT_NONE) {
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            return TRUE;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = QB_HitTest(hwnd, x, y);

        if (hit == HIT_NONE) {
            /* Start drag */
            QB_HideTip();
            g.qbar_hot = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            SetCapture(hwnd);
            g.qbar_dragging = true;
            POINT sp; GetCursorPos(&sp);
            RECT  wr; GetWindowRect(hwnd, &wr);
            g.qbar_drag_ox = sp.x - wr.left;
            g.qbar_drag_oy = sp.y - wr.top;
        } else if (hit == HIT_ARROW_PREV) {
            QB_Scroll(hwnd, -(QBAR_BTN_SIZE + QBAR_GAP));
        } else if (hit == HIT_ARROW_NEXT) {
            QB_Scroll(hwnd, +(QBAR_BTN_SIZE + QBAR_GAP));
        }
        /* Button presses are handled on LBUTTONUP to avoid false triggers */
        return 0;
    }

    case WM_LBUTTONUP:
    {
        bool was_drag = g.qbar_dragging;
        if (g.qbar_dragging) {
            ReleaseCapture();
            g.qbar_dragging = false;
            QB_SavePos();
        }
        if (g.suppress_lbuttonup) {
            g.suppress_lbuttonup = false;
            return 0;
        }
        if (!was_drag) {
            int x   = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int hit = QB_HitTest(hwnd, x, y);
            if (hit >= 0) {
                int fi = 0, si = 0;
                QB_GetFav(hit, &fi, &si);
                if (g.repeat_mode) {
                    bool same = (g.repeat_fi == fi && g.repeat_si == si);
                    Repeat_Stop();
                    if (same) return 0; /* cancel repeat, don't re-run */
                }
                Runner_Run(fi, si);
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        int x   = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = QB_HitTest(hwnd, x, y);
        if (hit < 0) return 0;
        if (!g.cfg.qbar_repeat_on_dblclick) return 0;
        if (g.cfg.show_console) {
            PostStatus(L"Repeat mode not available in console mode.");
            return 0;
        }
        int fi = 0, si = 0;
        QB_GetFav(hit, &fi, &si);
        if (g.repeat_mode && g.repeat_fi == fi && g.repeat_si == si) {
            /* Double-click on the already-repeating script → toggle off */
            Repeat_Stop();
            g.suppress_lbuttonup = false;
            PostStatus(L"Repeat cancelled.");
            return 0;
        }
        /* Activate (cancels any previous repeat on a different script) */
        Repeat_Stop();
        Repeat_Start(fi, si);
        g.suppress_lbuttonup = true;
        Script *rs = QB_GetFav(hit, NULL, NULL);
        PostStatus(L"Repeat: %s  •  Esc or click to stop", rs ? rs->name : L"");
        InvalidateRect(hwnd, NULL, FALSE);
        if (g.hwnd) InvalidateRect(g.hwnd, NULL, FALSE);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (g.repeat_mode) {
                Repeat_Stop();
                PostStatus(L"Repeat cancelled.");
            }
            if (!g.cfg.show_console && g.run_process)
                Runner_Stop();
        }
        return 0;

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);

        if (g.qbar_dragging) {
            POINT sp; GetCursorPos(&sp);
            SetWindowPos(hwnd, NULL,
                sp.x - g.qbar_drag_ox, sp.y - g.qbar_drag_oy,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        int hit = QB_HitTest(hwnd, x, y);
        int btn = (hit >= 0) ? hit : HIT_NONE;
        if (btn != g.qbar_hot) {
            g.qbar_hot = btn;
            InvalidateRect(hwnd, NULL, FALSE);
            if (btn >= 0) QB_ShowTip(hwnd, btn);
            else          QB_HideTip();
        }

        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        g.qbar_hot = HIT_NONE;
        QB_HideTip();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_RBUTTONUP:
        QB_ShowContextMenu(hwnd);
        return 0;

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        QB_Scroll(hwnd, -(delta * (QBAR_BTN_SIZE + QBAR_GAP)) / WHEEL_DELTA);
        return 0;
    }

    /* Forward context menu commands to the main window's dispatcher */
    case WM_COMMAND:
        SendMessage(g.hwnd, WM_COMMAND, wp, lp);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  QuickBar_Register                                                  */
/*  Registers window classes and creates shared fonts.                */
/*  Must be called once during Window_Create before QuickBar_Create.  */
/* ================================================================== */
void QuickBar_Register(HINSTANCE hInst)
{
    WNDCLASSEX wc = {
        .cbSize        = sizeof(wc),
        .style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        .lpfnWndProc   = QuickBarProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName = QBAR_WNDCLASS
    };
    RegisterClassEx(&wc);

    WNDCLASSEX wct = {
        .cbSize        = sizeof(wct),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = QBarTipProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
        .lpszClassName = QBAR_TIPCLASS
    };
    RegisterClassEx(&wct);

    /* Large bold font for 2-letter button labels */
    s_font_label = CreateFont(
        -22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

/* ================================================================== */
/*  QuickBar_Create                                                    */
/*  Creates the floating bar window and its tooltip companion.        */
/* ================================================================== */
void QuickBar_Create(void)
{
    if (g.hwnd_qbar) return;

    HINSTANCE hInst = GetModuleHandle(NULL);

    /* Default position: near the right edge below the taskbar */
    if (!g.cfg.qbar_x && !g.cfg.qbar_y) {
        RECT work; SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
        g.cfg.qbar_x = work.right - (QBAR_BTN_SIZE + 2 * QBAR_PAD + 2) - 20;
        g.cfg.qbar_y = work.top + 60;
    }

    DWORD ex_style = WS_EX_TOOLWINDOW;
    if (g.cfg.qbar_topmost_with_catia && g.cfg.qbar_target_app[0] &&
        QB_IsTarget(GetForegroundWindow()))
        ex_style |= WS_EX_TOPMOST;

    int init_w = QBAR_BTN_SIZE + 2 * QBAR_PAD + 2;
    int init_h = QBAR_BTN_SIZE + 2 * QBAR_PAD + 2;

    g.hwnd_qbar = CreateWindowExW(
        ex_style, QBAR_WNDCLASS, L"Quick Launch Bar",
        WS_POPUP | WS_BORDER,
        g.cfg.qbar_x, g.cfg.qbar_y, init_w, init_h,
        NULL, NULL, hInst, NULL);

    if (!g.hwnd_qbar) return;

    /* Tooltip companion – always topmost so it floats above the bar */
    g.hwnd_qbar_tip = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        QBAR_TIPCLASS, NULL,
        WS_POPUP | WS_BORDER,
        0, 0, QBAR_TIP_W, 60,
        NULL, NULL, hInst, NULL);

    Window_ApplyDarkMode(g.hwnd_qbar);
    if (g.hwnd_qbar_tip) Window_ApplyDarkMode(g.hwnd_qbar_tip);

    /* Hook foreground changes (topmost) and minimize/restore (hide/show) */
    s_event_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        NULL, QB_WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT);
    s_minimize_hook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND,
        NULL, QB_WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT);

    QB_UpdateGeometry();

    /* Show only if enabled AND (no target set OR target has a visible window) */
    if (g.cfg.qbar_enabled &&
        (!g.cfg.qbar_target_app[0] || QB_CatiaState() == CATIA_VISIBLE))
        ShowWindow(g.hwnd_qbar, SW_SHOWNOACTIVATE);
}

/* ================================================================== */
/*  QuickBar_Destroy                                                   */
/*  Destroys the bar and tooltip windows and unhooks the event hook.  */
/*  Does NOT unregister window classes or delete s_font_label —       */
/*  those are cleaned up only when the application exits.             */
/* ================================================================== */
void QuickBar_Destroy(void)
{
    if (s_event_hook)   { UnhookWinEvent(s_event_hook);   s_event_hook   = NULL; }
    if (s_minimize_hook){ UnhookWinEvent(s_minimize_hook); s_minimize_hook = NULL; }
    QB_HideTip();
    if (g.hwnd_qbar_tip) { DestroyWindow(g.hwnd_qbar_tip); g.hwnd_qbar_tip = NULL; }
    if (g.hwnd_qbar)     { DestroyWindow(g.hwnd_qbar);     g.hwnd_qbar     = NULL; }
    if (s_font_label)    { DeleteObject(s_font_label);      s_font_label    = NULL; }
}

/* ================================================================== */
/*  QuickBar_Show                                                      */
/* ================================================================== */
void QuickBar_Show(bool show)
{
    if (!g.hwnd_qbar) return;
    if (show) {
        /* When no target is configured, always show.
           Otherwise only show if the target has a visible window. */
        if (!g.cfg.qbar_target_app[0] || QB_CatiaState() == CATIA_VISIBLE)
            ShowWindow(g.hwnd_qbar, SW_SHOWNOACTIVATE);
        /* else: bar will appear automatically when the target app becomes visible */
    } else {
        QB_HideTip();
        ShowWindow(g.hwnd_qbar, SW_HIDE);
    }
}

/* ================================================================== */
/*  QuickBar_Rebuild                                                   */
/*  Called after the Favourites list changes.  Resets scroll and      */
/*  resizes the bar to match the new content.                         */
/* ================================================================== */
void QuickBar_Rebuild(void)
{
    if (!g.hwnd_qbar) return;
    g.qbar_scroll = 0;
    g.qbar_hot    = -1;
    QB_HideTip();
    QB_UpdateGeometry();
}

/* ================================================================== */
/*  QuickBar_OnThemeChange                                             */
/* ================================================================== */
void QuickBar_OnThemeChange(void)
{
    if (g.hwnd_qbar) {
        Window_ApplyDarkMode(g.hwnd_qbar);
        InvalidateRect(g.hwnd_qbar, NULL, FALSE);
    }
    if (g.hwnd_qbar_tip) {
        Window_ApplyDarkMode(g.hwnd_qbar_tip);
        InvalidateRect(g.hwnd_qbar_tip, NULL, FALSE);
    }
}

/* ================================================================== */
/*  QuickBar_SetTopmost                                                */
/* ================================================================== */
void QuickBar_SetTopmost(bool topmost)
{
    if (!g.hwnd_qbar) return;
    SetWindowPos(g.hwnd_qbar, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

/* ================================================================== */
/*  QB_TargetAppDlgProc                                                */
/*  Dialog procedure for IDD_QBAR_TARGET (309).                       */
/*  Lets the user set the window-title substring (qbar_target_app)    */
/*  and the optional process executable name (qbar_target_exe).       */
/* ================================================================== */
static INT_PTR CALLBACK QB_TargetAppDlgProc(HWND hwnd, UINT msg,
                                              WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET,     g.cfg.qbar_target_app);
        SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE, g.cfg.qbar_target_exe);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_BROWSE_TARGET_EXE:
        {
            WCHAR path[MAX_APPPATH] = {0};
            OPENFILENAME ofn = {
                .lStructSize = sizeof(ofn), .hwndOwner = hwnd,
                .lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0",
                .lpstrFile = path, .nMaxFile = MAX_APPPATH,
                .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                .lpstrTitle = L"Select Target Application"
            };
            if (GetOpenFileName(&ofn)) {
                WCHAR *slash = wcsrchr(path, L'\\');
                SetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE, slash ? slash + 1 : path);
            }
            return TRUE;
        }
        case IDOK:
        {
            /* Helper lambda-equivalent: trim spaces from a buffer in-place */
            #define TRIM(buf) do { \
                WCHAR *_s = (buf); \
                while (*_s == L' ') _s++; \
                WCHAR *_e = _s + wcslen(_s); \
                while (_e > _s && *(_e-1) == L' ') _e--; \
                *_e = L'\0'; \
                wcsncpy_s((buf), MAX_NAME, _s, _TRUNCATE); \
            } while (0)

            WCHAR buf[MAX_NAME] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET, buf, MAX_NAME);
            TRIM(buf);
            wcsncpy_s(g.cfg.qbar_target_app, MAX_NAME, buf, _TRUNCATE);

            GetDlgItemText(hwnd, IDC_EDIT_QBAR_TARGET_EXE, buf, MAX_NAME);
            TRIM(buf);
            wcsncpy_s(g.cfg.qbar_target_exe, MAX_NAME, buf, _TRUNCATE);
            #undef TRIM

            Settings_Save(&g.cfg);
            /* Re-evaluate visibility with the new target settings */
            QB_UpdateVisibility(NULL);
            /* If topmost is on but title target is now empty, remove topmost */
            if (!g.cfg.qbar_target_app[0])
                QuickBar_SetTopmost(false);
            EndDialog(hwnd, IDOK);
            break;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/*  QuickBar_ShowTargetDlg                                             */
/*  Opens the Set Target Application dialog.                          */
/* ================================================================== */
void QuickBar_ShowTargetDlg(void)
{
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_QBAR_TARGET),
              g.hwnd, QB_TargetAppDlgProc);
}
