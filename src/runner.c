/*
 * runner.c  -  Execute a PyCATIA script via python.exe.
 * CatiaMenuWin32
 */

#include "main.h"

typedef struct {
    WCHAR python[MAX_APPPATH];
    WCHAR script[MAX_APPPATH];
    bool  show_console;
} RunArg;

/* ================================================================== */
/*  Runner_FindPython                                                   */
/* ================================================================== */
bool Runner_FindPython(WCHAR *out, int max)
{
    /* 1. From settings */
    if (g.cfg.python_exe[0] &&
        GetFileAttributes(g.cfg.python_exe) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy(out, g.cfg.python_exe, max - 1);
        return true;
    }

    /* 2. PATH */
    WCHAR found[MAX_APPPATH];
    if (SearchPath(NULL, L"python.exe", NULL, MAX_APPPATH, found, NULL)) {
        wcsncpy(out, found, max - 1);
        return true;
    }

    /* 3. Common install locations */
    static const WCHAR *candidates[] = {
        L"C:\\Python313\\python.exe",
        L"C:\\Python312\\python.exe",
        L"C:\\Python311\\python.exe",
        L"C:\\Python310\\python.exe",
        L"C:\\Python39\\python.exe",
        L"C:\\Program Files\\Python313\\python.exe",
        L"C:\\Program Files\\Python312\\python.exe",
        L"C:\\Program Files\\Python311\\python.exe",
        L"C:\\Program Files\\Python310\\python.exe",
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
/*  Always computed fresh so it reflects the current cache_dir setting */
/* ================================================================== */
static void Runner_BuildLocalPath(int fi, int si, WCHAR *out, int max)
{
    /* gh_path is e.g. "Part_Document_Scripts/Hide_Planes.py"
       Extract just the filename part after the last slash */
    const WCHAR *gh  = g.folders[fi].scripts[si].gh_path;
    const WCHAR *sep = wcsrchr(gh, L'/');
    const WCHAR *fname = sep ? sep + 1 : gh;

    _snwprintf(out, max - 1, L"%s\\%s\\%s",
               g.cfg.cache_dir,
               g.folders[fi].name,
               fname);
    out[max - 1] = L'\0';
}

/* ================================================================== */
/*  Runner_Thread                                                       */
/* ================================================================== */
DWORD WINAPI Runner_Thread(LPVOID arg)
{
    RunArg *ra = (RunArg *)arg;

    DWORD flags = ra->show_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    WCHAR cmd[MAX_APPPATH * 2];
    _snwprintf(cmd, MAX_APPPATH * 2 - 1,
               L"\"%s\" \"%s\"", ra->python, ra->script);
    cmd[MAX_APPPATH * 2 - 1] = L'\0';

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       flags, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exit_code == 0)
            PostStatus(L"Script finished successfully.");
        else
            PostStatus(L"Script exited with code %lu.", exit_code);
    } else {
        DWORD err = GetLastError();
        PostStatus(L"Failed to launch Python. Error %lu. Check Settings.", err);
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

    /* Always build local path fresh from current cache_dir */
    WCHAR local[MAX_APPPATH] = {0};
    Runner_BuildLocalPath(fi, si, local, MAX_APPPATH);

    /* Find Python - show error if not found */
    WCHAR python[MAX_APPPATH] = {0};
    if (!Runner_FindPython(python, MAX_APPPATH)) {
        MessageBox(g.hwnd,
                   L"Python executable not found.\n\n"
                   L"Please set the path in File -> Settings.",
                   L"Python Not Found", MB_ICONWARNING | MB_OK);
        return false;
    }

    /* Download if file is missing or download_before_run is set */
    bool missing = (GetFileAttributes(local) == INVALID_FILE_ATTRIBUTES);

    if (missing || g.cfg.download_before_run) {
        PostStatus(L"Downloading %s...", s->name);

        const WCHAR *tok = g.cfg.github_token[0]
                           ? g.cfg.github_token : NULL;

        if (!GitHub_DownloadRaw(s->gh_path, local, tok)) {
            MessageBox(g.hwnd,
                       L"Failed to download script from GitHub.\n\n"
                       L"Check your internet connection.\n"
                       L"If the repo is private, add a GitHub token in Settings.",
                       L"Download Error", MB_ICONERROR | MB_OK);
            return false;
        }

        /* Update stored local path now that it's confirmed downloaded */
        wcsncpy(s->local, local, MAX_APPPATH - 1);
    }

    /* Remember for Run Last */
    wcsncpy(g.last_run_path, s->gh_path, MAX_APPPATH - 1);

    PostStatus(L"Running: %s", s->name);

    RunArg *ra = (RunArg *)malloc(sizeof(RunArg));
    if (!ra) return false;

    wcsncpy(ra->python,       python, MAX_APPPATH - 1);
    wcsncpy(ra->script,       local,  MAX_APPPATH - 1);
    ra->show_console = g.cfg.show_console;

    HANDLE hT = CreateThread(NULL, 0, Runner_Thread, ra, 0, NULL);
    if (hT) { CloseHandle(hT); return true; }

    free(ra);
    return false;
}

/* ================================================================== */
/*  Runner_UpdateDeps  -  run setup/update.bat from the cache dir     */
/* ================================================================== */
void Runner_UpdateDeps(void)
{
    /* Look for update.bat in the setup subfolder of the cache dir */
    WCHAR bat[MAX_APPPATH];
    _snwprintf(bat, MAX_APPPATH - 1, L"%s\\setup\\update.bat",
               g.cfg.cache_dir);
    bat[MAX_APPPATH - 1] = L'\0';

    if (GetFileAttributes(bat) == INVALID_FILE_ATTRIBUTES) {
        /* Try downloading it first */
        SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)L"Downloading update.bat...");
        const WCHAR *tok = g.cfg.github_token[0] ? g.cfg.github_token : NULL;
        if (!GitHub_DownloadRaw(L"setup/update.bat", bat, tok)) {
            MessageBox(g.hwnd,
                       L"Could not find or download setup/update.bat.\n\n"
                       L"Make sure the repo has been synced first.",
                       L"Update Dependencies", MB_ICONWARNING | MB_OK);
            return;
        }
    }

    /* Build working directory (setup folder) */
    WCHAR work_dir[MAX_APPPATH];
    _snwprintf(work_dir, MAX_APPPATH - 1, L"%s\\setup", g.cfg.cache_dir);

    WCHAR cmd[MAX_APPPATH * 2];
    _snwprintf(cmd, MAX_APPPATH * 2 - 1, L"cmd.exe /c \"%s\"", bat);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    SendMessage(g.hwnd_status, SB_SETTEXT, 0, (LPARAM)L"Update Dependencies launched - see console window.");

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                       CREATE_NEW_CONSOLE, NULL, work_dir, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        MessageBox(g.hwnd,
                   L"Failed to run update.bat.",
                   L"Update Dependencies", MB_ICONERROR | MB_OK);
    }
}
