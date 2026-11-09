# D4BoxEditor · 独立配置编辑器

**C++17 + Qt 6 Widgets / SVG + QPainter**，不引入 QML、WebView、Python 运行时或游戏资源。
生成 `schemaVersion: 1` 的 UTF-8 JSON；目前只保存 `draft`。**不接 AHK、不执行配方、不发送游戏输入。**

## 编辑流程

1. 无参启动＝新建内存草稿；传目录＝按文件名选第一份合法配置；传文件＝精确打开，并可切换同目录配置。
2. 默认覆盖选中显示器：全屏绘制窗完全穿透，点位、背包 4 条窄边框及 8 个手柄分别使用小型交互窗口。
   **不是用一张透明大 QWidget 接管整个屏幕**；背包内部仍穿透。拖图标改位置，拖边框移动背包，拖 8 个柄缩放。点位右键可改名或删除，双击仍可改名。
3. 顶部工具栏可拖动；“编辑步骤”按需打开一个无边框圆角浮动面板；按住自定义标题区拖动，右上角关闭，右下角调整大小。关闭只隐藏，保留本次会话的编辑。
4. 在步骤/点位/检查/布局页配置顺序与属性；保存后得到独立 JSON。退出或切换未保存配置时提示保存/放弃/继续。
5. “预览”只显示点位、短名称与背包格线；格位的行列编号是静态辅助标记，**不是实时识别到的物品序号**。

### 已实现

- 点位新增、删除、改名、归一化坐标编辑、拖动；引用中的点位禁止直接删除。重复点通过点位列表选择。
- 背包固定 3×11、整体移动、8 向缩放、最小尺寸与边界限制。模板未标定背包可在布局页放置初始框。
- 左键/右键/等待/状态检查步骤；新增、删除、上下移、拖动排序；稳定 ID、动态当前物品或固定点位引用。
- 来源：鼠标单件、背包有限遍历、预先装入；颜色筛选、1–33 件上限、排除格位与有限总超时。
- 步骤的原始颜色条件、间隔、超时、点击风险、完成检查；颜色规则支持待标定值、容差、稳定样本、存在/转换模式。
- 入口检查、结果检查、悬空引用/错误类型/超限检查；不接受任意脚本或命令。
- 布局切换/复制/删除，参考区域裁剪、参考像素尺寸、人工校准标记。
- 撤销/重做；一次拖动只有一个历史操作。拖动期间工具栏/面板避让，结束后不自动盖回落点。
- 原子保存、修订号、保存期间写锁、外部修改/删除防覆盖、另存新 UUID、未知元数据保留。
- 未完成的颜色/排除格输入留在内存并阻止非法保存，不悄悄丢掉。重复配置 UUID/损坏配置会在目录提示中报告。
- 内置“自动嬗变”模板包含显式装入＋原 INI 的 13 步；旧坐标只作来源记录，实际点位/检查仍待标定。

点位改名与步骤面板复用圆角无边框窗口组件。改名弹窗在点位所在屏幕的可用区域居中，标题可拖动；确定后才修改，取消 / Esc / 右上角关闭均放弃本次输入。删除受引用保护，成功删除和改名均可撤销。

### 快捷键与退出

| 操作 | 快捷键 |
| --- | --- |
| 召回/隐藏工具栏与面板 | Ctrl+Alt+P；Mac 为 **Control+Option+P** |
| 切换纯只读预览 | Ctrl+Alt+D；Mac 为 **Control+Option+D** |
| 保存 / 撤销 / 重做 | Ctrl+S / Ctrl+Z / Ctrl+Shift+Z；Mac 使用 Command |
| 编辑器有焦点时返回控件 | Esc |

全局热键使用 macOS Carbon / Windows RegisterHotKey；注册失败明确报告，隐藏模式 10 秒后自动返回。
另提供托盘/菜单栏图标召回与退出，不要求辅助控制权限，也不使用屏幕捕获。
“操作底层”暂时隐藏所有几何编辑控件，工具栏保留“返回编辑”。编辑器不会切换或操作游戏自身状态。

## 本地开发入口

在仓库根目录执行 `./dev.sh`；每次配置 CMake、增量编译、运行完整 CTest，全部成功后才前台启动。
改完代码退出编辑器后重新运行即可，**不是文件监听或热重载**；不会关闭其他会话或丢弃未保存修改。

```sh
./dev.sh --demo
./dev.sh --demo -- --template
./dev.sh -- --background "/路径/游戏截图.png"
./dev.sh -- ./workflows
./dev.sh --build-only
./dev.sh --build-type RelWithDebInfo --jobs 8
./dev.sh --qt-prefix "/路径/Qt/6.11.3/macos"
./dev.sh --help
```

- 默认 Debug、最多 4 个并行编译任务；也支持 RelWithDebInfo / MinSizeRel。
- 构建目录为 `.runtime/editor/dev/<系统>-<架构>-<SDK路径摘要>/<构建类型>`，与发布构建隔离。这里只运行开发版，不打包。
- SDK 优先级：`--qt-prefix` → `QT_PREFIX` → 仓库固定版本的 `.runtime/editor/qt-official` / `.runtime/editor/qt`。显式路径错误直接报错，不回退。需要 Qt Base + SVG。
- 编辑器参数一律放在 `--` 后；相对配置/图片路径相对于调用时的终端目录，可从其他目录调用脚本绝对路径。
- `--demo` 使用 `.runtime/editor/backgrounds/transmute-game-original.png`；缺失则报错，不下载。不能与 `--background`、`--overlay`、`validate` 同用。
- `--build-only` 不接受底图或编辑器参数，避免静默忽略。构建/测试失败不启动界面。
- 前台运行可直接看日志；正常退出请使用界面退出，以处理保存提示。不会自动提交、推送或修改发布包。

macOS 首次安装 CMake/Ninja、Xcode Command Line Tools 后，可运行
`python3 scripts/fetch_editor_qt_macos.py` 获取固定版本 SDK；开发脚本自身只使用已有 SDK。
Windows 在 **VS 2022 x64 Developer PowerShell** 中用 Python 入口（需 Python 3.9+、CMake、Ninja 和匹配的 MSVC Qt SDK）：

```powershell
python scripts/dev_editor.py --qt-prefix 'C:\Qt\6.11.3\msvc2022_64' -- --template
```

Windows 构建命令生成有离线回归测试，启动入口仍需在 Windows 实机验证；不支持 Linux 覆盖层。

## 命令行

```sh
# macOS 的 EDITOR 指向 .app 内程序；Windows 对应 D4BoxEditor.exe
EDITOR="$PWD/.runtime/editor/dist/D4BoxEditor.app/Contents/MacOS/D4BoxEditor"
"$EDITOR"                         # 默认透明桌面覆盖，未保存新建
"$EDITOR" ./workflows             # 目录第一份合法配置
"$EDITOR" ./workflows/example.d4workflow.json
"$EDITOR" ./workflows --new
"$EDITOR" --template              # 内置自动嬗变的待校准草稿
"$EDITOR" --background ./cube.png # 静态底图调试
"$EDITOR" --overlay --screen 0
"$EDITOR" --list-screens
"$EDITOR" validate ./workflows/example.d4workflow.json --format json
"$EDITOR" validate ./workflows --for-run --format json
```

`--overlay` 与 `--background` 互斥；不存在的路径、非法文件与无效显示器会报错，绝不猜测或覆盖。
空目录新建；全是损坏文件的目录需显式 `--new`。无参不会偷偷扫描 CWD/home。
`validate` 用 QCoreApplication，无 GUI 平台插件也能检查；`--for-run` 仅静态前提，不实际找游戏。
退出码：0 正常（不表示保存/可执行），2 参数，3 文档/校验，4 I/O，5 平台/会话锁，6 内部 UI 测试失败。

## 坐标与安全边界

- 几何保存为有效参考区的 0–1 坐标；屏幕原点和 DPR 不写进点位。显示器逻辑坐标与参考像素尺寸分开。
- 默认参考区是整屏或整张等比例底图；需要在布局页自行排除标题栏/黑边。不自动识别游戏窗口、跟踪外部图片窗口或猜裁剪高度。
- 保存时将“配置 UUID＋布局 ID＋底图内容摘要/显示器”的裁剪记忆写入本机应用数据目录 `editor-views.ini`；同配置与同底图重开可恢复裁剪，不记录截图路径、不复制图片、不上传。
- 背景图仅留在内存，不写入 JSON。**截图模式**特意用独立底图窗口接收空白点击，避免可见截图上的操作误落到被遮住的真实桌面；**桌面覆盖模式**才是空白跨进程穿透。
- 更换底图、裁剪、显示器或显示尺度会失效当前参考标定，保留已有编辑并开始新的撤销历史段；不会允许撤销恢复旧屏幕的“已标定”状态。
- 人工确认只记录参考尺寸，不代表游戏实测。所有存盘结果仍是 draft；现有 AHK 仍读 INI，不能拿 JSON 直接运行。
- 同显示器的普通编辑会话有锁；不同编辑器的写入由文件锁和内容 hash 冲突检查防覆盖。文件锁只覆盖保存事务，不把另一个窗口冒充“只读模式”。
- 不承诺独占全屏、HDR、窗口自动跟踪、图像识别、实时 Debug、热键绑定执行、异常崩溃恢复或自动缩放适配游戏 UI。
- 当前没有主程序安全握手；如果同时打开 AHK，请在编辑前手动暂停所有宏。

## 代码分层与测试

- `WorkflowDocument`：协议、模板、语义校验、文件事务；不依赖 Widgets。
- `OverlayLayer`：归一化映射、绘制、局部交互窗、拖动边界；借用文档，不执行步骤。
- `NativeOverlay`：macOS/Windows 输入穿透属性和全局编辑快捷键，不混入配方业务。
- `WorkflowPanel`：步骤、点位、检查、布局属性；通过事务回调进入撤销栈。
- `EditorWindow`：编辑会话、工具栏、保存提示、预览与窗口生命周期。

构建/本地测量见 [构建与包体](../../docs/editor-build-size.md)。
发布工作流和标签规则见 [Release 配置](../../docs/editor-release.md)。
Schema 是 `schema/workflow-v1.schema.json`，C++ 验证器另检查引用、范围关联与静态前提。
`tests/core_tests.cpp`、`tests/overlay_geometry_tests.cpp`、`--ui-self-test`、`--smoke-test` 都进入 CTest。
`--render` 只合成底图/编辑 UI，永不截取桌面；`--diagnose-overlay` 输出实际原生窗口标志，不代表真实鼠标穿透已经实测。

## 界面图标与缩放

- 主题箭头、勾选符号使用内嵌 SVG（resources/theme），由 Qt SVG 按目标像素尺寸绘制；不再维护 PNG 图标。
- 顶栏图标仍使用 QPainter 矢量路径生成多倍率图标；背景截图仍支持 PNG/JPEG，不转换为 SVG。
- 运行时增加 Qt Svg、SVG imageformats/iconengines 插件，不引入 QML 或 WebEngine。
- CTest 与包内 --smoke-test 校验 SVG 解码、16/24/32/48/64 像素渲染和图标加载；发布必须携带 Qt SVG 源码与许可。
