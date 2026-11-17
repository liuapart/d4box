# 帮助手册截图来源与维护

[手册首页](../README.md) · [开发指引](../../development.md)

## 来源与范围

- 界面版本：d4box v0.3.1，中文、浅色主题。
- 截图来自真实 AHK GUI 控件；使用单独创建的示例配置，不读取用户的 `settings.ini`、流程或个人模板。
- 图片保留界面客户区，未包含桌面、窗口标题栏或个人游戏画面。
- 第 09 张为合成演示面板上的真实冻结截图框选 UI，面板明确标注“不是暗黑 IV 游戏截图”。
- 第 11 张的出现／关闭结果由演示窗口上的真实截图及匹配测试获得，只证明该演示样本，不证明游戏检测可靠性。
- 第 17 张只设置状态显示数据，不运行流程或发送输入；图片本身标注“状态展示，未执行”。
- 第 18 张展示更新窗口初始说明，不伪造网络查询结果或可用更新。

图中的坐标、颜色、示例名称不是可以直接执行的游戏配方。不要把这些图片说明成“游戏实测通过”。

## 图片清单

| 文件 | 说明 |
| --- | --- |
| [01-main.png](01-main.png) | 流程快捷键首页，默认 F3 |
| [02-macros-default.png](02-macros-default.png) | 宏设置初始状态 |
| [03-macro-example.png](03-macro-example.png) | 仅启用一个技能的宏配置，未启动 |
| [04-workflow-builtin.png](04-workflow-builtin.png) | 内置只读流程 |
| [05-workflow-new.png](05-workflow-new.png) | 实际新建流程的初始步骤 |
| [06-workflow-item-target.png](06-workflow-item-target.png) | 单物品来源与右键步骤 |
| [07-confirm-color.png](07-confirm-color.png) | 可选接受的独立单点检测 |
| [08-detector-new.png](08-detector-new.png) | 区域库草稿 |
| [09-detector-selection.png](09-detector-selection.png) | 合成面板上的冻结截图框选 |
| [10-detector-captured.png](10-detector-captured.png) | 模板预览与待测试状态 |
| [11-detector-tested.png](11-detector-tested.png) | 合成面板实际双向测试通过 |
| [12-confirm-region.png](12-confirm-region.png) | 可选接受引用区域节点 |
| [13-final-check.png](13-final-check.png) | 末尾检测节点 |
| [14-inventory-workflow.png](14-inventory-workflow.png) | 背包模式、品质与上限 |
| [15-add-binding.png](15-add-binding.png) | 新增快捷键绑定 |
| [16-bindings-example.png](16-bindings-example.png) | F3 和 F6 分别绑定示例流程 |
| [17-running-state-demo.png](17-running-state-demo.png) | 仅展示的运行状态与取消控件 |
| [18-updates.png](18-updates.png) | 版本与更新窗口 |

## 重新采集

从仓库根目录在 Windows 交互桌面执行：

```powershell
.\scripts\capture-help.ps1
# 若解释器不在默认目录：
.\scripts\capture-help.ps1 -AhkPath 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe'
```

脚本将模块复制到 `.runtime/help-capture/<唯一目录>/app`，仅在该位置创建演示数据。它使用实际界面创建函数，不加载游戏热键入口，不启动宏或流程执行器；不需要游戏启动。

脚本调用 [CaptureHelp.ahk](../../../scripts/help/CaptureHelp.ahk) 完成界面展示与截图。BMP、测试记录和演示配置留在被忽略的工作目录，最终 18 张 PNG 写入本目录。超时或检测测试失败会报错，不应把不完整采集当成成功。

运行会短暂显示并切换窗口，需要未锁定且没有遮挡的交互桌面；结束后逐张检查图片，再运行 `python scripts/check_docs.py`。生成器依赖当前 UI 控件，界面结构变化时需同步维护。

## 文档结构参考

本手册采用“首页概览 → 安装 → 最短上手路径 → 分功能教程 → 维护 → 排障”的组织方式，参考以下官方项目文档的导航方法，正文与截图均为 d4box 自有内容：

- [PowerToys](https://github.com/microsoft/PowerToys)：功能总览、安装、文档和反馈入口。
- [ShareX](https://github.com/ShareX/ShareX)：下载、功能以及工作流和快捷键导航。
- [OBS Quick Start Guide](https://obsproject.com/kb/quick-start-guide)：按步骤开始、验证设置、引向后续帮助。
