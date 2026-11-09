# d4box 开发约定

- 主项目在 macOS 开发，AutoHotkey 和游戏交互在 Windows 验证。
- `app/ahk` 是主仓库直接维护的应用源码，可以修改、重构；项目不再使用 submodule。
- 源码基于 D4KeyHelp 迁入。保留 `THIRD_PARTY_NOTICES.md`、许可文本和 `docs/upstream` 中的来源声明。
- Windows 执行主程序的统一入口是 `app/ahk/D4Box.ahk`，一个进程管理主界面、热键、宏和诊断。新增 AHK 项目级辅助函数使用 `D4Box_` 前缀。
- `app/editor` 是独立 C++17 / Qt 6 Widgets 配置编辑器，以体积优先，不引入 QML。使用穿透绘制层与局部交互小窗，只编辑草稿 JSON，不发送游戏输入；主程序尚未接入 JSON。新编辑器代码与 AHK 入口保持解耦。
- 编辑器构建、Qt SDK 和打包结果放 `.runtime/editor`；构建后运行 CTest。用 `scripts/package_editor_macos.py` 测量包含依赖的发布包，不能把裸可执行文件大小当成完整交付大小。
- 界面文案在 `app/ahk/utils/Lang.ahk` 同时维护中文、英文；语言模块必须早于依赖它的全局变量加载。
- 保持原有配置兼容。迁入代码的设置及日志仍在入口脚本旁，新诊断产物在该目录的 `.runtime`；这些文件都已忽略。
- 不要提交账号、凭据、个人设置或含私人信息的截图。
- 新增自动点击流程应检查前置界面、执行结果、超时和取消；不要仅依靠固定坐标与延时。
- 提交前运行 `python3 scripts/check_project.py`、`python3 -m unittest discover -s scripts/tests -v`、`bash -n app/ahk/tests/run.sh` 和 `git diff --check`。
- Windows 上使用 `bash app/ahk/tests/run.sh validate` 、`workflow` 和 `smoke` 验证加载、流程与界面；血球回归还需要本地截图样本。
- 区分静态检查、离线验证和真实 Windows/游戏验证，未经实际验证不得声称功能已可用。
- 魔盒配方通过 `config/cube-workflows.ini` 和流程编辑器维护，执行器只处理通用动作。新增配方优先添加数据，避免恢复专用 `TransmuteXXX` 函数。
