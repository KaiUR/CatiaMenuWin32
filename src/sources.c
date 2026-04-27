/*
 * sources.c  -  Sources dialog: manage extra GitHub repos and local folders.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  DeleteFolderRecursive  -  remove a directory and all its contents   */
/* ================================================================== */
static void DeleteFolderRecursive(const WCHAR *path)
{
    WCHAR pattern[MAX_APPPATH];
    _snwprintf(pattern, MAX_APPPATH - 1, L"%s\\*", path);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 ||
            wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR full[MAX_APPPATH];
        _snwprintf(full, MAX_APPPATH - 1, L"%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            DeleteFolderRecursive(full);
        else
            DeleteFileW(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    RemoveDirectoryW(path);
}

/* ================================================================== */
/*  Helper: populate the repo list view                                 */
/* ================================================================== */
static void Repos_Populate(HWND hList)
{
    ListView_DeleteAllItems(hList);
    for (int i = 0; i < g.cfg.extra_repo_count; i++) {
        ExtraRepo *r = &g.cfg.extra_repos[i];
        LVITEM lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.iSubItem = 0;
        lvi.pszText = r->url;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, r->branch[0] ? r->branch : L"main");
        ListView_SetItemText(hList, i, 2, r->enabled ? L"Yes" : L"No");
        WCHAR *health_str = r->health == HEALTH_OK    ? L"OK"
                          : r->health == HEALTH_ERROR ? L"Error"
                          :                             L"Unknown";
        ListView_SetItemText(hList, i, 3, health_str);
        ListView_SetItemText(hList, i, 4,
            r->last_sync[0] ? r->last_sync : L"Never");
    }
}

static void Locals_Populate(HWND hList)
{
    ListView_DeleteAllItems(hList);
    for (int i = 0; i < g.cfg.local_dir_count; i++) {
        LocalDir *d = &g.cfg.local_dirs[i];
        LVITEM lvi = {0};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = i;
        lvi.pszText  = d->path;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, d->enabled ? L"Yes" : L"No");
    }
}

/* ================================================================== */
/*  RepoEdit dialog                                                     */
/* ================================================================== */
typedef struct { ExtraRepo *repo; bool is_new; } RepoEditArg;

static INT_PTR CALLBACK RepoEditDlgProc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp)
{
    RepoEditArg *arg = (RepoEditArg *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_INITDIALOG:
        arg = (RepoEditArg *)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)arg);
        if (!arg || !arg->repo) { EndDialog(hwnd, IDCANCEL); return FALSE; }
        SetDlgItemText(hwnd, IDC_EDIT_REPO_URL,    arg->repo->url);
        SetDlgItemText(hwnd, IDC_EDIT_REPO_BRANCH, arg->repo->branch[0]
                                                    ? arg->repo->branch : L"main");
        CheckDlgButton(hwnd, IDC_CHK_REPO_TOKEN,
            arg->repo->token[0] ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemText(hwnd, IDC_EDIT_REPO_TOKEN, arg->repo->token);
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REPO_TOKEN),
                     arg->repo->token[0] != 0);
        CheckDlgButton(hwnd, IDC_CHK_REPO_ENABLED,
            arg->repo->enabled ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        if (!arg) { EndDialog(hwnd, IDCANCEL); return TRUE; }
        switch (LOWORD(wp)) {
        case IDC_CHK_REPO_TOKEN:
            EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REPO_TOKEN),
                IsDlgButtonChecked(hwnd, IDC_CHK_REPO_TOKEN) == BST_CHECKED);
            break;
        case IDOK:
        {
            WCHAR url[512] = {0};
            GetDlgItemText(hwnd, IDC_EDIT_REPO_URL, url, 511);
            if (!url[0]) {
                MessageBox(hwnd, L"Please enter a GitHub URL.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                return TRUE;
            }
            /* Validate it looks like a github URL */
            if (!wcsstr(url, L"github.com")) {
                MessageBox(hwnd,
                    L"URL must be a GitHub repository URL.\n"
                    L"Example: https://github.com/owner/repo",
                    L"Sources", MB_ICONWARNING | MB_OK);
                return TRUE;
            }
            wcsncpy(arg->repo->url, url, 511);
            GetDlgItemText(hwnd, IDC_EDIT_REPO_BRANCH,
                           arg->repo->branch, 63);
            if (!arg->repo->branch[0])
                wcscpy(arg->repo->branch, L"main");
            if (IsDlgButtonChecked(hwnd, IDC_CHK_REPO_TOKEN) == BST_CHECKED)
                GetDlgItemText(hwnd, IDC_EDIT_REPO_TOKEN,
                               arg->repo->token, 255);
            else
                arg->repo->token[0] = L'\0';
            arg->repo->enabled =
                IsDlgButtonChecked(hwnd, IDC_CHK_REPO_ENABLED) == BST_CHECKED;
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
/*  SourcesDlgProc                                                      */
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
        HWND hRepos  = GetDlgItem(hwnd, IDC_LST_REPOS);
        HWND hLocals = GetDlgItem(hwnd, IDC_LST_LOCAL);

        LVCOLUMN lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

        lvc.pszText = L"Repository URL"; lvc.cx = 180;
        ListView_InsertColumn(hRepos, 0, &lvc);
        lvc.pszText = L"Branch";         lvc.cx = 60;
        ListView_InsertColumn(hRepos, 1, &lvc);
        lvc.pszText = L"Enabled";        lvc.cx = 46;
        ListView_InsertColumn(hRepos, 2, &lvc);
        lvc.pszText = L"Health";         lvc.cx = 60;
        ListView_InsertColumn(hRepos, 3, &lvc);
        lvc.pszText = L"Last Sync";      lvc.cx = 100;
        ListView_InsertColumn(hRepos, 4, &lvc);

        lvc.pszText = L"Local Folder Path"; lvc.cx = 220;
        ListView_InsertColumn(hLocals, 0, &lvc);
        lvc.pszText = L"Enabled";           lvc.cx = 46;
        ListView_InsertColumn(hLocals, 1, &lvc);

        CheckDlgButton(hwnd, IDC_CHK_MAIN_REPO,
            g.cfg.main_repo_enabled ? BST_CHECKED : BST_UNCHECKED);
        /* Show main repo health inline */
        WCHAR *mh = g.main_repo_health == HEALTH_OK    ? L"Health: OK"
                  : g.main_repo_health == HEALTH_ERROR ? L"Health: Error"
                  :                                      L"Health: Unknown";
        SetDlgItemText(hwnd, IDC_MAIN_REPO_HEALTH, mh);
        if (g.main_repo_last_sync[0])
            SetWindowText(hwnd, L"Script Sources");

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
            if (g.cfg.extra_repo_count >= MAX_EXTRA_REPOS) {
                MessageBox(hwnd, L"Maximum number of repositories reached.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                break;
            }
            {
                ExtraRepo tmp = {0};
                tmp.enabled = true;
                wcscpy(tmp.branch, L"main");
                RepoEditArg arg = { &tmp, true };
                if (DialogBoxParam(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(IDD_REPO_EDIT),
                        hwnd, RepoEditDlgProc, (LPARAM)&arg) == IDOK) {
                    g.cfg.extra_repos[g.cfg.extra_repo_count++] = tmp;
                    Repos_Populate(GetDlgItem(hwnd, IDC_LST_REPOS));
                }
            }
            break;

        case IDC_BTN_REPO_EDIT:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int  sel   = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;
            RepoEditArg arg = { &g.cfg.extra_repos[sel], false };
            if (DialogBoxParam(GetModuleHandle(NULL),
                    MAKEINTRESOURCE(IDD_REPO_EDIT),
                    hwnd, RepoEditDlgProc, (LPARAM)&arg) == IDOK)
                Repos_Populate(hList);
            break;
        }

        case IDC_BTN_REPO_TOGGLE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int  sel   = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;
            g.cfg.extra_repos[sel].enabled = !g.cfg.extra_repos[sel].enabled;
            Repos_Populate(hList);
            break;
        }

        case IDC_BTN_REPO_REMOVE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_REPOS);
            int  sel   = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;

            ExtraRepo *repo = &g.cfg.extra_repos[sel];

            /* Parse owner/repo from URL for cache path */
            WCHAR owner[MAX_NAME] = {0}, reponame[MAX_NAME] = {0};
            bool has_cache = GitHub_ParseOwnerRepo(repo->url, owner, reponame);

            /* Build confirmation message */
            WCHAR msg[512];
            if (has_cache) {
                _snwprintf(msg, 511,
                    L"Remove repository:\n%s\n\n"
                    L"Delete cached scripts and requirements from:\n"
                    L"%s\\%s_%s\n\n"
                    L"This cannot be undone.",
                    repo->url, g.cfg.cache_dir, owner, reponame);
            } else {
                _snwprintf(msg, 511,
                    L"Remove repository:\n%s\n\n"
                    L"No cached files found to delete.",
                    repo->url);
            }

            int res = MessageBox(hwnd, msg, L"Remove Repository",
                                 MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
            if (res != IDYES) break;

            /* Delete cached scripts folder: cache_dir\owner_reponame\ */
            if (has_cache) {
                WCHAR scripts_dir[MAX_APPPATH];
                _snwprintf(scripts_dir, MAX_APPPATH - 1, L"%s\\%s_%s",
                           g.cfg.cache_dir, owner, reponame);
                DeleteFolderRecursive(scripts_dir);

                /* Delete cached requirements.txt */
                WCHAR req[MAX_APPPATH];
                _snwprintf(req, MAX_APPPATH - 1,
                           L"%s\\setup\\%s_%s\\requirements.txt",
                           g.cfg.cache_dir, owner, reponame);
                DeleteFileW(req);
                WCHAR req_dir[MAX_APPPATH];
                _snwprintf(req_dir, MAX_APPPATH - 1,
                           L"%s\\setup\\%s_%s",
                           g.cfg.cache_dir, owner, reponame);
                RemoveDirectoryW(req_dir);
            }

            /* Shift array left */
            for (int i = sel; i < g.cfg.extra_repo_count - 1; i++)
                g.cfg.extra_repos[i] = g.cfg.extra_repos[i + 1];
            g.cfg.extra_repo_count--;
            Repos_Populate(hList);
            break;
        }

        /* ── Local dirs ── */
        case IDC_BTN_LOCAL_ADD:
            if (g.cfg.local_dir_count >= MAX_LOCAL_DIRS) {
                MessageBox(hwnd, L"Maximum number of local folders reached.",
                           L"Sources", MB_ICONWARNING | MB_OK);
                break;
            }
            {
                WCHAR path[MAX_APPPATH] = {0};
                BROWSEINFO bi = {
                    .hwndOwner = hwnd,
                    .lpszTitle = L"Select Script Folder",
                    .ulFlags   = BIF_USENEWUI | BIF_RETURNONLYFSDIRS
                };
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
                if (pidl) {
                    SHGetPathFromIDList(pidl, path);
                    CoTaskMemFree(pidl);
                    if (path[0]) {
                        LocalDir d = {0};
                        wcsncpy(d.path, path, MAX_APPPATH - 1);
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
            int  sel   = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;
            g.cfg.local_dirs[sel].enabled = !g.cfg.local_dirs[sel].enabled;
            Locals_Populate(hList);
            break;
        }

        case IDC_BTN_LOCAL_REMOVE:
        {
            HWND hList = GetDlgItem(hwnd, IDC_LST_LOCAL);
            int  sel   = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel < 0) break;
            for (int i = sel; i < g.cfg.local_dir_count - 1; i++)
                g.cfg.local_dirs[i] = g.cfg.local_dirs[i + 1];
            g.cfg.local_dir_count--;
            Locals_Populate(hList);
            break;
        }

        case IDOK:
            Settings_Save(&g.cfg);
            EndDialog(hwnd, IDOK);
            break;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;
    }
    return FALSE;
}
