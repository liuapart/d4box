#!/usr/bin/env python3
"""Exercise the built editor CLI without modifying user files or sending OS input."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("QT_", "QML", "DYLD_"))}
    count = 0
    with tempfile.TemporaryDirectory(prefix="d4box-cli-") as temporary:
        root = Path(temporary)
        profiles = root / "中文 configs"
        profiles.mkdir()
        broken = profiles / "broken.d4workflow.json"
        broken.write_text("{broken", encoding="utf-8")

        def check(name: str, arguments: list, expected: int, extra_env: dict | None = None) -> subprocess.CompletedProcess:
            nonlocal count
            result = subprocess.run([str(executable), *map(str, arguments)],
                                    env={**env, **(extra_env or {})}, capture_output=True, text=True, timeout=25,
                                    encoding="utf-8", errors="replace")
            if result.returncode != expected:
                raise AssertionError(f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}")
            count += 1
            print(f"PASS: {name}", flush=True)
            return result

        check("help", ["--help"], 0)
        check("version", ["--version"], 0)
        check("nonexistent target", [root / "missing.json"], 2)
        check("file conflicts with new", [broken, "--new"], 2)
        check("file conflicts with template", [broken, "--template"], 2)
        check("explicit corrupt file", [broken], 3)
        check("directory with only corrupt files", [profiles], 3)
        check("missing image", ["--background", root / "missing.png"], 4)
        check("explicit desktop overlay", ["--overlay", "--smoke-test"], 0)
        check("overlay/background conflict", ["--overlay", "--background", root / "missing.png"], 2)
        check("invalid screen index", ["--screen", "-1"], 2)
        check("invalid screen text", ["--screen", "no"], 2)
        check("extra positional argument", [profiles, profiles], 2)
        check("diagnostic mode conflict", ["--render", root / "x.png", "--smoke-test"], 2)
        check("explicit new in corrupt catalog", [profiles, "--new", "--render", root / "preview.png"], 0)
        check("render I/O failure", ["--render", root / "missing" / "x.png"], 4)
        check("built-in template", ["--template", "--smoke-test"], 0)
        if (root / "preview.png").stat().st_size <= 1000 or broken.read_text() != "{broken":
            raise AssertionError("Preview missing or input file modified")
        fixture = {
            "kind": "d4box.workflow", "schemaVersion": 1, "id": "efcd087a-d329-4823-aa66-650595cc50a7",
            "revision": 1, "name": "中文 fixture", "description": "CLI test", "status": "draft", "requires": [], "points": [],
            "layouts": [{"id": "default", "name": "Draft", "coordinateSpace": "client_normalized", "referenceClientPx": None,
                         "calibration": "unverified", "pointPositions": {}, "inventory": {"rows": 3, "columns": 11, "rect": None}}],
            "defaultLayoutId": "default", "steps": [], "checks": [], "entryCheckId": None,
            "source": {"mode": "hover_item", "maxItems": 1, "originalQualities": [], "excludedSlots": []},
            "limits": {"maxItemsPerRun": 1, "totalTimeoutMs": 60000},
        }
        target = profiles / "A 中文.d4workflow.json"
        original = json.dumps(fixture, ensure_ascii=False).encode("utf-8")
        target.write_bytes(original)
        check("explicit UTF-8 file", [target, "--render", root / "opened.png"], 0)
        check("valid catalog with corrupt sibling", [profiles, "--render", root / "directory.png"], 0)
        check("PNG background", [target, "--background", root / "preview.png", "--smoke-test"], 0)
        check("headless schema validation needs no GUI plugin", ["validate", target, "--format", "json"], 0,
              {"QT_QPA_PLATFORM": "not-a-real-platform"})
        check("draft is not statically ready", ["validate", target, "--for-run"], 3)
        check("validate corrupt sibling does not silently succeed", ["validate", profiles], 3)
        check("validate missing target", ["validate", root / "missing.json"], 2)
        check("validate unknown output format", ["validate", target, "--format", "xml"], 2)
        ui = check("offline editor interactions", ["--ui-self-test"], 0)
        if json.loads(ui.stdout)["failed"]:
            raise AssertionError(ui.stdout)
        native = check("native overlay diagnostics", ["--diagnose-overlay"], 0)
        report = json.loads(native.stdout)
        if not report["drawing"]["qtTransparentForInput"] or report["backgroundBlocksDesktopInput"]:
            raise AssertionError("Overlay must be transparent, not an opaque full-screen hit surface")
        if report["drawing"]["platform"] == "cocoa" and not report["drawing"]["nsWindowIgnoresMouseEvents"]:
            raise AssertionError("Cocoa ignoresMouseEvents flag is not set")
        if report["drawing"]["platform"] == "windows" and not report["drawing"]["nativeTransparent"]:
            raise AssertionError("Win32 drawing must have WS_EX_TRANSPARENT")
        if target.read_bytes() != original or len(list(profiles.glob("*.json"))) != 2:
            raise AssertionError("Diagnostic launch must not write configurations")
    print(f"PASS: {count} CLI cases; no user configuration touched")


if __name__ == "__main__":
    main()
