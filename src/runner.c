/*
 * runner.c  -  Execute a PyCATIA script via python.exe.
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  RunArg                                                              */
/*  Purpose: Heap-allocated argument block passed to Runner_Thread.    */
/*           Carries all data the thread needs so no globals are read  */
/*           after the launch call returns.                            */
/*  In:  (allocated and populated by Runner_Run before CreateThread)   */
/*  Out: (freed by Runner_Thread on completion)                        */
/* ================================================================== */
typedef struct {
    WCHAR python[MAX_APPPATH];
    WCHAR script[MAX_APPPATH];
    bool  show_console;
    bool  keep_open;
} RunArg;

/* ================================================================== */
/*  Runner_FindPython                                                   */
/*  Purpose: Locates a Python executable by checking (in order) the    */
/*           user-configured path in settings, PATH via SearchPath,   */
/*           and a list of common installation directories.             */
/*  In:  out — buffer to receive the full path on success              */
/*       max — capacity of out in WCHARs                               */
/*  Out: true and out filled if Python was found; false otherwise       */
/* ================================================================== */
bool Runner_FindPython(WCHAR *out, int max)
{
    if (g.cfg.python_exe[0] &&
        GetFileAttributes(g.cfg.python_exe) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy_s(out, max, g.cfg.python_exe, _TRUNCATE);
        return true;
    }
    WCHAR found[MAX_APPPATH];
    if (SearchPath(NULL, L"python.exe", NULL, MAX_APPPATH, found, NULL)) {
        wcsncpy_s(out, max, found, _TRUNCATE);
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
            wcsncpy_s(out, max, candidates[i], _TRUNCATE);
            return true;
        }
    }
    return false;
}

/* ================================================================== */
/*  Runner_BuildLocalPath  (static)                                    */
/*  Purpose: Constructs the local disk path for a cached script by     */
/*           combining the cache directory, folder name, and the       */
/*           filename portion of the script's GitHub path.             */
/*  In:  fi  — folder index                                            */
/*       si  — script index within that folder                         */
/*       out — buffer to receive the full local path                   */
/*       max — capacity of out in WCHARs                               */
/*  Out: (void — out is populated)                                      */
/* ================================================================== */
static void Runner_BuildLocalPath(int fi, int si, WCHAR *out, int max)
{
    const WCHAR *gh    = g.folders[fi].scripts[si].gh_path;
    const WCHAR *sep   = wcsrchr(gh, L'/');
    const WCHAR *fname = sep ? sep + 1 : gh;
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\%s\\%s",
               g.cfg.cache_dir, g.folders[fi].name, fname);
    out[max - 1] = L'\0';
}

/* ================================================================== */
/*  Runner_Thread                                                       */
/*  Purpose: Background thread that executes the Python script via     */
/*           CreateProcessW.  When show_console is false it waits for  */
/*           the process and posts the exit code to the status bar.   */
/*           Frees the RunArg before returning.                        */
/*  In:  arg — pointer to a heap-allocated RunArg (freed on return)    */
/*  Out: 0 (thread exit code)                                           */
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
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE, L"cmd.exe /k \"\"%s\" \"%s\"\"",
                   ra->python, ra->script);
    } else {
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE, L"\"%s\" \"%s\"", ra->python, ra->script);
    }
    cmd[MAX_APPPATH * 2 - 1] = L'\0';

    DWORD flags = ra->show_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       flags, NULL, NULL, &si, &pi)) {
        if (!ra->show_console) {
            /* Background run: duplicate handle into g.run_process so Runner_Stop can kill it */
            HANDLE dup = NULL;
            DuplicateHandle(GetCurrentProcess(), pi.hProcess,
                            GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
            InterlockedExchangePointer((void **)&g.run_process, dup);
            PostMessage(g.hwnd, WM_SCRIPT_STARTED, 0, 0);

            WaitForSingleObject(pi.hProcess, 30 * 60 * 1000);

            /* Atomically take back the handle. If NULL, Runner_Stop already claimed it. */
            HANDLE old = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
            if (old) {
                CloseHandle(old);
                DWORD exit_code = 0;
                GetExitCodeProcess(pi.hProcess, &exit_code);
                PostMessage(g.hwnd, WM_SCRIPT_STOPPED, (WPARAM)exit_code, 0);
                if (exit_code == 0)
                    PostStatus(L"Script finished successfully.");
                else
                    PostStatus(L"Script exited with code %lu.", exit_code);
            }
            /* else: Runner_Stop already posted WM_SCRIPT_STOPPED and its own status */
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
/*  Purpose: Validates folder/script indices, locates Python, downloads */
/*           the script if missing or if download_before_run is set,  */
/*           performs SHA verification, then launches Runner_Thread.   */
/*           Increments the run count and updates last_run_path.       */
/*  In:  fi — folder index                                             */
/*       si — script index within that folder                          */
/*  Out: true if the thread was successfully launched; false on error   */
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
        wcsncpy_s(s->local, MAX_APPPATH, local, _TRUNCATE);
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

    wcsncpy_s(g.last_run_path, MAX_APPPATH, s->gh_path, _TRUNCATE);
    Prefs_IncrementRunCount(s->gh_path);
    s->run_count++;
    PostStatus(L"Running: %s", s->name);

    RunArg *ra = (RunArg *)malloc(sizeof(RunArg));
    if (!ra) return false;
    wcsncpy_s(ra->python, MAX_APPPATH, python, _TRUNCATE);
    wcsncpy_s(ra->script, MAX_APPPATH, local,  _TRUNCATE);
    ra->show_console = g.cfg.show_console;
    ra->keep_open    = g.cfg.show_console && g.cfg.console_keep_open;

    HANDLE hT = CreateThread(NULL, 0, Runner_Thread, ra, 0, NULL);
    if (hT) { CloseHandle(hT); return true; }
    free(ra);
    return false;
}

/* ================================================================== */
/*  Runner_Stop                                                         */
/*  Purpose: Terminates the currently running background script, if    */
/*           any.  Atomically claims g.run_process so only one caller  */
/*           can win; the other gets NULL and does nothing.  Posts     */
/*           WM_SCRIPT_STOPPED so the UI disables the Stop button.    */
/*  In:  (reads/clears g.run_process)                                  */
/*  Out: (void)                                                         */
/* ================================================================== */
void Runner_Stop(void)
{
    HANDLE h = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
    if (!h) return;
    TerminateProcess(h, 1);
    CloseHandle(h);
    PostStatus(L"Script stopped.");
    PostMessage(g.hwnd, WM_SCRIPT_STOPPED, 0, 0);
}

/* ================================================================== */
/*  RunPipInstall  (static)                                            */
/*  Purpose: Runs `python -m pip install -r <req>` in a new console   */
/*           window.  When keep_open is true uses `cmd /k` so the     */
/*           console stays visible after pip finishes.  Otherwise uses */
/*           `cmd /c` and waits for the process to complete.           */
/*  In:  python   — full path to the Python executable                 */
/*       req      — full path to the requirements.txt file             */
/*       work_dir — working directory passed to CreateProcessW         */
/*       keep_open — true = /k (stay open); false = /c (auto-close)   */
/*  Out: (void)                                                         */
/* ================================================================== */
static void RunPipInstall(const WCHAR *python, const WCHAR *req,
                           const WCHAR *work_dir, bool keep_open)
{
    WCHAR cmd[MAX_APPPATH * 4];
    if (keep_open) {
        /* cmd /k requires the inner command wrapped in extra quotes */
        _snwprintf_s(cmd, MAX_APPPATH * 4 - 1, _TRUNCATE,
                   L"cmd.exe /k \"\"%s\" -m pip install --upgrade pip && \"%s\" -m pip install --upgrade -r \"%s\"\"",
                   python, python, req);
    } else {
        /* Outer ""..."" wrapping is required for cmd /c so the && chain is
           parsed correctly when the python path contains spaces. */
        _snwprintf_s(cmd, MAX_APPPATH * 4 - 1, _TRUNCATE,
                   L"cmd.exe /c \"\"%s\" -m pip install --upgrade pip && \"%s\" -m pip install --upgrade -r \"%s\"\"",
                   python, python, req);
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

/* ================================================================== */
/*  Runner_RunWithArgs                                                  */
/*  Purpose: Executes the script at (fi, si) with additional command-  */
/*           line arguments provided by the caller (from the Run with  */
/*           Arguments dialog).  Respects console and keep-open        */
/*           settings.  Does not perform SHA re-verification.          */
/*  In:  fi   — folder index                                           */
/*       si   — script index within that folder                        */
/*       args — extra argument string appended after the script path   */
/*  Out: true if CreateProcessW succeeded; false on any error          */
/* ================================================================== */
/* Escapes cmd.exe shell metacharacters so user-supplied args cannot inject commands. */
static void EscapeForCmd(const WCHAR *src, WCHAR *dst, int dst_max)
{
    int i = 0;
    for (; *src && i < dst_max - 3; src++) {
        if      (*src == L'^') { dst[i++] = L'^'; dst[i++] = L'^'; }
        else if (*src == L'"') { dst[i++] = L'^'; dst[i++] = L'"'; }
        else if (*src == L'%') { dst[i++] = L'%'; dst[i++] = L'%'; }
        else                   { dst[i++] = *src; }
    }
    dst[i] = L'\0';
}

bool Runner_RunWithArgs(int fi, int si, const WCHAR *args)
{
    if (fi < 0 || fi >= g.folder_count) return false;
    if (si < 0 || si >= g.folders[fi].count) return false;
    Script *s = &g.folders[fi].scripts[si];

    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH)) return false;

    WCHAR cmd[MAX_APPPATH * 2];
    DWORD flags;
    if (g.cfg.show_console && g.cfg.console_keep_open) {
        /* cmd.exe /k keeps the window open: escape args to prevent shell injection */
        WCHAR esc[MAX_APPPATH] = {0};
        EscapeForCmd(args ? args : L"", esc, MAX_APPPATH);
        _snwprintf_s(cmd, MAX_APPPATH*2-1, _TRUNCATE,
                     L"cmd.exe /k \"\\\"%s\\\" \\\"%s\\\" %s\"",
                     python, s->local, esc);
        flags = CREATE_NEW_CONSOLE;
    } else {
        /* Run Python directly — no cmd.exe shell, so no shell injection possible */
        _snwprintf_s(cmd, MAX_APPPATH*2-1, _TRUNCATE, L"\"%s\" \"%s\" %s",
                     python, s->local, args ? args : L"");
        flags = g.cfg.show_console ? CREATE_NEW_CONSOLE : 0;
    }

    STARTUPINFOW si2; PROCESS_INFORMATION pi;
    ZeroMemory(&si2, sizeof(si2)); si2.cb = sizeof(si2);

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        flags, NULL, NULL, &si2, &pi))
        return false;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Prefs_IncrementRunCount(s->gh_path);
    s->run_count++;
    wcsncpy_s(g.last_run_path, MAX_APPPATH, s->gh_path, _TRUNCATE);
    PostStatus(L"Running: %s (with args)", s->name);
    return true;
}

/* ================================================================== */
/*  Runner_UpdateDeps                                                   */
/*  Purpose: For each enabled script source, locates the setup/         */
/*           requirements.txt, downloads it if missing, then calls    */
/*           RunPipInstall to install dependencies.  Covers the main  */
/*           repo, all extra GitHub repos, and all local directories.  */
/*  In:  (reads g.cfg — enabled sources, cache_dir, deps_keep_open)   */
/*  Out: (void — shows a warning dialog if no requirements found)      */
/* ================================================================== */
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
        _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\requirements.txt",
                   g.cfg.cache_dir);
        WCHAR work_dir[MAX_APPPATH];
        _snwprintf_s(work_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup", g.cfg.cache_dir);
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
        _snwprintf_s(sub_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\%s_%s",
                   g.cfg.cache_dir, owner, reponame);
        SHCreateDirectoryEx(NULL, sub_dir, NULL);

        WCHAR req[MAX_APPPATH];
        _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\requirements.txt", sub_dir);

        /* Download if missing */
        if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES) {
            const WCHAR *tok = repo->token[0] ? repo->token
                             : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
            const WCHAR *branch = repo->branch[0] ? repo->branch : L"main";
            WCHAR raw_path[MAX_APPPATH];
            _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/setup/requirements.txt",
                       owner, reponame, branch);
            GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, req, tok);
        }

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
            WCHAR status[256];
            _snwprintf_s(status, 255, _TRUNCATE, L"Installing %s/%s requirements...",
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
        _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\requirements.txt",
                   dir->path);

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
            WCHAR work_dir[MAX_APPPATH];
            _snwprintf_s(work_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup", dir->path);
            WCHAR status[256];
            _snwprintf_s(status, 255, _TRUNCATE, L"Installing local folder requirements (%d)...",
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
