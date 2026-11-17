# 开发指引

[项目首页](../README.md) · [文档目录](README.md) · [用户手册](guide/README.md)

主分支入口为 `app/ahk/D4Box.ahk`。开发约定见仓库 [AGENTS.md](../AGENTS.md)。

## 源码运行

准备 AutoHotkey v2 后，在仓库根目录运行：

```powershell
.\scripts\start-windows.ps1
# 或指定本机实际存在的解释器
.\scripts\start-windows.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

默认优先寻找 `.runtime\autohotkey\v2\AutoHotkey64.exe`。详情见[安装文档](guide/installation.md#source)。源码用户数据位于 `app/ahk`，不得提交到仓库或打入发布包。

## 检查

仓库和文档静态检查需要 Python 3.10+：

```sh
python scripts/check_project.py
python scripts/check_docs.py
python -m unittest discover -s scripts/tests -v
bash -n app/ahk/tests/run.sh
git diff --check
```

Windows Git Bash 上的 AHK 离线回归：

```sh
bash app/ahk/tests/run.sh all
```

这些检查不会证明游戏坐标、检测模板或配方结果正确。实际范围和人工步骤见[Windows 验证清单](windows-validation.md)。

## 构建独立 EXE

Windows PowerShell：

```powershell
.\scripts\build-windows.ps1
```

脚本下载并校验固定版本的官方工具，不需要 VS。产物位于 `.runtime/dist/`，包括版本化 EXE、ZIP 和 `SHA256SUMS.txt`。唯一版本来源为 `app/ahk/core/Version.ahk`。

Actions 在 main／PR 时构建测试，匹配的 `d4box-vX.Y.Z` 标签触发发布。构建成功不代表已公开发行；发布约定见[构建与发布](ahk-release.md)。

## 维护图文手册

帮助页位于 `docs/guide/`，图片统一放在 `docs/guide/images/`。功能变化时同步修改相关步骤、按钮名和截图；截图使用隔离示例数据，不采集真实个人配置。

在可交互的 Windows 桌面生成图片：

```powershell
.\scripts\capture-help.ps1
# 或指定解释器
.\scripts\capture-help.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

脚本会短暂显示并切换演示窗口，运行期间不要遮挡它们；不要在游戏流程执行期间运行。用途、限制和完整图片清单见[截图维护说明](guide/images/README.md)。

`check_docs.py` 检查当前项目文档的本地链接、锚点、图片路径、文件名大小写及 PNG 基本信息，并确认手册引用了全部截图；不联网检查外部网站，也不把上游 README 快照当作当前帮助文档。

<a id="editor-archive"></a>
## 独立 editor 归档

Qt editor 源码、设计、开发脚本和发布流程保存在带注释标签 `archive/editor-before-ahk-only`，对应提交 `3ec4e87`；旧 `editor-v*` 标签保留。主分支只维护 AHK 应用及其内置编辑器。

可另建工作区研究历史版本：

```sh
git worktree add --detach ../d4box-editor-archive archive/editor-before-ahk-only
```
