; Pure workflow/model regression: uses a fake adapter, never sends game input.
#Requires AutoHotkey v2.0
#SingleInstance Off
#NoTrayIcon
#Include "..\utils\Lang.ahk"
#Include "..\core\GlobalVars.ahk"
#Include "..\utils\Logger.ahk"
#Include "..\utils\Settings.ahk"
#Include "..\core\WindowManager.ahk"
#Include "..\core\TimerManager.ahk"
#Include "..\core\MacroControl.ahk"
#Include "..\core\HotkeyManager.ahk"
#Include "..\gui\ModernUI.ahk"
#Include "..\gui\MainGUI.ahk"
#Include "..\gui\SkillControls.ahk"
#Include "..\gui\MouseControls.ahk"
#Include "..\gui\CubeWorkflowGUI.ahk"
#Include "..\functions\SkillSystem.ahk"
#Include "..\functions\MouseActions.ahk"
#Include "..\functions\UtilityActions.ahk"
#Include "..\functions\InventoryScanner.ahk"
#Include "..\functions\CubeProfiles.ahk"
#Include "..\functions\CubeRunner.ahk"
#Include "..\functions\HealthGlobe.ahk"
#Include "..\functions\ConditionSystem.ahk"
#Include "..\functions\WindowDiagnostics.ahk"

global D4Box_TestFailures := 0
global D4Box_TestReport := A_ScriptDir "\cube_result.txt"
if FileExist(D4Box_TestReport)
    FileDelete(D4Box_TestReport)

D4Box_Assert(condition, label) {
    global D4Box_TestFailures, D4Box_TestReport
    if !condition
        D4Box_TestFailures += 1
    FileAppend((condition ? "PASS " : "FAIL ") label "`n", D4Box_TestReport, "UTF-8")
}

D4Box_ExpectError(callback, label) {
    try {
        callback.Call()
        D4Box_Assert(false, label)
    } catch {
        D4Box_Assert(true, label)
    }
}

class D4Box_TestAdapter {
    clicks := 0
    checks := 0
    waits := 0
    cancelled := false
    cancelAfterClick := false
    failColor := false
    Check() {
        if this.cancelled
            throw Error("cancelled")
    }
    Report(step) {
    }
    Click(x, y, button) {
        this.Check()
        this.clicks += 1
    }
    Sleep(ms) {
        this.waits += 1
        if this.cancelAfterClick && this.clicks > 0
            this.cancelled := true
        this.Check()
    }
    WaitColor(step) {
        this.Check()
        this.checks += 1
        if this.failColor
            throw Error("color timeout")
    }
}

try {
    builtin := D4Box_CubeReadFile(D4Box_CubeBuiltinsPath(), true)["inventory_transmute"]
    D4Box_CubeValidate(builtin, true)
    red := D4Box_CubePlan(builtin, "red")
    yellow := D4Box_CubePlan(builtin, "yellow")
    D4Box_Assert(red.Length = 4 && yellow.Length = 10, "legacy route lengths")
    D4Box_Assert(red[1].y = 0.380 && yellow[7].y = 0.350, "distinct legacy transmute positions")
    D4Box_Assert(red[4].action = "clear" && yellow[10].action = "clear", "one final clear per item")
    D4Box_Assert(yellow[1].y = 0.233 && yellow[4].y = 0.720, "rare upgrade order")
    red[1].x := 0
    D4Box_Assert(builtin.steps[1].x = 0.445, "plan is a snapshot, not an alias")

    custom := D4Box_CubeCopy(builtin)
    custom.builtin := false
    custom.legacyYellow := false
    custom.id := "user_regression"
    custom.name := "中文流程 round trip"
    custom.source := "manual"
    custom.steps := [D4Box_CubeStep("Execute|execute|any|0.168|0.793|450|-|24|3000")]
    D4Box_ExpectError(() => D4Box_CubeValidate(custom, true), "custom flow requires visual checkpoints")
    guard := D4Box_CubeStep("Ready|check|any|0.1|0.2|0|AABBCC|24|3000")
    custom.ready := D4Box_CubeStepText(guard)
    custom.steps.Push(D4Box_CubeCopy(guard))
    D4Box_CubeValidate(custom, true)
    custom.steps[1].quality := "red"
    D4Box_ExpectError(() => D4Box_CubeValidate(custom, true), "manual source rejects inventory conditions")
    custom.steps[1].quality := "any"
    custom.steps[1].x := 1.1
    D4Box_ExpectError(() => D4Box_CubeValidate(custom), "out-of-range coordinate rejected")
    custom.steps[1].x := 0.168
    custom.steps[2].color := "invalid"
    D4Box_ExpectError(() => D4Box_CubeValidate(custom), "malformed color rejected")
    custom.steps[2].color := "AABBCC"
    D4Box_ExpectError(() => D4Box_CubeSave(builtin), "built-in cannot be overwritten")

    adapter := D4Box_TestAdapter()
    D4Box_RunCubePlan(D4Box_CubePlan(custom, "any"), adapter)
    D4Box_Assert(adapter.clicks = 1 && adapter.checks = 1, "adapter receives click then visual check")
    adapter := D4Box_TestAdapter()
    adapter.cancelled := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(custom.steps, adapter), "cancel before first step")
    D4Box_Assert(adapter.clicks = 0, "cancelled plan sends no input")
    adapter := D4Box_TestAdapter()
    adapter.cancelAfterClick := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(D4Box_CubePlan(builtin, "yellow"), adapter), "cancel interrupts between actions")
    D4Box_Assert(adapter.clicks = 1, "cancel stops remaining recipe actions")
    adapter := D4Box_TestAdapter()
    adapter.failColor := true
    plan := [guard, custom.steps[1]]
    D4Box_ExpectError(() => D4Box_RunCubePlan(plan, adapter), "failed visual checkpoint stops the plan")
    D4Box_Assert(adapter.clicks = 0, "no clicks after failed checkpoint")
    D4Box_Assert(D4Box_CubeColorMatches(0xAABBCC, 0xA9BACB, 1), "color tolerance inclusive")
    D4Box_Assert(!D4Box_CubeColorMatches(0xAABBCC, 0xA8BBCC, 1), "color tolerance rejects mismatched channel")

    isRunning := true
    isPaused := false
    previouslyPaused := false
    isAutoTransmuting := true
    D4Box_InterruptCube()
    D4Box_Assert(!isAutoTransmuting && isPaused && previouslyPaused, "manual interruption also pauses combat")
    isRunning := false
    isPaused := false
    previouslyPaused := false

    file := D4Box_CubeUserPath()
    backup := file ".test-backup-" A_TickCount
    existed := FileExist(file)
    if existed
        FileCopy(file, backup)
    try {
        D4Box_CubeSave(custom, custom.id)
        loaded := D4Box_CubeReadFile(file)[custom.id]
        D4Box_Assert(loaded.name = custom.name && loaded.steps.Length = 2, "INI unicode/steps round trip")
        D4Box_Assert(D4Box_CubeSelectedId() = custom.id, "selected workflow persists")
        custom.steps.Pop()
        D4Box_CubeSave(custom, custom.id)
        D4Box_Assert(IniRead(file, "Workflow." custom.id, "Step2", "missing") = "missing", "removed steps do not remain on disk")
        D4Box_CubeSave("", "inventory_transmute", custom.id)
        D4Box_Assert(!D4Box_CubeReadFile(file).Has(custom.id), "custom deletion preserves built-in selection")
    } finally {
        if FileExist(file)
            FileDelete(file)
        if existed
            FileMove(backup, file)
    }
} catch as err {
    D4Box_Assert(false, err.Message " @" err.File ":" err.Line)
}
FileAppend("done`n", D4Box_TestReport, "UTF-8")
ExitApp(D4Box_TestFailures ? 1 : 0)
