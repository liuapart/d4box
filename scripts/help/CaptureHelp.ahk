; App modules are included by capture-help.ps1 in a fresh, isolated source copy.
; Never call InitializeGUI, register game hotkeys, start macro timers or send game input.
global D4Box_HelpOutput := A_Args[1]
DirCreate(D4Box_HelpOutput)
SetTimer(() => ExitApp(2), -60000)

D4Box_HelpShot(gui, name) {
    gui.Show()
    WinActivate(gui.Hwnd)
    WinRedraw(gui.Hwnd)
    Sleep 350
    WinGetClientPos(&x, &y, &w, &h, gui.Hwnd)
    cap := CaptureScreenRegion(x, y, w, h)
    if cap.ok != h
        throw Error("Cannot capture documentation window: " name)
    D4Box_DetectorWriteBitmap(cap, 0, 0, w, h, D4Box_HelpOutput "\" name ".bmp")
}

D4Box_HelpSamples(node, frame) {
    samples := []
    loop 3 {
        samples.Push(D4Box_DetectorMatch(node, frame))
        Sleep 100
    }
    return samples
}

D4Box_HelpVerifyNode(node, canvas, heading, frame) {
    canvas.Show()
    WinActivate(canvas.Hwnd)
    WinRedraw(canvas.Hwnd)
    Sleep 350
    if !D4Box_DetectorRecordTest(node, true, D4Box_HelpSamples(node, frame))
        throw Error("Synthetic panel positive test failed")
    heading.Visible := false
    WinRedraw(canvas.Hwnd)
    Sleep 250
    if !D4Box_DetectorRecordTest(node, false, D4Box_HelpSamples(node, frame))
        throw Error("Synthetic panel negative test failed")
    heading.Visible := true
    D4Box_DetectorSave(node)
    return node
}

try {
    appLang := "zh"
    IniWrite("light", A_ScriptDir "\settings.ini", "UI", "Theme")
    CreateMainGUI()
    MUI_ApplyNativeTheme(myGui)
    D4Box_WorkflowBindings := [{id: "default", key: "F3", profileId: "inventory_transmute"}]
    D4Box_BindingsRefresh()
    myGui.Show("x40 y30 w720 h532")
    D4Box_HelpShot(myGui, "01-main")

    D4Box_MainTabs.Choose(2)
    D4Box_HelpShot(myGui, "02-macros-default")
    skillControls[1].strategy.Choose(2)
    skillControls[1].interval.Value := "800"
    skillControls[1].delay.Value := "0"
    skillControls[1].random.Value := 0
    D4Box_HelpShot(myGui, "03-macro-example")
    D4Box_MainTabs.Choose(1)

    D4Box_OpenCubeEditor()
    D4Box_CubeUI.gui.Show("x40 y20")
    D4Box_HelpShot(D4Box_CubeUI.gui, "04-workflow-builtin")
    D4Box_CubeNewProfile()
    D4Box_HelpShot(D4Box_CubeUI.gui, "05-workflow-new")
    profile := D4Box_CubeCopy(D4Box_CubeUI.profile)
    profile.name := "示例·单物品处理（需自行采集）"
    profile.ready := "示例就绪|check|any|0.1|0.2|0|AABBCC|20|3000"
    profile.steps := [D4Box_CubeStep("选择当前物品|rightclick|any|0|0|180|-|20|3000")
        , D4Box_CubeStep("选择目标配方|select|any|0.4|0.38|180|-|20|3000")
        , D4Box_CubeStep("执行配方|execute|any|0.17|0.79|0|-|20|3000")
        , D4Box_CubeStep("按需接受绑定|confirm_if_present|any|0.43|0.56|180|AABBCC|20|1500|0.4|0.3")
        , D4Box_CubeStep("清除本次物品|clear|any|0.17|0.68|0|-|20|3000")
        , D4Box_CubeStep("等待结果状态|check|any|0.1|0.2|0|AABBCC|20|3000")]
    D4Box_CubeUI.profiles[profile.id] := profile
    D4Box_CubeLoadEditor(profile.id)
    D4Box_HelpShot(D4Box_CubeUI.gui, "06-workflow-item-target")
    D4Box_CubeRenderSteps(4)
    D4Box_HelpShot(D4Box_CubeUI.gui, "07-confirm-color")
    D4Box_CubeUI.gui.Hide()
    myGui.Hide()

    D4Box_OpenDetectorLibrary()
    ui := D4Box_DetectorUI
    ui.node.name := "示例·绑定确认面板"
    D4Box_DetectorRefreshUI()
    D4Box_HelpShot(ui.gui, "08-detector-new")
    ui.gui.Hide()
    canvas := Gui("-DPIScale", "文档演示面板（不是游戏）")
    canvas.BackColor := "18212B"
    canvas.SetFont("s13 cFFFFFF", "Microsoft YaHei UI")
    canvas.AddText("x24 y24 w670 h28", "文档演示面板 · 不是暗黑 IV 游戏截图")
    canvas.AddText("x160 y100 w400 h270 Background293544")
    heading := canvas.AddText("x205 y128 w310 h52 Background293544", "示例确认面板")
    canvas.SetFont("s10 cCBD5E1")
    canvas.AddText("x205 y208 w310 h70 Background293544", "框选固定标题，避开动态内容。`n本面板用于说明采集与测试方法。")
    canvas.AddButton("x205 y304 w130 h32", "接受（示例）")
    canvas.AddButton("x374 y304 w130 h32", "取消（示例）")
    canvas.Show("x40 y40 w720 h440")
    Sleep 400
    WinGetClientPos(&x, &y, &w, &h, canvas.Hwnd)
    frame := {x: x, y: y, w: w, h: h, dpi: DllCall("GetDpiForWindow", "Ptr", canvas.Hwnd, "UInt")}
    cap := CaptureScreenRegion(x, y, w, h)
    D4Box_CubeCapturePending := true
    D4Box_DetectorSelectRegion(ui, cap, frame)
    ToolTip()
    selectionHwnd := ui.selection.Hwnd
    D4Box_DetectorDrag(1, 200 | (123 << 16), 0x201, selectionHwnd)
    D4Box_DetectorDrag(1, 518 | (182 << 16), 0x200, selectionHwnd)
    D4Box_HelpShot(ui.selection, "09-detector-selection")
    DllCall("ReleaseCapture")
    D4Box_DetectorAcceptSelection(ui, {x: 200, y: 123, w: 319, h: 60})
    D4Box_HelpShot(ui.gui, "10-detector-captured")
    ui.gui.Hide()
    confirmNode := D4Box_HelpVerifyNode(ui.node, canvas, heading, frame)
    canvas.Hide()
    ui.node := confirmNode
    ui.nodes := D4Box_DetectorRead()
    ui.dirty := false
    D4Box_DetectorRefreshUI()
    D4Box_HelpShot(ui.gui, "11-detector-tested")
    ui.gui.Hide()

    additionalNodes := []
    for title in ["示例·魔盒就绪", "示例·结果状态"] {
        canvas.Show()
        heading.Text := title
        WinRedraw(canvas.Hwnd)
        Sleep 350
        node := D4Box_DetectorNew()
        node.name := title
        node.x := 200, node.y := 123, node.w := 319, node.h := 60
        node.frameW := frame.w, node.frameH := frame.h, node.dpi := frame.dpi
        node.image := node.id "_help.bmp"
        cap := CaptureScreenRegion(frame.x, frame.y, frame.w, frame.h)
        D4Box_DetectorWriteBitmap(cap, node.x, node.y, node.w, node.h, D4Box_DetectorRoot() "\" node.image)
        additionalNodes.Push(D4Box_HelpVerifyNode(node, canvas, heading, frame))
    }
    canvas.Destroy()
    D4Box_DetectorClose()
    profile.ready := "就绪检查|check|any|0|0|0|-|20|3000|0|0|" additionalNodes[1].id
    profile.steps[4].detectorId := confirmNode.id
    profile.steps[6].detectorId := additionalNodes[2].id
    D4Box_CubeUI.profiles[profile.id] := profile
    D4Box_CubeLoadEditor(profile.id)
    D4Box_CubeUI.gui.Show()
    D4Box_CubeRenderSteps(4)
    D4Box_HelpShot(D4Box_CubeUI.gui, "12-confirm-region")
    D4Box_CubeRenderSteps(6)
    D4Box_HelpShot(D4Box_CubeUI.gui, "13-final-check")
    D4Box_CubeSave(profile)
    inventory := D4Box_CubeCopy(profile)
    inventory.id := "user_help_inventory", inventory.name := "示例·背包逐格处理"
    inventory.source := "inventory", inventory.quality := "red", inventory.maxItems := 1
    D4Box_CubeSave(inventory)
    D4Box_CubeUI.profiles[inventory.id] := inventory
    D4Box_CubeLoadEditor(inventory.id)
    D4Box_HelpShot(D4Box_CubeUI.gui, "14-inventory-workflow")
    D4Box_CubeUI.dirty := false
    D4Box_CubeCloseEditor()

    D4Box_WorkflowBindings := [{id: "default", key: "F3", profileId: profile.id}]
    D4Box_BindingsRefresh()
    myGui.Show()
    D4Box_BindingEdit(true)
    D4Box_BindingEditor.key.Value := "F6"
    for index, id in D4Box_BindingEditor.ids
        if id = inventory.id
            D4Box_BindingEditor.flow.Choose(index)
    D4Box_HelpShot(D4Box_BindingEditor.gui, "15-add-binding")
    D4Box_BindingClose()
    D4Box_WorkflowBindings.Push({id: "help_extra", key: "F6", profileId: inventory.id})
    D4Box_BindingsRefresh()
    D4Box_HelpShot(myGui, "16-bindings-example")

    ; Display-only fixture: no executor or input timers are started.
    D4Box_CubeBusy := true, isAutoTransmuting := true
    D4Box_WorkflowName := profile.name
    D4Box_WorkflowStep := "6 / 6 · 等待结果状态（状态展示，未执行）"
    D4Box_RefreshRuntimeStatus()
    D4Box_HelpShot(myGui, "17-running-state-demo")
    D4Box_CubeBusy := false, isAutoTransmuting := false
    D4Box_WorkflowName := "", D4Box_WorkflowStep := ""
    D4Box_RefreshRuntimeStatus()
    D4Box_ShowUpdates(false)
    D4Box_HelpShot(D4Box_UpdateUI.gui, "18-updates")
    D4Box_CloseUpdates()
    FileAppend("PASS documentation screenshots v" D4Box_Version() "`n", D4Box_HelpOutput "\capture-result.txt", "UTF-8")
    ExitApp(0)
} catch as err {
    FileAppend("FAIL " err.Message " @" err.File ":" err.Line "`n", D4Box_HelpOutput "\capture-result.txt", "UTF-8")
    ExitApp(1)
}
