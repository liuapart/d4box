#!/usr/bin/env python3
"""Collect editor-only native diagnostics after a failed macOS CI job.

This helper never changes the release gate or sends game input. It compares
build-tree and deployed smoke tests under LLDB to capture native crash stacks.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time


def probe(command: list[str], destination: Path, env: dict) -> None:
    print(f"Collecting {destination.name}", flush=True)
    newline = bytes([10])
    try:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                env=env, timeout=75, check=False)
        output = result.stdout + newline + f"Diagnostic command exit: {result.returncode}".encode() + newline
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or b"") + newline + b"Diagnostic command timed out after 75s" + newline
    except OSError as error:
        output = str(error).encode()
    destination.write_bytes(output)
    print(output.decode("utf-8", errors="replace"), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--package-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    clean_env = {k: v for k, v in os.environ.items()
                 if not k.startswith(("QT_", "QML", "DYLD_"))}
    clean_env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    for label, root in (("build", args.build_dir), ("package", args.package_dir)):
        executable = root.resolve() / "D4BoxEditor.app/Contents/MacOS/D4BoxEditor"
        if not executable.is_file():
            continue
        probe(["/usr/bin/xcrun", "lldb", "--batch", "--no-lldbinit",
               "-o", "settings set target.disable-aslr false",
               "-o", "run", "-k", "thread backtrace all", "-k", "image list",
               "--", str(executable), "--smoke-test"],
              args.output_dir / f"{label}-native-smoke.txt", clean_env)
    ctest_log = args.build_dir / "Testing/Temporary/LastTest.log"
    if ctest_log.is_file():
        shutil.copy2(ctest_log, args.output_dir / "CTest-LastTest.log")
    # Never collect unrelated crash reports or developer screenshots.
    if os.environ.get("GITHUB_ACTIONS") == "true":
        reports = Path.home() / "Library/Logs/DiagnosticReports"
        recent = sorted((p for p in reports.glob("D4BoxEditor*")
                         if p.is_file() and p.stat().st_mtime > time.time() - 900),
                        key=lambda p: p.stat().st_mtime, reverse=True)
        for report in recent[:5]:
            shutil.copy2(report, args.output_dir / report.name)
            print(f"Saved native crash report: {report.name}", flush=True)


if __name__ == "__main__":
    main()
