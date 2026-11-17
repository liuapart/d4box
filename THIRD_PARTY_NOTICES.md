# Third-party source notices

## D4KeyHelp

- Project: https://github.com/duzefu/D4keyHelp
- Author attribution: duzefu and D4KeyHelp contributors.
- Imported revision: `e57d2853e26059690398b3504727f35d09fe8a05`.
- Imported on: 2026-11-06.
- Destination: `app/ahk` (application modules and existing tests).
- Upstream declares the MIT license in both README files. The imported revision contains no separate LICENSE file; the original declarations are preserved in `docs/upstream/D4KeyHelp-README.zh.md` and `docs/upstream/D4KeyHelp-README.en.md`.
- MIT license text is retained in `LICENSES/D4KeyHelp-MIT.txt`.

Initial local changes: renamed the application entry to `D4Box.ahk`, changed the window title, integrated d4box window diagnostics, and adjusted the test runner. The imported modules are now maintained directly in this repository.

The archived upstream README files are provenance snapshots. Their relative asset paths refer to the original upstream repository; use the root README for current d4box instructions.

## AutoHotkey runtime in Windows releases

- Official project: https://github.com/AutoHotkey/AutoHotkey
- Runtime version: v2.0.31; copyright AutoHotkey contributors; GPL version 2.
- Corresponding unmodified runtime source: https://github.com/AutoHotkey/AutoHotkey/archive/refs/tags/v2.0.31.zip
- The build downloads the official portable archive with a pinned SHA-256 and compiles against AutoHotkey64.exe. Its original license.txt is included as AutoHotkey-license.txt in the ZIP and embedded in the EXE (extracted to .runtime/licenses on startup).
- Ahk2Exe 1.1.37.02a2 is a build tool from https://github.com/AutoHotkey/Ahk2Exe; its tool binary is not shipped as a separate application asset.

Third-party notices do not imply a new license grant for unrelated project code.
