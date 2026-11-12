# Windows 验证清单

状态：截至 2026-11-12，本清单中的 Windows 与游戏内实测均未在本次文档核对中执行。macOS 仓库检查不能替代 AutoHotkey、PowerShell 或真实游戏验证；未实际完成的项目保持未验证。

## 验证层级

### 仓库静态与离线检查

在 macOS 仓库根目录运行：

```sh
python3 scripts/check_project.py
python3 -m unittest discover -s scripts/tests -v
bash -n app/ahk/tests/run.sh
git diff --check
```

这些命令检查仓库结构、配置回归、Python 测试、shell 语法和差异空白；它们不启动 AHK、不运行 PowerShell，也不操作游戏。通过时只能记录为仓库级检查通过。

### Windows AHK 离线检查

确认已安装 AutoHotkey v2，再从 Windows Git Bash 的仓库根目录分别运行：

```sh
bash app/ahk/tests/run.sh validate
bash app/ahk/tests/run.sh workflow
bash app/ahk/tests/run.sh smoke
```

- `validate` 对 `D4Box.ahk`、`SmokeTest.ahk`、`HealthGlobeTest.ahk` 和 `CubeWorkflowTest.ahk` 执行 AHK 加载期检查。
- `workflow` 使用假输入适配器验证流程规划、配置读写、取消和检查失败；不发送游戏输入，也不需要截图样本。
- `smoke` 加载模块并创建主界面，读取 `app/ahk/tests/smoke_result.txt` 判断结果；检查报告内容，不只看进程退出码。
- `globe` 使用测试截图导出的像素数据；`all` 依次运行加载检查、界面冒烟、workflow 和 globe。缺少样本或 Pillow 时 globe 不构成通过。

`run.sh` 会探测 AutoHotkey v2；可用 `AHK` 环境变量指定解释器。Git Bash 下由脚本处理 AHK 开关的路径转换和超时保护。脚本退出码 `0` 表示所选步骤通过，`1` 表示测试失败，`2` 表示运行环境不满足或参数无效。

PowerShell 启动入口为 `scripts/start-windows.ps1`。它要求 Windows 和 AutoHotkey v2，可自动探测常见安装位置，也可用 `-AhkPath` 指定解释器；实际入口是 `app/ahk/D4Box.ahk`，工作目录设为入口脚本所在的 `app/ahk`。启动后主界面、热键、宏、配方流程和诊断由同一 AHK 进程管理。入口旁的既有 `settings.ini`、日志及 `.runtime` 数据是本机运行数据，不应作为个人配置提交。

### 真实 Windows 与游戏内检查

只有在实际 Windows 环境完成并记录的项目才能标记通过。首先验证启动器能打开单一 d4box 主界面、设置可保存并在重启后恢复；然后在可控游戏环境检查宏与输入行为、窗口诊断以及魔盒流程。记录 Windows 版本、AutoHotkey 版本、仓库提交、分辨率、DPI、游戏 UI 缩放、显示模式、结果和相关日志。未运行或缺少证据的项目记录为未验证。

## 主程序与输入

- [ ] 使用 `scripts/start-windows.ps1` 启动，确认它找到 AHK v2 并打开 `app/ahk/D4Box.ahk`；另在含空格的仓库路径验证启动。
- [ ] 确认只有一个主程序实例；四组预设、主题、语言切换、设置保存及重启恢复正常。
- [ ] 验证 F1 启停、切出游戏暂停、返回恢复和手动点击暂停。
- [ ] 分别验证连点、维持 BUFF、按住及鼠标按键配置；检查翻滚、喝药、强制移动和鼠标移动。
- [ ] 在可控配置下检查满血、低血量、护盾遮挡时的读数及喝药策略；检查停止和退出时会释放按住的按键。

## 窗口诊断

- [ ] 游戏前台按 Ctrl+Alt+F8 显示客户区坐标和尺寸，约 5 秒后提示消失。
- [ ] 确认日志写入 `app/ahk/.runtime/window-diagnostics.log`，切出游戏时诊断热键不触发。
- [ ] 记录 1080p、1440p、4K、DPI 缩放、窗口模式及多显示器下的表现。
- [ ] 确认诊断只读取窗口信息，不发送按键、不点击或滚动。

## 血球像素回归

`globe` 依赖本地截图样本，仓库不保证这些个人截图存在。测试导出器从仓库根目录读取图片，生成 `app/ahk/tests/data/*.bin`；没有匹配样本时必须记录未验证，不能把缺失数据视为通过。

| 仓库根目录图片文件 | 样本名 | 预期 |
| --- | --- | --- |
| `screenShot.png` | `screenShot` | 满血 |
| `screenShot2.png` | `screenShot2` | 护盾遮挡 |
| `lowblood.png` | `lowblood` | 21%～31% |
| `sample2.PNG` | `sample2` | 满血 |
| `test.png` | `live` | 满血 |

补齐与测试标定匹配的 2560×1440 样本，或基于新采集样本建立并记录明确预期；安装带 Pillow 的 Python 后运行：

```sh
bash app/ahk/tests/run.sh globe --export
```

不要提交含私人信息的截图或个人设置。

## 魔盒游戏内验证

`workflow` 仅验证模型和假适配器行为。以下项目必须另在 Windows 游戏内逐项完成，不能由该测试或界面预览代替：

- [ ] 内置配置读取及旧路径：在确认可恢复的测试条件下核对传奇与稀有物品动作顺序，以及不同的 `0.380` / `0.350` 嬗变位置。
- [ ] 内置流程使用主屏全屏坐标；游戏窗口未铺满主屏时应拒绝执行。自定义客户区坐标应按当前布局重新采集。
- [ ] “升级黄装”只改变内置流程筛选；自定义流程使用自身品质筛选。升级后的步骤条件仍依据放入前的原始品质。
- [ ] 手动流程不自动从背包装入物品，且每次只执行一次；缺少就绪标记、执行步骤或末尾颜色检查时应拒绝运行。
- [ ] 采集只读取位置和颜色，预览不移动鼠标、不发送输入。
- [ ] 在安全可控场景测试颜色检查匹配与超时；只观察点击序列不足以证明配方结果成功。
- [ ] 测试 F3 取消、失焦、窗口移动或尺寸改变、总超时和手动中断；确认停止后没有后续动作，战斗宏不会在错误或中断后意外恢复。

颜色检查只确认指定屏幕像素达到配置的颜色容差，不识别配方内容、物品属性或游戏是否接受了操作。上述游戏内项目完成前，魔盒真实操作仍属未验证。
