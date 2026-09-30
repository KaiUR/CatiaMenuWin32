---
title: FAQ — CatiaMenuWin32
description: Frequently asked questions about CatiaMenuWin32 — installation, sync errors, Python setup, GitHub tokens, scripts, and security.
---

# FAQ

## Getting Started

**Do I need to install CatiaMenuWin32?**
No. It is a portable `.exe` — download it, place it anywhere on your machine, and double-click to run. No installer, no admin rights required.

**What Windows versions does it support?**
Windows 10 and Windows 11. Older versions are not supported.

**Do I need Python installed?**
Yes. CatiaMenuWin32 launches your scripts — it does not include a Python runtime. Install Python 3.9 or later from [python.org](https://www.python.org/downloads/). The app auto-detects Python on your PATH; if it is not found, open Settings and click Browse next to Python Interpreter.

**Does it work with CATIA V6, 3DEXPERIENCE, or other CAD software?**
The built-in scripts target CATIA V5 via PyCATIA. The app itself is a general-purpose Python script launcher — disable the built-in repository and point it at your own scripts to use it with any software.

---

## Sync and Connectivity

**Why does it say "Connect to internet to sync"?**
Three common causes:
1. No internet connection
2. The GitHub API rate limit has been reached — without a token, GitHub allows 60 requests per hour per IP address. Add a Personal Access Token in Settings to raise this to 5000 per hour (see [GitHub Token](user-guide#github-token))
3. A corporate firewall is blocking `api.github.com` — contact your IT department, or add a token and try again

**What happens if I use it offline?**
Scripts load from the local cache and the app works normally. You can run any script that was synced during a previous session. The status bar notes that sync was skipped.

**Multiple people in my office share the same IP and we keep hitting the rate limit.**
GitHub's 60 requests/hour limit is per public IP, so everyone on the same network shares the quota. Since v3.0.0 each sync uses only **one request per GitHub source** (the whole repository is listed at once, and an unchanged repository is answered with a short "not modified" reply), so the limit is reached far less often. If it still happens, each user should add their own Personal Access Token in Settings → General → GitHub Token → Use token. This gives each user an individual limit of 5000 requests per hour.

**Why did a script disappear from its tab after a sync?**
It was removed or renamed in its GitHub repository. Since v3.0.0 the app also deletes such scripts from the local cache, so they no longer reappear when you are offline.

---

## Running Scripts

**Why do I get "Python Not Found"?**
Open Settings (⚙ button or Menu → File → Settings), click Browse next to Python Interpreter, and locate `python.exe`. It is usually at:
`C:\Users\<YourName>\AppData\Local\Programs\Python\Python3xx\python.exe`

**I clicked a script and nothing happened.**
- Confirm Python is configured correctly (see above)
- Enable **Show Python console window** in Settings → Console — this reveals any error output from the script
- Ensure CATIA V5 is running if the script requires it

**CATIA V5 is running but the script cannot connect to it.**
- Wait for CATIA to fully finish loading before running the script
- Install PyCATIA if you have not already — use the **↓ Deps** button in the toolbar, or open a terminal and run `pip install pycatia`

**What does the "SHA mismatch warning" mean?**
The locally cached copy of the script differs from what GitHub records. Click **Yes** when prompted to re-download the clean version. If it recurs, open Settings, clear the cache folder path, and re-sync.

**A script has disappeared from the list.**
It may have been hidden. Go to **Menu → File → Manage Hidden Scripts** to review and restore hidden scripts. Also check that the filter bar below the toolbar is empty.

---

## Configuration

**Can I use the app without the built-in CATIA scripts?**
Yes. Go to **Menu → File → Sources**, uncheck **Enable built-in repository (KaiUR/Pycatia_Scripts)**, click OK, then click **↺ Refresh**. The built-in scripts are removed from view immediately. Re-check the box and refresh to restore them at any time.

**Can I add a private GitHub repository?**
Yes. When adding the repository in the Sources dialog, paste a Personal Access Token with read access to that repository. The token is stored in `settings.ini` and sent only to GitHub over validated HTTPS.

**How do I add my own scripts?**
Organise your `.py` files into subfolders — each subfolder becomes a tab. Host them in a GitHub repository or a local folder, then add that source in **Menu → File → Sources**. See [Writing Your Own Scripts](user-guide#writing-your-own-scripts) in the User Guide for the script header format.

**Can the app start automatically with Windows?**
Yes. Enable **Start with Windows** in Settings → Window. Combine it with **Start Minimized** to have the app start silently in the system tray.

**How do I find a script quickly without clicking through tabs?**
Press **Ctrl+K** to open the command palette, type part of the script's name (the letters only need to appear in order) and press **Enter**. Ctrl+K works system-wide by default, so you can use it while CATIA has focus; change or disable it in Settings → Command Palette.

**What do the green and blue dots on script buttons mean?**
Green marks a script that is new since you last looked, blue one that has been updated; the tooltip shows its latest change. Running the script or opening its details clears the dot, and **Menu → View → Mark All Scripts as Seen** clears them all. Turn them off in Settings → Window.

**Ctrl+K no longer works in Word / my browser while CatiaMenuWin32 is running.**
The command palette registers Ctrl+K system-wide by default, which takes it from other programs. Untick **Enable system-wide hotkey** or choose another combination in Settings → Command Palette — Ctrl+K keeps working inside CatiaMenuWin32 either way.

**Can I run the app from a USB stick without installing anything into my profile?**
Yes — use portable mode. Put an empty file named `settings.ini` next to `CatiaMenuWin32.exe`; the app then keeps all its settings, cache and data in that folder and never touches the registry. See [Portable mode](user-guide#portable-mode).

**Why does a script ask to install packages before it runs?**
The script lists packages in its header's `dependencies = [...]` block and one of them is missing or the wrong version for the Python the app uses. Choose **Yes** to install them with pip, **No** to run anyway, or **Cancel**. The check can be turned off in Settings → Console. See [Dependency Check](user-guide#dependency-check).

**Can I try a script without adding it to a source?**
Yes — drag the `.py` file onto the main window to run it once. Dragging a folder onto the window offers to add it as a local script source.

**Can I run a script with custom arguments?**
Yes. Right-click any script button and select **Run with Arguments...** to pass command-line arguments before running. Type them as you would after `python script.py`; quotes group words into one argument. The run is logged and can be stopped like any other. Scripts that declare an `Args:` block in their header show a form instead, with a field per parameter and the last values remembered.

---

## Security

**Is the app safe to use?**
Yes. Every HTTPS connection validates the answering server — its host must be a GitHub host, its certificate must be valid for that exact host name, and it must be issued by a CA GitHub uses. Every script is verified against its GitHub blob SHA before execution — a script that has been modified locally or tampered with will be blocked until it is re-downloaded clean. Automatic updates are installed only if the download is Authenticode-signed with the same key as the installed version.

**Why did auto-update open the releases page instead of installing?**
The download could not be verified — it was unsigned, signed by a different key, or its signature was damaged — so it was deleted and nothing was installed. Download the release manually from the page that opened. Local (self-built) copies never auto-install.

**Where does the app store data?**
Everything is in `%APPDATA%\CatiaMenuWin32\` — settings, cached scripts, favourites, and notes — or, in portable mode, in the folder that contains the exe. Nothing is written outside this folder except an optional autorun registry entry when **Start with Windows** is enabled.

**Is my GitHub token stored securely?**
Tokens are stored in plaintext in `%APPDATA%\CatiaMenuWin32\settings.ini` on your local machine. They are transmitted only to GitHub (`api.github.com`, `raw.githubusercontent.com`, and `github.com` for update downloads) over validated HTTPS. For maximum safety, create a fine-grained token scoped to **Public Repositories (read-only)** with no write permissions.

---

Still stuck? Open an issue on [GitHub](https://github.com/KaiUR/CatiaMenuWin32/issues) or check the [Troubleshooting](user-guide#troubleshooting) section of the User Guide.
