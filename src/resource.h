#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP_ICON            101

#define IDR_MAINMENU            200
#define IDM_REFRESH             201
#define IDM_SETTINGS            202
#define IDM_EXIT                203
#define IDM_RUN_LAST            204
#define IDM_OPEN_CACHE          205
#define IDM_ABOUT               206
#define IDM_GITHUB              207
#define IDM_GITHUB_SCRIPTS      216
#define IDM_UPDATE_DEPS         208
#define IDM_ALWAYS_ON_TOP       209
#define IDM_MINIMIZE_TO_TRAY    210
#define IDM_START_WITH_WINDOWS  211
#define IDM_START_MINIMIZED     212
#define IDM_THEME_DARK          213
#define IDM_THEME_LIGHT         214
#define IDM_THEME_SYSTEM        215

#define IDD_SETTINGS            300
#define IDD_ABOUT               301

#define IDC_EDIT_PYTHON         401
#define IDC_BTN_BROWSE_PY       402
#define IDC_EDIT_CACHE          403
#define IDC_BTN_BROWSE_DIR      404
#define IDC_CHK_DOWNLOAD        405
#define IDC_CHK_CONSOLE         406
#define IDC_CHK_TOKEN           407
#define IDC_EDIT_TOKEN          408
#define IDC_CHK_AUTOSYNC        409

#define IDC_TAB_CTRL            501
#define IDC_STATUS_BAR          502
#define IDC_BTN_MENU            502
#define IDC_BTN_REFRESH         503
#define IDC_BTN_SETTINGS        504
#define IDC_SCROLL_PANEL        505

#define IDC_CHK_KEEP_OPEN       410
#define IDC_CHK_CHECK_UPDATES   411
#define IDC_CHK_DEPS_KEEP_OPEN  412
#define IDC_BTN_RESET           420
#define IDC_BTN_UPDATE_DEPS     506

#define IDC_SCRIPT_BTN_BASE     1000

/* System tray message */
#define WM_TRAYICON             (WM_USER + 10)
#define TRAY_ID                 1

#endif

/* Sources dialog */
#define IDM_SOURCES             217
#define IDD_SOURCES             302
#define IDC_LST_REPOS           601
#define IDC_BTN_REPO_ADD        602
#define IDC_BTN_REPO_EDIT       603
#define IDC_BTN_REPO_REMOVE     604
#define IDC_LST_LOCAL           605
#define IDC_BTN_LOCAL_ADD       606
#define IDC_BTN_LOCAL_REMOVE    607
#define IDC_CHK_MAIN_REPO       608

/* Repo edit dialog */
#define IDD_REPO_EDIT           303
#define IDC_EDIT_REPO_URL       610
#define IDC_EDIT_REPO_BRANCH    611
#define IDC_EDIT_REPO_TOKEN     612
#define IDC_CHK_REPO_TOKEN      613
#define IDC_CHK_REPO_ENABLED    614
#define IDC_BTN_REPO_TOGGLE     615
#define IDC_BTN_LOCAL_TOGGLE    616
