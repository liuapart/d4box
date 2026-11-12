# d4box 开发约定

本文件描述仓库当前约束。实现细节以源码和 CI 工作流为准；发现文档与代码不符时，同一改动中修正文档。不要把设计草案、测试模拟或 CI 冒烟称为游戏内验证。

## 项目边界与来源

- `app/ahk` 是主仓库直接维护的 AutoHotkey v2 应用，不再使用 submodule。保留 `THIRD_PARTY_NOTICES.md`、`LICENSES/D4KeyHelp-MIT.txt` 和 `docs/upstream/` 的来源声明；不要把上游 README 当作本项目现行使用说明。
- Windows 主程序唯一入口为 `app/ahk/D4Box.ahk`；一个进程管理 GUI、热键、宏、魔盒流程和诊断。新增项目级 AHK 辅助函数使用 `D4Box_` 前缀。`app/ahk/utils/Lang.ahk` 必须在依赖文案的全局变量之前加载；界面文案同时维护中文、英文。
- `app/editor` 是独立的 C++17 / Qt 6 Widgets 编辑器，使用 QPainter、全屏穿透绘制层与局部交互窗口；不引入 QML / WebEngine，也不为更新器随意增加 Qt Network。它只编辑、校验并保存 **draft JSON**，不识别真实物品、不发送游戏输入。AHK 目前读取 INI，不消费编辑器 JSON；两者保持解耦。
- macOS 用于主要开发和离线调试；AHK、系统输入及真实暗黑 IV 交互必须在 Windows 验证。Mac 截图底图模式不是实机覆盖或游戏验证。

## AHK 与配置

- 保持迁入设置兼容。应用设置和日志仍在入口脚本旁：`app/ahk/settings.ini`、`debugd4*.log`；新诊断产物在 `app/ahk/.runtime/`。自定义魔盒配置存于 `app/ahk/.runtime/cube-workflows.ini`，仓库内置配置是 `app/ahk/config/cube-workflows.ini`。这些运行时文件不得提交。
- 魔盒配方优先以 INI 数据和现有流程编辑器扩展，通用执行器负责动作；避免重新增加配方专用的 `TransmuteXXX` 函数。不要声称独立 Qt 编辑器的 JSON 已接入 AHK。
- 新增自动点击或消耗材料的动作需检查游戏/窗口前置状态、目标、执行结果、超时、取消、焦点与客户区变化。固定坐标和延时只能是既有流程兼容或辅助参数，不能作为安全判断的全部依据。
- 不要静默改变 F1/F3 等既有热键、原配置语义或游戏输入行为。修改这些行为时同步更新用户文档和 Windows 测试。

## 编辑器实现与开发

- 编辑器以小体积和现有 Qt Widgets 模式为准，优先使用现有 `WorkflowDocument`、`OverlayLayer`、`NativeOverlay`、`EditorWindow`、`WorkflowPanel`、`EditorTheme` 等职责边界。绘制层保持穿透，只让局部控件接收输入；不要把调试底图写入共享 JSON。
- JSON Schema、模板和 `validate` 命令在 `app/editor/`；保存始终是草稿，结构校验与 `--for-run` 静态前提校验不等于可在游戏中执行。点位、背包和步骤的更改应维持稳定 ID、引用保护、撤销、原子保存与外部修改冲突检查。
- Qt SDK、CMake 构建、打包产物、临时样本放 `.runtime/editor/`。本地开发入口 `./dev.sh`（实质调用 `scripts/dev_editor.py`），可用 `--build-only` 只构建和运行 CTest；`--demo` 使用本机忽略的底图。`--qt-prefix` 优先于 `QT_PREFIX` 和仓库内 SDK；不会替用户自动下载 SDK。
- 修改编辑器后构建并运行 `ctest --test-dir <build-dir> --output-on-failure`。涉及 UI、拖拽、坐标、JSON 或更新器时，增补相邻测试；GUI 自测只投递本程序事件，不能代替跨进程鼠标验证。
- 完整交付体积以 `scripts/package_editor_macos.py` 和 `scripts/package_editor_windows.py` 的完整包/报告为准，不能拿裸可执行文件大小替代。精简 Qt 插件或运行库前先检查原生依赖、目标架构、插件搜索路径、启动/渲染/安装测试与许可证材料。

## 检查层级

- 每次提交前在仓库根目录运行：`python3 scripts/check_project.py`、`python3 -m unittest discover -s scripts/tests -v`、`bash -n app/ahk/tests/run.sh`、`git diff --check`。修改发布或打包时额外运行 `python3 -Werror scripts/package_editor_windows.py --self-test`、相关 `py_compile` / `sh -n`；编辑器改动需运行 CTest。
- Windows Git Bash 上运行 `bash app/ahk/tests/run.sh validate`、`bash app/ahk/tests/run.sh workflow`、`bash app/ahk/tests/run.sh smoke`。血球回归还需本地截图样本、Pillow 及 `globe` 测试；样本和生成的 `.bin` 不入库。不要在 macOS 上宣称 AHK 通过。
- 区分四层证据：仓库静态检查、编辑器/模拟器离线测试、Windows/macOS CI 原生打包与启动、真实 Windows/游戏人工验证。每层只报告实际覆盖的内容；CI 不验证游戏操作、最低系统版本或干净机器安装。

## Release 与更新

- `.github/workflows/editor-release.yml` 匹配 `editor-v*` 标签，prepare job 再严格校验 `editor-vX.Y.Z`（可带预发布后缀）；发布物只含编辑器，不含 AHK。发布前确认远端最新 tag/Release 和工作树，更新 `app/editor/CMakeLists.txt` 的默认版本与工作流发行说明，再跑上述检查。不要复用、移动已发布标签。
- 维护者明确要求发布时：只提交相关文件，先推送提交，再新建并推送唯一的带注释标签；用 `gh run` 跟踪 prepare、Windows x64、macOS arm64、macOS x64 和 publish 至最终结论。失败时读取失败步骤日志，修复后按发布规则重跑；不得把 tag 已推送或 draft 已建立称作发布成功。
- CI 的 prepare 检查标签、仓库、许可证和固定 SHA-256 的 Qt 源码；三平台用官方 Qt 6.11.3 Base/SVG、MinSizeRel 构建并跑 CTest、打包、原生启动/替换检查。publish 只有在全部目标成功后才校验资产集合和 SHA256SUMS、上传 draft、核对远端文件名/大小并公开。构建 job 只读令牌，publish 才有写权限。
- 每次发布后核对 Release 不是 draft、资产齐全、`SHA256SUMS.txt` 存在、GitHub 最新稳定版指向新标签。Windows 提供 x64 ZIP 与 Inno Setup EXE；macOS arm64/x64 各有 ZIP 与 DMG；各平台另有报告，Qt 源码归档也在 Release。旧发行版不覆盖，以新标签修复。不要把未签名的 Windows 包或 ad-hoc 签名的 macOS 包表述为已公证/可信签名。
- 当前公开仓库的编辑器更新器使用系统 `curl` 查询公开 Release，无需 `gh` 登录；下载后核对同一 Release 的 SHA-256，再由退出后的辅助脚本替换或安装、重启。0.2.4 及更旧二进制仍可能要求 `gh`，无法自更新时需先手动安装新版。SHA-256 不等于独立代码签名。
- Qt 版本、归档 SHA、SDK/许可来源、插件列表或发布工作流变更时，同步审查 `docs/editor-release.md`，并以 CI 的 Windows/macOS 实际运行结果为准。不要提交 `.runtime`、账号、凭据或个人截图。

## 文档与协作

- 当前用户说明以根 `README.md`、`app/editor/README.md` 和 `docs/` 中的架构、魔盒、验证、构建、发布文档为准。过时设计方案可删除；来源声明和上游快照必须保留。新增命令应可从仓库根目录复制执行，注明平台、先决条件和验证边界，避免写会随版本变化失效的“当前断言数/包体”。
- 若任务明确要求并发 agent，拆分不重叠的文档/代码写入范围：简单、边界清楚的任务用 `gpt-6.1-sol`，跨模块或发布契约梳理等复杂任务用 `gpt-6-astra`。主任务负责整合、冲突检查和最终验证；不要让多个 agent 同时编辑同一文件。
