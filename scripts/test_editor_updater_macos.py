#!/usr/bin/env python3
"""Exercise the detached macOS replacement helper with a packaged app copy."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "app/editor/resources/update/replace-macos.sh"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    args = parser.parse_args()
    app = args.app.resolve(strict=True)
    assert (app / "Contents/MacOS/D4BoxEditor").is_file()
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("QT_", "QML", "DYLD_"))}
    env["D4BOX_UPDATE_TEST_ONLY"] = "1"
    with tempfile.TemporaryDirectory(prefix="d4box-update-test-") as temporary:
        root = Path(temporary)
        installed = root / "installed/D4BoxEditor.app"
        payload = root / "stage/extracted/D4BoxEditor.app"
        installed.parent.mkdir()
        payload.parent.mkdir(parents=True)
        shutil.copytree(app, installed, symlinks=True)
        shutil.copytree(app, payload, symlinks=True)
        pid = 99999999
        subprocess.run(["/bin/sh", str(HELPER), str(pid), str(installed), str(payload), ""],
                       env=env, check=True, timeout=180)
        backup = Path(str(installed) + f".previous-{pid}")
        assert installed.is_dir() and backup.is_dir()
        subprocess.run([str(installed / "Contents/MacOS/D4BoxEditor"), "--version"],
                       env=env, check=True, timeout=30)
        # A missing payload must fail before moving the previous app.
        missing = root / "stage/missing.app"
        failed = subprocess.run(["/bin/sh", str(HELPER), str(pid + 1),
                                 str(installed), str(missing), ""], env=env, timeout=30)
        assert failed.returncode != 0 and installed.is_dir()
    print("PASS: macOS helper swaps a signed package, starts it, and preserves old app on failure")


if __name__ == "__main__":
    main()
