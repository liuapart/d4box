; Shared step executor. The adapter isolates timing, input, and visual checks for tests.
D4Box_RunCubePlan(plan, adapter, target := false) {
    lastMouseStep := 0
    if IsObject(target) && target.HasProp("source") && target.source = "manual" {
        for index, step in plan
            if step.action != "wait" && step.action != "check"
                lastMouseStep := index
    }
    for index, step in plan {
        adapter.Check()
        adapter.Report(step)
        waitAfter := true
        switch step.action {
            case "wait":
                ; Delay is applied below, after any pointer restoration.
            case "check":
                adapter.WaitColor(step)
            case "confirm_if_present":
                waitAfter := adapter.ClickIfPresent(step)
            case "rightclick":
                adapter.SelectItem(target)
            default:
                adapter.Click(step.x, step.y, "left")
        }
        ; The remaining steps only wait/read pixels. Return before this step's delay,
        ; but keep all subsequent checks and cancellation handling active.
        if index = lastMouseStep {
            adapter.Check()
            adapter.MoveTo(target.x, target.y)
        }
        if waitAfter
            adapter.Sleep(step.delay)
    }
    adapter.Check()
}

D4Box_CubeColorMatches(actual, expected, tolerance) {
    for shift in [16, 8, 0] {
        if Abs(((actual >> shift) & 255) - ((expected >> shift) & 255)) > tolerance
            return false
    }
    return true
}

D4Box_CubeFrame(hwnd, space) {
    WinGetClientPos(&x, &y, &w, &h, "ahk_id " hwnd)
    if w <= 0 || h <= 0
        throw Error(L("cube.noWindow"))
    if space = "screen" && (x != 0 || y != 0 || w != A_ScreenWidth || h != A_ScreenHeight)
        throw Error(L("cube.legacyFrame"))
    return {x: x, y: y, w: w, h: h, dpi: DllCall("GetDpiForWindow", "Ptr", hwnd, "UInt")}
}

class D4Box_CubeAdapter {
    __New(hwnd, frame, timeout) {
        this.hwnd := hwnd
        this.frame := frame
        this.started := A_TickCount
        this.timeout := timeout
    }

    Check() {
        global isAutoTransmuting
        if !isAutoTransmuting
            throw Error(L("cube.cancelled"))
        if !WinActive("ahk_id " this.hwnd)
            throw Error(L("cube.focusLost"))
        WinGetClientPos(&x, &y, &w, &h, "ahk_id " this.hwnd)
        f := this.frame
        if x != f.x || y != f.y || w != f.w || h != f.h
            throw Error(L("cube.frameChanged"))
        if A_TickCount - this.started > this.timeout
            throw Error(L("cube.timedOut"))
    }

    Sleep(duration) {
        deadline := A_TickCount + duration
        loop {
            this.Check()
            remaining := deadline - A_TickCount
            if remaining <= 0
                return
            Sleep Min(50, remaining)
        }
    }

    Point(x, y) {
        f := this.frame
        return {x: f.x + Min(f.w - 1, Round(f.w * x)), y: f.y + Min(f.h - 1, Round(f.h * y))}
    }

    MoveTo(x, y) {
        MouseMove x, y, 0
    }

    SendClick(button) {
        Click button
    }

    ReadColor(x, y) {
        point := this.Point(x, y)
        return PixelGetColor(point.x, point.y)
    }

    Now() {
        return A_TickCount
    }

    Matches(step, probe := false) {
        if D4Box_DetectorId(step) != "" {
            if !step.HasProp("detector")
                throw Error(L("detect.missing", step.detectorId))
            return this.MatchRegion(step.detector)
        }
        return D4Box_CubeColorMatches(this.ReadColor(probe ? step.probeX : step.x, probe ? step.probeY : step.y)
            , Integer("0x" step.color), step.tolerance)
    }

    MatchRegion(node) {
        return D4Box_DetectorMatch(node, this.frame)
    }

    Click(x, y, button, guard := false) {
        point := this.Point(x, y)
        this.Check()
        this.MoveTo(point.x, point.y)
        this.Sleep(80)
        this.Check()
        ; Optional confirmation must still be visible after moving the pointer.
        if IsObject(guard) && !this.Matches(guard, true)
            return false
        this.Check()
        this.SendClick(button)
        return true
    }

    ClickIfPresent(step) {
        deadline := this.Now() + step.timeout
        stable := 0
        loop {
            this.Check()
            stable := this.Matches(step, true) ? stable + 1 : 0
            if stable >= 2 {
                if this.Click(step.x, step.y, "left", step) {
                    this.ReportConfirmation(step, true)
                    return true
                }
                stable := 0
            }
            this.Check()
            remaining := deadline - this.Now()
            if remaining <= 0 {
                this.ReportConfirmation(step, false)
                return false
            }
            this.Sleep(Min(50, remaining))
        }
    }

    ReportConfirmation(step, clicked) {
        DebugLog(step.label ": " L(clicked ? "cube.confirmClicked" : "cube.confirmSkipped"))
    }

    SelectItem(target) {
        this.Check()
        if !IsObject(target) || !target.HasProp("x") || !target.HasProp("y")
            throw Error(L("cube.noItemTarget"))
        f := this.frame
        if target.x < f.x || target.y < f.y || target.x >= f.x + f.w || target.y >= f.y + f.h
            throw Error(L("cube.invalidPoint"))
        this.CheckItem(target)
        this.Click((target.x - f.x) / f.w, (target.y - f.y) / f.h, "right")
    }

    CheckItem(item) {
        if item.source != "inventory"
            return
        f := this.frame
        stepX := f.w * 0.0286563
        stepY := f.h * 0.075
        capture := CaptureInventoryGridBitmap(f.x + f.w * 0.674484
            , f.y + f.h * 0.706944, stepX, stepY)
        if !capture.ok || ClassifyInventorySlotFromCapture(capture, item.x, item.y, stepX, stepY) != item.quality
            throw Error(L("cube.itemChanged", item.row, item.col))
        this.Check()
    }

    WaitColor(step) {
        deadline := this.Now() + step.timeout
        stable := 0
        loop {
            this.Check()
            stable := this.Matches(step) ? stable + 1 : 0
            if stable >= 2
                return
            if this.Now() >= deadline
                throw Error(L("cube.checkFailed", step.label))
            this.Sleep(50)
        }
    }

    Report(step) {
        global D4Box_WorkflowStep
        D4Box_WorkflowStep := step.label
        D4Box_RefreshRuntimeStatus()
        DebugLog("魔盒步骤: " step.label " [" step.action "]")
    }
}

D4Box_CancelCube(*) {
    global isAutoTransmuting
    isAutoTransmuting := false
    D4Box_RefreshRuntimeStatus()
}

; Manual UI changes/focus loss also leave combat paused, preventing a restart on the wrong screen.
D4Box_InterruptCube() {
    global isRunning, isPaused, previouslyPaused
    D4Box_CancelCube()
    if isRunning {
        isPaused := true
        previouslyPaused := true
    }
    D4Box_RefreshRuntimeStatus()
}

; A bound key starts its workflow, or cancels the current one. Busy stays latched until finally.
AutoTransmute(profileId := "", *) {
    global D4Box_CubeCapturePending
    global D4Box_CubeBusy, isAutoTransmuting, isRunning, isPaused, previouslyPaused
    global D4Box_WorkflowName, D4Box_WorkflowStep
    if D4Box_CubeBusy {
        D4Box_CancelCube()
        return
    }
    if D4Box_CubeCapturePending
        return
    hwnd := WinActive("ahk_class Diablo IV Main Window Class")
    if !hwnd
        return

    D4Box_CubeBusy := true
    isAutoTransmuting := true
    D4Box_WorkflowName := "", D4Box_WorkflowStep := ""
    D4Box_RefreshRuntimeStatus()
    shouldResume := isRunning && !isPaused
    oldMouseMode := A_CoordModeMouse
    oldPixelMode := A_CoordModePixel
    completed := 0
    total := 0
    failed := false
    validated := false
    startupError := ""
    try {
        StopAllTimers()
        ReleaseAllKeys()
        CoordMode "Mouse", "Screen"
        CoordMode "Pixel", "Screen"
        ; Capture once before any workflow step can move the pointer.
        MouseGetPos(&itemMouseX, &itemMouseY)
        profiles := D4Box_CubeProfiles()
        selected := profileId != "" ? profileId : D4Box_DefaultBindingProfile()
        if !profiles.Has(selected)
            throw Error(L("cube.missingProfile"))
        profile := D4Box_CubeCopy(profiles[selected])
        D4Box_WorkflowName := D4Box_CubeProfileName(profile)
        D4Box_RefreshRuntimeStatus()
        D4Box_CubeValidate(profile, true)
        validated := true
        frame := D4Box_CubeFrame(hwnd, profile.space)
        readyGuard := D4Box_DetectorBind(profile, frame)
        adapter := D4Box_CubeAdapter(hwnd, frame, profile.timeout)
        adapter.Check()
        if IsObject(readyGuard)
            adapter.WaitColor(readyGuard)
        targets := profile.source = "inventory" ? FindInventoryItems(profile.quality, frame) : [{quality: "any", x: itemMouseX, y: itemMouseY}]
        adapter.Check()
        total := Min(targets.Length, profile.source = "inventory" ? profile.maxItems : 1)
        if !total {
            UpdateStatus("noTarget", L("cube.noTargets"))
            return
        }
        Loop total {
            item := targets[A_Index]
            adapter.Check()
            if IsObject(readyGuard)
                adapter.WaitColor(readyGuard)
            item.source := profile.source
            UpdateStatus("transmuting", L("cube.progress", D4Box_CubeProfileName(profile), A_Index, total))
            D4Box_RunCubePlan(D4Box_CubeItemPlan(profile, item.quality), adapter, item)
            completed += 1
        }
        UpdateStatus("transmuteDone", L("cube.done", completed))
    } catch as err {
        if !validated
            startupError := err.Message
        failed := isAutoTransmuting
        if failed
            UpdateStatus("transmuteFailed", L("bar.transmuteFailed", err.Message))
        else
            UpdateStatus("cancelled", L("bar.transmuteCancelled", completed, total))
        DebugLog("魔盒停止: " err.Message)
    } finally {
        isAutoTransmuting := false
        CoordMode "Mouse", oldMouseMode
        CoordMode "Pixel", oldPixelMode
        ; Do not resume combat after an error, a focus change, or a deliberate macro stop.
        if failed && isRunning {
            isPaused := true
            previouslyPaused := true
        }
        D4Box_CubeBusy := false
        if shouldResume && isRunning && !isPaused && WinActive("ahk_id " hwnd)
            StartAllTimers()
        D4Box_WorkflowName := "", D4Box_WorkflowStep := ""
        D4Box_RefreshRuntimeStatus()
    }
    if startupError != ""
        MsgBox(startupError, L("cube.cannotRun"))
}
