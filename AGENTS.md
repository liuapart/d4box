# d4box 开发约定

- 主项目在 macOS 开发，AutoHotkey 和游戏交互在 Windows 验证。
- `vendor/D4keyHelp` 是上游 Git submodule。默认不修改、不提交上游源码；更新其指针必须说明对应版本和验证情况。
- 自有 AHK 代码放在 `extensions/ahk`，标识符统一使用 `D4Box_` 前缀。
- 扩展与上游目前是独立进程；不要依赖跨进程全局变量，也不要声称已实现插件接口或通信协议。
- 运行产物写入 `.runtime`。不要提交账号、凭据、个人设置或包含私人信息的截图。
- 新增自动点击流程应检查前置界面、执行结果、超时和取消；不要仅依靠固定坐标与延时。
- 提交前运行 `python3 scripts/check_project.py` 和 `git diff --check`。
- 区分静态检查、离线验证和真实 Windows/游戏验证，未经实际验证不得声称功能已可用。
