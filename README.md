# d4box

暗黑 IV 的 Windows 辅助工具，使用 AutoHotkey v2。主程序内直接配置按键宏、流程快捷键、物品处理步骤和检测区域，不需要独立编辑器。

源自 D4KeyHelp，由本仓库直接维护；来源和许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。

## 运行

发行版提供 Windows x64 独立 EXE 和便携 ZIP，无需安装 AutoHotkey、Qt 或 Visual Studio。将 EXE 放在可写目录后启动。窗口标题显示版本，底部“检查更新”查询主程序稳定版，发现新版后可打开发布页下载。

源码运行需要 AutoHotkey v2。可将官方便携解释器放入 `.runtime/autohotkey/v2/`，然后运行：

```powershell
.\scripts\start-windows.ps1
# 或指定解释器
.\scripts\start-windows.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

## 主界面

- **流程快捷键**：默认 F3 绑定可改键、改流程，不能删除；可新增其他键位执行不同已保存流程。执行中再次按任一流程键取消，长按不重复触发。
- **按键宏设置**：四套预设、技能与左右键宏、间隔和延迟、宏启停键（默认 F1）、开始／停止与保存。
- **公共底部**：实时状态、流程名称与当前步骤、停止／取消、通知、检查更新、主题与语言。切换页面不改变热键范围。
- **编辑流程**：配置背包遍历或鼠标指向单个物品、动作、等待、视觉检查及“检测到后点击接受”。
- **检测区域库**：截图框选模板、命名、出现／关闭验证，供多个流程引用；保留旧单点颜色检测。

血球、辅助喝药、翻滚、强移、自动移动、点击暂停和罗盘辅助已裁剪。旧 INI 的这些开关不再执行。游戏前台按 Ctrl+Alt+F8 可查看窗口诊断。

使用前阅读 [流程说明](docs/cube-workflows.md) 和 [Windows 验证清单](docs/windows-validation.md)。内置流程保留旧坐标和延时；离线测试、编译包启动检查不能代替游戏内标定与真实操作验证。

## 配置和更新

`settings.ini`、`debugd4*.log` 写在程序旁；`.runtime/` 保存自定义流程、快捷键绑定、检测模板和诊断。源码运行时目录为 `app/ahk/`，EXE 运行时为 EXE 所在目录。

升级时先退出程序，只替换 EXE，保留 `settings.ini` 和整个 `.runtime/`。从源码迁移到 EXE 时，可在退出后将这两项从 `app/ahk/` 复制到 EXE 旁。独立 EXE 文件名可改为 `D4Box.exe`，方便保留快捷方式。

更新检查只查询 `d4box-vX.Y.Z` 稳定主程序版本，跳过旧 editor、草稿、预发布和资产不完整的版本。查询异步进行，无需 GitHub 登录；失败可重试。不会自动下载或覆盖正在运行的程序。

## 开发与构建

```text
app/ahk/        AHK 入口、界面、执行器、配置与 Windows 测试
scripts/        源码启动、工具下载、EXE 构建、仓库检查
.github/        AHK 构建与发布 Actions
docs/           架构、流程、发布与验证说明，及上游来源快照
LICENSES/       第三方许可
```

静态检查（Python 3.10+）：

```sh
python scripts/check_project.py
python -m unittest discover -s scripts/tests -v
bash -n app/ahk/tests/run.sh
git diff --check
```

Windows Git Bash 的原生离线回归：

```sh
bash app/ahk/tests/run.sh all
```

Windows PowerShell 构建独立包（下载并校验官方编译工具，无需 VS）：

```powershell
.\scripts\build-windows.ps1
```

产物位于 `.runtime/dist/`：版本化 EXE、ZIP 和 SHA256SUMS.txt。版本取自 `app/ahk/core/Version.ahk`；Action 在 main/PR 时构建测试，推送匹配的 `d4box-vX.Y.Z` 标签才发布。[发布说明](docs/ahk-release.md) 介绍完整过程和检查边界。

## 独立 editor 归档

独立 Qt editor 的源码、设计、开发脚本和发布流程保存在带注释标签 `archive/editor-before-ahk-only`，对应提交 `3ec4e87`。旧 `editor-v*` 标签保持不变。主分支只维护 AHK 应用；内置流程编辑器和检测区域库保留。

可在不改变主分支工作区的情况下恢复研究：

```sh
git worktree add --detach ../d4box-editor-archive archive/editor-before-ahk-only
```
