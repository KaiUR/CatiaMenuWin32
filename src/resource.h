#ifndef RESOURCE_H
#define RESOURCE_H

/* ------------------------------------------------------------------ */
/*  Application icons  (101–102)                                        */
/* ------------------------------------------------------------------ */
#define IDI_APP_ICON            101   /* main window / tray icon        */
/* IDI_HELP_ICON = 102 is defined further down with the help window    */

/* ------------------------------------------------------------------ */
/*  Menu resource and main menu command IDs  (200–257)                  */
/*  These are sent to WM_COMMAND when a menu item is selected.         */
/* ------------------------------------------------------------------ */
#define IDR_MAINMENU            200   /* menu resource handle           */
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

/* ------------------------------------------------------------------ */
/*  Dialog template IDs  (300–309)                                      */
/*  Each IDD_ identifies a dialog template in the .rc resource file.   */
/* ------------------------------------------------------------------ */
#define IDD_SETTINGS            300
#define IDD_ABOUT               301

/* ------------------------------------------------------------------ */
/*  Settings dialog controls — Script / Sync tab  (401–414)            */
/* ------------------------------------------------------------------ */
#define IDC_EDIT_PYTHON         401
#define IDC_BTN_BROWSE_PY       402
#define IDC_EDIT_CACHE          403
#define IDC_BTN_BROWSE_DIR      404
#define IDC_CHK_DOWNLOAD        405
#define IDC_CHK_CONSOLE         406
#define IDC_CHK_TOKEN           407
#define IDC_EDIT_TOKEN          408
#define IDC_CHK_AUTOSYNC        409
#define IDC_CHK_KEEP_OPEN       410
#define IDC_CHK_CHECK_UPDATES   411
#define IDC_CHK_DEPS_KEEP_OPEN  412
#define IDC_BTN_RESET           420
#define IDC_CHK_AUTO_UPDATE     413   /* auto-download and install updates  */
#define IDC_EDIT_REFRESH_INTERVAL 414 /* sync interval in hours             */

/* ------------------------------------------------------------------ */
/*  Main window controls  (501–509)                                     */
/* ------------------------------------------------------------------ */
#define IDC_TAB_CTRL            501
#define IDC_STATUS_BAR          507   /* was 502 — renumbered to avoid conflict with IDC_BTN_MENU */
#define IDC_BTN_MENU            502
#define IDC_BTN_REFRESH         503
#define IDC_BTN_SETTINGS        504
#define IDC_SCROLL_PANEL        505
#define IDC_BTN_UPDATE_DEPS     506
#define IDC_BTN_STOP            508
#define IDC_BTN_LOG             509   /* script output log window button (visible in bg mode) */

/* ------------------------------------------------------------------ */
/*  Script buttons  (1000+)                                             */
/*  Buttons are created dynamically; each button's control ID is       */
/*  IDC_SCRIPT_BTN_BASE + script_index within the active folder.       */
/* ------------------------------------------------------------------ */
#define IDC_SCRIPT_BTN_BASE     1000

/* ------------------------------------------------------------------ */
/*  Sources dialog  (217, 302, 601–616)                                 */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Repo edit dialog  (303, 610–616)                                    */
/* ------------------------------------------------------------------ */
#define IDD_REPO_EDIT           303
#define IDC_EDIT_REPO_URL       610
#define IDC_EDIT_REPO_BRANCH    611
#define IDC_EDIT_REPO_TOKEN     612
#define IDC_CHK_REPO_TOKEN      613
#define IDC_CHK_REPO_ENABLED    614
#define IDC_BTN_REPO_TOGGLE     615
#define IDC_BTN_LOCAL_TOGGLE    616

/* ------------------------------------------------------------------ */
/*  Sort menu commands  (218–221)                                        */
/* ------------------------------------------------------------------ */
#define IDM_SORT_DEFAULT    218
#define IDM_SORT_ALPHA      219
#define IDM_SORT_DATE       220
#define IDM_SORT_MOST_USED  221

/* ------------------------------------------------------------------ */
/*  View menu additions  (222–223)                                      */
/* ------------------------------------------------------------------ */
#define IDM_OPEN_EXE_FOLDER 222
#define IDM_HIDDEN_SCRIPTS  223

/* ------------------------------------------------------------------ */
/*  Script context menu commands  (230–237)                             */
/* ------------------------------------------------------------------ */
#define IDM_SCRIPT_DETAILS        230
#define IDM_SCRIPT_FAVOURITE      231
#define IDM_SCRIPT_HIDE           232
#define IDM_SCRIPT_NOTE           233
#define IDM_SCRIPT_RUN_ARGS       234
#define IDM_SCRIPT_OPEN_LOCATION  235
#define IDM_SCRIPT_OPEN_DEFAULT   236
#define IDM_SCRIPT_OPEN_EDITOR    237

/* ------------------------------------------------------------------ */
/*  Script details dialog  (304, 701–713)                               */
/* ------------------------------------------------------------------ */
#define IDD_SCRIPT_DETAILS  304
#define IDC_DETAIL_NAME     701
#define IDC_DETAIL_PURPOSE  702
#define IDC_DETAIL_AUTHOR   703
#define IDC_DETAIL_VERSION  704
#define IDC_DETAIL_DATE     705
#define IDC_DETAIL_CODE     706
#define IDC_DETAIL_RELEASE  707
#define IDC_DETAIL_DESC     708
#define IDC_DETAIL_REQS     709
#define IDC_DETAIL_NOTE     710
#define IDC_DETAIL_PATH     711
#define IDC_CHK_FAVOURITE   712
#define IDC_CHK_HIDDEN      713

/* ------------------------------------------------------------------ */
/*  Run with args dialog  (305, 720)                                    */
/* ------------------------------------------------------------------ */
#define IDD_RUN_ARGS        305
#define IDC_EDIT_RUN_ARGS   720

/* ------------------------------------------------------------------ */
/*  Script note dialog  (306, 721)                                      */
/* ------------------------------------------------------------------ */
#define IDD_SCRIPT_NOTE     306
#define IDC_EDIT_NOTE       721

/* ------------------------------------------------------------------ */
/*  Hidden scripts dialog  (307, 730–732)                               */
/* ------------------------------------------------------------------ */
#define IDD_HIDDEN_SCRIPTS  307
#define IDC_LST_HIDDEN      730
#define IDC_BTN_UNHIDE      731
#define IDC_BTN_UNHIDE_ALL  732

/* ------------------------------------------------------------------ */
/*  Main window — search / filter box  (800)                            */
/* ------------------------------------------------------------------ */
#define IDC_SEARCH          800

/* ------------------------------------------------------------------ */
/*  Settings dialog — tab control and Window tab  (415–420)             */
/* ------------------------------------------------------------------ */
#define IDC_TAB_SETTINGS        415   /* tab strip inside settings dialog */
#define IDC_CHK_ALWAYS_ON_TOP   416
#define IDC_CHK_MINIMIZE_TRAY   417
#define IDC_CHK_START_WINDOWS   418
#define IDC_CHK_START_MIN       419
/* IDC_BTN_RESET = 420 already defined in the Script/Sync block above  */

/* ------------------------------------------------------------------ */
/*  Settings dialog — Theme radio buttons  (421–423)                    */
/* ------------------------------------------------------------------ */
#define IDC_RAD_THEME_DARK      421
#define IDC_RAD_THEME_LIGHT     422
#define IDC_RAD_THEME_SYSTEM    423

/* ------------------------------------------------------------------ */
/*  Settings dialog — Sort radio buttons  (424–427)                     */
/* ------------------------------------------------------------------ */
#define IDC_RAD_SORT_DEFAULT    424
#define IDC_RAD_SORT_ALPHA      425
#define IDC_RAD_SORT_DATE       426
#define IDC_RAD_SORT_USED       427

/* ------------------------------------------------------------------ */
/*  Settings dialog — Quick Bar tab controls  (428–433)                 */
/* ------------------------------------------------------------------ */
#define IDC_CHK_QBAR_ENABLE         428
#define IDC_RAD_QBAR_HORIZ          429
#define IDC_RAD_QBAR_VERT           430
#define IDC_CHK_QBAR_TOPMOST        431
#define IDC_EDIT_QBAR_TARGET_S      432   /* title-substring filter field  */
#define IDC_EDIT_QBAR_TARGET_EXE_S  433   /* process exe name filter field */

/* ------------------------------------------------------------------ */
/*  Settings dialog — groupbox and label controls  (450–466)            */
/*  These are shown/hidden when switching between settings tabs.        */
/* ------------------------------------------------------------------ */
#define IDC_GRP_PYTHON          450
#define IDC_LBL_PYTHON_PATH     451
#define IDC_GRP_CACHE           452
#define IDC_LBL_CACHE_PATH      453
#define IDC_GRP_TOKEN           454
#define IDC_GRP_SYNC            455
#define IDC_LBL_REFRESH1        456   /* "Refresh every" label           */
#define IDC_LBL_REFRESH2        457   /* "hours" label after the spinner */
#define IDC_GRP_CONSOLE         458
#define IDC_GRP_WINDOW          459
#define IDC_GRP_THEME           460
#define IDC_GRP_SORT            461
#define IDC_GRP_QBAR_ORI            462   /* orientation group box       */
#define IDC_LBL_QBAR_TARGET         463
#define IDC_LBL_QBAR_TIP            464   /* hint text below target field */
#define IDC_LBL_QBAR_TARGET_EXE     465
#define IDC_LBL_QBAR_EXE_TIP        466   /* hint text below exe field    */

/* ------------------------------------------------------------------ */
/*  Help window  (240–245, 308, 840–842, 102)                           */
/* ------------------------------------------------------------------ */
#define IDM_HELP_CONTENTS   240
#define IDM_REPORT_BUG      241
#define IDM_WIKI            242
#define IDM_WIKI_SCRIPTS    243
#define IDM_CHECK_UPDATES   244
#define IDM_GITHUB_PAGES    245
#define IDD_HELP            308
#define IDC_HELP_TREE       840
#define IDC_HELP_RICHEDIT   841
#define IDC_HELP_SEARCH     842
#define IDI_HELP_ICON       102

/* ------------------------------------------------------------------ */
/*  About dialog controls  (740–743)                                    */
/* ------------------------------------------------------------------ */
#define IDC_ABOUT_ICON          740
#define IDC_ABOUT_TITLE         741
#define IDC_ABOUT_VER           742
#define IDC_BTN_ABOUT_GITHUB    743

/* ------------------------------------------------------------------ */
/*  Quick Launch Bar menu commands and settings  (250–255)              */
/* ------------------------------------------------------------------ */
#define IDM_QBAR_TOGGLE      250
#define IDM_QBAR_HORIZONTAL  251
#define IDM_QBAR_VERTICAL    252
#define IDM_QBAR_TOPMOST     253
#define IDM_QBAR_RESET_POS   254
#define IDM_QBAR_SET_TARGET  255

/* ------------------------------------------------------------------ */
/*  Double-click repeat menu commands  (256–257)                        */
/* ------------------------------------------------------------------ */
#define IDM_REPEAT_MAINAPP   256   /* toggle repeat-on-dblclick for main window */
#define IDM_REPEAT_QBAR      257   /* toggle repeat-on-dblclick for Quick Bar   */

/* ------------------------------------------------------------------ */
/*  Quick Bar target app dialog  (309, 760–762)                         */
/* ------------------------------------------------------------------ */
#define IDD_QBAR_TARGET              309
#define IDC_EDIT_QBAR_TARGET         760
#define IDC_EDIT_QBAR_TARGET_EXE     761
#define IDC_BTN_BROWSE_TARGET_EXE    762

/* ------------------------------------------------------------------ */
/*  Settings dialog — remaining Quick Bar / Console controls  (467–470) */
/* ------------------------------------------------------------------ */
#define IDC_BTN_BROWSE_QBAR_EXE_S   467   /* browse button for Quick Bar exe field */
#define IDC_CHK_REPEAT_MAIN   468         /* Console tab: repeat main-window scripts  */
#define IDC_CHK_REPEAT_QBAR   469         /* Quick Bar tab: repeat Quick Bar scripts  */
#define IDC_CHK_OFFLINE_CACHE 470         /* Sync tab: show cached scripts when offline */

/* ------------------------------------------------------------------ */
/*  Settings dialog — Script Display controls  (471–472)               */
/* ------------------------------------------------------------------ */
#define IDC_CHK_TINT_SOURCES  471   /* Window tab: tint local/extra scripts differently */
#define IDC_GRP_DISPLAY       472   /* Window tab: "Script Display" groupbox             */

#endif /* RESOURCE_H */
