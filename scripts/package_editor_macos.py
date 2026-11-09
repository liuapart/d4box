#!/usr/bin/env python3
"""Package the editor and measure a relocatable macOS runtime, not just its exe.

Uses the selected Qt SDK's macdeployqt, retains only Cocoa + JPEG plugins,
audits native dependencies, ad-hoc signs, and tests the deployed application.
Release packages remain ad-hoc signed, not Developer-ID signed or notarized.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MACHO_MAGIC = {
    b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe",
    b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
}


def run(*args: object, capture: bool = False, env: dict | None = None) -> str:
    result = subprocess.run(
        [str(x) for x in args], check=True, text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None, env=env, timeout=240,
    )
    return result.stdout if capture else ""


def regular_files(path: Path) -> list[Path]:
    return sorted(p for p in path.rglob("*") if p.is_file() and not p.is_symlink())


def macho_files(path: Path) -> list[Path]:
    result = []
    for p in regular_files(path):
        with p.open("rb") as stream:
            if stream.read(4) in MACHO_MAGIC:
                result.append(p)
    return result


def total_bytes(path: Path) -> int:
    return sum(p.stat().st_size for p in regular_files(path))


def rpaths(binary: Path) -> list[str]:
    commands = run("/usr/bin/otool", "-l", binary, capture=True).splitlines()
    return [commands[i + 2].strip().removeprefix("path ").split(" (", 1)[0]
            for i, line in enumerate(commands) if line.strip() == "cmd LC_RPATH"]


def audit(bundle: Path) -> dict:
    dependencies = {}
    for binary in macho_files(bundle):
        linked = [line.strip().split(" (", 1)[0]
                  for line in run("/usr/bin/otool", "-L", binary, capture=True).splitlines()[1:]]
        # otool -L also prints a dylib's own LC_ID_DYLIB; that is not a dependency.
        identities = set(run("/usr/bin/otool", "-D", binary, capture=True).splitlines()[1:])
        linked = [item for item in linked if item not in identities]
        # Non-system absolute references make the bundle machine-dependent.
        bad = [p for p in linked if p.startswith("/") and
               not p.startswith(("/usr/lib/", "/System/Library/"))]
        if bad:
            raise RuntimeError(f"Undeployed dependency in {binary.name}: {bad}")
        for dependency in linked:
            if dependency.startswith("@rpath/"):
                target = bundle / "Contents/Frameworks" / dependency.removeprefix("@rpath/")
                if not target.exists():
                    raise RuntimeError(f"Unresolved framework/library: {dependency}")
            elif dependency.startswith("@loader_path/"):
                if not (binary.parent / dependency.removeprefix("@loader_path/")).exists():
                    raise RuntimeError(f"Unresolved loader-relative dependency: {dependency}")
            elif dependency.startswith("@executable_path/"):
                target = bundle / "Contents/MacOS" / dependency.removeprefix("@executable_path/")
                if not target.exists():
                    raise RuntimeError(f"Unresolved executable-relative dependency: {dependency}")
        for entry in rpaths(binary):
            if entry.startswith("/"):
                raise RuntimeError(f"Absolute build rpath in {binary.name}: {entry}")
        dependencies[str(binary.relative_to(bundle))] = linked
    return dependencies


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".runtime/editor/build")
    parser.add_argument("--qt-prefix", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".runtime/editor/dist")
    parser.add_argument("--arch", default=platform.machine(), choices=("arm64", "x86_64"))
    parser.add_argument("--licenses-dir", type=Path,
                        help="Release license/attribution tree prepared from pinned Qt sources.")
    parser.add_argument("--release-version", help="Require the app version to match this release tag.")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This packager is macOS-only; Windows sizes require a native build.")
    if args.release_version and not args.licenses_dir:
        parser.error("Release packaging requires --licenses-dir.")
    if args.licenses_dir:
        for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt"):
            if not any(p.is_file() and p.stat().st_size > 1000 for p in args.licenses_dir.rglob(name)):
                parser.error(f"Missing Qt license text: {name}")
        if not any(args.licenses_dir.rglob("qt_attribution.json")):
            parser.error("Qt third-party attribution material is missing.")
        if not (args.licenses_dir / "QT-SOURCE.txt").is_file():
            parser.error("Qt source provenance is missing.")
    qt = args.qt_prefix.resolve()
    source = args.build_dir.resolve() / "D4BoxEditor.app"
    if not (source / "Contents/MacOS/D4BoxEditor").is_file():
        parser.error("Build D4BoxEditor.app first.")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    deployed = output / "D4BoxEditor.app"
    if deployed.exists():
        with (deployed / "Contents/Info.plist").open("rb") as stream:
            if plistlib.load(stream).get("CFBundleIdentifier") != "io.d4box.editor":
                raise RuntimeError("Refusing to replace an unrelated application.")
    plugins = Path(run(qt / "bin/qtpaths", "--query", "QT_INSTALL_PLUGINS", capture=True).strip())
    with tempfile.TemporaryDirectory(prefix=".d4box-package-", dir=output) as stage:
        app = Path(stage) / "D4BoxEditor.app"
        shutil.copytree(source, app, symlinks=True)
        extra = []
        for relative in ("platforms/libqcocoa.dylib", "imageformats/libqjpeg.dylib"):
            destination = app / "Contents/PlugIns" / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(plugins / relative, destination)
            extra.append(f"-executable={destination}")
        print("Deploying only the selected platform/image plugins...", flush=True)
        run(qt / "bin/macdeployqt", app, "-no-plugins", "-no-codesign", "-verbose=1", *extra)
        resources = app / "Contents/Resources"
        resources.mkdir(exist_ok=True)
        (resources / "qt.conf").write_text("[Paths]\nPlugins = PlugIns\n", encoding="utf-8")
        (resources / "EDITOR-NOTICE.txt").write_text(
            "D4Box Editor: standalone C++ / Qt Widgets editor only.\n"
            "Edits draft JSON; does not execute game workflows or send game input. No AHK runtime.\n"
            "Dynamically linked Qt and third-party dependencies retain their respective licenses.\n"
            "See licenses/QT-SOURCE.txt for Qt source provenance in release packages.\n"
            "Ad-hoc signing is not Developer ID signing or notarization; Gatekeeper may block launch.\n",
            encoding="utf-8",
        )
        # Keep available license material; never remove notices to reduce the size report.
        for candidate in (qt / "LICENSES", qt / "licenses", qt / "share/licenses"):
            if candidate.is_dir():
                shutil.copytree(candidate, resources / "licenses", dirs_exist_ok=True)
        if args.licenses_dir:
            shutil.copytree(args.licenses_dir, resources / "licenses", dirs_exist_ok=True)
        for binary in macho_files(app):
            arches = run("/usr/bin/lipo", "-archs", binary, capture=True).split()
            if args.arch not in arches:
                raise RuntimeError(f"{binary.name} does not support {args.arch}")
            if len(arches) > 1:
                thin = binary.with_name(binary.name + ".thin")
                run("/usr/bin/lipo", binary, "-thin", args.arch, "-output", thin)
                thin.replace(binary)
            # macdeployqt may leave an SDK build search path in third-party dylibs.
            # Internal dependencies are audited below before signing or launching.
            for entry in rpaths(binary):
                if entry.startswith("/"):
                    run("/usr/bin/install_name_tool", "-delete_rpath", entry, binary)
        executable = app / "Contents/MacOS/D4BoxEditor"
        run("/usr/bin/strip", "-S", "-x", executable)
        dependencies = audit(app)
        run("/usr/bin/codesign", "--force", "--deep", "--sign", "-", app)
        run("/usr/bin/codesign", "--verify", "--deep", "--strict", app)
        if deployed.exists():
            shutil.rmtree(deployed)
        shutil.move(app, deployed)
    print("Testing the deployed app with SDK/plugin overrides removed...", flush=True)
    clean_env = {k: v for k, v in os.environ.items()
                 if not k.startswith(("QT_", "QML", "DYLD_"))}
    clean_env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    executable = deployed / "Contents/MacOS/D4BoxEditor"
    runtime = json.loads(run(executable, "--runtime-info", capture=True, env=clean_env))
    if runtime["architecture"] != args.arch or runtime["platform"] != "cocoa":
        raise RuntimeError(f"Unexpected native macOS runtime: {runtime}")
    if args.release_version:
        version = run(executable, "--version", capture=True, env=clean_env).strip()
        if version != f"D4BoxEditor {args.release_version}":
            raise RuntimeError(f"App version does not match the release tag: {version}")
    for path in runtime["pluginPaths"]:
        if not Path(path).resolve().is_relative_to(deployed.resolve()):
            raise RuntimeError(f"Runtime can search outside packaged app: {path}")
    run(executable, "--smoke-test", env=clean_env)
    run(executable, "--ui-self-test", capture=True, env=clean_env)
    run(executable, "--render", output / "editor-preview.png", env=clean_env)
    with tempfile.TemporaryDirectory(prefix=".d4box-image-test-", dir=output) as image_dir:
        jpeg = Path(image_dir) / "input.jpg"
        run("/usr/bin/sips", "-s", "format", "jpeg", output / "editor-preview.png", "--out", jpeg,
            capture=True)
        for background in (output / "editor-preview.png", jpeg):
            run(executable, "--background", background, "--smoke-test", env=clean_env)
    archive = output / "D4BoxEditor-macos.zip"
    if archive.exists():
        archive.unlink()
    run("/usr/bin/ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", deployed, archive)
    files = regular_files(deployed)
    frameworks = deployed / "Contents/Frameworks"
    report = {
        "platform": "macOS", "architecture": args.arch,
        "qt_version": runtime["qtVersion"], "build_type": "MinSizeRel",
        "application_executable_bytes": executable.stat().st_size,
        "bundle_payload_bytes": total_bytes(deployed),
        "bundle_allocated_bytes": int(run("/usr/bin/du", "-sk", deployed, capture=True).split()[0]) * 1024,
        "zip_bytes": archive.stat().st_size,
        "frameworks_and_libraries": {
            p.name: total_bytes(p) if p.is_dir() else p.stat().st_size
            for p in sorted(frameworks.iterdir()) if not p.is_symlink()
        },
        "largest_files": [{"path": str(p.relative_to(deployed)), "bytes": p.stat().st_size}
                          for p in sorted(files, key=lambda p: p.stat().st_size, reverse=True)[:12]],
        "macho_dependency_audit": dependencies,
        "checks": {"external_absolute_dependencies": 0, "sdk_free_plugin_search": True,
                   "adhoc_signature": True, "native_cocoa_smoke": True, "window_render": True,
                   "png_and_jpeg_decode": True, "ui_interactions": True},
        "limitations": ["Not tested on a clean second Mac", "Not Developer-ID signed/notarized",
                        "No game execution tested"] +
                       ([] if args.licenses_dir else ["Local package: release license bundle not supplied"]),
    }
    (output / "size-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k.endswith("_bytes")}, indent=2))
    print(f"Report: {output / 'size-report.json'}")


if __name__ == "__main__":
    main()
