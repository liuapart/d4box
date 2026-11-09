#!/usr/bin/env python3
"""Incrementally build, test and run the standalone editor, without packaging."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import runpy
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEMO_BACKGROUND = ROOT / ".runtime/editor/backgrounds/transmute-game-original.png"


def positive_int(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("必须是正整数")
    return number


def parse_args(argv: list[str]) -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description="编辑器 dev：配置 CMake → 增量编译 → CTest → 前台启动；不打包、不执行游戏操作。",
        epilog="编辑器参数放在 -- 后，例如：./dev.sh --demo -- ./workflows --template",
        allow_abbrev=False,
    )
    parser.add_argument("--qt-prefix", type=Path, help="Qt SDK 路径，优先于 QT_PREFIX 和仓库内 SDK")
    parser.add_argument("--build-type", choices=("Debug", "RelWithDebInfo", "MinSizeRel"), default="Debug")
    parser.add_argument("--jobs", type=positive_int, default=min(os.cpu_count() or 1, 4), help="编译并行数，默认最多 4")
    parser.add_argument("--demo", action="store_true", help="使用本机已保存的游戏底图；不下载或提交图片")
    parser.add_argument("--build-only", action="store_true", help="只编译并运行 CTest，不启动 GUI")
    separator = argv.index("--") if "--" in argv else len(argv)
    options = parser.parse_args(argv[:separator])
    editor_args = argv[separator + 1:]
    if options.build_only and (editor_args or options.demo):
        parser.error("--build-only 不接受 --demo 或编辑器参数，以免静默忽略")
    return options, editor_args


def qt_prefix(explicit: Path | None, system: str, env: dict[str, str]) -> Path:
    # Reuse the SDK pin rather than introducing another version constant.
    version = runpy.run_path(str(ROOT / "scripts/fetch_editor_qt_macos.py"))["VERSION"]
    arch = "macos" if system == "Darwin" else "msvc2022_64"
    candidates = ([explicit] if explicit is not None else
                  [Path(env["QT_PREFIX"])] if env.get("QT_PREFIX") else
                  [ROOT / ".runtime/editor" / folder / version / arch for folder in ("qt-official", "qt")])
    for candidate in candidates:
        prefix = candidate.expanduser().resolve()
        if all((prefix / "lib/cmake" / module / (module + "Config.cmake")).is_file()
               for module in ("Qt6", "Qt6Widgets", "Qt6Svg")):
            return prefix
    hint = ("先运行 python3 scripts/fetch_editor_qt_macos.py 下载仓库固定版本 SDK。"
            if system == "Darwin" else "请安装匹配 MSVC 2022 x64 的 Qt Base + SVG SDK。")
    raise ValueError("未找到完整 Qt Base + SVG SDK：" + ", ".join(map(str, candidates)) +
                     "\n" + hint + "\n也可通过 --qt-prefix 或 QT_PREFIX 指定；显式指定错误时不会偷偷回退。")


def launch_args(demo: bool, args: list[str]) -> list[str]:
    if not demo:
        return list(args)
    if any(arg.split("=", 1)[0] in ("--background", "--overlay") for arg in args):
        raise ValueError("--demo 不能和编辑器的 --background / --overlay 同时使用")
    if args and args[0] == "validate":
        raise ValueError("validate 是无界面校验，不能与 --demo 同时使用")
    if not DEMO_BACKGROUND.is_file():
        raise ValueError(f"本机底图不存在：{DEMO_BACKGROUND}\n请自行保存本地图片，或用 -- --background /路径/图片.png")
    return ["--background", str(DEMO_BACKGROUND), *args]


def build_plan(prefix: Path, system: str, machine: str, build_type: str, jobs: int,
               cmake: str, ctest: str, ninja: str) -> tuple[Path, Path, list[list[str]]]:
    # Isolate from release builds and from different SDKs, architectures and build types.
    sdk_id = hashlib.sha256(str(prefix).encode()).hexdigest()[:10]
    build = ROOT / ".runtime/editor/dev" / f"{system.lower()}-{machine}-{sdk_id}" / build_type
    source = ROOT / "app/editor"
    match = re.search(r'set\(D4BOX_RELEASE_VERSION "([^"]+)"', (source / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not match:
        raise ValueError("无法从 CMakeLists.txt 读取当前编辑器版本")
    configure = [cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                 f"-DCMAKE_MAKE_PROGRAM={ninja}", f"-DCMAKE_PREFIX_PATH={prefix}",
                 f"-DCMAKE_BUILD_TYPE={build_type}", "-DBUILD_TESTING=ON",
                 f"-DD4BOX_RELEASE_VERSION={match.group(1)}"]
    if system == "Darwin":
        configure += [f"-DCMAKE_OSX_ARCHITECTURES={machine}", "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0"]
    executable = (build / "D4BoxEditor.app/Contents/MacOS/D4BoxEditor" if system == "Darwin"
                  else build / "D4BoxEditor.exe")
    return build, executable, [configure,
                             [cmake, "--build", str(build), "--parallel", str(jobs)],
                             [ctest, "--test-dir", str(build), "--output-on-failure"]]


def run(command: list[str], env: dict[str, str], cwd: Path) -> None:
    display = subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)
    print("+ " + display, flush=True)
    subprocess.run(command, check=True, env=env, cwd=cwd)


def main(argv: list[str] | None = None) -> int:
    options, forwarded = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        system, machine = platform.system(), platform.machine().lower()
        if system not in ("Darwin", "Windows"):
            raise ValueError("dev 入口目前仅支持 macOS 和 Windows；不承诺 Linux 覆盖层支持。")
        env = os.environ.copy()
        if system == "Windows" and (env.get("VSCMD_ARG_TGT_ARCH", "").lower() not in ("x64", "amd64")
                                    or not shutil.which("cl.exe")):
            raise ValueError("请在 VS 2022 x64 Developer PowerShell 中运行此脚本。")
        prefix = qt_prefix(options.qt_prefix, system, env)
        editor_args = launch_args(options.demo, forwarded)
        programs = [shutil.which(name) for name in ("cmake", "ctest", "ninja")]
        if not all(programs):
            raise ValueError("需要 cmake、ctest、ninja；macOS 可用 brew install cmake ninja 安装。")
        build, executable, commands = build_plan(prefix, system, machine, options.build_type,
                                                 options.jobs, *programs)
        # Use this SDK's DLLs/plugins rather than an unrelated system Qt install.
        env["PATH"] = str(prefix / "bin") + os.pathsep + env.get("PATH", "")
        env["QT_PLUGIN_PATH"] = str(prefix / "plugins")
        env.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
        env.pop("QT_QPA_PLATFORM", None)  # GUI must not silently launch offscreen
        print(f"Qt SDK: {prefix}\nDev build: {build}", flush=True)
        for command in commands:
            run(command, env, ROOT)
        if options.build_only:
            print(f"编译和 CTest 通过：{executable}")
            return 0
        print("启动开发版。请用界面退出以处理未保存修改；已有会话不会被强制关闭。", flush=True)
        # Relative config/image paths retain the terminal caller's meaning.
        run([str(executable), *editor_args], env, Path.cwd())
        return 0
    except (ValueError, OSError) as error:
        print(f"dev: {error}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as error:
        print(f"dev: 命令失败（退出码 {error.returncode}），已停止后续步骤。", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except KeyboardInterrupt:
        print("\ndev: 已中断。", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
