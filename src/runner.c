/*
 * runner.c  -  Execute a PyCATIA script via python.exe.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

typedef struct {
    WCHAR python[MAX_APPPATH];
    WCHAR script[MAX_APPPATH];
    bool  show_console;
    bool  keep_open;
} RunArg;

/* ================================================================== */
/*  Runner_FindPython                                                   */
/* ================================================================== */
bool Runner_FindPython(WCHAR *out, int max)
{
    if (g.cfg.python_exe[0] &&
        GetFileAttributes(g.cfg.python_exe) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy(out, g.cfg.python_exe, max - 1);
        return true;
    }
    WCHAR found[MAX_APPPATH];
    if (SearchPath(NULL, L"python.exe", NULL, MAX_APPPATH, found, NULL)) {
        wcsncpy(out, found, max - 1);
        return true;
    }
    static const WCHAR *candidates[] = {
        L"C:\\Python313\\python.exe",
        L"C:\\Python312\\python.exe",
        L"C:\\Python311\\python.exe",
        L"C:\\Python310\\python.exe",
        L"C:\\Python39\\python.exe",
        L"C:\\Program Files\\Python313\\python.exe",
        L"C:\\Program Files\\Python312\\python.exe",
        L"C:\\Program Files\\Python311\\python.exe",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (GetFileAttributes(candidates[i]) != INVALID_FILE_ATTRIBUTES) {
            wcsncpy(out, candidates[i], max - 1);
            return true;
        }
    }
    return false;
}

/* ================================================================== */
/*  Runner_BuildLocalPath                                               */
/* ================================================================== */
static void Runner_BuildLocalPath(int fi, int si, WCHAR *out, int max)
{
    const WCHAR *gh    = g.folders[fi].scripts[si].gh_path;
    const WCHAR *sep   = wcsrchr(gh, L'/');
    const WCHAR *fname = sep ? sep + 1 : gh;
    _snwprintf(out, max - 1, L"%s\\%s\\%s",
               g.cfg.cache_dir, g.folders[fi].name, fname);
    out[max - 1] = L'\0';
}

/* ================================================================== */
/*  Runner_Thread                                                       */
/* ================================================================== */
DWORD WINAPI Runner_Thread(LPVOID arg)
{
    RunArg *ra = (RunArg *)arg;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);

    WCHAR cmd[MAX_APPPATH * 2];

    if (ra->show_console && ra->keep_open) {
        /* Wrap in cmd.exe /k so the window stays open after script ends */
        _snwprintf(cmd, MAX_APPPATH * 2 - 1,
                   L"cmd.exe /k \"\"%s\" \"%s\"\"",
                   ra->python, ra->script);
    } else {
        _snwprintf(cmd, MAX_APPPATH * 2 - 1,
                   L"\"%s\" \"%s\"", ra->python, ra->script);
    }
    cmd[MAX_APPPATH * 2 - 1] = L'\0';

    DWORD flags = ra->show_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       flags, NULL, NULL, &si, &pi)) {
        if (!ra->show_console) {
            /* Background run - wait and report exit code */
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            if (exit_code == 0)
                PostStatus(L"Script finished successfully.");
            else
                PostStatus(L"Script exited with code %lu.", exit_code);
        } else {
            PostStatus(L"Script launched in console.");
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        DWORD err = GetLastError();
        PostStatus(L"Failed to launch Python. Error %lu.", err);
    }

    free(ra);
    return 0;
}

/* ================================================================== */
/*  Runner_Run                                                          */
/* ================================================================== */
bool Runner_Run(int fi, int si)
{
    if (fi < 0 || fi >= g.folder_count)      return false;
    if (si < 0 || si >= g.folders[fi].count) return false;

    Script *s = &g.folders[fi].scripts[si];

    WCHAR local[MAX_APPPATH] = {0};
    Runner_BuildLocalPath(fi, si, local, MAX_APPPATH);

    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH)) {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File \u2192 Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return false;
    }

    bool missing = (GetFileAttributes(local) == INVALID_FILE_ATTRIBUTES);
    if (missing || g.cfg.download_before_run) {
        PostStatus(L"Downloading %s\u2026", s->name);
        const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
        if (!GitHub_DownloadRaw(s->gh_path, local, tok)) {
            MessageBox(g.hwnd,
                       L"Failed to download script from GitHub.",
                       L"Download Error", MB_ICONERROR | MB_OK);
            return false;
        }
        wcsncpy(s->local, local, MAX_APPPATH - 1);
    }

    /* SHA verification - confirm local file matches GitHub blob SHA */
    if (s->sha[0] && !GitHub_VerifyScriptSHA(s)) {
        int res = MessageBox(g.hwnd,
            L"Warning: Script SHA does not match the expected value from GitHub.\n\n"
            L"The local file may have been tampered with.\n\n"
            L"Do you want to re-download and run the script?",
            L"Security Warning", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        if (res == IDYES) {
            /* Force re-download */
            const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
            if (!GitHub_DownloadRaw(s->gh_path, local, tok)) {
                MessageBox(g.hwnd, L"Failed to re-download script.",
                           L"Error", MB_ICONERROR | MB_OK);
                return false;
            }
            /* Verify again after re-download */
            if (!GitHub_VerifyScriptSHA(s)) {
                MessageBox(g.hwnd,
                    L"SHA still does not match after re-download.\n"
                    L"Script will not run.",
                    L"Security Error", MB_ICONERROR | MB_OK);
                return false;
            }
        } else {
            return false;
        }
    }

    wcsncpy(g.last_run_path, s->gh_path, MAX_APPPATH - 1);
    PostStatus(L"Running: %s", s->name);

    RunArg *ra = (RunArg *)malloc(sizeof(RunArg));
    if (!ra) return false;
    wcsncpy(ra->python, python, MAX_APPPATH - 1);
    wcsncpy(ra->script, local,  MAX_APPPATH - 1);
    ra->show_console = g.cfg.show_console;
    ra->keep_open    = g.cfg.show_console && g.cfg.console_keep_open;

    HANDLE hT = CreateThread(NULL, 0, Runner_Thread, ra, 0, NULL);
    if (hT) { CloseHandle(hT); return true; }
    free(ra);
    return false;
}

/* ================================================================== */
/*  Runner_UpdateDeps                                                   */
/* ================================================================== */
/* ------------------------------------------------------------------ */
/*  Run a single pip install -r <req> in its own console window        */
/* ------------------------------------------------------------------ */
static void RunPipInstall(const WCHAR *python, const WCHAR *req,
                           const WCHAR *work_dir, bool keep_open)
{
    WCHAR cmd[MAX_APPPATH * 2];
    if (keep_open) {
        /* cmd /k requires the inner command wrapped in extra quotes */
        _snwprintf(cmd, MAX_APPPATH * 2 - 1,
                   L"cmd.exe /k \"\"%s\" -m pip install -r \"%s\"\"",
                   python, req);
    } else {
        _snwprintf(cmd, MAX_APPPATH * 2 - 1,
                   L"cmd.exe /c \"%s\" -m pip install -r \"%s\"",
                   python, req);
    }

    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       CREATE_NEW_CONSOLE, NULL, work_dir, &si, &pi)) {
        if (!keep_open) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void Runner_UpdateDeps(void)
{
    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH)) {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File \u2192 Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return;
    }

    /* ── Collect all requirements files to run ────────────────────── */
    /* We run each separately so pip resolves each independently.      */
    /* Order: main repo bat/req, extra repo reqs, local dir reqs.     */

    bool found_any = false;

    /* 1. Main repo: try update.bat first, then requirements.txt */
    if (g.cfg.main_repo_enabled) {
        WCHAR req[MAX_APPPATH];
        _snwprintf(req, MAX_APPPATH - 1, L"%s\\setup\\requirements.txt",
                   g.cfg.cache_dir);
        WCHAR work_dir[MAX_APPPATH];
        _snwprintf(work_dir, MAX_APPPATH - 1, L"%s\\setup", g.cfg.cache_dir);
        SHCreateDirectoryEx(NULL, work_dir, NULL);

        /* Download if missing */
        if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES) {
            SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                        (LPARAM)L"Downloading main repo requirements.txt...");
            const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
            GitHub_DownloadRaw(L"setup/requirements.txt", req, tok);
        }

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
            SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                        (LPARAM)L"Installing main repo requirements...");
            RunPipInstall(python, req, work_dir, g.cfg.deps_keep_open);
            found_any = true;
        }
    }

    /* 2. Extra GitHub repos: each has its own cached requirements.txt */
    for (int i = 0; i < g.cfg.extra_repo_count; i++) {
        ExtraRepo *repo = &g.cfg.extra_repos[i];
        if (!repo->enabled || !repo->url[0]) continue;

        WCHAR owner[MAX_NAME] = {0}, reponame[MAX_NAME] = {0};
        if (!GitHub_ParseOwnerRepo(repo->url, owner, reponame)) continue;

        /* Cache path: cache_dir\setup\owner_reponame
equirements.txt */
        WCHAR sub_dir[MAX_APPPATH];
        _snwprintf(sub_dir, MAX_APPPATH - 1, L"%s\\setup\\%s_%s",
                   g.cfg.cache_dir, owner, reponame);
        SHCreateDirectoryEx(NULL, sub_dir, NULL);

        WCHAR req[MAX_APPPATH];
        _snwprintf(req, MAX_APPPATH - 1, L"%s\\requirements.txt", sub_dir);

        /* Download if missing */
        if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES) {
            const WCHAR *tok = repo->token[0] ? repo->token
                             : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
            const WCHAR *branch = repo->branch[0] ? repo->branch : L"main";
            WCHAR raw_path[MAX_APPPATH];
            _snwprintf(raw_path, MAX_APPPATH - 1,
                       L"/%s/%s/%s/setup/requirements.txt",
                       owner, reponame, branch);
            GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, req, tok);
        }

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
            WCHAR status[256];
            _snwprintf(status, 255, L"Installing %s/%s requirements...",
                       owner, reponame);
            SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)status);
            RunPipInstall(python, req, sub_dir, g.cfg.deps_keep_open);
            found_any = true;
        }
    }

    /* 3. Local dirs: run directly from source - no caching needed */
    for (int i = 0; i < g.cfg.local_dir_count; i++) {
        LocalDir *dir = &g.cfg.local_dirs[i];
        if (!dir->enabled || !dir->path[0]) continue;

        WCHAR req[MAX_APPPATH];
        _snwprintf(req, MAX_APPPATH - 1, L"%s\\setup\\requirements.txt",
                   dir->path);

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
            WCHAR work_dir[MAX_APPPATH];
            _snwprintf(work_dir, MAX_APPPATH - 1, L"%s\\setup", dir->path);
            WCHAR status[256];
            _snwprintf(status, 255, L"Installing local folder requirements (%d)...",
                       i + 1);
            SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)status);
            RunPipInstall(python, req, work_dir, g.cfg.deps_keep_open);
            found_any = true;
        }
    }

    if (!found_any) {
        MessageBox(g.hwnd,
                   L"No requirements.txt or update.bat found in any source.\n\n"
                   L"Make sure at least one source has a setup folder containing\n"
                   L"requirements.txt or update.bat.",
                   L"Update Dependencies", MB_ICONWARNING | MB_OK);
        return;
    }

    SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                (LPARAM)L"All dependencies updated successfully.");
}
