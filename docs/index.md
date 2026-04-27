---
title: CatiaMenuWin32
description: A native Win32 macro launcher for PyCATIA scripts used with CATIA V5
---

# CatiaMenuWin32

**Stop copy-pasting scripts. Click a button.**

CatiaMenuWin32 is a fast, native Windows application that syncs your PyCATIA scripts directly from GitHub and presents them as one-click buttons — organised by tab, always up to date, always ready to run.

Built for CATIA V5 engineers who want to run automation scripts without touching the CATIA macro editor.

[Download Latest Release](https://github.com/KaiUR/CatiaMenuWin32/releases/latest){: .btn} &nbsp; [View on GitHub](https://github.com/KaiUR/CatiaMenuWin32){: .btn}

---

## What It Does

- **Syncs scripts from GitHub** — connects to [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts) on startup and keeps your local cache up to date
- **One click to run** — every script is a button; click it and Python runs the script immediately
- **No setup per script** — no manual path configuration, no copy-pasting, no CATIA macro editor
- **Works offline** — scripts load from local cache immediately even without an internet connection
- **Add your own sources** — connect additional GitHub repositories or local folders alongside the built-in scripts
- **Secure** — every HTTPS connection validates the server certificate; every script is SHA-verified before running

---

## Documentation

- [User Guide](user-guide) — installation, settings, sources, running scripts, favourites, search, hiding scripts
- [Developer Guide](developer-guide) — building from source, project structure, releasing
- [File Reference](file-reference) — source files, structs, functions, constants
- [Changelog](changelog) — release history
- [Contributing](https://github.com/KaiUR/CatiaMenuWin32/blob/main/CONTRIBUTING.md)

---

## Screenshots

**Dark Mode**
![CatiaMenuWin32 Dark Mode](images/Darkmode_ScreenshotV1.2.3.32.JPG)

**Light Mode**
![CatiaMenuWin32 Light Mode](images/Lightmode_ScreenshotV1.2.3.32.JPG)

---

## Key Features

| Feature | Detail |
|---------|--------|
| Live GitHub sync | Fetches repo structure and compares SHA hashes — only downloads changed files |
| Offline cache | Scripts load from disk immediately on startup |
| Search/filter | Real-time filter bar — find scripts by name or purpose instantly |
| Favourites tab | Star any script; a dedicated Favourites tab appears automatically |
| Script details | Right-click any script to see full header info, add notes, hide, or favourite |
| Multiple sources | Add extra GitHub repos or local folders; same-named folders merge into one tab |
| Security | Certificate pinning + SHA verification on every script before execution |
| Always on Top | Window stays above CATIA — click scripts without alt-tabbing |
| Dark / Light theme | Follows Windows theme or set manually |
| Auto-update | Optionally download and install new versions automatically |

---

## Requirements

- **Windows 10** or later
- **Python 3.9+** — [download from python.org](https://www.python.org/downloads/)
- **PyCATIA** — `pip install pycatia` (or use the built-in **↓ Deps** button)
- **CATIA V5** — must be running for scripts that interact with it

---

## Quick Links

- [Download Latest Release](https://github.com/KaiUR/CatiaMenuWin32/releases/latest)
- [Script Repository](https://github.com/KaiUR/Pycatia_Scripts)
- [PyCATIA Library](https://github.com/evereux/pycatia)
- [Report an Issue](https://github.com/KaiUR/CatiaMenuWin32/issues)

---

**Author:** [Kai-Uwe Rathjen](https://github.com/KaiUR) — MIT License — AI assistance by [Claude](https://anthropic.com)
