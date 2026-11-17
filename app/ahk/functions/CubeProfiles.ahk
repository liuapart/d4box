; Declarative cube workflows. No game input or GUI dependency in this module.
D4Box_CubeBuiltinsPath() {
    static path := RegExReplace(A_LineFile, "[^\\/]+$", "") "..\config\cube-workflows.ini"
    return path
}

D4Box_CubeUserPath() {
    return A_ScriptDir "\.runtime\cube-workflows.ini"
}

D4Box_CubeCopy(value) {
    if !IsObject(value)
        return value
    if (value is Array) {
        result := []
        for child in value
            result.Push(D4Box_CubeCopy(child))
        return result
    }
    result := {}
    for key, child in value.OwnProps()
        result.%key% := D4Box_CubeCopy(child)
    return result
}

D4Box_CubeStep(text) {
    fields := StrSplit(text, "|")
    if (fields.Length != 9 && fields.Length != 11 && fields.Length != 12)
        throw Error(L("cube.invalidStep"))
    return {label: fields[1], action: fields[2], quality: fields[3]
        , x: Number(fields[4]), y: Number(fields[5]), delay: Integer(fields[6])
        , color: fields[7], tolerance: Integer(fields[8]), timeout: Integer(fields[9])
        , probeX: Number(fields.Length >= 11 ? fields[10] : fields[4])
        , probeY: Number(fields.Length >= 11 ? fields[11] : fields[5])
        , detectorId: fields.Length = 12 ? fields[12] : ""}
}

D4Box_CubeStepText(step) {
    text := (step.label "|" step.action "|" step.quality "|" step.x "|" step.y "|"
        step.delay "|" step.color "|" step.tolerance "|" step.timeout)
    if D4Box_DetectorId(step) != ""
        return text "|0|0|" step.detectorId
    return step.action = "confirm_if_present" ? text "|" step.probeX "|" step.probeY : text
}

D4Box_CubeValidate(profile, forRun := false) {
    if !RegExMatch(profile.id, "^[a-zA-Z0-9_]+$") || Trim(profile.name) = ""
        throw Error(L("cube.invalidName"))
    if RegExMatch(profile.name, "[\r\n]")
        throw Error(L("cube.invalidName"))
    if !InStr("|inventory|manual|", "|" profile.source "|")
        throw Error(L("cube.invalidSource"))
    if !InStr("|red|yellow|blue|redyellow|any|", "|" profile.quality "|")
        throw Error(L("cube.invalidQuality"))
    if !InStr("|screen|client|", "|" profile.space "|")
        throw Error(L("cube.invalidSpace"))
    if profile.maxItems < 1 || profile.maxItems > 33 || profile.timeout < 1000 || profile.timeout > 1800000
        throw Error(L("cube.invalidLimits"))
    if profile.steps.Length = 0 || profile.steps.Length > 100
        throw Error(L("cube.invalidStepCount"))
    detectors := false
    for step in profile.steps {
        if Trim(step.label) = "" || RegExMatch(step.label, "[|\r\n]")
            throw Error(L("cube.invalidStep"))
        if !InStr("|select|execute|confirm|clear|wait|check|rightclick|confirm_if_present|", "|" step.action "|")
            throw Error(L("cube.invalidStep"))
        if !InStr("|any|red|yellow|blue|", "|" step.quality "|")
            throw Error(L("cube.invalidQuality"))
        if step.action != "rightclick" && (step.x < 0 || step.x > 1 || step.y < 0 || step.y > 1)
            throw Error(L("cube.invalidPoint"))
        detectorId := D4Box_DetectorId(step)
        if detectorId != "" && (!RegExMatch(detectorId, "^region_[a-zA-Z0-9_]+$")
            || (step.action != "check" && step.action != "confirm_if_present"))
            throw Error(L("detect.invalid"))
        if forRun && detectorId != "" {
            if !IsObject(detectors)
                detectors := D4Box_DetectorRead()
            D4Box_DetectorRequire(detectorId, detectors)
        }
        if step.action = "confirm_if_present" && detectorId = "" && (!step.HasProp("probeX") || !step.HasProp("probeY")
            || step.probeX < 0 || step.probeX > 1 || step.probeY < 0 || step.probeY > 1)
            throw Error(L("cube.invalidPoint"))
        if step.delay < 0 || step.delay > 10000 || step.timeout < 100 || step.timeout > 30000
            throw Error(L("cube.invalidLimits"))
        if step.tolerance < 0 || step.tolerance > 255
            throw Error(L("cube.invalidColor"))
        if detectorId = "" && (step.action = "check" || step.action = "confirm_if_present") && !RegExMatch(step.color, "i)^[0-9a-f]{6}$")
            throw Error(L("cube.invalidColor"))
    }
    if profile.ready != "" {
        guard := D4Box_CubeStep(profile.ready)
        if guard.action != "check" || guard.quality != "any"
            throw Error(L("cube.invalidColor"))
        ; Validate the guard by the same rules as an ordinary check step.
        guardProfile := D4Box_CubeCopy(profile)
        guardProfile.ready := ""
        guardProfile.steps := [guard]
        D4Box_CubeValidate(guardProfile)
        if forRun && D4Box_DetectorId(guard) != "" {
            if !IsObject(detectors)
                detectors := D4Box_DetectorRead()
            D4Box_DetectorRequire(guard.detectorId, detectors)
        }
    }
    if forRun {
        if profile.source = "manual" {
            for step in profile.steps
                if step.quality != "any"
                    throw Error(L("cube.manualConditions"))
        }
        issues := !profile.builtin && profile.ready = "" ? L("cube.missingReady") : ""
        qualities := D4Box_CubeQualities(profile)
        for quality in qualities {
            plan := D4Box_CubePlan(profile, quality)
            hasExecute := false
            for step in plan
                if step.action = "execute"
                    hasExecute := true
            if !hasExecute
                throw Error(L("cube.emptyRoute", quality))
            ; Imported built-in keeps its legacy behavior. New workflows must verify UI state.
            if !profile.builtin && plan[plan.Length].action != "check"
                issues .= (issues = "" ? "" : "`n") L("cube.missingFinal", L("cube." quality))
        }
        if issues != ""
            throw Error(issues)
    }
}

D4Box_CubeRunIssue(profile) {
    try D4Box_CubeValidate(profile, true)
    catch as err
        return err.Message
    return ""
}

D4Box_CubeQualities(profile) {
    if profile.source = "manual"
        return ["any"]
    switch profile.quality {
        case "redyellow": return ["red", "yellow"]
        case "any": return ["red", "yellow", "blue"]
        default: return [profile.quality]
    }
}

D4Box_CubePlan(profile, quality) {
    result := []
    for step in profile.steps {
        ; Conditions refer to the originally scanned quality, not the upgraded item's color.
        if step.quality = "any" || step.quality = quality
            result.Push(D4Box_CubeCopy(step))
    }
    return result
}

; Existing inventory profiles still auto-load; explicit selection controls its own position.
D4Box_CubeItemPlan(profile, quality) {
    plan := D4Box_CubePlan(profile, quality)
    if profile.source = "inventory" {
        for step in plan
            if step.action = "rightclick"
                return plan
        plan.InsertAt(1, D4Box_CubeStep(L("cube.rightclick") "|rightclick|any|0|0|250|-|24|3000"))
    }
    return plan
}

D4Box_CubeReadFile(file, builtin := false) {
    profiles := Map()
    if !FileExist(file)
        return profiles
    text := FileRead(file)
    sections := Map()
    for line in StrSplit(text, "`n", "`r") {
        if RegExMatch(Trim(line), "^\[Workflow\.([a-zA-Z0-9_]+)\]$", &match)
            sections[match[1]] := "Workflow." match[1]
    }
    for id, section in sections {
        get := ((iniSection, key, fallback := "") => IniRead(file, iniSection, key, fallback)).Bind(section)
        profile := {id: id, name: builtin ? L(get("NameKey")) : get("Name"), nameEn: get("NameEn"), builtin: builtin
            , source: get("Source", "inventory"), quality: get("Quality", "red")
            , space: get("Space", "client"), maxItems: Integer(get("MaxItems", "33"))
            , timeout: Integer(get("TimeoutMs", "300000")), legacyYellow: builtin && get("LegacyYellow", "0") = "1"
            , ready: get("Ready"), steps: []}
        count := Integer(get("StepCount", "0"))
        if count < 1 || count > 100
            throw Error(L("cube.invalidStepCount"))
        Loop count {
            step := D4Box_CubeStep(get("Step" A_Index))
            if builtin
                step.label := L("cube.step." step.label)
            profile.steps.Push(step)
        }
        D4Box_CubeValidate(profile)
        profiles[profile.id] := profile
    }
    return profiles
}

D4Box_CubeProfiles() {
    profiles := D4Box_CubeReadFile(D4Box_CubeBuiltinsPath(), true)
    for id, profile in D4Box_CubeReadFile(D4Box_CubeUserPath()) {
        if profiles.Has(id)
            throw Error(L("cube.reservedId"))
        profiles[id] := profile
    }
    return profiles
}

D4Box_CubeSelectedId() {
    return IniRead(D4Box_CubeUserPath(), "Selection", "Id", "inventory_transmute")
}

D4Box_CubeProfileName(profile) {
    global appLang
    return profile.builtin && appLang = "en" && profile.nameEn != "" ? profile.nameEn : profile.name
}

D4Box_CubeSave(profile, selectedId := "", removeId := "") {
    if IsObject(profile) {
        if profile.builtin
            throw Error(L("cube.copyFirst"))
        D4Box_CubeValidate(profile)
    }
    file := D4Box_CubeUserPath()
    SplitPath(file, , &directory)
    DirCreate(directory)
    temp := file ".tmp"
    if FileExist(temp)
        FileDelete(temp)
    try {
        if FileExist(file)
            FileCopy(file, temp)
        if removeId != "" {
            if !RegExMatch(removeId, "^user_[a-zA-Z0-9_]+$")
                throw Error(L("cube.copyFirst"))
            try IniDelete(temp, "Workflow." removeId)
        }
        if IsObject(profile) {
            if !RegExMatch(profile.id, "^user_[a-zA-Z0-9_]+$")
                throw Error(L("cube.reservedId"))
            section := "Workflow." profile.id
            ; Replace the whole section so deleted steps cannot survive a save.
            try IniDelete(temp, section)
            fields := Map("Name", profile.name, "Source", profile.source, "Quality", profile.quality
                , "Space", profile.space, "MaxItems", profile.maxItems, "TimeoutMs", profile.timeout
                , "Ready", profile.ready, "StepCount", profile.steps.Length)
            for key, value in fields
                IniWrite(value, temp, section, key)
            for index, step in profile.steps
                IniWrite(D4Box_CubeStepText(step), temp, section, "Step" index)
        }
        if selectedId != ""
            IniWrite(selectedId, temp, "Selection", "Id")
        FileMove(temp, file, 1)
    } finally {
        if FileExist(temp)
            FileDelete(temp)
    }
}
