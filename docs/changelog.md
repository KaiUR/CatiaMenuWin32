# Changelog

All notable changes to CatiaMenuWin32 are documented here.

---

## v1.2.1 — Sources, scrollable tabs, security

### Added
- **Script Sources dialog** (`Menu → File → Sources...`) — manage additional GitHub repositories and local script folders alongside the built-in repo
- Extra GitHub repos sync through the same certificate validation and SHA verification as the main repo; subfolders become tabs, same-named folders merge
- Local script folders treated identically to GitHub repos — subfolders become tabs, scripts run directly from disk
- `setup/` folder is never created as a tab in any source
- **Enable/Disable** toggle button for each repo and local dir in the Sources dialog
- Removing an extra GitHub repo prompts for confirmation and deletes its cached scripts and requirements
- **Update Deps** now runs `pip install -r requirements.txt` separately for each source in order; each waits to complete before the next starts
- Extra repo `requirements.txt` cached to unique `cache_dir\setup\owner_reponame\` folders; local dir requirements run directly from source
- **Scrollable tab bar** — tabs sized to natural text width, left/right arrow buttons, mouse wheel scrolling; no text truncation
- Default window width increased to 820px so all 4 default tabs fit without scroll arrows
- Tooltip positioning uses `MonitorFromWindow` + `GetMonitorInfo` — stays on screen when maximised or on secondary monitors
- **Reset to Defaults** button in Settings dialog
- Documentation folder (`docs/`) with user guide, developer guide, file reference, and changelog

### Fixed
- Update checker now runs for all builds — `IS_LOCAL_BUILD` guard removed
- Local build version detection switched to `git tag --sort=-creatordate` so it always picks the most recently created tag
- Fixed null pointer dereference in `RepoEditDlgProc`
- Fixed embedded null bytes and curly quotes in `Sync_ExtraRepo` string literals
- `update.bat` removed — dependencies always installed via `pip install -r` directly
- Tab text no longer truncated with ellipsis

---

## v1.2.0 — Certificate validation, SHA verification, single instance

### Added
- **Certificate pinning** — every HTTPS request validates server cert subject and issuer; blocks MITM attacks
- **SHA verification** — every script verified against GitHub blob SHA before execution; detects tampered files
- **Single instance enforcement** — second launch restores existing window from tray and exits
- **Executable version info** — `resource.rc.in` replaces `resource.rc`; CMake injects real version, build number, and copyright year into `.exe` properties
- Update dialog shows `v` prefix consistently for both current and latest versions
- Office/shared network rate limit note in README and user guide
- GitHub Token explanation improved

### Fixed
- Version comparison in update checker uses manual digit parser instead of `swscanf` (fixes MinGW locale issues)
- Dual GitHub menu links — "View App on GitHub" and "View Scripts on GitHub"
- Menu bar and toolbar buttons fully dark in dark mode via owner-draw
- `requirements.txt` download during sync fixed (broken escape sequences)
- Sync no longer clears cached tabs on no-internet response

---

## v1.1.4 — Dark mode, tooltip fix, tab improvements

### Added
- Custom `CMW32TabBar` replaces native `WC_TABCONTROL` — full dark/light mode control
- Tooltip auto-sizes to description length using `DT_CALCRECT`
- Dark popup menus via `SetMenuInfo` background brush
- "Keep Update Deps console open" option
- `deps_keep_open` setting

### Fixed
- Tab labels readable in both dark and light mode
- Tooltip description no longer cut off for long descriptions
- Status bar and toolbar buttons correctly themed in dark mode
- `requirements.txt` always downloaded during sync

---

## v1.1.0 — Always on Top, tray, theme, sources

### Added
- Always on Top (default on) — toggle via `View → Always on Top`
- Minimize to Tray — hide to system tray on minimize/close; double-click to restore
- Start with Windows — registry autorun with `/minimized` flag support
- Start Minimized option
- Dark / Light / System theme toggle (`View → Theme`)
- `ThemeMode` enum: `THEME_SYSTEM`, `THEME_DARK`, `THEME_LIGHT`
- Custom `CMW32StatusBar` — fully owner-drawn, dark mode aware
- Hamburger `☰ Menu` button replaces native menu bar
- `Check for app updates on startup` setting
- `Console keep open` sub-option under Show Console

### Fixed
- Always on Top now applied after `ShowWindow` so it reliably sticks
- Theme reapplied on `WM_SHOWWINDOW` when restoring from tray
- `WM_SETTINGCHANGE` updates theme when following system setting

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
- GitHub Personal Access Token support
- Auto-versioning via CMake + `build_number.txt`
- GitHub Actions CI/CD workflow — tag push → automatic release
- Dark mode support via `DwmSetWindowAttribute`
- Custom GDI rendering throughout — double-buffered
