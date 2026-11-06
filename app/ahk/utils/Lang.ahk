; ========== 界面多语言（中文 / English） ==========
; 界面文字统一用 L("key") 取当前语言，LANG_TABLE 里按 key 存 zh / en 两列；
; 带 {1}、{2}… 占位符的文本用 L("key", 参数…) 取，参数按顺序替换占位符。
;
; 当前语言存在 settings.ini 的 [UI] Language：zh = 中文（默认），en = 英语。
; 切换语言会重载脚本（与切换主题同一做法），因为控件文字在创建时就定下来了。
;
; 范围说明：本模块只负责界面文字（窗口、控件、状态栏、提示、日志开关回显）。
; debugd4.log 里的运行明细仍是中文 —— 那是排查用的内部记录，与界面语言无关。

global appLang := "zh"

; 语言表：key -> {zh, en}
global LANG_TABLE := Map(
    "cube.selected", {zh: "已选用配方流程：{1}（游戏内 F3 执行）", en: "Selected workflow: {1} (F3 in game)"},
    "cube.newProfile", {zh: "新建流程", en: "New workflow"},
    "cube.defaultName", {zh: "自动嬗变背包物品", en: "Transmute inventory items"},
    "cube.step.transmute", {zh: "选择嬗变", en: "Select transmute"},
    "cube.step.execute", {zh: "执行", en: "Execute"},
    "cube.step.confirm", {zh: "接受", en: "Accept"},
    "cube.step.affix", {zh: "添加词缀", en: "Add affix"},
    "cube.step.upgrade", {zh: "升级至传奇", en: "Upgrade to legendary"},
    "cube.step.clear", {zh: "清除", en: "Clear"},
    "cube.manualConditions", {zh: "手动放入模式的全部步骤需使用“全部／无条件”，不按背包品质分支。", en: "Manual mode requires unconditional steps; inventory quality routing is unavailable."},
    "cube.legacyYellowTip", {zh: "仅影响内置“自动嬗变背包物品”；自定义流程使用自己的品质筛选。", en: "Only affects the built-in inventory workflow; custom workflows use their own quality filter."},
    ; ---------- 魔盒流程 ----------
    "cube.title", {zh: "魔盒配方流程", en: "Cube workflows"},
    "cube.copy", {zh: "复制为新流程", en: "Copy as new"},
    "cube.deleteProfile", {zh: "删除流程", en: "Delete workflow"},
    "cube.name", {zh: "流程名称", en: "Name"},
    "cube.source", {zh: "处理来源", en: "Source"},
    "cube.inventory", {zh: "扫描背包物品", en: "Scan inventory"},
    "cube.manual", {zh: "已手动放入魔盒", en: "Already loaded in cube"},
    "cube.quality", {zh: "筛选品质", en: "Quality filter"},
    "cube.space", {zh: "坐标基准", en: "Coordinates"},
    "cube.screen", {zh: "主屏全屏（旧版标定）", en: "Primary fullscreen (legacy)"},
    "cube.client", {zh: "游戏窗口客户区", en: "Game client area"},
    "cube.maxItems", {zh: "物品上限", en: "Item limit"},
    "cube.timeout", {zh: "总超时（秒）", en: "Total timeout (sec)"},
    "cube.captureReady", {zh: "采集就绪标记（3秒）", en: "Capture ready marker (3s)"},
    "cube.stepName", {zh: "步骤名称", en: "Step name"},
    "cube.action", {zh: "操作", en: "Action"},
    "cube.applies", {zh: "原始品质条件", en: "Original quality"},
    "cube.delay", {zh: "等待（毫秒）", en: "Delay (ms)"},
    "cube.captureStep", {zh: "采集鼠标位置与颜色（3秒）", en: "Capture pointer position/color (3s)"},
    "cube.color", {zh: "颜色 RGB", en: "RGB color"},
    "cube.tolerance", {zh: "颜色容差", en: "Color tolerance"},
    "cube.checkTimeout", {zh: "检查超时（毫秒）", en: "Check timeout (ms)"},
    "cube.addStep", {zh: "添加步骤", en: "Add step"},
    "cube.updateStep", {zh: "更新步骤", en: "Update step"},
    "cube.deleteStep", {zh: "删除步骤", en: "Delete step"},
    "cube.up", {zh: "上移", en: "Move up"},
    "cube.down", {zh: "下移", en: "Move down"},
    "cube.saveUse", {zh: "保存并选用", en: "Save and select"},
    "cube.preview", {zh: "预览执行顺序", en: "Preview steps"},
    "cube.close", {zh: "关闭", en: "Close"},
    "cube.red", {zh: "传奇／暗金色", en: "Legendary/unique color"},
    "cube.yellow", {zh: "稀有（黄）", en: "Rare (yellow)"},
    "cube.blue", {zh: "魔法（蓝）", en: "Magic (blue)"},
    "cube.redyellow", {zh: "传奇色＋稀有", en: "Legendary color + rare"},
    "cube.any", {zh: "全部／无条件", en: "All / unconditional"},
    "cube.select", {zh: "选择配方", en: "Select recipe"},
    "cube.execute", {zh: "点击执行", en: "Click execute"},
    "cube.confirm", {zh: "点击接受", en: "Click accept"},
    "cube.clear", {zh: "清除物品", en: "Clear item"},
    "cube.wait", {zh: "等待", en: "Wait"},
    "cube.check", {zh: "等待颜色匹配", en: "Wait for color"},
    "cube.builtin", {zh: "[内置]", en: "[Built-in]"},
    "cube.discard", {zh: "当前修改尚未保存，是否放弃？", en: "Discard unsaved changes?"},
    "cube.noReady", {zh: "未采集就绪标记。自定义流程运行前需要就绪标记及末尾颜色检查。", en: "No ready marker. Custom workflows need a ready marker and a final color check."},
    "cube.readySet", {zh: "已设置魔盒就绪标记", en: "Cube ready marker captured"},
    "cube.builtinNote", {zh: "内置旧版流程：保留原坐标和延时，未验证游戏结果。修改请先复制；升级黄装沿用主界面勾选项。", en: "Legacy built-in: original coordinates/delays, no result verification. Copy to edit; rare upgrades follow the main checkbox."},
    "cube.customNote", {zh: "坐标范围 0～1。品质条件按放入前的品质判断。添加颜色检查可等待界面变化；选用后切回游戏按 F3。", en: "Coordinates: 0–1. Conditions use original quality. Add color checks to wait for UI changes. Return to the game and press F3."},
    "cube.newStep", {zh: "新步骤", en: "New step"},
    "cube.copyFirst", {zh: "内置流程不可覆盖，请先复制为新流程。", en: "Copy the built-in workflow before editing."},
    "cube.copySuffix", {zh: "副本", en: "copy"},
    "cube.saved", {zh: "已保存并选用。切回魔盒界面按 F3 执行，再按 F3 取消。自定义流程缺少检查点时会拒绝执行。", en: "Saved and selected. In the cube UI press F3 to run/cancel. Custom workflows without checkpoints cannot run."},
    "cube.deleteQuestion", {zh: "删除此自定义流程？当前选用将恢复为内置嬗变。", en: "Delete this workflow and select the built-in transmute workflow?"},
    "cube.previewNote", {zh: "仅展示配置顺序，不移动鼠标、不点击，也不验证游戏结果。", en: "Configuration preview only: no mouse movement, clicks, or game verification."},
    "cube.loadItem", {zh: "右键放入当前背包物品 → 等待 250ms", en: "Right-click inventory item → wait 250ms"},
    "cube.captureHint", {zh: "3 秒内切回游戏，把鼠标放到要采集的按钮或稳定色块上。", en: "Return to the game and point at the desired button/stable color within 3 seconds."},
    "cube.ready", {zh: "魔盒就绪", en: "Cube ready"},
    "cube.busy", {zh: "流程正在执行，请先按 F3 取消。", en: "A workflow is running. Press F3 to cancel first."},
    "cube.invalidStep", {zh: "步骤格式无效；名称不能为空，也不能包含竖线或换行。", en: "Invalid step; names cannot be empty or contain pipes/newlines."},
    "cube.invalidName", {zh: "流程标识或名称无效。", en: "Invalid workflow ID or name."},
    "cube.invalidSource", {zh: "处理来源无效。", en: "Invalid input source."},
    "cube.invalidQuality", {zh: "品质条件无效。", en: "Invalid quality condition."},
    "cube.invalidSpace", {zh: "坐标基准无效。", en: "Invalid coordinate space."},
    "cube.invalidLimits", {zh: "物品上限需为 1～33；总超时 1～1800 秒；单步等待 0～10000ms；检查超时 100～30000ms。", en: "Limits: 1–33 items; total 1–1800 sec; delay 0–10000ms; check timeout 100–30000ms."},
    "cube.invalidStepCount", {zh: "流程必须有 1～100 个步骤。", en: "A workflow must have 1–100 steps."},
    "cube.invalidPoint", {zh: "坐标必须在游戏区域内，归一化 X/Y 范围为 0～1。", en: "Position must be inside the game area; normalized X/Y must be 0–1."},
    "cube.invalidColor", {zh: "颜色应为六位 RGB 十六进制值，容差为 0～255。", en: "Color must be six RGB hex digits; tolerance must be 0–255."},
    "cube.emptyRoute", {zh: "品质 {1} 没有可执行步骤。", en: "No steps for quality {1}."},
    "cube.needChecks", {zh: "自定义流程需采集就绪标记，并让品质 {1} 的最后一步为颜色检查。", en: "Capture a ready marker and end the {1} route with a color check."},
    "cube.reservedId", {zh: "不能覆盖内置流程标识。", en: "A custom workflow cannot overwrite a built-in ID."},
    "cube.noWindow", {zh: "没有检测到前台暗黑4游戏窗口。", en: "Diablo IV must be the foreground window."},
    "cube.legacyFrame", {zh: "旧版标定要求游戏铺满主屏。窗口模式请复制流程、改用客户区并重新采集位置。", en: "Legacy calibration requires primary-screen fullscreen. For windowed mode, copy the workflow, use client coordinates, and recapture positions."},
    "cube.cancelled", {zh: "用户取消", en: "Cancelled"},
    "cube.focusLost", {zh: "游戏失去焦点，流程停止。", en: "Game lost focus; workflow stopped."},
    "cube.frameChanged", {zh: "游戏窗口位置或尺寸发生变化，流程停止。", en: "Game window moved or resized; workflow stopped."},
    "cube.timedOut", {zh: "流程超过总超时。", en: "Workflow exceeded its total timeout."},
    "cube.checkFailed", {zh: "等待界面状态超时：{1}", en: "Timed out waiting for UI state: {1}"},
    "cube.missingProfile", {zh: "选用的流程不存在，请在配方流程窗口重新选择。", en: "Selected workflow is missing. Select one in the workflow editor."},
    "cube.noTargets", {zh: "背包中没有匹配的目标物品。", en: "No matching inventory items."},
    "cube.itemChanged", {zh: "背包第 {1} 行、第 {2} 列的物品状态已变化，停止执行。", en: "Inventory slot row {1}, column {2} changed; stopped."},
    "cube.progress", {zh: "{1}：处理 {2}/{3}（F3 取消）", en: "{1}: processing {2}/{3} (F3 cancels)"},
    "cube.done", {zh: "已执行 {1} 个物品的配置步骤。", en: "Configured steps executed for {1} item(s)."},
    ; ---------- 通用 ----------
    "common.none",                  {zh: "无", en: "None"},
    "common.enable",                {zh: "启用", en: "Enable"},
    "common.intervalMs",            {zh: "间隔 (ms)：", en: "Interval (ms):"},

    ; ---------- 主窗口 ----------
    "app.title",                    {zh: "d4box · 暗黑4助手", en: "d4box · Diablo IV Helper"},
    "main.renameHint",              {zh: "右键配置名可重命名", en: "Right-click to rename"},
    "main.theme.toDark",            {zh: "深色模式", en: "Dark mode"},
    "main.theme.toLight",           {zh: "浅色模式", en: "Light mode"},
    "main.theme.tip",               {zh: "切换浅色/深色主题（会重载脚本）", en: "Switch light/dark theme (reloads the script)"},
    ; 按钮上显示的是"点击后切到"的语言，和主题按钮显示目标主题保持一致
    "main.lang.button",             {zh: "English", en: "中文"},
    "main.lang.tip",                {zh: "界面语言：点击切换到 English", en: "UI language: click to switch to 中文"},
    "main.section.skills",          {zh: "按键宏设置", en: "Skill Macro"},
    "main.section.extra",           {zh: "额外设置", en: "Extra Settings"},
    "main.col.hotkey",              {zh: "快捷键", en: "Key"},
    "main.col.strategy",            {zh: "策略", en: "Mode"},
    "main.col.interval",            {zh: "执行间隔 (ms)", en: "Interval (ms)"},
    "main.col.delay",               {zh: "延迟 (ms)", en: "Delay (ms)"},
    "main.col.random",              {zh: "延迟随机", en: "Random"},
    "main.status.prefix",           {zh: "● 状态: ", en: "● Status: "},
    "main.toggle.label",            {zh: "启停：", en: "Toggle:"},
    "main.toggle.keyboard",         {zh: "键盘", en: "Keyboard"},
    "main.toggle.middle",           {zh: "中键", en: "Middle"},
    "main.toggle.side1",            {zh: "侧键1", en: "Side 1"},
    "main.toggle.side2",            {zh: "侧键2", en: "Side 2"},
    "main.start",                   {zh: "开始", en: "Start"},
    "main.stop",                    {zh: "停止", en: "Stop"},
    "main.save",                    {zh: "保存设置", en: "Save settings"},
    "main.hint",                    {zh: "提示：启停键可配置 | F3 运行选定配方/取消（魔盒界面）| Tab 查看地图暂停 | 仅暗黑4窗口生效"
                                   , en: "Tips: toggle key configurable | F3 selected workflow/cancel (cube UI) | Tab map pauses | D4 window only"},
    "main.ready",                   {zh: "就绪", en: "Ready"},

    ; ---------- 技能行 ----------
    "skill.1",                      {zh: "技能一：", en: "Skill 1:"},
    "skill.2",                      {zh: "技能二：", en: "Skill 2:"},
    "skill.3",                      {zh: "技能三：", en: "Skill 3:"},
    "skill.4",                      {zh: "技能四：", en: "Skill 4:"},
    "skill.left",                   {zh: "左键技能：", en: "LMB skill:"},
    "skill.right",                  {zh: "右键技能：", en: "RMB skill:"},
    "mouseKey.lbutton",             {zh: "鼠标左键", en: "LMB"},
    "mouseKey.rbutton",             {zh: "鼠标右键", en: "RMB"},
    "mouseKey.tip",                 {zh: "下拉选择鼠标左键/右键，或直接输入键名（如 5、F、XButton1）"
                                   , en: "Pick LMB/RMB from the list, or type a key name (e.g. 5, F, XButton1)"},
    "mouseKey.invalid",             {zh: "「{1}」不是有效的按键名，已恢复为 {2}"
                                   , en: "`"{1}`" is not a valid key name, reverted to {2}"},

    ; ---------- 技能策略 ----------
    "strategy.off",                 {zh: "禁用", en: "Off"},
    "strategy.click",               {zh: "连点", en: "Repeat"},
    "strategy.buff",                {zh: "维持BUFF", en: "Keep buff"},
    "strategy.hold",                {zh: "按住", en: "Hold"},

    ; ---------- 额外设置 ----------
    "extra.shift",                  {zh: "按住 Shift", en: "Hold Shift"},
    "extra.debugLog",               {zh: "调试日志", en: "Debug log"},
    "extra.debugLog.tip",           {zh: "关闭后不再写日志文件；开启后可在 settings.ini 里把 DebugLogVerbose 设为 1 记录每次按键明细"
                                   , en: "When off, nothing is written to the log file; when on, set DebugLogVerbose=1 in settings.ini to log every key press"},
    "extra.clearLog",               {zh: "清理日志", en: "Clear log"},
    "extra.dodge",                  {zh: "翻滚：", en: "Dodge:"},
    "extra.key.space",              {zh: "空格", en: "Space"},
    "extra.potion",                 {zh: "喝药：", en: "Potion:"},
    "extra.healthCheck",            {zh: "血量检测", en: "HP check"},
    "extra.healthCheck.tip",        {zh: "按血球液面自动判断血量，低于阈值才喝药；建议把喝药间隔改成 1000~2000ms"
                                   , en: "Estimate HP from the health globe and drink only below the threshold; set the potion interval to 1000~2000 ms"},
    "extra.shieldSafe",             {zh: "护盾遮挡时不喝药", en: "Skip if shielded"},
    "extra.shieldSafe.tip",         {zh: "护盾盖住血球时读不出血量：勾选=视为安全跳过；取消=照常喝药（适合常驻护盾的构筑）"
                                   , en: "A shield covering the globe hides the HP: checked = treat as safe and skip; unchecked = drink anyway (builds with a permanent shield)"},
    "extra.forceMove",              {zh: "强移：", en: "Force move:"},
    "extra.mouseAutoMove",          {zh: "鼠标自动移动", en: "Auto mouse move"},
    "extra.pauseOnClick",           {zh: "鼠标点击时暂停宏", en: "Pause on click"},
    "extra.compass",                {zh: "罗盘专用", en: "Compass mode"},
    "extra.upgradeYellow",          {zh: "升级黄装", en: "Upgrade rares"},
    "extra.transmute",              {zh: "配方流程…", en: "Cube workflows…"},
    "extra.transmuteHelp.tip",      {zh: "打开说明文档", en: "Open the documentation"},
    ; 说明文档链接跟着界面语言走：中文看 README.md，英文看 README_EN.md
    "extra.transmuteHelp.url",      {zh: "https://github.com/duzefu/D4keyHelp#readme"
                                   , en: "https://github.com/duzefu/D4keyHelp/blob/main/README_EN.md"},
    "extra.threshold",              {zh: "血量阈值：", en: "HP threshold:"},
    "extra.threshold.tip",          {zh: "血量低于该百分比才喝药（血球液面高度换算，50 即球心位置）"
                                   , en: "Drink only below this HP percentage (derived from the globe's liquid level; 50 is the orb center)"},
    "extra.testGlobe",              {zh: "检测血球", en: "Test globe"},
    "extra.testGlobe.tip",          {zh: "点后 3 秒识别血球（留时间切回游戏）：球心位置、当前血量与判定结果显示在下方状态栏"
                                   , en: "Detects the globe 3 seconds after clicking (time to switch back to the game); the center, current HP and verdict appear in the status bar"},
    "extra.healthUnknown",          {zh: "未检测（点「检测血球」后切回游戏）", en: "Not tested (click `"Test globe`")"},
    "extra.healthStatus.tip",       {zh: "最近一次识别到的血量读数（宏运行中会自动刷新）"
                                   , en: "Latest detected HP reading (refreshed automatically while the macro runs)"},

    ; ---------- 运行状态 ----------
    "state.idle",                   {zh: "未运行", en: "Not running"},
    "state.running",                {zh: "运行中", en: "Running"},
    "state.paused",                 {zh: "已暂停", en: "Paused"},
    "state.pausedWindow",           {zh: "已暂停(窗口切换)", en: "Paused (window)"},
    "state.tempPaused",             {zh: "临时暂停", en: "Temp paused"},
    "state.stopped",                {zh: "已停止", en: "Stopped"},
    "state.transmuting",            {zh: "配方执行中", en: "Workflow running"},
    "state.transmuteDone",          {zh: "配方流程结束", en: "Workflow finished"},
    "state.transmuteFailed",        {zh: "配方执行失败", en: "Workflow failed"},
    "state.cancelled",              {zh: "已取消", en: "Cancelled"},
    "state.noTarget",               {zh: "未找到目标物品", en: "No target items"},

    ; ---------- 状态栏（宏运行） ----------
    "bar.macroStarted",             {zh: "宏已启动", en: "Macro started"},
    "bar.macroStopped",             {zh: "宏已停止", en: "Macro stopped"},
    "bar.windowInactive",           {zh: "宏已暂停 - 窗口未激活", en: "Macro paused - game window inactive"},
    "bar.windowActive",             {zh: "宏已恢复 - 窗口已激活", en: "Macro resumed - game window active"},
    "bar.macroPaused",              {zh: "宏已暂停", en: "Macro paused"},
    "bar.macroResumed",             {zh: "宏已继续", en: "Macro resumed"},
    "bar.clickPaused",              {zh: "检测到鼠标点击，宏临时暂停", en: "Mouse click detected - macro paused"},
    "bar.clickResumed",             {zh: "宏已从鼠标点击暂停中恢复", en: "Macro resumed after the click pause"},

    ; ---------- 状态栏（自动嬗变） ----------
    "bar.transmuteProgress",        {zh: "正在处理第 {1}/{2} 个物品[{3}]（F3 取消）"
                                   , en: "Processing item {1}/{2} [{3}] (F3 cancels)"},
    "bar.transmuteLabel.red",       {zh: "红装重塑", en: "reforge legendary"},
    "bar.transmuteLabel.yellow",    {zh: "黄装升传奇", en: "rare to legendary"},
    "bar.transmuteNoTargetRed",     {zh: "自动嬗变：装备栏/背包没有识别到暗金/传奇装备"
                                   , en: "Auto transmute: no legendary or unique items found in the inventory and stash"},
    "bar.transmuteNoTargetRedYellow", {zh: "自动嬗变：装备栏/背包没有识别到传奇/暗金或稀有装备"
                                   , en: "Auto transmute: no legendary, unique or rare items found in the inventory and stash"},
    "bar.transmuteCancelled",       {zh: "自动嬗变已取消，已处理 {1}/{2} 个物品"
                                   , en: "Auto transmute cancelled, processed {1}/{2} items"},
    "bar.transmuteDone",            {zh: "已处理 {1} 个物品", en: "Processed {1} items"},
    "bar.transmuteFailed",          {zh: "配方流程失败: {1}", en: "Workflow failed: {1}"},

    ; ---------- 预设 / 保存 ----------
    "preset.default",               {zh: "配置{1}", en: "Profile {1}"},
    "status.settingsSaved",         {zh: "设置已保存 [{1}]", en: "Settings saved [{1}]"},
    "status.settingsSaveFailed",    {zh: "保存设置失败: {1}", en: "Failed to save settings: {1}"},
    "status.presetStopFirst",       {zh: "请先停止宏再切换预设", en: "Stop the macro before switching profiles"},
    "status.presetSwitched",        {zh: "已切换到: {1}", en: "Switched to: {1}"},
    "status.presetSwitchFailed",    {zh: "切换预设失败: {1}", en: "Failed to switch profile: {1}"},
    "status.tabStopFirst",          {zh: "请先停止宏再切换配置", en: "Stop the macro before switching profiles"},
    "status.presetRenamed",         {zh: "配置{1}已重命名为: {2}", en: "Profile {1} renamed to: {2}"},
    "rename.title",                 {zh: "重命名配置{1}", en: "Rename profile {1}"},
    "rename.prompt",                {zh: "请输入新的配置名称:", en: "Enter a new profile name:"},
    "status.themeStopFirst",        {zh: "请先停止宏再切换主题", en: "Stop the macro before switching themes"},
    "status.themeFailed",           {zh: "切换主题失败: {1}", en: "Failed to switch theme: {1}"},
    "status.langStopFirst",         {zh: "请先停止宏再切换语言", en: "Stop the macro before switching languages"},
    "status.langFailed",            {zh: "切换语言失败: {1}", en: "Failed to switch language: {1}"},

    ; ---------- 启停键 ----------
    "status.hotkeyInvalid",         {zh: "启停键无效，已保留: {1}", en: "Invalid toggle key, kept: {1}"},
    "status.hotkeyRegisterFailed",  {zh: "启停键注册失败: {1}", en: "Failed to register the toggle key: {1}"},
    "status.hotkeySet",             {zh: "启停键已设置为: {1}", en: "Toggle key set to: {1}"},

    ; ---------- 调试日志开关 ----------
    "status.logEnabled",            {zh: "调试日志已开启", en: "Debug log enabled"},
    "status.logDisabled",           {zh: "调试日志已关闭", en: "Debug log disabled"},
    "status.logCleared",            {zh: "日志已清理，释放 {1} KB", en: "Logs cleared, {1} KB freed"},

    ; ---------- 血量检测 ----------
    "health.off",                   {zh: "血量检测已关闭（回到按间隔定时喝药）", en: "HP check off - back to a fixed potion interval"},
    "health.onAdjusted",            {zh: "血量检测已开启：喝药间隔已从 {1}ms 调整为 1500ms"
                                   , en: "HP check on - potion interval changed from {1} ms to 1500 ms"},
    "health.on",                    {zh: "血量检测已开启，点「检测血球」可确认识别结果"
                                   , en: "HP check on - click `"Test globe`" to verify detection"},
    "health.testing",               {zh: "3 秒后检测血球，请切回游戏并让血球露出来…"
                                   , en: "Testing the globe in 3 seconds - switch back to the game and keep the globe visible…"},
    "health.testAborted",           {zh: "检测中断：暗黑4 窗口不在前台，血球被别的窗口挡住时读不到"
                                   , en: "Test aborted: the D4 window is not in the foreground, so the globe is covered and cannot be read"},
    "health.result.drink",          {zh: "会喝药", en: "drinks"},
    "health.result.noDrink",        {zh: "不喝药", en: "no potion"},
    "health.result.ok",             {zh: "血球已识别：球心({1},{2}) 半径{3} · 当前血量≈{4}%（阈值 {5}% → {6}）"
                                   , en: "Globe detected: center ({1},{2}) radius {3} · HP ≈ {4}% (threshold {5}% → {6})"},
    "health.result.shield",         {zh: "血球被护盾遮挡（护盾占比 {1}%），读不到血量"
                                   , en: "Globe covered by a shield ({1}%), HP cannot be read"},
    "health.result.fail",           {zh: "未识别到血球：{1}", en: "Globe not detected: {1}"},
    "health.ui.ok",                 {zh: "血量 {1}%（阈值 {2}%）", en: "HP {1}% (threshold {2}%)"},
    "health.ui.shield",             {zh: "护盾遮挡血球，读不到血量", en: "Shield covers the globe, HP unreadable"},
    "health.ui.fail",               {zh: "未识别到血球", en: "Globe not detected"},

    ; ---------- 血球识别失败原因（显示在状态栏 / 检测结果里） ----------
    "globe.err.capture",            {zh: "屏幕截图失败", en: "screen capture failed"},
    "globe.err.noLiquid",           {zh: "血球内没有检测到血液（窗口液体占比 {1}%；取景球心 {2},{3}）"
                                   , en: "no blood found inside the globe (liquid ratio {1}%; sampled center {2},{3})"},
    "globe.retry.soft",             {zh: "液面附近缺少亮红液体，已按保守方式估算"
                                   , en: "little bright red near the liquid surface, estimated conservatively"}
)

/**
 * 读取当前语言设置（启动时调用，早于界面创建）
 * 默认中文：文件缺失、取值异常都回落到 zh
 */
Lang_Init() {
    global appLang

    try {
        saved := IniRead(A_ScriptDir "\settings.ini", "UI", "Language", "zh")
        appLang := (saved = "en") ? "en" : "zh"
    } catch {
        appLang := "zh"
    }
}

/**
 * 取指定语言的文本（内部用，主要给跨语言比较/格式化）
 * @param {String} key - 语言表键名
 * @param {String} lang - "zh" 或 "en"
 * @param {Any} params - 依次替换 {1}、{2}…
 * @returns {String} 文本；键名不存在时返回键名本身（便于发现漏配）
 */
Lang_Get(key, lang, params*) {
    global LANG_TABLE

    if !LANG_TABLE.Has(key)
        return key

    entry := LANG_TABLE[key]
    text := (lang = "en") ? entry.en : entry.zh
    if (text = "")
        text := entry.zh

    for i, param in params
        text := StrReplace(text, "{" i "}", param)

    return text
}

/**
 * 取当前语言的界面文字
 */
L(key, params*) {
    global appLang
    return Lang_Get(key, appLang, params*)
}

/**
 * 语言按钮上的文字：显示"点击后切换到"的语言
 */
Lang_SwitchButtonText() {
    return L("main.lang.button")
}

/**
 * 预设名称归一化：没被用户改过的默认名字（"配置1"/"Profile 1"）跟着当前语言显示
 * @param {String} name - settings.ini 里存的名字
 * @param {Integer} index - 预设序号 (1-4)
 * @returns {String} 显示用名称
 */
Lang_NormalizePresetName(name, index) {
    if (name = "")
        return L("preset.default", index)

    isDefault := (name = Lang_Get("preset.default", "zh", index))
        || (name = Lang_Get("preset.default", "en", index))
    return isDefault ? L("preset.default", index) : name
}

; 启动即确定语言，后续所有 L() 调用都以它为准
Lang_Init()
