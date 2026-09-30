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
    WCHAR args[MAX_APPPATH]; /* extra command-line arguments; empty = none */
    LONG run_id; /* g.run_seq at launch; tags this run's WM_SCRIPT_* messages */
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
/*  EscapeForCmd  (static)                                             */
/*  Purpose: Caret-escapes every cmd.exe metacharacter in user-supplied */
/*           arguments so they reach Python literally when the command */
/*           is routed through `cmd.exe /k`.  Quotes are escaped too,   */
/*           so cmd never enters its quoted state and every following  */
/*           metacharacter stays escapable; `^%` defeats %VAR%         */
/*           expansion on a command line (`%%` only works in batch).   */
/*  In:  src     — raw argument string                                 */
/*       dst     — buffer to receive the escaped string                */
/*       dst_max — capacity of dst in WCHARs                           */
/*  Out: (void — dst is populated, truncated if it would overflow)     */
/* ================================================================== */
static void EscapeForCmd(const WCHAR *src, WCHAR *dst, int dst_max)
{
    int i = 0;
    for (; *src && i < dst_max - 3; src++)
    {
        if (wcschr(L"^\"%&|<>()", *src))
            dst[i++] = L'^';
        dst[i++] = *src;
    }
    dst[i] = L'\0';
}

/* ================================================================== */
/*  Runner_Thread                                                       */
/*  Purpose: Background thread that executes the Python script via     */
/*           CreateProcessW.  In background mode it creates an         */
/*           anonymous pipe, redirects stdout/stderr into it, starts   */
/*           LogReader_Thread to forward output to the log window,     */
/*           places the process in a job object so Stop can end the   */
/*           whole process tree, and waits for the process to exit.   */
/*           Frees the RunArg before returning.                        */
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

    WCHAR cmd[MAX_APPPATH * 6]; /* python + script + escaped args (escaping can double the args) */

    if (ra->show_console && ra->keep_open)
    {
        /* Wrap in cmd.exe /k so the console window stays open after the script exits;
           arguments must be escaped because cmd.exe parses the whole line */
        WCHAR esc[MAX_APPPATH * 2] = {0};
        EscapeForCmd(ra->args, esc, (int)_countof(esc));
        _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"cmd.exe /k \"\"%s\" \"%s\"%s%s\"",
                     ra->python, ra->script, esc[0] ? L" " : L"", esc);
    }
    else
    {
        /* Run Python directly without a shell wrapper — no shell parses the arguments */
        _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"\"%s\" \"%s\"%s%s",
                     ra->python, ra->script, ra->args[0] ? L" " : L"", ra->args);
    }

    /* CREATE_NEW_CONSOLE shows a visible terminal; CREATE_NO_WINDOW runs silently in background */
    DWORD flags = ra->show_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

    /* Background runs go in a job object so Stop ends every process the script
       started, not just python.exe.  No kill-on-close limit: processes a script
       deliberately leaves running survive its normal exit.  Console runs are not
       tracked or stoppable, so they need no job. */
    HANDLE job = ra->show_console ? NULL : CreateJobObjectW(NULL, NULL);
    if (job) flags |= CREATE_SUSPENDED; /* assign to the job before the script can spawn anything */

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
        if (job)
        {
            /* On failure the script still runs; Stop then falls back to ending python.exe alone */
            if (!AssignProcessToJobObject(job, pi.hProcess))
            {
                Util_Log(L"Runner: AssignProcessToJobObject failed (%lu)", GetLastError());
                CloseHandle(job);
                job = NULL;
            }
            ResumeThread(pi.hThread);
        }

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

            /* Background run: publish the handles so Runner_Stop can end the run.
               This thread keeps ownership — it closes them only after unpublishing
               below, so Runner_Stop never touches a closed (or recycled) handle. */
            EnterCriticalSection(&g.cs_run);
            g.run_job = job;
            g.run_proc = pi.hProcess;
            g.run_stop_requested = false;
            LeaveCriticalSection(&g.cs_run);
            PostMessage(g.hwnd, WM_SCRIPT_STARTED, (WPARAM)ra->run_id, 0);

            /* No timeout: a script may legitimately run for hours, and a hung one
               is ended with Stop.  Reporting it as finished early would leave it
               running with no way to stop it. */
            WaitForSingleObject(pi.hProcess, INFINITE);

            /* Unpublish — unless a newer run has already replaced ours.  Comparing
               handle values is safe: ours is still open, so no other open handle
               can have the same value. */
            bool by_user = false;
            EnterCriticalSection(&g.cs_run);
            if (g.run_proc == pi.hProcess)
            {
                by_user = g.run_stop_requested;
                g.run_job = NULL;
                g.run_proc = NULL;
                g.run_stop_requested = false;
            }
            LeaveCriticalSection(&g.cs_run);

            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            PostMessage(g.hwnd, WM_SCRIPT_STOPPED, (WPARAM)exit_code,
                        RUN_STOP_LPARAM(ra->run_id, by_user));
        }
        else
        {
            PostStatus(L"Script launched in console.");
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (job) CloseHandle(job); /* no kill-on-close limit, so this leaves survivors running */
    }
    else
    {
        DWORD err = GetLastError(); /* read before CloseHandle below can overwrite it */

        /* CreateProcess failed — release the job and any pipe handles we may have opened */
        if (job) CloseHandle(job);
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
        PostStatus(L"Failed to launch Python. Error %lu.", err);
    }

    free(ra);
    return 0;
}

/* ================================================================== */
/*  Runner_Terminate  (static)                                         */
/*  Purpose: Ends the published background run, if any: the whole     */
/*           job (every process the script started), or python.exe    */
/*           alone if the job could not be set up.  Marks the run as  */
/*           stopped by the user; the owning Runner_Thread then        */
/*           unpublishes it and posts WM_SCRIPT_STOPPED.               */
/*  In:  wait — true to wait (up to 5 s) for python.exe to exit, so a */
/*              replacing launch starts after the old run has ended    */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Runner_Terminate(bool wait)
{
    EnterCriticalSection(&g.cs_run);
    if (g.run_proc && !g.run_stop_requested)
    {
        /* Exit code 1 distinguishes a forced stop from a clean exit (0) */
        if (!g.run_job || !TerminateJobObject(g.run_job, 1))
            TerminateProcess(g.run_proc, 1);
        g.run_stop_requested = true;
    }
    /* Waiting inside the lock is safe: the owning thread needs the lock only
       after python.exe has exited, which is what is being waited for. */
    if (wait && g.run_proc)
        WaitForSingleObject(g.run_proc, 5000); /* 5000 ms: termination completes in well under a second */
    LeaveCriticalSection(&g.cs_run);
}

/* ================================================================== */
/*  Runner_Start  (static)                                             */
/*  Purpose: Starts a validated script: ends any running background   */
/*           script first (only one is tracked at a time, so Stop     */
/*           always reaches the script actually running), clears the  */
/*           previous run's highlight, records the new run and hands  */
/*           it to Runner_Thread.  Shared by button runs and dropped  */
/*           files.                                                    */
/*  In:  python — Python executable                                    */
/*       script — script file to run                                   */
/*       args   — extra command-line arguments, or NULL/empty          */
/*       name   — name shown in the status bar and the log header      */
/*       fi, si — button position, or -1/-1 for a script without one  */
/*  Out: true if the thread was launched                               */
/* ================================================================== */
static bool Runner_Start(const WCHAR *python, const WCHAR *script, const WCHAR *args,
                         const WCHAR *name, int fi, int si)
{
    /* Starting a different script ends repeat mode */
    if (g.repeat_mode && (g.repeat_fi != fi || g.repeat_si != si))
        Repeat_Stop();
    Runner_Terminate(true);

    /* New run id: the replaced run's late WM_SCRIPT_* messages carry the old one */
    g.run_seq++;

    /* Clear the previous run's running state and green highlight before we reassign
       run_fi/run_si.  A replaced run's WM_SCRIPT_STOPPED is ignored as stale, so it
       must not be left to do this. */
    if (g.script_running)
    {
        g.script_running = false;
        EnableWindow(GetDlgItem(g.hwnd, IDC_BTN_STOP), FALSE);
        HWND hOldBtn = GetDlgItem(g.hwnd_scroll, IDC_SCRIPT_BTN_BASE + g.run_si);
        if (hOldBtn) InvalidateRect(hOldBtn, NULL, FALSE);
        if (g.hwnd_qbar) InvalidateRect(g.hwnd_qbar, NULL, FALSE);
    }
    g.run_fi = fi;
    g.run_si = si;
    wcsncpy_s(g.run_name, MAX_NAME, name, _TRUNCATE);
    bool has_args = (args && args[0]);
    PostStatus(has_args ? L"Running: %s (with args)" : L"Running: %s", name);

    RunArg *ra = (RunArg *)malloc(sizeof(RunArg));
    if (!ra) return false; /* OOM - thread cannot be launched */
    wcsncpy_s(ra->python, MAX_APPPATH, python, _TRUNCATE);
    wcsncpy_s(ra->script, MAX_APPPATH, script, _TRUNCATE);
    wcsncpy_s(ra->args, MAX_APPPATH, has_args ? args : L"", _TRUNCATE);
    ra->run_id = g.run_seq;
    ra->show_console = g.cfg.show_console;
    ra->keep_open = g.cfg.show_console && g.cfg.console_keep_open;
    ra->hPipe_read = NULL;
    ra->hPipe_write = NULL;

    HANDLE hT = CreateThread(NULL, 0, Runner_Thread, ra, 0, NULL);
    if (hT)
    {
        CloseHandle(hT); /* thread owns ra now; close our copy of the thread handle */
        return true;
    }
    free(ra); /* CreateThread failed - free the block we allocated */
    return false;
}

/* ================================================================== */
/*  Dependency check probe                                             */
/*  Written to <data folder>\dep_check.py and run with the script's   */
/*  requirement strings as arguments.  Prints one line per missing or */
/*  out-of-range requirement: "<requirement>\t<note>".  Uses          */
/*  packaging (or pip's vendored copy) for version specifiers and     */
/*  environment markers; without it, only presence is checked.        */
/* ================================================================== */
static const char s_dep_probe[] =
    "import sys\n"
    "try:\n"
    "    import importlib.metadata as md\n"
    "except Exception:\n"
    "    import importlib_metadata as md\n"
    "R = None\n"
    "try:\n"
    "    from packaging.requirements import Requirement as R\n"
    "except Exception:\n"
    "    try:\n"
    "        from pip._vendor.packaging.requirements import Requirement as R\n"
    "    except Exception:\n"
    "        pass\n"
    "for a in sys.argv[1:]:\n"
    "    try:\n"
    "        if R:\n"
    "            q = R(a)\n"
    "            if q.marker is not None and not q.marker.evaluate():\n"
    "                continue\n"
    "            v = md.version(q.name)\n"
    "            if q.specifier and not q.specifier.contains(v, prereleases=True):\n"
    "                print(a + '\\tinstalled ' + v)\n"
    "        else:\n"
    "            n = ''\n"
    "            for c in a:\n"
    "                if c.isalnum() or c in '._-':\n"
    "                    n += c\n"
    "                else:\n"
    "                    break\n"
    "            md.version(n)\n"
    "    except md.PackageNotFoundError:\n"
    "        print(a + '\\tnot installed')\n"
    "    except Exception:\n"
    "        pass\n";

/* ================================================================== */
/*  Runner_DepsHash  (static)                                          */
/*  Purpose: FNV-1a hash of a Python path and a requirement list — the */
/*           key under which a passed check is remembered.             */
/*  In:  python, deps — strings to hash                                */
/*  Out: 32-bit hash                                                    */
/* ================================================================== */
static DWORD Runner_DepsHash(const WCHAR *python, const WCHAR *deps)
{
    DWORD h = 2166136261u; /* FNV-1a offset basis */
    for (const WCHAR *p = python; *p; p++)
        h = (h ^ (DWORD)towlower(*p)) * 16777619u; /* FNV prime; path compared case-insensitively */
    h = (h ^ 0x7Cu) * 16777619u; /* separator so "ab"+"c" differs from "a"+"bc" */
    for (const WCHAR *p = deps; *p; p++)
        h = (h ^ (DWORD)*p) * 16777619u;
    return h;
}

/* ================================================================== */
/*  Runner_ForgetDeps                                                   */
/*  Purpose: Forgets every remembered passed dependency check — called */
/*           when packages or the Python interpreter may have changed  */
/*           (Update Dependencies, Settings OK).                       */
/*  In:  (none)                                                         */
/*  Out: (void)                                                         */
/* ================================================================== */
void Runner_ForgetDeps(void)
{
    g.deps_ok_count = 0;
}

/* ================================================================== */
/*  Runner_FindMissingDeps  (static)                                   */
/*  Purpose: Runs the probe with the given Python and collects the     */
/*           requirements it reports as missing or out of range.       */
/*  In:  python  — Python executable                                   */
/*       deps    — '\n'-separated requirement strings                  */
/*       display — receives "requirement (note)" lines for the user    */
/*       reqs    — receives the bare requirement strings, quoted and   */
/*                 space-separated, ready for pip                      */
/*       max     — capacity of display and reqs in WCHARs              */
/*  Out: 1 = everything present, 0 = something missing, -1 = the      */
/*       check itself could not run (the script is then not blocked) */
/* ================================================================== */
static int Runner_FindMissingDeps(const WCHAR *python, const WCHAR *deps,
                                  WCHAR *display, WCHAR *reqs, int max)
{
    display[0] = reqs[0] = L'\0';

    WCHAR probe[MAX_APPPATH];
    _snwprintf_s(probe, MAX_APPPATH, _TRUNCATE, L"%s\\dep_check.py", g.appdata_dir);
    if (!Util_WriteFile(probe, s_dep_probe, (DWORD)strlen(s_dep_probe))) return -1;

    /* "python" "probe" "req1" "req2" ... — no shell, so > and < need no escaping */
    WCHAR cmd[MAX_APPPATH * 4];
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"\"%s\" \"%s\"", python, probe);
    WCHAR list[512];
    wcsncpy_s(list, _countof(list), deps, _TRUNCATE);
    WCHAR *ctx = NULL;
    for (WCHAR *r = wcstok_s(list, L"\n", &ctx); r; r = wcstok_s(NULL, L"\n", &ctx))
    {
        wcsncat_s(cmd, _countof(cmd), L" \"", _TRUNCATE);
        wcsncat_s(cmd, _countof(cmd), r, _TRUNCATE);
        wcsncat_s(cmd, _countof(cmd), L"\"", _TRUNCATE);
    }

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; /* TRUE = child inherits the write end */
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {.cb = sizeof(si), .dwFlags = STARTF_USESTDHANDLES, .hStdOutput = wr, .hStdError = wr};
    PROCESS_INFORMATION pi;
    BOOL started = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(wr); /* only the child holds the write end now */
    if (!started)
    {
        CloseHandle(rd);
        return -1;
    }

    /* Read until the probe exits (EOF); give up after 15 s so a hung Python never blocks a run */
    char out[4096];
    DWORD total = 0, got = 0;
    DWORD deadline = GetTickCount() + 15000; /* 15000 ms */
    bool timed_out = false;
    for (;;)
    {
        DWORD avail = 0;
        if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL)) break; /* pipe closed: probe finished */
        if (avail && total < sizeof(out) - 1)
        {
            if (!ReadFile(rd, out + total, (DWORD)(sizeof(out) - 1 - total), &got, NULL) || !got) break;
            total += got;
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0 && !avail)
        {
            DWORD more = 0; /* drain anything written just before exit */
            if (!PeekNamedPipe(rd, NULL, 0, NULL, &more, NULL) || !more) break;
        }
        if (GetTickCount() > deadline)
        {
            timed_out = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    out[total] = '\0';
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (timed_out || code != 0) return -1; /* the probe itself failed: do not block the script */

    WCHAR wout[4096];
    MultiByteToWideChar(CP_ACP, 0, out, -1, wout, (int)_countof(wout));
    ctx = NULL;
    for (WCHAR *line = wcstok_s(wout, L"\r\n", &ctx); line; line = wcstok_s(NULL, L"\r\n", &ctx))
    {
        WCHAR *tab = wcschr(line, L'\t');
        const WCHAR *note = L"";
        if (tab)
        {
            *tab = L'\0';
            note = tab + 1;
        }
        WCHAR entry[256];
        _snwprintf_s(entry, _countof(entry), _TRUNCATE, L"    %s  (%s)\n", line, note);
        wcsncat_s(display, max, entry, _TRUNCATE);
        _snwprintf_s(entry, _countof(entry), _TRUNCATE, L" \"%s\"", line);
        wcsncat_s(reqs, max, entry, _TRUNCATE);
    }
    return reqs[0] ? 0 : 1;
}

/* ================================================================== */
/*  Runner_PipInstall  (static)                                        */
/*  Purpose: Installs the given requirements with pip in a visible     */
/*           console and waits for it.  The console stays open (pause)*/
/*           only if pip fails, so errors can be read.                 */
/*  In:  python — Python executable; reqs — quoted requirement list    */
/*  Out: (void)                                                         */
/* ================================================================== */
static void Runner_PipInstall(const WCHAR *python, const WCHAR *reqs)
{
    WCHAR cmd[MAX_APPPATH * 4];
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE,
                 L"cmd.exe /c \"\"%s\" -m pip install%s || pause\"", python, reqs);
    STARTUPINFOW si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE); /* the run continues only after pip finished */
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

/* ================================================================== */
/*  Runner_CheckDependencies  (static)                                 */
/*  Purpose: Before a run, checks the packages the script declares in  */
/*           its dependencies = [...] list against the Python that     */
/*           will run it.  A passed check is remembered (per Python    */
/*           and list) until Runner_ForgetDeps.  When something is    */
/*           missing the user chooses: install now with pip, run      */
/*           anyway, or cancel.                                        */
/*  In:  python — Python executable                                    */
/*       deps   — '\n'-separated requirement strings (may be empty)    */
/*       name   — script name for the message                          */
/*  Out: true to go ahead with the run, false to cancel it             */
/* ================================================================== */
static bool Runner_CheckDependencies(const WCHAR *python, const WCHAR *deps, const WCHAR *name)
{
    if (!g.cfg.check_deps || !deps || !deps[0]) return true;
    DWORD h = Runner_DepsHash(python, deps);
    for (int i = 0; i < g.deps_ok_count; i++)
        if (g.deps_ok[i] == h) return true; /* already passed with this Python */

    SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)L"Checking script dependencies...");
    HCURSOR old_cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    WCHAR display[1024], reqs[1024];
    int r = Runner_FindMissingDeps(python, deps, display, reqs, (int)_countof(display));
    SetCursor(old_cursor);
    if (r < 0) return true; /* the check could not run: never block the script over it */

    if (r == 0)
    {
        WCHAR msg[2048];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                     L"%s needs Python packages that are not installed for\n%s:\n\n%s\n"
                     L"Yes - install them now with pip, then run\n"
                     L"No - run the script anyway\n"
                     L"Cancel - do not run",
                     name, python, display);
        int res = MessageBox(g.hwnd, msg, L"Missing Dependencies", MB_ICONWARNING | MB_YESNOCANCEL);
        if (res == IDNO) return true;
        if (res != IDYES) return false;

        Runner_PipInstall(python, reqs);
        r = Runner_FindMissingDeps(python, deps, display, reqs, (int)_countof(display));
        if (r == 0)
        {
            _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                         L"These packages are still missing after pip ran:\n\n%s\n"
                         L"%s will not run. Check the pip output, or install them manually.",
                         display, name);
            MessageBox(g.hwnd, msg, L"Missing Dependencies", MB_ICONERROR | MB_OK);
            return false;
        }
    }

    /* Remember the pass; the table is small, so the oldest entries are overwritten */
    if (g.deps_ok_count < (int)_countof(g.deps_ok))
        g.deps_ok[g.deps_ok_count++] = h;
    else
        g.deps_ok[h % _countof(g.deps_ok)] = h;
    return true;
}

/* ================================================================== */
/*  Runner_Launch  (static)                                            */
/*  Purpose: Validates folder/script indices, locates Python, downloads */
/*           the script if missing or if download_before_run is set,  */
/*           performs SHA verification, then launches Runner_Thread.   */
/*           Invalidates the previously highlighted button before      */
/*           overwriting g.run_fi/g.run_si so only one button is green.*/
/*           Increments the run count and updates last_run_path.       */
/*  In:  fi   — folder index                                           */
/*       si   — script index within that folder                        */
/*       args — extra command-line arguments, or NULL/empty for none   */
/*  Out: true if the thread was successfully launched; false on error   */
/* ================================================================== */
static bool Runner_Launch(int fi, int si, const WCHAR *args)
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
        /* Run the cache copy sync maintains (s->local).  Building the path
           from the tab name instead would resolve to a separate copy under
           the synthetic Favourites tab, which sync never refreshes — the
           quick bar would keep running a stale snapshot. */
        if (s->local[0])
            wcsncpy_s(local, MAX_APPPATH, s->local, _TRUNCATE);
        else
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

    /* Check the packages the script declares before starting it */
    Meta_Parse(s);
    if (!Runner_CheckDependencies(python, s->meta.dependencies, s->name)) return false;

    wcsncpy_s(g.last_run_path, MAX_APPPATH, s->gh_path, _TRUNCATE);
    Prefs_IncrementRunCount(s->gh_path);
    s->run_count++;
    Badges_MarkSeen(s->gh_path, s->sha); /* running a script counts as having seen this version */

    return Runner_Start(python, local, args, s->name, fi, si);
}

/* ================================================================== */
/*  Runner_Run                                                          */
/*  Purpose: Runs the script at (fi, si) with no extra arguments.      */
/*  In:  fi — folder index                                             */
/*       si — script index within that folder                          */
/*  Out: true if the thread was successfully launched; false on error   */
/* ================================================================== */
bool Runner_Run(int fi, int si)
{
    return Runner_Launch(fi, si, NULL);
}

/* ================================================================== */
/*  Runner_RunWithArgs                                                  */
/*  Purpose: Runs the script at (fi, si) with extra command-line       */
/*           arguments from the Run with Arguments dialog.  Goes       */
/*           through the same download, SHA check, output log and     */
/*           Stop handling as a normal run.                           */
/*  In:  fi   — folder index                                           */
/*       si   — script index within that folder                        */
/*       args — argument string appended after the script path         */
/*  Out: true if the thread was successfully launched; false on error   */
/* ================================================================== */
bool Runner_RunWithArgs(int fi, int si, const WCHAR *args)
{
    return Runner_Launch(fi, si, args);
}

/* ================================================================== */
/*  Runner_RunPath                                                      */
/*  Purpose: Runs a .py file that is not one of the script buttons —   */
/*           a file dropped on the main window.  No download or SHA   */
/*           check (the user chose the file); the dependency check,   */
/*           output log and Stop work as for a button run.            */
/*  In:  path — full path of the .py file                              */
/*  Out: true if the run was started                                    */
/* ================================================================== */
bool Runner_RunPath(const WCHAR *path)
{
    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH))
    {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File > Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return false;
    }

    /* Read the file's header for its dependencies, like a button script's */
    Script *tmp = (Script *)calloc(1, sizeof(Script));
    if (!tmp) return false;
    wcsncpy_s(tmp->local, MAX_APPPATH, path, _TRUNCATE);
    Meta_Parse(tmp);
    const WCHAR *name = PathFindFileNameW(path);
    bool go = Runner_CheckDependencies(python, tmp->meta.dependencies, name);
    free(tmp);
    return go && Runner_Start(python, path, NULL, name, -1, -1);
}

/* ================================================================== */
/*  Runner_Stop                                                         */
/*  Purpose: Terminates the currently running background script and   */
/*           every process it started, if any.  The owning thread     */
/*           posts WM_SCRIPT_STOPPED once the script has exited, which */
/*           disables the Stop button.                                 */
/*  In:  (reads g.run_job / g.run_proc under g.cs_run)                 */
/*  Out: (void)                                                         */
/* ================================================================== */
void Runner_Stop(void)
{
    Runner_Terminate(false);
}

/* ================================================================== */
/*  Runner_IsRunning                                                    */
/*  Purpose: Reports whether a background script is running and has   */
/*           not already been stopped.                                 */
/*  In:  (reads g.run_proc / g.run_stop_requested under g.cs_run)      */
/*  Out: true if a background run can still be stopped                 */
/* ================================================================== */
bool Runner_IsRunning(void)
{
    EnterCriticalSection(&g.cs_run);
    bool running = g.run_proc && !g.run_stop_requested;
    LeaveCriticalSection(&g.cs_run);
    return running;
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
    Runner_ForgetDeps(); /* packages are about to change */
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
