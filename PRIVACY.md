# Privacy Policy

**Last updated: 8 May 2026**

CatiaMenuWin32 is a free, open-source desktop application. This policy explains what data the app accesses, stores, and transmits.

---

## Data Collected by the App

CatiaMenuWin32 does **not** collect, transmit, or store any personal data on behalf of the developer. There is no telemetry, no analytics, and no tracking of any kind.

---

## Data Stored Locally

The app writes the following files to your machine under `%APPDATA%\CatiaMenuWin32\` — or, in portable mode (a `settings.ini` next to the exe), in the folder that contains the exe:

| File | Contents |
|------|----------|
| `settings.ini` | App settings — Python path, sync options, theme, window state, optional GitHub token |
| `prefs.ini` | Favourites, hidden scripts, script notes, run counts, the version of each script you last saw (for the new/updated badges), and the Run with Arguments values you last used per script |
| `dep_check.py` | A small helper the app runs with your Python to check which of a script's declared packages are installed; it only reads your local Python installation |
| `manifest.ini` | Cached file list and SHA hashes from the last sync |
| `scripts\` | Cached Python script files downloaded from configured repositories |
| `trees\` | The last repository file listing from GitHub and its ETag, so an unchanged repository is not listed again |
| `venv\` | Optional Python virtual environment, only if you create one in Settings |

All data stays on your machine. None of it is sent to the developer.

---

## Network Requests

The app makes HTTPS requests solely to fetch scripts and check for updates:

| Destination | Purpose |
|-------------|---------|
| `api.github.com` | Fetch script file listings and SHA hashes, check for app updates |
| `raw.githubusercontent.com` | Download script files |
| `github.com` (and the GitHub release-download host it redirects to) | Download an app update, only when you accept or enable automatic updates |
| Additional GitHub repos you configure | Same as above, for extra sources |
| `pypi.org` / `files.pythonhosted.org` (via pip) | Download Python packages — only when you click **↓ Deps** or choose **Install** when a script's dependencies are missing. These requests are made by pip, not by the app itself |

These requests are made directly from your machine to GitHub's servers. GitHub's own [Privacy Policy](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement) applies to those connections — GitHub may log your IP address and request metadata.

No requests are ever made to any server controlled by the developer.

---

## GitHub Personal Access Token

If you choose to configure a GitHub Personal Access Token in Settings, it is stored in plain text in `%APPDATA%\CatiaMenuWin32\settings.ini` on your machine. It is sent only to GitHub — `api.github.com`, `raw.githubusercontent.com`, and `github.com` when downloading an update — as an `Authorization` header. It is never sent anywhere else.

---

## Scripts

Scripts downloaded and run by the app are Python files sourced from GitHub repositories you configure. The app verifies each script's SHA hash against GitHub before running it. The developer is not responsible for the behaviour of third-party scripts you add via extra sources.

---

## Children

CatiaMenuWin32 is a professional engineering tool and is not directed at children.

---

## Changes

If this policy changes, the updated version will be committed to the repository with a new **Last updated** date.

---

## Contact

For questions or concerns, open an issue at [github.com/KaiUR/CatiaMenuWin32/issues](https://github.com/KaiUR/CatiaMenuWin32/issues) or email [kairathjen@yahoo.com](mailto:kairathjen@yahoo.com).
