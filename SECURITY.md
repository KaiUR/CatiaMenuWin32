# Security Policy

## Supported Versions

Only the latest release of CatiaMenuWin32 receives security updates.

| Version | Supported |
|---------|-----------|
| Latest release | ✅ |
| Older releases | ❌ |

Always update to the [latest release](https://github.com/KaiUR/CatiaMenuWin32/releases/latest) to ensure you have all security fixes.

---

## Security Features

CatiaMenuWin32 is designed with security in mind:

- **Certificate validation** — every HTTPS connection to `api.github.com` and `raw.githubusercontent.com` validates the server certificate subject and issuer before any data is read
- **SHA verification** — every script is verified against its GitHub blob SHA before execution, detecting any tampering with cached files
- **No telemetry** — the app makes no connections other than to `api.github.com` and `raw.githubusercontent.com`, and only when syncing scripts or checking for updates
- **GitHub token stored locally** — tokens are stored in `%APPDATA%\CatiaMenuWin32\settings.ini` and never transmitted anywhere except the configured GitHub API hosts

---

## Known Limitations

### Local manifest trust window

SHA verification at script runtime uses `s->sha` which is populated from the GitHub API during sync. At startup, before the first sync completes, `s->sha` is loaded from the local `manifest.ini` file stored in `%APPDATA%\CatiaMenuWin32\`. This file is writable by the current Windows user.

A local attacker who already has access to the user account could theoretically modify `manifest.ini` to replace a stored SHA with that of a malicious script, causing verification to pass before the first sync overwrites the value with the real GitHub SHA.

**Impact is limited** — an attacker with write access to `%APPDATA%` already has full access to the user account and can run arbitrary code without involving this app. Once a sync completes, all in-memory SHAs are overwritten with values from the GitHub API and the manifest is no longer trusted for verification.

---



**Please do not report security vulnerabilities as public GitHub issues.**

If you discover a security vulnerability, please report it privately so it can be addressed before it is publicly disclosed.

**To report a vulnerability:**

1. Go to the [Security Advisories](https://github.com/KaiUR/CatiaMenuWin32/security/advisories) page
2. Click **Report a vulnerability**
3. Provide a clear description of the issue, steps to reproduce, and the potential impact

Alternatively, contact the maintainer directly via GitHub: [@KaiUR](https://github.com/KaiUR)

---

## Response Timeline

- **Acknowledgement** — within 5 business days
- **Assessment** — within 10 business days
- **Fix and release** — as soon as possible depending on severity

---

## Scope

The following are considered in scope for security reports:

- Certificate validation bypass
- SHA verification bypass allowing execution of tampered scripts
- Unauthorised network connections
- Local privilege escalation via the app
- Token/credential exposure

The following are out of scope:

- Vulnerabilities in PyCATIA scripts themselves (report to [KaiUR/Pycatia_Scripts](https://github.com/KaiUR/Pycatia_Scripts))
- Vulnerabilities in Python or PyCATIA (report to their respective projects)
- Social engineering attacks
- GitHub API rate limiting

---

**Maintainer:** [Kai-Uwe Rathjen](https://github.com/KaiUR)
