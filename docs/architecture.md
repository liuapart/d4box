# 架构与实现边界

Windows 唯一入口 `app/ahk/D4Box.ahk`，源码运行由 `scripts/start-windows.ps1` 启动；发布版由 Ahk2Exe 将同一入口及解释器打成独立 x64 EXE。

## 模块

- `core/`：状态、窗口、定时器、按键宏、流程快捷键、版本与更新检查。
- `gui/`：主界面两页、流程编辑、检测区域库、版本与更新窗口。宏配置与启动控件只属于宏页，公共底部承载实时状态和停止／取消入口。
- `CubeProfiles.ahk`：内置与用户 INI、稳定 ID、校验和保存；`CubeRunner.ahk`：通用动作执行、窗口/取消/超时检查及单物品鼠标归位。
- `DetectionRegions.ahk`：命名区域、BMP 模板、匹配和测试状态；流程引用稳定区域 ID，由执行器决定等待或点击。
- `WorkflowHotkeys.ahk`：全局流程绑定、默认 F3 迁移、键位冲突、原子保存及注册失败回滚。绑定与四套战斗宏配置独立。
- `Version.ahk`：版本与发布仓库；`Updates.ahk`：异步 WinHTTP、严格 JSON、分页和稳定版本筛选；`UpdatesGUI.ahk`：查询状态与发布页入口。更新检查不修改程序或配置。
- `Distribution.ahk`：第三方资源提取及编译包 `--self-test`。编译版通过 FileInstall 内嵌默认流程和许可；不依赖源码目录或系统 AHK。

## 数据

入口旁的 `settings.ini` 与旧日志路径保持兼容；用户流程、绑定、检测模板和诊断在入口目录 `.runtime/`。源码版入口目录为 `app/ahk`，编译版为 EXE 所在目录。所有个人配置、截图、模板和工具均被 Git 忽略，不进入发布包。

编译版每次启动刷新 `.runtime/builtin/cube-workflows.ini`，该目录只存内置默认值，用户数据路径不受影响。构建脚本使用新的暂存目录，只复制明确的源码目录和许可，再验证单独 EXE 的界面与内置流程可读取。

## 归档与验证

独立 Qt editor 已从主分支移除，保存在 `archive/editor-before-ahk-only`；AHK 内置流程编辑器继续维护。来源快照和许可证保留。

静态/Python 检查可跨平台运行；AHK 语法、模型、GUI 与 EXE 探测必须在 Windows 执行。CI 不验证游戏输入、分辨率/DPI 兼容性或干净机器安装。详见 [Windows 清单](windows-validation.md)、[流程说明](cube-workflows.md) 和 [发布说明](ahk-release.md)。
