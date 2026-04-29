# Changelog

All notable changes to CatiaMenuWin32 are documented here.

---

## v1.3.3 — In-app help, bounds-checking functions

### Added
- **In-app help window** — `F1` or `Menu → Help → Help Contents` opens a resizable help window
- TreeView table of contents with 11 topics: Getting Started, The Interface, Running Scripts, Settings, Script Sources, Favourites & Search, Script Details & Notes, Sort & Hide Scripts, Update Dependencies, Keyboard Shortcuts, Troubleshooting
- RichEdit panel with formatted topic content
- Single instance — `F1` when already open brings window to front
- Help icon embedded as resource (`res/help_icon.ico`)
- New `src/help.c` added to project

### Fixed
- Tooltips and Script Details blank on first launch on a new machine — meta retries after sync downloads files
- Auto-update download URL fixed — missing `v` prefix caused download failure
- Search filter rebuilds button list — no gaps between filtered results
- All-caps script names (e.g. IGES) now match correctly in search

---

## v1.3.2 — New machine tooltip fix, default settings

### Changed
- Replaced all `_snwprintf` with `_snwprintf_s` using `_TRUNCATE` across all source files
- Replaced `memset` with `ZeroMemory` throughout
- Replaced `memmove` with `memmove_s` in `meta.c`
- Replaced `snprintf` with `_snprintf_s` in `github.c`

### Fixed
- Fixed two static analysis warnings in `meta.c` — dead code and unused variable
- Suppressed unavoidable `GetProcAddress` cast warning with `#pragma GCC diagnostic`

---

## v1.3.2 — New machine tooltip fix, default settings

### Changed
- Default settings changed: **Start with Windows** on, **Minimize to Tray** on, **Start Minimized** off

### Fixed
- Tooltips and Script Details blank on first launch on a new machine — meta retries after sync downloads files
- Auto-update download URL fixed — missing `v` prefix caused download failure

---

## v1.3.1 — Search fix, update prompt fix

### Fixed
- Search/filter now rebuilds button list instead of hiding buttons — no more gaps between results
- All-caps script names (e.g. IGES) now match correctly in search (case-insensitive via `towlower`)
- Local builds no longer trigger the update prompt
- GitHub Pages homepage improved

---

## v1.3.0 — Script management, search, sorting, favourites, details

### Added
- **Search/filter box** — real-time filter bar below toolbar; filters by script name and purpose
- **Favourites tab** — right-click any script → Add to Favourites; tab appears/disappears automatically; persisted across sessions
- **Script details dialog** — right-click → Script Details; shows all header fields, note, favourite/hidden toggles
- **Hide scripts** — right-click → Hide Script; restore via Menu → File → Manage Hidden Scripts
- **Script notes** — right-click → Add/Edit Note; stored in `prefs.ini`
- **Run with Arguments** — right-click → Run with Arguments
- **Script sorting** — Menu → View → Sort Scripts: Default, Alphabetical, By Date, Most Used
- **Most Used sort** — run count tracked per script in `prefs.ini`
- **Keyboard shortcuts** — `Ctrl+Tab` / `Ctrl+Shift+Tab` to cycle through tabs
- **Open Executable Folder** — Menu → File → Open Executable Folder
- **Auto-refresh interval** — background sync every N hours; default 6 hours; configurable in Settings
- **Auto-update** — download and install new versions automatically; falls back to manual if download fails
- **Prefs system** — new `prefs.ini` stores favourites, hidden scripts, notes, run counts
- **Double `.py` fix** — scripts were cached as `scriptname.py.py`; now correctly `scriptname.py`
- `Code`, `Release`, and multi-line `requirements:` fields now correctly parsed and displayed

### Fixed
- `Tabs_BuildFavourites` removes existing tab before rebuilding — no duplicate tabs
- Settings dialog layout — Auto-install updates no longer overlaps Console Options group
- Sources dialog widened so all columns visible
- `cmd.exe /k` quoting fixed in `Runner_RunWithArgs`

---

## v1.2.6 — Update Deps fix, GitHub Pages

### Added
- GitHub Pages site at `https://kaiur.github.io/CatiaMenuWin32`
- `sitemap.xml` submitted to Google Search Console
- `.github` and dot-folders now skipped — never appear as tabs

### Fixed
- Update Deps crash — `cmd.exe /k` requires inner command wrapped in extra quotes when paths contain spaces

---

## v1.2.5 — Disable main repo fix

### Fixed
- Disabling the main repository and pressing Refresh now immediately clears scripts from the UI
- Ghost buttons no longer appear on hover after all sources are disabled
- `g.folder_count` reset at start of sync so disabled sources never persist after refresh

---

## v1.2.4 — Documentation, reset to defaults, single instance

### Added
- Full `docs/` folder — user guide, developer guide, file reference, changelog
- Screenshots in README and user guide
- `CONTRIBUTING.md` and `SECURITY.md` for both repos
- **Reset to Defaults** button in Settings dialog
- Single instance enforcement — second launch restores existing window

### Fixed
- Update checker `IS_LOCAL_BUILD` guard removed — all builds now check for updates
- Local version detection switched to `git tag --sort=-creatordate`
- Update dialog shows `v` prefix consistently
- Toolbar "Update Deps" renamed to "Deps"
- Tooltip positioning fixed for maximised/multi-monitor
- Sources dialog null pointer dereference fixed
- Sync no longer clears cached tabs on no-internet

---

## v1.2.3 — Certificate validation, SHA verification

### Added
- **Certificate pinning** — every HTTPS request validates server cert subject and issuer
- **SHA verification** — every script verified against GitHub blob SHA before execution

### Fixed
- Raw CDN certificate — added `github.io` to allowed subjects, Let's Encrypt to allowed issuers

---

## v1.2.2 — Sources, scrollable tabs

### Added
- **Script Sources dialog** — manage additional GitHub repositories and local script folders
- Extra GitHub repos sync with same cert validation and SHA verification
- Local script folders — subfolders become tabs, scripts run from disk
- **Enable/Disable** toggle for each source
- **Scrollable tab bar** — natural text width, ◄ ► arrows, mouse wheel scrolling
- Default window width 820px
- `setup/` folder never becomes a tab

### Fixed
- Sync no longer clears cached tabs on no-internet
- Tab text no longer truncated with ellipsis

---

## v1.2.1 — Update checker fix, version info

### Added
- `.exe` file properties populated via `resource.rc.in`
- Dual GitHub menu links — View App and View Scripts

### Fixed
- Update checker version comparison — manual digit parser replaces `swscanf`
- `IS_LOCAL_BUILD` guard removed

---

## v1.2.0 — Dark mode, tooltip, custom tab bar

### Added
- Custom `CMW32TabBar` replaces native `WC_TABCONTROL` — full dark/light mode control
- Tooltip auto-sizes to description length
- Dark popup menus
- `deps_keep_open` setting

### Fixed
- Tab labels readable in both dark and light mode
- Tooltip description no longer cut off
- Status bar and toolbar buttons correctly themed

---

## v1.1.0 — Always on Top, tray, theme, autorun

### Added
- Always on Top toggle
- Minimize to Tray — hide to system tray on minimize/close
- Start with Windows — registry autorun
- Start Minimized option
- Dark / Light / System theme toggle
- Custom `CMW32StatusBar` — owner-drawn, dark mode aware
- Hamburger `☰ Menu` button replaces native menu bar
- Check for updates on startup setting
- GitHub PAT support — raises rate limit from 60 to 5000 req/hr
- Show console / Keep console open options

### Fixed
- Always on Top applied correctly after `ShowWindow`
- Theme reapplied on `WM_SHOWWINDOW` when restoring from tray
- Script descriptions no longer cut off

---

## v1.0.0 — Initial release

### Features
- Native Win32/C application — no frameworks
- Live sync from `KaiUR/Pycatia_Scripts` via GitHub API
- Script buttons organised by folder into tabs
- Script metadata tooltip (Purpose, Author, Version, Date, Description)
- SHA-based delta sync — only downloads changed files
- Offline cache — scripts load immediately from disk on startup
- Python auto-detection
- AppData settings (`settings.ini` and `manifest.ini`)
- Auto-versioning via CMake + `build_number.txt`
- GitHub Actions CI/CD — tag push → automatic release
- Dark mode via `DwmSetWindowAttribute`
- Custom GDI rendering — double-buffered throughout
