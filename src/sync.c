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
            L"setup/update.bat",
            NULL
        };
        const WCHAR *local_names[] = {
            L"requirements.txt",
            L"update.bat",
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

    /* ── Step 6: Persist updated manifest ───────────────────────── */
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
