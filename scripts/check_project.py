#!/usr/bin/env python3
"""Check local source wiring without executing AutoHotkey or game automation."""

from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "app/ahk"


def check_includes(source: Path) -> int:
    count = 0
    for line in source.read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r'^\s*#Include\s+(.+?)\s*$', line, re.IGNORECASE)
        if not match:
            continue
        name = match.group(1).strip('"').replace("\\", "/")
        target = (source.parent / name).resolve()
        if not target.is_file():
            raise ValueError(f"Missing include in {source.relative_to(ROOT)}: {name}")
        # Check exact casing even on a case-insensitive Mac.
        parent = ROOT
        for part in target.relative_to(ROOT).parts:
            if part not in {item.name for item in parent.iterdir()}:
                raise ValueError(f"Include path casing mismatch: {name}")
            parent = parent / part
        count += 1
    return count


def main() -> int:
    required = (
        "app/ahk/D4Box.ahk",
        "app/ahk/functions/WindowDiagnostics.ahk",
        "app/ahk/tests/SmokeTest.ahk",
        "app/ahk/tests/HealthGlobeTest.ahk",
        "app/ahk/tests/run.sh",
        "scripts/start-windows.ps1",
        "docs/architecture.md",
        "docs/windows-validation.md",
        "THIRD_PARTY_NOTICES.md",
        "LICENSES/D4KeyHelp-MIT.txt",
    )
    try:
        missing = [name for name in required if not (ROOT / name).is_file()]
        if missing:
            raise ValueError("Missing files: " + ", ".join(missing))
        if (ROOT / ".gitmodules").exists():
            raise ValueError("This project should not require Git submodules")
        entries = subprocess.check_output(
            ["git", "-C", str(ROOT), "ls-files", "--stage"], text=True
        ).splitlines()
        if any(entry.startswith("160000 ") for entry in entries):
            raise ValueError("The Git index still contains a submodule")
        sources = sorted(APP.rglob("*.ahk"))
        count = sum(check_includes(source) for source in sources)
        launcher = (ROOT / "scripts/start-windows.ps1").read_text()
        if "app\\ahk\\D4Box.ahk" not in launcher or "SkipUpstream" in launcher:
            raise ValueError("Launcher must start the single local d4box application")
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print("PASS: local application sources and attribution present; no submodules")
    print(f"PASS: {len(sources)} AHK source files, {count} include paths resolved")
    print("Scope: repository checks only. AHK/PowerShell/game behavior requires Windows validation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
