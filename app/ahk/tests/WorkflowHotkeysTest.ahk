; Included by model/native smoke tests; never sends game input.
D4Box_BindingsAssert(condition, label) {
    if !condition
        throw Error(label)
}

D4Box_BindingsExpectError(callback, label) {
    try callback.Call()
    catch {
        return
    }
    throw Error(label)
}

D4Box_TestBindingsModel() {
    global D4Box_WorkflowBindings, D4Box_CubeBusy, D4Box_CubeCapturePending, isAutoTransmuting
    root := A_ScriptDir "\.runtime\binding-test-" A_TickCount
    DirCreate(root)
    file := root "\bindings.ini"
    old := D4Box_WorkflowBindings
    try {
        profiles := Map("first", {}, "second", {})
        rows := D4Box_BindingsRead(file, "first")
        D4Box_BindingsAssert(rows.Length = 1 && rows[1].id = "default" && rows[1].key = "F3" && rows[1].profileId = "first", "bindings migrate selected workflow to default F3")
        rows[1].key := "^F6"
        rows.Push({id: "other", key: "!F7", profileId: "second"})
        D4Box_BindingsValidate(rows, profiles)
        D4Box_BindingsWrite(rows, file)
        rows := D4Box_BindingsRead(file, "second")
        D4Box_BindingsAssert(rows[1].key = "^F6" && rows[1].profileId = "first" && rows[2].profileId = "second", "bindings persist edited default and independent workflow IDs")
        D4Box_BindingsAssert(GetStartStopHotkeyDisplay("^!+F6") = "Ctrl+Alt+Shift+F6", "modifier labels do not insert extra Shift")
        D4Box_BindingsExpectError(() => D4Box_BindingRemove(rows, "default"), "default binding cannot be removed")
        D4Box_BindingsAssert(D4Box_BindingRemove(rows, "other").Length = 1, "custom binding can be removed")
        for key in ["", "Tab", "LButton", "RButton", "Control", "^!F8", "unknown", "F3 Up", "*F3"]
            D4Box_BindingsExpectError(() => D4Box_BindingKey(key), "reserved or invalid binding rejected: " key)
        duplicate := D4Box_CubeCopy(rows)
        duplicate[1].key := "!^F6", duplicate[2].key := "^!F6"
        D4Box_BindingsExpectError(() => D4Box_BindingsValidate(duplicate, profiles), "modifier order cannot bypass duplicate detection")
        duplicate[1].key := "Esc", duplicate[2].key := "Escape"
        D4Box_BindingsExpectError(() => D4Box_BindingsValidate(duplicate, profiles), "key aliases cannot bypass duplicate detection")
        D4Box_BindingsExpectError(() => D4Box_BindingsValidate(rows, profiles, "F6"), "modified workflow key conflicts with wildcard macro key")
        missing := D4Box_CubeCopy(rows), missing[2].profileId := "missing"
        D4Box_BindingsExpectError(() => D4Box_BindingsValidate(missing, profiles), "missing workflow rejected without fallback")
        missing := D4Box_CubeCopy(rows), missing[1].id := "changed"
        D4Box_BindingsExpectError(() => D4Box_BindingsValidate(missing, profiles), "exactly one default is required")
        D4Box_WorkflowBindings := rows
        D4Box_BindingsAssert(D4Box_DefaultBindingProfile() = "first" && D4Box_BindingReferences("second") = "Alt+F7", "default selection and delete references use stable workflow IDs")
        D4Box_BindingsAssert(IsForbiddenStartStopHotkey("F6") && !IsForbiddenStartStopHotkey("F3"), "macro conflict check follows edited bindings instead of reserving F3")
        calls := []
        fakeRun := (id) => calls.Push(id)
        D4Box_DispatchWorkflowBinding(rows[2], fakeRun)
        D4Box_BindingsAssert(calls.Length = 1 && calls[1] = "second", "additional hotkey runs its assigned workflow, not default selection")
        D4Box_CubeBusy := true, isAutoTransmuting := true
        D4Box_DispatchWorkflowBinding(rows[1], fakeRun)
        D4Box_BindingsAssert(!isAutoTransmuting && D4Box_CubeBusy && calls.Length = 1, "any binding cancels busy runner without starting another; busy latch remains")
        D4Box_CubeBusy := false, D4Box_CubeCapturePending := true
        D4Box_DispatchWorkflowBinding(rows[2], fakeRun)
        D4Box_BindingsAssert(calls.Length = 1, "capture blocks workflow dispatch")
    } finally {
        D4Box_WorkflowBindings := old
        D4Box_CubeBusy := false, D4Box_CubeCapturePending := false, isAutoTransmuting := false
        DirDelete(root, true)
    }
}

D4Box_TestBindingsNative() {
    global D4Box_WorkflowBindings, D4Box_RegisteredBindings, D4Box_BindingsUI, D4Box_BindingEditor
    global D4Box_MainTabs, skillControls, myGui, statusBar, startStopHotkey, isRunning
    old := D4Box_CubeCopy(D4Box_WorkflowBindings)
    root := A_ScriptDir "\.runtime\binding-native-" A_TickCount
    DirCreate(root)
    path := root "\bindings.ini"
    try {
        rows := [{id: "default", key: "F6", profileId: "inventory_transmute"}
            , {id: "second", key: "^F7", profileId: "inventory_transmute"}]
        D4Box_BindingsApply(rows, true, path)
        if D4Box_RegisteredBindings.Length != 2 || D4Box_BindingsRead(path)[1].key != "F6"
            throw Error("Binding registration/persistence mismatch")
        DirCreate(root "\blocked")
        next := D4Box_CubeCopy(rows), next[1].key := "F9"
        rejected := false
        try D4Box_BindingsApply(next, true, root "\blocked")
        catch {
            rejected := true
        }
        if !rejected || D4Box_RegisteredBindings[1].key != "F6" || D4Box_WorkflowBindings[1].key != "F6"
            throw Error("Failed save did not restore original hotkeys")
        D4Box_BindingsRefresh()
        if D4Box_BindingsUI.remove.Enabled
            throw Error("Default binding delete button must be disabled")
        D4Box_BindingEdit(false)
        if !IsObject(D4Box_BindingEditor) || D4Box_BindingEditor.key.Value != "F6"
            throw Error("Default binding editor missing or wrong key")
        D4Box_BindingClose()
        D4Box_BindingsUI.list.Modify(1, "-Select")
        D4Box_BindingsUI.list.Modify(2, "Select Focus")
        D4Box_BindingsSelectionChanged()
        if !D4Box_BindingsUI.remove.Enabled
            throw Error("Custom binding delete button must be enabled")
        D4Box_BindingsApply(D4Box_BindingRemove(rows, "second"), true, path)
        if D4Box_RegisteredBindings.Length != 1
            throw Error("Deleted hotkey remains registered")
        isRunning := true, rejected := false
        try D4Box_BindingsApply(rows, false)
        catch {
            rejected := true
        }
        isRunning := false
        if !rejected
            throw Error("Running macro allowed binding edits")
        D4Box_BindingsRefresh()
        ; Inspect actual native windows; no synthetic keyboard/mouse events.
        for page in [1, 2] {
            D4Box_MainTabs.Choose(page)
            WinActivate(myGui.Hwnd)
            WinRedraw(myGui.Hwnd)
            Sleep 350
            WinGetClientPos(&x, &y, &w, &h, myGui.Hwnd)
            capture := CaptureScreenRegion(x, y, w, h)
            D4Box_DetectorWriteBitmap(capture, 0, 0, w, h, A_ScriptDir "\.runtime\main-page-" page ".bmp")
            for ctrl in myGui {
                ctrl.GetPos(&cx, &cy, &cw, &ch)
                if ctrl.Visible && (cy < 0 || cy + ch > 532 || cx < 0 || cx + cw > 720)
                    throw Error("Main control outside client bounds: " ctrl.Type)
            }
        }
        D4Box_MainTabs.Choose(1)
    } finally {
        isRunning := false
        D4Box_BindingClose()
        D4Box_BindingsApply(old, false)
        D4Box_BindingsRefresh()
        DirDelete(root, true)
    }
}

D4Box_TestMainVariants() {
    global myGui, appLang, D4Box_MainTabs, presetNames, strategyNames
    file := A_ScriptDir "\settings.ini"
    saved := FileRead(file, "RAW")
    oldLang := appLang
    try {
        for variant in [{lang: "en", theme: "light"}, {lang: "zh", theme: "dark"}, {lang: "en", theme: "dark"}] {
            myGui.Destroy()
            appLang := variant.lang
            strategyNames := [L("strategy.off"), L("strategy.click"), L("strategy.buff"), L("strategy.hold")]
            IniWrite(variant.theme, file, "UI", "Theme")
            Loop 4
                presetNames[A_Index] := L("preset.default", A_Index)
            CreateMainGUI()
            MUI_ApplyNativeTheme(myGui)
            D4Box_BindingsRefresh()
            myGui.Show("w720 h532")
            for page in [1, 2] {
                D4Box_MainTabs.Choose(page)
                WinRedraw(myGui.Hwnd)
                Sleep 300
                WinGetClientPos(&x, &y, &w, &h, myGui.Hwnd)
                capture := CaptureScreenRegion(x, y, w, h)
                D4Box_DetectorWriteBitmap(capture, 0, 0, w, h, A_ScriptDir "\.runtime\main-" variant.lang "-" variant.theme "-" page ".bmp")
                for ctrl in myGui {
                    if RegExMatch(ctrl.Text, "^(binding|detect)\.[a-zA-Z]+$")
                        throw Error("Untranslated main control: " ctrl.Text)
                }
            }
        }
    } finally {
        appLang := oldLang
        out := FileOpen(file, "w")
        out.RawWrite(saved)
        out.Close()
    }
}

D4Box_TestRuntimeFooter() {
    global D4Box_MainTabs, startStopHotkeyCtrl, startStopMouseCtrl, startStopButton, D4Box_MacroSave
    global D4Box_RuntimeStop, D4Box_RuntimeDetail, D4Box_RuntimeAction, statusText, statusBar
    global isRunning, isPaused, previouslyPaused, isAutoTransmuting, D4Box_CubeBusy
    global D4Box_WorkflowName, D4Box_WorkflowStep, myGui
    ; Keep window polling from interpreting fixture state as a real running macro.
    SetTimer(CheckWindow, 0)
    try {
        for page in [1, 2] {
            D4Box_MainTabs.Choose(page)
            for ctrl in [startStopHotkeyCtrl, startStopMouseCtrl, startStopButton.ctrl, D4Box_MacroSave]
                if !!DllCall("IsWindowVisible", "Ptr", ctrl.Hwnd) != (page = 2)
                    throw Error("Macro controls are not confined to macro tab")
        }
        D4Box_MainTabs.Choose(1)
        isRunning := false, isPaused := false, D4Box_CubeBusy := false
        D4Box_RefreshRuntimeStatus()
        if D4Box_RuntimeStop.Visible || statusText.Text != L("main.ready")
            throw Error("Idle footer exposes a start/stop button")
        ; A queued stop event after completion cannot toggle a stopped macro on.
        D4Box_RuntimeAction := "macro"
        D4Box_StopFromFooter()
        if isRunning
            throw Error("Stale stop action started a macro")
        isRunning := true
        D4Box_RefreshRuntimeStatus()
        if !D4Box_RuntimeStop.Visible || D4Box_RuntimeStop.Text != L("runtime.stopMacro")
            || !InStr(statusText.Text, L("runtime.macroRunning", ""))
            throw Error("Macro status/stop unavailable on workflow tab")
        statusBar.Text := "Saved fixture"
        if !InStr(statusText.Text, L("runtime.macroRunning", ""))
            throw Error("Save notice replaced live macro status")
        isPaused := true
        D4Box_RefreshRuntimeStatus()
        if !InStr(statusText.Text, L("runtime.macroPaused", ""))
            throw Error("Paused macro status missing")
        isPaused := false, D4Box_CubeBusy := true, isAutoTransmuting := true
        D4Box_WorkflowName := "Fixture workflow", D4Box_WorkflowStep := "Verify result"
        D4Box_RefreshRuntimeStatus()
        if D4Box_RuntimeStop.Text != L("runtime.cancelWorkflow") || !InStr(statusText.Text, "Fixture workflow")
            || !InStr(D4Box_RuntimeDetail.Text, "Verify result") || !InStr(D4Box_RuntimeDetail.Text, "`n")
            throw Error("Workflow name, step, or macro suspension missing")
        WinRedraw(myGui.Hwnd)
        Sleep 250
        WinGetClientPos(&x, &y, &w, &h, myGui.Hwnd)
        capture := CaptureScreenRegion(x, y, w, h)
        D4Box_DetectorWriteBitmap(capture, 0, 0, w, h, A_ScriptDir "\.runtime\main-running-fixture.bmp")
        D4Box_StopFromFooter()
        if isAutoTransmuting || !D4Box_CubeBusy || !isPaused || !previouslyPaused || D4Box_RuntimeStop.Enabled
            throw Error("Footer cancellation must latch busy and leave combat paused")
        ; Simulate the executor's finally block after cancellation/failure.
        D4Box_CubeBusy := false, D4Box_WorkflowName := "", D4Box_WorkflowStep := ""
        D4Box_RefreshRuntimeStatus()
        if D4Box_RuntimeStop.Text != L("runtime.stopMacro") || !InStr(statusText.Text, L("runtime.macroPaused", ""))
            throw Error("Footer stayed in workflow state after completion")
        isRunning := false
        D4Box_RefreshRuntimeStatus()
        if D4Box_RuntimeStop.Visible
            throw Error("Footer stop did not hide after all work stopped")
    } finally {
        isRunning := false, isPaused := false, previouslyPaused := false
        D4Box_CubeBusy := false, isAutoTransmuting := false
        D4Box_WorkflowName := "", D4Box_WorkflowStep := ""
        D4Box_MainTabs.Choose(1)
        UpdateStartStopButton()
        SetTimer(CheckWindow, 100)
    }
}
