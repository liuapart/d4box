#!/usr/bin/env python3
"""Build a self-contained x64 Qt Widgets ZIP and optional real Inno Setup installer.

Requires the official MSVC Qt SDK and a VS 2022 developer environment on Windows.
Never copies development-machine CRT DLLs: --crt-dir must be the VS Redist tree.
The package is unsigned. --self-test exercises portable safeguards on any host.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FAILURE_OUTPUT_LIMIT = 16_384  # Characters per stream; retain both the start and end.
VERSION_RE = r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?"
QT_ARCHIVE = "6.11.3-0-202610220312qtbase-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z"
QT_ARCHIVE_SHA256 = "68be61b613b963daf206834fdcc57dd3b84baff3082e50679e5846ca4a7a8b86"
QT_ARCHIVE_URL = (
    "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6113/"
    "qt6_6113_msvc2022_64/qt.qt6.6113.win64_msvc2022_64/" + QT_ARCHIVE
)

QT_SVG_ARCHIVE = QT_ARCHIVE.replace('qtbase-', 'qtsvg-')
QT_SVG_SHA256 = "e8c789e1a059601dd82114dd9560952891e7c8e94e1cb291166eb61d74800353"
QT_SVG_URL = QT_ARCHIVE_URL.replace(QT_ARCHIVE, QT_SVG_ARCHIVE)

# The editor uses raster Qt Widgets, local files and SVG images. It has no
# QtNetwork or Direct3D 12 shader compilation path. Keep the normal import audit
# and packaged runtime tests: these exclusions must fail closed if that changes.
RASTER_EXCLUDED_LIBRARIES = frozenset({"qt6network.dll", "dxcompiler.dll", "dxil.dll"})
RASTER_DEPLOY_OPTIONS = (
    "--release", "--no-translations", "--no-plugins", "--no-compiler-runtime",
    "--no-opengl-sw", "--no-system-d3d-compiler", "--no-system-dxc-compiler", "--no-network",
)


def validate_raster_dependencies(report: dict[str, list[str]]) -> None:
    """Refuse both excluded payload files and direct/delay imports of them.

    This is an explicit application policy, not a generic unused-DLL detector:
    plugin and LoadLibrary dependencies cannot be inferred from imports alone.
    """
    for binary, imports in report.items():
        names = {PureWindowsPath(binary).name.lower()}
        names.update(PureWindowsPath(name).name.lower() for name in imports)
        unexpected = names & RASTER_EXCLUDED_LIBRARIES
        if unexpected:
            raise RuntimeError(
                f"Raster-only deployment contract changed in {binary}: {sorted(unexpected)}; "
                "review deployment options and runtime tests before adding this dependency."
            )


def version_number(value: str) -> str:
    if not re.fullmatch(VERSION_RE, value):
        raise ValueError("Expected X.Y.Z or X.Y.Z-prerelease, without editor-v.")
    numbers = value.split("-", 1)[0].split(".")
    if any(int(part) > 65535 for part in numbers):
        raise ValueError("Version components must fit the Windows version resource (0..65535).")
    return ".".join(numbers) + ".0"


def run(*args: object, env: dict | None = None, cwd: Path | None = None) -> str:
    try:
        result = subprocess.run(
            [str(arg) for arg in args], check=True, capture_output=True,
            text=True, encoding="utf-8", errors="replace", env=env, cwd=cwd, timeout=300,
        )
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"{type(error).__name__}: {error}", file=sys.stderr, flush=True)
        for name, output in (("stdout", error.stdout), ("stderr", error.stderr)):
            # TimeoutExpired can retain bytes even when text=True was requested.
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            output = output or ""
            if len(output) > FAILURE_OUTPUT_LIMIT:
                omitted = len(output) - FAILURE_OUTPUT_LIMIT
                half = FAILURE_OUTPUT_LIMIT // 2
                output = (output[:half] + f"\n... [{omitted} characters omitted] ...\n"
                          + output[-half:])
            print(f"--- subprocess {name} ---\n{output or '<empty>'}", file=sys.stderr, flush=True)
        raise
    if result.stderr.strip():
        print(result.stderr, flush=True)
    return result.stdout


def require_x64(binary: Path) -> None:
    with binary.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise RuntimeError(f"Not a PE binary: {binary}")
        stream.seek(0x3C)
        offset = stream.read(4)
        if len(offset) != 4:
            raise RuntimeError(f"Truncated PE header: {binary}")
        stream.seek(struct.unpack("<I", offset)[0])
        if stream.read(6) != b"PE\x00\x00\x64\x86":
            raise RuntimeError(f"Expected Windows AMD64 binary: {binary}")


def validate_archive_listing(listing: str) -> None:
    members = [line.removeprefix("Path = ") for line in listing.splitlines() if line.startswith("Path = ")]
    if not members:
        raise RuntimeError("7-Zip returned no SDK members.")
    for member in members:
        path = PureWindowsPath(member)
        if path.drive or path.root or ".." in path.parts or ":" in member:
            raise RuntimeError(f"Unsafe SDK member: {member}")
    if "Symbolic Link = " in listing or "Hard Link = " in listing:
        raise RuntimeError("SDK archive must not contain filesystem links.")


def fetch_qt_sdk(prefix: Path, sevenzip: Path) -> None:
    """Qt 6.11 Windows uses compiler-specific repositories unsupported by aqt 3.3.0.

    Pin the official MSVC 2022 Qt Base/SVG archives AND their SHA-256 checksums instead of
    monkey-patching aqt, skipping integrity checks or using a third-party SDK.
    """
    prefix = prefix.resolve()
    if prefix.exists():
        raise RuntimeError("Refusing to overwrite an existing Qt SDK prefix.")
    prefix.parent.mkdir(parents=True, exist_ok=True)
    archives = []
    for name, url, expected in ((QT_ARCHIVE, QT_ARCHIVE_URL, QT_ARCHIVE_SHA256),
                                (QT_SVG_ARCHIVE, QT_SVG_URL, QT_SVG_SHA256)):
        archive = prefix.parent / name
        if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            partial = archive.with_suffix(".partial")
            print(f"Downloading official Qt SDK: {url}", flush=True)
            with urllib.request.urlopen(url, timeout=120) as response, partial.open("wb") as out:
                shutil.copyfileobj(response, out)
            with partial.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
            if actual != expected:
                raise RuntimeError(f"Qt SDK SHA-256 mismatch: {actual}")
            partial.replace(archive)
        validate_archive_listing(run(sevenzip, "l", "-slt", "-ba", "-sccUTF-8", archive))
        archives.append(archive)
    with tempfile.TemporaryDirectory(prefix=".qt-extract-", dir=prefix.parent) as temporary:
        stage = Path(temporary)
        for archive in archives:
            run(sevenzip, "x", "-y", "-sccUTF-8", f"-o{stage}", archive)
        # Current online archives contain bin/, lib/, etc. Accept the older versioned layout too.
        candidates = (stage, stage / "6.11.3/msvc2022_64")
        sdk = next((p for p in candidates if (p / "bin/windeployqt.exe").is_file()), None)
        if sdk is None:
            raise RuntimeError("Unexpected official Qt SDK archive layout.")
        require_x64(sdk / "bin/Qt6Core.dll")
        require_x64(sdk / "bin/Qt6Svg.dll")
        (sdk / "bin/qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
        if run(sdk / "bin/qtpaths.exe", "--query", "QT_VERSION").strip() != "6.11.3":
            raise RuntimeError("Wrong Qt SDK version.")
        shutil.copytree(sdk, prefix)
    (prefix / "SDK-SOURCE.json").write_text(json.dumps({
        "url": QT_ARCHIVE_URL, "sha256": QT_ARCHIVE_SHA256,
        "svg_url": QT_SVG_URL, "svg_sha256": QT_SVG_SHA256,
        "qt_version": "6.11.3", "architecture": "win64_msvc2022_64",
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Verified Qt SDK: {prefix}", flush=True)


def validate_licenses(directory: Path) -> None:
    # The workflow prepares these from the SHA-256-pinned Qt Base/SVG sources.
    if not (directory / "qtsvg/LICENSES/LGPL-3.0-only.txt").is_file():
        raise RuntimeError("Qt SVG license material is missing.")
    for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt"):
        if not any(p.is_file() and p.stat().st_size > 1000 for p in directory.rglob(name)):
            raise RuntimeError(f"Missing Qt license text: {name}")
    if not any(directory.rglob("qt_attribution.json")):
        raise RuntimeError("Qt third-party attribution material is missing.")
    if not (directory / "QT-SOURCE.txt").is_file():
        raise RuntimeError("Qt source provenance is missing.")


def clean_environment() -> dict[str, str]:
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(("QT_", "QML", "DYLD_"))}
    system = Path(os.environ["SystemRoot"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    return env


def test_runtime(root: Path, expected_qt: str, expected_version: str) -> dict:
    executable = root / "D4BoxEditor.exe"
    env = clean_environment()
    info = json.loads(run(executable, "--runtime-info", env=env, cwd=root))
    if run(executable, "--version", env=env, cwd=root).strip() != f"D4BoxEditor {expected_version}":
        raise RuntimeError("Application version does not match the release tag.")
    if info["qtVersion"] != expected_qt or info["architecture"] != "x86_64":
        raise RuntimeError(f"Unexpected packaged Qt/architecture: {info}")
    if info["platform"] != "windows" or not info["pluginPaths"]:
        raise RuntimeError(f"Expected native Windows platform: {info}")
    for path in info["pluginPaths"]:
        if not Path(path).resolve().is_relative_to(root.resolve()):
            raise RuntimeError(f"Plugin search escaped the package: {path}")
    run(executable, "--smoke-test", env=env, cwd=root)
    run(executable, "--ui-self-test", env=env, cwd=root)
    with tempfile.TemporaryDirectory(prefix="d4box-render-") as temporary:
        preview = Path(temporary) / "preview.png"
        run(executable, "--render", preview, env=env, cwd=root)
        if not preview.is_file() or preview.stat().st_size < 1000:
            raise RuntimeError("Packaged editor failed to render a real preview.")
        run(executable, "--background", preview, "--smoke-test", env=env, cwd=root)
    return info


def audit_dependencies(root: Path, dumpbin: Path) -> dict[str, list[str]]:
    system = Path(os.environ["SystemRoot"]) / "System32"
    report = {}
    binaries = sorted(p for p in root.rglob("*") if p.suffix.lower() in (".dll", ".exe"))
    for binary in binaries:
        require_x64(binary)
        output = run(dumpbin, "/nologo", "/dependents", binary)
        dependencies = re.findall(r"^\s+([\w.-]+\.dll)\s*$", output, re.I | re.M)
        if not dependencies:
            raise RuntimeError(f"No import information from dumpbin: {binary}")
        for name in dependencies:
            if (root / name).is_file() or (binary.parent / name).is_file():
                continue
            # Even if installed globally on the runner, CRT/Qt must be app-local.
            if name.lower().startswith(("qt6", "msvcp", "vcruntime", "concrt")):
                raise RuntimeError(f"Missing app-local dependency {name} of {binary.name}")
            if name.lower().startswith(("api-ms-win-", "ext-ms-win-")) or (system / name).is_file():
                continue
            raise RuntimeError(f"Unresolved dependency {name} of {binary.name}")
        report[str(binary.relative_to(root))] = dependencies
    return report


def inno_script(payload: Path, output: Path, version: str, stem: str) -> str:
    numeric = version_number(version)
    # Paths are generated by the workflow, not user-controlled Inno preprocessor code.
    for path in (payload, output):
        if any(c in str(path) for c in ('"', '\n', '\r', '{', '}')):
            raise ValueError("Unsupported character in Inno Setup path.")
    return fr"""; Generated by package_editor_windows.py, editor only.
[Setup]
AppId={{{{5A093F70-B9CD-437B-B19E-F33CD284FB19}}
AppName=D4Box Editor
AppVersion={version}
VersionInfoVersion={numeric}
DefaultDirName={{localappdata}}\Programs\D4BoxEditor
DefaultGroupName=D4Box Editor
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={output}
OutputBaseFilename={stem}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayIcon={{app}}\D4BoxEditor.exe
CloseApplications=no
RestartApplications=no
[Files]
Source: "{payload}\*"; DestDir: "{{app}}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{{group}}\D4Box Editor"; Filename: "{{app}}\D4BoxEditor.exe"
"""


def package(args: argparse.Namespace) -> None:
    version_number(args.version)
    source = args.build_dir.resolve() / "D4BoxEditor.exe"
    require_x64(source)
    qt = args.qt_prefix.resolve(strict=True)
    licenses = args.licenses_dir.resolve(strict=True)
    validate_licenses(licenses)
    crt = args.crt_dir.resolve(strict=True)
    if ("redist" not in [p.lower() for p in crt.parts] or
            crt.parent.name.lower() != "x64" or crt.name != "Microsoft.VC143.CRT"):
        raise RuntimeError("CRT must come from VS Redist/MSVC/.../x64/Microsoft.VC143.CRT.")
    for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        if not (crt / name).is_file():
            raise RuntimeError(f"Required redistributable missing: {name}")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    stem = f"D4BoxEditor-{args.version}-windows-x64"
    if any(output.glob(stem + "*")):
        raise RuntimeError("Output already exists; choose a fresh output directory.")
    with tempfile.TemporaryDirectory(prefix=".d4box-package-", dir=output) as temporary:
        staging = Path(temporary)
        payload = staging / "D4BoxEditor"
        payload.mkdir()
        shutil.copy2(source, payload / source.name)
        print("Deploying Qt Widgets/SVG and native Windows/JPEG/SVG plugins...", flush=True)
        run(qt / "bin/windeployqt.exe", *RASTER_DEPLOY_OPTIONS, payload / source.name)
        for relative in ("platforms/qwindows.dll", "imageformats/qjpeg.dll",
                         "imageformats/qsvg.dll", "iconengines/qsvgicon.dll"):
            destination = payload / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(qt / "plugins" / relative, destination)
        for library in crt.glob("*.dll"):
            require_x64(library)
            shutil.copy2(library, payload / library.name)
        (payload / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\n", encoding="utf-8")
        shutil.copytree(licenses, payload / "licenses")
        (payload / "EDITOR-NOTICE.txt").write_text(
            f"D4Box Editor {args.version}: standalone C++ / Qt Widgets editor only.\n"
            "Edits draft JSON; does not execute game workflows or send game input.\n"
            "Extract the whole ZIP before launching D4BoxEditor.exe. Do not move the EXE alone.\n"
            "This unsigned build may trigger Windows SmartScreen. No AHK runtime is included.\n"
            "Qt is dynamically linked; see licenses/QT-SOURCE.txt and the enclosed license texts.\n"
            "MSVC runtime DLLs are from Visual Studio 2022's designated Redist directory,\n"
            "not developer bin/System32. Their Microsoft licenses remain applicable.\n"
            "https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution\n"
            "App-local CRT security updates require a new editor package.\n",
            encoding="utf-8",
        )
        dependencies = audit_dependencies(payload, args.dumpbin)
        validate_raster_dependencies(dependencies)
        runtime = test_runtime(payload, args.qt_version, args.version)
        print(run(sys.executable, ROOT / "scripts/check_editor_cli.py", payload / source.name,
                  env=clean_environment(), cwd=payload), flush=True)
        archive = staging / (stem + ".zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
            for path in sorted(payload.rglob("*")):
                if path.is_file():
                    zipped.write(path, path.relative_to(staging))
        # Test what users actually download, after extraction to an unrelated path.
        with tempfile.TemporaryDirectory(prefix="d4box-relocated-") as extracted:
            with zipfile.ZipFile(archive) as zipped:
                if zipped.testzip() is not None:
                    raise RuntimeError("Corrupt ZIP.")
                zipped.extractall(extracted)
            test_runtime(Path(extracted) / "D4BoxEditor", args.qt_version, args.version)
        installer = None
        if args.iscc:
            script = staging / "editor.iss"
            script.write_text(inno_script(payload, staging, args.version, stem), encoding="utf-8-sig")
            print(run(args.iscc, script), flush=True)
            installer = staging / (stem + "-setup.exe")
            if not installer.is_file() or installer.stat().st_size < 100000:
                raise RuntimeError("Inno Setup did not create a real installer.")
            with tempfile.TemporaryDirectory(prefix="d4box-install-") as temporary_install:
                installed = Path(temporary_install) / "Editor"
                run(installer, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-",
                    f"/DIR={installed}", "/NOICONS")
                try:
                    test_runtime(installed, args.qt_version, args.version)
                finally:
                    run(installed / "unins000.exe", "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART")
        report = {
            "platform": "Windows", "architecture": "x64", "release_version": args.version,
            "qt_version": runtime["qtVersion"], "build_type": "MinSizeRel",
            "zip_bytes": archive.stat().st_size,
            "payload_bytes": sum(p.stat().st_size for p in payload.rglob("*") if p.is_file()),
            "installer_bytes": installer.stat().st_size if installer else None,
            "pe_dependency_audit": dependencies,
            "deployment_policy": {
                "rendering": "raster Qt Widgets with local files and SVG",
                "excluded_optional_libraries": sorted(RASTER_EXCLUDED_LIBRARIES),
                "windeployqt_options": list(RASTER_DEPLOY_OPTIONS),
            },
            "largest_files": [
                {"path": str(p.relative_to(payload)), "bytes": p.stat().st_size}
                for p in sorted((p for p in payload.rglob("*") if p.is_file()),
                                key=lambda p: p.stat().st_size, reverse=True)[:15]
            ],
            "checks": {"native_windows_smoke": True, "window_render": True,
                       "ui_interactions": True, "cli_regression": True, "svg_theme_multiscale": True,
                       "raster_dependency_contract": True,
                       "sdk_free_plugin_search": True, "relocated_zip_smoke": True,
                       "installer_install_launch_uninstall": installer is not None},
            "limitations": ["Unsigned; no Windows SmartScreen reputation",
                            "Not tested on a clean end-user PC; no game execution tested"],
        }
        shutil.move(archive, output / archive.name)
        if installer:
            shutil.move(installer, output / installer.name)
        (output / (stem + "-report.json")).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2), flush=True)


def self_test() -> None:
    """Offline guard tests; these do not claim native Windows deployment works."""
    from contextlib import redirect_stderr, redirect_stdout
    import io
    import unittest
    from unittest import mock

    class GuardTests(unittest.TestCase):
        def test_raster_dependency_contract(self):
            validate_raster_dependencies({
                "D4BoxEditor.exe": ["Qt6Widgets.dll", "Qt6Svg.dll"],
                "Qt6Widgets.dll": ["Qt6Gui.dll", "Qt6Core.dll"],
                "imageformats\\qsvg.dll": ["Qt6Svg.dll"],
            })
            for name in ("Qt6Network.dll", "dxcompiler.dll", "DXIL.DLL"):
                with self.subTest(name=name), self.assertRaisesRegex(RuntimeError, "contract changed"):
                    validate_raster_dependencies({"Qt6Gui.dll": [name]})
                with self.subTest(payload=name), self.assertRaisesRegex(RuntimeError, "contract changed"):
                    validate_raster_dependencies({"plugins\\" + name: ["Qt6Core.dll"]})

        def test_subprocess_failure_emits_both_streams_and_reraises(self):
            log = io.StringIO()
            with redirect_stderr(log), self.assertRaises(subprocess.CalledProcessError) as caught:
                run(sys.executable, "-c",
                    "import sys; print('stdout-marker', flush=True); "
                    "print('stderr-marker', file=sys.stderr, flush=True); sys.exit(23)")
            self.assertEqual(caught.exception.returncode, 23)
            self.assertEqual(caught.exception.stdout.strip(), "stdout-marker")
            self.assertEqual(caught.exception.stderr.strip(), "stderr-marker")
            self.assertIn("--- subprocess stdout ---\nstdout-marker", log.getvalue())
            self.assertIn("--- subprocess stderr ---\nstderr-marker", log.getvalue())

        def test_subprocess_timeout_emits_partial_bytes_and_reraises(self):
            real_run = subprocess.run

            def short_timeout(*args, **kwargs):
                # Exercise real OS pipes and TimeoutExpired, without the production 300s wait.
                kwargs["timeout"] = 2
                return real_run(*args, **kwargs)

            log = io.StringIO()
            with mock.patch("subprocess.run", side_effect=short_timeout), redirect_stderr(log):
                with self.assertRaises(subprocess.TimeoutExpired) as caught:
                    run(sys.executable, "-c",
                        "import sys,time; print('timeout-stdout', flush=True); "
                        "sys.stderr.buffer.write(bytes([0xe4, 0xb8, 0xad, 0xff])); "
                        "sys.stderr.flush(); time.sleep(30)")
            self.assertEqual(caught.exception.timeout, 2)
            self.assertIn("--- subprocess stdout ---\ntimeout-stdout", log.getvalue())
            self.assertIn("--- subprocess stderr ---\n中\ufffd", log.getvalue())

        def test_subprocess_failure_log_is_bounded(self):
            log = io.StringIO()
            code = ("import sys; "
                    f"print('OUT-HEAD' + 'x' * {FAILURE_OUTPUT_LIMIT * 2} + 'OUT-TAIL'); "
                    f"print('ERR-HEAD' + 'y' * {FAILURE_OUTPUT_LIMIT * 2} + 'ERR-TAIL', file=sys.stderr); "
                    "sys.exit(9)")
            with redirect_stderr(log), self.assertRaises(subprocess.CalledProcessError) as caught:
                run(sys.executable, "-c", code)
            self.assertEqual(caught.exception.returncode, 9)
            self.assertGreater(len(caught.exception.stdout), FAILURE_OUTPUT_LIMIT)
            text = log.getvalue()
            for marker in ("OUT-HEAD", "OUT-TAIL", "ERR-HEAD", "ERR-TAIL"):
                self.assertIn(marker, text)
            self.assertEqual(text.count("characters omitted"), 2)
            self.assertLess(len(text), 2 * FAILURE_OUTPUT_LIMIT + 1024)

        def test_subprocess_success_keeps_existing_behavior(self):
            stdout, stderr = io.StringIO(), io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                result = run(sys.executable, "-c",
                             "import sys; print('success'); print('warning', file=sys.stderr)")
            self.assertEqual(result.strip(), "success")
            self.assertEqual(stdout.getvalue().strip(), "warning")
            self.assertEqual(stderr.getvalue(), "")

        def test_versions(self):
            self.assertEqual(version_number("1.2.3-rc.1"), "1.2.3.0")
            for bad in ("editor-v1.0.0", "01.2.3", "1.2", "1.2.3\n", "1.2.3;echo", "65536.0.0"):
                with self.assertRaises(ValueError):
                    version_number(bad)

        def test_pe_architecture(self):
            with tempfile.TemporaryDirectory() as directory:
                binary = Path(directory) / "fixture.exe"
                content = bytearray(128)
                content[:2] = b"MZ"
                struct.pack_into("<I", content, 0x3C, 64)
                content[64:70] = b"PE\x00\x00\x64\x86"
                binary.write_bytes(content)
                require_x64(binary)
                content[68:70] = b"\x4c\x01"
                binary.write_bytes(content)
                with self.assertRaises(RuntimeError):
                    require_x64(binary)
                binary.write_bytes(b"MZ")
                with self.assertRaises(RuntimeError):
                    require_x64(binary)

        def test_license_guard(self):
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(RuntimeError):
                    validate_licenses(Path(directory))

        def test_sdk_archive_paths(self):
            validate_archive_listing("Path = bin/Qt6Core.dll\n\nPath = lib/Qt6Core.lib\n")
            for path in ("../escape", "C:\\escape", "\\\\server\\share", "/absolute", "bin/file:stream"):
                with self.assertRaises(RuntimeError):
                    validate_archive_listing("Path = " + path)
            for listing in ("", "Path = file\nSymbolic Link = elsewhere"):
                with self.assertRaises(RuntimeError):
                    validate_archive_listing(listing)

        def test_installer_manifest(self):
            script = inno_script(Path("C:/payload"), Path("C:/out"), "1.2.3-rc.1", "editor")
            self.assertIn("AppId={{5A093F70-B9CD-437B-B19E-F33CD284FB19}", script)
            self.assertIn("VersionInfoVersion=1.2.3.0", script)
            self.assertIn("PrivilegesRequired=lowest", script)
            self.assertNotIn("[Run]", script)
            with self.assertRaises(ValueError):
                inno_script(Path('bad"path'), Path("out"), "1.2.3", "editor")

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(GuardTests))
    if not result.wasSuccessful():
        raise SystemExit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--fetch-qt", action="store_true", help="Fetch the SHA-256-pinned official MSVC SDK.")
    parser.add_argument("--sevenzip", type=Path, default=Path("7z.exe"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".runtime/editor/build")
    parser.add_argument("--qt-prefix", type=Path)
    parser.add_argument("--qt-version", default="6.11.3")
    parser.add_argument("--licenses-dir", type=Path)
    parser.add_argument("--crt-dir", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--dumpbin", type=Path, default=Path("dumpbin.exe"))
    parser.add_argument("--iscc", type=Path, help="Compile AND install/launch/uninstall-test a real installer.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".runtime/editor/dist-windows")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if platform.system() != "Windows":
        parser.error("Native packaging requires Windows; use --self-test for portable checks.")
    if args.fetch_qt:
        if not args.qt_prefix:
            parser.error("--fetch-qt requires --qt-prefix.")
        fetch_qt_sdk(args.qt_prefix, args.sevenzip)
        return
    if not all((args.qt_prefix, args.licenses_dir, args.crt_dir, args.version)):
        parser.error("--qt-prefix, --licenses-dir, --crt-dir and --version are required.")
    package(args)


if __name__ == "__main__":
    main()
