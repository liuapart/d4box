; Shared step executor. The adapter isolates timing, input, and visual checks for tests.
D4Box_RunCubePlan(plan, adapter) {
    for step in plan {
        adapter.Check()
        adapter.Report(step)
        switch step.action {
            case "wait":
                adapter.Sleep(step.delay)
            case "check":
                adapter.WaitColor(step)
                adapter.Sleep(step.delay)
            default:
                adapter.Click(step.x, step.y, "left")
                adapter.Sleep(step.delay)
        }
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
    return {x: x, y: y, w: w, h: h}
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

    Click(x, y, button) {
        point := this.Point(x, y)
        this.Check()
        MouseMove point.x, point.y, 0
        this.Sleep(80)
        this.Check()
        Click button
    }

    WaitColor(step) {
        expected := Integer("0x" step.color)
        deadline := A_TickCount + step.timeout
        stable := 0
        loop {
            this.Check()
            point := this.Point(step.x, step.y)
            actual := PixelGetColor(point.x, point.y)
            stable := D4Box_CubeColorMatches(actual, expected, step.tolerance) ? stable + 1 : 0
            if stable >= 2
                return
            if A_TickCount >= deadline
                throw Error(L("cube.checkFailed", step.label))
            this.Sleep(50)
        }
    }

    Report(step) {
        DebugLog("魔盒步骤: " step.label " [" step.action "]")
    }
}

D4Box_CancelCube(*) {
    global isAutoTransmuting
    isAutoTransmuting := false
}

; Manual UI changes/focus loss also leave combat paused, preventing a restart on the wrong screen.
D4Box_InterruptCube() {
    global isRunning, isPaused, previouslyPaused
    D4Box_CancelCube()
    if isRunning {
        isPaused := true
        previouslyPaused := true
    }
}

; F3 starts the selected workflow, or cancels the current one. Busy stays latched until finally.
AutoTransmute(*) {
    global D4Box_CubeCapturePending
    global D4Box_CubeBusy, isAutoTransmuting, isRunning, isPaused, previouslyPaused, utilityControls
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
    shouldResume := isRunning && !isPaused
    oldMouseMode := A_CoordModeMouse
    oldPixelMode := A_CoordModePixel
    completed := 0
    total := 0
    failed := false
    try {
        StopAllTimers()
        SetTimer(ResumeAfterClickPause, 0)
        ReleaseAllKeys()
        CoordMode "Mouse", "Screen"
        CoordMode "Pixel", "Screen"
        profiles := D4Box_CubeProfiles()
        selected := D4Box_CubeSelectedId()
        if !profiles.Has(selected)
            throw Error(L("cube.missingProfile"))
        profile := D4Box_CubeCopy(profiles[selected])
        if profile.legacyYellow && utilityControls.HasProp("upgradeYellow") && utilityControls.upgradeYellow.enable.Value
            profile.quality := "redyellow"
        D4Box_CubeValidate(profile, true)
        frame := D4Box_CubeFrame(hwnd, profile.space)
        adapter := D4Box_CubeAdapter(hwnd, frame, profile.timeout)
        adapter.Check()
        if profile.ready != ""
            adapter.WaitColor(D4Box_CubeStep(profile.ready))
        targets := profile.source = "inventory" ? FindInventoryItems(profile.quality, frame) : [{quality: "any"}]
        adapter.Check()
        total := Min(targets.Length, profile.source = "inventory" ? profile.maxItems : 1)
        if !total {
            UpdateStatus("noTarget", L("cube.noTargets"))
            return
        }
        Loop total {
            item := targets[A_Index]
            adapter.Check()
            if profile.ready != ""
                adapter.WaitColor(D4Box_CubeStep(profile.ready))
            if profile.source = "inventory" {
                ; Recheck the slot before using a coordinate from the initial inventory snapshot.
                stepX := frame.w * 0.0286563
                stepY := frame.h * 0.075
                capture := CaptureInventoryGridBitmap(frame.x + frame.w * 0.674484
                    , frame.y + frame.h * 0.706944, stepX, stepY)
                if !capture.ok || ClassifyInventorySlotFromCapture(capture, item.x, item.y, stepX, stepY) != item.quality
                    throw Error(L("cube.itemChanged", item.row, item.col))
                adapter.Click((item.x - frame.x) / frame.w, (item.y - frame.y) / frame.h, "right")
                adapter.Sleep(250)
            }
            UpdateStatus("transmuting", L("cube.progress", D4Box_CubeProfileName(profile), A_Index, total))
            D4Box_RunCubePlan(D4Box_CubePlan(profile, item.quality), adapter)
            completed += 1
        }
        UpdateStatus("transmuteDone", L("cube.done", completed))
    } catch as err {
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
    }
}
