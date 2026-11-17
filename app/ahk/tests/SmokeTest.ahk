; 冒烟测试：加载全部模块 + 创建界面，1.5 秒后自动退出
; 用来确认改动后「脚本能正常加载、界面能正常创建、设置能正常保存」
; 运行：AutoHotkey64.exe tests\SmokeTest.ahk，结果写在 tests\smoke_result.txt
#Requires AutoHotkey v2.0
#SingleInstance Off
#NoTrayIcon

; 与应用完全一致的模块加载顺序
#Include "..\utils\Lang.ahk"
#Include "..\core\Version.ahk"
#Include "..\utils\Json.ahk"
#Include "..\core\Updates.ahk"
#Include "..\gui\UpdatesGUI.ahk"
#Include "..\core\GlobalVars.ahk"
#Include "..\utils\Logger.ahk"
#Include "..\utils\Settings.ahk"
#Include "..\core\WindowManager.ahk"
#Include "..\core\TimerManager.ahk"
#Include "..\core\MacroControl.ahk"
#Include "..\core\HotkeyManager.ahk"
#Include "..\core\WorkflowHotkeys.ahk"
#Include "..\gui\ModernUI.ahk"
#Include "..\gui\MainGUI.ahk"
#Include "..\gui\WorkflowHotkeysGUI.ahk"
#Include "..\gui\SkillControls.ahk"
#Include "..\gui\CubeWorkflowGUI.ahk"
#Include "..\functions\SkillSystem.ahk"
#Include "..\functions\MouseActions.ahk"
#Include "..\functions\UtilityActions.ahk"
#Include "..\functions\InventoryScanner.ahk"
#Include "..\functions\CubeProfiles.ahk"
#Include "..\functions\DetectionRegions.ahk"
#Include "..\gui\DetectionRegionsGUI.ahk"
#Include "..\functions\CubeRunner.ahk"
#Include "..\functions\WindowDiagnostics.ahk"
#Include "..\hotkeys\GameHotkeys.ahk"
#Include "DetectionRegionsTest.ahk"
#Include "WorkflowHotkeysTest.ahk"

report := A_ScriptDir "\smoke_result.txt"
if FileExist(report)
    FileDelete report
Append(text) {
    global report
    FileAppend text "`n", report, "UTF-8"
}

try {
    InitializeGUI()
    OnExit(CleanupOnExit, 0) ; Test never starts combat; do not release keys into other apps.
    Append "GUI 创建成功"
} catch as err {
    Append "GUI 创建失败: " err.Message " @" err.File ":" err.Line
}

try {
    D4Box_TestBindingsNative()
    Append "流程快捷键注册、持久化、回滚与原生界面正常"
} catch as err {
    Append "流程快捷键异常: " err.Message " @" err.File ":" err.Line
}

try {
    D4Box_OpenCubeEditor()
    if !IsObject(D4Box_CubeUI) || D4Box_CubeUI.list.GetCount() < 1
        throw Error("Workflow editor has no steps")
    D4Box_CubeUI.profile.builtin := false
    D4Box_CubeUI.profile.steps.Push(D4Box_CubeStep("Load item|rightclick|any|0.7|0.8|250|-|24|3000"))
    D4Box_CubeFillStep(D4Box_CubeUI.profile.steps.Length)
    editedStep := D4Box_CubeReadStep()
    if editedStep.action != "rightclick" || editedStep.x != 0 || editedStep.y != 0
        || D4Box_CubeUI.action.Text != L("cube.rightclick")
        throw Error("Right-click action did not round trip through the editor")
    if D4Box_CubeUI.x.Enabled || D4Box_CubeUI.y.Enabled || D4Box_CubeUI.captureStep.Enabled
        throw Error("Item selection must not accept fixed coordinates")
    D4Box_CubeUI.source.Choose(2)
    D4Box_CubeSourceChanged()
    if !InStr(D4Box_CubeUI.note.Text, L("cube.mouseTarget"))
        throw Error("Single-item target hint is missing")
    D4Box_CubeUI.source.Choose(1)
    D4Box_CubeSourceChanged()
    if !InStr(D4Box_CubeUI.note.Text, L("cube.inventoryTarget"))
        throw Error("Inventory target hint is missing")
    D4Box_CubeUI.action.Choose(1)
    D4Box_CubeActionChanged()
    if !D4Box_CubeUI.x.Enabled || !D4Box_CubeUI.y.Enabled || !D4Box_CubeUI.captureStep.Enabled
        throw Error("Recipe actions must retain editable coordinates")
    D4Box_CubeReadStep()
    D4Box_CubeUI.profile.steps.Pop()
    D4Box_CubeUI.profile.steps.Push(D4Box_CubeStep("Optional|confirm_if_present|any|0.4|0.6|180|AABBCC|24|1500|0.1|0.2"))
    D4Box_CubeFillStep(D4Box_CubeUI.profile.steps.Length)
    editedStep := D4Box_CubeReadStep()
    if editedStep.action != "confirm_if_present" || editedStep.probeX != 0.1 || editedStep.probeY != 0.2
        || editedStep.x != 0.4 || editedStep.y != 0.6 || D4Box_CubeUI.action.Text != L("cube.confirm_if_present")
        throw Error("Optional confirmation fields did not round trip")
    if !D4Box_CubeUI.probeX.Enabled || !D4Box_CubeUI.probeY.Enabled || !D4Box_CubeUI.captureProbe.Enabled
        || D4Box_CubeUI.captureStep.Text != L("cube.captureConfirmClick")
        throw Error("Optional confirmation capture controls are missing")
    D4Box_CubeApplyCapture(D4Box_CubeUI, "confirmClick", 0.45, 0.65, "FFFFFF")
    editedStep := D4Box_CubeReadStep()
    if editedStep.x != 0.45 || editedStep.y != 0.65 || editedStep.probeX != 0.1 || editedStep.color != "AABBCC"
        throw Error("Capturing accept position changed probe/color")
    D4Box_CubeApplyCapture(D4Box_CubeUI, "probe", 0.15, 0.25, "112233")
    editedStep := D4Box_CubeReadStep()
    if editedStep.x != 0.45 || editedStep.y != 0.65 || editedStep.probeX != 0.15 || editedStep.probeY != 0.25 || editedStep.color != "112233"
        throw Error("Capturing probe changed accept position")
    D4Box_CubeUI.action.Choose(3)
    D4Box_CubeActionChanged()
    if D4Box_CubeUI.probeX.Enabled || D4Box_CubeUI.probeY.Enabled || D4Box_CubeUI.captureProbe.Enabled
        || D4Box_CubeUI.captureStep.Text != L("cube.captureStep")
        throw Error("Ordinary accept must not enable probe controls")
    D4Box_CubeUI.profile.steps.Pop()
    D4Box_TestDetectionUI()
    D4Box_CubeUI.dirty := false
    D4Box_CubeCloseEditor()
    Append "配方流程编辑器创建正常"
} catch as err {
    Append "配方流程编辑器异常: " err.Message " @" err.File ":" err.Line
}

; 两个按钮的回调直接调用一遍（不依赖点击）
try {
    D4Box_TestDetectionNative()
    Append "检测区域原生截图与匹配正常"
} catch as err {
    Append "检测区域原生匹配异常: " err.Message " @" err.File ":" err.Line
}

try {
    ; 左右键技能行的按键框：下拉项 / 键盘键 / 鼠标侧键 / 无效输入
    ctrl := mouseControls.left.key
    got := []
    for key in ["5", "RButton", "xbutton1", "LButton"] {
        SetMouseSlotKey(ctrl, key)
        got.Push(ResolveMouseSlotKey(ctrl))
    }
    ctrl.Text := "不存在的键"
    got.Push(ResolveMouseSlotKey(ctrl))
    ; 输入任意字符串后：运行/存盘仍用最后一次有效按键，失去焦点时恢复显示
    SetMouseSlotKey(ctrl, "5")
    ctrl.Text := "abc"
    OnMouseSlotKeyChange(ctrl)
    got.Push(GetMouseSlotKey(ctrl))
    OnMouseSlotKeyBlur(ctrl)
    got.Push(ctrl.Text)
    SetMouseSlotKey(ctrl, "LButton")
    joined := ""
    for k in got
        joined .= k "|"
    if (joined = "5|RButton|XButton1|LButton||5|5|")
        Append "左右键按键解析正常"
    else
        Append "左右键按键解析失败: " joined
} catch as err {
    Append "左右键按键解析异常: " err.Message " @" err.File ":" err.Line
}
try {
    SaveSettings()
    Append "保存设置正常"
} catch as err {
    Append "保存设置异常: " err.Message " @" err.File ":" err.Line
}

try {
    D4Box_TestRuntimeFooter()
    Append "分页控件归属、公共运行状态与取消入口正常"
} catch as err {
    Append "公共运行状态异常: " err.Message " @" err.File ":" err.Line
}

try {
    D4Box_TestMainVariants()
    Append "中英文与明暗主题页面创建正常"
} catch as err {
    Append "主界面主题异常: " err.Message " @" err.File ":" err.Line
}

try {
    for lang in ["zh", "en"] {
        appLang := lang
        D4Box_ShowUpdates(false)
        ui := D4Box_UpdateUI
        D4Box_UpdateFinished(ui, false, "")
        if ui.status.Text != L("update.none") || ui.open.Enabled
            throw Error("Missing-release UI state mismatch")
        D4Box_UpdateFinished(ui, {version: D4Box_Version(), url: ""}, "")
        if ui.status.Text != L("update.latest") || ui.open.Enabled
            throw Error("Current-version UI state mismatch")
        D4Box_UpdateFinished(ui, {version: "9999.0.0", url: "https://github.com/liuapart/d4box/releases/tag/d4box-v9999.0.0"}, "")
        if !ui.open.Enabled || !InStr(ui.status.Text, "9999.0.0")
            throw Error("Update-available UI state mismatch")
        WinRedraw(ui.gui.Hwnd)
        Sleep 100
        WinGetClientPos(&ux, &uy, &uw, &uh, ui.gui.Hwnd)
        preview := CaptureScreenRegion(ux, uy, uw, uh)
        D4Box_DetectorWriteBitmap(preview, 0, 0, uw, uh, A_ScriptDir "\.runtime\update-" lang ".bmp")
        D4Box_CloseUpdates()
        D4Box_UpdateFinished(ui, false, "late callback")
        if IsObject(D4Box_UpdateUI)
            throw Error("Closed update window recreated")
        D4Box_ShowUpdates(false)
        D4Box_UpdateFinished(D4Box_UpdateUI, false, "HTTP 403")
        if !InStr(D4Box_UpdateUI.status.Text, "HTTP 403") || D4Box_UpdateUI.open.Enabled || !D4Box_UpdateUI.check.Enabled
            throw Error("Update error/retry UI state mismatch")
        D4Box_CloseUpdates()
    }
    Append "版本更新窗口状态、关闭与重试正常"
} catch as err {
    Append "版本更新窗口异常: " err.Message " @" err.File ":" err.Line
}

SetTimer(Quit, -1500)
Quit(*) {
    global myGui
    try {
        myGui.Destroy()
    }
    ExitApp
}
