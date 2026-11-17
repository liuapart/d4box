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
    "main.saveMacro", {zh: "保存宏设置", en: "Save macro settings"},
    "runtime.workflow", {zh: "流程执行中 · {1}", en: "Workflow running · {1}"},
    "runtime.cancelling", {zh: "正在取消流程 · {1}", en: "Cancelling workflow · {1}"},
    "runtime.step", {zh: "当前步骤：{1}", en: "Current step: {1}"},
    "runtime.preparing", {zh: "正在准备并检查流程…", en: "Preparing and checking workflow…"},
    "runtime.macroSuspended", {zh: "按键宏已暂停 · {1} · 正在执行流程", en: "Macro suspended · {1} · Workflow in progress"},
    "runtime.macroRunning", {zh: "按键宏运行中 · {1}", en: "Macro running · {1}"},
    "runtime.macroPaused", {zh: "按键宏已暂停 · {1}", en: "Macro paused · {1}"},
    "runtime.macroKey", {zh: "游戏内按 {1} 停止宏，或点击右侧“停止宏”。", en: "Press {1} in game or use Stop macro to stop."},
    "runtime.stopMacro", {zh: "停止宏", en: "Stop macro"},
    "runtime.cancelWorkflow", {zh: "取消流程", en: "Cancel workflow"},
    "runtime.idleHint", {zh: "流程使用各自的快捷键执行；按键宏请在“按键宏设置”页启动。", en: "Run workflows with their assigned keys; start macros on the Skill Macro tab."},
    "binding.title", {zh: "流程快捷键", en: "Workflow hotkeys"},
    "binding.subtitle", {zh: "为保存好的流程分配快捷键。默认绑定始终保留，键位和流程均可修改。", en: "Assign keys to saved workflows. The default binding stays; its key and workflow are editable."},
    "binding.key", {zh: "快捷键", en: "Hotkey"},
    "binding.workflow", {zh: "执行流程", en: "Workflow"},
    "binding.state", {zh: "绑定类型", en: "Binding type"},
    "binding.default", {zh: "默认 · 不可删除", en: "Default (required)"},
    "binding.custom", {zh: "自定义", en: "Custom"},
    "binding.add", {zh: "新增绑定", en: "Add binding"},
    "binding.edit", {zh: "编辑绑定", en: "Edit binding"},
    "binding.delete", {zh: "删除", en: "Delete"},
    "binding.manage", {zh: "编辑流程…", en: "Edit workflows…"},
    "binding.rename", {zh: "重命名配置", en: "Rename profile"},
    "binding.save", {zh: "保存并生效", en: "Save and apply"},
    "binding.saveOnly", {zh: "保存流程", en: "Save workflow"},
    "binding.savedOnly", {zh: "流程已保存，原有快捷键绑定保持不变。", en: "Workflow saved. Existing key assignments are unchanged."},
    "binding.savedDraft", {zh: "草稿已保存，执行前还需完善：{1}", en: "Draft saved. Before running, complete: {1}"},
    "binding.cancel", {zh: "取消", en: "Cancel"},
    "binding.hint", {zh: "切回游戏后按绑定键执行。执行中再次按任一流程快捷键取消；长按不会重复执行。`n绑定独立于按键宏配置，修改后立即保存。", en: "In game, press a bound key to run; any workflow key cancels while running. Holding a key does not repeat.`nBindings are global and saved immediately."},
    "binding.defaultHint", {zh: "默认绑定不能删除，可以修改键位和流程。", en: "Default binding: change its key or workflow."},
    "binding.keyHint", {zh: "点击快捷键框后按键，可组合 Ctrl / Alt / Shift。", en: "Press a key in the box; Ctrl / Alt / Shift are supported."},
    "binding.invalidKey", {zh: "请设置键盘快捷键，可组合 Ctrl/Alt/Shift；Tab、鼠标键与单独修饰键不可用。", en: "Use a keyboard key with Ctrl/Alt/Shift. Tab, mouse keys and modifier-only keys are reserved."},
    "binding.reserved", {zh: "Ctrl+Alt+F8 用于窗口诊断，请换一个快捷键。", en: "Ctrl+Alt+F8 is reserved for window diagnostics."},
    "binding.duplicate", {zh: "快捷键 {1} 已有绑定，请换一个。", en: "Hotkey {1} is already assigned."},
    "binding.macroConflict", {zh: "与宏启停键 {1} 冲突，请使用不同的主键。", en: "Conflicts with macro toggle {1}; choose a different base key."},
    "binding.missing", {zh: "流程不存在：{1}", en: "Missing workflow: {1}"},
    "binding.chooseWorkflow", {zh: "请选择一个已保存的流程。", en: "Choose a saved workflow."},
    "binding.defaultRequired", {zh: "必须保留一条默认绑定，不能删除。", en: "Exactly one default binding is required; it cannot be deleted."},
    "binding.limit", {zh: "支持 1～32 条快捷键绑定。", en: "Use between 1 and 32 bindings."},
    "binding.invalidFile", {zh: "快捷键配置格式无效，请检查 workflow-hotkeys.ini。", en: "Invalid workflow-hotkeys.ini configuration."},
    "binding.stopFirst", {zh: "请先停止按键宏、流程和截图采集，再修改绑定。", en: "Stop the macro, workflow and capture before changing bindings."},
    "binding.referenced", {zh: "此流程仍被 {1} 绑定，请先修改或删除相关快捷键绑定。", en: "This workflow is bound to {1}. Reassign or remove those bindings first."},
    "binding.unavailable", {zh: "未启用 · 需修复", en: "Disabled: needs repair"},
    "binding.loadError", {zh: "快捷键未启用：{1}", en: "Workflow hotkeys disabled: {1}"},
    "cube.selected", {zh: "默认绑定已选用流程：{1}", en: "Default binding now uses: {1}"},
    "cube.newProfile", {zh: "新建流程", en: "New workflow"},
    "cube.defaultName", {zh: "自动嬗变背包物品", en: "Transmute inventory items"},
    "cube.step.transmute", {zh: "选择嬗变", en: "Select transmute"},
    "cube.step.execute", {zh: "执行", en: "Execute"},
    "cube.step.confirm", {zh: "接受", en: "Accept"},
    "cube.step.affix", {zh: "添加词缀", en: "Add affix"},
    "cube.step.upgrade", {zh: "升级至传奇", en: "Upgrade to legendary"},
    "cube.step.clear", {zh: "清除", en: "Clear"},
    "cube.manualConditions", {zh: "单次物品模式的全部步骤需使用“全部／无条件”，不按背包品质分支。", en: "Single-item mode requires unconditional steps; inventory quality routing is unavailable."},
    ; ---------- 魔盒流程 ----------
    "cube.title", {zh: "魔盒配方流程", en: "Cube workflows"},
    "cube.copy", {zh: "复制为新流程", en: "Copy as new"},
    "cube.deleteProfile", {zh: "删除流程", en: "Delete workflow"},
    "cube.name", {zh: "流程名称", en: "Name"},
    "cube.source", {zh: "处理来源", en: "Source"},
    "cube.inventory", {zh: "扫描背包物品", en: "Scan inventory"},
    "cube.manual", {zh: "单次物品（鼠标指向）", en: "Single item (under pointer)"},
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
    "cube.saveUse", {zh: "保存并设为默认流程", en: "Save as default workflow"},
    "cube.preview", {zh: "预览执行顺序", en: "Preview steps"},
    "cube.close", {zh: "关闭", en: "Close"},
    "cube.red", {zh: "传奇／暗金色", en: "Legendary/unique color"},
    "cube.yellow", {zh: "稀有（黄）", en: "Rare (yellow)"},
    "cube.blue", {zh: "魔法（蓝）", en: "Magic (blue)"},
    "cube.redyellow", {zh: "传奇色＋稀有", en: "Legendary color + rare"},
    "cube.any", {zh: "全部／无条件", en: "All / unconditional"},
    "cube.select", {zh: "选择配方", en: "Select recipe"},
    "cube.rightclick", {zh: "右键选择物品", en: "Right-click item"},
    "cube.mouseTarget", {zh: "按流程快捷键时鼠标指向的物品", en: "Item under pointer when the workflow key was pressed"},
    "cube.inventoryTarget", {zh: "当前遍历的背包物品", en: "Current inventory item"},
    "cube.itemTargetNote", {zh: "右键目标：{1}。X/Y 自动确定，无需采集。背包流程已有此步骤时不会重复自动放入。", en: "Right-click target: {1}. X/Y is automatic; no capture needed. Explicit selection replaces automatic inventory loading."},
    "cube.noItemTarget", {zh: "没有可选择的物品目标，已停止流程。", en: "No item target is available; workflow stopped."},
    "cube.execute", {zh: "点击执行", en: "Click execute"},
    "detect.title", {zh: "检测区域库", en: "Detection library"},
    "detect.new", {zh: "新建区域", en: "New region"},
    "detect.delete", {zh: "删除", en: "Delete"},
    "detect.variation", {zh: "颜色容差", en: "Color tolerance"},
    "detect.margin", {zh: "搜索余量（像素）", en: "Search margin (px)"},
    "detect.node", {zh: "检测节点", en: "Detector"},
    "detect.useReady", {zh: "设为就绪检测", en: "Use as ready check"},
    "detect.capture", {zh: "截图并框选区域", en: "Capture and select region"},
    "detect.testPresent", {zh: "面板出现时测试", en: "Test with panel present"},
    "detect.testAbsent", {zh: "面板关闭时测试", en: "Test with panel absent"},
    "detect.save", {zh: "保存区域", en: "Save region"},
    "detect.passed", {zh: "双向测试已通过", en: "Both tests passed"},
    "detect.ok", {zh: "通过", en: "passed"},
    "detect.pending", {zh: "待测试", en: "pending"},
    "detect.draft", {zh: "待测试（草稿）", en: "Draft: tests required"},
    "detect.present", {zh: "面板出现", en: "Panel present"},
    "detect.absent", {zh: "面板关闭", en: "Panel absent"},
    "detect.retest", {zh: "采集或参数已变化，请分别在面板出现、面板关闭时重新测试。", en: "Capture/settings changed. Repeat tests with the panel present and absent."},
    "detect.help", {zh: "先命名，再截图框选固定标题或图标（8×8～512×256）。分别在面板出现和关闭时测试，通过后保存。测试只读取画面，不点击。调整分辨率、DPI 或游戏 UI 缩放后重新采集。", en: "Name the region, capture a fixed title/icon (8×8 to 512×256), test with the panel present and absent, then save. Tests only read pixels. Recapture after resolution, DPI or game UI scale changes."},
    "detect.saved", {zh: "已保存。双向测试通过的区域可在流程中选择；名称修改不影响引用。", en: "Saved. Regions passing both tests can be selected in workflows. Renaming preserves references."},
    "detect.invalid", {zh: "检测区域名称或配置无效；名称不能包含换行或竖线。", en: "Invalid detection region/name; no newlines or pipes allowed."},
    "detect.limits", {zh: "区域颜色容差为 0～100，搜索余量为 0～80 像素。", en: "Region color tolerance: 0–100; search margin: 0–80 pixels."},
    "detect.bounds", {zh: "请选择游戏画面内 8×8～512×256 的稳定小区域。", en: "Select a stable region inside the game, 8×8 to 512×256 pixels."},
    "detect.untested", {zh: "检测区域“{1}”尚未通过当前参数的出现/关闭测试。", en: "Detector '{1}' needs present/absent tests for its current settings."},
    "detect.missingImage", {zh: "检测区域“{1}”的模板文件缺失，请重新采集。", en: "Template missing for '{1}'; recapture it."},
    "detect.duplicate", {zh: "已有同名检测区域，请使用其他名称。", en: "A region with this name already exists."},
    "detect.missing", {zh: "找不到检测区域：{1}。请重新选择。", en: "Detector not found: {1}. Select another detector."},
    "detect.inUse", {zh: "此区域被已保存流程或当前草稿引用，请先解除引用。", en: "This region is referenced by a saved workflow or open draft. Remove the references first."},
    "detect.deleteQuestion", {zh: "删除此检测区域？已采集的模板文件会保留在本机运行目录。", en: "Delete this region? Captured images remain in the local runtime directory."},
    "detect.frame", {zh: "检测区域“{1}”的采集分辨率/DPI 与当前游戏不符，请重新采集并测试。", en: "Resolution/DPI differs for '{1}'. Recapture and retest."},
    "detect.writeFailed", {zh: "截图或模板文件写入失败。", en: "Screenshot/template write failed."},
    "detect.pauseMacro", {zh: "请先暂停战斗宏，再采集或测试检测区域。", en: "Pause the combat macro before capturing or testing."},
    "detect.captureFirst", {zh: "请先截图并框选检测区域。", en: "Capture and select a region first."},
    "detect.captureHint", {zh: "3 秒内切回游戏并显示目标面板；截图后在冻结画面上拖动框选，Esc 取消。", en: "Show the target panel within 3 seconds. Drag a region on the frozen screenshot; Esc cancels."},
    "detect.testHint", {zh: "3 秒内切回游戏，准备好对应的出现/关闭状态；本次测试不会点击游戏。", en: "Return to the game within 3 seconds with the expected panel state. This test sends no clicks."},
    "detect.dragHint", {zh: "冻结截图：拖动框选固定标题或图标；松开完成，Esc 取消。", en: "Frozen screenshot: drag over a fixed title/icon; release to finish, Esc to cancel."},
    "detect.testPassed", {zh: "{1}测试通过（连续 3 次）。请完成另一项测试并保存。", en: "{1} test passed (3 samples). Complete the other test and save."},
    "detect.testFailed", {zh: "{1}测试失败：检测结果不符合预期。请检查模板、容差及画面状态。", en: "{1} test failed. Check the template, tolerance and screen state."},
    "detect.legacy", {zh: "单点颜色（原设置）", en: "Single pixel (existing settings)"},
    "detect.unavailable", {zh: "不可用／需重测：{1}", en: "Unavailable / retest: {1}"},
    "detect.workflowHint", {zh: "使用区域库中的模板和参数；X/Y 仍是接受按钮位置。等待检测步骤只读取画面。修改区域后需重新测试保存。", en: "Uses the library template/settings. X/Y remains the accept button position. Wait steps only read pixels. Retest and save after changing a region."},
    "cube.confirm_if_present", {zh: "检测到后点击接受", en: "Click accept if present"},
    "cube.probeX", {zh: "检测 X", en: "Probe X"},
    "cube.probeY", {zh: "检测 Y", en: "Probe Y"},
    "cube.captureConfirmProbe", {zh: "采集检测点与颜色", en: "Capture probe position and color"},
    "cube.captureConfirmClick", {zh: "采集接受位置", en: "Capture accept position"},
    "cube.confirmNote", {zh: "X/Y 为接受按钮位置，检测 X/Y 为确认框稳定色块。连续两次颜色匹配且点击前仍匹配才点击；超时未检测到则跳过。延迟仅在点击后生效。", en: "X/Y: accept button. Probe X/Y: stable dialog color. Two matches plus a check before clicking are required. Timeout skips; delay applies only after clicking."},
    "cube.confirmClicked", {zh: "检测到确认框，已点击接受", en: "Confirmation detected; accept clicked"},
    "cube.confirmSkipped", {zh: "未检测到确认框，已跳过接受", en: "No confirmation detected; accept skipped"},
    "cube.confirm", {zh: "点击接受", en: "Click accept"},
    "cube.clear", {zh: "清除物品", en: "Clear item"},
    "cube.wait", {zh: "等待", en: "Wait"},
    "cube.check", {zh: "等待检测匹配", en: "Wait for detection"},
    "cube.builtin", {zh: "[内置]", en: "[Built-in]"},
    "cube.discard", {zh: "当前修改尚未保存，是否放弃？", en: "Discard unsaved changes?"},
    "cube.noReady", {zh: "未采集就绪标记。自定义流程运行前需要就绪标记及末尾颜色检查。", en: "No ready marker. Custom workflows need a ready marker and a final color check."},
    "cube.readySet", {zh: "已设置魔盒就绪标记", en: "Cube ready marker captured"},
    "cube.builtinNote", {zh: "内置旧版流程：保留原坐标和延时，未验证游戏结果。修改请先复制；升级黄装沿用主界面勾选项。", en: "Legacy built-in: original coordinates/delays, no result verification. Copy to edit; rare upgrades follow the main checkbox."},
    "cube.customNote", {zh: "坐标范围 0～1。品质条件按放入前的品质判断。添加颜色检查可等待界面变化；保存后在主界面分配快捷键。", en: "Coordinates: 0–1. Conditions use original quality. Add color checks to wait for UI changes. Assign a hotkey in the main window after saving."},
    "cube.newStep", {zh: "新步骤", en: "New step"},
    "cube.copyFirst", {zh: "内置流程不可覆盖，请先复制为新流程。", en: "Copy the built-in workflow before editing."},
    "cube.copySuffix", {zh: "副本", en: "copy"},
    "cube.saved", {zh: "已保存并设为默认流程，运行前配置检查通过。切回游戏按绑定键执行，再按任一流程快捷键取消。", en: "Saved and selected; pre-run configuration checks passed. In game, use a bound workflow key to run/cancel."},
    "cube.savedDraft", {zh: "已保存并设为默认流程，但当前尚不能执行：{1}", en: "Saved and selected, but not ready to run: {1}"},
    "cube.cannotRun", {zh: "配方流程暂不能执行", en: "Workflow cannot run yet"},
    "cube.missingReady", {zh: "缺少就绪标记：请在魔盒打开且准备好执行时，使用“采集就绪标记”。", en: "Missing ready marker: use Capture ready marker while the cube is open and ready."},
    "cube.missingFinal", {zh: "{1}路径缺少末尾检查：最后添加“等待检测匹配”，采集流程完成后的界面标记。点击步骤中的颜色不会自动检查。", en: "The {1} route needs a final Wait for detection step sampling the completed UI. Colors in click steps are not checked automatically."},
    "cube.deleteQuestion", {zh: "删除此自定义流程？当前选用将恢复为内置嬗变。", en: "Delete this workflow and select the built-in transmute workflow?"},
    "cube.previewNote", {zh: "仅展示配置顺序，不移动鼠标、不点击，也不验证游戏结果。", en: "Configuration preview only: no mouse movement, clicks, or game verification."},
    "cube.loadItem", {zh: "右键放入当前背包物品 → 等待 250ms", en: "Right-click inventory item → wait 250ms"},
    "cube.captureHint", {zh: "3 秒内切回游戏，把鼠标放到要采集的按钮或稳定色块上。", en: "Return to the game and point at the desired button/stable color within 3 seconds."},
    "cube.ready", {zh: "魔盒就绪", en: "Cube ready"},
    "cube.busy", {zh: "流程正在执行，请先按任一流程快捷键取消。", en: "A workflow is running. Press any workflow hotkey to cancel first."},
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
    "cube.needChecks", {zh: "自定义流程需采集就绪标记，并在品质 {1} 路径的最后添加“等待检测匹配”（在清除之后）。点击步骤采集的颜色不会自动用于检查。", en: "Capture a ready marker and end the {1} route with Wait for detection (after Clear). Colors captured for click steps are not checked automatically."},
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
    "cube.progress", {zh: "{1}：处理 {2}/{3}（流程快捷键取消）", en: "{1}: processing {2}/{3} (workflow hotkey cancels)"},
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
    "main.hint",                    {zh: "仅游戏前台启动流程 · Tab / 鼠标点击可中断流程"
                                   , en: "Start workflows in game · Tab / mouse clicks interrupt"},
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
    "bar.transmuteProgress",        {zh: "正在处理第 {1}/{2} 个物品[{3}]（流程快捷键取消）"
                                   , en: "Processing item {1}/{2} [{3}] (workflow hotkey cancels)"},
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
    "status.themeStopFirst",        {zh: "请先停止宏、流程及采集再切换主题", en: "Stop macros, workflows and capture before switching themes"},
    "status.themeFailed",           {zh: "切换主题失败: {1}", en: "Failed to switch theme: {1}"},
    "status.langStopFirst",         {zh: "请先停止宏、流程及采集再切换语言", en: "Stop macros, workflows and capture before switching languages"},
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
