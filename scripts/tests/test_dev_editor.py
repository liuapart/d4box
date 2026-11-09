"""Offline coverage for the development launcher; no SDK/GUI required."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("dev_editor", Path(__file__).resolve().parents[1] / "dev_editor.py")
dev = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dev)


class DevEditorTests(unittest.TestCase):
    def test_defaults_and_passthrough(self):
        options, args = dev.parse_args(["--demo", "--", "a directory/config.json", "--template"])
        self.assertEqual(options.build_type, "Debug")
        self.assertTrue(options.demo)
        self.assertEqual(args, ["a directory/config.json", "--template"])
        self.assertEqual(dev.parse_args([])[1], [])

    def test_invalid_arguments(self):
        for args in (["--jobs", "0"], ["--jobs", "bad"], ["--unknown"],
                     ["--build-only", "--demo"], ["--build-only", "--", "config.json"]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                dev.parse_args(args)

    def test_demo_and_conflicts(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "game image.png"
            with patch.object(dev, "DEMO_BACKGROUND", image):
                with self.assertRaises(ValueError):
                    dev.launch_args(True, [])
                image.touch()
                self.assertEqual(dev.launch_args(True, ["--template"]),
                                 ["--background", str(image), "--template"])
                for args in (["--background=x.png"], ["--background", "x.png"],
                             ["--overlay"], ["validate", "config.json"]):
                    with self.subTest(args=args), self.assertRaises(ValueError):
                        dev.launch_args(True, args)
        self.assertEqual(dev.launch_args(False, ["--overlay"]), ["--overlay"])

    def test_sdk_precedence_and_incomplete_sdk(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def sdk(name):
                prefix = root / name
                for module in ("Qt6", "Qt6Widgets", "Qt6Svg"):
                    path = prefix / "lib/cmake" / module / (module + "Config.cmake")
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                return prefix.resolve()
            explicit, environment = sdk("explicit sdk"), sdk("environment")
            self.assertEqual(dev.qt_prefix(explicit, "Darwin", {"QT_PREFIX": str(environment)}), explicit)
            self.assertEqual(dev.qt_prefix(None, "Darwin", {"QT_PREFIX": str(environment)}), environment)
            with self.assertRaises(ValueError):
                dev.qt_prefix(root / "missing", "Darwin", {"QT_PREFIX": str(environment)})
            (explicit / "lib/cmake/Qt6Svg/Qt6SvgConfig.cmake").unlink()
            with self.assertRaises(ValueError):
                dev.qt_prefix(explicit, "Darwin", {})

    def test_build_plan_isolation_and_platforms(self):
        def plan(prefix="/sdk with spaces", system="Darwin", machine="arm64", mode="Debug"):
            return dev.build_plan(Path(prefix), system, machine, mode, 3, "cmake", "ctest", "ninja")
        build, executable, commands = plan()
        self.assertIn(".runtime/editor/dev/", build.as_posix())
        self.assertTrue(str(executable).endswith("D4BoxEditor.app/Contents/MacOS/D4BoxEditor"))
        self.assertIn("-DCMAKE_PREFIX_PATH=/sdk with spaces", commands[0])
        self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64", commands[0])
        self.assertIn("-DBUILD_TESTING=ON", commands[0])
        self.assertTrue(any(arg.startswith("-DD4BOX_RELEASE_VERSION=") for arg in commands[0]))
        self.assertEqual(commands[1][-2:], ["--parallel", "3"])
        self.assertEqual(commands[2][0], "ctest")
        for kwargs in ({"prefix": "/other"}, {"machine": "x86_64"}, {"mode": "MinSizeRel"}):
            self.assertNotEqual(build, plan(**kwargs)[0])
        _, win_executable, win_commands = plan(system="Windows", machine="amd64")
        self.assertEqual(win_executable.name, "D4BoxEditor.exe")
        self.assertFalse(any("OSX" in arg for arg in win_commands[0]))

    def run_main(self, argv, failure=None):
        with patch.object(dev.platform, "system", return_value="Darwin"), \
             patch.object(dev.platform, "machine", return_value="arm64"), \
             patch.object(dev, "qt_prefix", return_value=Path("/sdk")), \
             patch.object(dev.shutil, "which", side_effect=lambda name: "/bin/" + name), \
             patch.dict(dev.os.environ, {"QT_QPA_PLATFORM": "offscreen", "QT_QPA_PLATFORM_PLUGIN_PATH": "/bad"}), \
             patch.object(dev, "run", side_effect=failure) as run, \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            code = dev.main(argv)
        return code, run.call_args_list

    def test_main_preserves_args_cwd_and_environment(self):
        code, calls = self.run_main(["--", "relative config/file.json", "--background", "game image.png"])
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), 4)
        command, env, cwd = calls[-1].args
        self.assertEqual(command[1:], ["relative config/file.json", "--background", "game image.png"])
        self.assertEqual(cwd, Path.cwd())
        self.assertEqual(env["QT_PLUGIN_PATH"], "/sdk/plugins")
        self.assertNotIn("QT_QPA_PLATFORM", env)
        self.assertNotIn("QT_QPA_PLATFORM_PLUGIN_PATH", env)
        self.assertTrue(all(call.args[2] == dev.ROOT for call in calls[:3]))

    def test_build_only_and_failures_never_launch(self):
        code, calls = self.run_main(["--build-only"])
        self.assertEqual((code, len(calls)), (0, 3))
        for position in range(3):
            failure = [None] * position + [subprocess.CalledProcessError(7, "build/test")]
            code, calls = self.run_main([], failure)
            self.assertEqual(code, 7)
            self.assertEqual(len(calls), position + 1)

    def test_subprocess_uses_argv_not_shell(self):
        command = ["/path with spaces/editor", "a;not-shell.json"]
        with patch.object(dev.subprocess, "run") as run, contextlib.redirect_stdout(io.StringIO()):
            dev.run(command, {"KEY": "value"}, Path.cwd())
        run.assert_called_once_with(command, check=True, env={"KEY": "value"}, cwd=Path.cwd())


if __name__ == "__main__":
    unittest.main()
