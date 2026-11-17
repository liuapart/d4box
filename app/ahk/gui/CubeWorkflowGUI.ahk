; Workflow editor. Saving never sends input; dynamic hotkeys run saved workflow IDs.
D4Box_OpenCubeEditor(*) {
    global D4Box_CubeUI, D4Box_CubeBusy, D4Box_CubeCapturePending
    if D4Box_CubeCapturePending
        return
    if D4Box_CubeBusy {
        MsgBox(L("cube.busy"), "d4box")
        return
    }
    if IsObject(D4Box_CubeUI) {
        D4Box_CubeUI.gui.Show()
        return
    }
    try {
        profiles := D4Box_CubeProfiles()
        editor := Gui("", L("cube.title"))
        editor.SetFont("s9", "Segoe UI")
        ui := {gui: editor, profiles: profiles, ids: [], profile: "", row: 0, loading: true
            , dirty: false, stepDirty: false, captureTimer: 0}
        D4Box_CubeUI := ui
        ui.selector := editor.AddDropDownList("x16 y16 w360")
        editor.AddButton("x388 y14 w104", L("cube.newProfile")).OnEvent("Click", D4Box_CubeNewProfile)
        editor.AddButton("x504 y14 w124", L("cube.copy")).OnEvent("Click", D4Box_CubeCopyProfile)
        editor.AddButton("x640 y14 w120", L("cube.deleteProfile")).OnEvent("Click", D4Box_CubeDeleteProfile)
        editor.AddText("x16 y57 w85", L("cube.name"))
        ui.name := editor.AddEdit("x110 y53 w310")
        editor.AddText("x448 y57 w85", L("cube.source"))
        ui.source := editor.AddDropDownList("x540 y53 w220", [L("cube.inventory"), L("cube.manual")])
        editor.AddText("x16 y92 w85", L("cube.quality"))
        ui.quality := editor.AddDropDownList("x110 y88 w310", D4Box_CubeQualityLabels())
        editor.AddText("x448 y92 w85", L("cube.space"))
        ui.space := editor.AddDropDownList("x540 y88 w220", [L("cube.screen"), L("cube.client")])
        editor.AddText("x16 y127 w85", L("cube.maxItems"))
        ui.maxItems := editor.AddEdit("x110 y123 w75 Number")
        editor.AddText("x215 y127 w150", L("cube.timeout"))
        ui.timeout := editor.AddEdit("x370 y123 w90 Number")
        ui.guardText := editor.AddText("x16 y162 w520 h38")
        editor.AddButton("x540 y157 w220", L("cube.captureReady")).OnEvent("Click", D4Box_CubeCapture.Bind("ready"))
        ui.list := editor.AddListView("x16 y205 w744 h155 -Multi", [L("cube.stepName"), L("cube.action"), L("cube.applies")
            , "X", "Y", L("cube.delay")])
        for index, width in [185, 145, 125, 80, 80, 105]
            ui.list.ModifyCol(index, width)
        ui.label := editor.AddEdit("x16 y375 w230")
        ui.action := editor.AddDropDownList("x260 y375 w245", D4Box_CubeActionLabels())
        ui.when := editor.AddDropDownList("x520 y375 w240", [L("cube.any"), L("cube.red"), L("cube.yellow"), L("cube.blue")])
        editor.AddText("x16 y414 w18", "X")
        ui.x := editor.AddEdit("x38 y410 w85")
        editor.AddText("x135 y414 w18", "Y")
        ui.y := editor.AddEdit("x157 y410 w85")
        editor.AddText("x260 y414 w95", L("cube.delay"))
        ui.delay := editor.AddEdit("x360 y410 w80 Number")
        ui.captureStep := editor.AddButton("x460 y407 w300", L("cube.captureStep"))
        ui.captureStep.OnEvent("Click", D4Box_CubeCapture.Bind("step"))
        editor.AddText("x16 y449 w85", L("cube.color"))
        ui.color := editor.AddEdit("x105 y445 w100")
        editor.AddText("x220 y449 w100", L("cube.tolerance"))
        ui.tolerance := editor.AddEdit("x325 y445 w65 Number")
        editor.AddText("x410 y449 w150", L("cube.checkTimeout"))
        ui.checkTimeout := editor.AddEdit("x570 y445 w90 Number")
        editor.AddText("x16 y484 w65", L("cube.probeX"))
        ui.probeX := editor.AddEdit("x85 y480 w85")
        editor.AddText("x190 y484 w65", L("cube.probeY"))
        ui.probeY := editor.AddEdit("x260 y480 w85")
        ui.captureProbe := editor.AddButton("x460 y477 w300", L("cube.captureConfirmProbe"))
        ui.captureProbe.OnEvent("Click", D4Box_CubeCapture.Bind("probe"))
        editor.AddText("x16 y519 w90", L("detect.node"))
        ui.detector := editor.AddDropDownList("x110 y515 w310")
        ui.detectorIds := [""]
        ui.detector.OnEvent("Change", D4Box_CubeDetectorChanged)
        ui.useReady := editor.AddButton("x432 y513 w145", L("detect.useReady"))
        ui.useReady.OnEvent("Click", D4Box_CubeDetectorReady)
        editor.AddButton("x589 y513 w171", L("detect.title")).OnEvent("Click", D4Box_OpenDetectorLibrary)
        for spec in [["add", "cube.addStep", 16], ["update", "cube.updateStep", 170], ["delete", "cube.deleteStep", 324]
            , ["up", "cube.up", 478], ["down", "cube.down", 632]]
            editor.AddButton("x" spec[3] " y555 w128", L(spec[2])).OnEvent("Click", D4Box_CubeEditStep.Bind(spec[1]))
        ui.note := editor.AddText("x16 y598 w744 h42")
        editor.AddButton("x16 y652 w130", L("binding.saveOnly")).OnEvent("Click", D4Box_CubeSaveEditor.Bind(false))
        editor.AddButton("x156 y652 w254", L("cube.saveUse")).OnEvent("Click", D4Box_CubeSaveEditor.Bind(true))
        editor.AddButton("x420 y652 w164", L("cube.preview")).OnEvent("Click", D4Box_CubePreview)
        editor.AddButton("x594 y652 w166", L("cube.close")).OnEvent("Click", D4Box_CubeCloseEditor)
        for control in [ui.name, ui.source, ui.quality, ui.space, ui.maxItems, ui.timeout]
            control.OnEvent("Change", D4Box_CubeDirty)
        for control in [ui.label, ui.action, ui.when, ui.x, ui.y, ui.delay, ui.color, ui.tolerance, ui.checkTimeout, ui.probeX, ui.probeY]
            control.OnEvent("Change", D4Box_CubeStepDirty)
        ui.action.OnEvent("Change", D4Box_CubeActionChanged)
        ui.source.OnEvent("Change", D4Box_CubeSourceChanged)
        ui.selector.OnEvent("Change", D4Box_CubeChangeProfile)
        ui.list.OnEvent("ItemSelect", D4Box_CubeSelectStep)
        editor.OnEvent("Close", D4Box_CubeCloseEditor)
        editor.OnEvent("Escape", D4Box_CubeCloseEditor)
        D4Box_CubeRefreshProfiles(D4Box_DefaultBindingProfile())
        editor.Show("w780 h697")
    } catch as err {
        if IsObject(D4Box_CubeUI)
            D4Box_CubeUI.gui.Destroy()
        D4Box_CubeUI := ""
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeQualityLabels() {
    return [L("cube.red"), L("cube.yellow"), L("cube.blue"), L("cube.redyellow"), L("cube.any")]
}

D4Box_CubeActionLabels() {
    return [L("cube.select"), L("cube.execute"), L("cube.confirm"), L("cube.clear"), L("cube.wait"), L("cube.check"), L("cube.rightclick"), L("cube.confirm_if_present")]
}

D4Box_CubeIndex(values, wanted) {
    for index, value in values
        if value = wanted
            return index
    return 1
}

D4Box_CubeDirty(*) {
    global D4Box_CubeUI
    if IsObject(D4Box_CubeUI) && !D4Box_CubeUI.loading
        D4Box_CubeUI.dirty := true
}

D4Box_CubeStepDirty(*) {
    global D4Box_CubeUI
    if IsObject(D4Box_CubeUI) && !D4Box_CubeUI.loading {
        D4Box_CubeUI.stepDirty := true
        D4Box_CubeUI.dirty := true
    }
}

D4Box_CubeRefreshProfiles(selected) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    ui.loading := true
    ui.ids := []
    names := []
    for id, profile in ui.profiles {
        ui.ids.Push(id)
        names.Push((profile.builtin ? L("cube.builtin") " " : "") D4Box_CubeProfileName(profile))
    }
    ui.selector.Delete()
    ui.selector.Add(names)
    ui.selector.Choose(D4Box_CubeIndex(ui.ids, selected))
    D4Box_CubeLoadEditor(ui.ids[ui.selector.Value])
}

D4Box_CubeChangeProfile(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.loading
        return
    id := ui.ids[ui.selector.Value]
    if id = ui.profile.id
        return
    if ui.dirty && MsgBox(L("cube.discard"), "d4box", "YesNo") != "Yes" {
        ui.selector.Choose(D4Box_CubeIndex(ui.ids, ui.profile.id))
        return
    }
    D4Box_CubeLoadEditor(id)
}

D4Box_CubeLoadEditor(id) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    ui.loading := true
    p := D4Box_CubeCopy(ui.profiles[id])
    ui.profile := p
    ui.name.Value := D4Box_CubeProfileName(p)
    ui.source.Choose(D4Box_CubeIndex(["inventory", "manual"], p.source))
    ui.quality.Choose(D4Box_CubeIndex(["red", "yellow", "blue", "redyellow", "any"], p.quality))
    ui.space.Choose(D4Box_CubeIndex(["screen", "client"], p.space))
    ui.maxItems.Value := p.maxItems
    ui.timeout.Value := p.timeout // 1000
    ui.guardText.Text := p.ready = "" ? L("cube.noReady") : L("cube.readySet")
    if p.ready != "" {
        readyId := D4Box_DetectorId(D4Box_CubeStep(p.ready))
        if readyId != "" {
            nodes := D4Box_DetectorRead()
            ui.guardText.Text .= " — " (nodes.Has(readyId) ? nodes[readyId].name : readyId)
        }
    }
    ui.note.Text := p.builtin ? L("cube.builtinNote") : L("cube.customNote")
    for control in [ui.name, ui.source, ui.quality, ui.space, ui.maxItems, ui.timeout
        , ui.label, ui.action, ui.when, ui.x, ui.y, ui.delay, ui.color, ui.tolerance, ui.checkTimeout, ui.probeX, ui.probeY]
        control.Enabled := !p.builtin
    D4Box_CubeSourceChanged()
    D4Box_CubeRenderSteps(1)
    ui.dirty := false
    ui.stepDirty := false
    ui.loading := false
}

D4Box_CubeRenderSteps(row := 0) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    ui.loading := true
    ui.list.Delete()
    for step in ui.profile.steps
        ui.list.Add("", step.label, L("cube." step.action), L("cube." step.quality), step.action = "rightclick" ? "—" : step.x, step.action = "rightclick" ? "—" : step.y, step.delay)
    ui.row := Min(row, ui.profile.steps.Length)
    if ui.row > 0
        ui.list.Modify(ui.row, "Select Focus Vis")
    D4Box_CubeFillStep(ui.row)
    ui.loading := false
}

D4Box_CubeFillStep(row) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    wasLoading := ui.loading
    ui.loading := true
    step := row > 0 ? ui.profile.steps[row] : {label: L("cube.newStep"), action: "select", quality: "any"
        , x: 0.445, y: 0.380, delay: 180, color: "-", tolerance: 24, timeout: 3000}
    ui.label.Value := step.label
    ui.action.Choose(D4Box_CubeIndex(["select", "execute", "confirm", "clear", "wait", "check", "rightclick", "confirm_if_present"], step.action))
    ui.when.Choose(D4Box_CubeIndex(["any", "red", "yellow", "blue"], step.quality))
    ui.x.Value := step.action = "rightclick" ? "" : step.x
    ui.y.Value := step.action = "rightclick" ? "" : step.y
    ui.probeX.Value := step.HasProp("probeX") ? step.probeX : step.x
    ui.probeY.Value := step.HasProp("probeY") ? step.probeY : step.y
    ui.delay.Value := step.delay
    ui.color.Value := step.color
    ui.tolerance.Value := step.tolerance
    ui.checkTimeout.Value := step.timeout
    D4Box_CubeRefreshDetectorChoices(D4Box_DetectorId(step))
    D4Box_CubeActionChanged()
    ui.stepDirty := false
    ui.loading := wasLoading
}

D4Box_CubeItemTargetLabel(source) {
    return L(source = "inventory" ? "cube.inventoryTarget" : "cube.mouseTarget")
}

D4Box_CubeActionChanged(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    optionalConfirm := ui.action.Value = 8
    usesDetector := D4Box_CubeDetectorSelected() != "" && (optionalConfirm || ui.action.Value = 6)
    ui.detector.Enabled := !ui.profile.builtin && (optionalConfirm || ui.action.Value = 6)
    ui.useReady.Enabled := !ui.profile.builtin && usesDetector
    ui.color.Enabled := !ui.profile.builtin && !usesDetector
    ui.tolerance.Enabled := !ui.profile.builtin && !usesDetector
    dynamic := ui.action.Value = 7
    for control in [ui.probeX, ui.probeY, ui.captureProbe]
        control.Enabled := optionalConfirm && !ui.profile.builtin && !usesDetector
    ui.captureStep.Text := L(optionalConfirm ? "cube.captureConfirmClick" : "cube.captureStep")
    enabled := !ui.profile.builtin && !dynamic && !(usesDetector && ui.action.Value = 6)
    ui.x.Enabled := enabled
    ui.y.Enabled := enabled
    ui.captureStep.Enabled := enabled
    if dynamic {
        ui.x.Value := ""
        ui.y.Value := ""
    } else {
        if ui.x.Value = ""
            ui.x.Value := 0
        if ui.y.Value = ""
            ui.y.Value := 0
    }
    if dynamic
        ui.note.Text := L("cube.itemTargetNote", D4Box_CubeItemTargetLabel(["inventory", "manual"][ui.source.Value]))
    else if usesDetector
        ui.note.Text := L("detect.workflowHint")
    else if optionalConfirm
        ui.note.Text := L("cube.confirmNote")
    else
        ui.note.Text := ui.profile.builtin ? L("cube.builtinNote") : L("cube.customNote")
}

D4Box_CubeReadStep() {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    return {label: Trim(ui.label.Value), action: ["select", "execute", "confirm", "clear", "wait", "check", "rightclick", "confirm_if_present"][ui.action.Value]
        , quality: ["any", "red", "yellow", "blue"][ui.when.Value], x: ui.action.Value = 7 ? 0 : Number(ui.x.Value), y: ui.action.Value = 7 ? 0 : Number(ui.y.Value)
        , delay: Integer(ui.delay.Value), color: Trim(ui.color.Value), tolerance: Integer(ui.tolerance.Value)
        , timeout: Integer(ui.checkTimeout.Value)
        , probeX: ui.action.Value = 8 ? Number(ui.probeX.Value) : 0
        , probeY: ui.action.Value = 8 ? Number(ui.probeY.Value) : 0
        , detectorId: (ui.action.Value = 6 || ui.action.Value = 8) ? D4Box_CubeDetectorSelected() : ""}
}

D4Box_CubeDetectorSelected() {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    return ui.detector.Value ? ui.detectorIds[ui.detector.Value] : ""
}

D4Box_CubeRefreshDetectorChoices(selected := unset) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if !IsObject(ui) || !ui.HasProp("detector")
        return
    if !IsSet(selected)
        selected := D4Box_CubeDetectorSelected()
    nodes := D4Box_DetectorRead()
    ui.detectorIds := [""]
    labels := [L("detect.legacy")]
    for id, node in nodes {
        if !D4Box_DetectorPassed(node) || !FileExist(D4Box_DetectorRoot() "\" node.image)
            continue
        ui.detectorIds.Push(id)
        labels.Push(node.name)
    }
    found := selected = ""
    for id in ui.detectorIds
        if id = selected
            found := true
    if !found {
        ui.detectorIds.Push(selected)
        labels.Push(L("detect.unavailable", nodes.Has(selected) ? nodes[selected].name : selected))
    }
    wasLoading := ui.loading
    ui.loading := true
    ui.detector.Delete()
    ui.detector.Add(labels)
    ui.detector.Choose(D4Box_CubeIndex(ui.detectorIds, selected))
    ui.loading := wasLoading
    if IsObject(ui.profile)
        D4Box_CubeActionChanged()
}

D4Box_CubeDetectorChanged(*) {
    D4Box_CubeStepDirty()
    D4Box_CubeActionChanged()
}

D4Box_CubeDetectorReady(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    try {
        id := D4Box_CubeDetectorSelected()
        node := D4Box_DetectorRequire(id, D4Box_DetectorRead())
        guard := D4Box_CubeStep(L("cube.ready") "|check|any|0|0|0|-|0|3000|0|0|" id)
        ui.profile.ready := D4Box_CubeStepText(guard)
        ui.guardText.Text := L("cube.readySet") " — " node.name
        ui.dirty := true
    } catch as err
        MsgBox(err.Message, "d4box")
}

D4Box_CubeFlushStep() {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.stepDirty && !ui.profile.builtin {
        step := D4Box_CubeReadStep()
        if ui.row > 0 {
            ui.profile.steps[ui.row] := step
            wasLoading := ui.loading
            ui.loading := true
            ui.list.Modify(ui.row, "", step.label, L("cube." step.action), L("cube." step.quality), step.action = "rightclick" ? "—" : step.x, step.action = "rightclick" ? "—" : step.y, step.delay)
            ui.loading := wasLoading
        } else {
            ui.profile.steps.Push(step)
        }
        ui.stepDirty := false
    }
}

D4Box_CubeSelectStep(ctrl, row, selected) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.loading || !selected
        return
    try {
        D4Box_CubeFlushStep()
        ui.row := row
        D4Box_CubeFillStep(row)
    } catch as err {
        ui.loading := true
        if ui.row > 0
            ui.list.Modify(ui.row, "Select Focus")
        ui.loading := false
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeEditStep(action, *) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.profile.builtin {
        MsgBox(L("cube.copyFirst"), "d4box")
        return
    }
    try {
        switch action {
            case "add":
                ui.profile.steps.Push(D4Box_CubeReadStep())
                ui.row := ui.profile.steps.Length
            case "update":
                if ui.row > 0
                    ui.profile.steps[ui.row] := D4Box_CubeReadStep()
            case "delete":
                if ui.row > 0
                    ui.profile.steps.RemoveAt(ui.row)
            default:
                D4Box_CubeFlushStep()
                next := ui.row + (action = "up" ? -1 : 1)
                if ui.row > 0 && next > 0 && next <= ui.profile.steps.Length {
                    step := ui.profile.steps[ui.row]
                    ui.profile.steps[ui.row] := ui.profile.steps[next]
                    ui.profile.steps[next] := step
                    ui.row := next
                }
        }
        ui.dirty := true
        ui.stepDirty := false
        D4Box_CubeRenderSteps(ui.row)
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeReadEditor() {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    D4Box_CubeFlushStep()
    p := D4Box_CubeCopy(ui.profile)
    if !p.builtin {
        p.name := Trim(ui.name.Value)
        p.source := ["inventory", "manual"][ui.source.Value]
        p.quality := ["red", "yellow", "blue", "redyellow", "any"][ui.quality.Value]
        p.space := ["screen", "client"][ui.space.Value]
        p.maxItems := Integer(ui.maxItems.Value)
        p.timeout := Integer(ui.timeout.Value) * 1000
    }
    return p
}

D4Box_CubeSourceChanged(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    enabled := !ui.profile.builtin && ui.source.Value = 1
    ui.quality.Enabled := enabled
    ui.maxItems.Enabled := enabled
    D4Box_CubeActionChanged()
}

D4Box_CubeNewProfile(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.dirty && MsgBox(L("cube.discard"), "d4box", "YesNo") != "Yes"
        return
    p := D4Box_CubeCopy(ui.profiles["inventory_transmute"])
    p.steps := D4Box_CubePlan(p, "red")
    for step in p.steps
        step.quality := "any"
    p.id := "user_" A_Now "_" Random(10000, 99999)
    p.builtin := false
    p.name := L("cube.newProfile")
    p.nameEn := ""
    p.legacyYellow := false
    p.source := "manual"
    p.space := "client"
    p.maxItems := 1
    p.timeout := 60000
    p.ready := ""
    ui.profiles[p.id] := p
    D4Box_CubeRefreshProfiles(p.id)
    ui.dirty := true
}

D4Box_CubeCopyProfile(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    try {
        p := D4Box_CubeReadEditor()
        p.id := "user_" A_Now "_" Random(10000, 99999)
        p.name := D4Box_CubeProfileName(p) " " L("cube.copySuffix")
        p.builtin := false
        p.legacyYellow := false
        p.nameEn := ""
        ui.profiles[p.id] := p
        D4Box_CubeRefreshProfiles(p.id)
        ui.dirty := true
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeSaveEditor(useDefault := true, *) {
    global D4Box_CubeUI, D4Box_CubeBusy
    if D4Box_CubeBusy
        return
    ui := D4Box_CubeUI
    try {
        p := D4Box_CubeReadEditor()
        D4Box_CubeValidate(p)
        D4Box_BindingsEditable()
        ; Persist content first. The default binding is the only selection authority;
        ; a failed binding save must not change the legacy migration selection.
        if !p.builtin
            D4Box_CubeSave(p)
        if useDefault
            D4Box_BindingsSelectDefault(p.id)
        else
            D4Box_BindingsRefresh()
        ui.profiles[p.id] := p
        D4Box_CubeRefreshProfiles(p.id)
        issue := D4Box_CubeRunIssue(p)
        if issue != "" {
            ui.note.Text := L("binding.savedDraft", issue)
            UpdateStatus("idle", L("binding.savedDraft", issue))
            MsgBox(L("binding.savedDraft", issue), L("cube.cannotRun"))
        } else {
            ui.note.Text := L(useDefault ? "cube.saved" : "binding.savedOnly")
            UpdateStatus("idle", useDefault ? L("cube.selected", D4Box_CubeProfileName(p)) : L("binding.savedOnly"))
        }
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeDeleteProfile(*) {
    global D4Box_CubeUI
    ui := D4Box_CubeUI
    if ui.profile.builtin {
        MsgBox(L("cube.copyFirst"), "d4box")
        return
    }
    keys := D4Box_BindingReferences(ui.profile.id)
    if keys != "" {
        MsgBox(L("binding.referenced", keys), "d4box")
        return
    }
    if MsgBox(L("cube.deleteQuestion"), "d4box", "YesNo") != "Yes"
        return
    try {
        D4Box_CubeSave("", "inventory_transmute", ui.profile.id)
        ui.profiles.Delete(ui.profile.id)
        D4Box_CubeRefreshProfiles("inventory_transmute")
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubePreview(*) {
    global D4Box_CubeUI
    try {
        p := D4Box_CubeReadEditor()
        D4Box_CubeValidate(p)
        text := D4Box_CubeProfileName(p) "`r`n" L("cube.previewNote") "`r`n"
        for quality in D4Box_CubeQualities(p) {
            text .= "`r`n[" L("cube." quality) "]`r`n"
            for index, step in D4Box_CubeItemPlan(p, quality) {
                targetText := step.action = "rightclick" ? D4Box_CubeItemTargetLabel(p.source) : step.x ", " step.y
                if D4Box_DetectorId(step) != "" {
                    nodes := D4Box_DetectorRead()
                    targetText .= "; " L("detect.node") ": " (nodes.Has(step.detectorId) ? nodes[step.detectorId].name : step.detectorId)
                } else if step.action = "confirm_if_present"
                    targetText .= ("; " L("cube.probeX") "=" step.probeX ", " L("cube.probeY") "=" step.probeY
                        ", #" step.color ", ±" step.tolerance ", " step.timeout "ms")
                text .= index ". " step.label " — " L("cube." step.action) " (" targetText ") +" step.delay "ms`r`n"
            }
        }
        preview := Gui("+Owner" D4Box_CubeUI.gui.Hwnd, L("cube.preview"))
        preview.SetFont("s10", "Segoe UI")
        preview.AddEdit("w720 h470 ReadOnly", text)
        preview.OnEvent("Close", (*) => preview.Destroy())
        preview.OnEvent("Escape", (*) => preview.Destroy())
        preview.Show()
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_CubeCapture(kind, *) {
    global D4Box_CubeUI, D4Box_CubeCapturePending, D4Box_CubeBusy
    ui := D4Box_CubeUI
    if ui.profile.builtin {
        MsgBox(L("cube.copyFirst"), "d4box")
        return
    }
    if D4Box_CubeBusy || D4Box_CubeCapturePending
        return
    if kind = "step" && ui.action.Value = 8
        kind := "confirmClick"
    D4Box_CubeCapturePending := true
    ui.captureTimer := D4Box_CubeCaptureNow.Bind(ui, kind)
    ui.gui.Hide()
    ToolTip(L("cube.captureHint"))
    SetTimer(ui.captureTimer, -3000)
}

D4Box_CubeCaptureNow(ui, kind) {
    global D4Box_CubeUI, D4Box_CubeCapturePending
    oldMouse := A_CoordModeMouse
    oldPixel := A_CoordModePixel
    try {
        ToolTip()
        Sleep 150
        if !IsObject(D4Box_CubeUI) || D4Box_CubeUI != ui
            return
        hwnd := WinActive("ahk_class Diablo IV Main Window Class")
        if !hwnd
            throw Error(L("cube.noWindow"))
        frame := D4Box_CubeFrame(hwnd, ["screen", "client"][ui.space.Value])
        CoordMode "Mouse", "Screen"
        CoordMode "Pixel", "Screen"
        MouseGetPos(&x, &y)
        if x < frame.x || y < frame.y || x >= frame.x + frame.w || y >= frame.y + frame.h
            throw Error(L("cube.invalidPoint"))
        nx := Round((x - frame.x) / frame.w, 6)
        ny := Round((y - frame.y) / frame.h, 6)
        color := Format("{:06X}", PixelGetColor(x, y))
        D4Box_CubeApplyCapture(ui, kind, nx, ny, color)
    } catch as err {
        MsgBox(err.Message, "d4box")
    } finally {
        CoordMode "Mouse", oldMouse
        CoordMode "Pixel", oldPixel
        D4Box_CubeCapturePending := false
        ui.captureTimer := 0
        if IsObject(D4Box_CubeUI) && D4Box_CubeUI = ui
            ui.gui.Show()
    }
}

D4Box_CubeApplyCapture(ui, kind, nx, ny, color) {
    if kind = "ready" {
        ui.profile.ready := D4Box_CubeStepText({label: L("cube.ready"), action: "check", quality: "any"
            , x: nx, y: ny, delay: 0, color: color, tolerance: 24, timeout: 3000})
        ui.guardText.Text := L("cube.readySet") " (" nx ", " ny ", #" color ")"
    } else {
        if kind = "probe" {
            ui.probeX.Value := nx
            ui.probeY.Value := ny
            ui.color.Value := color
        } else {
            ui.x.Value := nx
            ui.y.Value := ny
            if kind != "confirmClick"
                ui.color.Value := color
        }
        ui.stepDirty := true
    }
    ui.dirty := true
}

D4Box_CubeCloseEditor(*) {
    global D4Box_CubeUI, D4Box_CubeCapturePending, D4Box_DetectorUI
    if IsObject(D4Box_DetectorUI) {
        D4Box_DetectorClose()
        if IsObject(D4Box_DetectorUI)
            return true
    }
    ui := D4Box_CubeUI
    if ui.dirty && MsgBox(L("cube.discard"), "d4box", "YesNo") != "Yes"
        return true
    if ui.captureTimer
        SetTimer(ui.captureTimer, 0)
    D4Box_CubeCapturePending := false
    ToolTip()
    ui.gui.Destroy()
    D4Box_CubeUI := ""
    return true
}
