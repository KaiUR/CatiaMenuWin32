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
static bool Sync_ExtraRepo(const ExtraRepo *repo);

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

    for (int fi = 0; fi < g.folder_count; fi++)
    {
        ScriptFolder *f = &g.folders[fi];
        for (int si = 0; si < f->count; si++)
        {
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

    for (int _fi = 0; _fi < g.folder_count; _fi++)
        Folder_Free(&g.folders[_fi]);
    g.folder_count = 0;

    if (g.cfg.main_repo_enabled)
    {
        WCHAR pattern[MAX_APPPATH];
        _snwprintf_s(pattern, MAX_APPPATH, _TRUNCATE, L"%s\\*", g.cfg.cache_dir);

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue; /* skip files — only process subdirs */
            if (wcscmp(fd.cFileName, L".") == 0) continue; /* skip current-dir entry */
            if (wcscmp(fd.cFileName, L"..") == 0) continue; /* skip parent-dir entry */
            if (wcscmp(fd.cFileName, L"setup") == 0) continue; /* setup/ holds requirements, not scripts */
            if (g.folder_count >= MAX_FOLDERS) break; /* hard cap — stop adding tabs */

            ScriptFolder *f = &g.folders[g.folder_count++];
            ZeroMemory(f, sizeof(*f));
            wcsncpy_s(f->name, MAX_NAME, fd.cFileName, _TRUNCATE);
            wcsncpy_s(f->display, MAX_NAME, f->name, _TRUNCATE);
            Util_SnakeToTitle(f->display);
            Folder_Alloc(f, 64);

            /* Scan .py files inside this folder */
            WCHAR sub[MAX_APPPATH];
            _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\*.py",
                         g.cfg.cache_dir, f->name);

            WIN32_FIND_DATAW sf;
            HANDLE hSub = FindFirstFileW(sub, &sf);
            if (hSub == INVALID_HANDLE_VALUE) continue;

            do
            {
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
                if (s->sha[0] && GetFileAttributes(s->local) != INVALID_FILE_ATTRIBUTES)
                {
                    WCHAR computed[MAX_SHA] = {0};
                    if (GitHub_ComputeFileSHA1(s->local, computed, MAX_SHA) &&
                        wcscmp(computed, s->sha) != 0)
                    {
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
    for (int i = 0; i < g.cfg.local_dir_count; i++)
    {
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
    RemoveDirectory(dir); /* silently fails if not empty - that's fine */
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
    /* Hold cs_folders for the entire merge — Folder_Push may realloc f->scripts,
       and we modify g.folder_count, both of which the UI thread reads concurrently. */
    EnterCriticalSection(&g.cs_folders);

    /* Find existing folder with same name */
    for (int fi = 0; fi < g.folder_count; fi++)
    {
        if (_wcsicmp(g.folders[fi].name, folder_name) == 0)
        {
            /* Merge: append scripts not already present */
            for (int si = 0; si < count; si++)
            {
                /* Only skip if exact same gh_path (same file from same repo).
               Scripts with the same display name from different sources
               are both added - the user can distinguish them by running them. */
                bool found = false;
                for (int ei = 0; ei < g.folders[fi].count; ei++)
                {
                    if (wcscmp(g.folders[fi].scripts[ei].gh_path,
                               scripts[si].gh_path) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    Script *dst = Folder_Push(&g.folders[fi]);
                    if (dst) *dst = scripts[si];
                }
            }
            goto done;
        }
    }
    /* New folder */
    if (g.folder_count >= MAX_FOLDERS) goto done;
    {
        ScriptFolder *f = &g.folders[g.folder_count++];
        ZeroMemory(f, sizeof(*f));
        wcsncpy_s(f->name, MAX_NAME, folder_name, _TRUNCATE);
        wcsncpy_s(f->display, MAX_NAME, f->name, _TRUNCATE);
        Util_SnakeToTitle(f->display);
        if (!Folder_Alloc(f, count > 0 ? count : 64))
        {
            g.folder_count--;
            goto done;
        }
        for (int i = 0; i < count; i++)
        {
            Script *dst = Folder_Push(f);
            if (dst) *dst = scripts[i];
        }
        f->loaded = true;
    }
done:
    LeaveCriticalSection(&g.cs_folders);
}

/* ================================================================== */
/*  Sync_TreeCachePath  (static)                                       */
/*  Purpose: Builds the path of one cached tree-listing file for a    */
/*           repository branch under %APPDATA%\CatiaMenuWin32\trees.  */
/*           Characters that are unsafe in a file name become '_'.    */
/*  In:  owner, repo, branch — repository identity                     */
/*       ext — file extension including the dot (".json" / ".etag")   */
/*       out — buffer to receive the path                              */
/*       max — capacity of out in WCHARs                               */
/*  Out: (void — out is populated)                                      */
/* ================================================================== */
static void Sync_TreeCachePath(const WCHAR *owner, const WCHAR *repo, const WCHAR *branch,
                               const WCHAR *ext, WCHAR *out, int max)
{
    WCHAR name[MAX_APPPATH];
    _snwprintf_s(name, _countof(name), _TRUNCATE, L"%s_%s_%s", owner, repo, branch);
    for (WCHAR *p = name; *p; p++)
        if (!IsCharAlphaNumericW(*p) && *p != L'-' && *p != L'.') *p = L'_';
    _snwprintf_s(out, max, _TRUNCATE, L"%s\\trees\\%s%s", g.appdata_dir, name, ext);
}

/* ================================================================== */
/*  Sync_FetchTree  (static)                                           */
/*  Purpose: Fetches the complete file listing of a repository branch  */
/*           in one call to the Git Trees API (?recursive=1) and      */
/*           parses it.  Sends the ETag of the previous listing as    */
/*           If-None-Match; a 304 reply (unchanged) re-uses the copy  */
/*           cached on disk, so an unchanged repository downloads no  */
/*           listing at all.                                           */
/*  In:  owner, repo, branch — repository identity                     */
/*       tok     — token, or NULL                                       */
/*       entries — receives a heap array of TreeEntry (caller frees)   */
/*       count   — receives the number of entries                      */
/*       req_sha — receives the blob SHA of setup/requirements.txt, or */
/*                 "" if the repository has none (MAX_SHA WCHARs)      */
/*  Out: true on success; false if GitHub could not be reached or     */
/*       refused, or the listing was truncated (repository too large)  */
/* ================================================================== */
static bool Sync_FetchTree(const WCHAR *owner, const WCHAR *repo, const WCHAR *branch,
                           const WCHAR *tok, TreeEntry **entries, int *count, WCHAR *req_sha)
{
    *entries = NULL;
    *count = 0;
    req_sha[0] = L'\0';

    WCHAR json_path[MAX_APPPATH], etag_path[MAX_APPPATH];
    Sync_TreeCachePath(owner, repo, branch, L".json", json_path, MAX_APPPATH);
    Sync_TreeCachePath(owner, repo, branch, L".etag", etag_path, MAX_APPPATH);

    /* A branch name may contain '/', which must be escaped inside one path segment */
    WCHAR branch_enc[MAX_NAME * 3] = {0};
    for (const WCHAR *b = branch; *b; b++)
    {
        WCHAR one[2] = {*b, L'\0'};
        wcsncat_s(branch_enc, _countof(branch_enc), (*b == L'/') ? L"%2F" : one, _TRUNCATE);
    }
    WCHAR api_path[MAX_APPPATH];
    _snwprintf_s(api_path, MAX_APPPATH, _TRUNCATE, L"/repos/%s/%s/git/trees/%s?recursive=1",
                 owner, repo, branch_enc);

    /* Offer the ETag only while the listing it belongs to is still on disk */
    char etag[128] = {0};
    if (GetFileAttributes(json_path) != INVALID_FILE_ATTRIBUTES)
    {
        DWORD elen = 0;
        char *e = Util_ReadFile(etag_path, &elen);
        if (e)
        {
            strncpy_s(etag, sizeof(etag), e, _TRUNCATE);
            free(e);
        }
    }

    HttpResponse r;
    if (!GitHub_HttpGetEx(GITHUB_API_HOST, api_path, tok, etag, &r)) return false;

    char *json = NULL;
    if (r.status == 304)
    {
        /* Unchanged since the cached listing — parse that copy */
        DWORD jlen = 0;
        json = Util_ReadFile(json_path, &jlen);
        if (!json)
        {
            /* The cached copy vanished after the check above — fetch the listing in full */
            if (!GitHub_HttpGetEx(GITHUB_API_HOST, api_path, tok, NULL, &r) || !r.body) return false;
        }
    }
    if (!json)
    {
        /* Fresh listing: cache it with its ETag for the next conditional request */
        json = r.body;
        Util_WriteFile(json_path, r.body, r.len);
        if (r.etag[0])
            Util_WriteFile(etag_path, r.etag, (DWORD)strlen(r.etag));
        else
            DeleteFile(etag_path); /* no ETag: never send a stale one */
    }

    bool truncated = false;
    int n = GitHub_ParseTree(json, entries, req_sha, &truncated);
    free(json);
    if (n < 0)
    {
        /* Not a tree listing (corrupt cache or unexpected reply) — drop the cache so the next sync refetches */
        Util_Log(L"Sync_FetchTree: unreadable listing for %s/%s", owner, repo);
        DeleteFile(json_path);
        DeleteFile(etag_path);
        return false;
    }
    if (truncated)
    {
        /* GitHub cut the listing short: syncing from it would treat missing files as removed */
        free(*entries);
        *entries = NULL;
        PostStatus(L"Sources: %s/%s is too large to list in one request.", owner, repo);
        return false;
    }
    *count = n;
    return true;
}

/* ================================================================== */
/*  Sync_LocalMatches  (static)                                        */
/*  Purpose: Reports whether a cached file exists and its Git blob     */
/*           SHA equals the SHA from the repository listing — i.e.     */
/*           whether it is current and needs no download.              */
/*  In:  local — cached file path                                      */
/*       sha   — blob SHA from the tree listing                        */
/*  Out: true if the file is present and current                       */
/* ================================================================== */
static bool Sync_LocalMatches(const WCHAR *local, const WCHAR *sha)
{
    if (GetFileAttributes(local) == INVALID_FILE_ATTRIBUTES) return false;
    WCHAR computed[MAX_SHA] = {0};
    return GitHub_ComputeFileSHA1(local, computed, MAX_SHA) && _wcsicmp(computed, sha) == 0;
}

/* ================================================================== */
/*  Sync_FillScript  (static)                                          */
/*  Purpose: Initialises a Script from one tree-listing entry: display */
/*           name (".py" stripped, Title Case), GitHub path, SHA, and  */
/*           cache path.                                                */
/*  In:  s         — script slot to fill (zeroed here)                 */
/*       e         — tree-listing entry                                */
/*       cache_sub — cache directory for the entry's folder            */
/*       src       — where the script comes from                       */
/*  Out: (void — s is populated)                                        */
/* ================================================================== */
static void Sync_FillScript(Script *s, const TreeEntry *e, const WCHAR *cache_sub, ScriptSource src)
{
    ZeroMemory(s, sizeof(*s));
    wcsncpy_s(s->name, MAX_NAME, e->file, _TRUNCATE);
    Util_StripExt(s->name);
    Util_SnakeToTitle(s->name);
    _snwprintf_s(s->gh_path, MAX_APPPATH, _TRUNCATE, L"%s/%s", e->folder, e->file);
    wcsncpy_s(s->sha, MAX_SHA, e->sha, _TRUNCATE);
    _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s", cache_sub, e->file);
    s->source = src;
}

/* ================================================================== */
/*  Sync_BuildMainFolders  (static)                                    */
/*  Purpose: Rebuilds g.folders[] from the main repository's tree      */
/*           listing: one folder per top-level directory that holds   */
/*           .py files, in listing order, each script pointing at its */
/*           cache copy.                                               */
/*  In:  entries — listing from Sync_FetchTree                         */
/*       count   — number of entries                                    */
/*  Out: (void — g.folders[] / g.folder_count populated)               */
/* ================================================================== */
static void Sync_BuildMainFolders(const TreeEntry *entries, int count)
{
    /* Hold cs_folders throughout — Folder_Push may realloc a folder's scripts
       while the UI thread paints, and g.folder_count changes here */
    EnterCriticalSection(&g.cs_folders);
    for (int i = 0; i < count; i++)
    {
        const TreeEntry *e = &entries[i];
        ScriptFolder *f = NULL;
        for (int fi = 0; fi < g.folder_count; fi++)
        {
            if (wcscmp(g.folders[fi].name, e->folder) == 0)
            {
                f = &g.folders[fi];
                break;
            }
        }
        if (!f)
        {
            if (g.folder_count >= MAX_FOLDERS) continue; /* hard cap — no more tabs */
            f = &g.folders[g.folder_count++];
            ZeroMemory(f, sizeof(*f));
            wcsncpy_s(f->name, MAX_NAME, e->folder, _TRUNCATE);
            wcsncpy_s(f->display, MAX_NAME, f->name, _TRUNCATE);
            Util_SnakeToTitle(f->display); /* "Part_Document_Scripts" -> "Part Document Scripts" */
            f->loaded = true;
        }
        Script *s = Folder_Push(f);
        if (!s) continue; /* OOM — skip this script */
        WCHAR cache_sub[MAX_APPPATH];
        _snwprintf_s(cache_sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s", g.cfg.cache_dir, f->name);
        Sync_FillScript(s, e, cache_sub, SCRIPT_SRC_MAIN);
    }
    LeaveCriticalSection(&g.cs_folders);
}

/* ================================================================== */
/*  Sync_PruneRepoCache  (static)                                      */
/*  Purpose: Deletes cached .py files of an extra repository that its  */
/*           latest listing no longer contains, and removes folders   */
/*           left empty.  Only called with a complete listing, so a   */
/*           file missing from it really was removed from the repo.   */
/*  In:  repo_cache — the repository's cache directory                 */
/*       entries    — complete tree listing                            */
/*       count      — number of entries                                */
/*  Out: (void — stale files and empty folders deleted)                */
/* ================================================================== */
static void Sync_PruneRepoCache(const WCHAR *repo_cache, const TreeEntry *entries, int count)
{
    WCHAR pattern[MAX_APPPATH];
    _snwprintf_s(pattern, MAX_APPPATH, _TRUNCATE, L"%s\\*", repo_cache);
    WIN32_FIND_DATAW fd;
    HANDLE hDir = FindFirstFileW(pattern, &fd);
    if (hDir == INVALID_HANDLE_VALUE) return;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR sub[MAX_APPPATH];
        _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\*.py", repo_cache, fd.cFileName);
        WIN32_FIND_DATAW sf;
        HANDLE hFile = FindFirstFileW(sub, &sf);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            do
            {
                bool listed = false;
                for (int i = 0; i < count && !listed; i++)
                    listed = _wcsicmp(entries[i].folder, fd.cFileName) == 0 &&
                             _wcsicmp(entries[i].file, sf.cFileName) == 0;
                if (!listed)
                {
                    WCHAR stale[MAX_APPPATH];
                    _snwprintf_s(stale, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\%s",
                                 repo_cache, fd.cFileName, sf.cFileName);
                    DeleteFileW(stale);
                }
            } while (FindNextFileW(hFile, &sf));
            FindClose(hFile);
        }

        /* Fails harmlessly unless the folder is now empty */
        WCHAR dir[MAX_APPPATH];
        _snwprintf_s(dir, MAX_APPPATH, _TRUNCATE, L"%s\\%s", repo_cache, fd.cFileName);
        RemoveDirectoryW(dir);
    } while (FindNextFileW(hDir, &fd));
    FindClose(hDir);
}

/* ================================================================== */
/*  Sync_ExtraRepo  (static)                                           */
/*  Purpose: Syncs one configured extra GitHub repository from a       */
/*           single tree listing of its configured branch: downloads  */
/*           every .py file whose cached copy is missing or differs   */
/*           from the listed SHA, merges the scripts into g.folders[],*/
/*           deletes cached files the repository no longer has, and   */
/*           refreshes its setup/requirements.txt when that changed.  */
/*  In:  repo — extra repository configuration (url, token, branch)   */
/*  Out: true if the listing and every needed download succeeded       */
/* ================================================================== */
static bool Sync_ExtraRepo(const ExtraRepo *repo)
{
    WCHAR owner[MAX_NAME] = {0}, reponame[MAX_NAME] = {0};
    if (!GitHub_ParseOwnerRepo(repo->url, owner, reponame))
    {
        PostStatus(L"Sources: invalid URL %s", repo->url);
        return false;
    }

    const WCHAR *tok = repo->token[0] ? repo->token
                                      : (g.cfg.github_token[0] ? g.cfg.github_token : NULL);
    const WCHAR *branch = repo->branch[0] ? repo->branch : L"main";

    PostStatus(L"Syncing %s/%s...", owner, reponame);

    TreeEntry *tree = NULL;
    int count = 0;
    WCHAR req_sha[MAX_SHA] = {0};
    if (!Sync_FetchTree(owner, reponame, branch, tok, &tree, &count, req_sha))
    {
        PostStatus(L"Sources: failed to reach %s/%s", owner, reponame);
        return false;
    }

    WCHAR repo_cache[MAX_APPPATH];
    _snwprintf_s(repo_cache, MAX_APPPATH, _TRUNCATE, L"%s\\%s_%s", g.cfg.cache_dir, owner, reponame);

    bool ok = true;
    for (int i = 0; i < count; i++)
    {
        /* Handle each folder once, at its first entry */
        bool seen = false;
        for (int j = 0; j < i && !seen; j++)
            seen = wcscmp(tree[j].folder, tree[i].folder) == 0;
        if (seen) continue;

        int in_folder = 0;
        for (int j = i; j < count; j++)
            if (wcscmp(tree[j].folder, tree[i].folder) == 0) in_folder++;
        Script *scripts = (Script *)calloc((size_t)in_folder, sizeof(Script));
        if (!scripts)
        {
            ok = false;
            continue;
        }

        WCHAR cache_sub[MAX_APPPATH];
        _snwprintf_s(cache_sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s", repo_cache, tree[i].folder);
        SHCreateDirectoryEx(NULL, cache_sub, NULL);

        int n = 0;
        for (int j = i; j < count; j++)
        {
            if (wcscmp(tree[j].folder, tree[i].folder) != 0) continue;
            Script *s = &scripts[n++];
            Sync_FillScript(s, &tree[j], cache_sub, SCRIPT_SRC_EXTRA);

            /* Compare the cached file itself with the listed SHA: the shared
               manifest keys by "Folder/file.py", which can collide with the
               main repository */
            if (!Sync_LocalMatches(s->local, s->sha))
            {
                WCHAR raw_path[MAX_APPPATH];
                _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/%s", owner, reponame, branch, s->gh_path);
                if (!GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, s->local, tok))
                    ok = false;
            }
        }
        Sync_MergeFolder(tree[i].folder, scripts, n);
        free(scripts);
    }

    /* The listing is complete, so cached files it lacks were removed from the repository */
    Sync_PruneRepoCache(repo_cache, tree, count);

    /* setup/requirements.txt — refreshed whenever its SHA changes */
    WCHAR req[MAX_APPPATH];
    _snwprintf_s(req, MAX_APPPATH, _TRUNCATE, L"%s\\setup\\%s_%s\\requirements.txt",
                 g.cfg.cache_dir, owner, reponame);
    if (!req_sha[0])
    {
        DeleteFile(req); /* the repository no longer has one */
    }
    else if (!Sync_LocalMatches(req, req_sha))
    {
        WCHAR raw_path[MAX_APPPATH];
        _snwprintf_s(raw_path, MAX_APPPATH, _TRUNCATE, L"/%s/%s/%s/setup/requirements.txt",
                     owner, reponame, branch);
        if (!GitHub_DownloadRawFull(GITHUB_RAW_HOST, raw_path, req, tok))
            ok = false;
    }

    free(tree);
    return ok;
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

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0) continue;
        if (wcscmp(fd.cFileName, L"..") == 0) continue;
        if (_wcsicmp(fd.cFileName, L"setup") == 0) continue; /* skip setup */

        /* Scan .py files in subfolder */
        WCHAR sub[MAX_APPPATH];
        _snwprintf_s(sub, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\*.py",
                     dir->path, fd.cFileName);

        Script *scripts = (Script *)calloc(MAX_SCRIPTS, sizeof(Script));
        if (!scripts) continue;
        int count = 0;

        WIN32_FIND_DATAW sf;
        HANDLE hs = FindFirstFileW(sub, &sf);
        if (hs == INVALID_HANDLE_VALUE)
        {
            free(scripts);
            continue;
        }
        do
        {
            if (sf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (count >= MAX_SCRIPTS) break;
            Script *s = &scripts[count++];
            ZeroMemory(s, sizeof(*s));
            /* Local scripts have no gh_path or sha */
            _snwprintf_s(s->local, MAX_APPPATH, _TRUNCATE, L"%s\\%s\\%s",
                         dir->path, fd.cFileName, sf.cFileName);
            wcsncpy_s(s->name, MAX_NAME, sf.cFileName, _TRUNCATE);
            Util_StripExt(s->name);
            Util_SnakeToTitle(s->name);
            /* Use local path as gh_path for uniqueness */
            wcsncpy_s(s->gh_path, MAX_APPPATH, s->local, _TRUNCATE); /* local scripts have no GitHub path; use local path as a unique key for prefs */
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
    if (GetFileAttributes(req) != INVALID_FILE_ATTRIBUTES)
    {
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
/*  Sync_IsMainFolder  (static)                                        */
/*  Purpose: Reports whether a pre-sync folder is a main-repo tab —    */
/*           it holds at least one main-repo script.  The synthetic   */
/*           Favourites tab only holds copies and never counts.       */
/*  In:  f — folder to test                                            */
/*  Out: true if f is a main-repo folder                               */
/* ================================================================== */
static bool Sync_IsMainFolder(const ScriptFolder *f)
{
    if (wcscmp(f->name, L"Favourites") == 0) return false;
    for (int si = 0; si < f->count; si++)
        if (f->scripts[si].source == SCRIPT_SRC_MAIN) return true;
    return false;
}

/* ================================================================== */
/*  Sync_FindOldMainFolder  (static)                                   */
/*  Purpose: Finds the pre-sync main-repo folder with the given name.  */
/*  In:  old   — pre-sync folders (may be NULL after an OOM)           */
/*       count — number of entries in old                              */
/*       name  — folder name to look up                                */
/*  Out: the matching folder, or NULL if the folder is new             */
/* ================================================================== */
static const ScriptFolder *Sync_FindOldMainFolder(const ScriptFolder *old, int count,
                                                  const WCHAR *name)
{
    for (int oi = 0; old && oi < count; oi++)
        if (wcscmp(old[oi].name, name) == 0 && Sync_IsMainFolder(&old[oi]))
            return &old[oi];
    return NULL;
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
                             ? g.cfg.github_token
                             : NULL;

    SyncResult *sr = (SyncResult *)calloc(1, sizeof(SyncResult));
    if (!sr)
    {
        /* OOM: post NULL so the main window clears the syncing flag even without a result */
        PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)NULL, 0);
        return 1;
    }

    if (g.cfg.main_repo_enabled)
    {
        /* ── Step 1: Fetch the whole repository listing in one call ── */
        PostStatus(L"Connecting to GitHub\u2026");

        TreeEntry *tree = NULL;
        int tree_count = 0;
        WCHAR req_sha[MAX_SHA] = {0};
        bool main_repo_ok = true;
        if (!Sync_FetchTree(GITHUB_OWNER, GITHUB_REPO, GITHUB_BRANCH, token,
                            &tree, &tree_count, req_sha))
        {
            main_repo_ok = false;
            sr->status = SR_NO_INTERNET;
            if (!g.cfg.offline_use_cache)
            {
                /* User opted out of offline mode — clear folders so the UI shows nothing */
                EnterCriticalSection(&g.cs_folders);
                for (int _fi = 0; _fi < g.folder_count; _fi++)
                    Folder_Free(&g.folders[_fi]);
                g.folder_count = 0;
                LeaveCriticalSection(&g.cs_folders);
                wcsncpy_s(sr->message, 256,
                          L"\u26a0 No internet connection.", _TRUNCATE);
            }
            else if (g.folder_count > 0)
            {
                /* Offline but cache exists — keep showing cached scripts with a warning */
                _snwprintf_s(sr->message, 255, _TRUNCATE,
                             L"\u26a0 Offline \u2013 showing %d cached folder(s). Scripts may be out of date.",
                             g.folder_count);
            }
            else
            {
                wcsncpy_s(sr->message, 256,
                          L"\u26a0 No internet connection. No cached scripts found.", _TRUNCATE);
            }
            /* Do NOT return here — fall through to sync extra repos and local dirs,
               which may be reachable even when the main repo is not. */
        }

        if (main_repo_ok)
        {
            /* Connected ─ move the current folders aside so disabled sources don't linger
               in the rebuild.  They are kept (not freed) until this sync has compared them
               with the new listing: they hold the main-repo folders and scripts known
               before this sync — from the previous sync or, on the first sync after
               startup, from the cache scan — which is what additions and removals are
               measured against.  Hold cs_folders to guard against a concurrent UI-thread
               paint reading the array while it is emptied. */
            ScriptFolder *old_folders = (ScriptFolder *)calloc(MAX_FOLDERS, sizeof(ScriptFolder));
            int old_count = 0;
            EnterCriticalSection(&g.cs_folders);
            for (int _fi = 0; _fi < g.folder_count; _fi++)
            {
                if (old_folders)
                    old_folders[old_count++] = g.folders[_fi]; /* old_folders takes over the scripts buffer */
                else
                    Folder_Free(&g.folders[_fi]); /* OOM — no comparison possible this sync */
                ZeroMemory(&g.folders[_fi], sizeof(ScriptFolder));
            }
            g.folder_count = 0;
            LeaveCriticalSection(&g.cs_folders);

            /* ── Step 2: Build the folders from the listing and detect folder changes ── */
            Sync_BuildMainFolders(tree, tree_count);

            /* Count additions */
            for (int ni = 0; ni < g.folder_count; ni++)
            {
                if (!Sync_FindOldMainFolder(old_folders, old_count, g.folders[ni].name))
                    sr->folders_added++;
            }

            /* Count removals and delete their cached files.  The listing is one complete
               response, so a folder missing from it really was removed. */
            for (int oi = 0; oi < old_count; oi++)
            {
                if (!Sync_IsMainFolder(&old_folders[oi])) continue; /* not a main-repo tab */
                bool found = false;
                for (int ni = 0; ni < g.folder_count; ni++)
                {
                    if (wcscmp(old_folders[oi].name, g.folders[ni].name) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    sr->folders_removed++;
                    /* Delete the folder's cache directory */
                    WCHAR cache_dir[MAX_APPPATH];
                    _snwprintf_s(cache_dir, MAX_APPPATH, _TRUNCATE, L"%s\\%s",
                                 g.cfg.cache_dir, old_folders[oi].name);
                    /* Simple recursive delete via SHFileOperation */
                    WCHAR del_from[MAX_APPPATH + 2];
                    _snwprintf_s(del_from, MAX_APPPATH + 1, _TRUNCATE, L"%s\\", cache_dir);
                    del_from[wcslen(del_from) + 1] = L'\0'; /* SHFileOperation requires double-null terminator after the path */
                    SHFILEOPSTRUCT fo = {
                        .wFunc = FO_DELETE,
                        .pFrom = del_from,
                        .fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT};
                    SHFileOperation(&fo);
                }
            }

            /* ── Step 3: For each folder, detect script changes ──────────── */
            for (int fi = 0; fi < g.folder_count; fi++)
            {
                ScriptFolder *f = &g.folders[fi];

                /* This folder as it was before the sync (NULL if it is new) */
                const ScriptFolder *old_f = Sync_FindOldMainFolder(old_folders, old_count, f->name);

                /* Ensure cache folder exists */
                WCHAR folder_cache[MAX_APPPATH];
                _snwprintf_s(folder_cache, MAX_APPPATH, _TRUNCATE, L"%s\\%s",
                             g.cfg.cache_dir, f->name);
                SHCreateDirectoryEx(NULL, folder_cache, NULL);

                /* Detect newly added scripts */
                for (int si = 0; si < f->count; si++)
                {
                    bool existed = false;
                    for (int oi = 0; old_f && oi < old_f->count; oi++)
                    {
                        if (old_f->scripts[oi].source == SCRIPT_SRC_MAIN &&
                            wcscmp(f->scripts[si].gh_path,
                                   old_f->scripts[oi].gh_path) == 0)
                        {
                            existed = true;
                            break;
                        }
                    }
                    if (!existed) sr->scripts_added++;
                }

                /* Detect removed scripts and delete them from the cache */
                for (int oi = 0; old_f && oi < old_f->count; oi++)
                {
                    if (old_f->scripts[oi].source != SCRIPT_SRC_MAIN) continue; /* extra-repo / local script merged into this tab */
                    bool still_present = false;
                    for (int si = 0; si < f->count; si++)
                    {
                        if (wcscmp(old_f->scripts[oi].gh_path,
                                   f->scripts[si].gh_path) == 0)
                        {
                            still_present = true;
                            break;
                        }
                    }
                    if (!still_present)
                    {
                        sr->scripts_removed++;
                        DeleteLocalScript(old_f->scripts[oi].local);
                    }
                }

                /* ── Step 4: Download changed or missing scripts ─────────── */
                for (int si = 0; si < f->count; si++)
                {
                    Script *s = &f->scripts[si];

                    /* Check stored SHA vs GitHub SHA */
                    WCHAR stored_sha[MAX_SHA] = {0};
                    bool have_local = Sync_GetLocalSHA(s->gh_path, stored_sha);

                    bool sha_changed = (wcscmp(stored_sha, s->sha) != 0);
                    bool file_missing = (GetFileAttributes(s->local) == INVALID_FILE_ATTRIBUTES);

                    /* Download when: no manifest entry, SHA changed on GitHub, or cached file is gone */
                    if (!have_local || sha_changed || file_missing)
                    {
                        WCHAR dl_msg[128];
                        const WCHAR *fname = wcsrchr(s->gh_path, L'/');
                        _snwprintf_s(dl_msg, 127, _TRUNCATE, L"Downloading %s\u2026",
                                     fname ? fname + 1 : s->gh_path);
                        PostStatus(L"%s", dl_msg);

                        if (GitHub_DownloadRaw(s->gh_path, s->local, token))
                        {
                            sr->scripts_updated++;
                        }
                        else
                        {
                            /* Download failed — revert the in-memory SHA to the old manifest value.
                               Saving the old SHA prevents a false tamper warning on the next run;
                               the script will simply retry on the next sync cycle. */
                            wcsncpy_s(s->sha, MAX_SHA, stored_sha, _TRUNCATE);
                            sr->status = SR_PARTIAL;
                        }
                    }
                }
            }

            /* ── Step 5: Refresh setup/requirements.txt when its SHA changed ── */
            if (req_sha[0])
            {
                WCHAR setup_dir[MAX_APPPATH];
                _snwprintf_s(setup_dir, MAX_APPPATH, _TRUNCATE, L"%s\\setup", g.cfg.cache_dir);
                SHCreateDirectoryEx(NULL, setup_dir, NULL);

                WCHAR lpath[MAX_APPPATH];
                _snwprintf_s(lpath, MAX_APPPATH, _TRUNCATE, L"%s\\requirements.txt", setup_dir);
                if (!Sync_LocalMatches(lpath, req_sha))
                    GitHub_DownloadRaw(L"setup/requirements.txt", lpath, token);
            }

            /* Comparison done — release the pre-sync folders */
            for (int oi = 0; oi < old_count; oi++)
                Folder_Free(&old_folders[oi]);
            free(old_folders);

        } /* end if (main_repo_ok) */
        free(tree);
    }
    else
    {
        /* Main repo disabled — clear now so extra repos and local dirs
           rebuild from scratch rather than merging onto stale manifest data. */
        EnterCriticalSection(&g.cs_folders);
        for (int _fi = 0; _fi < g.folder_count; _fi++)
            Folder_Free(&g.folders[_fi]);
        g.folder_count = 0;
        LeaveCriticalSection(&g.cs_folders);
    } /* end if/else (main_repo_enabled) */

    /* ── Step 6: Sync extra GitHub repos ───────────────────────── */
    for (int i = 0; i < g.cfg.extra_repo_count; i++)
    {
        ExtraRepo *xr = &g.cfg.extra_repos[i];
        if (!xr->enabled || !xr->url[0]) continue; /* skip disabled or empty extra repo entries */
        if (!Sync_ExtraRepo(xr) && sr->status == SR_OK)
            sr->status = SR_PARTIAL; /* never masks an offline main repo */
    }

    /* ── Step 7: Scan local dirs ─────────────────────────────────── */
    for (int i = 0; i < g.cfg.local_dir_count; i++)
    {
        if (g.cfg.local_dirs[i].enabled && g.cfg.local_dirs[i].path[0])
            Sync_LocalDir(&g.cfg.local_dirs[i]);
    }

    /* ── Step 8: Persist updated manifest ───────────────────────── */
    Sync_SaveManifest();

    /* ── Step 9: Build human-readable result message ────────────── */
    if (sr->status == SR_OK)
    {
        if (sr->scripts_updated == 0 && sr->folders_added == 0 && sr->folders_removed == 0 && sr->scripts_added == 0 && sr->scripts_removed == 0)
        {
            /* Nothing changed — brief "up to date" message */
            wcsncpy_s(sr->message, 256, L"All scripts are up to date.", _TRUNCATE);
        }
        else
        {
            /* Something changed — show a change summary */
            _snwprintf_s(sr->message, 255, _TRUNCATE, L"Sync complete. "
                                                      L"+%d/-%d folders, "
                                                      L"+%d/-%d scripts, "
                                                      L"%d updated.",
                         sr->folders_added, sr->folders_removed,
                         sr->scripts_added, sr->scripts_removed,
                         sr->scripts_updated);
        }
    }
    else if (sr->status == SR_PARTIAL)
    { /* at least one folder or file failed to download */
        _snwprintf_s(sr->message, 255, _TRUNCATE, L"Sync partial \u2013 some downloads failed. "
                                                  L"%d updated.",
                     sr->scripts_updated);
    }

    PostMessage(g.hwnd, WM_SYNC_DONE, (WPARAM)sr, 0);
    return 0;
}
