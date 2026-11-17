# AHK 独立程序构建与发布

## Editor 保留点

带注释标签 `archive/editor-before-ahk-only` 指向 `3ec4e87`，保留独立 Qt editor 全部源码、设计、构建和发布脚本。标签名不匹配旧 `editor-v*` 或新 `d4box-v*` 发布触发器。旧 editor 发布标签不改动。需要将归档备份到远端时单独推送此标签；本地创建标签本身不会上传 GitHub。

## 本地构建

Windows PowerShell：`./scripts/build-windows.ps1`。无需 Visual Studio、Qt 或系统安装的 AHK。需要首次联网下载官方 AutoHotkey v2.0.31 与 Ahk2Exe 1.1.37.02a2；版本及 SHA-256 固定在 `scripts/prepare-ahk-tools.ps1`，每次重新校验归档并解压。构建基于上游 D4KeyHelp 的 Ahk2Exe x64、无压缩方式。

版本唯一来源 `app/ahk/core/Version.ahk`，使用 `X.Y.Z`，每段最多四位。它用于标题、更新比较、文件名、ProductVersion 和 FileVersion（追加 `.0`）。`-Tag d4box-vX.Y.Z` 会严格校验标签与源码一致。

构建仅复制明确源码目录，内嵌默认流程、D4KeyHelp MIT、AutoHotkey GPLv2 和来源声明。发布 EXE 不需要旁边存在源码/config；首次启动提取资源到 `.runtime`。单 EXE 和 ZIP 的许可证均可获得。

产物：

- `D4Box-vX.Y.Z-windows-x64.exe`：独立程序，可重命名为 `D4Box.exe`。
- `D4Box-vX.Y.Z-windows-x64.zip`：同一 EXE、使用说明和许可证。
- `SHA256SUMS.txt`：上面两个文件的 SHA-256。

它们位于 `.runtime/dist`。文件未签名；哈希清单用于完整性核对，不是独立数字签名。

构建会检查 PE 架构及版本，在新的含空格目录仅放 EXE，运行 `--self-test` 并检查结果。该模式不注册游戏热键、不发送输入、不写个人设置；只提取内置资源、创建主界面和流程编辑界面后退出。源码启动器仍保持独立，不会悄悄改为运行旧 EXE。

## GitHub Actions

`.github/workflows/ahk-release.yml`：main 推送、PR 和手动运行执行静态/Python/AHK 回归、编译与 EXE 探测，上传 artifact；只有 `d4box-v*` 标签进入发布 job，且标签必须严格匹配源码版本。build 只有读权限；publish 才能写 Release。

发布前改版本、运行检查并提交。明确决定发布后，推送代码，再创建和推送新的带注释标签；不要移动已有发布标签。publish 校验资产集合和 SHA256SUMS，创建 draft 上传，回下载核对，再设为公开稳定版本。流程失败时检查日志；已有 draft 需维护者检查后处理，不自动覆盖已发布文件。

发布完成需确认 Actions 成功、Release 非 draft、三个资产齐全，且单 EXE 能在可写目录运行。仅本地构建通过不代表 GitHub CI 或 Release 已成功。

## 更新检查

主界面底部“检查更新”请求公开 GitHub releases API，WinHTTP 异步轮询不阻塞界面和热键。请求有超时；关闭窗口取消请求；离线、HTTP 限流、响应错误显示失败并可重试。

分页查询每页 100 个，最多 10 页，总计 30 秒；超过限制提示检查未完成，不误报已是最新版。只考虑非草稿、非预发布的 `d4box-vX.Y.Z`，且对应版本的 EXE 与 SHA256SUMS 资产均已上传，按数字版本选最高值。旧 editor、归档标签与不完整发布不会提示安装。

只提供本仓库固定 HTTPS 发布页，不执行 API 中返回的 URL 或程序；不自动下载或替换。升级时退出程序，只替换 EXE，保留 `settings.ini` 与整个 `.runtime`。从源码迁移到 EXE 时可复制 `app/ahk` 下的这两项。源码与编译版的版本信息一致。
