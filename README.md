# d4box

暗黑 IV 辅助工具：Windows AHK 主程序与独立的 macOS/Windows 配置编辑器。在 macOS 开发，游戏交互在 Windows 验证。

主界面分为“流程快捷键”和“按键宏设置”两页。默认流程绑定初始为 F3，可修改键位和对应流程，但不能删除；可新增其他键位执行不同的已保存流程。绑定独立于四套宏配置，修改后立即保存。血球检测、辅助喝药、翻滚、强移、自动移动、点击暂停和罗盘辅助已移除，旧 INI 中的这些开关不再加载或执行。

项目已将 D4KeyHelp 的 AHK 源码迁入主仓库，由 d4box 直接维护，不再使用 submodule。来源与许可记录见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

宏启停键配置、开始／停止按钮和“保存宏设置”位于“按键宏设置”页；公共底部只显示运行状态、停止／取消入口、通知及主题／语言。切换页面不影响热键生效。执行流程时底部显示流程名称和当前步骤；原有宏暂停期间会明确提示。空闲时不显示公共停止按钮，点击公共“取消流程”会让战斗宏保持暂停。

## 已迁入的能力

- 图形化配置、四组预设、中英文和明暗主题。
- 技能与鼠标宏：连点、维持 BUFF、按住，以及自定义按键和执行间隔。
- 装备格颜色识别、可配置的魔盒步骤执行器；内置背包嬗变及可选黄装升级流程。
- 配方流程编辑器：复制内置流程、编辑步骤、采集位置与颜色、预览并保存；默认 F3 执行绑定流程，可改键并增加其他流程快捷键。
- 检测区域库：命名、截图框选模板、测试面板出现/关闭，保存后供流程的就绪检查、等待检测和可选接受共用；旧单点颜色检查保持兼容。
- 窗口切换暂停、手动点击暂停、退出时释放按键、配置持久化和日志轮转。
- d4box 窗口诊断：游戏在前台时按 **Ctrl+Alt+F8**，显示并记录客户区位置和尺寸。

AHK 与游戏交互尚无本项目的真实 Windows/游戏内验收记录。内置嬗变保留旧版坐标、取色与延时；新流程支持颜色检查，但通用 OCR、跨布局定位和滚动找目标还没有实现。编辑器的 Windows CI 构建/启动测试不等于 AHK 或游戏验证。

## 目录

```text
app/ahk/
  D4Box.ahk        应用入口
  core/           状态、定时器、窗口和宏控制
  functions/      技能、鼠标、魔盒和诊断
  gui/            界面控件
  hotkeys/        热键
  utils/          配置、日志、多语言
  config/         内置魔盒流程数据
  tests/          Windows 冒烟、流程与快捷键测试
app/editor/       独立 Qt 配置编辑器、Schema、模板与 CTest
scripts/          启动器、开发/打包脚本与仓库检查
docs/             架构、验证清单、上游文档快照
LICENSES/         第三方许可文本
```

## Mac 开发

需要 Git 和 Python 3.10+。普通克隆即可获得全部应用源码：

```sh
git clone git@github.com:liuapart/d4box.git
cd d4box
python3 scripts/check_project.py
python3 -m unittest discover -s scripts/tests -v
bash -n app/ahk/tests/run.sh
git diff --check
```

Mac 上的检查覆盖仓库结构和 AHK 引用路径，不执行 Windows API，也不验证游戏交互。

## 编辑器本地开发启动

在仓库根目录运行（只构建独立 Qt 编辑器，不启动 AHK）：

```sh
./dev.sh                        # Debug 增量编译 → CTest → 透明覆盖编辑器
./dev.sh --demo                 # 使用本机已保存的游戏底图
./dev.sh --demo -- --template   # 带底图打开内置嬗变模板
./dev.sh --build-only           # 编译和测试，不启动界面
./dev.sh -- --background "/路径/游戏截图.png"
./dev.sh -- ./workflows/example.d4workflow.json
```

首次需要 Xcode Command Line Tools、Python 3.10+、CMake/Ninja（`brew install cmake ninja`）和 Qt SDK。
SDK 未安装时运行 `python3 scripts/fetch_editor_qt_macos.py`；启动脚本不会自动下载。
`--demo` 使用本机忽略目录中的 `.runtime/editor/backgrounds/transmute-game-original.png`，图片不会随仓库分发。
SDK 选择、参数及 Windows 入口见 [编辑器开发说明](app/editor/README.md#本地开发入口)。

## Windows 运行

主程序源码启动器已包含在仓库中。将 AutoHotkey v2 官方便携包解压到 `.runtime/autohotkey/v2/`（其中应直接包含 `AutoHotkey64.exe`），或安装系统版后，在 PowerShell 中执行：

```powershell
.\scripts\start-windows.ps1
```

启动器优先使用项目内便携版，再探测系统安装目录。`.runtime/` 已被 Git 忽略，解释器不会提交。也可以明确指定实际存在的解释器路径，或用 AutoHotkey v2 直接打开 `app\ahk\D4Box.ahk`：

```powershell
.\scripts\start-windows.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

启动后只有一个 d4box 进程。默认 F1 启停宏，默认 F3 执行绑定的魔盒流程（键位与流程可修改），Ctrl+Alt+F8 显示窗口诊断。先阅读 [Windows 验证清单](docs/windows-validation.md)，在游戏中验证配置和识别位置后再启用自动操作。

Windows 编辑器开发启动器会自动加载 VS 2022 x64 编译环境，再构建、运行 CTest 并打开开发版。无需完整 VS IDE：安装 **Visual Studio 2022 Build Tools** 的“使用 C++ 的桌面开发”（含 MSVC v143 x64/x86 与 Windows SDK），并准备 Python 3.10+、CMake、Ninja 和 Qt 6 MSVC Base + SVG SDK；普通 PowerShell 即可运行。Build Tools 的 C++ CMake 工具也提供 Ninja。已按 CI 方式把 SDK 安装到仓库 `.runtime/editor/qt/6.11.3/msvc2022_64` 时：

```powershell
.\scripts\start-editor-windows.ps1 -- --template
```

`-QtPrefix` 可指定实际存在的外部 SDK 目录（例如 `-QtPrefix 'C:\Qt\6.11.3\msvc2022_64'`）；省略时沿用 `QT_PREFIX` 或仓库内已准备的 SDK。编辑器参数放在 `--` 后；例如 `-- --background 'C:\screenshots\cube.png'`。此脚本用于源码开发，不是发布包启动器；独立发布包只含编辑器，不含 AHK 主程序。

旧版双进程入口及 `-SkipUpstream` 参数已移除。迁移后请先退出仍在运行的旧 D4KeyHelp 和扩展脚本。主程序目前通过 AutoHotkey v2 解释器运行源码；独立 EXE 发布流程尚未接入本仓库。

## 配置与日志

为保留迁入代码行为，应用设置仍写入 `app/ahk/settings.ini`，日志为同目录的 `debugd4.log` 和 `debugd4.old.log`。窗口诊断日志在 `app/ahk/.runtime/window-diagnostics.log`。所有运行产物均被 Git 忽略。

原有 D4KeyHelp 设置可以在退出程序后手动复制到新的 `app/ahk/settings.ini`。测试脚本使用自己的 `tests/settings.ini`，不会覆盖应用配置。

## Windows 测试

在 Windows Git Bash 中运行：

```sh
bash app/ahk/tests/run.sh validate
bash app/ahk/tests/run.sh smoke
bash app/ahk/tests/run.sh workflow
```


后续可以直接修改 `app/ahk` 中的源码，重构方案见 [架构说明](docs/architecture.md)。

## 配置其他配方

点击主界面的“配方流程…”，复制内置配置后编辑步骤，支持背包筛选和单次物品（鼠标指向）两种来源。“检测区域库”中通过出现/关闭测试并保存的区域，可以在步骤的“检测节点”中按名称选择，也能设为就绪检查。流程的操作说明、检查点要求与当前限制见 [魔盒流程说明](docs/cube-workflows.md)。


## 独立配置编辑器（不接 AHK）

`app/editor` 使用 C++17 / Qt 6 Widgets / QPainter：全屏透明穿透绘制、局部可拖点位、
3×11 背包八向缩放、按需浮动步骤面板、步骤/检查编排、撤销与草稿 JSON 原子保存。
截图调试与真实桌面覆盖分开；不发送游戏输入，不识别真实物品，现有 AHK 仍读取 INI。

- [编辑器使用说明](app/editor/README.md)
- [构建、验证与实际包体](docs/editor-build-size.md)
- [Tag 构建与 Release 安装包](docs/editor-release.md)：`editor-vX.Y.Z` 触发 Windows x64、Mac arm64/x64，全部成功才发布。

发布包只含编辑器：Windows x64 ZIP/安装 EXE，Mac arm64/Intel 各有 ZIP/DMG；不包含 AHK 主程序。编辑器可从“更多 → 检查更新”读取公开 Release，无需 GitHub CLI 登录；0.2.4 及更早版本因旧更新器可能需要先手动升级。Windows 包未签名、Mac 包仅 ad-hoc 签名且未公证；CI 通过不等于干净机器安装或游戏内验证。
