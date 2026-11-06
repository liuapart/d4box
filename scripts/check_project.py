#!/usr/bin/env python3
"""Check repository wiring without executing Windows or game automation."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = "vendor/D4keyHelp"
UPSTREAM_URL = "https://github.com/duzefu/D4keyHelp.git"


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True, stderr=subprocess.PIPE
    ).strip()


def main() -> int:
    required = (
        ".gitmodules",
        "extensions/ahk/Main.ahk",
        "extensions/ahk/WindowDiagnostics.ahk",
        "scripts/start-windows.ps1",
        "docs/architecture.md",
        "docs/windows-validation.md",
        f"{UPSTREAM}/D4keyHelp.ahk",
    )
    missing = [name for name in required if not (ROOT / name).is_file()]
    if missing:
        print("FAIL: missing files: " + ", ".join(missing), file=sys.stderr)
        print("Initialize dependencies: git submodule update --init --recursive", file=sys.stderr)
        return 1

    try:
        url = git("config", "-f", ".gitmodules", "--get", f"submodule.{UPSTREAM}.url")
        if url != UPSTREAM_URL:
            raise ValueError(f"Unexpected upstream URL: {url}")
        entry = git("ls-files", "--stage", "--", UPSTREAM).split()
        if len(entry) != 4 or entry[0] != "160000" or entry[2] != "0":
            raise ValueError("Upstream must be recorded as one Git submodule, not copied files")
        head = git("-C", UPSTREAM, "rev-parse", "HEAD")
        if head != entry[1]:
            raise ValueError("Upstream HEAD differs from the recorded submodule commit")
        if git("-C", UPSTREAM, "status", "--porcelain", "--untracked-files=no"):
            raise ValueError("Upstream tracked files have local modifications")

        for source in (ROOT / "extensions/ahk").glob("*.ahk"):
            for line in source.read_text(encoding="utf-8-sig").splitlines():
                if line.startswith("#Include "):
                    target = source.parent / line.removeprefix("#Include ").strip()
                    if not target.is_file():
                        raise ValueError(f"Missing include in {source.name}: {target.name}")

    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print(f"PASS: upstream submodule pinned to {head}")
    print("PASS: upstream tracked files unchanged; extension files and includes present")
    print("Scope: repository checks only. AHK/PowerShell/game behavior requires Windows validation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
