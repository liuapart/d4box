#!/usr/bin/env python3
"""Fetch only the pinned official Qt Base SDK into the ignored local runtime.

No Homebrew changes or global installation. The upstream adjacent SHA-1 checks
download integrity over HTTPS; the local report also records SHA-256.
"""

from pathlib import Path
import hashlib
import json
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = "6.11.3"
ARCHIVE = "6.11.3-0-202610220312qtbase-MacOS-MacOS_15-Clang-MacOS-MacOS_15-X86_64-ARM64.7z"
BASE = "https://download.qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_6113/qt6_6113/qt.qt6.6113.clang_64/"


def main() -> None:
    directory = ROOT / ".runtime/editor/qt-official"
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / ARCHIVE
    with urllib.request.urlopen(BASE + ARCHIVE + ".sha1", timeout=45) as response:
        expected = response.read().decode("ascii").split()[0].lower()
    if len(expected) != 40 or any(x not in "0123456789abcdef" for x in expected):
        raise RuntimeError("Invalid upstream checksum")
    if not target.exists() or hashlib.sha1(target.read_bytes()).hexdigest() != expected:
        partial = target.with_suffix(".partial")
        print("Downloading the official Qt Base SDK (local workspace only)...", flush=True)
        with urllib.request.urlopen(BASE + ARCHIVE, timeout=120) as response, partial.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                output.write(chunk)
        if hashlib.sha1(partial.read_bytes()).hexdigest() != expected:
            raise RuntimeError("Qt SDK checksum mismatch; archive was not extracted")
        partial.replace(target)
    # Validate member paths before extraction; the SDK is from the official HTTPS repository.
    listing = subprocess.check_output(["/usr/bin/bsdtar", "-tf", str(target)], text=True)
    for name in listing.splitlines():
        if Path(name).is_absolute() or ".." in Path(name).parts:
            raise RuntimeError(f"Unsafe archive member: {name}")
    prefix = directory / VERSION / "macos"
    prefix.mkdir(parents=True, exist_ok=True)
    subprocess.run(["/usr/bin/bsdtar", "-xf", str(target), "-C", str(prefix)], check=True)
    if not (prefix / "bin/macdeployqt").is_file():
        raise RuntimeError("Unexpected Qt SDK directory structure")
    (prefix / "bin/qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
    manifest = {"qt_version": VERSION, "url": BASE + ARCHIVE, "upstream_sha1": expected,
                "archive_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                "archive_bytes": target.stat().st_size}
    (directory / "download-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Qt prefix: {prefix}")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
