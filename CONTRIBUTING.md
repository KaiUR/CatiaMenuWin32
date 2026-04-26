# Contributing to CatiaMenuWin32

Thank you for your interest in contributing. This document explains how to get involved.

## Ways to Contribute

- **Report bugs** — open a [Bug Report](https://github.com/KaiUR/CatiaMenuWin32/issues/new?template=bug_report.md) issue
- **Suggest features** — open a [Feature Request](https://github.com/KaiUR/CatiaMenuWin32/issues/new?template=feature_request.md) issue
- **Submit a pull request** — fix a bug or add a feature (see below)
- **Improve documentation** — corrections and additions to the `docs/` folder are always welcome
- **Add PyCATIA scripts** — contribute scripts to [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts)

---

## Reporting Bugs

Before opening an issue:
1. Check [existing issues](https://github.com/KaiUR/CatiaMenuWin32/issues) to avoid duplicates
2. Make sure you are on the [latest release](https://github.com/KaiUR/CatiaMenuWin32/releases/latest)

When reporting, include:
- App version (shown in the toolbar)
- Windows version
- Python version
- Steps to reproduce
- What you expected vs what happened

---

## Pull Requests

### Setup

```bash
git clone https://github.com/KaiUR/CatiaMenuWin32
cd CatiaMenuWin32
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

See the [Developer Guide](docs/developer-guide.md) for full build instructions.

### Process

1. **Fork** the repository
2. **Create a branch** from `main` with a descriptive name:
   ```bash
   git checkout -b fix/tooltip-clipping
   git checkout -b feat/script-favourites
   ```
3. **Make your changes** — keep them focused on one thing
4. **Test** on both dark and light mode, and with the default 4 tabs
5. **Commit** with a clear message:
   ```
   fix: tooltip no longer clips long descriptions
   feat: add script favourites to toolbar
   ```
6. **Push** and open a pull request against `main`

### Code Style

- C11, Win32 API only — no external libraries, no CRT where avoidable
- Unicode throughout — `WCHAR`, `L""` literals, `_snwprintf`, `wcslen`
- All GDI painting double-buffered — memory DC + `BitBlt` to screen
- All state in the global `AppState g` struct — no additional globals
- Runtime colour functions — use `COL_BG()`, `COL_TEXT()` etc., never hardcode RGB values
- Cross-thread communication via `PostMessage` — never call UI functions from worker threads directly
- See [File Reference](docs/file-reference.md) for a full description of each source file

### What We Won't Accept

- External library dependencies (no Qt, no boost, no curl etc.)
- MSVC-only code — must compile with MinGW-w64 GCC 13+
- Changes that break the dark/light theme system
- Changes that remove existing settings without a migration path

---

## Contributing Scripts

Scripts live in the separate [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts) repository. To contribute a script:

1. Fork `KaiUR/Pycatia_Scripts`
2. Add your `.py` file to the appropriate subfolder (`Any_Document_Scripts/`, `Part_Document_Scripts/` etc.)
3. Include the standard header block — see [Writing Your Own Scripts](docs/user-guide.md#writing-your-own-scripts) for the exact format
4. Add your dependency to `setup/requirements.txt` if needed
5. Open a pull request

### Script Header Requirements

Every script must include a properly formatted header so CatiaMenuWin32 can display its metadata in the tooltip:

```python
\'\'\'
    -----------------------------------------------------------------------------------------------------------------------
    Script name:    Your_Script_Name.py
    Version:        1.0
    Code:           Python3.10.4, Pycatia 0.8.3
    Release:        V5R32
    Purpose:        One line summary — shown on the script button.
    Author:         Your Name
    Date:           DD.MM.YY
    Description:    Full description of what the script does.
                    Continuation lines must be indented.
    dependencies = [
                    "pycatia",
                    ]
    requirements:   Python >= 3.10
                    pycatia
                    Catia V5 running with an open document.
    -----------------------------------------------------------------------------------------------------------------------
\'\'\'
```

---

## Questions

Open a [GitHub Discussion](https://github.com/KaiUR/CatiaMenuWin32/discussions) or an issue if you have questions about the codebase or how to contribute.

---

**Author:** [Kai-Uwe Rathjen](https://github.com/KaiUR)  
**AI Assistance:** Claude (Anthropic)
