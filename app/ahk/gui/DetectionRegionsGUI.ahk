; Library UI: capture/select on a frozen screenshot; tests never send game input.
D4Box_OpenDetectorLibrary(*) {
    global D4Box_DetectorUI, D4Box_CubeUI, D4Box_CubeBusy, D4Box_CubeCapturePending
    if D4Box_CubeBusy || D4Box_CubeCapturePending
        return
    if IsObject(D4Box_DetectorUI) {
        D4Box_DetectorUI.gui.Show()
        return
    }
    try {
        editor := Gui(IsObject(D4Box_CubeUI) ? "+Owner" D4Box_CubeUI.gui.Hwnd : "", L("detect.title"))
        editor.SetFont("s9", "Segoe UI")
        ui := {gui: editor, nodes: D4Box_DetectorRead(), ids: [], node: D4Box_DetectorNew()
            , loading: true, dirty: false, timer: 0, selection: false, hidden: []}
        D4Box_DetectorUI := ui
        ui.selector := editor.AddDropDownList("x16 y16 w440")
        editor.AddButton("x470 y14 w100", L("detect.new")).OnEvent("Click", D4Box_DetectorNewUI)
        editor.AddButton("x580 y14 w100", L("detect.delete")).OnEvent("Click", D4Box_DetectorDeleteUI)
        editor.AddText("x16 y59 w90", L("cube.name"))
        ui.name := editor.AddEdit("x110 y55 w570")
        editor.AddText("x16 y99 w100", L("detect.variation"))
        ui.variation := editor.AddEdit("x120 y95 w65 Number")
        editor.AddText("x215 y99 w150", L("detect.margin"))
        ui.margin := editor.AddEdit("x370 y95 w65 Number")
        ui.status := editor.AddText("x16 y135 w664 h46")
        ui.preview := editor.AddPicture("x16 y188 w512 h150 +Border")
        editor.AddButton("x16 y354 w210", L("detect.capture")).OnEvent("Click", D4Box_DetectorStart.Bind("capture"))
        editor.AddButton("x238 y354 w210", L("detect.testPresent")).OnEvent("Click", D4Box_DetectorStart.Bind("present"))
        editor.AddButton("x460 y354 w220", L("detect.testAbsent")).OnEvent("Click", D4Box_DetectorStart.Bind("absent"))
        ui.note := editor.AddText("x16 y397 w664 h85", L("detect.help"))
        editor.AddButton("x16 y495 w210", L("detect.save")).OnEvent("Click", D4Box_DetectorSaveUI)
        editor.AddButton("x470 y495 w210", L("cube.close")).OnEvent("Click", D4Box_DetectorClose)
        for control in [ui.name, ui.variation, ui.margin]
            control.OnEvent("Change", D4Box_DetectorDirty)
        ui.selector.OnEvent("Change", D4Box_DetectorSelectUI)
        editor.OnEvent("Close", D4Box_DetectorClose)
        editor.OnEvent("Escape", D4Box_DetectorClose)
        D4Box_DetectorRefreshUI()
        editor.Show("w700 h540")
    } catch as err {
        if IsObject(D4Box_DetectorUI)
            D4Box_DetectorUI.gui.Destroy()
        D4Box_DetectorUI := ""
        MsgBox(err.Message, "d4box")
    }
}

D4Box_DetectorDirty(ctrl, *) {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    if ui.loading
        return
    ui.dirty := true
    if ctrl != ui.name {
        D4Box_DetectorInvalidate(ui.node)
        ui.status.Text := L("detect.retest")
    }
}

D4Box_DetectorReadUI() {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    node := D4Box_CubeCopy(ui.node)
    node.name := Trim(ui.name.Value)
    node.variation := Integer(ui.variation.Value)
    node.margin := Integer(ui.margin.Value)
    if D4Box_DetectorSignature(node) != D4Box_DetectorSignature(ui.node)
        D4Box_DetectorInvalidate(node)
    D4Box_DetectorValidate(node)
    ui.node := node
    return node
}

D4Box_DetectorRefreshUI() {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    ui.loading := true
    ui.ids := []
    labels := []
    for id, node in ui.nodes {
        ui.ids.Push(id)
        labels.Push(node.name " — " L(D4Box_DetectorPassed(node) ? "detect.passed" : "detect.draft"))
    }
    ui.selector.Delete()
    ui.selector.Add(labels)
    for index, id in ui.ids
        if id = ui.node.id
            ui.selector.Choose(index)
    ui.name.Value := ui.node.name
    ui.variation.Value := ui.node.variation
    ui.margin.Value := ui.node.margin
    ui.status.Text := L(D4Box_DetectorPassed(ui.node) ? "detect.passed" : "detect.draft")
    signature := D4Box_DetectorSignature(ui.node)
    ui.status.Text .= " | " L("detect.present") ": " L(ui.node.present = signature ? "detect.ok" : "detect.pending")
        . " | " L("detect.absent") ": " L(ui.node.absent = signature ? "detect.ok" : "detect.pending")
    if ui.node.image != ""
        ui.status.Text .= " | " ui.node.w "×" ui.node.h " | " ui.node.frameW "×" ui.node.frameH " | DPI " ui.node.dpi
    ui.preview.Value := ""
    path := D4Box_DetectorRoot() "\" ui.node.image
    if ui.node.image != "" && FileExist(path) {
        scale := Min(1, 512 / ui.node.w, 150 / ui.node.h)
        ui.preview.Value := "*w" Round(ui.node.w * scale) " *h" Round(ui.node.h * scale) " " path
    }
    ui.loading := false
}

D4Box_DetectorDiscard() {
    global D4Box_DetectorUI
    return !D4Box_DetectorUI.dirty || MsgBox(L("cube.discard"), "d4box", "YesNo") = "Yes"
}

D4Box_DetectorSelectUI(*) {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    if ui.loading || !ui.selector.Value
        return
    id := ui.ids[ui.selector.Value]
    if !D4Box_DetectorDiscard() {
        D4Box_DetectorRefreshUI()
        return
    }
    ui.node := D4Box_CubeCopy(ui.nodes[id])
    ui.dirty := false
    ui.note.Text := L("detect.help")
    D4Box_DetectorRefreshUI()
}

D4Box_DetectorNewUI(*) {
    global D4Box_DetectorUI
    if !D4Box_DetectorDiscard()
        return
    D4Box_DetectorUI.node := D4Box_DetectorNew()
    D4Box_DetectorUI.dirty := false
    D4Box_DetectorRefreshUI()
}

D4Box_DetectorSaveUI(*) {
    global D4Box_DetectorUI, D4Box_CubeBusy, D4Box_CubeCapturePending
    if D4Box_CubeBusy || D4Box_CubeCapturePending
        return
    try {
        ui := D4Box_DetectorUI
        node := D4Box_DetectorReadUI()
        D4Box_DetectorSave(node)
        ui.nodes := D4Box_DetectorRead()
        ui.dirty := false
        D4Box_DetectorRefreshUI()
        D4Box_CubeRefreshDetectorChoices()
        ui.note.Text := L("detect.saved")
    } catch as err
        MsgBox(err.Message, "d4box")
}

D4Box_DetectorDeleteUI(*) {
    global D4Box_DetectorUI, D4Box_CubeUI, D4Box_CubeBusy, D4Box_CubeCapturePending
    if D4Box_CubeBusy || D4Box_CubeCapturePending
        return
    try {
        ui := D4Box_DetectorUI
        profiles := D4Box_CubeProfiles()
        if IsObject(D4Box_CubeUI) {
            ; Include both saved references and unsaved editor drafts.
            for id, profile in D4Box_CubeUI.profiles
                profiles["draft_" id] := profile
            profiles["active_draft"] := D4Box_CubeReadEditor()
        }
        if D4Box_DetectorReferences(ui.node.id, profiles).Length
            throw Error(L("detect.inUse"))
        if MsgBox(L("detect.deleteQuestion"), "d4box", "YesNo") != "Yes"
            return
        D4Box_DetectorDelete(ui.node.id, profiles)
        ui.nodes := D4Box_DetectorRead()
        ui.node := D4Box_DetectorNew()
        ui.dirty := false
        D4Box_DetectorRefreshUI()
        D4Box_CubeRefreshDetectorChoices()
    } catch as err
        MsgBox(err.Message, "d4box")
}

D4Box_DetectorStart(kind, *) {
    global D4Box_DetectorUI, D4Box_CubeUI, D4Box_CubeBusy, D4Box_CubeCapturePending, myGui, isRunning, isPaused
    if D4Box_CubeBusy || D4Box_CubeCapturePending
        return
    try {
        if isRunning && !isPaused
            throw Error(L("detect.pauseMacro"))
        ui := D4Box_DetectorUI
        node := D4Box_DetectorReadUI()
        if kind != "capture" && node.image = ""
            throw Error(L("detect.captureFirst"))
        ; Re-tests revoke old evidence immediately, even if interrupted.
        if kind != "capture" {
            node.%(kind = "present" ? "present" : "absent")% := ""
            ui.dirty := true
        }
        ui.hidden := []
        for window in [ui.gui, IsObject(D4Box_CubeUI) ? D4Box_CubeUI.gui : false, myGui] {
            if IsObject(window) && (WinGetStyle("ahk_id " window.Hwnd) & 0x10000000) {
                ui.hidden.Push(window)
                window.Hide()
            }
        }
        D4Box_CubeCapturePending := true
        ui.timer := D4Box_DetectorJob.Bind(ui, kind)
        ToolTip(L(kind = "capture" ? "detect.captureHint" : "detect.testHint"))
        SetTimer(ui.timer, -3000)
    } catch as err
        MsgBox(err.Message, "d4box")
}

D4Box_DetectorGameFrame(hwnd) {
    if !WinActive("ahk_id " hwnd)
        throw Error(L("cube.focusLost"))
    frame := D4Box_CubeFrame(hwnd, "client")
    return frame
}

D4Box_DetectorJob(ui, kind) {
    global D4Box_DetectorUI
    selecting := false
    try {
        ToolTip()
        Sleep 150
        if !IsObject(D4Box_DetectorUI) || D4Box_DetectorUI != ui
            return
        hwnd := WinActive("ahk_class Diablo IV Main Window Class")
        if !hwnd
            throw Error(L("cube.noWindow"))
        frame := D4Box_DetectorGameFrame(hwnd)
        if kind = "capture" {
            cap := CaptureScreenRegion(frame.x, frame.y, frame.w, frame.h)
            if cap.ok != frame.h
                throw Error(L("detect.writeFailed"))
            after := D4Box_DetectorGameFrame(hwnd)
            if after.x != frame.x || after.y != frame.y || after.w != frame.w || after.h != frame.h
                throw Error(L("cube.frameChanged"))
            D4Box_DetectorSelectRegion(ui, cap, frame)
            selecting := true
        } else {
            samples := []
            Loop 3 {
                current := D4Box_DetectorGameFrame(hwnd)
                if current.x != frame.x || current.y != frame.y || current.w != frame.w || current.h != frame.h
                    throw Error(L("cube.frameChanged"))
                samples.Push(D4Box_DetectorMatch(ui.node, current))
                Sleep 100
            }
            current := D4Box_DetectorGameFrame(hwnd)
            if current.x != frame.x || current.y != frame.y || current.w != frame.w || current.h != frame.h
                throw Error(L("cube.frameChanged"))
            passed := D4Box_DetectorRecordTest(ui.node, kind = "present", samples)
            ui.dirty := true
            ui.note.Text := L(passed ? "detect.testPassed" : "detect.testFailed", L("detect." kind))
        }
    } catch as err {
        ui.note.Text := err.Message
        if IsObject(ui.selection)
            D4Box_DetectorEndSelection(ui)
    } finally {
        if !selecting
            D4Box_DetectorFinishJob(ui)
    }
}

D4Box_DetectorSelectRegion(ui, cap, frame) {
    DirCreate(D4Box_DetectorRoot())
    ui.snapshot := D4Box_DetectorRoot() "\selection.bmp"
    D4Box_DetectorWriteBitmap(cap, 0, 0, cap.width, cap.height, ui.snapshot)
    selection := Gui("-Caption +ToolWindow +AlwaysOnTop -DPIScale", L("detect.capture"))
    ui.selection := selection
    ui.capture := cap
    ui.frame := frame
    ui.startPoint := false
    selection.AddPicture("x0 y0 w" frame.w " h" frame.h, ui.snapshot)
    ui.borders := []
    Loop 4
        ui.borders.Push(selection.AddText("x0 y0 w1 h1 Background00FFFF"))
    selection.OnEvent("Escape", D4Box_DetectorCancelSelection)
    selection.OnEvent("Close", D4Box_DetectorCancelSelection)
    OnMessage(0x201, D4Box_DetectorDrag)
    OnMessage(0x200, D4Box_DetectorDrag)
    OnMessage(0x202, D4Box_DetectorDrag)
    selection.Show("x" frame.x " y" frame.y " w" frame.w " h" frame.h)
    ToolTip(L("detect.dragHint"), frame.x + 16, frame.y + 16)
}

D4Box_DetectorDrag(wParam, lParam, msg, hwnd) {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    if !IsObject(ui) || !IsObject(ui.selection) || GuiFromHwnd(hwnd, true) != ui.selection
        return
    ; Use this event's coordinates, not a newer global mouse position.
    point := Buffer(8, 0)
    px := lParam & 0xFFFF, py := (lParam >> 16) & 0xFFFF
    NumPut("Int", px > 32767 ? px - 65536 : px, point, 0)
    NumPut("Int", py > 32767 ? py - 65536 : py, point, 4)
    if !DllCall("ClientToScreen", "Ptr", hwnd, "Ptr", point)
        return 0
    x := Max(0, Min(ui.frame.w - 1, NumGet(point, 0, "Int") - ui.frame.x))
    y := Max(0, Min(ui.frame.h - 1, NumGet(point, 4, "Int") - ui.frame.y))
    if msg = 0x201 {
        ui.startPoint := {x: x, y: y}
        DllCall("SetCapture", "Ptr", hwnd)
    } else if IsObject(ui.startPoint) {
        rect := {x: Min(x, ui.startPoint.x), y: Min(y, ui.startPoint.y)
            , w: Abs(x - ui.startPoint.x) + 1, h: Abs(y - ui.startPoint.y) + 1}
        ui.borders[1].Move(rect.x, rect.y, rect.w, 2)
        ui.borders[2].Move(rect.x, rect.y + rect.h - 2, rect.w, 2)
        ui.borders[3].Move(rect.x, rect.y, 2, rect.h)
        ui.borders[4].Move(rect.x + rect.w - 2, rect.y, 2, rect.h)
        if msg = 0x202 {
            DllCall("ReleaseCapture")
            ui.startPoint := false
            SetTimer(D4Box_DetectorAcceptSelection.Bind(ui, rect), -1)
        }
    }
    return 0
}

D4Box_DetectorAcceptSelection(ui, rect) {
    global D4Box_DetectorUI
    if !IsObject(D4Box_DetectorUI) || D4Box_DetectorUI != ui || !IsObject(ui.selection)
        return
    try {
        node := D4Box_CubeCopy(ui.node)
        node.x := rect.x, node.y := rect.y, node.w := rect.w, node.h := rect.h
        node.frameW := ui.frame.w, node.frameH := ui.frame.h, node.dpi := ui.frame.dpi
        node.image := node.id "_" A_Now "_" Random(10000, 99999) ".bmp"
        D4Box_DetectorInvalidate(node)
        D4Box_DetectorValidate(node)
        D4Box_DetectorWriteBitmap(ui.capture, rect.x, rect.y, rect.w, rect.h, D4Box_DetectorRoot() "\" node.image)
        ui.node := node
        ui.dirty := true
        ui.note.Text := L("detect.retest")
    } catch as err {
        ui.note.Text := err.Message
    } finally {
        D4Box_DetectorEndSelection(ui)
        D4Box_DetectorFinishJob(ui)
    }
}

D4Box_DetectorEndSelection(ui) {
    for msg in [0x201, 0x200, 0x202]
        OnMessage(msg, D4Box_DetectorDrag, 0)
    DllCall("ReleaseCapture")
    if IsObject(ui.selection)
        ui.selection.Destroy()
    ui.selection := false
    if ui.HasProp("snapshot") && FileExist(ui.snapshot)
        FileDelete(ui.snapshot)
    ui.capture := false
}

D4Box_DetectorCancelSelection(*) {
    global D4Box_DetectorUI
    ui := D4Box_DetectorUI
    D4Box_DetectorEndSelection(ui)
    D4Box_DetectorFinishJob(ui)
}

D4Box_DetectorFinishJob(ui) {
    global D4Box_CubeCapturePending
    D4Box_CubeCapturePending := false
    ui.timer := 0
    ToolTip()
    D4Box_DetectorRefreshUI()
    for window in ui.hidden
        window.Show("NA")
    ui.hidden := []
    ui.gui.Show()
}

D4Box_DetectorClose(*) {
    global D4Box_DetectorUI, D4Box_CubeCapturePending
    if !IsObject(D4Box_DetectorUI)
        return true
    ui := D4Box_DetectorUI
    if D4Box_CubeCapturePending
        return true
    if !D4Box_DetectorDiscard()
        return true
    ui.gui.Destroy()
    D4Box_DetectorUI := ""
    return true
}

