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
typedef struct
{
    WCHAR python[MAX_APPPATH];
    WCHAR script[MAX_APPPATH];
    bool show_console;
    bool keep_open;
    /* Pipe handles for background-mode stdout/stderr capture (NULL when unused) */
    HANDLE hPipe_read; /* parent's read end; passed to LogReader_Thread */
    HANDLE hPipe_write; /* child's write end; set in STARTUPINFO, closed after CreateProcess */
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
    /* 1. User-configured path: if set and the file actually exists on disk, use it */
    if (g.cfg.python_exe[0] &&
        GetFileAttributes(g.cfg.python_exe) != INVALID_FILE_ATTRIBUTES)
    {
        wcsncpy_s(out, max, g.cfg.python_exe, _TRUNCATE);
        return true;
    }
    /* 2. PATH search: lets the system find whatever "python" the user has activated */
    WCHAR found[MAX_APPPATH];
    if (SearchPath(NULL, L"python.exe", NULL, MAX_APPPATH, found, NULL))
    {
        wcsncpy_s(out, max, found, _TRUNCATE);
        return true;
    }
    /* 3. Well-known installation directories: covers users who installed Python
          without adding it to PATH */
    static const WCHAR *candidates[] = {
        L"C:\\Python313\\python.exe",
        L"C:\\Python312\\python.exe",
        L"C:\\Python311\\python.exe",
        L"C:\\Python310\\python.exe",
        L"C:\\Python39\\python.exe",
        L"C:\\Program Files\\Python313\\python.exe",
        L"C:\\Program Files\\Python312\\python.exe",
        L"C:\\Program Files\\Python311\\python.exe",
        NULL /* sentinel — terminates the loop */
    };
    for (int i = 0; candidates[i]; i++)
    {
        if (GetFileAttributes(candidates[i]) != INVALID_FILE_ATTRIBUTES)
        {
            wcsncpy_s(out, max, candidates[i], _TRUNCATE);
            return true;
        }
    }
    return false; /* Python not found by any method */
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
    const WCHAR *gh = g.folders[fi].scripts[si].gh_path;
    const WCHAR *sep = wcsrchr(gh, L'/');
    const WCHAR *fname = sep ? sep + 1 : gh; /* advance past '/' to get filename; use whole string if no '/' found */
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\%s\\%s",
                 g.cfg.cache_dir, g.folders[fi].name, fname);
    out[max - 1] = L'\0';
}

/* ================================================================== */
/*  LogReader_Thread  (static)                                         */
/*  Purpose: Reads raw bytes from the child process's stdout/stderr    */
/*           pipe, converts them to wide characters (CP_ACP), and     */
/*           posts WM_LOG_OUTPUT to the main window.  The heap buffer  */
/*           in each message is freed by the WM_LOG_OUTPUT handler.   */
/*           Runs until ReadFile returns 0 (child has exited and the   */
/*           write end of the pipe is fully closed).                   */
/*  In:  arg — inheritable read-end HANDLE cast to LPVOID; closed here */
/*  Out: 0 (thread exit code)                                          */
/* ================================================================== */
static DWORD WINAPI LogReader_Thread(LPVOID arg)
{
    HANDLE hRead = (HANDLE)arg;
    char buf[1024];
    DWORD nRead;

    while (ReadFile(hRead, buf, sizeof(buf) - 1, &nRead, NULL) && nRead > 0)
    {
        /* Convert the ANSI/OEM bytes the child wrote to the console into wide chars */
        int wlen = MultiByteToWideChar(CP_ACP, 0, buf, (int)nRead, NULL, 0);
        if (wlen <= 0) continue;

        WCHAR *wbuf = (WCHAR *)malloc((wlen + 1) * sizeof(WCHAR));
        if (!wbuf) continue;

        MultiByteToWideChar(CP_ACP, 0, buf, (int)nRead, wbuf, wlen);
        wbuf[wlen] = L'\0';

        /* Main thread's WM_LOG_OUTPUT handler owns wbuf and must free it */
        PostMessage(g.hwnd, WM_LOG_OUTPUT, 0, (LPARAM)wbuf);
    }

    CloseHandle(hRead);
    return 0;
}

/* ================================================================== */
/*  Runner_Thread                                                       */
/*  Purpose: Background thread that executes the Python script via     */
/*           CreateProcessW.  In background mode it creates an         */
/*           anonymous pipe, redirects stdout/stderr into it, starts   */
/*           LogReader_Thread to forward output to the log window, and */
/*           waits for the process.  Frees the RunArg before returning. */
/*  In:  arg — pointer to a heap-allocated RunArg (freed on return)    */
/*  Out: 0 (thread exit code)                                           */
/* ================================================================== */
DWORD WINAPI Runner_Thread(LPVOID arg)
{
    RunArg *ra = (RunArg *)arg;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    WCHAR cmd[MAX_APPPATH * 2];

    if (ra->show_console && ra->keep_open)
    {
        /* Wrap in cmd.exe /k so the console window stays open after the script exits */
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE, L"cmd.exe /k \"\"%s\" \"%s\"\"",
                     ra->python, ra->script);
    }
    else
    {
        /* Run Python directly without a shell wrapper */
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE, L"\"%s\" \"%s\"", ra->python, ra->script);
    }
    cmd[MAX_APPPATH * 2 - 1] = L'\0';

    /* CREATE_NEW_CONSOLE shows a visible terminal; CREATE_NO_WINDOW runs silently in background */
    DWORD flags = ra->show_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

    /* Background mode: create pipe so stdout/stderr are captured for the log window */
    if (!ra->show_console)
    {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; /* TRUE = child can inherit write end */
        if (CreatePipe(&ra->hPipe_read, &ra->hPipe_write, &sa, 0))
        {
            /* Make the read end non-inheritable so the child cannot hold it open */
            SetHandleInformation(ra->hPipe_read, HANDLE_FLAG_INHERIT, 0);
            si.hStdOutput = ra->hPipe_write;
            si.hStdError = ra->hPipe_write;
            si.dwFlags |= STARTF_USESTDHANDLES;
        }
    }

    /* Inherit handles only when we connected a pipe; console runs do not need it */
    BOOL inherit = (ra->hPipe_write != NULL);

    if (CreateProcessW(NULL, cmd, NULL, NULL, inherit,
                       flags, NULL, NULL, &si, &pi))
    {
        if (!ra->show_console)
        {
            /* Close our copy of the write end — only the child keeps one open now.
               When the child exits the last write end closes and ReadFile in the
               reader thread returns 0 (EOF), naturally ending the reader loop. */
            if (ra->hPipe_write)
            {
                CloseHandle(ra->hPipe_write);
                ra->hPipe_write = NULL;
            }

            /* Launch log reader; it owns hPipe_read and closes it on EOF */
            if (ra->hPipe_read)
            {
                HANDLE hRT = CreateThread(NULL, 0, LogReader_Thread,
                                          ra->hPipe_read, 0, NULL);
                if (hRT)
                    CloseHandle(hRT);
                else
                    CloseHandle(ra->hPipe_read); /* CreateThread failed — release handle */
                ra->hPipe_read = NULL; /* thread (or cleanup above) now owns it */
            }

            /* Background run: duplicate the process handle into g.run_process so Runner_Stop can terminate it */
            HANDLE dup = NULL;
            DuplicateHandle(GetCurrentProcess(), pi.hProcess,
                            GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
            InterlockedExchangePointer((void **)&g.run_process, dup); /* atomic publish so Runner_Stop sees it */
            PostMessage(g.hwnd, WM_SCRIPT_STARTED, 0, 0);

            WaitForSingleObject(pi.hProcess, 30 * 60 * 1000); /* 30-minute safety timeout = 30*60*1000 ms */

            /* Atomically swap g.run_process to NULL and take ownership of the handle */
            HANDLE old = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
            if (old)
            {
                /* We won the race — Runner_Stop has not claimed the handle */
                CloseHandle(old);
                DWORD exit_code = 0;
                GetExitCodeProcess(pi.hProcess, &exit_code);
                PostMessage(g.hwnd, WM_SCRIPT_STOPPED, (WPARAM)exit_code, 0);
                if (exit_code == 0)
                    PostStatus(L"Script finished successfully.");
                else
                    PostStatus(L"Script exited with code %lu.", exit_code);
            }
            /* else NULL: Runner_Stop already claimed the handle, closed it, and posted WM_SCRIPT_STOPPED */
        }
        else
        {
            PostStatus(L"Script launched in console.");
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        /* CreateProcess failed — release any pipe handles we may have opened */
        if (ra->hPipe_write)
        {
            CloseHandle(ra->hPipe_write);
            ra->hPipe_write = NULL;
        }
        if (ra->hPipe_read)
        {
            CloseHandle(ra->hPipe_read);
            ra->hPipe_read = NULL;
        }
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
/*           Invalidates the previously highlighted button before      */
/*           overwriting g.run_fi/g.run_si so only one button is green.*/
/*           Increments the run count and updates last_run_path.       */
/*  In:  fi — folder index                                             */
/*       si — script index within that folder                          */
/*  Out: true if the thread was successfully launched; false on error   */
/* ================================================================== */
bool Runner_Run(int fi, int si)
{
    /* Guard against stale indices if folders were rebuilt since the button was drawn */
    if (fi < 0 || fi >= g.folder_count) return false;
    if (si < 0 || si >= g.folders[fi].count) return false;

    Script *s = &g.folders[fi].scripts[si];

    WCHAR local[MAX_APPPATH] = {0};

    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH))
    {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File \u2192 Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return false;
    }

    if (s->source == SCRIPT_SRC_LOCAL)
    {
        /* Local scripts are already on disk \u2014 use the stored path directly; no download or SHA check */
        wcsncpy_s(local, MAX_APPPATH, s->local, _TRUNCATE);
        if (GetFileAttributes(local) == INVALID_FILE_ATTRIBUTES)
        {
            MessageBox(g.hwnd,
                       L"Local script file not found.\n\nThe file may have been moved or deleted.",
                       L"File Not Found", MB_ICONERROR | MB_OK);
            return false;
        }
    }
    else
    {
        Runner_BuildLocalPath(fi, si, local, MAX_APPPATH);

        bool missing = (GetFileAttributes(local) == INVALID_FILE_ATTRIBUTES);
        if (missing || g.cfg.download_before_run)
        {
            /* Download if the cached file is absent or if the user wants a fresh copy every run */
            PostStatus(L"Downloading %s\u2026", s->name);
            const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
            if (!GitHub_DownloadRaw(s->gh_path, local, tok))
            {
                MessageBox(g.hwnd,
                           L"Failed to download script from GitHub.",
                           L"Download Error", MB_ICONERROR | MB_OK);
                return false;
            }
            wcsncpy_s(s->local, MAX_APPPATH, local, _TRUNCATE);
        }

        /* SHA verification: only run if the manifest has a known SHA for this script */
        if (s->sha[0] && !GitHub_VerifyScriptSHA(s))
        {
            int res = MessageBox(g.hwnd,
                                 L"Warning: Script SHA does not match the expected value from GitHub.\n\n"
                                 L"The local file may have been tampered with.\n\n"
                                 L"Do you want to re-download and run the script?",
                                 L"Security Warning", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
            if (res == IDYES)
            {
                /* User chose to trust GitHub \u2014 force a fresh download */
                const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
                if (!GitHub_DownloadRaw(s->gh_path, local, tok))
                {
                    MessageBox(g.hwnd, L"Failed to re-download script.",
                               L"Error", MB_ICONERROR | MB_OK);
                    return false;
                }
                /* Verify once more; a second mismatch means the repo itself is suspect */
                if (!GitHub_VerifyScriptSHA(s))
                {
                    MessageBox(g.hwnd,
                               L"SHA still does not match after re-download.\n"
                               L"Script will not run.",
                               L"Security Error", MB_ICONERROR | MB_OK);
                    return false;
                }
            }
            else
            {
                return false; /* user declined to run \u2014 abort silently */
            }
        }
    }

    wcsncpy_s(g.last_run_path, MAX_APPPATH, s->gh_path, _TRUNCATE);
    Prefs_IncrementRunCount(s->gh_path);
    s->run_count++;

    /* Clear the green highlight on the previously running button before we reassign run_fi/run_si */
    if (g.script_running)
    {
        HWND hOldBtn = GetDlgItem(g.hwnd_scroll, IDC_SCRIPT_BTN_BASE + g.run_si);
        if (hOldBtn) InvalidateRect(hOldBtn, NULL, FALSE);
        if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
    }
    g.run_fi = fi;
    g.run_si = si;
    PostStatus(L"Running: %s", s->name);

    RunArg *ra = (RunArg *)malloc(sizeof(RunArg));
    if (!ra) return false; /* OOM \u2014 thread cannot be launched */
    wcsncpy_s(ra->python, MAX_APPPATH, python, _TRUNCATE);
    wcsncpy_s(ra->script, MAX_APPPATH, local, _TRUNCATE);
    ra->show_console = g.cfg.show_console;
    ra->keep_open = g.cfg.show_console && g.cfg.console_keep_open;
    ra->hPipe_read = NULL;
    ra->hPipe_write = NULL;

    HANDLE hT = CreateThread(NULL, 0, Runner_Thread, ra, 0, NULL);
    if (hT)
    {
        CloseHandle(hT);
        return true;
    } /* thread owns ra now; close our copy of the thread handle */
    free(ra); /* CreateThread failed \u2014 free the block we allocated */
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
    /* Atomically swap g.run_process to NULL; only one caller can get a non-NULL handle */
    HANDLE h = (HANDLE)InterlockedExchangePointer((void **)&g.run_process, NULL);
    if (!h) return; /* nothing running, or Runner_Thread already claimed the handle */
    TerminateProcess(h, 1); /* exit code 1 distinguishes a forced stop from a clean exit (0) */
    CloseHandle(h);
    PostStatus(L"Script stopped.");
    PostMessage(g.hwnd, WM_SCRIPT_STOPPED, 0, 1); /* lp=1 signals user-initiated stop */
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
    if (keep_open)
    {
        /* /k keeps the console open after pip finishes so the user can read output */
        _snwprintf_s(cmd, MAX_APPPATH * 4 - 1, _TRUNCATE,
                     L"cmd.exe /k \"\"%s\" -m pip install --upgrade pip && \"%s\" -m pip install --upgrade -r \"%s\"\"",
                     python, python, req);
    }
    else
    {
        /* /c closes the console when done; outer double-quotes are required so cmd
           correctly parses the && chain when the python path contains spaces */
        _snwprintf_s(cmd, MAX_APPPATH * 4 - 1, _TRUNCATE,
                     L"cmd.exe /c \"\"%s\" -m pip install --upgrade pip && \"%s\" -m pip install --upgrade -r \"%s\"\"",
                     python, python, req);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       CREATE_NEW_CONSOLE, NULL, work_dir, &si, &pi))
    {
        if (!keep_open)
        {
            WaitForSingleObject(pi.hProcess, INFINITE); /* wait for pip to finish before returning */
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
    for (; *src && i < dst_max - 3; src++)
    {
        if (*src == L'^')
        {
            dst[i++] = L'^';
            dst[i++] = L'^';
        }
        else if (*src == L'"')
        {
            dst[i++] = L'^';
            dst[i++] = L'"';
        }
        else if (*src == L'%')
        {
            dst[i++] = L'%';
            dst[i++] = L'%';
        }
        else
        {
            dst[i++] = *src;
        }
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
    if (g.cfg.show_console && g.cfg.console_keep_open)
    {
        /* Routed through cmd.exe /k — must escape args to prevent shell metacharacter injection */
        WCHAR esc[MAX_APPPATH] = {0};
        EscapeForCmd(args ? args : L"", esc, MAX_APPPATH);
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE,
                     L"cmd.exe /k \"\\\"%s\\\" \\\"%s\\\" %s\"",
                     python, s->local, esc);
        flags = CREATE_NEW_CONSOLE;
    }
    else
    {
        /* Python is invoked directly — no shell interpretation, no injection risk */
        _snwprintf_s(cmd, MAX_APPPATH * 2 - 1, _TRUNCATE, L"\"%s\" \"%s\" %s",
                     python, s->local, args ? args : L"");
        flags = g.cfg.show_console ? CREATE_NEW_CONSOLE : 0; /* 0 = no special console flag = hidden window */
    }

    STARTUPINFOW si2;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si2, sizeof(si2));
    si2.cb = sizeof(si2);

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
    if (!Runner_FindPython(python, MAX_APPPATH))
    {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File \u2192 Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return;
    }

    /* ── Collect all requirements files to run ────────────────────── */
    /* We run each separately so pip resolves each independently.      */
    /* Order: main repo bat/req, extra repo reqs, local dir reqs.     */

    bool found_any = false; /* set to true once any requirements.txt is located and run */

    /* 1. Main repo: try update.bat first, then requirements.txt */
    if (g.cfg.main_repo_enabled)
    {
        WCHAR req[MAX_APPPATH];
        _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\requirements.txt",
                     g.cfg.cache_dir);
        WCHAR work_dir[MAX_APPPATH];
        _snwprintf_s(work_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup", g.cfg.cache_dir);
        SHCreateDirectoryEx(NULL, work_dir, NULL);

        /* Download if missing */
        if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES)
        {
            SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                        (LPARAM)L"Downloading main repo requirements.txt...");
            const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
            GitHub_DownloadRaw(L"setup/requirements.txt", req, tok);
        }

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES)
        {
            SendMessage(g.hwnd_status, SB_SETTEXT, 0,
                        (LPARAM)L"Installing main repo requirements...");
            RunPipInstall(python, req, work_dir, g.cfg.deps_keep_open);
            found_any = true;
        }
    }

    /* 2. Extra GitHub repos: each has its own cached requirements.txt */
    for (int i = 0; i < g.cfg.extra_repo_count; i++)
    {
        ExtraRepo *repo = &g.cfg.extra_repos[i];
        if (!repo->enabled || !repo->url[0]) continue; /* skip disabled or empty entries */

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
        if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES)
        {
            const WCHAR *tok = repo->token[0] ? repo->token
                                              : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
            const WCHAR *branch = repo->branch[0] ? repo->branch : L"main";
            WCHAR raw_path[MAX_APPPATH];
            _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/setup/requirements.txt",
                         owner, reponame, branch);
            GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, req, tok);
        }

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES)
        {
            WCHAR status[256];
            _snwprintf_s(status, 255, _TRUNCATE, L"Installing %s/%s requirements...",
                         owner, reponame);
            SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)status);
            RunPipInstall(python, req, sub_dir, g.cfg.deps_keep_open);
            found_any = true;
        }
    }

    /* 3. Local dirs: run directly from source - no caching needed */
    for (int i = 0; i < g.cfg.local_dir_count; i++)
    {
        LocalDir *dir = &g.cfg.local_dirs[i];
        if (!dir->enabled || !dir->path[0]) continue; /* skip disabled or empty local dir entries */

        WCHAR req[MAX_APPPATH];
        _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\requirements.txt",
                     dir->path);

        if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES)
        {
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

    if (!found_any)
    {
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
