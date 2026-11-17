; Global workflow bindings, independent of combat presets. Runtime files never enter Git.
global D4Box_WorkflowBindings := []
global D4Box_RegisteredBindings := []
global D4Box_BindingHeld := Map()
global D4Box_BindingsError := ""

D4Box_BindingsPath() => A_ScriptDir "\.runtime\workflow-hotkeys.ini"

D4Box_BindingsRead(path := "", selected := "") {
    if path = ""
        path := D4Box_BindingsPath()
    if selected = ""
        selected := D4Box_CubeSelectedId()
    if !FileExist(path)
        return [{id: "default", key: "F3", profileId: selected}]
    rows := []
    count := Integer(IniRead(path, "Bindings", "Count", 0))
    if count < 1 || count > 32
        throw Error(L("binding.invalidFile"))
    Loop count {
        section := "Binding" A_Index
        rows.Push({id: IniRead(path, section, "Id"), key: IniRead(path, section, "Key")
            , profileId: IniRead(path, section, "Workflow")})
    }
    return rows
}

D4Box_BindingKey(value) {
    if !RegExMatch(Trim(value), "^([\^!+#]*)([a-zA-Z0-9]+)$", &parts)
        throw Error(L("binding.invalidKey"))
    modifiers := ""
    for char in ["^", "!", "+", "#"]
        if InStr(parts[1], char)
            modifiers .= char
    key := GetKeyName(parts[2])
    if key = "" || InStr(modifiers, "#") || RegExMatch(key, "i)^(.*Button\d*|Wheel.*|Tab|.*Control|.*Ctrl|.*Shift|.*Alt|.*Win)$")
        throw Error(L("binding.invalidKey"))
    if key = "F8" && modifiers = "^!"
        throw Error(L("binding.reserved"))
    return modifiers key
}

D4Box_BindingBase(key) => StrLower(GetKeyName(RegExReplace(key, "^[*\^!+#$~]+", "")))

D4Box_BindingsValidate(rows, profiles, macroKey := "F1") {
    if rows.Length < 1 || rows.Length > 32
        throw Error(L("binding.limit"))
    ids := Map(), keys := Map(), defaults := 0
    for row in rows {
        if !RegExMatch(row.id, "^[a-zA-Z0-9_]+$") || ids.Has(row.id)
            throw Error(L("binding.invalidFile"))
        ids[row.id] := true
        defaults += row.id = "default" ? 1 : 0
        key := D4Box_BindingKey(row.key)
        ; Compare virtual keys as well as spelling: Esc/Escape are the same key.
        identity := RegExReplace(key, "[^\^!+#]", "") ":" GetKeyVK(RegExReplace(key, "^[\^!+#]+", "")) ":" GetKeySC(RegExReplace(key, "^[\^!+#]+", ""))
        if keys.Has(identity)
            throw Error(L("binding.duplicate", GetStartStopHotkeyDisplay(key)))
        keys[identity] := true
        ; Macro uses a wildcard, so any modifiers on the same base key conflict.
        if D4Box_BindingBase(key) = D4Box_BindingBase(macroKey)
            throw Error(L("binding.macroConflict", GetStartStopHotkeyDisplay(macroKey)))
        if !profiles.Has(row.profileId)
            throw Error(L("binding.missing", row.profileId))
    }
    if defaults != 1
        throw Error(L("binding.defaultRequired"))
}

D4Box_BindingsWrite(rows, path := "") {
    if path = ""
        path := D4Box_BindingsPath()
    if DirExist(path)
        throw Error(L("binding.invalidFile"))
    SplitPath(path, , &dir)
    DirCreate(dir)
    tmp := path ".tmp"
    try {
        if FileExist(tmp)
            FileDelete(tmp)
        IniWrite(rows.Length, tmp, "Bindings", "Count")
        for index, row in rows {
            section := "Binding" index
            IniWrite(row.id, tmp, section, "Id")
            IniWrite(row.key, tmp, section, "Key")
            IniWrite(row.profileId, tmp, section, "Workflow")
        }
        FileMove(tmp, path, 1)
    } finally {
        if FileExist(tmp)
            FileDelete(tmp)
    }
}

D4Box_BindingsContext(*) {
    global D4Box_CubeBusy
    return D4Box_CubeBusy || WinActive("ahk_class Diablo IV Main Window Class")
}

D4Box_BindingsUnregister() {
    global D4Box_RegisteredBindings
    HotIf(D4Box_BindingsContext)
    try {
        for row in D4Box_RegisteredBindings
            Hotkey("$" row.key, "Off")
        D4Box_RegisteredBindings := []
    } finally {
        HotIf()
    }
}

D4Box_BindingsRegister(rows) {
    global D4Box_RegisteredBindings
    HotIf(D4Box_BindingsContext)
    try {
        for row in rows {
            Hotkey("$" row.key, D4Box_RunBoundWorkflow.Bind(row.id), "On T2 I0")
            D4Box_RegisteredBindings.Push(D4Box_CubeCopy(row))
        }
    } finally {
        HotIf()
    }
}

D4Box_BindingsEditable() {
    global isRunning, D4Box_CubeBusy, D4Box_CubeCapturePending
    if isRunning || D4Box_CubeBusy || D4Box_CubeCapturePending
        throw Error(L("binding.stopFirst"))
}

; Register and save as one transaction. A write/registration error restores old keys.
D4Box_BindingsApply(rows, persist := true, path := "") {
    global D4Box_WorkflowBindings, D4Box_RegisteredBindings, D4Box_BindingsError, startStopHotkey
    D4Box_BindingsEditable()
    D4Box_BindingsValidate(rows, D4Box_CubeProfiles(), startStopHotkey)
    next := D4Box_CubeCopy(rows)
    for row in next
        row.key := D4Box_BindingKey(row.key)
    old := D4Box_CubeCopy(D4Box_RegisteredBindings)
    Critical "On"
    try {
        D4Box_BindingsUnregister()
        D4Box_BindingsRegister(next)
        if persist
            D4Box_BindingsWrite(next, path)
        D4Box_WorkflowBindings := next
        D4Box_BindingsError := ""
    } catch as err {
        D4Box_BindingsUnregister()
        D4Box_BindingsRegister(old)
        throw err
    } finally {
        Critical "Off"
    }
}

D4Box_BindingsInit() {
    global D4Box_WorkflowBindings, D4Box_BindingsError
    try {
        D4Box_WorkflowBindings := D4Box_BindingsRead()
        D4Box_BindingsApply(D4Box_WorkflowBindings, false)
    } catch as err {
        ; Never substitute a different workflow or overwrite a damaged file at startup.
        D4Box_BindingsError := err.Message
        DebugLog("Workflow hotkeys disabled: " err.Message)
    }
    D4Box_BindingsRefresh()
}

D4Box_DefaultBindingProfile() {
    global D4Box_WorkflowBindings
    for row in D4Box_WorkflowBindings
        if row.id = "default"
            return row.profileId
    return D4Box_CubeSelectedId()
}

D4Box_BindingsSelectDefault(profileId) {
    global D4Box_WorkflowBindings
    rows := D4Box_CubeCopy(D4Box_WorkflowBindings)
    for row in rows
        if row.id = "default" {
            row.profileId := profileId
            D4Box_BindingsApply(rows)
            D4Box_BindingsRefresh()
            return
        }
    throw Error(L("binding.defaultRequired"))
}

D4Box_BindingReferences(profileId) {
    global D4Box_WorkflowBindings
    keys := ""
    for row in D4Box_WorkflowBindings
        if row.profileId = profileId
            keys .= (keys = "" ? "" : ", ") GetStartStopHotkeyDisplay(row.key)
    return keys
}

D4Box_BindingRemove(rows, id) {
    if id = "default"
        throw Error(L("binding.defaultRequired"))
    result := []
    for row in rows
        if row.id != id
            result.Push(D4Box_CubeCopy(row))
    return result
}

; Physical key state prevents autorepeat from restarting/cancelling a workflow.
D4Box_BindingReleasePoll() {
    global D4Box_BindingHeld
    released := []
    for key in D4Box_BindingHeld
        if !GetKeyState(key, "P")
            released.Push(key)
    for key in released
        D4Box_BindingHeld.Delete(key)
    if !D4Box_BindingHeld.Count
        SetTimer(D4Box_BindingReleasePoll, 0)
}

D4Box_RunBoundWorkflow(id, *) {
    global D4Box_WorkflowBindings, D4Box_BindingHeld, D4Box_CubeBusy, D4Box_CubeCapturePending
    for row in D4Box_WorkflowBindings {
        if row.id != id
            continue
        base := D4Box_BindingBase(row.key)
        if D4Box_BindingHeld.Has(base)
            return
        D4Box_BindingHeld[base] := true
        SetTimer(D4Box_BindingReleasePoll, 30)
        D4Box_DispatchWorkflowBinding(row)
        return
    }
}

D4Box_DispatchWorkflowBinding(row, run := AutoTransmute) {
    global D4Box_CubeBusy, D4Box_CubeCapturePending
    if D4Box_CubeBusy {
        D4Box_CancelCube()
        return
    }
    if !D4Box_CubeCapturePending
        run.Call(row.profileId)
}
