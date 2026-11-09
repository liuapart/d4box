# 配置编辑器：实现、构建与包体实测

2026-11-06 · C++17 / Qt 6 Widgets / QPainter / CMake · 当前编辑器版本 0.2.0。

完整操作说明见 [编辑器 README](../app/editor/README.md)，标签发布见 [Release 配置](editor-release.md)。
**只编辑草稿 JSON，不接 AHK，不执行游戏操作，不识别真实背包物品。**

## 当前实现

- 全屏透明绘制窗不接受输入；只有点位、4 条背包边框、8 个缩放柄是交互小窗。
- 可拖动顶栏、默认隐藏的浮动步骤面板；点位/步骤/检查增删、排序、属性、目标来源及有限循环配置。
- 归一化坐标、DPR/显示器原点转换、底图客户区裁剪与本机裁剪记忆、人工参考尺寸校准。
- Undo/Redo、一次拖拽一个事务、草稿/退出提示、原子保存、写锁及外部变更冲突保护。
- JSON Schema/语义校验、稳定 ID、引用保护、内置待校准嬗变模板；`validate` 不依赖 GUI。
- 只读预览仅点位与格线；静态格位标签不是识别结果。全局编辑热键与托盘用于召回控件。
- 截图模式有安全底图，避免点击透到被图片遮住的真实桌面；桌面覆盖模式才是空白跨进程穿透。

尚未接入：AHK/游戏窗口安全握手、自动窗口跟踪、OCR/滚动找目标、物品识别、实际执行与实时 Debug。
没有 Windows/游戏实测、Apple Developer ID 公证或 Windows 代码签名。普通桌面覆盖不能冒充独占全屏支持。

## 本地复现

本机使用官方 Qt Base 6.11.1；已安装 SDK 可复用，不需要每次重新下载。Python 只用于构建辅助脚本，不打包进应用。

```sh
python3 scripts/fetch_editor_qt_macos.py
QT_PREFIX="$PWD/.runtime/editor/qt-official/6.11.1/macos"
cmake -S app/editor -B .runtime/editor/build-official -G Ninja \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTING=ON
cmake --build .runtime/editor/build-official --parallel 4
ctest --test-dir .runtime/editor/build-official --output-on-failure
python3 scripts/package_editor_macos.py \
  --build-dir .runtime/editor/build-official --qt-prefix "$QT_PREFIX" \
  --output-dir .runtime/editor/dist
python3 scripts/check_editor_cli.py \
  .runtime/editor/dist/D4BoxEditor.app/Contents/MacOS/D4BoxEditor
```

上述不带 `--release-version` 是本地打包；正式发行必须像 workflow 一样传入固定 Qt 源码中准备的
`--licenses-dir` 和与 tag 一致的 `--release-version`。本次实测也包含完整许可材料，不能通过省略许可缩小数字。

```sh
EDITOR="$PWD/.runtime/editor/dist/D4BoxEditor.app/Contents/MacOS/D4BoxEditor"
"$EDITOR"                  # 全屏透明编辑，未保存新配置
"$EDITOR" --template       # 内置嬗变草稿，点位/检查需校准
"$EDITOR" ./workflows --new --background ./cube.png
"$EDITOR" validate ./workflows --for-run --format json
```

文件/目录和底图必须真实存在；无参不扫描 home/CWD。图片、个人路径、SDK 和包体不提交 Git。
运行时可用 Control+Option+P 召回工具栏，Control+Option+D 退出只读预览；Windows 对应 Ctrl+Alt。

## 当前包体

本机 Apple Silicon arm64、macOS 26.5.2、AppleClang 17.0.0、Qt 6.11.1、MinSizeRel。
下表是 0.2.0 最终本地打包结果，不是裸可执行文件的预测交付大小；MB 按十进制计。

| 项目 | 实测 |
| --- | ---: |
| 主程序（不能脱离 Qt 依赖运行） | 583,664 B（0.58 MB） |
| 完整 .app payload（含动态库/插件/许可） | 24,351,023 B（24.35 MB） |
| .app 磁盘分配空间 | 24,911,872 B（24.91 MB） |
| ZIP 发行包 | 10,077,498 B（10.08 MB） |
| DMG 镜像 | 10,963,533 B（10.96 MB） |

当前本机产物在 `.runtime/editor/dist/`；机器可读报告是 `size-report.json`。
所有本机包都是 ad-hoc 签名，不是公证发行。Gatekeeper / SmartScreen 首次打开提示需在真实下载后确认。

### 优化方式

- 只用 Qt Base Core/Gui/Widgets，不用 QML、WebEngine、Python、自带字体和游戏截图。
- MinSizeRel + 支持时的 LTO / dead strip；发布主程序剥离符号，测试程序不进入应用。
- 官方 universal SDK 按目标架构裁片，只保留 Cocoa/JPEG 插件，审计 Mach-O/rpath/相对依赖。
- 不删除所需运行库、许可证或安全检查；不使用 UPX 规避签名或声称静态单文件。
- 首个更小功能骨架对比：Homebrew 完整包 64.90 MB / ZIP 25.35 MB，官方 SDK 22.99 MB / ZIP 9.28 MB。
  这些是旧骨架数据，不是当前编辑器；当前应用多了完整逻辑和发行许可材料，不做跨版本性能等价推断。

## 已验证与边界

- 485 条核心断言：结构/引用/范围、未知元数据、原子保存、锁、外部修改、模板和目录异常。
- CTest 四组全部通过：核心、覆盖层几何、UI 冒烟、UI 交互。
- UI 离线交互 35 项通过：增删/切换动作/排序、来源/校验、撤销/修订号、局部状态与聚焦字段重绑定等。
- 双屏负原点/DPR 离线模拟 169 项通过；真实 Cocoa 窗口属性/几何 178 项通过。
- 实际 Cocoa 绘制窗 `ignoresMouseEvents=true`、不透明标志关闭；小窗保留输入，Retina DPR=2，Carbon 热键注册成功。
- **这些不是物理鼠标跨进程穿透/连续拖拽、真实热键触发或 Windows 游戏验证。** 测试只向本程序投递离线事件，不注入 OS 游戏输入。
- 部署后二进制通过 27 个 CLI 用例；无 GUI 校验、错误路径、中文文件名、诊断不修改用户 JSON 都有回归。
- 包内插件搜索只指向 .app；清除 Qt/DYLD SDK 覆盖后启动并加载 PNG/JPEG；签名与依赖审计通过。
- 最终 ZIP 重新解压与 DMG 只读挂载后启动通过；这不是另一台干净 Mac 或最低系统版本测试。
- JSON Schema 和内置模板通过独立 schema 校验；仓库静态检查、5 个 Python 测试、shell/空白检查通过。
- Release YAML 通过 actionlint，Windows 打包器 5 个离线保护测试通过；MSVC/Inno 与 Intel Mac 原生构建仍需首次 CI 验证。
- 以上为本地构建检查记录；editor-v0.2.0 的三平台 CI 和 Release 状态应以 GitHub Actions / Release 实际结果为准，安装包只在全部 job 成功后发布。
