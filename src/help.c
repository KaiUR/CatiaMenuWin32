/*
 * help.c  -  In-app help window with TreeView contents and RichEdit display.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"
#include <richedit.h>
#include <commctrl.h>

/* ================================================================== */
/*  Help topic IDs                                                      */
/* ================================================================== */
typedef enum {
    HELP_GETTING_STARTED = 0,
    HELP_INTERFACE,
    HELP_RUNNING_SCRIPTS,
    HELP_SETTINGS,
    HELP_SOURCES,
    HELP_FAVOURITES,
    HELP_DETAILS_NOTES,
    HELP_SORT_HIDE,
    HELP_UPDATE_DEPS,
    HELP_KEYBOARD,
    HELP_TROUBLESHOOTING,
    HELP_TOPIC_COUNT
} HelpTopic;

/* ================================================================== */
/*  RTF content per topic                                               */
/* ================================================================== */
static const char *Help_GetRTF(HelpTopic topic)
{
    switch (topic)
    {
    case HELP_GETTING_STARTED:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Getting Started}\\par\\par"
            "{\\b Requirements}\\par"
            "\\bullet  Windows 10 or later\\par"
            "\\bullet  Python 3.9+ installed and in your PATH\\par"
            "\\bullet  PyCATIA: {\\f1\\cf3 pip install pycatia}\\par"
            "\\bullet  CATIA V5 must be running for scripts that interact with it\\par\\par"
            "{\\b Installation}\\par"
            "1. Download {\\b CatiaMenuWin32.exe} from the GitHub releases page.\\par"
            "2. Place it anywhere on your machine \\emdash no installer required.\\par"
            "3. Double-click to run.\\par\\par"
            "{\\b First Launch}\\par"
            "On first launch the app will:\\par"
            "\\bullet  Create {\\f1\\cf3 %APPDATA%\\\\CatiaMenuWin32\\\\} to store settings and scripts.\\par"
            "\\bullet  Auto-detect your Python installation.\\par"
            "\\bullet  Sync scripts from the built-in KaiUR/Pycatia_Scripts repository.\\par"
            "\\bullet  Display the scripts as clickable buttons organised by tab.\\par\\par"
            "By default the app starts with Windows and minimizes to the system tray. "
            "Double-click the tray icon to restore it.\\par"
            "}";

    case HELP_INTERFACE:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 The Interface}\\par\\par"
            "{\\b Toolbar}\\par\\par"
            "\\bullet  {\\b \\u9776?  Menu} \\emdash access all app functions\\par"
            "\\bullet  {\\b \\u8635?  Refresh} \\emdash re-sync scripts from all sources\\par"
            "\\bullet  {\\b \\u9881?  Settings} \\emdash open the settings dialog\\par"
            "\\bullet  {\\b \\u8615?  Deps} \\emdash install/update Python dependencies\\par\\par"
            "{\\b Search Bar}\\par\\par"
            "The filter bar below the toolbar filters scripts in real time by name or purpose. "
            "Type any text to filter, clear it to show all scripts.\\par\\par"
            "{\\b Tab Bar}\\par\\par"
            "Each tab corresponds to a script folder. Click a tab to switch categories. "
            "When more tabs exist than fit, {\\b \\u9668?  } and {\\b \\u9658?  } arrows appear. "
            "Use the mouse wheel over the tab bar to scroll.\\par\\par"
            "{\\b Script Buttons}\\par\\par"
            "\\bullet  {\\b Left-click} the main area to run the script.\\par"
            "\\bullet  {\\b Hover} the {\\b i} badge on the right to see script info in a tooltip.\\par"
            "\\bullet  {\\b Right-click} any button to see the full context menu.\\par\\par"
            "{\\b Status Bar}\\par\\par"
            "Shows sync progress, script launch status, and exit codes.\\par"
            "}";

    case HELP_RUNNING_SCRIPTS:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Running Scripts}\\par\\par"
            "\\pard\\f0\\fs20\\cf2 Click any script button to run it. The app will:\\par"
            "  1. Verify the script SHA hash against GitHub to confirm it has not been tampered with.\\par"
            "  2. Launch Python with the script path.\\par"
            "  3. Show the status in the status bar.\\par\\par"
            "{\\b Console Options}\\par\\par"
            "Configurable in Settings:\\par\\par"
            "\\bullet  {\\b Show console} \\emdash  opens a visible Python console when the script runs.\\par"
            "\\bullet  {\\b Keep console open} \\emdash  window stays open after the script finishes "
            "({\\f1\\cf3 cmd /k} mode) so you can read output and errors.\\par\\par"
            "Without Show console, scripts run silently in the background.\\par\\par"
            "{\\b Run with Arguments}\\par\\par"
            "Right-click any script and select {\\b Run with Arguments...} to pass custom "
            "command line arguments to the script.\\par\\par"
            "{\\b Download Before Run}\\par\\par"
            "Enable {\\b Always download latest before running} in Settings to always fetch "
            "the newest version of a script before execution.\\par"
            "}";

    case HELP_SETTINGS:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Settings}\\par\\par"
            "Open via {\\b Menu \\u8594?  File \\u8594?  Settings...} or the toolbar button.\\par\\par"
            "{\\b Python Interpreter}\\par\\par"
            "Full path to {\\f1\\cf3 python.exe}. Click Browse to locate it. "
            "If blank, the app auto-detects Python from your PATH.\\par\\par"
            "{\\b Script Cache Folder}\\par\\par"
            "Where downloaded scripts are stored. Defaults to "
            "{\\f1\\cf3 %APPDATA%\\\\CatiaMenuWin32\\\\scripts}.\\par\\par"
            "{\\b Sync & Updates}\\par\\par"
            "\\bullet  {\\b Sync on startup} \\emdash download latest scripts when the app starts.\\par"
            "\\bullet  {\\b Auto-refresh interval} \\emdash sync in the background every N hours (0 = off, default 6).\\par"
            "\\bullet  {\\b Check for updates} \\emdash notify when a newer app version is available.\\par"
            "\\bullet  {\\b Auto-install updates} \\emdash download and install new versions automatically.\\par\\par"
            "{\\b Console Options}\\par\\par"
            "\\bullet  {\\b Show console} \\emdash open a visible console when running scripts.\\par"
            "\\bullet  {\\b Keep console open} \\emdash keep console open after script finishes.\\par"
            "\\bullet  {\\b Keep Deps console open} \\emdash keep dependency install window open.\\par\\par"
            "{\\b GitHub Token}\\par\\par"
            "Optional Personal Access Token. Raises API rate limit from 60 to 5000 req/hr. "
            "Required for private repositories.\\par\\par"
            "{\\b Window}\\par\\par"
            "\\bullet  {\\b Always on Top} \\emdash keep window above CATIA.\\par"
            "\\bullet  {\\b Minimize to Tray} \\emdash hide to system tray on minimize.\\par"
            "\\bullet  {\\b Start with Windows} \\emdash launch automatically at login.\\par"
            "\\bullet  {\\b Start Minimized} \\emdash start hidden in the tray.\\par"
            "\\bullet  {\\b Theme} \\emdash Dark, Light, or follow Windows setting.\\par\\par"
            "{\\b Reset to Defaults}\\par\\par"
            "The button at the bottom left resets all settings to their defaults. "
            "Sources (extra repos and local folders) are not affected.\\par"
            "}";

    case HELP_SOURCES:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Script Sources}\\par\\par"
            "Open via {\\b Menu \\u8594?  File \\u8594?  Sources...}\\par\\par"
            "The app can load scripts from three types of sources simultaneously:\\par\\par"
            "{\\b Built-in Repository}\\par\\par"
            "The KaiUR/Pycatia_Scripts repository is always the primary source. "
            "Use the checkbox at the top of the Sources dialog to enable or disable it.\\par\\par"
            "{\\b Additional GitHub Repositories}\\par\\par"
            "Add any GitHub repository that uses the same folder structure:\\par"
            "1. Click {\\b Add...} under Additional GitHub Repositories.\\par"
            "2. Enter the full URL: {\\f1\\cf3 https://github.com/owner/repo}\\par"
            "3. Enter the branch name (defaults to main).\\par"
            "4. Optionally add a token for private repos or higher rate limits.\\par"
            "5. Click OK.\\par\\par"
            "If two repositories have a folder with the same name, their scripts are merged into one tab.\\par\\par"
            "{\\b Local Script Folders}\\par\\par"
            "Add a folder on your machine that contains subfolders with .py files:\\par"
            "{\\f1\\cf3 My_Scripts\\\\}\\par"
            "{\\f1\\cf3   Any_Document_Scripts\\\\my_script.py}\\par"
            "{\\f1\\cf3   Part_Document_Scripts\\\\another.py}\\par\\par"
            "Local scripts run directly from disk \\emdash no downloading or SHA checking.\\par\\par"
            "{\\b Setup Folder}\\par\\par"
            "If a source has a {\\f1\\cf3 setup/requirements.txt} file, clicking {\\b Deps} "
            "will install those dependencies. The {\\f1\\cf3 setup/} folder never becomes a tab.\\par"
            "}";

    case HELP_FAVOURITES:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Favourites & Search}\\par\\par"
            "{\\b Favourites}\\par\\par"
            "Right-click any script \\u8594?  {\\b Add to Favourites} to add it to a dedicated "
            "{\\b \\u9733?  Favourites} tab at the top of the tab bar.\\par\\par"
            "\\bullet  The tab only appears when you have at least one favourite.\\par"
            "\\bullet  It disappears automatically when all favourites are removed.\\par"
            "\\bullet  Favourites are stored in {\\f1\\cf3 %APPDATA%\\\\CatiaMenuWin32\\\\prefs.ini}.\\par\\par"
            "To remove a favourite: right-click the script \\u8594?  {\\b Remove from Favourites}.\\par\\par"
            "{\\b Search / Filter}\\par\\par"
            "The filter bar below the toolbar filters scripts in the current tab in real time.\\par\\par"
            "\\bullet  Filters by both script name and purpose line.\\par"
            "\\bullet  Case-insensitive \\emdash searching {\\f1\\cf3 iges} matches {\\b IGES Export Curve AXIS}.\\par"
            "\\bullet  Clear the box to show all scripts again.\\par"
            "}";

    case HELP_DETAILS_NOTES:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Script Details & Notes}\\par\\par"
            "{\\b Script Details}\\par\\par"
            "Right-click any script \\u8594?  {\\b Script Details...} opens a full details dialog showing:\\par"
            "\\bullet  Name, Purpose, Author, Version, Date\\par"
            "\\bullet  Code environment (e.g. Python 3.10.4, PyCATIA 0.8.3)\\par"
            "\\bullet  CATIA release (e.g. V5R32)\\par"
            "\\bullet  Full description and requirements\\par"
            "\\bullet  Local cache path\\par"
            "\\bullet  Your personal note\\par"
            "\\bullet  Favourite and Hidden toggles\\par\\par"
            "Changes to the note, favourite, and hidden state are saved when you click OK.\\par\\par"
            "{\\b Script Notes}\\par\\par"
            "Right-click any script \\u8594?  {\\b Add Note...} (or {\\b Edit Note...}) to attach "
            "a personal note to a script.\\par\\par"
            "Notes are visible in the Script Details dialog and stored in "
            "{\\f1\\cf3 prefs.ini}.\\par"
            "}";

    case HELP_SORT_HIDE:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Sort & Hide Scripts}\\par\\par"
            "{\\b Sorting}\\par\\par"
            "{\\b Menu \\u8594?  View \\u8594?  Sort Scripts} offers four sort modes:\\par\\par"
            "\\bullet  {\\b Default Order} \\emdash order from GitHub API or disk\\par"
            "\\bullet  {\\b Alphabetical} \\emdash A to Z by script name\\par"
            "\\bullet  {\\b By Date} \\emdash most recent scripts first (from script header Date field)\\par"
            "\\bullet  {\\b Most Used} \\emdash scripts you run most often appear first\\par\\par"
            "The sort mode is saved in Settings and applied to all tabs.\\par\\par"
            "{\\b Hiding Scripts}\\par\\par"
            "Right-click any script \\u8594?  {\\b Hide Script} removes it from view.\\par\\par"
            "\\bullet  Hidden scripts are not deleted \\emdash they remain in the cache.\\par"
            "\\bullet  Hidden scripts will not reappear after a sync.\\par"
            "\\bullet  To restore: {\\b Menu \\u8594?  File \\u8594?  Manage Hidden Scripts}\\par"
            "\\bullet  Select a script and click {\\b Unhide}, or click {\\b Unhide All}.\\par"
            "}";

    case HELP_UPDATE_DEPS:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Update Dependencies}\\par\\par"
            "Click {\\b \\u8615?  Deps} (or {\\b Menu \\u8594?  Run \\u8594?  Update Dependencies}) "
            "to install Python packages required by the scripts.\\par\\par"
            "The app runs {\\f1\\cf3 pip install -r requirements.txt} for each source that has a "
            "{\\f1\\cf3 setup/requirements.txt} file, in order:\\par"
            "1. Main repository requirements\\par"
            "2. Each extra GitHub repository's requirements\\par"
            "3. Each local folder's requirements\\par\\par"
            "Each source runs in its own console window sequentially.\\par\\par"
            "{\\b Keep Deps console open}\\par\\par"
            "Enable this in Settings to keep each console window visible until you close it manually. "
            "Useful to read pip output and check for errors.\\par\\par"
            "{\\b Adding Dependencies to Your Scripts}\\par\\par"
            "Place a {\\f1\\cf3 requirements.txt} file in the {\\f1\\cf3 setup/} subfolder of "
            "your script repository or local folder. The app will find and run it automatically.\\par"
            "}";

    case HELP_KEYBOARD:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Keyboard Shortcuts}\\par\\par"
            "\\trowd\\trgaph100\\trleft100"
            "\\cellx2200\\cellx5000"
            "{\\b Shortcut}\\intbl\\cell {\\b Action}\\intbl\\cell\\row"
            "{\\f1\\cf3 F5}\\intbl\\cell Refresh + Sync\\intbl\\cell\\row"
            "{\\f1\\cf3 F9}\\intbl\\cell Run last script\\intbl\\cell\\row"
            "{\\f1\\cf3 F1}\\intbl\\cell Open Help\\intbl\\cell\\row"
            "{\\f1\\cf3 Ctrl+Tab}\\intbl\\cell Next tab\\intbl\\cell\\row"
            "{\\f1\\cf3 Ctrl+Shift+Tab}\\intbl\\cell Previous tab\\intbl\\cell\\row"
            "\\pard\\par\\par"
            "{\\b Right-click Menu}\\par\\par"
            "Right-clicking any script button shows:\\par"
            "\\bullet  Script Details...\\par"
            "\\bullet  Run with Arguments...\\par"
            "\\bullet  Add to Favourites / Remove from Favourites\\par"
            "\\bullet  Add Note... / Edit Note...\\par"
            "\\bullet  Hide Script\\par"
            "}";

    case HELP_TROUBLESHOOTING:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Troubleshooting}\\par\\par"
            "{\\b \\u8220? Connect to internet to sync\\u8221? }\\par"
            "\\bullet  Check your internet connection.\\par"
            "\\bullet  You may have hit the GitHub API rate limit (60 req/hr without a token). "
            "Add a GitHub token in Settings.\\par"
            "\\bullet  Corporate firewalls may block {\\f1\\cf3 api.github.com}.\\par\\par"
            "{\\b Tooltips or Script Details are blank}\\par"
            "This happens on first launch before scripts have downloaded. "
            "Wait for the sync to complete then hover the script again.\\par\\par"
            "{\\b Script fails with \\u8220? Python Not Found\\u8221? }\\par"
            "Open Settings and click Browse next to Python Interpreter to locate "
            "{\\f1\\cf3 python.exe}.\\par\\par"
            "{\\b SHA mismatch warning}\\par"
            "The local cached script differs from GitHub. Click {\\b Yes} to re-download. "
            "If it persists, delete the cache folder in Settings and re-sync.\\par\\par"
            "{\\b A script has disappeared}\\par"
            "It may have been hidden. Check {\\b Menu \\u8594?  File \\u8594?  Manage Hidden Scripts }. "
            "Also check if the search filter box has text in it.\\par\\par"
            "{\\b Update prompt appears on local builds }\\par"
            "Run {\\f1\\cf3 git fetch --tags} and {\\f1\\cf3 git pull origin main} then delete "
            "the build folder and rebuild.\\par"
            "}";

    default:
        return "{\\rtf1\\ansi Hello}";
    }
}

/* ================================================================== */
/*  Topic labels for the TreeView                                       */
/* ================================================================== */
static const WCHAR *Help_TopicLabel(HelpTopic t)
{
    switch (t) {
    case HELP_GETTING_STARTED:  return L"Getting Started";
    case HELP_INTERFACE:        return L"The Interface";
    case HELP_RUNNING_SCRIPTS:  return L"Running Scripts";
    case HELP_SETTINGS:         return L"Settings";
    case HELP_SOURCES:          return L"Script Sources";
    case HELP_FAVOURITES:       return L"Favourites & Search";
    case HELP_DETAILS_NOTES:    return L"Script Details & Notes";
    case HELP_SORT_HIDE:        return L"Sort & Hide Scripts";
    case HELP_UPDATE_DEPS:      return L"Update Dependencies";
    case HELP_KEYBOARD:         return L"Keyboard Shortcuts";
    case HELP_TROUBLESHOOTING:  return L"Troubleshooting";
    default:                    return L"";
    }
}

/* ================================================================== */
/*  Load RTF into RichEdit                                              */
/* ================================================================== */
typedef struct { const char *data; DWORD pos; } RtfStream;

static DWORD CALLBACK RtfCallback(DWORD_PTR cookie, LPBYTE buf,
                                   LONG cb, LONG *read)
{
    RtfStream *s = (RtfStream *)cookie;
    DWORD avail = (DWORD)strlen(s->data) - s->pos;
    DWORD n = (avail < (DWORD)cb) ? avail : (DWORD)cb;
    if (n) { memcpy(buf, s->data + s->pos, n); s->pos += n; }
    *read = (LONG)n;
    return 0;
}

static void Help_LoadTopic(HWND hEdit, HelpTopic topic)
{
    const char *rtf = Help_GetRTF(topic);
    RtfStream stream = { rtf, 0 };
    EDITSTREAM es = { (DWORD_PTR)&stream, 0, RtfCallback };
    SendMessage(hEdit, EM_STREAMIN, SF_RTF, (LPARAM)&es);
    /* Scroll to top */
    SendMessage(hEdit, EM_SETSEL, 0, 0);
    SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
}

/* ================================================================== */
/*  HelpDlgProc                                                         */
/* ================================================================== */
static HWND s_hwnd_help = NULL;

static INT_PTR CALLBACK HelpDlgProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{ (void)wp; /* suppress unused warning - required by dialog callback signature */
    static HWND hTree = NULL, hEdit = NULL;
    static int  splitter = 180; /* px */

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        /* Load RichEdit */
        LoadLibrary(L"RICHED20.DLL");

        /* Set help window icon */
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_HELP_ICON));
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_QUESTION);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);

        /* Create TreeView */
        hTree = CreateWindowEx(0, WC_TREEVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            0, 0, splitter, 400,
            hwnd, (HMENU)(UINT_PTR)IDC_HELP_TREE,
            GetModuleHandle(NULL), NULL);

        /* Create RichEdit */
        hEdit = CreateWindowEx(0, RICHEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
            ES_READONLY | ES_AUTOVSCROLL,
            splitter + 2, 0, 400, 400,
            hwnd, (HMENU)(UINT_PTR)IDC_HELP_RICHEDIT,
            GetModuleHandle(NULL), NULL);

        /* Style RichEdit */
        SendMessage(hEdit, EM_SETBKGNDCOLOR, 0,
                    (LPARAM)RGB(28, 28, 35));
        SendMessage(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                    MAKELONG(10, 10));

        /* Populate TreeView */
        for (int i = 0; i < HELP_TOPIC_COUNT; i++) {
            TVINSERTSTRUCTW tvi = {0};
            tvi.hParent      = TVI_ROOT;
            tvi.hInsertAfter = TVI_LAST;
            tvi.item.mask    = TVIF_TEXT | TVIF_PARAM;
            tvi.item.pszText = (LPWSTR)Help_TopicLabel((HelpTopic)i);
            tvi.item.lParam  = (LPARAM)i;
            TreeView_InsertItem(hTree, &tvi);
        }

        /* Select first item */
        HTREEITEM first = TreeView_GetRoot(hTree);
        if (first) {
            TreeView_SelectItem(hTree, first);
            Help_LoadTopic(hEdit, HELP_GETTING_STARTED);
        }

        /* Apply dark mode to tree */
        SetWindowTheme(hTree, L"DarkMode_Explorer", NULL);
        Window_ApplyDarkMode(hwnd);

        /* Size controls */
        RECT rc; GetClientRect(hwnd, &rc);
        SendMessage(hwnd, WM_SIZE, 0,
                    MAKELPARAM(rc.right, rc.bottom));
        return TRUE;
    }

    case WM_SIZE:
    {
        int w = LOWORD(lp), h = HIWORD(lp);
        if (hTree)
            SetWindowPos(hTree, NULL, 0, 0, splitter, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        if (hEdit)
            SetWindowPos(hEdit, NULL, splitter + 2, 0,
                         w - splitter - 2, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == IDC_HELP_TREE &&
            nm->code == TVN_SELCHANGED) {
            NMTREEVIEW *ntv = (NMTREEVIEW *)lp;
            HelpTopic t = (HelpTopic)ntv->itemNew.lParam;
            if (hEdit) Help_LoadTopic(hEdit, t);
        }
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 500;
        mm->ptMinTrackSize.y = 400;
        return 0;
    }

    case WM_CLOSE:
        s_hwnd_help = NULL;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hwnd_help = NULL;
        return 0;
    }
    return FALSE;
}

/* ================================================================== */
/*  Help_Show — public entry point                                      */
/* ================================================================== */
void Help_Show(void)
{
    /* Only one instance */
    if (s_hwnd_help) {
        SetForegroundWindow(s_hwnd_help);
        return;
    }

    s_hwnd_help = CreateDialog(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_HELP),
        g.hwnd,
        HelpDlgProc);

    if (s_hwnd_help)
        ShowWindow(s_hwnd_help, SW_SHOW);
}
