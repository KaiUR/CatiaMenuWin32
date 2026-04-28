# Changelog

All notable changes to CatiaMenuWin32 are documented here.

---

## v1.3.2 — New machine tooltip fix, default settings, in-app help

### Added
- **In-app help window** — `F1` or `Menu → Help → Help Contents` opens a resizable help window with TreeView contents and RichEdit topic display
- 11 topics: Getting Started, The Interface, Running Scripts, Settings, Script Sources, Favourites & Search, Script Details & Notes, Sort & Hide Scripts, Update Dependencies, Keyboard Shortcuts, Troubleshooting
- Single instance — `F1` when already open brings window to front
- New `src/help.c` added to project

### Changed
- Default settings changed: **Start with Windows** on, **Minimize to Tray** on, **Start Minimized** off

### Fixed
- Tooltips and Script Details blank on first launch on a new machine — meta retries after sync downloads files
- Auto-update download URL fixed — missing `v` prefix caused download failure
- Search filter rebuilds button list — no gaps between filtered results
- All-caps script names (e.g. IGES) now match correctly in search

---

## v1.3.1 — Search fix, update prompt fix

### Fixed
- Search/filter now rebuilds button list instead of hiding buttons — no more gaps between results
- IGES and other all-caps script names now match correctly in search (case-insensitive via `towlower`)
- Local builds no longer trigger the update prompt — `git fetch --tags` + `git pull` picks up latest tag correctly

---

## v1.3.0 — Script management, search, sorting, favourites, details

### Added
- **Search/filter box** — real-time filter bar below toolbar; filters by script name and purpose
- **Favourites tab** — right-click any script → Add to Favourites; a ⭐ Favourites tab appears automatically; disappears when no favourites remain; persisted across sessions
- **Script details dialog** — right-click any script → Script Details; shows all header fields: Name, Purpose, Author, Version, Date, Code, Release, Description, Requirements, local path, note, favourite and hidden toggles
- **Hide scripts** — right-click → Hide Script removes it from view; hidden scripts persist across syncs; restore via Menu → File → Manage Hidden Scripts
- **Script notes** — right-click → Add/Edit Note; per-script user notes stored in `prefs.ini`
- **Run with Arguments** — right-click → Run with Arguments; pass custom command line arguments to any script
- **Script sorting** — Menu → View → Sort Scripts: Default, Alphabetical, By Date, Most Used
- **Most Used sort** — run count tracked per script in `prefs.ini`; scripts sorted by run count descending
- **Keyboard shortcuts** — `Ctrl+Tab` / `Ctrl+Shift+Tab` to cycle through tabs
- **Open Executable Folder** — Menu → File → Open Executable Folder; opens the folder containing the `.exe` in Explorer
- **Auto-refresh interval** — sync automatically every N hours in the background; default 6 hours; set to 0 to disable; configurable in Settings
- **Auto-update** — when a newer version is detected, option to download and install automatically; falls back to manual if download fails
- **Prefs system** — new `prefs.ini` in AppData stores favourites, hidden scripts, notes, run counts
- **Double `.py` fix** — scripts were being cached as `scriptname.py.py`; now correctly cached as `scriptname.py`
- **Requirements field** — script details now shows full Requirements section from script header, preserving line breaks
- **Code and Release fields** — now correctly parsed and displayed in script details

### Fixed
- Double `.py` extension bug in both `GitHub_ParseFolder` and `Sync_ExtraRepo`
- `meta.c` now correctly parses `Code:`, `Release:`, and `requirements:` (multi-line) fields
- `requirements:` continuation lines preserved with correct line breaks in details dialog
- `Tabs_BuildFavourites` now removes existing Favourites tab before rebuilding — no duplicate tabs
- Settings dialog layout — Auto-install updates no longer overlaps Console Options group
- Sources dialog widened so all columns visible
- `cmd.exe /k` quoting fixed in `Runner_RunWithArgs`
- `Updater_AutoUpdate` string literal and quoting issues fixed

---

## v1.2.6 — Update Deps fix, GitHub Pages, Search Console

### Added
- GitHub Pages site at `https://kaiur.github.io/CatiaMenuWin32`
- `sitemap.xml` submitted to Google Search Console for indexing

### Fixed
- Update Deps crash — `cmd.exe /k` requires inner command wrapped in extra quotes when paths contain spaces

---

## v1.2.5 — Source management fixes

### Fixed
- Disabling main repository and pressing Refresh now immediately clears scripts from the UI
- Ghost buttons no longer appear on hover after all sources are disabled
- `g.folder_count` reset at start of sync so disabled sources never persist after refresh
- Main repo correctly wrapped in `if` blocks in both `Sync_LoadManifest` and `Sync_Thread`
- Local build number increments by 2 so local builds are always ahead of the latest release

---

## v1.2.4 — Stability, documentation, sources improvements

### Added
- Full `docs/` folder — user guide, developer guide, file reference, changelog
- Screenshots in README and user guide
- `CONTRIBUTING.md` and `SECURITY.md` for both repos
- **Reset to Defaults** button in Settings dialog
- PyCATIA and Python 3.9+ requirements documented throughout
- Script header format documented in user guide
- Local build number increments by 2

### Fixed
- Update checker `IS_LOCAL_BUILD` guard removed
- Local version detection switched to `git tag --sort=-creatordate`
- Update dialog shows `v` prefix consistently
- Toolbar "Update Deps" renamed to "Deps"
- Tab natural width — no ellipsis truncation
- Tab scroll arrows correct
- Tooltip positioning fixed for maximised/multi-monitor
- Sources dialog null pointer dereference fixed
- Sync no longer clears cached tabs on no-internet

---

## v1.2.3 — Update checker fix, version info, single instance

### Added
- Single instance enforcement
- `.exe` file properties populated via `resource.rc.in`
- Dual GitHub links under Help

### Fixed
- Update checker version comparison — manual digit parser replaces `swscanf`
- Original GitHub link pointed to wrong repo

---

## v1.2.2 — Certificate validation, SHA verification

### Added
- Certificate pinning — validates server cert subject and issuer on every HTTPS request
- SHA verification — every script verified against GitHub blob SHA before execution

### Fixed
- Raw CDN certificate — added `github.io` to allowed subjects, Let's Encrypt to allowed issuers

---

## v1.2.1 — Sources, scrollable tabs, offline cache

### Added
- Script Sources dialog — extra GitHub repos and local folders
- Scrollable tab bar — natural text width, ◄ ► arrows, mouse wheel
- Default window width 820px
- `setup/` and dot-folders never become tabs
- Update Deps runs per source sequentially

### Fixed
- Sync no longer clears cached tabs on no-internet
- `update.bat` removed — always use pip directly

---

## v1.2.0 — Offline cache

### Added
- `Sync_LoadManifest` — scripts load from disk immediately on startup

### Fixed
- Handle_SyncDone skips tab rebuild on no-internet if cached data exists

---

## v1.1.4 — Dark mode, tooltip, custom tab bar

### Added
- Custom `CMW32TabBar` replaces native tab control
- Tooltip height auto-sized
- `deps_keep_open` setting

---

## v1.1.3 — Settings, autorun, theme

### Added
- Always on Top, Minimize to Tray, Start with Windows
- Dark / Light / System theme toggle
- Custom `CMW32StatusBar`
- Hamburger menu

---

## v1.1.2 — GitHub token, console options

### Added
- GitHub PAT support
- Show console / Keep console open options

---

## v1.1.1 — Script metadata tooltip

### Added
- Script info tooltip with Purpose, Author, Version, Date, Description
- `i` badge on script buttons

---

## v1.1.0 — Sync, delta download, manifest

### Added
- Live GitHub sync via API
- SHA-based delta sync
- `manifest.ini` for efficient sync

---

## v1.0.0 — Initial release

### Features
- Native Win32/C, no frameworks
- Scripts from `KaiUR/Pycatia_Scripts` as clickable buttons
- Python auto-detection
- AppData settings
- Auto-versioning
- GitHub Actions CI/CD
- Dark mode
