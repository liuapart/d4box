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
#Include "..\functions\DetectionRegions.ahk"
#Include "..\gui\DetectionRegionsGUI.ahk"
#Include "..\functions\CubeRunner.ahk"
#Include "..\functions\HealthGlobe.ahk"
#Include "..\functions\ConditionSystem.ahk"
#Include "..\functions\WindowDiagnostics.ahk"
#Include "DetectionRegionsTest.ahk"

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

class D4Box_TestAdapter extends D4Box_CubeAdapter {
    __New() {
        this.frame := {x: 100, y: 200, w: 1000, h: 500}
    }
    itemChecks := 0
    failItem := false
    CheckItem(item) {
        this.itemChecks += 1
        if this.failItem
            throw Error("item changed")
    }
    clicks := 0
    inputs := []
    moves := []
    events := []
    cancelOnInput := false
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
    MoveTo(x, y) {
        this.events.Push("move")
        this.moves.Push({x: x, y: y})
        this.pointer := {x: (x - this.frame.x) / this.frame.w, y: (y - this.frame.y) / this.frame.h}
    }
    SendClick(button) {
        this.events.Push("click")
        if this.cancelOnInput
            this.cancelled := true
        this.clicks += 1
        this.inputs.Push({x: this.pointer.x, y: this.pointer.y, button: button})
    }
    Sleep(ms) {
        this.events.Push("sleep:" ms)
        this.waits += 1
        if this.cancelAfterClick && this.clicks > 0
            this.cancelled := true
        this.Check()
    }
    WaitColor(step) {
        this.events.Push("color")
        this.Check()
        this.checks += 1
        if this.failColor
            throw Error("color timeout")
    }
}

; Exercise the production detection and guarded-click code with fake pixels/time/input.
class D4Box_ConfirmTestAdapter extends D4Box_TestAdapter {
    clock := 0
    pixels := [0xAABBCC]
    samples := []
    results := []
    delays := []
    failAtRead := 0
    failure := "cancelled"
    Check() {
        super.Check()
        if this.failAtRead && this.samples.Length >= this.failAtRead
            throw Error(this.failure)
    }
    Now() {
        return this.clock
    }
    ReadColor(x, y) {
        this.samples.Push({x: x, y: y})
        return this.pixels[Min(this.samples.Length, this.pixels.Length)]
    }
    Sleep(ms) {
        this.delays.Push(ms)
        this.clock += ms
        super.Sleep(ms)
    }
    ReportConfirmation(step, clicked) {
        this.results.Push(clicked)
    }
}

class D4Box_RegionTestAdapter extends D4Box_ConfirmTestAdapter {
    regionResults := [true]
    MatchRegion(node) {
        this.samples.Push(node.id)
        return this.regionResults[Min(this.samples.Length, this.regionResults.Length)]
    }
    WaitColor(step) {
        D4Box_CubeAdapter.Prototype.WaitColor.Call(this, step)
    }
}

try {
    D4Box_TestDetectionModel()
    D4Box_Assert(true, "detector model: evidence, rename, serialization, reference protection, frame mismatch and missing files")
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
    issue := D4Box_CubeRunIssue(custom)
    D4Box_Assert(InStr(issue, L("cube.missingReady")) && InStr(issue, L("cube.missingFinal", L("cube.any")))
        , "draft feedback lists both missing startup requirements")
    guard := D4Box_CubeStep("Ready|check|any|0.1|0.2|0|AABBCC|24|3000")
    custom.ready := D4Box_CubeStepText(guard)
    custom.steps.Push(D4Box_CubeCopy(guard))
    D4Box_CubeValidate(custom, true)
    D4Box_Assert(D4Box_CubeRunIssue(custom) = "", "runnable profile has no draft warning")
    incomplete := D4Box_CubeCopy(custom)
    incomplete.steps.Pop()
    D4Box_Assert(D4Box_CubeRunIssue(incomplete) = L("cube.missingFinal", L("cube.any"))
        , "final-check-only failure does not ask for another ready marker")
    incomplete := D4Box_CubeCopy(custom)
    incomplete.ready := ""
    D4Box_Assert(D4Box_CubeRunIssue(incomplete) = L("cube.missingReady")
        , "ready-only failure does not report a missing final check")
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

    regionStep := D4Box_CubeStep("Region accept|confirm_if_present|any|0.4|0.6|180|-|0|200|0|0|region_fixture")
    regionStep.detector := {id: "region_fixture"}
    adapter := D4Box_RegionTestAdapter()
    D4Box_RunCubePlan([regionStep], adapter)
    D4Box_Assert(adapter.clicks = 1 && adapter.samples.Length = 3 && adapter.inputs[1].x = 0.4
        , "region confirm uses named matcher twice and rechecks before independent accept click")
    adapter := D4Box_RegionTestAdapter()
    adapter.regionResults := [false]
    D4Box_RunCubePlan([regionStep], adapter)
    D4Box_Assert(adapter.clicks = 0 && adapter.clock = 200 && !adapter.results[1], "missing region skips optional accept")
    adapter := D4Box_RegionTestAdapter()
    adapter.regionResults := [true, true, false]
    D4Box_RunCubePlan([regionStep], adapter)
    D4Box_Assert(adapter.clicks = 0, "region disappearing before click does not receive input")
    adapter := D4Box_RegionTestAdapter()
    adapter.failAtRead := 3
    D4Box_ExpectError(() => D4Box_RunCubePlan([regionStep], adapter), "region cancel before click propagates")
    D4Box_Assert(adapter.clicks = 0, "no input after region cancellation")
    regionWait := D4Box_CubeCopy(regionStep)
    regionWait.action := "check"
    adapter := D4Box_RegionTestAdapter()
    D4Box_RunCubePlan([regionWait], adapter)
    D4Box_Assert(adapter.clicks = 0 && adapter.samples.Length = 2, "region wait uses two matches and never clicks")
    adapter := D4Box_RegionTestAdapter()
    adapter.regionResults := [false]
    D4Box_ExpectError(() => D4Box_RunCubePlan([regionWait], adapter), "required region wait timeout still fails")
    unresolved := D4Box_CubeStep(D4Box_CubeStepText(regionStep))
    adapter := D4Box_RegionTestAdapter()
    D4Box_ExpectError(() => D4Box_RunCubePlan([unresolved], adapter), "missing detector binding cannot fall back to pixel mode")

    optional := D4Box_CubeStep("Optional accept|confirm_if_present|any|0.4|0.6|180|AABBCC|0|200|0.1|0.2")
    confirmProfile := D4Box_CubeCopy(custom)
    confirmProfile.steps.InsertAt(2, optional)
    D4Box_CubeValidate(confirmProfile, true)
    legacyOptional := D4Box_CubeStep("Accept|confirm_if_present|any|0.4|0.6|180|AABBCC|0|200")
    D4Box_Assert(legacyOptional.probeX = 0.4 && legacyOptional.probeY = 0.6, "nine-field step defaults probe to click point")
    D4Box_Assert(StrSplit(D4Box_CubeStepText(guard), "|").Length = 9, "ordinary steps keep nine-field format")
    invalid := D4Box_CubeCopy(confirmProfile)
    invalid.steps[2].probeX := 1.1
    D4Box_ExpectError(() => D4Box_CubeValidate(invalid), "invalid confirmation probe rejected")
    invalid.steps[2].probeX := 0.1
    invalid.steps[2].color := "-"
    D4Box_ExpectError(() => D4Box_CubeValidate(invalid), "confirmation requires a sampled color")
    invalid := D4Box_CubeCopy(confirmProfile)
    invalid.steps.Pop()
    D4Box_ExpectError(() => D4Box_CubeValidate(invalid, true), "optional confirmation cannot replace final result check")
    D4Box_ExpectError(() => D4Box_CubeStep("Accept|confirm_if_present|any|0.4|0.6|180|AABBCC|0|200|0.1"), "partial probe fields rejected")

    adapter := D4Box_ConfirmTestAdapter()
    D4Box_RunCubePlan([optional, guard], adapter)
    D4Box_Assert(adapter.clicks = 1 && adapter.inputs[1].button = "left"
        && adapter.inputs[1].x = 0.4 && adapter.inputs[1].y = 0.6 && adapter.checks = 1
        , "detected confirmation clicks accept once and continues")
    D4Box_Assert(adapter.samples.Length = 3 && adapter.samples[3].x = 0.1 && adapter.samples[3].y = 0.2
        && adapter.results[1] && adapter.delays[3] = 180, "independent probe is rechecked before click; post-click delay applies")
    adapter := D4Box_ConfirmTestAdapter()
    adapter.pixels := [0]
    D4Box_RunCubePlan([optional, guard], adapter)
    D4Box_Assert(adapter.clicks = 0 && adapter.clock = 200 && adapter.checks = 1 && !adapter.results[1]
        , "absent confirmation times out without click or post-click delay and continues")
    adapter := D4Box_ConfirmTestAdapter()
    adapter.pixels := [0, 0, 0xAABBCC, 0xAABBCC]
    D4Box_RunCubePlan([optional], adapter)
    D4Box_Assert(adapter.clicks = 1, "delayed confirmation detected within timeout")
    adapter := D4Box_ConfirmTestAdapter()
    adapter.pixels := [0xAABBCC, 0]
    D4Box_RunCubePlan([optional], adapter)
    D4Box_Assert(adapter.clicks = 0, "one transient match does not click")
    adapter := D4Box_ConfirmTestAdapter()
    adapter.pixels := [0xAABBCC, 0xAABBCC, 0]
    D4Box_RunCubePlan([optional], adapter)
    D4Box_Assert(adapter.clicks = 0 && !adapter.results[1], "confirmation disappearing during pointer move is not clicked")
    for failure in ["cancelled", "focus lost", "frame changed", "total timeout"] {
        for sampleCount in [1, 3] {
            adapter := D4Box_ConfirmTestAdapter()
            adapter.failAtRead := sampleCount
            adapter.failure := failure
            D4Box_ExpectError(() => D4Box_RunCubePlan([optional, guard], adapter), failure " aborts optional confirmation at sample " sampleCount)
            D4Box_Assert(adapter.clicks = 0 && adapter.checks = 0, "no input or next step after " failure)
        }
    }
    adapter := D4Box_ConfirmTestAdapter()
    adapter.pixels := [0]
    forced := D4Box_CubeStep("Accept|confirm|any|0.4|0.6|180|-|24|200")
    D4Box_RunCubePlan([forced], adapter)
    D4Box_Assert(adapter.clicks = 1 && adapter.samples.Length = 0, "legacy accept remains unconditional")

    mouseItem := {source: "manual", x: 350, y: 400, quality: "any"}
    rightClick := D4Box_CubeStep("Load item|rightclick|any|0.7|0.8|250|-|24|3000")
    rightProfile := D4Box_CubeCopy(custom)
    rightProfile.steps.InsertAt(1, rightClick)
    D4Box_CubeValidate(rightProfile, true)
    adapter := D4Box_TestAdapter()
    D4Box_RunCubePlan(D4Box_CubeItemPlan(rightProfile, "any"), adapter, mouseItem)
    D4Box_Assert(adapter.inputs.Length = 2 && adapter.inputs[1].button = "right"
        && adapter.inputs[1].x = 0.25 && adapter.inputs[1].y = 0.4
        && adapter.inputs[2].button = "left" && adapter.checks = 1, "single-item selection uses captured pointer, ignores step XY; execute stays left-click")
    D4Box_Assert(adapter.moves.Length = 3 && adapter.moves[3].x = mouseItem.x && adapter.moves[3].y = mouseItem.y
        && adapter.clicks = 2, "successful single-item flow restores original screen position without clicking")
    adapter := D4Box_TestAdapter()
    D4Box_RunCubePlan(custom.steps, adapter, mouseItem)
    D4Box_Assert(adapter.moves.Length = 2 && adapter.moves[2].x = 350 && adapter.moves[2].y = 400
        && adapter.clicks = 1, "single-item flow without selection also restores original pointer")
    D4Box_Assert(adapter.events[3] = "click" && adapter.events[4] = "move"
        && adapter.events[5] = "sleep:450" && adapter.events[6] = "color"
        , "pointer returns immediately after last click, before its delay and final color check")
    adapter := D4Box_TestAdapter()
    D4Box_RunCubePlan([custom.steps[1], guard, forced, guard], adapter, mouseItem)
    D4Box_Assert(adapter.moves.Length = 3 && adapter.events[4] = "sleep:450" && adapter.events[5] = "color"
        && adapter.events[9] = "click" && adapter.events[10] = "move" && adapter.events[11] = "sleep:180"
        , "intermediate color check does not trigger restoration before a later click")
    adapter := D4Box_TestAdapter()
    D4Box_RunCubePlan([guard], adapter, mouseItem)
    D4Box_Assert(adapter.moves.Length = 0 && adapter.checks = 1, "pixel-only plan does not move pointer")
    adapter := D4Box_TestAdapter()
    adapter.cancelOnInput := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(custom.steps, adapter, mouseItem), "interruption before early restore stops the plan")
    D4Box_Assert(adapter.moves.Length = 1, "no early restore if interrupted at final click")
    for present in [true, false] {
        adapter := D4Box_ConfirmTestAdapter()
        adapter.pixels := [present ? 0xAABBCC : 0]
        D4Box_RunCubePlan([optional, guard], adapter, mouseItem)
        lastMove := adapter.moves[adapter.moves.Length]
        D4Box_Assert(lastMove.x = mouseItem.x && lastMove.y = mouseItem.y && adapter.checks = 1
            && adapter.clicks = (present ? 1 : 0), "last optional confirmation restores after click or skip: " present)
        if present
            D4Box_Assert(adapter.events[4] = "click" && adapter.events[5] = "move" && adapter.events[6] = "sleep:180"
                , "optional confirmation restores before post-click delay")
    }
    adapter := D4Box_TestAdapter()
    adapter.failColor := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(custom.steps, adapter, mouseItem), "failed final check stops single-item completion")
    D4Box_Assert(adapter.moves.Length = 2 && adapter.clicks = 1, "final check failure occurs after early restore without another movement")
    adapter := D4Box_TestAdapter()
    adapter.cancelled := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(rightProfile.steps, adapter, mouseItem), "cancel before right-click")
    D4Box_Assert(adapter.clicks = 0, "cancelled right-click sends no input")
    adapter := D4Box_TestAdapter()
    adapter.cancelAfterClick := true
    D4Box_ExpectError(() => D4Box_RunCubePlan(rightProfile.steps, adapter, mouseItem), "cancel after right-click")
    D4Box_Assert(adapter.clicks = 1, "no execute after cancelled right-click")
    D4Box_Assert(adapter.moves.Length = 1, "cancelled single-item flow never restores pointer")
    adapter := D4Box_TestAdapter()
    adapter.failColor := true
    D4Box_ExpectError(() => D4Box_RunCubePlan([guard, rightClick], adapter, mouseItem), "failed target check blocks right-click")
    D4Box_Assert(adapter.clicks = 0, "no right-click after failed target check")

    adapter := D4Box_TestAdapter()
    D4Box_ExpectError(() => D4Box_RunCubePlan([rightClick], adapter), "missing target rejected")
    D4Box_Assert(adapter.clicks = 0, "missing target never falls back to stored XY")
    outside := {source: "manual", x: 99, y: 400, quality: "any"}
    D4Box_ExpectError(() => D4Box_RunCubePlan([rightClick], adapter, outside), "pointer outside game rejected")
    D4Box_Assert(adapter.clicks = 0, "outside pointer sends no input")
    adapter := D4Box_TestAdapter()
    adapter.failItem := true
    D4Box_ExpectError(() => D4Box_RunCubePlan([rightClick], adapter, mouseItem), "changed item blocks selection")
    D4Box_Assert(adapter.clicks = 0, "changed item sends no input")

    inventoryProfile := D4Box_CubeCopy(rightProfile)
    inventoryProfile.source := "inventory"
    inventoryProfile.steps.InsertAt(1, D4Box_CubeCopy(custom.steps[1]))
    inventoryPlan := D4Box_CubeItemPlan(inventoryProfile, "red")
    D4Box_Assert(inventoryPlan.Length = 4 && inventoryPlan[1].action = "execute"
        && inventoryPlan[2].action = "rightclick", "explicit selection preserves order without extra preload")
    adapter := D4Box_TestAdapter()
    for item in [{source: "inventory", x: 700, y: 450, quality: "red"}
        , {source: "inventory", x: 900, y: 500, quality: "red"}]
        D4Box_RunCubePlan(inventoryPlan, adapter, item)
    D4Box_Assert(adapter.moves.Length = 6, "inventory flow adds no pointer restoration")
    D4Box_Assert(adapter.inputs.Length = 6 && adapter.inputs[2].button = "right"
        && adapter.inputs[2].x = 0.6 && adapter.inputs[2].y = 0.5
        && adapter.inputs[5].button = "right" && adapter.inputs[5].x = 0.8
        && adapter.inputs[5].y = 0.6 && adapter.itemChecks = 2, "each inventory iteration selects its own checked target")
    legacyPlan := D4Box_CubeItemPlan(builtin, "red")
    D4Box_Assert(legacyPlan.Length = 5 && legacyPlan[1].action = "rightclick"
        && legacyPlan[1].delay = 250 && legacyPlan[2].action = "select", "existing inventory flow retains one automatic preload")
    inventoryProfile.steps[2].quality := "red"
    D4Box_Assert(D4Box_CubeItemPlan(inventoryProfile, "yellow")[1].action = "rightclick"
        , "preload decision follows the current quality route")
    D4Box_Assert(D4Box_CubeItemPlan(custom, "any").Length = 2
        , "existing preloaded single-item flow gets no new selection")

    isRunning := true
    isPaused := false
    previouslyPaused := false
    isAutoTransmuting := true
    D4Box_InterruptCube()
    D4Box_Assert(!isAutoTransmuting && isPaused && previouslyPaused, "manual interruption also pauses combat")
    isRunning := false
    isPaused := false
    previouslyPaused := false

    workflowFile := D4Box_CubeUserPath()
    backup := workflowFile ".test-backup-" A_TickCount
    existed := FileExist(workflowFile)
    if existed
        FileCopy(workflowFile, backup)
    try {
        D4Box_CubeSave(custom, custom.id)
        loaded := D4Box_CubeReadFile(workflowFile)[custom.id]
        D4Box_Assert(loaded.name = custom.name && loaded.steps.Length = 2, "INI unicode/steps round trip")
        D4Box_Assert(D4Box_CubeSelectedId() = custom.id, "selected workflow persists")
        D4Box_CubeSave(rightProfile, rightProfile.id)
        loaded := D4Box_CubeReadFile(workflowFile)[rightProfile.id]
        D4Box_Assert(loaded.steps.Length = 3 && loaded.steps[1].action = "rightclick"
            && loaded.steps[1].x = 0.7 && loaded.steps[1].y = 0.8, "right-click INI round trip")
        D4Box_CubeSave(confirmProfile, confirmProfile.id)
        loaded := D4Box_CubeReadFile(workflowFile)[confirmProfile.id]
        savedConfirm := loaded.steps[2]
        D4Box_Assert(savedConfirm.action = "confirm_if_present" && savedConfirm.x = 0.4 && savedConfirm.y = 0.6
            && savedConfirm.probeX = 0.1 && savedConfirm.probeY = 0.2 && savedConfirm.color = "AABBCC"
            && savedConfirm.timeout = 200, "optional confirmation INI preserves distinct probe and click fields")
        namedProfile := D4Box_CubeCopy(custom)
        namedProfile.steps.InsertAt(2, unresolved)
        D4Box_CubeSave(namedProfile, namedProfile.id)
        loaded := D4Box_CubeReadFile(workflowFile)[namedProfile.id]
        D4Box_Assert(loaded.steps[2].detectorId = "region_fixture" && loaded.steps[2].x = 0.4
            , "workflow INI round trip preserves detector reference and independent click position")
        D4Box_ExpectError(() => D4Box_CubeValidate(loaded, true), "draft can retain missing detector but cannot run")
        custom.steps.Pop()
        D4Box_CubeSave(custom, custom.id)
        D4Box_Assert(IniRead(workflowFile, "Workflow." custom.id, "Step2", "missing") = "missing", "removed steps do not remain on disk")
        D4Box_CubeSave("", "inventory_transmute", custom.id)
        D4Box_Assert(!D4Box_CubeReadFile(workflowFile).Has(custom.id), "custom deletion preserves built-in selection")
    } finally {
        if FileExist(workflowFile)
            FileDelete(workflowFile)
        if existed
            FileMove(backup, workflowFile)
    }
} catch as err {
    D4Box_Assert(false, err.Message " [" err.Extra "] @" err.File ":" err.Line)
}
FileAppend("done`n", D4Box_TestReport, "UTF-8")
ExitApp(D4Box_TestFailures ? 1 : 0)
