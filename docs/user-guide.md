# User Guide

## Contents
- [Installation](#installation)
- [First Launch](#first-launch)
- [The Interface](#the-interface)
- [Running Scripts](#running-scripts)
- [Settings](#settings)
- [Script Sources](#script-sources)
- [Update Dependencies](#update-dependencies)
- [System Tray](#system-tray)
- [Themes](#themes)
- [GitHub Token](#github-token)
- [Troubleshooting](#troubleshooting)

---

## Installation

1. Download the latest `CatiaMenuWin32.exe` from the [releases page](https://github.com/KaiUR/CatiaMenuWin32/releases/latest)
2. Place the `.exe` anywhere on your machine — no installer required
3. Double-click to run

**Requirements:**
- Windows 10 or later
- **Python 3.9+** — install from [python.org](https://www.python.org/downloads/)
- **[PyCATIA](https://github.com/evereux/pycatia)** — install via `pip install pycatia` (or use **↓ Deps** in the app)
- **CATIA V5** — must be running for scripts that interact with it

---

## First Launch

On first launch the app will:

1. Create `%APPDATA%\CatiaMenuWin32\` to store settings and cached scripts
2. Attempt to auto-detect your Python installation
3. Sync scripts from the built-in `KaiUR/Pycatia_Scripts` repository
4. Display the scripts as clickable buttons organised by tab

If the sync fails with "Connect to internet to sync", check your internet connection. If you are on a corporate network see the [GitHub Token](#github-token) section.

---

## The Interface

```
┌─ Menu ──┬─ Refresh ──┬─ Settings ──┬─ Deps ──┐   CATIA Macro Menu  v1.2.0.31
│         │            │             │         │
├─ Any Document Scripts ──┬─ Part Document Scripts ──┬─ ...
│                         │                          │
│  ► Hide Planes And Axis Systems                [i] │
│  ► Rename Hybrid Shapes                        [i] │
│  ► Replace Name Hybrid Shapes                  [i] │
│                                                    │
└────────────────────────────────────────────────────┘
 Status bar — sync messages and script output
```

**Dark Mode**
![CatiaMenuWin32 Dark Mode](images/Darkmode_ScreenshotV1.2.3.32.JPG)

**Light Mode**
![CatiaMenuWin32 Light Mode](images/Lightmode_ScreenshotV1.2.3.32.JPG)

**Toolbar buttons:**
- **☰ Menu** — access all app functions via dropdown menus
- **↺ Refresh** — re-sync scripts from all sources
- **⚙ Settings** — open the settings dialog
- **↓ Deps** — install/update Python dependencies

**Tab bar:**
- Each tab corresponds to a script folder
- Click a tab to switch between script categories
- When there are more tabs than fit, ◄ ► arrows appear — click or use the **mouse wheel** to scroll

**Script buttons:**
- Click the main area to run the script
- Click the **i** badge on the right to see script information (purpose, author, version, description)

---

## Running Scripts

Click any script button to run it. The app will:

1. Verify the script's SHA hash against GitHub to confirm it hasn't been tampered with
2. Launch Python with the script path
3. Show "Script launched in console" or the exit code in the status bar

**Console options** (configurable in Settings):
- **Show console** — opens a visible Python console window when the script runs
- **Keep console open** — keeps the console window open after the script finishes so you can read output and errors (`cmd /k` mode)

Without **Show console**, scripts run silently in the background.

---

## Settings

Open via **☰ Menu → File → Settings...** or the **⚙ Settings** toolbar button.

### Python Interpreter
Set the full path to your `python.exe`. Click **Browse...** to locate it. If left blank the app auto-detects Python from your PATH and common installation locations.

### Script Cache Folder
Where downloaded scripts are stored locally. Defaults to `%APPDATA%\CatiaMenuWin32\scripts`. Click **Browse...** to change.

### Sync & Updates

| Option | Default | Description |
|--------|---------|-------------|
| Sync scripts automatically on startup | On | Downloads latest scripts when the app starts |
| Always download latest before running | Off | Re-downloads the script every time before running |
| Check for app updates on startup | On | Notifies you when a newer version is available |

### Console Options

| Option | Default | Description |
|--------|---------|-------------|
| Show Python console window | Off | Opens a visible console when running scripts |
| Keep console open after script finishes | On | Window stays open so you can read output/errors |
| Keep Update Deps console open | Off | Keeps the dependency install window open |

### GitHub Token
Optional Personal Access Token to raise the API rate limit from 60 to 5000 requests/hour. See [GitHub Token](#github-token).

### Reset to Defaults
The **Reset to Defaults** button at the bottom left resets all settings to their original values. Your script sources (extra repos and local folders) are not affected.

---

## Script Sources

Open via **☰ Menu → File → Sources...**

CatiaMenuWin32 can load scripts from three types of sources simultaneously:

### Built-in Repository
The `KaiUR/Pycatia_Scripts` repository is always the primary source. Use the checkbox at the top of the Sources dialog to enable or disable it.

### Additional GitHub Repositories
Add any GitHub repository that uses the same folder structure (subfolders contain `.py` files):

1. Click **Add...** under "Additional GitHub Repositories"
2. Enter the full URL: `https://github.com/owner/repo`
3. Enter the branch name (defaults to `main`)
4. Optionally add a token for private repos or higher rate limits
5. Click **OK**

If two repositories have a folder with the same name, their scripts are merged into one tab.

To **enable or disable** a repo without removing it, select it and click **Enable/Disable**.

To **remove** a repo, select it and click **Remove** — you will be asked to confirm and given the option to delete its cached files.

### Local Script Folders
Add a folder on your machine that contains subfolders with `.py` files:

```
My_Scripts/
├── Any_Document_Scripts/
│   └── my_script.py
└── Part_Document_Scripts/
    └── another_script.py
```

1. Click **Add...** under "Local Script Folders"
2. Browse to your folder and click OK

Local scripts run directly from disk — no downloading or SHA checking. A `setup/` subfolder is never shown as a tab — it is used for dependencies only.

### Update Dependencies for Sources
If a source has a `setup/requirements.txt` file, clicking **↓ Deps** will run `pip install -r requirements.txt` for each source separately in order:
1. Main repo requirements
2. Each extra GitHub repo's requirements
3. Each local folder's requirements

---

## Update Dependencies

Click **↓ Deps** (or **☰ Menu → Run → Update Dependencies**) to install Python packages required by the scripts.

The app runs `pip install -r requirements.txt` for each configured source that has a `setup/requirements.txt` file. Each source runs in its own console window sequentially.

Enable **Keep Update Deps console open** in Settings to keep each window visible until you close it manually.

---

## System Tray

Enable **Minimize to Tray** in **☰ Menu → Window** to hide the window to the system tray instead of the taskbar when minimised or closed.

- **Double-click** the tray icon to restore the window
- **Right-click** the tray icon for a quick menu

Enable **Start with Windows** to launch the app automatically at login. Combine with **Start Minimized** to have it start silently in the tray.

---

## Themes

Switch between dark, light, and system-default themes via **☰ Menu → View → Theme**:

- **System (default)** — follows your Windows theme setting automatically
- **Dark** — always dark regardless of Windows setting
- **Light** — always light regardless of Windows setting

---

## GitHub Token

The app uses the GitHub REST API to fetch script lists. Without a token, GitHub allows **60 requests per hour per IP address**.

A token raises this to **5000 requests per hour** and is required for private repositories.

**To create a token:**
1. Go to GitHub → Settings → Developer settings → Personal access tokens → Fine-grained tokens
2. Click **Generate new token**
3. Name it `CatiaMenuWin32`
4. Set expiry as preferred
5. Under Repository access select **Public Repositories (read-only)**
6. Click **Generate token** and copy it

**To add it to the app:**
1. Open **☰ Menu → File → Settings...**
2. Tick **Use token**
3. Paste the token
4. Click **OK**

> **Office / shared network users:** If multiple people share the same public IP address, all their requests count against the same 60/hour limit. Each user should set their own token to avoid seeing "Connect to internet to sync" errors.

---

## Writing Your Own Scripts

CatiaMenuWin32 reads metadata from a structured header block at the top of each `.py` file. This header powers the tooltip shown when you hover over the **i** badge on a script button.

### Header Format

```python
'''
    -----------------------------------------------------------------------------------------------------------------------
    Script name:    My_Script_Name.py
    Version:        1.0
    Code:           Python3.10.4, Pycatia 0.8.3
    Release:        V5R32
    Purpose:        One line summary shown under the script name in the button.
    Author:         Your Name
    Date:           DD.MM.YY
    Description:    Full description of what the script does. This is shown in the
                    tooltip popup. Continuation lines must be indented.
    dependencies = [
                    "pycatia",
                    ]
    requirements:   Python >= 3.10
                    pycatia
                    Catia V5 running with an open document.
    -----------------------------------------------------------------------------------------------------------------------

    Change:

    -----------------------------------------------------------------------------------------------------------------------
'''
```

### Rules
- The header must be inside a triple-quoted string `'''...'''` or `"""..."""` at the top of the file
- The dashed separator lines (`-----...`) mark the start and end of the header block
- Keys are matched case-insensitively: `Script name:`, `Purpose:`, `Author:`, `Date:`, `Version:`, `Description:`
- **Purpose** — shown as the subtitle line on the script button (keep it short, one line)
- **Description** — shown in the tooltip; continuation lines must be indented with spaces or tabs
- Parsing stops at the second dashed separator line, or at `dependencies`, `requirements`, `import`, `def`, or `class`
- If no metadata is found the script still appears as a button — just without tooltip details

### Folder Structure

Scripts must be organised in subfolders — the subfolder name becomes the tab name:

```
Your_Repo/
├── Any_Document_Scripts/
│   └── My_Script.py
├── Part_Document_Scripts/
│   └── Another_Script.py
└── setup/
    └── requirements.txt
```

Folder names use snake_case — the app converts them to title case automatically:
`Any_Document_Scripts` → `Any Document Scripts`

### PyCATIA

Scripts in `KaiUR/Pycatia_Scripts` use the [PyCATIA](https://github.com/evereux/pycatia) library by evereux for automating CATIA V5 via COM. To write your own scripts:

1. Install PyCATIA: `pip install pycatia`
2. See the [PyCATIA documentation](https://pycatia.readthedocs.io/) for the full API reference
3. CATIA V5 must be running before scripts that interact with it are executed

### Setup Folder

If your repository or local folder contains a `setup/` subfolder with a `requirements.txt`, clicking **↓ Deps** will automatically install those dependencies:

```
Your_Repo/
└── setup/
    └── requirements.txt
```

The `setup/` folder is never shown as a tab.

---

## Troubleshooting

### "Connect to internet to sync"
- Check your internet connection
- You may have hit the GitHub API rate limit — set a token (see [GitHub Token](#github-token))
- Corporate firewalls may block `api.github.com` — contact your IT department

### Script fails with "Python Not Found"
- Open **☰ Menu → File → Settings...**
- Click **Browse...** next to Python Interpreter and locate `python.exe`

### SHA mismatch warning
- The local cached script differs from what GitHub expects
- Click **Yes** when prompted to re-download the script
- If it persists, delete the cache folder in Settings and re-sync

### Update prompt on local builds
- Local builds pick up the latest release tag via git
- Pull the latest tags with `git fetch --tags` and rebuild
- The local build number will be one higher than the release and no prompt will appear

### Scripts don't appear after adding a source
- Click **↺ Refresh** to trigger a sync with the new source
- Check the Sources dialog to confirm the source is enabled
