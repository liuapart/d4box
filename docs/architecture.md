# 架构与实现边界

## 两条独立链路

```text
Windows 主程序：scripts/start-windows.ps1 → app/ahk/D4Box.ahk
  → GUI / 热键 / 战斗宏 / 背包识别 / 魔盒 INI 流程 / 血球 / 窗口诊断

macOS + Windows 编辑器：app/editor/src/main.cpp → WorkflowDocument
  → EditorWindow / WorkflowPanel / OverlayLayer / NativeOverlay
  → app/editor/schema/workflow-v1.schema.json 对应的 draft JSON
```

AHK 是一个 AutoHotkey v2 进程，源码由主仓库直接维护；无需 submodule。编辑器是独立 C++17 / Qt 6 Widgets 程序，**没有与 AHK 运行时连接**。编辑器的 JSON 目前不被 AHK 读取；AHK 仍从内置 `app/ahk/config/cube-workflows.ini` 和用户 `app/ahk/.runtime/cube-workflows.ini` 执行魔盒配方。不要把编辑器静态 `--for-run` 校验理解为流程已经可运行。详见 [魔盒流程](cube-workflows.md) 与 [编辑器说明](../app/editor/README.md)。

## AHK 模块

- `D4Box.ahk` 依次加载语言、全局状态、工具、核心、GUI、功能模块和热键；语言表必须先于依赖它的全局变量。
- `core/` 管理定时器、宏、窗口与热键；`functions/` 包含技能、鼠标、背包、魔盒、血球和诊断；`gui/` 是现有 Windows 控件界面。历史遗留的全局状态和 GUI/业务耦合仍存在，修改需关注加载顺序与配置兼容。
- 魔盒已分为 `CubeProfiles.ahk` 的 INI 读写及校验、`InventoryScanner.ahk` 的格位扫描、`CubeRunner.ahk` 的通用动作执行与 `CubeWorkflowGUI.ahk` 的配置界面。自定义流程有视觉前置/结果检查；内置兼容流程仍保留原坐标和延时。
- 设置与旧日志沿用入口目录；新诊断和自定义魔盒配置在该目录的 `.runtime`，测试目录使用独立配置。这些运行产物被 Git 忽略。

## 编辑器模块

- `WorkflowDocument` 管理 JSON、稳定 ID、引用/结构校验和保存事务；`validate PATH` 可在无 GUI 环境运行，`--for-run` 只检查静态前提。
- `OverlayLayer` 绘制全屏穿透标记及局部点位、背包拖柄；`NativeOverlay` 包含 macOS AppKit/Carbon 与 Windows Win32 适配；`EditorWindow` 和 `WorkflowPanel` 管理编辑、撤销、无边框窗口及草稿会话。
- 截图底图只用于本机离线调试，不进 JSON。编辑器不会扫描真实背包、自动跟踪游戏窗口、执行点击或提供运行时 Debug。
- 更新器访问公开 GitHub Release、下载当前平台文件及 SHA-256 清单，验证后交给退出后的替换脚本；它只更新编辑器。构建和发包见 [构建与包体](editor-build-size.md) 和 [发布流程](editor-release.md)。

## 验证与来源

Mac 的仓库检查、编辑器 CTest 和截图调试不能验证 AHK。编辑器 Windows/macOS CI 原生打包和启动也不能验证游戏输入、真实跨进程穿透、干净机器或最低系统版本。AHK 需在 Windows 跑加载、模拟流程与 GUI 冒烟测试；坐标、颜色和实际消耗动作仍需游戏内人工确认，见 [Windows 清单](windows-validation.md)。

迁入的上游来源和许可证见 [第三方声明](../THIRD_PARTY_NOTICES.md)、`../LICENSES/` 与 `upstream/`。`docs/upstream` 是来源快照，不是本项目现行启动或发行说明。
