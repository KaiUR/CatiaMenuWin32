/*
 * sync.c  -  Startup sync engine.
 *
 * On every launch (or manual refresh) this module:
 *   1. Fetches the repo root to discover current folders.
 *   2. Compares against the locally cached manifest.
 *   3. Adds tabs for new folders, removes tabs for deleted ones.
 *   4. For each folder, fetches its file listing and compares SHA
 *      values against the manifest.
 *   5. Downloads any file whose SHA differs or that is missing locally.
 *   6. Deletes cached files that were removed from the repo.
 *   7. Saves the updated manifest back to AppData.
 *   8. Posts WM_SYNC_DONE with a heap-allocated SyncResult.
 *
 * If the main repo is enabled but unreachable, the module returns
 * SR_NO_INTERNET without clearing folders, so cached scripts loaded
 * at startup by Sync_LoadManifest remain visible in the UI.
 *
 * Manifest format  (%APPDATA%\CatiaMenuWin32\manifest.ini):
 *   [FolderName]
 *   script_gh_path=sha40hex
 *
 * All settings (python path, cache dir, token, etc.) are stored in
 *   %APPDATA%\CatiaMenuWin32\settings.ini
 *
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * AI Assistance: Claude (Anthropic)
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  ManifestPath  (static)                                             */
/*  Purpose: Builds the full path to manifest.ini in the AppData       */
/*           directory and stores it in out.                           */
/*  In:  out — buffer to receive the path                              */
/*       max — capacity of out in WCHARs                               */
/*  Out: (void — out is populated)                                      */
/* ================================================================== */
static void ManifestPath(WCHAR *out, int max)
{
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\%s", g.appdata_dir, MANIFEST_FILE);
}

/* ================================================================== */
/*  Sync_GetLocalSHA                                                    */
/*  Purpose: Reads the cached SHA for gh_path from manifest.ini.       */
/*           The INI section is derived from the first path component. */
/*  In:  gh_path  — GitHub-relative script path (e.g. "Folder/s.py") */
/*       sha_out  — buffer to receive the 40-char hex SHA              */
/*  Out: true and sha_out filled if found; false (sha_out="") otherwise*/
/* ================================================================== */
bool Sync_GetLocalSHA(const WCHAR *gh_path, WCHAR *sha_out)
{
    sha_out[0] = L'\0';

    /* Section = folder name = first path component */
    WCHAR section[MAX_NAME] = {0};
    const WCHAR *slash = wcschr(gh_path, L'/');
    if (!slash) return false;
    size_t n = (size_t)(slash - gh_path);
    wcsncpy_s(section, MAX_NAME, gh_path, n < MAX_NAME ? n : MAX_NAME - 1);

    WCHAR manifest[MAX_APPPATH];
    ManifestPath(manifest, MAX_APPPATH);

    GetPrivateProfileString(section, gh_path, L"",
                            sha_out, MAX_SHA, manifest);
    return sha_out[0] != L'\0';
}

static void Sync_LocalDir(const LocalDir *dir);
static void Sync_ExtraRepo(const ExtraRepo *repo, char *buf);

/* ================================================================== */
/*  Sync_SaveManifest                                                   */
/*  Purpose: Rewrites manifest.ini from scratch using the current      */
/*           g.folders[] state. Deletes the old file first so stale   */
/*           entries from removed scripts cannot persist.              */
/*  In:  (none — reads g.folders[])                                    */
/*  Out: (void — manifest.ini updated on disk)                         */
/* ================================================================== */
void Sync_SaveManifest(void)
{
    WCHAR manifest[MAX_APPPATH];
    ManifestPath(manifest, MAX_APPPATH);

    /* Delete old manifest so stale keys disappear */
    DeleteFile(manifest);

    for (int fi = 0; fi < g.folder_count; fi++) {
        ScriptFolder *f = &g.folders[fi];
        for (int si = 0; si < f->count; si++) {
            Script *s = &f->scripts[si];
            WritePrivateProfileString(f->name, s->gh_path, s->sha, manifest);
        }
    }
}

/* ================================================================== */
/*  Sync_LoadManifest                                                   */
/*  Purpose: Scans the cache directory and populates g.folders[] from  */
/*           scripts already on disk. Runs at startup so scripts are   */
/*           immediately visible without waiting for the sync thread.  */
/*           Verifies each file's SHA against manifest.ini and clears  */
/*           mismatched entries so the sync thread will re-download.  */
/*  In:  (none — reads g.cfg.cache_dir, g.cfg.local_dirs)             */
/*  Out: (void — populates g.folders[], g.folder_count)               */
/* ================================================================== */
void Sync_LoadManifest(void)
{
    /* Scan the cache directory and populate g.folders[] from what is
       already on disk. This runs at startup so scripts are visible
       immediately even before the sync completes or if there is no internet. */
    if (!g.cfg.cache_dir[0]) return; /* no cache path configured — nothing to scan */

    for (int _fi = 0; _fi < g.folder_count; _fi++) Folder_Free(&g.folders[_fi]);
    g.folder_count = 0;

    if (g.cfg.main_repo_enabled) {
    WCHAR pattern[MAX_APPPATH];
    _snwprintf_s(pattern, MAX_APPPATH, _TRUNCATE, L"%s\\*", g.cfg.cache_dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue; /* skip files — only process subdirs */
        if (wcscmp(fd.cFileName, L".") == 0)     continue;               /* skip current-dir entry */
        if (wcscmp(fd.cFileName, L"..") == 0)    continue;               /* skip parent-dir entry */
        if (wcscmp(fd.cFileName, L"setup") == 0) continue;               /* setup/ holds requirements, not scripts */
        if (g.folder_count >= MAX_FOLDERS)        break;                  /* hard cap — stop adding tabs */

        ScriptFolder *f = &g.folders[g.folder_count++];
        ZeroMemory(f, sizeof(*f));
        wcsncpy_s(f->name,    MAX_NAME, fd.cFileName, _TRUNCATE);
        wcsncpy_s(f->display, MAX_NAME, f->name,      _TRUNCATE);
        Util_SnakeToTitle(f->display);
        Folder_Alloc(f, 64);

        /* Scan .py files inside this folder */
        WCHAR sub[MAX_APPPATH];
        _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\*.py",
                   g.cfg.cache_dir, f->name);

        WIN32_FIND_DATAW sf;
        HANDLE hSub = FindFirstFileW(sub, &sf);
        if (hSub == INVALID_HANDLE_VALUE) continue;

        do {
            if (sf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; /* skip subdirs inside the script folder */

            Script *s = Folder_Push(f);
            if (!s) break; /* OOM — stop adding scripts for this folder */

            /* gh_path: FolderName/filename.py */
            _snwprintf_s(s->gh_path, MAX_APPPATH, _TRUNCATE, L"%s/%s", f->name, sf.cFileName);

            /* local path */
            _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\%s",
                       g.cfg.cache_dir, f->name, sf.cFileName);

            /* display name: strip .py and format */
            wcsncpy_s(s->name, MAX_NAME, sf.cFileName, _TRUNCATE);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);

            s->source = SCRIPT_SRC_MAIN; /* loaded from built-in main repo cache */

            /* SHA from manifest */
            Sync_GetLocalSHA(s->gh_path, s->sha);

            /* Verify local file actually matches the manifest SHA.
               If not (e.g. previous failed download wrote wrong SHA to manifest),
               clear the SHA so sync will re-download on next refresh. */
            if (s->sha[0] && GetFileAttributes(s->local) != INVALID_FILE_ATTRIBUTES) {
                WCHAR computed[MAX_SHA] = {0};
                if (GitHub_ComputeFileSHA1(s->local, computed, MAX_SHA) &&
                    wcscmp(computed, s->sha) != 0) {
                    /* SHA mismatch — clear so sync re-downloads */
                    s->sha[0] = L'\0';
                    /* Also clear manifest entry */
                    WCHAR manifest[MAX_APPPATH];
                    ManifestPath(manifest, MAX_APPPATH);
                    WritePrivateProfileString(f->name, s->gh_path, L"", manifest);
                }
            }

        } while (FindNextFileW(hSub, &sf));
        FindClose(hSub);

        f->loaded = (f->count > 0); /* mark loaded only if at least one script was found on disk */

    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    } /* end if (main_repo_enabled) */

    /* Also scan local dirs so they appear at startup without internet */
    for (int i = 0; i < g.cfg.local_dir_count; i++) {
        if (g.cfg.local_dirs[i].enabled && g.cfg.local_dirs[i].path[0])
            Sync_LocalDir(&g.cfg.local_dirs[i]);
    }
}

/* ================================================================== */
/*  DeleteLocalScript  (static)                                        */
/*  Purpose: Deletes a cached script file from disk and removes its    */
/*           parent directory if the directory is now empty.           */
/*  In:  local_path — absolute path to the cached .py file            */
/*  Out: (void — file and possibly its parent folder removed)          */
/* ================================================================== */
static void DeleteLocalScript(const WCHAR *local_path)
{
    if (GetFileAttributes(local_path) != INVALID_FILE_ATTRIBUTES)
        DeleteFile(local_path);

    /* Remove parent folder if empty */
    WCHAR dir[MAX_APPPATH];
    wcsncpy_s(dir, MAX_APPPATH, local_path, _TRUNCATE);
    PathRemoveFileSpec(dir);
    RemoveDirectory(dir);  /* silently fails if not empty - that's fine */
}

/* ================================================================== */
/*  Sync_MergeFolder                                                    */
/*  Purpose: Merges an array of Script entries into an existing folder  */
/*           matched by name, or creates a new folder in g.folders[].  */
/*           Skips entries whose gh_path already exists in the folder. */
/*  In:  folder_name — storage/display name for the folder             */
/*       scripts     — array of Script entries to merge                */
/*       count       — number of entries in scripts                    */
/*  Out: (void — g.folders[] updated in place)                        */
/* ================================================================== */
void Sync_MergeFolder(const WCHAR *folder_name, Script *scripts, int count)
{
    /* Ensure any existing folder with this name has its scripts freed first */
    /* Find existing folder with same name */
    for (int fi = 0; fi < g.folder_count; fi++) {
        if (_wcsicmp(g.folders[fi].name, folder_name) == 0) {
            /* Merge: append scripts not already present */
            for (int si = 0; si < count; si++) {
                /* Only skip if exact same gh_path (same file from same repo).
               Scripts with the same display name from different sources
               are both added - the user can distinguish them by running them. */
                bool found = false;
                for (int ei = 0; ei < g.folders[fi].count; ei++) {
                    if (wcscmp(g.folders[fi].scripts[ei].gh_path,
                               scripts[si].gh_path) == 0) {
                        found = true; break;
                    }
                }
                if (!found) {
                    Script *dst = Folder_Push(&g.folders[fi]);
                    if (dst) *dst = scripts[si];
                }
            }
            return;
        }
    }
    /* New folder */
    if (g.folder_count >= MAX_FOLDERS) return;
    ScriptFolder *f = &g.folders[g.folder_count++];
    ZeroMemory(f, sizeof(*f));
    wcsncpy_s(f->name,    MAX_NAME, folder_name, _TRUNCATE);
    wcsncpy_s(f->display, MAX_NAME, f->name,     _TRUNCATE);
    Util_SnakeToTitle(f->display);
    if (!Folder_Alloc(f, count > 0 ? count : 64)) { g.folder_count--; return; }
    for (int i = 0; i < count; i++) {
        Script *dst = Folder_Push(f);
        if (dst) *dst = scripts[i];
    }
    f->loaded = true;
}

/* ================================================================== */
/*  Sync_ExtraRepo  (static)                                           */
/*  Purpose: Fetches the root and all sub-folder contents of a         */
/*           configured extra GitHub repository, downloads any changed */
/*           .py files, and merges results into g.folders[].          */
/*  In:  repo — extra repository configuration (url, token, branch)   */
/*       buf  — caller-supplied HTTP response buffer (HTTP_BUF_SIZE)  */
/*  Out: (void — g.folders[] updated; files downloaded to cache)      */
/* ================================================================== */
static void Sync_ExtraRepo(const ExtraRepo *repo, char *buf)
{
    WCHAR owner[MAX_NAME] = {0}, reponame[MAX_NAME] = {0};
    if (!GitHub_ParseOwnerRepo(repo->url, owner, reponame)) {
        PostStatus(L"Sources: invalid URL %s", repo->url);
        return;
    }

    const WCHAR *tok = repo->token[0] ? repo->token
                     : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
    const WCHAR *branch = repo->branch[0] ? repo->branch : L"main";

    PostStatus(L"Syncing %s/%s...", owner, reponame);

    /* Get root contents */
    WCHAR api_path[MAX_APPPATH];
    _snwprintf_s(api_path, MAX_APPPATH, _TRUNCATE, L"/repos/%s/%s/contents/",
               owner, reponame);

    DWORD len = 0;
    if (!GitHub_HttpGet(GITHUB_API_HOST, api_path, tok, buf, &len)) {
        PostStatus(L"Sources: failed to reach %s/%s", owner, reponame);

        return;
    }

    /* Parse folders from root — use a separate buffer for folder-contents fetches
       so that buf (root listing) remains valid throughout the outer loop. */
    char *fbuf = (char *)malloc(HTTP_BUF_SIZE);
    if (!fbuf) return;

    const char *p = buf;
    while ((p = strstr(p, "\"type\"")) != NULL) {
        char type_val[32] = {0};
        char name_a[MAX_NAME] = {0};
        const char *obj = p;
        while (obj > buf && *obj != '{') obj--;

        /* Get type */
        const char *tp = strstr(obj, "\"type\"");
        if (!tp) { p++; continue; }
        tp += 6;
        while (*tp == ' ' || *tp == ':') tp++;
        if (*tp != '"') { p++; continue; }
        tp++;
        int ti = 0;
        while (*tp && *tp != '"' && ti < 31) type_val[ti++] = *tp++;
        type_val[ti] = '\0';

        if (strcmp(type_val, "dir") != 0) { p = tp; continue; }

        /* Get name */
        const char *np = strstr(obj, "\"name\"");
        if (np) {
            np += 6;
            while (*np == ' ' || *np == ':') np++;
            if (*np == '"') {
                np++;
                int ni = 0;
                while (*np && *np != '"' && ni < MAX_NAME-1) name_a[ni++] = *np++;
                name_a[ni] = '\0';
            }
        }
        if (!name_a[0] || strcmp(name_a, "setup") == 0 || name_a[0] == '.') { p = tp; continue; }

        /* Get folder contents into fbuf so buf (root listing) stays intact */
        WCHAR folder_w[MAX_NAME] = {0};
        MultiByteToWideChar(CP_UTF8, 0, name_a, -1, folder_w, MAX_NAME);

        WCHAR folder_api[MAX_APPPATH];
        _snwprintf_s(folder_api, MAX_APPPATH, _TRUNCATE, L"/repos/%s/%s/contents/%s", owner, reponame, folder_w);

        DWORD flen = 0;
        if (!GitHub_HttpGet(GITHUB_API_HOST, folder_api, tok, fbuf, &flen))
            { p = tp; continue; }

        /* Parse scripts */
        Script *scripts = (Script *)calloc(MAX_SCRIPTS, sizeof(Script));
        if (!scripts) { p = tp; continue; }
        int    script_count = 0;
        WCHAR  cache_sub[MAX_APPPATH];
        _snwprintf_s(cache_sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s_%s\\%s",
                   g.cfg.cache_dir, owner, reponame, folder_w);
        SHCreateDirectoryEx(NULL, cache_sub, NULL);

        const char *fp = fbuf;
        while ((fp = strstr(fp, "\"type\"")) != NULL && script_count < MAX_SCRIPTS) {
            const char *fo = fp;
            while (fo > buf && *fo != '{') fo--;

            char ftype[16]={0}, fname[MAX_NAME]={0},
                 fpath[MAX_APPPATH]={0}, fsha[MAX_SHA]={0};

            /* type */
            const char *ftp = strstr(fo, "\"type\"");
            if (ftp) { ftp+=6; while(*ftp==' '||*ftp==':') ftp++;
                if(*ftp=='"'){ftp++; int i=0; while(*ftp&&*ftp!='"'&&i<15) ftype[i++]=*ftp++; ftype[i]=0;}}
            if (strcmp(ftype,"file")!=0) { fp=ftp?ftp+1:fp+1; continue; }

            /* name */
            const char *fnp = strstr(fo, "\"name\"");
            if (fnp) { fnp+=6; while(*fnp==' '||*fnp==':') fnp++;
                if(*fnp=='"'){fnp++; int i=0; while(*fnp&&*fnp!='"'&&i<MAX_NAME-1) fname[i++]=*fnp++; fname[i]=0;}}
            size_t nl = strlen(fname);
            if (nl < 4 || strcmp(fname+nl-3,".py")!=0) { fp=fnp?fnp+1:fp+1; continue; }

            /* path */
            const char *fpp = strstr(fo, "\"path\"");
            if (fpp) { fpp+=6; while(*fpp==' '||*fpp==':') fpp++;
                if(*fpp=='"'){fpp++; int i=0; while(*fpp&&*fpp!='"'&&i<MAX_APPPATH-1) fpath[i++]=*fpp++; fpath[i]=0;}}

            /* sha */
            const char *fsp = strstr(fo, "\"sha\"");
            if (fsp) { fsp+=5; while(*fsp==' '||*fsp==':') fsp++;
                if(*fsp=='"'){fsp++; int i=0; while(*fsp&&*fsp!='"'&&i<MAX_SHA-1) fsha[i++]=*fsp++; fsha[i]=0;}}

            Script *s = &scripts[script_count++];
            ZeroMemory(s, sizeof(*s));
            MultiByteToWideChar(CP_UTF8, 0, fname, -1, s->name,    MAX_NAME);
            MultiByteToWideChar(CP_UTF8, 0, fpath, -1, s->gh_path, MAX_APPPATH);
            MultiByteToWideChar(CP_UTF8, 0, fsha,  -1, s->sha,     MAX_SHA);
            /* Build local path from original filename (includes .py),
               then strip .py from display name */
            WCHAR fname_w[MAX_NAME] = {0};
            MultiByteToWideChar(CP_UTF8, 0, fname, -1, fname_w, MAX_NAME);
            _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s", cache_sub, fname_w);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            s->source = SCRIPT_SRC_EXTRA;

            /* Download if changed */
            WCHAR existing_sha[MAX_SHA] = {0};
            Sync_GetLocalSHA(s->gh_path, existing_sha);
            if (wcscmp(existing_sha, s->sha) != 0 || 
                GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES) {
                WCHAR raw_path[MAX_APPPATH];
                _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/%s", owner, reponame, branch, s->gh_path);
                GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path,
                                       s->local, tok);
            }

            fp = fsp ? fsp+1 : fp+1;
        }

        Sync_MergeFolder(folder_w, scripts, script_count);
        free(scripts);
        p = tp;
    }

    free(fbuf);
}

/* ================================================================== */
/*  Sync_LocalDir  (static)                                            */
/*  Purpose: Scans a local filesystem directory; each sub-directory    */
/*           becomes a tab and every .py file inside it becomes a      */
/*           script entry merged into g.folders[]. Also copies a       */
/*           requirements.txt from the local setup/ sub-folder into    */
/*           the cache setup directory if one is found.                */
/*  In:  dir — local directory configuration entry (path, enabled)    */
/*  Out: (void — g.folders[] updated; requirements.txt possibly copied)*/
/* ================================================================== */
static void Sync_LocalDir(const LocalDir *dir)
{
    WCHAR pattern[MAX_APPPATH];
    _snwprintf_s(pattern, MAX_APPPATH, _TRUNCATE, L"%s\\*", dir->path);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0)      continue;
        if (wcscmp(fd.cFileName, L"..") == 0)     continue;
        if (_wcsicmp(fd.cFileName, L"setup") == 0) continue; /* skip setup */

        /* Scan .py files in subfolder */
        WCHAR sub[MAX_APPPATH];
        _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\*.py",
                   dir->path, fd.cFileName);

        Script *scripts = (Script *)calloc(MAX_SCRIPTS, sizeof(Script));
        if (!scripts) continue;
        int    count = 0;

        WIN32_FIND_DATAW sf;
        HANDLE hs = FindFirstFileW(sub, &sf);
        if (hs == INVALID_HANDLE_VALUE) { free(scripts); continue; }
        do {
            if (sf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (count >= MAX_SCRIPTS) break;
            Script *s = &scripts[count++];
            ZeroMemory(s, sizeof(*s));
            /* Local scripts have no gh_path or sha */
            _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\%s",
                       dir->path, fd.cFileName, sf.cFileName);
            wcsncpy_s(s->name,    MAX_NAME,    sf.cFileName, _TRUNCATE);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            /* Use local path as gh_path for uniqueness */
            wcsncpy_s(s->gh_path, MAX_APPPATH, s->local,     _TRUNCATE); /* local scripts have no GitHub path; use local path as a unique key for prefs */
            s->source = SCRIPT_SRC_LOCAL;
        } while (FindNextFileW(hs, &sf));
        FindClose(hs);

        if (count > 0)
            Sync_MergeFolder(fd.cFileName, scripts, count);
        free(scripts);

    } while (FindNextFileW(h, &fd));
    FindClose(h);

    /* Check for a setup/requirements.txt in this local dir */
    WCHAR req[MAX_APPPATH];
    _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\requirements.txt", dir->path);
    if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
        /* Store path so Runner_UpdateDeps can find it.
           We reuse the cache_dir setup folder concept - just copy the path
           into the setup subfolder of cache so runner finds it automatically. */
        WCHAR dest_dir[MAX_APPPATH];
        _snwprintf_s(dest_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup", g.cfg.cache_dir);
        SHCreateDirectoryEx(NULL, dest_dir, NULL);
        WCHAR dest[MAX_APPPATH];
        _snwprintf_s(dest, MAX_APPPATH, _TRUNCATE, L"%s\\requirements.txt", dest_dir);
        /* Only copy if newer or missing */
        if (GetFileAttributes(dest) == INVALID_FILE_ATTRIBUTES) /* only copy if not already present */
            CopyFile(req, dest, FALSE);
    }
}

/* ================================================================== */
/*  Sync_Thread                                                         */
/*  Purpose: Background worker that performs the full sync sequence:   */
/*           fetches the GitHub root, detects folder/script additions  */
/*           and removals, downloads changed files, syncs extra repos  */
/*           and local dirs, saves the manifest, then posts            */
/*           WM_SYNC_DONE with a heap-allocated SyncResult.           */
/*  In:  unused — LPVOID thread parameter (not used)                   */
/*  Out: 0 on success; 1 if OOM prevented posting the done message     */
/* ================================================================== */
DWORD WINAPI Sync_Thread(LPVOID unused)
{
    (void)unused;

    const WCHAR *token = g.cfg.github_token[0]
                         ? g.cfg.github_token : NULL;

    SyncResult *sr = (SyncResult *)calloc(1, sizeof(SyncResult));
    if (!sr) {
        /* OOM: post NULL so the main window clears the syncing flag even without a result */
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)NULL, 0);
        return 1;
    }

    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) {
        sr->status = SR_API_ERROR;
        wcsncpy_s(sr->message, 256, L"Out of memory.", _TRUNCATE);
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
        return 1;
    }

        if (g.cfg.main_repo_enabled) {
    /* ── Step 1: Fetch root to get current folder list ───────────── */
    PostStatus(L"Connecting to GitHub\u2026");

    WCHAR api_root[MAX_APPPATH];
    _snwprintf_s(api_root, MAX_APPPATH, _TRUNCATE, L"/repos/%s/%s/contents/",
               GITHUB_OWNER, GITHUB_REPO);

    DWORD len = 0;
    bool main_repo_ok = true;
    if (!GitHub_HttpGet(GITHUB_API_HOST, api_root, token, buf, &len)) {
        main_repo_ok = false;
        sr->status = SR_NO_INTERNET;
        if (!g.cfg.offline_use_cache) {
            /* User opted out of offline mode \u2014 clear folders so the UI shows nothing */
            EnterCriticalSection(&g.cs_folders);
            for (int _fi = 0; _fi < g.folder_count; _fi++) Folder_Free(&g.folders[_fi]);
            g.folder_count = 0;
            LeaveCriticalSection(&g.cs_folders);
            wcsncpy_s(sr->message, 256,
                L"\u26a0 No internet connection.", _TRUNCATE);
        } else if (g.folder_count > 0) {
            /* Offline but cache exists \u2014 keep showing cached scripts with a warning */
            _snwprintf_s(sr->message, 255, _TRUNCATE,
                L"\u26a0 Offline \u2013 showing %d cached folder(s). Scripts may be out of date.",
                g.folder_count);
        } else {
            wcsncpy_s(sr->message, 256,
                L"\u26a0 No internet connection. No cached scripts found.", _TRUNCATE);
        }
        /* Do NOT return here \u2014 fall through to sync extra repos and local dirs,
           which may be reachable even when the main repo is not. */
    }

    if (main_repo_ok) {

    /* Connected ─ clear folders so disabled sources don't linger before rebuild.
       Hold cs_folders to guard against a concurrent UI-thread paint reading freed pointers. */
    EnterCriticalSection(&g.cs_folders);
    for (int _fi = 0; _fi < g.folder_count; _fi++) Folder_Free(&g.folders[_fi]);
    g.folder_count = 0;
    LeaveCriticalSection(&g.cs_folders);

    /* ── Step 2: Parse root and detect folder changes ───────────── */

    /* Save the old folder names to detect removals */
    WCHAR old_names[MAX_FOLDERS][MAX_NAME];
    int   old_count = g.folder_count;
    for (int i = 0; i < old_count; i++)
        wcsncpy_s(old_names[i], MAX_NAME, g.folders[i].name, _TRUNCATE);

    /* Parse new folder list from API */
    GitHub_ParseRoot(buf);

    /* Count additions */
    for (int ni = 0; ni < g.folder_count; ni++) {
        bool found = false;
        for (int oi = 0; oi < old_count; oi++) {
            if (wcscmp(g.folders[ni].name, old_names[oi]) == 0) {
                found = true; break;
            }
        }
        if (!found) sr->folders_added++;
    }

    /* Count removals and delete their cached files */
    for (int oi = 0; oi < old_count; oi++) {
        bool found = false;
        for (int ni = 0; ni < g.folder_count; ni++) {
            if (wcscmp(old_names[oi], g.folders[ni].name) == 0) {
                found = true; break;
            }
        }
        if (!found) {
            sr->folders_removed++;
            /* Delete the folder's cache directory */
            WCHAR cache_dir[MAX_APPPATH];
            _snwprintf_s(cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\%s",
                       g.cfg.cache_dir, old_names[oi]);
            /* Simple recursive delete via SHFileOperation */
            WCHAR del_from[MAX_APPPATH + 2];
            _snwprintf_s(del_from, MAX_APPPATH + 1, _TRUNCATE, L"%s\\", cache_dir);
            del_from[wcslen(del_from) + 1] = L'\0';  /* SHFileOperation requires double-null terminator after the path */
            SHFILEOPSTRUCT fo = {
                .wFunc  = FO_DELETE,
                .pFrom  = del_from,
                .fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT
            };
            SHFileOperation(&fo);
        }
    }

    /* ── Step 3: For each folder, sync scripts ───────────────────── */
    for (int fi = 0; fi < g.folder_count; fi++) {
        ScriptFolder *f = &g.folders[fi];

        WCHAR status_msg[128];
        _snwprintf_s(status_msg, 127, _TRUNCATE, L"Checking %s\u2026", f->display);
        PostStatus(L"%s", status_msg);

        /* Fetch folder contents from API */
        WCHAR api_folder[MAX_APPPATH];
        _snwprintf_s(api_folder, MAX_APPPATH, _TRUNCATE, L"/repos/%s/%s/contents/%s",
                   GITHUB_OWNER, GITHUB_REPO, f->name);

        len = 0;
        if (!GitHub_HttpGet(GITHUB_API_HOST, api_folder,
                             token, buf, &len)) {
            sr->status = SR_PARTIAL;
            continue;
        }

        /* Save old script list for removal detection */
        Script *old_scripts = (Script *)calloc(MAX_SCRIPTS, sizeof(Script));
        if (!old_scripts) continue;
        int    old_scount = f->count;
        if (old_scount > 0)
            memcpy_s(old_scripts, MAX_SCRIPTS * sizeof(Script),
                     f->scripts, old_scount * sizeof(Script));

        /* Parse new script list (fills f->scripts[], f->count) */
        GitHub_ParseFolder(buf, fi);

        /* Tag all scripts in this folder as main-repo origin */
        for (int si = 0; si < f->count; si++)
            f->scripts[si].source = SCRIPT_SRC_MAIN;

        /* Ensure cache folder exists */
        WCHAR folder_cache[MAX_APPPATH];
        _snwprintf_s(folder_cache, MAX_APPPATH, _TRUNCATE, L"%s\\%s",
                   g.cfg.cache_dir, f->name);
        SHCreateDirectoryEx(NULL, folder_cache, NULL);

        /* Detect newly added scripts */
        for (int si = 0; si < f->count; si++) {
            bool existed = false;
            for (int oi = 0; oi < old_scount; oi++) {
                if (wcscmp(f->scripts[si].gh_path,
                           old_scripts[oi].gh_path) == 0) {
                    existed = true; break;
                }
            }
            if (!existed) sr->scripts_added++;
        }

        /* Detect removed scripts and delete from cache */
        for (int oi = 0; oi < old_scount; oi++) {
            bool still_present = false;
            for (int si = 0; si < f->count; si++) {
                if (wcscmp(old_scripts[oi].gh_path,
                           f->scripts[si].gh_path) == 0) {
                    still_present = true; break;
                }
            }
            if (!still_present) {
                sr->scripts_removed++;
                DeleteLocalScript(old_scripts[oi].local);
            }
        }
        free(old_scripts);

        /* ── Step 4: Download changed or missing scripts ─────────── */
        for (int si = 0; si < f->count; si++) {
            Script *s = &f->scripts[si];

            /* Check stored SHA vs GitHub SHA */
            WCHAR stored_sha[MAX_SHA] = {0};
            bool  have_local = Sync_GetLocalSHA(s->gh_path, stored_sha);

            bool sha_changed   = (wcscmp(stored_sha, s->sha) != 0);
            bool file_missing  = (GetFileAttributes(s->local)
                                  == INVALID_FILE_ATTRIBUTES);

            /* Download when: no manifest entry, SHA changed on GitHub, or cached file is gone */
            if (!have_local || sha_changed || file_missing) {
                WCHAR dl_msg[128];
                const WCHAR *fname = wcsrchr(s->gh_path, L'/');
                _snwprintf_s(dl_msg, 127, _TRUNCATE, L"Downloading %s\u2026",
                           fname ? fname + 1 : s->gh_path);
                PostStatus(L"%s", dl_msg);

                if (GitHub_DownloadRaw(s->gh_path, s->local, token)) {
                    sr->scripts_updated++;
                } else {
                    /* Download failed — revert the in-memory SHA to the old manifest value.
                       Saving the old SHA prevents a false tamper warning on the next run;
                       the script will simply retry on the next sync cycle. */
                    wcsncpy_s(s->sha, MAX_SHA, stored_sha, _TRUNCATE);
                    sr->status = SR_PARTIAL;
                }
            }
        }
    }

    /* Step 5: Download setup files */
    {
        WCHAR setup_dir[MAX_APPPATH];
        _snwprintf_s(setup_dir, MAX_APPPATH, _TRUNCATE, L"%s" L"\\" L"setup", g.cfg.cache_dir);
        setup_dir[MAX_APPPATH - 1] = L'\0';
        SHCreateDirectoryEx(NULL, setup_dir, NULL);

        const WCHAR *gh_files[] = {
            L"setup/requirements.txt",
            NULL
        };
        const WCHAR *local_names[] = {
            L"requirements.txt",
            NULL
        };
        for (int i = 0; gh_files[i]; i++) {
            WCHAR lpath[MAX_APPPATH];
            _snwprintf_s(lpath, MAX_APPPATH, _TRUNCATE, L"%s" L"\\" L"%s", setup_dir, local_names[i]);
            lpath[MAX_APPPATH - 1] = L'\0';
            GitHub_DownloadRaw(gh_files[i], lpath, token);
        }
    }

    } /* end if (main_repo_ok) */

    } else {
        /* Main repo disabled — clear now so extra repos and local dirs
           rebuild from scratch rather than merging onto stale manifest data. */
        EnterCriticalSection(&g.cs_folders);
        for (int _fi = 0; _fi < g.folder_count; _fi++) Folder_Free(&g.folders[_fi]);
        g.folder_count = 0;
        LeaveCriticalSection(&g.cs_folders);
    } /* end if/else (main_repo_enabled) */

    /* ── Step 6: Sync extra GitHub repos ───────────────────────── */
    {
        char *xbuf = (char *)malloc(HTTP_BUF_SIZE);
        if (xbuf) {
            for (int i = 0; i < g.cfg.extra_repo_count; i++) {
                ExtraRepo *xr = &g.cfg.extra_repos[i];
                if (!xr->enabled || !xr->url[0]) continue; /* skip disabled or empty extra repo entries */
                Sync_ExtraRepo(xr, xbuf);

                /* Cache requirements.txt for this repo in unique subfolder */
                WCHAR owner[MAX_NAME]={0}, reponame[MAX_NAME]={0};
                if (GitHub_ParseOwnerRepo(xr->url, owner, reponame)) {
                    WCHAR sub[MAX_APPPATH];
                    _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\%s_%s",
                               g.cfg.cache_dir, owner, reponame);
                    SHCreateDirectoryEx(NULL, sub, NULL);
                    WCHAR req[MAX_APPPATH];
                    _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\requirements.txt", sub);
                    if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES) { /* only download if not already cached */
                        const WCHAR *tok = xr->token[0] ? xr->token
                                         : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
                        const WCHAR *branch = xr->branch[0] ? xr->branch : L"main";
                        WCHAR raw_path[MAX_APPPATH];
                        _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/setup/requirements.txt",
                                   owner, reponame, branch);
                        GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, req, tok);
                    }
                }
            }
            free(xbuf);
        }
    }

    /* ── Step 7: Scan local dirs ─────────────────────────────────── */
    for (int i = 0; i < g.cfg.local_dir_count; i++) {
        if (g.cfg.local_dirs[i].enabled && g.cfg.local_dirs[i].path[0])
            Sync_LocalDir(&g.cfg.local_dirs[i]);
    }

    /* ── Step 8: Persist updated manifest ───────────────────────── */
    Sync_SaveManifest();


    /* ── Step 9: Build human-readable result message ────────────── */
    if (sr->status == SR_OK) {
        if (sr->scripts_updated == 0 && sr->folders_added == 0
            && sr->folders_removed == 0 && sr->scripts_added == 0
            && sr->scripts_removed == 0) {
            /* Nothing changed — brief "up to date" message */
            wcsncpy_s(sr->message, 256, L"All scripts are up to date.", _TRUNCATE);
        } else {
            /* Something changed — show a change summary */
            _snwprintf_s(sr->message, 255, _TRUNCATE, L"Sync complete. "
                L"+%d/-%d folders, "
                L"+%d/-%d scripts, "
                L"%d updated.",
                sr->folders_added,  sr->folders_removed,
                sr->scripts_added,  sr->scripts_removed,
                sr->scripts_updated);
        }
    } else if (sr->status == SR_PARTIAL) { /* at least one folder or file failed to download */
        _snwprintf_s(sr->message, 255, _TRUNCATE, L"Sync partial \u2013 some downloads failed. "
                   L"%d updated.", sr->scripts_updated);
    }

    free(buf);
    PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
    return 0;
}
