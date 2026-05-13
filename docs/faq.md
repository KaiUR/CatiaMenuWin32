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
GitHub's 60 requests/hour limit is per public IP, so everyone on the same network shares the quota. Each user should add their own Personal Access Token in Settings → General → GitHub Token → Use token. This gives each user an individual limit of 5000 requests per hour.

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

**Can I run a script with custom arguments?**
Yes. Right-click any script button and select **Run with Arguments...** to pass command-line arguments before running.

---

## Security

**Is the app safe to use?**
Yes. Every HTTPS connection validates the server certificate (subject and issuer). Every script is verified against its GitHub blob SHA before execution — a script that has been modified locally or tampered with will be blocked until it is re-downloaded clean.

**Where does the app store data?**
Everything is in `%APPDATA%\CatiaMenuWin32\` — settings, cached scripts, favourites, and notes. Nothing is written outside this folder except an optional autorun registry entry when **Start with Windows** is enabled.

**Is my GitHub token stored securely?**
Tokens are stored in plaintext in `%APPDATA%\CatiaMenuWin32\settings.ini` on your local machine. They are transmitted only to `api.github.com` and `raw.githubusercontent.com` over validated HTTPS. For maximum safety, create a fine-grained token scoped to **Public Repositories (read-only)** with no write permissions.

---

Still stuck? Open an issue on [GitHub](https://github.com/KaiUR/CatiaMenuWin32/issues) or check the [Troubleshooting](user-guide#troubleshooting) section of the User Guide.
