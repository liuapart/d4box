global D4Box_BindingsUI := ""
global D4Box_BindingEditor := ""

D4Box_CreateBindingsGUI() {
    global myGui, D4Box_BindingsUI, MUI_T
    myGui.SetFont("s11 bold")
    myGui.AddText("x32 y54 w640 h24", L("binding.title"))
    myGui.SetFont("s9 norm")
    myGui.AddText("x32 y84 w648 h32", L("binding.subtitle"))
    list := myGui.AddListView("x32 y122 w656 h164 -Multi +LV0x10000", [L("binding.key"), L("binding.workflow"), L("binding.state")])
    for index, width in [130, 340, 160]
        list.ModifyCol(index, width)
    list.OnEvent("DoubleClick", (*) => D4Box_BindingEdit(false))
    list.OnEvent("ItemSelect", D4Box_BindingsSelectionChanged)
    myGui.AddButton("x32 y298 w120 h30", L("binding.add")).OnEvent("Click", (*) => D4Box_BindingEdit(true))
    edit := myGui.AddButton("x160 y298 w96 h30", L("binding.edit"))
    edit.OnEvent("Click", (*) => D4Box_BindingEdit(false))
    remove := myGui.AddButton("x264 y298 w96 h30", L("binding.delete"))
    remove.OnEvent("Click", D4Box_BindingDelete)
    myGui.AddButton("x410 y298 w132 h30", L("binding.manage")).OnEvent("Click", D4Box_OpenCubeEditor)
    myGui.AddButton("x550 y298 w138 h30", L("detect.title")).OnEvent("Click", D4Box_OpenDetectorLibrary)
    note := myGui.AddText("x32 y342 w656 h42", L("binding.hint"))
    D4Box_BindingsUI := {list: list, ids: [], note: note, edit: edit, remove: remove}
}

D4Box_BindingsRefresh(selectedId := "default") {
    global D4Box_WorkflowBindings, D4Box_BindingsUI, D4Box_BindingsError
    if !IsObject(D4Box_BindingsUI)
        return
    ui := D4Box_BindingsUI
    ui.list.Delete()
    ui.ids := []
    try profiles := D4Box_CubeProfiles()
    catch {
        profiles := Map()
    }
    for row in D4Box_WorkflowBindings {
        ui.ids.Push(row.id)
        name := profiles.Has(row.profileId) ? D4Box_CubeProfileName(profiles[row.profileId]) : L("binding.missing", row.profileId)
        state := row.id = "default" ? L("binding.default") : L("binding.custom")
        if !profiles.Has(row.profileId) || D4Box_BindingsError != ""
            state := L("binding.unavailable")
        ui.list.Add(row.id = selectedId ? "Select Focus" : "", GetStartStopHotkeyDisplay(row.key), name, state)
    }
    ui.note.Text := D4Box_BindingsError != "" ? L("binding.loadError", D4Box_BindingsError) : L("binding.hint")
    D4Box_BindingsSelectionChanged()
}

D4Box_BindingsSelectionChanged(*) {
    global D4Box_BindingsUI
    if !IsObject(D4Box_BindingsUI)
        return
    ui := D4Box_BindingsUI
    index := ui.list.GetNext()
    ui.edit.Enabled := index > 0
    ui.remove.Enabled := index > 0 && ui.ids[index] != "default"
}

D4Box_BindingEdit(create := false, *) {
    global D4Box_BindingEditor, D4Box_BindingsUI, D4Box_WorkflowBindings, myGui
    try {
        D4Box_BindingsEditable()
        if IsObject(D4Box_BindingEditor) {
            D4Box_BindingEditor.gui.Show()
            return
        }
        index := D4Box_BindingsUI.list.GetNext()
        if !create && !index
            return
        row := create ? {id: "key_" A_Now "_" Random(1000, 9999), key: "", profileId: ""} : D4Box_WorkflowBindings[index]
        editor := Gui("+Owner" myGui.Hwnd, create ? L("binding.add") : L("binding.edit"))
        editor.SetFont("s10", "Segoe UI")
        editor.AddText("x20 y20 w400 h24", row.id = "default" ? L("binding.defaultHint") : L("binding.keyHint"))
        editor.AddText("x20 y62 w100", L("binding.key"))
        key := editor.AddHotkey("x130 y58 w280", row.key)
        editor.AddText("x20 y104 w100", L("binding.workflow"))
        profiles := D4Box_CubeProfiles(), ids := [], names := [], selected := 0
        for id, profile in profiles {
            ids.Push(id), names.Push(D4Box_CubeProfileName(profile))
            if id = row.profileId
                selected := ids.Length
        }
        flow := editor.AddDropDownList("x130 y100 w280", names)
        flow.Choose(selected || create ? Max(1, selected) : 0)
        errorText := editor.AddText("x20 y142 w390 h58 cB91C1C", "")
        editor.AddButton("x190 y210 w105 h32 Default", L("binding.save")).OnEvent("Click", D4Box_BindingSave)
        editor.AddButton("x305 y210 w105 h32", L("binding.cancel")).OnEvent("Click", D4Box_BindingClose)
        D4Box_BindingEditor := {gui: editor, id: row.id, key: key, flow: flow, ids: ids, errorText: errorText}
        editor.OnEvent("Close", D4Box_BindingClose)
        editor.OnEvent("Escape", D4Box_BindingClose)
        editor.Show("w430 h260")
        key.Focus()
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_BindingSave(*) {
    global D4Box_BindingEditor, D4Box_WorkflowBindings
    ui := D4Box_BindingEditor
    try {
        D4Box_BindingsEditable()
        if !ui.flow.Value
            throw Error(L("binding.chooseWorkflow"))
        row := {id: ui.id, key: D4Box_BindingKey(ui.key.Value), profileId: ui.ids[ui.flow.Value]}
        rows := D4Box_CubeCopy(D4Box_WorkflowBindings), found := false
        for index, old in rows
            if old.id = row.id {
                rows[index] := row
                found := true
                break
            }
        if !found
            rows.Push(row)
        D4Box_BindingsApply(rows)
        D4Box_BindingClose()
        D4Box_BindingsRefresh(row.id)
    } catch as err {
        ui.errorText.Text := err.Message
    }
}

D4Box_BindingDelete(*) {
    global D4Box_WorkflowBindings, D4Box_BindingsUI
    try {
        D4Box_BindingsEditable()
        index := D4Box_BindingsUI.list.GetNext()
        if !index
            return
        D4Box_BindingsApply(D4Box_BindingRemove(D4Box_WorkflowBindings, D4Box_BindingsUI.ids[index]))
        D4Box_BindingsRefresh()
    } catch as err {
        MsgBox(err.Message, "d4box")
    }
}

D4Box_BindingClose(*) {
    global D4Box_BindingEditor
    if IsObject(D4Box_BindingEditor)
        D4Box_BindingEditor.gui.Destroy()
    D4Box_BindingEditor := ""
}
