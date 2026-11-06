# d4box

暗黑 IV 辅助工具扩展项目。在 macOS 开发，在 Windows 验证 AutoHotkey 和游戏交互。

上游 [D4KeyHelp](https://github.com/duzefu/D4keyHelp) 作为 Git submodule 固定在特定提交；自己的代码保存在主仓库，不改动上游源码。

## 当前内容

- `vendor/D4keyHelp/`：上游 submodule。
- `extensions/ahk/`：独立运行的 AutoHotkey v2 扩展。
- `scripts/start-windows.ps1`：启动上游与扩展，也可只启动扩展。
- `scripts/check_project.py`：仅使用 Python 标准库的项目完整性检查，可在 Mac 运行。
- `docs/`：架构边界及 Windows 验证清单。
- `.runtime/`：本机运行日志，不纳入版本控制。

初始扩展只提供窗口诊断：游戏在前台时按 **Ctrl+Alt+F8**，显示并记录游戏客户区的位置和尺寸。它不自动点击、不滚动、不做 OCR。

## 克隆与 Mac 开发

```sh
git clone --recurse-submodules git@github.com:liuapart/d4box.git
cd d4box
python3 scripts/check_project.py
git diff --check
```

如果克隆时遗漏 submodule：

```sh
git submodule update --init --recursive
```

Mac 上可以编辑 AHK、维护流程设计及运行项目检查。Mac 上的检查不会执行 AHK，也不代表游戏交互已经通过验证。

## Windows 启动

安装 AutoHotkey v2，在 PowerShell 中执行：

```powershell
.\scripts\start-windows.ps1
```

启动器会尝试常见安装位置；也可明确指定解释器：

```powershell
.\scripts\start-windows.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

仅启动自己的扩展：

```powershell
.\scripts\start-windows.ps1 -SkipUpstream
```

如本机执行策略阻止 `.ps1`，可直接用 AutoHotkey v2 分别运行 `vendor\D4keyHelp\D4keyHelp.ahk` 和 `extensions\ahk\Main.ahk`。

两个程序各自拥有托盘图标和退出入口。窗口诊断日志在 `.runtime/window-diagnostics.log`；上游仍按自身逻辑保存设置。

## 扩展边界

目前采用独立进程，优先保持上游更新简单。新增识别、定位和滚动搜索可以在主项目实现，但尚未开发这些功能。两个程序没有共享调度器或 IPC；未来增加自动输入前，必须先设计输入互斥，避免两边同时控制鼠标键盘。

修改上游既有 UI 或点击流程，需要上游公开接口、专门适配或维护补丁，submodule 本身不提供插件机制。详见 [架构说明](docs/architecture.md) 和 [Windows 验证清单](docs/windows-validation.md)。

## 更新上游

主项目记录的是上游的具体提交，而不是自动跟随最新版。更新时先在 submodule 中拉取并切换到明确的提交，审查差异并完成 Windows 验证，然后提交主仓库的 submodule 指针变化。

不要把 `vendor/D4keyHelp` 的文件复制进主仓库，也不要在尚未验证时把上游更新视为兼容。
