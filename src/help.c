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
/*  HelpTopic                                                           */
/*  Purpose: Enumerates all help topics shown in the TreeView pane of  */
/*           the in-app help window. HELP_TOPIC_COUNT allows iterating */
/*           the full set without hard-coding the count.               */
/*  In:  (enum — no parameters)                                        */
/*  Out: (values used as TreeView item lParam and switch keys)         */
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
    HELP_QUICK_BAR,
    HELP_KEYBOARD,
    HELP_TROUBLESHOOTING,
    HELP_TOPIC_COUNT
} HelpTopic;

/* ================================================================== */
/*  Help_GetRTF  (static)                                              */
/*  Purpose: Returns a static RTF-formatted string for the given help  */
/*           topic, ready to be streamed into a RichEdit control via   */
/*           EM_STREAMIN.                                              */
/*  In:  topic — help topic identifier from the HelpTopic enum        */
/*  Out: pointer to a null-terminated RTF string; never NULL           */
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
            "{\\b Requirements}\\par\\par"
            "\\bullet  Windows 10 or later\\par"
            "\\bullet  Python 3.9+ installed and in your PATH\\par"
            "\\bullet  PyCATIA: {\\f1\\cf3 pip install pycatia}\\par"
            "\\bullet  CATIA V5 must be running for scripts that interact with it\\par\\par"
            "{\\b Installation}\\par\\par"
            "1. Download {\\b CatiaMenuWin32.exe} from the GitHub releases page.\\par"
            "2. Place it anywhere on your machine \\emdash no installer required.\\par"
            "3. Double-click to run.\\par\\par"
            "{\\b First Launch}\\par\\par"
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
            "\\bullet  {\\b \\u9776?  Menu} \\emdash Access all app functions\\par"
            "\\bullet  {\\b \\u8635?  Refresh} \\emdash Re-sync scripts from all sources\\par"
            "\\bullet  {\\b \\u9881?  Settings} \\emdash Open the settings dialog\\par"
            "\\bullet  {\\b \\u8615?  Deps} \\emdash Install/update Python dependencies\\par"
            "\\bullet  {\\b \\u9632?  Stop} \\emdash Terminate the running background script "
            "(grayed out when idle, red when active)\\par\\par"
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
            "Shows sync progress, script launch status, and exit codes.\\par\\par"
            "{\\b Quick Launch Bar}\\par\\par"
            "An optional floating toolbar that pins your favourite scripts as icon buttons outside "
            "the main window. Enable it in {\\b Settings \\u8594?  Quick Bar} or "
            "{\\b Menu \\u8594?  View \\u8594?  Quick Bar \\u8594?  Enable Quick Bar}. "
            "See the {\\b Quick Launch Bar} help topic for details.\\par"
            "}";

    case HELP_RUNNING_SCRIPTS:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Running Scripts}\\par\\par"
            "Click any script button to run it. The app will:\\par"
            "\\bullet  Verify the script SHA hash against GitHub to confirm it has not been tampered with.\\par"
            "\\bullet  Launch Python with the script path.\\par"
            "\\bullet  Show the status in the status bar.\\par\\par"
            "{\\b Console Options}\\par\\par"
            "Configurable in Settings:\\par\\par"
            "\\bullet  {\\b Show console} \\emdash Opens a visible Python console when the script runs.\\par"
            "\\bullet  {\\b Keep console open} \\emdash Window stays open after the script finishes "
            "({\\f1\\cf3 cmd /k} mode) so you can read output and errors.\\par\\par"
            "Without Show console, scripts run silently in the background.\\par\\par"
            "{\\b Running Script Highlight}\\par\\par"
            "In background mode (no console) the button you clicked turns {\\b green} \\emdash "
            "border, left accent bar, and label text \\emdash for the duration of the run. "
            "The highlight clears automatically when the script exits or is stopped.\\par\\par"
            "{\\b Stopping a Running Script}\\par\\par"
            "Click the {\\b \\u9632?  Stop} toolbar button to immediately terminate a running "
            "background script. The button is grayed out when no script is running and turns red "
            "when one is active. Only background (no-console) runs can be stopped this way \\emdash "
            "if Show console is enabled, close the console window directly or press "
            "{\\f1\\cf3 Ctrl+C} inside it.\\par\\par"
            "{\\b Run with Arguments}\\par\\par"
            "Right-click any script and select {\\b Run with Arguments...} to pass custom "
            "command line arguments to the script.\\par\\par"
            "{\\b Repeat Mode}\\par\\par"
            "{\\b Double-click} any script button to enter repeat mode: the script re-runs "
            "automatically after each completion until you stop it.\\par\\par"
            "\\bullet  The button turns {\\b amber} (border, accent bar, label and \\u8635? symbol) "
            "to show which script is looping.\\par"
            "\\bullet  Press {\\b Escape} to cancel repeat (current run finishes normally).\\par"
            "\\bullet  {\\b Single-click the same script} to cancel repeat without a re-run.\\par"
            "\\bullet  {\\b Single-click a different script} to cancel repeat and run that script.\\par"
            "\\bullet  Click {\\b \\u9632?  Stop} to cancel repeat and terminate the current run.\\par\\par"
            "Toggle repeat mode on/off via {\\b Menu \\u8594?  Run \\u8594?  Repeat Script on Double-Click} "
            "or in {\\b Settings \\u8594?  Console}.\\par\\par"
            "> Note: Repeat mode is not available when {\\b Show console} is enabled in Settings.\\par\\par"
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
            "Open via the {\\b \\u2699?  Settings} toolbar button or {\\b Menu \\u8594?  File \\u8594?  Settings...}. "
            "The dialog has five tabs:\\par\\par"
            "{\\b General Tab}\\par\\par"
            "\\bullet  {\\b Python Interpreter} \\emdash Path to {\\f1\\cf3 python.exe}. "
            "Browse or leave blank to auto-detect from PATH.\\par"
            "\\bullet  {\\b Script Cache Folder} \\emdash Where downloaded scripts are stored "
            "(defaults to {\\f1\\cf3 %APPDATA%\\\\CatiaMenuWin32\\\\scripts}).\\par"
            "\\bullet  {\\b GitHub Token} \\emdash Optional Personal Access Token. "
            "Raises the API rate limit from 60 to 5000 req/hr. Required for private repositories.\\par\\par"
            "{\\b Sync Tab}\\par\\par"
            "\\bullet  {\\b Sync on startup} \\emdash Download latest scripts when the app starts.\\par"
            "\\bullet  {\\b Always download latest before running} \\emdash Fetch the newest version of a script before each launch.\\par"
            "\\bullet  {\\b Check for updates} \\emdash Notify when a newer app version is available.\\par"
            "\\bullet  {\\b Auto-install updates} \\emdash Download and install new versions automatically.\\par"
            "\\bullet  {\\b Auto-refresh every N hours} \\emdash Background sync interval (0 = disabled, default 6).\\par\\par"
            "{\\b Console Tab}\\par\\par"
            "\\bullet  {\\b Show console} \\emdash Open a visible Python console window when running scripts.\\par"
            "\\bullet  {\\b Keep console open} \\emdash Keep the window open after the script finishes "
            "({\\f1\\cf3 cmd /k} mode) so you can read output and errors.\\par"
            "\\bullet  {\\b Keep Deps Console Open} \\emdash Keep the dependency install window open until you close it.\\par"
            "\\bullet  {\\b Repeat on double-click (main window)} \\emdash Enable repeat mode for scripts in the main window "
            "(see {\\b Running Scripts} topic for details).\\par\\par"
            "{\\b Window Tab}\\par\\par"
            "\\bullet  {\\b Always on Top} \\emdash Keep the main window above other applications such as CATIA.\\par"
            "\\bullet  {\\b Minimize to Tray} \\emdash Hide to the system tray instead of the taskbar when minimized.\\par"
            "\\bullet  {\\b Start with Windows} \\emdash Launch automatically at login.\\par"
            "\\bullet  {\\b Start Minimized} \\emdash Start hidden in the tray.\\par"
            "\\bullet  {\\b Theme} \\emdash Dark, Light, or System (follows Windows setting).\\par"
            "\\bullet  {\\b Sort Scripts} \\emdash Default Order, Alphabetical, By Date, or Most Used.\\par\\par"
            "{\\b Quick Bar Tab}\\par\\par"
            "\\bullet  {\\b Enable Quick Launch Bar} \\emdash Show the floating icon toolbar for favourite scripts.\\par"
            "\\bullet  {\\b Orientation} \\emdash Vertical (stacked column) or Horizontal (row).\\par"
            "\\bullet  {\\b Stay on Top with Target App} \\emdash Auto-elevate the bar when the target application is in the foreground.\\par"
            "\\bullet  {\\b Target App} \\emdash Window title substring to watch (e.g. {\\f1\\cf3 CATIA V5}). "
            "Leave empty for the bar to always remain visible.\\par"
            "\\bullet  {\\b Target Exe} \\emdash Process executable name to match alongside Target App "
            "(e.g. {\\f1\\cf3 CNEXT.exe}). Prevents false positives from other windows whose titles "
            "contain the same substring. Leave empty to match any process.\\par\\par"
            "{\\b Reset to Defaults}\\par\\par"
            "The {\\b Reset to Defaults} button resets all settings to their factory defaults. "
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
            "\\bullet  {\\b Default Order} \\emdash Order from GitHub API or disk\\par"
            "\\bullet  {\\b Alphabetical} \\emdash A to Z by script name\\par"
            "\\bullet  {\\b By Date} \\emdash Most recent scripts first (from script header Date field)\\par"
            "\\bullet  {\\b Most Used} \\emdash Scripts you run most often appear first\\par\\par"
            "The sort mode is saved in Settings and applied to all tabs.\\par\\par"
            "{\\b Hiding Scripts}\\par\\par"
            "Right-click any script \\u8594?  {\\b Hide Script} removes it from view.\\par\\par"
            "\\bullet  Hidden scripts are not deleted \\emdash They remain in the cache.\\par"
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
            "{\\b Keep Deps Console Open}\\par\\par"
            "Enable this in Settings to keep each console window visible until you close it manually. "
            "Useful to read pip output and check for errors.\\par\\par"
            "{\\b Adding Dependencies to Your Scripts}\\par\\par"
            "Place a {\\f1\\cf3 requirements.txt} file in the {\\f1\\cf3 setup/} subfolder of "
            "your script repository or local folder. The app will find and run it automatically.\\par"
            "}";

    case HELP_QUICK_BAR:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Quick Launch Bar}\\par\\par"
            "The Quick Launch Bar is a small floating toolbar that displays your favourite scripts as "
            "icon buttons, always within reach outside the main window.\\par\\par"
            "{\\b Enabling}\\par\\par"
            "\\bullet  Open {\\b \\u2699?  Settings \\u8594?  Quick Bar tab} and check {\\b Enable Quick Launch Bar}.\\par"
            "\\bullet  Or use {\\b Menu \\u8594?  View \\u8594?  Quick Bar \\u8594?  Enable Quick Bar}.\\par\\par"
            "{\\b Populating the Bar}\\par\\par"
            "Right-click any script \\u8594?  {\\b Add to Favourites}. "
            "Every script you mark as a favourite automatically appears on the Quick Bar.\\par\\par"
            "{\\b Orientation}\\par\\par"
            "Switch between {\\b Vertical} (stacked column) and {\\b Horizontal} (row) layouts "
            "from {\\b Settings \\u8594?  Quick Bar} or {\\b Menu \\u8594?  View \\u8594?  Quick Bar}.\\par\\par"
            "{\\b Moving the Bar}\\par\\par"
            "Click and drag the bar to reposition it anywhere on screen. "
            "The position is saved automatically.\\par"
            "Use {\\b Menu \\u8594?  View \\u8594?  Quick Bar \\u8594?  Reset Position} to move it back to the default location.\\par\\par"
            "{\\b Stay on Top with Target Application}\\par\\par"
            "When a {\\b Target App} is configured, the Quick Bar automatically elevates to topmost "
            "when that application\\u8217?s window is in the foreground (e.g. CATIA V5), and lowers "
            "itself when a different window becomes active.\\par\\par"
            "\\bullet  Set the target in {\\b Settings \\u8594?  Quick Bar \\u8594?  Target App field}.\\par"
            "\\bullet  Leave the field empty to keep the bar always visible on top.\\par"
            "\\bullet  Toggle the behaviour with {\\b Settings \\u8594?  Quick Bar \\u8594?  Stay on Top with Target App}.\\par\\par"
            "{\\b Repeat on Double-Click}\\par\\par"
            "{\\b Double-click} any Quick Bar button to enter repeat mode: the script re-runs after each "
            "completion until you stop it. The button turns amber with a \\u8635? loop symbol.\\par\\par"
            "\\bullet  Press {\\b Escape} to cancel.\\par"
            "\\bullet  Single-click any button to cancel (same = no re-run, different = run that script).\\par"
            "\\bullet  Toggle via right-click \\u8594?  {\\b Repeat on Double-Click} or "
            "{\\b Menu \\u8594?  View \\u8594?  Quick Bar \\u8594?  Repeat on Double-Click}.\\par\\par"
            "{\\b Process Executable Filter}\\par\\par"
            "If another application has the same window-title substring as your target, the bar will "
            "incorrectly respond to it. The {\\b Target Exe} field adds a second filter: the window\\u8217?s "
            "owning process must also match the given executable filename.\\par\\par"
            "\\bullet  Set {\\b Settings \\u8594?  Quick Bar \\u8594?  Target Exe} to the executable "
            "filename (e.g. {\\f1\\cf3 CNEXT.exe} for CATIA V5).\\par"
            "\\bullet  The check is case-insensitive and matches the filename only, not the full path.\\par"
            "\\bullet  Leave the field empty (default) to match any process — identical to the previous behaviour.\\par"
            "\\bullet  {\\b Repeat on double-click (Quick Bar)} \\emdash Enable repeat mode for Quick Bar buttons "
            "(see {\\b Running Scripts} topic for details).\\par"
            "}";

    case HELP_KEYBOARD:
        return "{\\rtf1\\ansi\\deff0"
            "{\\fonttbl{\\f0 Segoe UI;}{\\f1 Consolas;}}"
            "{\\colortbl ;\\red82\\green155\\blue245;\\red210\\green215\\blue240;\\red110\\green116\\blue148;}"
            "\\f0\\fs20\\cf2"
            "{\\b\\fs28\\cf1 Keyboard Shortcuts}\\par\\par"
            "\\pard\\tx2160"
            "{\\b Shortcut}\\tab{\\b Action}\\par"
            "{\\f1\\cf3 F5}\\tab Refresh + Sync\\par"
            "{\\f1\\cf3 F9}\\tab Run last script\\par"
            "{\\f1\\cf3 F1}\\tab Open Help\\par"
            "{\\f1\\cf3 Ctrl+Tab}\\tab Next tab\\par"
            "{\\f1\\cf3 Ctrl+Shift+Tab}\\tab Previous tab\\par"
            "{\\f1\\cf3 Escape}\\tab Cancel repeat mode (when active)\\par"
            "\\pard\\par"
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
/*  Help_TopicLabel  (static)                                          */
/*  Purpose: Maps a HelpTopic enum value to the wide-string label      */
/*           displayed in the TreeView pane of the help window.        */
/*  In:  t  — help topic identifier                                    */
/*  Out: pointer to a static wide-string label; L"" for unknown values */
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
    case HELP_QUICK_BAR:        return L"Quick Launch Bar";
    case HELP_KEYBOARD:         return L"Keyboard Shortcuts";
    case HELP_TROUBLESHOOTING:  return L"Troubleshooting";
    default:                    return L"";
    }
}

/* ================================================================== */
/*  RtfStream  (static)                                                */
/*  Purpose: Streaming state struct passed as the cookie to            */
/*           RtfCallback so successive EM_STREAMIN chunks are fed      */
/*           from a static RTF source string without copying.          */
/*  In:  data — pointer to a null-terminated RTF source string        */
/*       pos  — current byte offset into data (advanced by callback)  */
/*  Out: (struct value — consumed by RtfCallback via cookie pointer)   */
/* ================================================================== */
typedef struct { const char *data; DWORD pos; } RtfStream;

/* ================================================================== */
/*  RtfCallback  (static)                                              */
/*  Purpose: EDITSTREAMCALLBACK that supplies successive chunks of RTF */
/*           bytes to the RichEdit EM_STREAMIN mechanism.              */
/*  In:  cookie — pointer to an RtfStream holding the RTF source      */
/*       buf    — output buffer to fill with RTF bytes                 */
/*       cb     — maximum bytes to write into buf                      */
/*       read   — receives the number of bytes actually written        */
/*  Out: 0 always (non-zero signals a streaming error to RichEdit)     */
/* ================================================================== */
static DWORD CALLBACK RtfCallback(DWORD_PTR cookie, LPBYTE buf,
                                   LONG cb, LONG *read)
{
    RtfStream *s = (RtfStream *)cookie;
    DWORD avail = (DWORD)strlen(s->data) - s->pos;
    DWORD n = (avail < (DWORD)cb) ? avail : (DWORD)cb;
    if (n) { if (memmove_s(buf, n, s->data + s->pos, n) == 0) s->pos += n; else n = 0; }
    *read = (LONG)n;
    return 0;
}

/* ================================================================== */
/*  Help_LoadTopic  (static)                                           */
/*  Purpose: Replaces the content of a RichEdit control with the RTF   */
/*           for the given help topic via EM_STREAMIN, then scrolls    */
/*           the control back to the top.                              */
/*  In:  hEdit — handle to the RichEdit control                        */
/*       topic — topic whose RTF string Help_GetRTF returns            */
/*  Out: (void — control content replaced; scroll position reset)      */
/* ================================================================== */
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
/*  HelpDlgProc  (static)                                              */
/*  Purpose: Dialog procedure for the in-app help window. On init it   */
/*           creates a borderless, flat-select TreeView of topics and a */
/*           RichEdit display pane.  Handles resizing, topic selection, */
/*           dark mode, background painting (WM_ERASEBKGND fills with  */
/*           COL_BG), and a COL_DIVIDER vertical separator (WM_PAINT). */
/*  In:  hwnd — dialog window handle                                   */
/*       msg  — window message identifier                              */
/*       wp   — WPARAM (message-dependent)                             */
/*       lp   — LPARAM (message-dependent)                             */
/*  Out: TRUE if message handled; FALSE to defer to DefDlgProc         */
/* ================================================================== */
static HWND s_hwnd_help = NULL;

static INT_PTR CALLBACK HelpDlgProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    static HWND hTree = NULL, hEdit = NULL;
    static int  splitter = 200; /* px */

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        /* Load RichEdit */
        LoadLibrary(L"RICHED20.DLL");

        /* Icon */
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_HELP_ICON));
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_QUESTION);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);

        /* Clip children so the dialog background fill doesn't paint over controls */
        SetWindowLongPtr(hwnd, GWL_STYLE,
            GetWindowLongPtr(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);

        /* Create TreeView — no border, no expand/collapse indicators (flat list) */
        hTree = CreateWindowEx(0, WC_TREEVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
            TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_NOHSCROLL,
            0, 0, splitter, 400,
            hwnd, (HMENU)(UINT_PTR)IDC_HELP_TREE,
            GetModuleHandle(NULL), NULL);

        /* Set TreeView colours to match app theme */
        TreeView_SetBkColor(hTree,   COL_BG());
        TreeView_SetTextColor(hTree, COL_TEXT());
        SendMessage(hTree, WM_SETFONT, (WPARAM)g.font_ui, FALSE);

        /* Create RichEdit */
        hEdit = CreateWindowEx(0, RICHEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPSIBLINGS |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            splitter + 1, 0, 400, 400,
            hwnd, (HMENU)(UINT_PTR)IDC_HELP_RICHEDIT,
            GetModuleHandle(NULL), NULL);

        /* Style RichEdit */
        SendMessage(hEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_BG());
        SendMessage(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                    MAKELONG(14, 14));

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

        /* Apply dark mode */
        SetWindowTheme(hTree, L"DarkMode_Explorer", NULL);
        Window_ApplyDarkMode(hwnd);

        /* Size controls */
        RECT rc; GetClientRect(hwnd, &rc);
        SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
        return TRUE;
    }

    case WM_ERASEBKGND:
    {
        /* Fill dialog background with app theme colour */
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g.br_bg);
        return TRUE;
    }

    case WM_PAINT:
    {
        /* Draw a subtle vertical divider between the tree and the content pane */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HPEN pen = CreatePen(PS_SOLID, 1, COL_DIVIDER());
        HPEN op  = SelectObject(hdc, pen);
        MoveToEx(hdc, splitter, 0, NULL);
        LineTo(hdc, splitter, rc.bottom);
        SelectObject(hdc, op);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
    {
        int w = LOWORD(lp), h = HIWORD(lp);
        if (hTree)
            SetWindowPos(hTree, NULL, 0, 0, splitter, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        if (hEdit)
            SetWindowPos(hEdit, NULL, splitter + 1, 0,
                         w - splitter - 1, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        /* Repaint the divider strip */
        RECT div = { splitter - 1, 0, splitter + 2, h };
        InvalidateRect(hwnd, &div, FALSE);
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
        mm->ptMinTrackSize.x = 600;
        mm->ptMinTrackSize.y = 420;
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
/*  Help_Show                                                           */
/*  Purpose: Creates and shows the in-app help dialog, or brings the   */
/*           existing instance to the foreground if already open.      */
/*           Enforces a single-instance constraint via s_hwnd_help.   */
/*  In:  (none)                                                         */
/*  Out: (void — help window created or focused)                       */
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
