# d4box

暗黑 IV 辅助工具。在 macOS 开发，在 Windows 运行和验证。

项目已将 D4KeyHelp 的 AHK 源码迁入主仓库，由 d4box 直接维护，不再使用 submodule。来源与许可记录见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 已迁入的能力

- 图形化配置、四组预设、中英文和明暗主题。
- 技能与鼠标宏：连点、维持 BUFF、按住，以及自定义按键和执行间隔。
- 翻滚、喝药、强制移动、鼠标移动、罗盘辅助。
- 血球颜色与液面识别、条件喝药和护盾处理。
- 装备格颜色识别、F3 魔盒嬗变、可选黄装升级流程。
- 窗口切换暂停、手动点击暂停、退出时释放按键、配置持久化和日志轮转。
- d4box 窗口诊断：游戏在前台时按 **Ctrl+Alt+F8**，显示并记录客户区位置和尺寸。

迁入意味着代码已纳入本仓库，尚未完成本项目的 Windows 实测。原有坐标、取色和固定延时限制仍然存在；通用 OCR、跨布局定位和滚动找目标还没有实现。

## 目录

```text
app/ahk/
  D4Box.ahk        应用入口
  core/           状态、定时器、窗口和宏控制
  functions/      技能、鼠标、魔盒、血球和诊断
  gui/            界面控件
  hotkeys/        热键
  utils/          配置、日志、多语言
  tests/          迁入的 Windows 冒烟与血球测试
scripts/          Windows 启动器、Mac 项目检查
docs/             架构、验证清单、上游文档快照
LICENSES/         第三方许可文本
```

## Mac 开发

需要 Git 和 Python 3.9+。普通克隆即可获得全部应用源码：

```sh
git clone git@github.com:liuapart/d4box.git
cd d4box
python3 scripts/check_project.py
bash -n app/ahk/tests/run.sh
git diff --check
```

Mac 上的检查覆盖仓库结构和 AHK 引用路径，不执行 Windows API，也不验证游戏交互。

## Windows 运行

安装 AutoHotkey v2，在 PowerShell 中执行：

```powershell
.\scripts\start-windows.ps1
```

也可以明确指定解释器，或用 AutoHotkey v2 直接打开 `app\ahk\D4Box.ahk`：

```powershell
.\scripts\start-windows.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

启动后只有一个 d4box 进程。默认 F1 启停宏，F3 执行魔盒流程，Ctrl+Alt+F8 显示窗口诊断。先阅读 [Windows 验证清单](docs/windows-validation.md)，在游戏中验证配置和识别位置后再启用自动操作。

旧版双进程入口及 `-SkipUpstream` 参数已移除。迁移后请先退出仍在运行的旧 D4KeyHelp 和扩展脚本。

## 配置与日志

为保留迁入代码行为，应用设置仍写入 `app/ahk/settings.ini`，日志为同目录的 `debugd4.log` 和 `debugd4.old.log`。窗口诊断日志在 `app/ahk/.runtime/window-diagnostics.log`。所有运行产物均被 Git 忽略。

原有 D4KeyHelp 设置可以在退出程序后手动复制到新的 `app/ahk/settings.ini`。测试脚本使用自己的 `tests/settings.ini`，不会覆盖应用配置。

## Windows 测试

在 Windows Git Bash 中运行：

```sh
bash app/ahk/tests/run.sh validate
bash app/ahk/tests/run.sh smoke
```

血球回归使用上游本地截图样本，这些图片和生成的 `.bin` 不在原仓库中，因此当前克隆不能直接完成血球回归。补齐对应样本并安装 Pillow 后，才可运行 `bash app/ahk/tests/run.sh globe --export`。详见验证清单。

后续可以直接修改 `app/ahk` 中的源码，重构方案见 [架构说明](docs/architecture.md)。
