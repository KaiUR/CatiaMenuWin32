/*
 * sources.c  -  Sources dialog: manage extra GitHub repos and local folders.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  DeleteFolderRecursive  (static)                                    */
/*  Purpose: Recursively deletes a directory and all its contents.     */
/*           Used when a user removes an extra GitHub repo to clean up */
/*           the corresponding cached script folder.                   */
/*  In:  path — full path of the directory to delete                   */
/*  Out: (void)                                                         */
/* ================================================================== */
static void DeleteFolderRecursive(const WCHAR *path)
{
    WCHAR pattern[MAX_APPPATH];
    _snwprintf_s(pattern, MAX_APPPATH, _TRUNCATE, L"%s\\*", path);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do
    {
        /* "." and ".." are the current and parent directory entries — never recurse into them */
        if (wcscmp(fd.cFileName, L".") == 0 ||
            wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR full[MAX_APPPATH];
        _snwprintf_s(full, MAX_APPPATH, _TRUNCATE, L"%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            DeleteFolderRecursive(full); /* recurse into subdirectory */
        else
            DeleteFileW(full); /* delete regular file */
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    RemoveDirectoryW(path);
}

/* ================================================================== */
/*  Repos_Populate  (static)                                           */
/*  Purpose: Clears and repopulates the extra-repos ListView control   */
/*           in the Sources dialog from g.cfg.extra_repos[].          */
/*  In:  hList — HWND of the IDC_LST_REPOS ListView control           */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Repos_Populate(HWND hList)
{
    ListView_DeleteAllItems(hList);
    for (int i = 0; i < g.cfg.extra_repo_count; i++)
    {
        ExtraRepo *r = &g.cfg.extra_repos[i];
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = r->url;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, r->branch[0] ? r->branch : L"main"); /* display "main" if branch was left blank */
        ListView_SetItemText(hList, i, 2, r->enabled ? L"Yes" : L"No");
    }
}

/* ================================================================== */
/*  Locals_Populate  (static)                                          */
/*  Purpose: Clears and repopulates the local-dirs ListView control    */
/*           in the Sources dialog from g.cfg.local_dirs[].           */
/*  In:  hList — HWND of the IDC_LST_LOCAL ListView control            */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Locals_Populate(HWND hList)
{
    ListView_DeleteAllItems(hList);
    for (int i = 0; i < g.cfg.local_dir_count; i++)
    {
        LocalDir *d = &g.cfg.local_dirs[i];
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = d->path;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, d->enabled ? L"Yes" : L"No");
    }
}

/* ================================================================== */
/*  RepoEditArg                                                         */
/*  Purpose: Argument struct passed to RepoEditDlgProc via LPARAM.    */
/*           Holds a pointer to the ExtraRepo to edit and a flag       */
/*           indicating whether this is a new or existing entry.       */
/*  In:  (set by SourcesDlgProc before DialogBoxParam call)            */
/*  Out: (repo is modified in-place when IDOK is returned)             */
/* ================================================================== */
typedef struct
{
    ExtraRepo *repo;
    bool is_new;
} RepoEditArg;

/* ================================================================== */
/*  RepoEditDlgProc  (static)                                          */
/*  Purpose: Dialog procedure for IDD_REPO_EDIT (add/edit a GitHub     */
/*           repository source).  WM_INITDIALOG populates fields from  */
/*           the repo struct; IDOK validates the URL and writes back.  */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (control ID in WM_COMMAND)                      */
/*       lp   — LPARAM on WM_INITDIALOG: pointer to RepoEditArg        */
/*  Out: INT_PTR — TRUE for handled; FALSE otherwise                   */
/* ================================================================== */
static INT_PTR CALLBACK RepoEditDlgProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lp)
{
    /* Retrieve the arg pointer stored during WM_INITDIALOG; NULL before first message */
    RepoEditArg *arg = (RepoEditArg *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_INITDIALOG:
        arg = (RepoEditArg *)lp; /* lp carries the RepoEditArg passed to DialogBoxParam */
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)arg); /* stash it so WM_COMMAND can retrieve it */
        if (!arg || !arg->repo)
        {
            EndDialog(hwnd, IDCANCEL);
            return FALSE;
        } /* safety: malformed caller */
        SetDlgItemText(hwnd, IDC_EDIT_REPO_URL, arg->repo->url);
        SetDlgItemText(hwnd, IDC_EDIT_REPO_BRANCH, arg->repo->branch[0] ? arg->repo->branch : L"main"); /* show "main" as default */
        CheckDlgButton(hwnd, IDC_CHK_REPO_TOKEN,
                       arg->repo->token[0] ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemText(hwnd, IDC_EDIT_REPO_TOKEN, arg->repo->token);
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REPO_TOKEN),
                     arg->repo->token[0] != 0); /* grey out token field if no token is set */
        CheckDlgButton(hwnd, IDC_CHK_REPO_ENABLED,
                       arg->repo->enabled ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        if (!arg)
        {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        } /* arg not yet set — ignore */
        switch (LOWORD(wp))
        {
        case IDC_CHK_REPO_TOKEN:
            /* Enable or disable the token text field to match the checkbox state */
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REPO_TOKEN),
                         IsDlgButtonChecked(hwnd, IDC_CHK_REPO_TOKEN) == BST_CHECKED);
            break;
        case IDOK:
        {
            WCHAR url[512] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_REPO_URL, url, 511);
            if (!url[0])
            {
                /* Reject empty URL — repo cannot be identified */
                MessageBox(hwnd, L"Please enter a GitHub URL.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                return TRUE; /* stay in dialog */
            }
            /* Basic domain check — full URL validation is left to the sync engine */
            if (!wcsstr(url, L"github.com"))
            {
                MessageBox(hwnd,
                           L"URL must be a GitHub repository URL.\n"
                           L"Example: https://github.com/owner/repo",
                           L"Sources", MB_ICONWARNING | MB_OK);
                return TRUE; /* stay in dialog */
            }
            wcsncpy_s(arg->repo->url, 512, url, _TRUNCATE);
            GetDlgItemText(hwnd, IDC_EDIT_REPO_BRANCH,
                           arg->repo->branch, 63);
            if (!arg->repo->branch[0])
                wcsncpy_s(arg->repo->branch, 64, L"main", _TRUNCATE); /* default branch if field left blank */
            if (IsDlgButtonChecked(hwnd, IDC_CHK_REPO_TOKEN) == BST_CHECKED)
                GetDlgItemText(hwnd, IDC_EDIT_REPO_TOKEN,
                               arg->repo->token, 255);
            else
                arg->repo->token[0] = L'\0'; /* clear token if checkbox is unchecked */
            arg->repo->enabled =
                IsDlgButtonChecked(hwnd, IDC_CHK_REPO_ENABLED) == BST_CHECKED;
            EndDialog(hwnd, IDOK);
            break;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL); /* discard changes */
            break;
        }
        return TRUE;
    }
    return FALSE; /* unhandled message — let DefDlgProc handle it */
}

/* ================================================================== */
/*  SourcesDlgProc                                                      */
/*  Purpose: Dialog procedure for the Sources dialog (IDD_SOURCES).    */
/*           Manages the main-repo enable checkbox, extra GitHub repo  */
/*           list (add/edit/toggle/remove via RepoEditDlgProc), and   */
/*           local folder list (add/toggle/remove).  IDOK saves via   */
/*           Settings_Save; caller re-syncs if IDOK is returned.      */
/*  In:  hwnd — dialog handle                                          */
/*       msg  — Windows message                                        */
/*       wp   — WPARAM (button ID in WM_COMMAND LOWORD)               */
/*       lp   — LPARAM (unused)                                        */
/*  Out: INT_PTR — TRUE for handled; FALSE otherwise; IDOK/IDCANCEL   */
/*                 via EndDialog                                        */
/* ================================================================== */
INT_PTR CALLBACK SourcesDlgProc(HWND hwnd, UINT msg,
                                WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        /* Set up repo list columns */
        HWND hRepos = GetDlgItem(hwnd, IDC_LST_REPOS);
        HWND hLocals = GetDlgItem(hwnd, IDC_LST_LOCAL);

        LVCOLUMN lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

        lvc.pszText = L"Repository URL";
        lvc.cx = 180;
        ListView_InsertColumn(hRepos, 0, &lvc);
        lvc.pszText = L"Branch";
        lvc.cx = 60;
        ListView_InsertColumn(hRepos, 1, &lvc);
        lvc.pszText = L"Enabled";
        lvc.cx = 46;
        ListView_InsertColumn(hRepos, 2, &lvc);

        lvc.pszText = L"Local Folder Path";
        lvc.cx = 220;
        ListView_InsertColumn(hLocals, 0, &lvc);
        lvc.pszText = L"Enabled";
        lvc.cx = 46;
        ListView_InsertColumn(hLocals, 1, &lvc);

        CheckDlgButton(hwnd, IDC_CHK_MAIN_REPO,
                       g.cfg.main_repo_enabled ? BST_CHECKED : BST_UNCHECKED);

        Repos_Populate(hRepos);
        Locals_Populate(hLocals);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        /* ── Main repo toggle ── */
        case IDC_CHK_MAIN_REPO:
            g.cfg.main_repo_enabled =
                IsDlgButtonChecked(hwnd, IDC_CHK_MAIN_REPO) == BST_CHECKED;
            break;

        /* ── Extra repos ── */
        case IDC_BTN_REPO_ADD:
            if (g.cfg.extra_repo_count >= MAX_EXTRA_REPOS)
            {
                /* Array is fixed-size — cannot grow further */
                MessageBox(hwnd, L"Maximum number of repositories reached.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                break;
            }
            {
                ExtraRepo tmp = {0};
                tmp.enabled = true;
                wcsncpy_s(tmp.branch, 64, L"main", _TRUNCATE);
                RepoEditArg arg = {&tmp, true};
                if (DialogBoxParam(GetModuleHandle(NULL),
                                   MAKEINTRESOURCE(IDD_REPO_EDIT),
                                   hwnd, RepoEditDlgProc, (LPARAM)&arg) == IDOK)
                {
                    g.cfg.extra_repos[g.cfg.extra_repo_count++] = tmp;
                    Repos_Populate(GetDlgItem(hwnd, IDC_LST_REPOS));
                }
            }
            break;

        case IDC_BTN_REPO_EDIT:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED); /* -1 = start from beginning; returns -1 if nothing selected */
            if (sel < 0) break; /* no selection — ignore button click */
            RepoEditArg arg = {&g.cfg.extra_repos[sel], false};
            if (DialogBoxParam(GetModuleHandle(NULL),
                               MAKEINTRESOURCE(IDD_REPO_EDIT),
                               hwnd, RepoEditDlgProc, (LPARAM)&arg) == IDOK)
                Repos_Populate(hList);
            break;
        }

        case IDC_BTN_REPO_TOGGLE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break; /* nothing selected */
            g.cfg.extra_repos[sel].enabled = !g.cfg.extra_repos[sel].enabled;
            Repos_Populate(hList);
            break;
        }

        case IDC_BTN_REPO_REMOVE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;

            ExtraRepo *repo = &g.cfg.extra_repos[sel];

            /* Parse owner/repo from URL for cache path */
            WCHAR owner[MAX_NAME] = {0}, reponame[MAX_NAME] = {0};
            bool has_cache = GitHub_ParseOwnerRepo(repo->url, owner, reponame);

            /* Build confirmation message */
            WCHAR confirm[512];
            if (has_cache)
            {
                _snwprintf_s(confirm, 511, _TRUNCATE, L"Remove repository:\n%s\n\n"
                                                      L"Delete cached scripts and requirements from:\n"
                                                      L"%s\\%s_%s\n\n"
                                                      L"This cannot be undone.",
                             repo->url, g.cfg.cache_dir, owner, reponame);
            }
            else
            {
                _snwprintf_s(confirm, 511, _TRUNCATE, L"Remove repository:\n%s\n\n"
                                                      L"No cached files found to delete.",
                             repo->url);
            }

            int res = MessageBox(hwnd, confirm, L"Remove Repository",
                                 MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2); /* MB_DEFBUTTON2 = default to "No" for safety */
            if (res != IDYES) break; /* user cancelled — leave the repo list unchanged */

            /* Delete cached scripts folder: cache_dir\owner_reponame\ */
            if (has_cache)
            {
                WCHAR scripts_dir[MAX_APPPATH];
                _snwprintf_s(scripts_dir, MAX_APPPATH, _TRUNCATE, L"%s\\%s_%s",
                             g.cfg.cache_dir, owner, reponame);
                DeleteFolderRecursive(scripts_dir);

                /* Delete cached requirements.txt */
                WCHAR req[MAX_APPPATH];
                _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\%s_%s\\requirements.txt",
                             g.cfg.cache_dir, owner, reponame);
                DeleteFileW(req);
                WCHAR req_dir[MAX_APPPATH];
                _snwprintf_s(req_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\%s_%s",
                             g.cfg.cache_dir, owner, reponame);
                RemoveDirectoryW(req_dir);
            }

            /* Remove by shifting all entries after sel one position left */
            for (int i = sel; i < g.cfg.extra_repo_count - 1; i++)
                g.cfg.extra_repos[i] = g.cfg.extra_repos[i + 1];
            g.cfg.extra_repo_count--;
            Repos_Populate(hList);
            break;
        }

        /* ── Local dirs ── */
        case IDC_BTN_LOCAL_ADD:
            if (g.cfg.local_dir_count >= MAX_LOCAL_DIRS)
            {
                /* Fixed-size array — cannot add more entries */
                MessageBox(hwnd, L"Maximum number of local folders reached.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                break;
            }
            {
                WCHAR path[MAX_APPPATH] = {0};
                BROWSEINFO bi = {
                    .hwndOwner = hwnd,
                    .lpszTitle = L"Select Script Folder",
                    .ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS};
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
                if (pidl)
                {
                    SHGetPathFromIDList(pidl, path);
                    CoTaskMemFree(pidl);
                    if (path[0])
                    {
                        LocalDir d = {0};
                        wcsncpy_s(d.path, MAX_APPPATH, path, _TRUNCATE);
                        d.enabled = true;
                        g.cfg.local_dirs[g.cfg.local_dir_count++] = d;
                        Locals_Populate(GetDlgItem(hwnd, IDC_LST_LOCAL));
                    }
                }
            }
            break;

        case IDC_BTN_LOCAL_TOGGLE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_LOCAL);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break; /* nothing selected */
            g.cfg.local_dirs[sel].enabled = !g.cfg.local_dirs[sel].enabled;
            Locals_Populate(hList);
            break;
        }

        case IDC_BTN_LOCAL_REMOVE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_LOCAL);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break; /* nothing selected */
            /* Remove by shifting entries after sel one position left */
            for (int i = sel; i < g.cfg.local_dir_count - 1; i++)
                g.cfg.local_dirs[i] = g.cfg.local_dirs[i + 1];
            g.cfg.local_dir_count--;
            Locals_Populate(hList);
            break;
        }

        case IDOK:
            Settings_Save(&g.cfg); /* persist source list changes before closing */
            EndDialog(hwnd, IDOK);
            break;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL); /* discard any in-memory changes — settings were not saved */
            break;
        }
        return TRUE;
    }
    return FALSE; /* unhandled message */
}
