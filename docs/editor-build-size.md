# 编辑器构建、测试与包体

`app/editor` 是独立 C++17 / Qt 6 Widgets + SVG 程序，只编辑草稿 JSON。当前默认版本见 `app/editor/CMakeLists.txt`；正式包的版本以 `editor-vX.Y.Z` 标签为准。构建、Qt SDK、样本和产物放在被 Git 忽略的 `.runtime/editor/`。本页的包体均指**包含动态库、插件及许可证的交付物**，不拿裸可执行文件大小代替。

## 开发构建

macOS 需要 Xcode Command Line Tools、Python 3.10+、CMake、Ninja 和官方 Qt 6.11.3 Base/SVG SDK。SDK 未准备时先执行 `python3 scripts/fetch_editor_qt_macos.py`；开发脚本不会自动下载。Windows 开发还需要 VS 2022 x64 Developer PowerShell 与匹配的 Qt Base/SVG MSVC SDK。`--qt-prefix` 优先于 `QT_PREFIX` 和仓库内 SDK；显式指定错误时不回退。

```sh
./dev.sh --build-only            # Debug 增量构建 + CTest；不打开 GUI
./dev.sh                         # 构建、CTest，然后打开编辑器
./dev.sh --demo -- --template    # 使用本机忽略的游戏底图；不发送游戏输入
```

`--demo` 的图片须预先保存在 `.runtime/editor/backgrounds/transmute-game-original.png`；也可用 `./dev.sh -- --background /path/to/image.png`。命令行参数及编辑功能见 [编辑器说明](../app/editor/README.md)。在已有构建目录执行 `ctest --test-dir <build-dir> --output-on-failure`。当前 CMake 注册七组跨平台 CTest，macOS 另有原生图像测试；具体数量以 CTest 输出为准。

## 本地完整打包

在 macOS 仓库根目录，先准备 Qt SDK 并选定自己的构建目录。下面的目录与发布 CI 隔离，打包后使用报告衡量尺寸：

```sh
QT_PREFIX="$PWD/.runtime/editor/qt-official/6.11.3/macos"
cmake -S app/editor -B .runtime/editor/build-local -G Ninja \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTING=ON
cmake --build .runtime/editor/build-local --parallel 4
ctest --test-dir .runtime/editor/build-local --output-on-failure
python3 scripts/package_editor_macos.py \
  --build-dir .runtime/editor/build-local --qt-prefix "$QT_PREFIX" \
  --output-dir .runtime/editor/dist-local
python3 scripts/check_editor_cli.py \
  .runtime/editor/dist-local/D4BoxEditor.app/Contents/MacOS/D4BoxEditor
```

结果在 `.runtime/editor/dist-local/size-report.json`、完整 `.app` 和 ZIP；本地默认打包可不传发行参数。**正式 Release** 还必须提供经固定 SHA-256 校验的 Qt Base/SVG 许可目录和与标签一致的 `--release-version`，并由工作流制作与复验 DMG。Windows 使用 `scripts/package_editor_windows.py` 在原生 MSVC/Qt 环境部署、审计 DLL 与 CRT、验证便携 ZIP；CI 额外生成并实测 Inno Setup 安装器。参数和发布门禁见 [发布流程](editor-release.md)。

## 已发布包体样本

`editor-v0.2.6` 的 GitHub Release 资产记录如下，单位为压缩文件字节；这**只代表该版本**，新版本应读取其 `*-report.json` 和 Release 资产，不沿用旧数字。

| 目标 | ZIP | 其他交付物 |
| --- | ---: | ---: |
| macOS arm64 | 10,768,392 B | DMG 12,576,758 B |
| macOS x64 | 12,121,246 B | DMG 14,130,946 B |
| Windows x64 | 13,475,419 B | Inno Setup 11,420,151 B |

每个平台的 `*-report.json` 包含完整应用载荷尺寸、依赖和打包测试结果。Release 另有 Qt Base/SVG 源码归档及 `SHA256SUMS.txt`；这些不包含在上表应用 ZIP 尺寸内。Mac 使用 ad-hoc 签名，Windows 未做代码签名；包体校验不证明发布者身份。

## 精简约束与验证边界

- 只部署 Qt Core/Gui/Widgets/Svg、实际用到的平台与图像插件以及必需运行库。Windows 打包器明确排除并审计 `Qt6Network.dll`、`dxcompiler.dll`、`dxil.dll`；若新增功能需要这些依赖，应先修改部署契约和测试，而不是跳过失败。
- Mac 对官方 universal Qt SDK 按目标架构裁片，检查 Mach-O 依赖、rpath、签名与 SDK 外启动。Windows 对 PE 架构、导入库、插件路径、CRT、解压包及安装器作原生检查。依赖许可文件不得为了减小体积而删除。
- CTest、CLI 回归、包体审计和 GitHub Actions 在 CI runner 上执行；不等于干净机器、最低支持系统或游戏内鼠标/热键验证。AHK 主程序不在编辑器 Release 中。
