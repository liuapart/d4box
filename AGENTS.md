# d4box 开发约定

本文件描述仓库当前约束。实现细节以源码和 CI 为准；同一改动中修正文档。不要把离线测试或 CI 冒烟称为游戏内验证。

## 项目边界

- 主分支只维护 Windows AutoHotkey v2 应用，唯一入口 `app/ahk/D4Box.ahk`。一个进程管理 GUI、热键、宏、流程、检测区域和更新检查。
- 独立 Qt editor 源码与设计保存在带注释标签 `archive/editor-before-ahk-only`（提交 `3ec4e87`）；不再构建、发布或维护它。AHK 内置流程编辑器继续维护。
- 保留 `THIRD_PARTY_NOTICES.md`、`LICENSES/D4KeyHelp-MIT.txt` 和 `docs/upstream/`。上游 README 是来源快照，不是本项目说明。
- 新项目级 AHK 函数使用 `D4Box_` 前缀；`utils/Lang.ahk` 先于依赖文案的全局变量加载。界面文案同时维护中英文。

## 配置与执行

- 设置、日志沿用入口目录；自定义流程、快捷键、检测 INI/BMP 在入口目录 `.runtime/`。源码运行的入口目录为 `app/ahk`，编译版为 EXE 所在目录。这些文件不得提交或打入发布包。
- 编译版内置默认流程和第三方声明，默认数据每次启动从 EXE 刷新至 `.runtime/builtin`；不得覆盖用户流程及检测模板。
- 配方通过 INI 数据和内置流程编辑器扩展，通用执行器负责动作。保持既有 INI 与稳定 ID 兼容。
- 自动输入必须保留游戏前台、目标、执行结果、超时、取消、窗口变化检查。坐标与延时不能成为新流程唯一判断。
- 不静默改变既有热键与运行语义；相关改动同步维护文档和 Windows 测试。更新检查不能阻塞热键、自动替换 EXE 或执行网络返回的代码。

## 检查

- 提交前：`python scripts/check_project.py`、`python -m unittest discover -s scripts/tests -v`、`bash -n app/ahk/tests/run.sh`、`git diff --check`。
- Windows Git Bash：`bash app/ahk/tests/run.sh all`，包括加载校验、GUI 冒烟、流程模型和更新回归；不发送游戏输入。
- 发布改动额外解析 PowerShell 脚本，并执行 `./scripts/build-windows.ps1`：实际编译 x64 EXE、校验 PE 版本、在只有 EXE 的新目录运行 `--self-test`。
- 区分仓库静态检查、Windows AHK 离线测试、编译包启动和真实游戏人工验证；只报告实际覆盖范围。macOS 只能运行跨平台静态/Python 检查。

## 发布与更新

- `app/ahk/core/Version.ahk` 为唯一版本来源，稳定版本格式 `X.Y.Z`。新主程序使用 `d4box-vX.Y.Z`；旧 `editor-v*` 不作为更新候选。归档标签不触发发布。
- `.github/workflows/ahk-release.yml` 在 main/PR/手动运行时构建测试，在新版本标签时发布。只用固定版本且 SHA-256 校验的官方 AHK/Ahk2Exe 工具；下载、产物位于被忽略的 `.runtime/`。
- build 只有读权限；publish 在所有检查和独立 EXE 探测通过后上传 draft、回下载核对哈希，再公开。禁止覆盖已有发布或移动已发布标签。
- 用户明确要求发布后才能推送版本标签；仅构建不代表发布成功。发行物为 Windows x64 EXE、便携 ZIP、SHA256SUMS.txt；源码来自对应 Git 标签。EXE 内嵌并解出第三方声明，ZIP 另附许可证。未签名不能表述为签名可信。
- 更新器只读公开 GitHub releases API，无需 gh 登录；检查已上传完整 EXE/哈希清单的稳定 AHK 版本，分页后按数字版本比较，处理超时/限流/格式错误。只提供固定仓库发布页，手动退出后替换 EXE并保留配置。
- 发布约定和恢复 editor 的方法见 `docs/ahk-release.md`。

## 协作

- 当前用户说明以 README、架构、流程、验证、发布文档为准；删除过时设计但保留来源声明。
- 如用户明确要求并发 agent，拆分不重叠的范围；主任务负责整合与验证。
