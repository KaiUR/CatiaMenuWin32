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
 * Manifest format  (%APPDATA%\CatiaMenuWin32\manifest.ini):
 *   [FolderName]
 *   script_gh_path=sha40hex
 *
 * All settings (python path, cache dir, token, etc.) are stored in
 *   %APPDATA%\CatiaMenuWin32\settings.ini
 *
 * CatiaMenuWin32
 * Author : Kai-Uwe Rathjen
 * License: MIT
 */

#include "main.h"

/* ================================================================== */
/*  Manifest helpers                                                    */
/* ================================================================== */

/* Full path to manifest.ini */
static void ManifestPath(WCHAR *out, int max)
{
    _snwprintf(out, max - 1, L"%s\\%s", g.appdata_dir, MANIFEST_FILE);
}

/*
 * Sync_GetLocalSHA
 * Read the stored SHA for gh_path from manifest.ini.
 * Returns false (and sha_out = "") if not found.
 */
bool Sync_GetLocalSHA(const WCHAR *gh_path, WCHAR *sha_out)
{
    sha_out[0] = L'\0';

    /* Section = folder name = first path component */
    WCHAR section[MAX_NAME] = {0};
    const WCHAR *slash = wcschr(gh_path, L'/');
    if (!slash) return false;
    size_t n = (size_t)(slash - gh_path);
    wcsncpy(section, gh_path, n < MAX_NAME ? n : MAX_NAME - 1);

    WCHAR manifest[MAX_APPPATH];
    ManifestPath(manifest, MAX_APPPATH);

    GetPrivateProfileString(section, gh_path, L"",
                            sha_out, MAX_SHA, manifest);
    return sha_out[0] != L'\0';
}

/*
 * Sync_SaveManifest
 * Write the SHA of every known script into manifest.ini.
 * Clears the old file first (delete + rewrite) so removed
 * scripts don't linger.
 */
static void Sync_LocalDir(const LocalDir *dir);
static void Sync_ExtraRepo(const ExtraRepo *repo, char *buf);

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

void Sync_LoadManifest(void)
{
    /* Scan the cache directory and populate g.folders[] from what is
       already on disk. This runs at startup so scripts are visible
       immediately even before the sync completes or if there is no internet. */
    if (!g.cfg.cache_dir[0]) return;

    g.folder_count = 0;

    WCHAR pattern[MAX_APPPATH];
    _snwprintf(pattern, MAX_APPPATH - 1, L"%s\\*", g.cfg.cache_dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0)     continue;
        if (wcscmp(fd.cFileName, L"..") == 0)    continue;
        if (wcscmp(fd.cFileName, L"setup") == 0) continue;
        if (g.folder_count >= MAX_FOLDERS)        break;

        ScriptFolder *f = &g.folders[g.folder_count++];
        memset(f, 0, sizeof(*f));
        wcsncpy(f->name, fd.cFileName, MAX_NAME - 1);
        wcscpy(f->display, f->name);
        Util_SnakeToTitle(f->display);

        /* Scan .py files inside this folder */
        WCHAR sub[MAX_APPPATH];
        _snwprintf(sub, MAX_APPPATH - 1, L"%s\\%s\\*.py",
                   g.cfg.cache_dir, f->name);

        WIN32_FIND_DATAW sf;
        HANDLE hSub = FindFirstFileW(sub, &sf);
        if (hSub == INVALID_HANDLE_VALUE) continue;

        do {
            if (sf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (f->count >= MAX_SCRIPTS) break;

            Script *s = &f->scripts[f->count++];
            memset(s, 0, sizeof(*s));

            /* gh_path: FolderName/filename.py */
            _snwprintf(s->gh_path, MAX_APPPATH - 1,
                       L"%s/%s", f->name, sf.cFileName);

            /* local path */
            _snwprintf(s->local, MAX_APPPATH - 1,
                       L"%s\\%s\\%s",
                       g.cfg.cache_dir, f->name, sf.cFileName);

            /* display name: strip .py and format */
            wcsncpy(s->name, sf.cFileName, MAX_NAME - 1);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);

            /* SHA from manifest */
            Sync_GetLocalSHA(s->gh_path, s->sha);

        } while (FindNextFileW(hSub, &sf));
        FindClose(hSub);

        f->loaded = (f->count > 0);

    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    /* Also scan local dirs so they appear at startup without internet */
    for (int i = 0; i < g.cfg.local_dir_count; i++) {
        if (g.cfg.local_dirs[i].enabled && g.cfg.local_dirs[i].path[0])
            Sync_LocalDir(&g.cfg.local_dirs[i]);
    }
}

/* ================================================================== */
/*  Delete local file + its folder if now empty                        */
/* ================================================================== */
static void DeleteLocalScript(const WCHAR *local_path)
{
    if (GetFileAttributes(local_path) != INVALID_FILE_ATTRIBUTES)
        DeleteFile(local_path);

    /* Remove parent folder if empty */
    WCHAR dir[MAX_APPPATH];
    wcsncpy(dir, local_path, MAX_APPPATH - 1);
    PathRemoveFileSpec(dir);
    RemoveDirectory(dir);  /* silently fails if not empty - that's fine */
}

/* ================================================================== */
/*  Sync_Thread  -  background worker                                  */
/* ================================================================== */
/* ================================================================== */
/*  Sync_MergeFolder                                                    */
/*  Add scripts to an existing folder or create a new one.             */
/* ================================================================== */
void Sync_MergeFolder(const WCHAR *folder_name, Script *scripts, int count)
{
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
                if (!found && g.folders[fi].count < MAX_SCRIPTS)
                    g.folders[fi].scripts[g.folders[fi].count++] = scripts[si];
            }
            return;
        }
    }
    /* New folder */
    if (g.folder_count >= MAX_FOLDERS) return;
    ScriptFolder *f = &g.folders[g.folder_count++];
    memset(f, 0, sizeof(*f));
    wcsncpy(f->name, folder_name, MAX_NAME - 1);
    wcscpy(f->display, f->name);
    Util_SnakeToTitle(f->display);
    int n = count < MAX_SCRIPTS ? count : MAX_SCRIPTS;
    for (int i = 0; i < n; i++)
        f->scripts[f->count++] = scripts[i];
    f->loaded = true;
}

/* ================================================================== */
/*  Sync_ExtraRepo                                                      */
/*  Sync one extra GitHub repository into g.folders via merging.       */
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
    _snwprintf(api_path, MAX_APPPATH-1, L"/repos/%s/%s/contents/",
               owner, reponame);

    DWORD len = 0;
    if (!GitHub_HttpGet(GITHUB_API_HOST, api_path, tok, buf, &len)) {
        PostStatus(L"Sources: failed to reach %s/%s", owner, reponame);
        return;
    }

    /* Parse folders from root */
    /* Reuse existing json parser logic inline */
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

        /* Get folder contents */
        WCHAR folder_w[MAX_NAME] = {0};
        MultiByteToWideChar(CP_UTF8, 0, name_a, -1, folder_w, MAX_NAME);

        WCHAR folder_api[MAX_APPPATH];
        _snwprintf(folder_api, MAX_APPPATH-1,
                   L"/repos/%s/%s/contents/%s", owner, reponame, folder_w);

        DWORD flen = 0;
        if (!GitHub_HttpGet(GITHUB_API_HOST, folder_api, tok, buf, &flen))
            { p = tp; continue; }

        /* Parse scripts */
        Script scripts[MAX_SCRIPTS];
        int    script_count = 0;
        WCHAR  cache_sub[MAX_APPPATH];
        _snwprintf(cache_sub, MAX_APPPATH-1, L"%s\\%s_%s\\%s",
                   g.cfg.cache_dir, owner, reponame, folder_w);
        SHCreateDirectoryEx(NULL, cache_sub, NULL);

        const char *fp = buf;
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
            memset(s, 0, sizeof(*s));
            MultiByteToWideChar(CP_UTF8, 0, fname, -1, s->name,    MAX_NAME);
            MultiByteToWideChar(CP_UTF8, 0, fpath, -1, s->gh_path, MAX_APPPATH);
            MultiByteToWideChar(CP_UTF8, 0, fsha,  -1, s->sha,     MAX_SHA);
            _snwprintf(s->local, MAX_APPPATH-1, L"%s\\%s", cache_sub,
                       s->name);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            wcscat(s->local, L".py");

            /* Download if changed */
            WCHAR existing_sha[MAX_SHA] = {0};
            Sync_GetLocalSHA(s->gh_path, existing_sha);
            if (wcscmp(existing_sha, s->sha) != 0 || 
                GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES) {
                WCHAR raw_path[MAX_APPPATH];
                _snwprintf(raw_path, MAX_APPPATH-1,
                           L"/%s/%s/%s/%s", owner, reponame, branch, s->gh_path);
                GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path,
                                       s->local, tok);
            }

            fp = fsp ? fsp+1 : fp+1;
        }

        Sync_MergeFolder(folder_w, scripts, script_count);
        p = tp;
    }
}

/* ================================================================== */
/*  Sync_LocalDir                                                       */
/*  Scan a local folder - subfolders become tabs, .py files = scripts  */
/* ================================================================== */
static void Sync_LocalDir(const LocalDir *dir)
{
    WCHAR pattern[MAX_APPPATH];
    _snwprintf(pattern, MAX_APPPATH-1, L"%s\\*", dir->path);

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
        _snwprintf(sub, MAX_APPPATH-1, L"%s\\%s\\*.py",
                   dir->path, fd.cFileName);

        Script scripts[MAX_SCRIPTS];
        int    count = 0;

        WIN32_FIND_DATAW sf;
        HANDLE hs = FindFirstFileW(sub, &sf);
        if (hs == INVALID_HANDLE_VALUE) continue;
        do {
            if (sf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (count >= MAX_SCRIPTS) break;
            Script *s = &scripts[count++];
            memset(s, 0, sizeof(*s));
            /* Local scripts have no gh_path or sha */
            _snwprintf(s->local, MAX_APPPATH-1, L"%s\\%s\\%s",
                       dir->path, fd.cFileName, sf.cFileName);
            wcsncpy(s->name, sf.cFileName, MAX_NAME-1);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            /* Use local path as gh_path for uniqueness */
            wcsncpy(s->gh_path, s->local, MAX_APPPATH-1);
        } while (FindNextFileW(hs, &sf));
        FindClose(hs);

        if (count > 0)
            Sync_MergeFolder(fd.cFileName, scripts, count);

    } while (FindNextFileW(h, &fd));
    FindClose(h);

    /* Check for a setup/requirements.txt in this local dir */
    WCHAR req[MAX_APPPATH];
    _snwprintf(req, MAX_APPPATH - 1, L"%s\\setup\\requirements.txt", dir->path);
    if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES) {
        /* Store path so Runner_UpdateDeps can find it.
           We reuse the cache_dir setup folder concept - just copy the path
           into the setup subfolder of cache so runner finds it automatically. */
        WCHAR dest_dir[MAX_APPPATH];
        _snwprintf(dest_dir, MAX_APPPATH - 1, L"%s\\setup", g.cfg.cache_dir);
        SHCreateDirectoryEx(NULL, dest_dir, NULL);
        WCHAR dest[MAX_APPPATH];
        _snwprintf(dest, MAX_APPPATH - 1, L"%s\\requirements.txt", dest_dir);
        /* Only copy if newer or missing */
        if (GetFileAttributes(dest) == INVALID_FILE_ATTRIBUTES)
            CopyFile(req, dest, FALSE);
    }
}

DWORD WINAPI Sync_Thread(LPVOID unused)
{
    (void)unused;

    const WCHAR *token = g.cfg.github_token[0]
                         ? g.cfg.github_token : NULL;

    SyncResult *sr = (SyncResult *)calloc(1, sizeof(SyncResult));
    if (!sr) {
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)NULL, 0);
        return 1;
    }

    char *buf = (char *)malloc(HTTP_BUF_SIZE);
    if (!buf) {
        sr->status = SR_API_ERROR;
        wcscpy(sr->message, L"Out of memory.");
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
        return 1;
    }

    /* ── Step 1: Fetch root to get current folder list ───────────── */
    PostStatus(L"Connecting to GitHub\u2026");

    WCHAR api_root[MAX_APPPATH];
    _snwprintf(api_root, MAX_APPPATH - 1,
               L"/repos/%s/%s/contents/",
               GITHUB_OWNER, GITHUB_REPO);

    DWORD len = 0;
    if (!GitHub_HttpGet(GITHUB_API_HOST, api_root, token, buf, &len)) {
        sr->status = SR_NO_INTERNET;
        if (g.folder_count > 0) {
            _snwprintf(sr->message, 255,
                       L"Showing %d cached folder(s). Connect to internet to sync.",
                       g.folder_count);
        } else {
            wcscpy(sr->message,
                   L"No internet connection. No cached scripts found.");
        }
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
        free(buf);
        return 0;
    }

    /* ── Step 2: Parse root and detect folder changes ───────────── */

    /* Save the old folder names to detect removals */
    WCHAR old_names[MAX_FOLDERS][MAX_NAME];
    int   old_count = g.folder_count;
    for (int i = 0; i < old_count; i++)
        wcscpy(old_names[i], g.folders[i].name);

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
            _snwprintf(cache_dir, MAX_APPPATH - 1, L"%s\\%s",
                       g.cfg.cache_dir, old_names[oi]);
            /* Simple recursive delete via SHFileOperation */
            WCHAR del_from[MAX_APPPATH + 2];
            _snwprintf(del_from, MAX_APPPATH + 1, L"%s\\", cache_dir);
            del_from[wcslen(del_from) + 1] = L'\0';  /* double-NUL */
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
        _snwprintf(status_msg, 127, L"Checking %s\u2026", f->display);
        PostStatus(L"%s", status_msg);

        /* Fetch folder contents from API */
        WCHAR api_folder[MAX_APPPATH];
        _snwprintf(api_folder, MAX_APPPATH - 1,
                   L"/repos/%s/%s/contents/%s",
                   GITHUB_OWNER, GITHUB_REPO, f->name);

        len = 0;
        if (!GitHub_HttpGet(GITHUB_API_HOST, api_folder,
                             token, buf, &len)) {
            sr->status = SR_PARTIAL;
            continue;
        }

        /* Save old script list for removal detection */
        Script old_scripts[MAX_SCRIPTS];
        int    old_scount = f->count;
        if (old_scount > 0)
            memcpy(old_scripts, f->scripts,
                   old_scount * sizeof(Script));

        /* Parse new script list (fills f->scripts[], f->count) */
        GitHub_ParseFolder(buf, fi);

        /* Ensure cache folder exists */
        WCHAR folder_cache[MAX_APPPATH];
        _snwprintf(folder_cache, MAX_APPPATH - 1, L"%s\\%s",
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

        /* ── Step 4: Download changed or missing scripts ─────────── */
        for (int si = 0; si < f->count; si++) {
            Script *s = &f->scripts[si];

            /* Check stored SHA vs GitHub SHA */
            WCHAR stored_sha[MAX_SHA] = {0};
            bool  have_local = Sync_GetLocalSHA(s->gh_path, stored_sha);

            bool sha_changed   = (wcscmp(stored_sha, s->sha) != 0);
            bool file_missing  = (GetFileAttributes(s->local)
                                  == INVALID_FILE_ATTRIBUTES);

            if (!have_local || sha_changed || file_missing) {
                WCHAR dl_msg[128];
                const WCHAR *fname = wcsrchr(s->gh_path, L'/');
                _snwprintf(dl_msg, 127, L"Downloading %s\u2026",
                           fname ? fname + 1 : s->gh_path);
                PostStatus(L"%s", dl_msg);

                if (GitHub_DownloadRaw(s->gh_path, s->local, token)) {
                    sr->scripts_updated++;
                } else {
                    sr->status = SR_PARTIAL;
                }
            }
        }
    }

    /* Step 5: Download setup files */
    {
        WCHAR setup_dir[MAX_APPPATH];
        _snwprintf(setup_dir, MAX_APPPATH - 1,
                   L"%s" L"\\" L"setup", g.cfg.cache_dir);
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
            _snwprintf(lpath, MAX_APPPATH - 1,
                       L"%s" L"\\" L"%s", setup_dir, local_names[i]);
            lpath[MAX_APPPATH - 1] = L'\0';
            GitHub_DownloadRaw(gh_files[i], lpath, token);
        }
    }

    /* ── Step 6: Sync extra GitHub repos ───────────────────────── */
    {
        char *xbuf = (char *)malloc(HTTP_BUF_SIZE);
        if (xbuf) {
            for (int i = 0; i < g.cfg.extra_repo_count; i++) {
                ExtraRepo *xr = &g.cfg.extra_repos[i];
                if (!xr->enabled || !xr->url[0]) continue;
                Sync_ExtraRepo(xr, xbuf);

                /* Cache requirements.txt for this repo in unique subfolder */
                WCHAR owner[MAX_NAME]={0}, reponame[MAX_NAME]={0};
                if (GitHub_ParseOwnerRepo(xr->url, owner, reponame)) {
                    WCHAR sub[MAX_APPPATH];
                    _snwprintf(sub, MAX_APPPATH-1, L"%s\\setup\\%s_%s",
                               g.cfg.cache_dir, owner, reponame);
                    SHCreateDirectoryEx(NULL, sub, NULL);
                    WCHAR req[MAX_APPPATH];
                    _snwprintf(req, MAX_APPPATH-1, L"%s\\requirements.txt", sub);
                    if (GetFileAttributes(req) == INVALID_FILE_ATTRIBUTES) {
                        const WCHAR *tok = xr->token[0] ? xr->token
                                         : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
                        const WCHAR *branch = xr->branch[0] ? xr->branch : L"main";
                        WCHAR raw_path[MAX_APPPATH];
                        _snwprintf(raw_path, MAX_APPPATH-1,
                                   L"/%s/%s/%s/setup/requirements.txt",
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

    /* ── Step 6: Build human-readable result message ────────────── */
    if (sr->status == SR_OK) {
        if (sr->scripts_updated == 0 && sr->folders_added == 0
            && sr->folders_removed == 0 && sr->scripts_added == 0
            && sr->scripts_removed == 0) {
            wcscpy(sr->message, L"All scripts are up to date.");
        } else {
            _snwprintf(sr->message, 255,
                L"Sync complete. "
                L"+%d/-%d folders, "
                L"+%d/-%d scripts, "
                L"%d updated.",
                sr->folders_added,  sr->folders_removed,
                sr->scripts_added,  sr->scripts_removed,
                sr->scripts_updated);
        }
    } else if (sr->status == SR_PARTIAL) {
        _snwprintf(sr->message, 255,
                   L"Sync partial \u2013 some downloads failed. "
                   L"%d updated.", sr->scripts_updated);
    }

    free(buf);
    PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
    return 0;
}
