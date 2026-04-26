# Changelog

All notable changes to CatiaMenuWin32 are documented here.

---

## v1.2.5 — Source management fixes

### Fixed
- Disabling the main repository and pressing Refresh now immediately clears scripts from the UI
- Ghost buttons no longer appear on hover after all sources are disabled
- `g.folder_count` reset at start of sync so disabled sources never persist after refresh
- Main repo correctly wrapped in `if` blocks in both `Sync_LoadManifest` and `Sync_Thread` — previous `goto` approach skipped over variable declarations causing undefined behaviour
- Local build number increments by 2 (CI by 1) so local builds are always ahead of the latest release

---

## v1.2.4 — Stability, documentation, sources improvements

### Added
- Full `docs/` folder — user guide, developer guide, file reference, changelog
- Screenshots in README and user guide
- `CONTRIBUTING.md` and `SECURITY.md` for both repos
- **Reset to Defaults** button in Settings dialog
- PyCATIA and Python 3.9+ requirements documented throughout
- Script header format documented in user guide with full example
- Office/shared network rate limit note added to README
- Local build number increments by 2 so local builds are always ahead of the latest release
- `.github` and all dot-folders skipped in all source types — never appear as tabs

### Fixed
- Update checker `IS_LOCAL_BUILD` guard removed — all builds now check for updates
- Local version detection switched to `git tag --sort=-creatordate` — always picks newest tag
- Update dialog shows `v` prefix consistently for both current and latest version
- Toolbar "Update Deps" button renamed to "Deps" — no longer overlaps app title
- Tab natural width calculation corrected — no ellipsis truncation
- Tab scroll arrows appear correctly when tabs overflow
- Tooltip stays on screen when maximised or on secondary monitor
- Sources dialog `RepoEditDlgProc` null pointer dereference fixed

---

## v1.2.3 — Update checker fix, version info, single instance

### Added
- Single instance enforcement — second launch restores existing window from tray and exits
- `.exe` file properties (version, description, copyright) now populated correctly via `resource.rc.in`
- Dual GitHub links under Help — "View App on GitHub" and "View Scripts on GitHub"
- Update dialog `v` prefix on latest version

### Fixed
- Update checker version comparison — replaced `swscanf` with manual digit parser (fixes MinGW locale issues where `IsNewer` always returned false)
- Original "View on GitHub" link was pointing to wrong repository

---

## v1.2.2 — Certificate validation, SHA verification

### Added
- **Certificate pinning** — every HTTPS request validates server cert subject and issuer; blocks MITM attacks
- **SHA verification** — every script verified against GitHub blob SHA before execution
- `GitHub_ComputeFileSHA1` — computes Git blob SHA1 using Windows CryptAPI
- `GitHub_VerifyScriptSHA` — blocks tampered scripts from running, prompts re-download
- `crypt32` added to link libraries

### Fixed
- Raw content CDN certificate — added `github.io` to allowed subjects and Let's Encrypt to allowed issuers
- `strstr` → `wcsstr` fix for cert subject/issuer checks in Unicode builds (then reverted to `char*` cast as WinINet fills ANSI regardless of UNICODE define)

---

## v1.2.1 — Sources, scrollable tabs, offline cache

### Added
- **Script Sources dialog** (`Menu → File → Sources...`) — add extra GitHub repos and local folders
- Extra GitHub repos sync with same cert validation and SHA verification as main repo
- Local script folders — subfolders become tabs, scripts run from disk
- Same-named folders from different sources merged into one tab
- Enable/Disable toggle for each source
- Removing an extra repo prompts and deletes cached files
- **Update Deps** runs `pip install -r requirements.txt` per source sequentially
- Extra repo requirements cached to unique `cache_dir\setup\owner_reponame\` folders
- **Scrollable tab bar** — natural text width tabs, ◄ ► arrows, mouse wheel scrolling
- Default window width 820px so all 4 default tabs fit
- Tooltip uses `MonitorFromWindow` + `GetMonitorInfo` work area — correct on maximised/multi-monitor
- `setup/` folder never becomes a tab in any source

### Fixed
- Sync no longer clears cached tabs on no-internet response
- `update.bat` removed — dependencies always via `pip install -r` directly

---

## v1.2.0 — Offline cache, dark mode polish, tray improvements

### Added
- **Offline cache** — `Sync_LoadManifest` scans cache on startup so scripts appear immediately without internet
- Scripts load from disk before sync thread completes
- Status bar shows "Showing X cached folders. Connect to internet to sync." when offline with cache

### Fixed
- Sync thread no longer overwrites cached tabs when GitHub API returns no internet
- `Handle_SyncDone` skips tab rebuild on `SR_NO_INTERNET` if cached data exists

---

## v1.1.4 — Dark mode, tooltip, custom tab bar

### Added
- Custom `CMW32TabBar` replaces native `WC_TABCONTROL` — full dark/light mode control
- Tooltip height auto-sized using `DT_CALCRECT` with correct font
- `deps_keep_open` setting — keep Update Deps console open until manually closed
- Dark popup menus

### Fixed
- Tab labels readable in both dark and light mode
- Tooltip description no longer cut off for long text
- Status bar and toolbar buttons correctly themed

---

## v1.1.3 — Settings, autorun, theme

### Added
- Always on Top toggle (`View → Always on Top`)
- Minimize to Tray — hide to system tray on minimize/close
- Start with Windows — registry autorun with `/minimized` flag
- Start Minimized option
- Dark / Light / System theme toggle (`View → Theme`)
- Custom `CMW32StatusBar` — owner-drawn, dark mode aware
- Hamburger `☰ Menu` button replaces native menu bar
- Check for updates on startup setting

---

## v1.1.2 — GitHub token, console options

### Added
- GitHub Personal Access Token support — raises rate limit from 60 to 5000 req/hr
- Show Python console window option
- Keep console open after script finishes (`cmd /k` mode)
- Console exit code shown in status bar when console hidden

---

## v1.1.1 — Script metadata tooltip

### Added
- Script info tooltip — hover `i` badge to see Purpose, Author, Version, Date, Description
- `meta.c` — parses script header block between dashed separators
- `i` badge on right side of each script button
- Tooltip positioned to stay on screen

---

## v1.1.0 — Sync, delta download, manifest

### Added
- Live GitHub sync via API — fetches repo structure on startup
- SHA-based delta sync — only downloads changed files
- `manifest.ini` tracks SHA per script for efficient sync
- `Sync_Thread` background thread — non-blocking UI during sync
- Script buttons organised by folder into tabs
- Status bar sync progress messages

---

## v1.0.0 — Initial release

### Features
- Native Win32/C application — no frameworks, pure C11
- Scripts from `KaiUR/Pycatia_Scripts` presented as clickable buttons
- Python auto-detection
- AppData settings (`settings.ini`)
- Auto-versioning via CMake + `build_number.txt`
- GitHub Actions CI/CD — tag push → automatic release
- Dark mode via `DwmSetWindowAttribute`
- Custom GDI rendering — double-buffered throughout
