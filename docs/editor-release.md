# 独立编辑器的 tag 构建与发布

工作流：`.github/workflows/editor-release.yml`。只构建 `app/editor` 的 C++17 / Qt 6 Widgets
编辑器，**不打包 AHK、游戏输入执行器、用户配置或本地截图**。不自动提交、推送或创建 tag。

## 触发与版本契约

维护者将包含编辑器源码、打包脚本和工作流的提交准备好后，自行推送
`editor-vX.Y.Z`，例如 `editor-v0.1.0`；预发布使用 `editor-v0.1.0-rc.1`。
其他 tag 和普通分支 push 不触发。版本三个数字段不允许前导零，范围 0–65535；
不支持 `+build` 元数据。带后缀的版本标为 GitHub prerelease。

CI 给 CMake 传入 `-DD4BOX_RELEASE_VERSION=0.1.0[-rc.1]`，**不在工作流中修改源码**。
`app/editor/CMakeLists.txt` 需要接收这个字符串 cache 变量：

- `project(... VERSION ...)` 和 Windows / Apple 的数字版本字段使用去掉后缀的 `X.Y.Z`。
- `D4BOX_EDITOR_VERSION`、`--version` 保留完整版本，包括 `-rc.1`；期望输出
  `D4BoxEditor 0.1.0-rc.1`。
- 本地不传入该变量时可以继续使用项目默认版本。
- Windows GUI 子系统程序的 CLI 诊断需要在 `subprocess` 重定向 stdout/stderr 时正常输出。

两个打包器都会检查实际 `--version`；CMake 忽略版本变量、CLI 无输出或仍然硬编码旧版本时
发布会失败，而不是上传版本标错的包。编辑器源码和 CMake 契约由编辑器维护者实现。

## 构建矩阵

| 下载平台 | GitHub-hosted runner | 官方 Qt 6.11.1 SDK | 编译方式 |
| --- | --- | --- | --- |
| Windows x64 | `windows-2022` | `win64_msvc2022_64` | VS 2022 x64 / Ninja / MinSizeRel |
| macOS Apple Silicon | `macos-15` | `clang_64`，官方 universal SDK | 原生 arm64 / Ninja / MinSizeRel |
| macOS Intel | `macos-15-intel` | 同一 universal SDK | 原生 x86_64 / Ninja / MinSizeRel |

三种目标都实际运行 CTest，不交叉编译后假装通过目标平台验证。
macOS 设置 deployment target 13.0；SDK 内 universal Mach-O 在打包时裁剪为对应架构。
Windows 基线为 Windows 10 1809 / Windows 11 x64。最低系统版本只是 Qt 支持基线，
本工作流不包含 Windows 10 或 macOS 13 的真机回归。

截至 **2026-11-06** 已核对官方文档：

- [Qt 6.11 支持平台](https://doc.qt.io/qt-6.11/supported-platforms.html)：Windows x64 支持
  MSVC 2022；macOS 13+、Xcode 15+，支持 arm64 和 x86_64。
- [GitHub runner 标签和架构](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)：
  `macos-15` 是 arm64，`macos-15-intel` 是 Intel；不使用已经退役的 macOS 14。
- [Windows 2022 runner 软件](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)：
  包含 VS 2022、Ninja、CMake 和 Inno Setup 6。
- [macOS Intel 软件](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md)
  与 [arm64 软件](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md)：
  Xcode 16.4 满足 Qt 要求。runner 镜像不是不可变的，本次实际工具版本以 CI 日志为准。

macOS 使用固定 `aqtinstall==3.3.0` 下载 Qt 官方 SDK，只安装 `qtbase`，不引入 QML / WebEngine。
Windows 6.11.1 官方仓库已按编译器拆分为 `qt6_6111_msvc2022_64`；实际检查发现 aqt 3.3.0
仍请求不存在的旧 URL。因此 Windows 使用 `package_editor_windows.py --fetch-qt` 直接下载
官方 Qt Base MSVC 2022 x64 归档，固定 SHA-256
`7f97edc3937fec7383eb865e010ed5128155bf9c80a563abca450860f3e9bef5`，通过 runner 的
7-Zip 验证成员路径并提取，写入可重定位 `qt.conf`，验证 Qt 版本。不跳过校验、不使用非官方 SDK。
升级 Qt 时需同步 helper 内归档 URL / SHA-256，不能只改工作流的 `QT_VERSION`。
GitHub actions 固定完整 commit SHA，旁注相应 release tag；升级时应重新核对官方 commit。
没有 Qt 账号、PAT、Apple 签名凭据或额外 GitHub secret 的要求。

## 发行文件

每个成功 tag 的 Release 包含：

- `D4BoxEditor-<version>-windows-x64.zip`：整个目录解压后运行 `D4BoxEditor.exe`。
- `D4BoxEditor-<version>-windows-x64-setup.exe`：真实 Inno Setup 安装程序，按用户安装，
  带开始菜单快捷方式和卸载器，不要求管理员权限。
- `D4BoxEditor-<version>-macos-arm64.zip` / `.dmg`。
- `D4BoxEditor-<version>-macos-x64.zip` / `.dmg`。
- 每个平台的 `*-report.json`：依赖审计、体积和打包器执行的验证结果。
- `qtbase-everywhere-src-6.11.1.tar.xz`：与动态 Qt 库对应的上游源代码归档。
- `SHA256SUMS.txt`：以上所有文件的 SHA-256。

macOS DMG 内是真实 `.app` 和 `/Applications` 快捷方式，不是空壳安装器。
源码归档和许可证之外没有其他非编辑器程序的下载。构建报告不是裸可执行文件体积，
Windows 报告包含 ZIP/安装器体积；macOS 报告来自原有完整 `.app` 测量，DMG 由后续步骤生成。

Windows 通过 `windeployqt --no-compiler-runtime` 部署 Qt，明确复制
`platforms/qwindows.dll` 和 JPEG 插件；CRT **只从** VS 的
`Redist/MSVC/.../x64/Microsoft.VC143.CRT` 目录复制。禁止从开发工具 `bin` 或 System32
收集 CRT。这样 ZIP 不要求预装 Qt / VS / VC++ Runtime。该方式属于 Microsoft 文档中的
app-local redistributable 部署；与系统集中安装相比，CRT 安全更新需要重新发布应用包。
参见 [Microsoft 分发说明](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170)
和 [VS 2022 可分发文件列表](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution)。

## 许可与来源材料

prepare job 下载 Qt 官方源代码归档，用固定 SHA-256
`d9594a31228aa23ad6b531719a29b45f0f3989fe6c136d45767ea179f233c1ac` 校验，
保持源代码相对路径收集 `LICENSES`、LICENSE / COPYING / COPYRIGHT / NOTICE 文本和
`qt_attribution.json`。每个应用包包含这些材料、项目已有 notices 和 `QT-SOURCE.txt`；
macOS 位于 `.app/Contents/Resources/licenses`，Windows 位于 `licenses`。
必需的 LGPLv3 / GPLv3 文本或第三方 attribution 缺失时失败，不能生成无许可证的 release 包。

完整 Qt Base 源码作为同一 Release 的附件，保留第三方源代码和编译材料，不只给一个可能失效的链接。
库保持动态链接，notice 说明可替换兼容 Qt 库以及调试这类修改的逆向工程权限。
保留仓库已有来源声明不意味着 AHK 被打包，也不替整个应用新增许可证授权。
发行维护者仍需确认自己符合 Qt LGPL、第三方和 Visual Studio 的分发条件。
参考 [Qt 许可概述](https://doc.qt.io/qt-6/licensing.html)；这不是完整法律合规审计。

## 发布门禁与权限

1. prepare：验证 tag，运行仓库静态 / Python 检查、打包器离线保护测试，准备许可证和源码。
2. Windows、macOS arm64、macOS x64：全部 configure/build、CTest、打包和实际包启动验证。
3. publish 显式 `needs: [prepare, macos, windows]`，不使用 `always()`，任一失败/取消都会阻止发布。
4. 下载当前 run 的发行 artifact；严格核对六个二进制包、三个报告、一个源码归档，拒绝空文件、
   缺失平台和额外文件；生成 SHA-256 清单。
5. 创建 **draft**，上传文件并比较远端文件名/大小后才转为公开 Release。
   上传失败最多留下不公开的 draft。重跑可以替换同一 draft 的资产；已公开 Release 拒绝覆盖，
   应使用新 tag。意外多出的 draft 资产也阻止公开，维护者需检查后自行清理。

构建 job 只有 `contents: read`，checkout 不持久化凭据；仅最终 publish job
使用 `GITHUB_TOKEN` 的 `contents: write`。不把 editor tag 标为全仓库的 Latest，避免覆盖
未来主应用的发行入口。仓库需要允许 GitHub Actions 运行，并允许最终 job 的写入权限；
组织级限制仍可能阻止发布。没有自动修改 tag、自动提交或自动推送步骤。

## 测试与本地复现

不修改其他脚本或测试的离线检查：

```sh
python3 -Werror scripts/package_editor_windows.py --self-test
python3 -Werror -m py_compile scripts/package_editor_windows.py scripts/package_editor_macos.py
python3 scripts/check_project.py
python3 -m unittest discover -s scripts/tests -v
bash -n app/ahk/tests/run.sh
git diff --check
actionlint .github/workflows/editor-release.yml
```

Windows helper 的离线测试覆盖 tag/数字版本约束、PE 架构与截断输入、SDK 路径越界/链接、许可缺失、
Inno 模板安全约束；不能代替 Windows 原生验证。
MSVC 环境通过 Visual Studio 自带的 Launch-VsDevShell.ps1 初始化，避免路径含空格时
PowerShell / cmd 多层引号破坏命令。打包子进程失败或超时会输出有长度限制的 stdout/stderr，
并保留失败状态。macOS job 失败后会对构建目录和部署目录的原生冒烟启动采集 LLDB 堆栈，
将日志与本次 CI 的编辑器崩溃报告保存为 editor-diagnostics-macos-* artifact；诊断不替代发布门禁。
在 VS x64 开发环境、官方 Qt SDK 和 prepare job 相同的许可证目录已备妥后，
调用 `python scripts/package_editor_windows.py --help` 查看参数；给出 `--iscc` 才生成安装器。
CI 强制提供 `--iscc`，找不到 Inno Setup 或安装失败就阻止发行，不降级为空壳文件。

Windows 打包器会审计每个 PE 文件为 x64、检查导入依赖、清除 Qt / SDK 环境覆盖，
执行原生 `--runtime-info`、`--version`、`--smoke-test`、`--ui-self-test`、`--render`、PNG 背景加载，
在部署目录执行完整 `check_editor_cli.py` 回归（包括无 GUI `validate`），
然后重新解压 ZIP 验证，并静默安装 → 启动 → 卸载真实安装器。
诊断 stdout 使用 `subprocess` 管道捕获，兼容有正确 CLI 实现的 GUI 子系统程序。

macOS 打包器保留原有 Mach-O 依赖、rpath、架构裁剪、签名、原生 Cocoa、PNG/JPEG、
预览和完整体积检查，并运行部署后原生 `--ui-self-test`；CI 额外运行 `check_editor_cli.py`，挂载只读 DMG、解压 ZIP，
检查签名并在清空 SDK 环境后启动两者。发布验证不依赖 `--overlay` 旧的“不支持”行为；
CLI 契约由编辑器维护者同步 `check_editor_cli.py`；CTest / CLI 的新增案例也自动进入发布门禁。

本次新增 release 参数不会改变原有本地 macOS 打包调用：`--licenses-dir` 与
`--release-version` 可省略；但发行时二者都传入并强制校验。所有构建、SDK、包、
测试临时目录放在 `.runtime/editor` 或操作系统临时目录，不提交个人配置和截图。

## 明确限制

- Windows 未代码签名；macOS 只有 ad-hoc 签名，没有 Apple Developer ID 或 notarization。
  SmartScreen / Gatekeeper 可能阻止首次打开。工作流不绕过安全设置，也不承诺双击无提示。
- 测试在装有开发工具的 CI runner 上进行；依赖审计和清理环境不等于干净系统/最低系统版本实测。
- Inno Setup、编译器、GitHub runner 系统镜像和 aqt 的传递依赖不是完全可复现锁定；Qt 版本、
  Qt 源码及 Windows SDK SHA-256、aqt 版本和 actions SHA 固定。后续升级必须重新验证三平台。
- 仅验证编辑器，不验证 Windows 游戏交互；没有 AHK smoke / workflow 执行，也不会发送游戏输入。
- 本地语法和打包测试不等于 GitHub Actions 已成功运行。第一次真实 tag 的三平台执行与
  Release 上传结果需要维护者在 GitHub 上确认；此实现不会自行创建 tag 来试发。
