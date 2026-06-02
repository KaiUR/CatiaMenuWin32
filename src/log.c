/*
 * log.c  -  Script output log window.
 *
 * Displays stdout/stderr captured from scripts running in non-console
 * (background) mode.  Each script run is framed by:
 *   === ScriptName  (HH:MM:SS) ===          ← header (blue)
 *   --- Finished successfully. ---           ← footer (green / amber / red)
 *
 * Output arrives via WM_LOG_OUTPUT posted by LogReader_Thread in runner.c.
 *
 * Text is ALWAYS buffered in s_buf regardless of whether the log window
 * is currently open.  When Log_Show() creates the window the buffer is
 * pre-populated so no output is ever lost.
 *
 * Pattern-based line coloring (applied via EM_SETCHARFORMAT after each
 * insert):
 *   === ... ===            header lines          → accent blue
 *   --- ... successfully   completion footers    → green
 *   --- ... stopped        user-stop footers     → amber
 *   --- ...                other footers         → red
 *   contains error / exception / traceback / fatal → red
 *   contains warning                             → amber
 *
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <richedit.h>

/* ------------------------------------------------------------------ */
/*  Module-level state                                                  */
/* ------------------------------------------------------------------ */
#define LOG_WNDCLASS L"CMW32LogWnd"
static HMODULE s_rich = NULL; /* RICHED20.DLL handle */
static HFONT s_font = NULL; /* Consolas font for the RichEdit */

/* Session text buffer — always appended even when the window is closed */
static WCHAR *s_buf = NULL; /* heap-allocated accumulated log text */
static size_t s_buf_len = 0; /* chars used (not including null terminator) */
static size_t s_buf_cap = 0; /* chars allocated (including null terminator) */

/* ------------------------------------------------------------------ */
/*  Child control IDs and layout                                       */
/* ------------------------------------------------------------------ */
#define LOG_ID_RICHEDIT 1 /* RichEdit control                       */
#define LOG_ID_BTN_SAVE 2 /* "Save log..." button                   */
#define LOG_ID_BTN_CLEAR 3 /* "Clear" button                         */
#define LOG_BTN_H 28 /* height of the button bar at the bottom */

/* ------------------------------------------------------------------ */
/*  Right-click context menu item IDs (local to RichEdit subclass)    */
/* ------------------------------------------------------------------ */
#define CTX_COPY 1
#define CTX_SEL_ALL 2
#define CTX_CLEAR 3

/* ------------------------------------------------------------------ */
/*  Highlight colours (theme-aware)                                    */
/* ------------------------------------------------------------------ */
static inline COLORREF LOG_COL_ERROR(void)
{
    return g.dark_mode ? RGB(255, 100, 100) : RGB(180, 0, 0);
}
static inline COLORREF LOG_COL_WARN(void)
{
    return g.dark_mode ? RGB(255, 200, 60) : RGB(150, 90, 0);
}
static inline COLORREF LOG_COL_SUCCESS(void)
{
    return g.dark_mode ? RGB(100, 220, 100) : RGB(0, 128, 0);
}

/* ================================================================== */
/*  Buf_Append  (static)                                               */
/*  Purpose: Appends text to the module-level session buffer,          */
/*           growing the heap allocation as needed.                    */
/*  In:  text — wide string to append                                  */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Buf_Append(const WCHAR *text)
{
    if (!text) return;
    size_t add = wcslen(text);
    if (add == 0) return;

    size_t need = s_buf_len + add + 1;
    if (need > s_buf_cap)
    {
        size_t new_cap = need * 2;
        if (new_cap < 4096) new_cap = 4096;
        WCHAR *nb = (WCHAR *)realloc(s_buf, new_cap * sizeof(WCHAR));
        if (!nb) return;
        s_buf = nb;
        s_buf_cap = new_cap;
    }
    memcpy(s_buf + s_buf_len, text, (add + 1) * sizeof(WCHAR));
    s_buf_len += add;
}

/* ================================================================== */
/*  Log_GetRichEdit  (static)                                          */
/*  Purpose: Returns the HWND of the RichEdit child of g.hwnd_log, or  */
/*           NULL when the log window does not exist.                  */
/* ================================================================== */
static HWND Log_GetRichEdit(void)
{
    if (!g.hwnd_log || !IsWindow(g.hwnd_log)) return NULL;
    return GetDlgItem(g.hwnd_log, LOG_ID_RICHEDIT);
}

/* ================================================================== */
/*  LineContainsI  (static)                                            */
/*  Purpose: Case-insensitive substring search for short ASCII         */
/*           keywords within a line of script output.                  */
/*  In:  line — null-terminated line text                              */
/*       kw   — keyword to search for (ASCII, lowercase expected)      */
/*  Out: true if found; false otherwise                                 */
/* ================================================================== */
static bool LineContainsI(const WCHAR *line, const WCHAR *kw)
{
    size_t klen = wcslen(kw);
    for (; *line; line++)
    {
        if (_wcsnicmp(line, kw, klen) == 0) return true;
    }
    return false;
}

/* ================================================================== */
/*  Log_LineColor  (static)                                            */
/*  Purpose: Determines the highlight colour for a single log line     */
/*           by pattern matching against known keywords and prefixes.  */
/*  In:  line — null-terminated line text (no trailing newline)        */
/*  Out: COLORREF to apply, or 0 to use the default theme text colour  */
/* ================================================================== */
static COLORREF Log_LineColor(const WCHAR *line)
{
    /* Header lines produced by Log_AddHeader */
    if (_wcsnicmp(line, L"===", 3) == 0) return COL_ACCENT;

    /* Completion footer lines produced by main.c WM_SCRIPT_STOPPED */
    if (_wcsnicmp(line, L"---", 3) == 0)
    {
        if (LineContainsI(line, L"successfully")) return LOG_COL_SUCCESS();
        if (LineContainsI(line, L"stopped")) return LOG_COL_WARN();
        return LOG_COL_ERROR(); /* "Exited with code N" and any other --- lines */
    }

    /* Python runtime output patterns */
    if (LineContainsI(line, L"traceback")) return LOG_COL_ERROR();
    if (LineContainsI(line, L"exception")) return LOG_COL_ERROR();
    if (LineContainsI(line, L"fatal")) return LOG_COL_ERROR();
    if (LineContainsI(line, L"error")) return LOG_COL_ERROR();
    if (LineContainsI(line, L"warning")) return LOG_COL_WARN();

    return 0; /* 0 = no override; caller skips EM_SETCHARFORMAT */
}

/* ================================================================== */
/*  Log_ColorizeNewLines  (static)                                     */
/*  Purpose: Walks lines [first_line .. total) in the RichEdit and     */
/*           applies per-line highlight colours via EM_SETCHARFORMAT.  */
/*           Leaves the selection at the document end when finished.   */
/*  In:  hRE        — RichEdit control handle                          */
/*       first_line — first line index to process (0-based)            */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Log_ColorizeNewLines(HWND hRE, int first_line)
{
    int total = (int)SendMessage(hRE, EM_GETLINECOUNT, 0, 0);

    for (int ln = first_line; ln < total; ln++)
    {
        int idx = (int)SendMessage(hRE, EM_LINEINDEX, (WPARAM)ln, 0);
        if (idx < 0) continue;
        int len = (int)SendMessage(hRE, EM_LINELENGTH, (WPARAM)idx, 0);
        if (len <= 0) continue;

        /* EM_GETLINE: first WORD of buffer = max chars to copy */
        WCHAR buf[512];
        int cap = len < 510 ? len : 510;
        *(WORD *)buf = (WORD)cap;
        int n = (int)SendMessage(hRE, EM_GETLINE, (WPARAM)ln, (LPARAM)buf);
        buf[n] = L'\0';

        COLORREF color = Log_LineColor(buf);
        if (color == 0) continue; /* default colour — skip */

        SendMessage(hRE, EM_SETSEL, (WPARAM)idx, (LPARAM)(idx + len));
        CHARFORMAT2W cf = {0};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        cf.dwEffects = 0; /* clear CFE_AUTOCOLOR so crTextColor is used */
        SendMessage(hRE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }

    /* Restore caret to end so the next EM_REPLACESEL appends correctly */
    int doc_len = (int)SendMessage(hRE, WM_GETTEXTLENGTH, 0, 0);
    SendMessage(hRE, EM_SETSEL, (WPARAM)doc_len, (LPARAM)doc_len);
}

/* ================================================================== */
/*  Log_ApplyColors  (static)                                          */
/*  Purpose: Applies the current theme background and default text     */
/*           colour to the RichEdit, then re-runs the line colorizer   */
/*           over all content.  Called on creation and theme change.   */
/*  In:  hRE — handle to the RichEdit control                          */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Log_ApplyColors(HWND hRE)
{
    if (!hRE) return;

    SendMessage(hRE, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_BG());

    /* Reset all text to the current theme colour first */
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = COL_TEXT();
    cf.dwEffects = 0; /* clear CFE_AUTOCOLOR */
    SendMessage(hRE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    /* Re-apply per-line highlights over the reset baseline */
    Log_ColorizeNewLines(hRE, 0);
}

/* ================================================================== */
/*  Log_Append                                                          */
/*  Purpose: Always appends text to the session buffer.  If the log    */
/*           window is open it also inserts into the RichEdit, applies  */
/*           line-level syntax highlighting, and scrolls to the end.   */
/*           Call from the UI thread only (invoked by WM_LOG_OUTPUT).  */
/*  In:  text — null-terminated wide string to append                  */
/*  Out: (void)                                                         */
/* ================================================================== */
void Log_Append(const WCHAR *text)
{
    if (!text) return;
    Buf_Append(text);

    HWND hRE = Log_GetRichEdit();
    if (!hRE) return;

    /* Record insertion point so colorizer knows which lines are new */
    int old_len = (int)SendMessage(hRE, WM_GETTEXTLENGTH, 0, 0);
    int first_ln = (int)SendMessage(hRE, EM_LINEFROMCHAR, (WPARAM)old_len, 0);

    SendMessage(hRE, EM_SETSEL, (WPARAM)old_len, (LPARAM)old_len);
    SendMessage(hRE, EM_REPLACESEL, FALSE, (LPARAM)text);

    Log_ColorizeNewLines(hRE, first_ln); /* colorize from the affected line onwards */
    SendMessage(hRE, WM_VSCROLL, SB_BOTTOM, 0);
}

/* ================================================================== */
/*  Log_AddHeader                                                       */
/*  Purpose: Inserts a separator/header line before a new script run:  */
/*           "=== ScriptName  (HH:MM:SS) ===\r\n"                      */
/*  In:  script_name — display name of the script about to run         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Log_AddHeader(const WCHAR *script_name)
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    WCHAR header[256];
    _snwprintf_s(header, 255, _TRUNCATE,
                 L"=== %s  (%02d:%02d:%02d) ===\r\n",
                 script_name ? script_name : L"Script",
                 st.wHour, st.wMinute, st.wSecond);

    Log_Append(header);
}

/* ================================================================== */
/*  Log_Clear                                                           */
/*  Purpose: Clears both the session buffer and the RichEdit content.  */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Log_Clear(void)
{
    s_buf_len = 0;
    if (s_buf) s_buf[0] = L'\0';

    HWND hRE = Log_GetRichEdit();
    if (hRE) SetWindowText(hRE, L"");
}

/* ================================================================== */
/*  Log_OnThemeChange                                                   */
/*  Purpose: Re-applies dark/light colours and re-runs the syntax      */
/*           highlighter over all existing content after a theme       */
/*           change.  Called from Window_ApplyThemeToChildren.         */
/*  In:  (reads g.dark_mode via COL_* helpers)                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Log_OnThemeChange(void)
{
    if (!g.hwnd_log || !IsWindow(g.hwnd_log)) return;
    Window_ApplyDarkMode(g.hwnd_log);
    Log_ApplyColors(Log_GetRichEdit()); /* resets base colour then re-highlights */
    InvalidateRect(g.hwnd_log, NULL, TRUE);
}

/* ================================================================== */
/*  Log_SaveToFile  (static)                                           */
/*  Purpose: Opens a Save-As dialog and writes the current session     */
/*           buffer to disk as a UTF-8 text file (with BOM).           */
/*  In:  hwnd — owner window for the dialog                            */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Log_SaveToFile(HWND hwnd)
{
    if (!s_buf || s_buf_len == 0)
    {
        MessageBox(hwnd, L"The log is empty — nothing to save.",
                   L"Save Log", MB_ICONINFORMATION | MB_OK);
        return;
    }

    WCHAR path[MAX_APPPATH] = {0};
    wcsncpy_s(path, MAX_APPPATH, L"script_log.txt", _TRUNCATE);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Text Files\0*.txt\0Log Files\0*.log\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_APPPATH;
    ofn.lpstrDefExt = L"txt";
    ofn.lpstrTitle = L"Save Log";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    /* Convert buffer to UTF-8 */
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, s_buf, (int)s_buf_len,
                                       NULL, 0, NULL, NULL);
    char *utf8 = (char *)malloc((size_t)utf8_len + 3); /* +3 for UTF-8 BOM */
    if (!utf8)
    {
        MessageBox(hwnd, L"Out of memory.", L"Save Log", MB_ICONERROR | MB_OK);
        return;
    }

    /* UTF-8 BOM (EF BB BF) so the file opens correctly in Notepad etc. */
    utf8[0] = (char)0xEF;
    utf8[1] = (char)0xBB;
    utf8[2] = (char)0xBF;
    WideCharToMultiByte(CP_UTF8, 0, s_buf, (int)s_buf_len,
                        utf8 + 3, utf8_len, NULL, NULL);

    FILE *f = NULL;
    if (_wfopen_s(&f, path, L"wb") == 0 && f)
    {
        fwrite(utf8, 1, (size_t)utf8_len + 3, f);
        fclose(f);
        MessageBox(hwnd, L"Log saved successfully.", L"Save Log",
                   MB_ICONINFORMATION | MB_OK);
    }
    else
    {
        MessageBox(hwnd, L"Could not write to the selected file.", L"Save Log",
                   MB_ICONERROR | MB_OK);
    }
    free(utf8);
}

/* ================================================================== */
/*  RichEditSubclassProc  (static)                                     */
/*  Purpose: Subclass procedure for the RichEdit child.  Intercepts   */
/*           WM_CONTEXTMENU to show a custom right-click menu with     */
/*           Copy, Select All, and Clear entries.  All other messages  */
/*           are passed to the original RichEdit procedure.            */
/*  In:  hwnd — RichEdit handle                                        */
/*       msg  — Windows message                                        */
/*       wp, lp — message parameters                                   */
/*       uid  — subclass ID (unused)                                   */
/*       data — reference data (unused)                                */
/*  Out: LRESULT                                                        */
/* ================================================================== */
static LRESULT CALLBACK RichEditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uid, DWORD_PTR data)
{
    (void)uid;
    (void)data;

    if (msg == WM_CONTEXTMENU)
    {
        /* Resolve screen coordinates (keyboard menu sends -1,-1) */
        POINT pt = {(int)(short)LOWORD(lp), (int)(short)HIWORD(lp)};
        if (pt.x == -1 && pt.y == -1) GetCursorPos(&pt);

        /* Enable Copy only when there is an active selection */
        CHARRANGE cr = {0};
        SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&cr);
        bool has_sel = (cr.cpMin != cr.cpMax);

        HMENU hm = CreatePopupMenu();
        AppendMenuW(hm, MF_STRING | (has_sel ? 0 : MF_GRAYED),
                    CTX_COPY, L"Copy\tCtrl+C");
        AppendMenuW(hm, MF_STRING, CTX_SEL_ALL, L"Select All\tCtrl+A");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hm, MF_STRING, CTX_CLEAR, L"Clear log");

        int cmd = TrackPopupMenu(hm,
                                 TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                 pt.x, pt.y, 0, GetParent(hwnd), NULL);
        DestroyMenu(hm);

        switch (cmd)
        {
        case CTX_COPY:
            SendMessage(hwnd, WM_COPY, 0, 0);
            break;
        case CTX_SEL_ALL:
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            break;
        case CTX_CLEAR:
            Log_Clear();
            break;
        }
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  LogWndProc  (static)                                               */
/*  Purpose: Window procedure for the modeless log window.  Creates   */
/*           the RichEdit child on WM_CREATE with theme colours, and   */
/*           handles resize, background erase, and cleanup.            */
/*  In:  hwnd — log window handle                                      */
/*       msg  — Windows message                                        */
/*       wp, lp — message parameters                                   */
/*  Out: LRESULT                                                        */
/* ================================================================== */
static LRESULT CALLBACK LogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HINSTANCE hinst = GetModuleHandle(NULL);

        /* RichEdit — leaves LOG_BTN_H pixels for the button bar */
        HWND hRE = CreateWindowExW(
            0, RICHEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            0, 0, w, h - LOG_BTN_H,
            hwnd, (HMENU)LOG_ID_RICHEDIT, hinst, NULL);

        if (hRE)
        {
            if (s_font) SendMessage(hRE, WM_SETFONT, (WPARAM)s_font, FALSE);
            SendMessage(hRE, EM_AUTOURLDETECT, FALSE, 0);
            Log_ApplyColors(hRE);
            SetWindowSubclass(hRE, RichEditSubclassProc, 0, 0); /* context menu */
        }

        /* Button bar — use default GUI font so buttons match system style */
        HFONT hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        int btn_y = h - LOG_BTN_H + 4;

        HWND hSave = CreateWindowExW(0, L"BUTTON", L"Save log...",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     6, btn_y, 90, 20, hwnd, (HMENU)LOG_ID_BTN_SAVE, hinst, NULL);
        HWND hClr = CreateWindowExW(0, L"BUTTON", L"Clear",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                    100, btn_y, 56, 20, hwnd, (HMENU)LOG_ID_BTN_CLEAR, hinst, NULL);

        if (hSave) SendMessage(hSave, WM_SETFONT, (WPARAM)hUiFont, FALSE);
        if (hClr) SendMessage(hClr, WM_SETFONT, (WPARAM)hUiFont, FALSE);
        (void)hUiFont; /* font set above; owner-draw uses g.font_ui via Paint_ToolbarButton */

        return 0;
    }

    case WM_ERASEBKGND:
    {
        /* Fill any area not yet covered by the RichEdit (visible briefly during resize) */
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(COL_BG());
        FillRect((HDC)wp, &rc, br);
        DeleteObject(br);
        return 1;
    }

    case WM_SIZE:
    {
        int w = (int)LOWORD(lp), h = (int)HIWORD(lp);
        HWND hRE = GetDlgItem(hwnd, LOG_ID_RICHEDIT);
        HWND hSave = GetDlgItem(hwnd, LOG_ID_BTN_SAVE);
        HWND hClr = GetDlgItem(hwnd, LOG_ID_BTN_CLEAR);
        if (hRE)
            SetWindowPos(hRE, NULL, 0, 0, w, h - LOG_BTN_H,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        int btn_y = h - LOG_BTN_H + 4;
        if (hSave) SetWindowPos(hSave, NULL, 6, btn_y, 90, 20, SWP_NOZORDER | SWP_NOACTIVATE);
        if (hClr) SetWindowPos(hClr, NULL, 100, btn_y, 56, 20, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlType == ODT_BUTTON)
            Paint_ToolbarButton(dis);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case LOG_ID_BTN_SAVE:
            Log_SaveToFile(hwnd);
            break;
        case LOG_ID_BTN_CLEAR:
            Log_Clear();
            break;
        }
        return 0;

    case WM_DESTROY:
        g.hwnd_log = NULL;
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ================================================================== */
/*  Log_Show                                                            */
/*  Purpose: Creates the log window on first call (registering the     */
/*           class and loading RICHED20 if needed).  On subsequent     */
/*           calls simply brings the existing window to the foreground. */
/*           When the window is newly created it is pre-populated from  */
/*           s_buf with syntax highlighting so buffered history is     */
/*           immediately visible.                                       */
/*  In:  (none)                                                         */
/*  Out: (void — g.hwnd_log is set on successful creation)             */
/* ================================================================== */
void Log_Show(void)
{
    if (g.hwnd_log && IsWindow(g.hwnd_log))
    {
        ShowWindow(g.hwnd_log, SW_RESTORE);
        SetForegroundWindow(g.hwnd_log);
        return;
    }

    if (!s_rich)
    {
        s_rich = LoadLibraryW(L"RICHED20.DLL");
        if (!s_rich) return;
    }

    if (!s_font)
    {
        s_font = CreateFont(
            -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LogWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); /* WM_ERASEBKGND fills theme colour */
    wc.lpszClassName = LOG_WNDCLASS;
    RegisterClassEx(&wc); /* subsequent calls fail silently — safe to call every time */

    g.hwnd_log = CreateWindowExW(
        0, LOG_WNDCLASS, L"Script Output Log",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 460,
        g.hwnd, NULL, GetModuleHandle(NULL), NULL);

    if (!g.hwnd_log) return;

    Window_ApplyDarkMode(g.hwnd_log); /* match title bar chrome to app theme */

    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));
    if (hIcon)
    {
        SendMessage(g.hwnd_log, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(g.hwnd_log, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    /* Pre-populate from buffer and apply syntax highlighting */
    if (s_buf && s_buf_len > 0)
    {
        HWND hRE = GetDlgItem(g.hwnd_log, 1);
        if (hRE)
        {
            SendMessage(hRE, WM_SETTEXT, 0, (LPARAM)s_buf);
            Log_ColorizeNewLines(hRE, 0); /* colorize the pre-loaded history */
            SendMessage(hRE, WM_VSCROLL, SB_BOTTOM, 0);
        }
    }

    ShowWindow(g.hwnd_log, SW_SHOW);
    UpdateWindow(g.hwnd_log);
}

/* ================================================================== */
/*  Log_Destroy                                                         */
/*  Purpose: Destroys the log window if open and releases all module   */
/*           resources.  Called from WM_DESTROY in main.c.             */
/*  In:  (none)                                                         */
/*  Out: (void — g.hwnd_log NULL, s_buf freed, font/DLL released)      */
/* ================================================================== */
void Log_Destroy(void)
{
    if (g.hwnd_log && IsWindow(g.hwnd_log))
        DestroyWindow(g.hwnd_log);
    g.hwnd_log = NULL;

    if (s_buf)
    {
        free(s_buf);
        s_buf = NULL;
        s_buf_len = 0;
        s_buf_cap = 0;
    }
    if (s_font)
    {
        DeleteObject(s_font);
        s_font = NULL;
    }
    if (s_rich)
    {
        FreeLibrary(s_rich);
        s_rich = NULL;
    }
}
