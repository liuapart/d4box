; Synthetic/offline regressions. No real game screenshots or input.
D4Box_DetectionAssert(condition, message) {
    if !condition
        throw Error(message)
}

D4Box_DetectionExpectError(callback, message) {
    failed := false
    try callback.Call()
    catch
        failed := true
    D4Box_DetectionAssert(failed, message)
}

D4Box_DetectionFixture(root, frameW := 240, frameH := 140) {
    node := D4Box_DetectorNew()
    node.name := "绑定确认 fixture"
    node.x := 20, node.y := 20, node.w := 80, node.h := 40
    node.frameW := frameW, node.frameH := frameH
    node.image := node.id "_fixture.bmp"
    stride := ((node.w * 3 + 3) // 4) * 4
    cap := {ok: node.h, width: node.w, height: node.h, stride: stride, bits: Buffer(stride * node.h, 80)}
    D4Box_DetectorWriteBitmap(cap, 0, 0, node.w, node.h, root "\" node.image)
    return node
}

D4Box_TestDetectionModel() {
    root := A_ScriptDir "\.runtime\detector-model-" A_TickCount
    DirCreate(root)
    try {
        node := D4Box_DetectionFixture(root)
        D4Box_DetectorSave(node, root)
        D4Box_DetectionAssert(!D4Box_DetectorPassed(node), "captured region is a draft")
        D4Box_DetectionAssert(D4Box_DetectorRecordTest(node, true, [true, true, true]), "positive samples")
        D4Box_DetectionAssert(!D4Box_DetectorPassed(node), "positive-only not eligible")
        D4Box_DetectionAssert(!D4Box_DetectorRecordTest(node, false, [false, true, false]), "negative test rejects transient match")
        D4Box_DetectionAssert(!D4Box_DetectorRecordTest(node, false, [false]), "one sample cannot pass")
        D4Box_DetectorRecordTest(node, false, [false, false, false])
        D4Box_DetectionAssert(D4Box_DetectorPassed(node), "both tests required")
        D4Box_DetectorSave(node, root)
        loaded := D4Box_DetectorRead(root)[node.id]
        D4Box_DetectionAssert(loaded.name = node.name && D4Box_DetectorPassed(loaded), "unicode/evidence disk round trip")
        D4Box_DetectorRequire(node.id, D4Box_DetectorRead(root), root)
        node.name := "重命名后的确认区域"
        D4Box_DetectorSave(node, root)
        D4Box_DetectionAssert(D4Box_DetectorPassed(node), "rename keeps evidence")
        for key in ["variation", "margin", "x", "y", "w", "h", "frameW", "frameH", "dpi"] {
            changed := D4Box_CubeCopy(node)
            changed.%key% += 1
            D4Box_DetectionAssert(!D4Box_DetectorPassed(changed), "changed " key " invalidates evidence")
        }
        changed := D4Box_CubeCopy(node)
        changed.image := node.id "_new.bmp"
        D4Box_DetectionAssert(!D4Box_DetectorPassed(changed), "recapture invalidates evidence")
        changed := D4Box_CubeCopy(node)
        changed.variation += 1
        D4Box_DetectorSave(changed, root)
        D4Box_DetectionExpectError(() => D4Box_DetectorRequire(node.id, D4Box_DetectorRead(root), root), "stale evidence cannot run")
        D4Box_DetectorSave(node, root)
        duplicate := D4Box_DetectorNew()
        duplicate.name := node.name
        D4Box_DetectionExpectError(() => D4Box_DetectorSave(duplicate, root), "duplicate names rejected")
        invalid := D4Box_CubeCopy(node)
        invalid.image := "..\outside.bmp"
        D4Box_DetectionExpectError(() => D4Box_DetectorValidate(invalid), "template path traversal rejected")
        step := D4Box_CubeStep("Accept|confirm_if_present|any|0.4|0.5|180|-|24|1000|0|0|" node.id)
        roundTrip := D4Box_CubeStep(D4Box_CubeStepText(step))
        D4Box_DetectionAssert(roundTrip.detectorId = node.id && roundTrip.x = 0.4, "step retains stable id and separate click point")
        profile := {name: "流程 fixture", steps: [step], ready: ""}
        profiles := Map("fixture", profile)
        D4Box_DetectionExpectError(() => D4Box_DetectorDelete(node.id, profiles, root), "referenced step blocks deletion")
        profile.steps := []
        step.action := "check"
        profile.ready := D4Box_CubeStepText(step)
        D4Box_DetectionExpectError(() => D4Box_DetectorDelete(node.id, profiles, root), "ready reference blocks deletion")
        frame := {w: node.frameW, h: node.frameH, dpi: node.dpi}
        D4Box_DetectorCheckFrame(node, frame)
        frame.w += 1
        D4Box_DetectionExpectError(() => D4Box_DetectorCheckFrame(node, frame), "resolution mismatch rejected")
        frame.w -= 1, frame.dpi += 24
        D4Box_DetectionExpectError(() => D4Box_DetectorCheckFrame(node, frame), "DPI mismatch rejected")
        FileDelete(root "\" node.image)
        D4Box_DetectionExpectError(() => D4Box_DetectorRequire(node.id, D4Box_DetectorRead(root), root), "missing image cannot run")
        D4Box_DetectorDelete(node.id, Map(), root)
        D4Box_DetectionAssert(!D4Box_DetectorRead(root).Has(node.id), "unreferenced deletion")
    } finally {
        ; Only this invocation's generated fixture directory.
        DirDelete(root, true)
    }
}

D4Box_TestDetectionNative() {
    root := A_ScriptDir "\.runtime\detector-native-" A_TickCount
    DirCreate(root)
    canvas := Gui("-DPIScale +AlwaysOnTop", "d4box detector native fixture")
    canvas.BackColor := "112233"
    block := canvas.AddText("x20 y20 w80 h40 Background156AD4")
    detail := canvas.AddText("x28 y28 w19 h15 BackgroundDDAA22")
    try {
        canvas.Show("x30 y40 w240 h140")
        WinActivate("ahk_id " canvas.Hwnd)
        WinRedraw("ahk_id " canvas.Hwnd)
        ; Let desktop window transitions finish before taking the baseline.
        Sleep 500
        WinGetClientPos(&x, &y, &w, &h, "ahk_id " canvas.Hwnd)
        frame := {x: x, y: y, w: w, h: h, dpi: DllCall("GetDpiForWindow", "Ptr", canvas.Hwnd, "UInt")}
        cap := CaptureScreenRegion(x, y, w, h)
        node := D4Box_DetectionFixture(root, w, h)
        node.dpi := frame.dpi
        node.variation := 0
        D4Box_DetectorWriteBitmap(cap, 20, 20, 80, 40, root "\" node.image)
        try matched := D4Box_DetectorMatch(node, frame, root)
        catch as err {
            bitmap := LoadPicture(root "\" node.image)
            info := Buffer(32, 0)
            got := DllCall("GetObject", "Ptr", bitmap, "Int", info.Size, "Ptr", info)
            DllCall("DeleteObject", "Ptr", bitmap)
            throw Error(err.Message " / " err.What " / " err.Extra " / cap=" cap.ok " bmp=" got
                " size=" NumGet(info, 4, "Int") "x" NumGet(info, 8, "Int") " / screen=" A_ScreenWidth "x" A_ScreenHeight)
        }
        D4Box_DetectionAssert(matched, "native ImageSearch matches captured crop")
        block.Move(25, 25), detail.Move(33, 33)
        WinRedraw("ahk_id " canvas.Hwnd)
        Sleep 300
        D4Box_DetectionAssert(D4Box_DetectorMatch(node, frame, root), "native ImageSearch supports local search margin")
        block.Visible := false, detail.Visible := false
        WinRedraw("ahk_id " canvas.Hwnd)
        Sleep 300
        D4Box_DetectionAssert(!D4Box_DetectorMatch(node, frame, root), "native ImageSearch rejects missing pattern")
    } finally {
        canvas.Destroy()
        DirDelete(root, true)
    }
}

D4Box_TestDetectionUI() {
    global D4Box_DetectorUI, D4Box_CubeUI, D4Box_CubeCapturePending
    ; Test process has its own runtime directory. Back up any existing fixture library.
    root := D4Box_DetectorRoot()
    DirCreate(root)
    file := root "\regions.ini"
    backup := file ".smoke-backup"
    existed := FileExist(file)
    if existed
        FileCopy(file, backup, 1)
    node := D4Box_DetectionFixture(root)
    capturedPath := ""
    try {
        D4Box_DetectorRecordTest(node, true, [true, true, true])
        D4Box_DetectorRecordTest(node, false, [false, false, false])
        D4Box_DetectorSave(node, root)
        ready := D4Box_CubeStep("Ready region|check|any|0|0|0|-|0|1000|0|0|" node.id)
        profile := D4Box_CubeCopy(D4Box_CubeReadFile(D4Box_CubeBuiltinsPath(), true)["inventory_transmute"])
        profile.builtin := false, profile.source := "manual", profile.name := "Region run fixture"
        profile.ready := D4Box_CubeStepText(ready)
        profile.steps := [D4Box_CubeStep("Execute|execute|any|0.4|0.5|180|-|24|1000"), ready]
        D4Box_CubeValidate(profile, true)
        frame := {x: 0, y: 0, w: node.frameW, h: node.frameH, dpi: node.dpi}
        boundGuard := D4Box_DetectorBind(profile, frame)
        D4Box_DetectionAssert(boundGuard.detector.id = node.id && profile.steps[2].detector.id = node.id, "startup resolves region dependencies")
        frame.w += 1
        D4Box_DetectionExpectError(() => D4Box_DetectorBind(profile, frame), "startup rejects incompatible capture size")
        D4Box_OpenDetectorLibrary()
        D4Box_DetectionAssert(IsObject(D4Box_DetectorUI), "library GUI opens")
        D4Box_DetectorUI.node := D4Box_CubeCopy(node)
        D4Box_DetectorRefreshUI()
        D4Box_DetectionAssert(InStr(D4Box_DetectorUI.status.Text, L("detect.passed")), "passed state displayed")
        D4Box_DetectorUI.name.Value := "改名 fixture"
        renamed := D4Box_DetectorReadUI()
        D4Box_DetectionAssert(renamed.id = node.id && D4Box_DetectorPassed(renamed), "UI rename preserves identity")
        D4Box_CubeUI.profile.builtin := false
        D4Box_CubeUI.profile.steps.Push(D4Box_CubeStep("Accept|confirm_if_present|any|0.4|0.6|180|-|24|1500|0|0|" node.id))
        D4Box_CubeFillStep(D4Box_CubeUI.profile.steps.Length)
        selected := D4Box_CubeReadStep()
        D4Box_DetectionAssert(selected.detectorId = node.id && D4Box_CubeUI.detector.Text = node.name, "workflow chooses named detector")
        D4Box_DetectionAssert(!D4Box_CubeUI.probeX.Enabled && !D4Box_CubeUI.color.Enabled && D4Box_CubeUI.x.Enabled, "library settings separate from accept coordinates")
        D4Box_CubeUI.action.Choose(6)
        D4Box_CubeActionChanged()
        D4Box_DetectionAssert(!D4Box_CubeUI.x.Enabled && !D4Box_CubeUI.captureStep.Enabled, "region wait has no click coordinates")
        D4Box_CubeDetectorReady()
        D4Box_DetectionAssert(D4Box_DetectorId(D4Box_CubeStep(D4Box_CubeUI.profile.ready)) = node.id, "region reusable for ready guard")
        D4Box_DetectorUI.variation.Value := node.variation + 1
        changed := D4Box_DetectorReadUI()
        D4Box_DetectionAssert(!D4Box_DetectorPassed(changed), "UI edit requires retest")
        D4Box_DetectorSave(changed, root)
        D4Box_DetectionExpectError(() => D4Box_CubeValidate(profile, true), "F3 preflight rejects saved untested detector")
        D4Box_DetectionAssert(boundGuard.detector.variation = node.variation, "running snapshot remains independent of library edits")
        D4Box_CubeRefreshDetectorChoices()
        D4Box_DetectionAssert(D4Box_CubeDetectorSelected() = node.id && InStr(D4Box_CubeUI.detector.Text, changed.name)
            && InStr(D4Box_CubeUI.detector.Text, L("detect.unavailable", "")), "unavailable reference preserved, not silently downgraded")
        D4Box_CubeRefreshDetectorChoices("")
        for id in D4Box_CubeUI.detectorIds
            D4Box_DetectionAssert(id != node.id, "untested node not offered for new selection")
        D4Box_CubeUI.profile.steps.Pop()
        D4Box_CubeUI.profile.ready := ""
        ; Selection happens over our synthetic frozen bitmap, never over the live game.
        cap := {ok: 140, width: 240, height: 140, stride: 720, bits: Buffer(720 * 140, 80)}
        D4Box_CubeCapturePending := true
        D4Box_DetectorSelectRegion(D4Box_DetectorUI, cap, {x: 30, y: 40, w: 240, h: 140, dpi: 96})
        D4Box_DetectionAssert(IsObject(D4Box_DetectorUI.selection), "frozen selection window opens")
        selectionHwnd := D4Box_DetectorUI.selection.Hwnd
        D4Box_DetectorDrag(1, 12 | (15 << 16), 0x201, selectionHwnd)
        D4Box_DetectorDrag(1, 51 | (34 << 16), 0x200, selectionHwnd)
        D4Box_DetectorDrag(0, 51 | (34 << 16), 0x202, selectionHwnd)
        Sleep 100
        capturedPath := root "\" D4Box_DetectorUI.node.image
        D4Box_DetectionAssert(FileExist(capturedPath) && !D4Box_CubeCapturePending
            && !D4Box_DetectorPassed(D4Box_DetectorUI.node) && D4Box_DetectorUI.node.x = 12
            && D4Box_DetectorUI.node.y = 15 && D4Box_DetectorUI.node.w = 40 && D4Box_DetectorUI.node.h = 20
            , "capture saves crop and revokes prior tests")
        D4Box_CubeCapturePending := true
        D4Box_DetectorSelectRegion(D4Box_DetectorUI, cap, {x: 30, y: 40, w: 240, h: 140, dpi: 96})
        D4Box_DetectorCancelSelection()
        D4Box_DetectionAssert(!D4Box_CubeCapturePending && !IsObject(D4Box_DetectorUI.selection), "cancel releases capture state")
        D4Box_DetectorUI.dirty := false
        D4Box_DetectorClose()
    } finally {
        if IsObject(D4Box_DetectorUI) {
            if IsObject(D4Box_DetectorUI.selection)
                D4Box_DetectorEndSelection(D4Box_DetectorUI)
            D4Box_DetectorUI.gui.Destroy()
            D4Box_DetectorUI := ""
        }
        D4Box_CubeCapturePending := false
        if FileExist(file)
            FileDelete(file)
        if existed
            FileMove(backup, file, 1)
        FileDelete(root "\" node.image)
        if capturedPath != "" && FileExist(capturedPath)
            FileDelete(capturedPath)
    }
}

