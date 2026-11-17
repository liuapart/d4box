; Named visual detectors. No game input: storage, image capture, matching and test evidence.
D4Box_DetectorRoot() {
    return A_ScriptDir "\.runtime\detection-regions"
}

D4Box_DetectorNew() {
    return {id: "region_" A_Now "_" Random(10000, 99999), name: L("detect.new")
        , image: "", x: 0, y: 0, w: 0, h: 0, frameW: 0, frameH: 0, dpi: 96
        , variation: 20, margin: 12, present: "", absent: ""}
}

D4Box_DetectorSignature(node) {
    return (node.image ":" node.x ":" node.y ":" node.w ":" node.h ":" node.frameW ":" node.frameH
        ":" node.dpi ":" node.variation ":" node.margin)
}

D4Box_DetectorPassed(node) {
    signature := D4Box_DetectorSignature(node)
    return node.image != "" && node.present = signature && node.absent = signature
}

D4Box_DetectorInvalidate(node) {
    node.present := ""
    node.absent := ""
}

D4Box_DetectorRecordTest(node, expected, samples) {
    passed := samples.Length >= 3
    for matched in samples
        if !!matched != !!expected
            passed := false
    key := expected ? "present" : "absent"
    node.%key% := passed ? D4Box_DetectorSignature(node) : ""
    return passed
}

D4Box_DetectorValidate(node, root := "", requirePassed := false) {
    if !RegExMatch(node.id, "^region_[a-zA-Z0-9_]+$") || Trim(node.name) = ""
        || StrLen(node.name) > 80 || RegExMatch(node.name, "[\r\n|]")
        throw Error(L("detect.invalid"))
    if node.variation < 0 || node.variation > 100 || node.margin < 0 || node.margin > 80
        throw Error(L("detect.limits"))
    if node.image != "" {
        if !RegExMatch(node.image, "^" node.id "_[a-zA-Z0-9_]+\.bmp$")
            throw Error(L("detect.invalid"))
        if node.x < 0 || node.y < 0 || node.w < 8 || node.h < 8 || node.w > 512 || node.h > 256
            || node.x + node.w > node.frameW || node.y + node.h > node.frameH || node.dpi < 48 || node.dpi > 768
            throw Error(L("detect.bounds"))
    }
    if requirePassed {
        if !D4Box_DetectorPassed(node)
            throw Error(L("detect.untested", node.name))
        if root = ""
            root := D4Box_DetectorRoot()
        if !FileExist(root "\" node.image)
            throw Error(L("detect.missingImage", node.name))
    }
}

D4Box_DetectorRead(root := "") {
    if root = ""
        root := D4Box_DetectorRoot()
    file := root "\regions.ini"
    nodes := Map()
    if !FileExist(file)
        return nodes
    for section in StrSplit(IniRead(file), "`n", "`r") {
        if !RegExMatch(section, "^Detector\.(region_[a-zA-Z0-9_]+)$", &m)
            continue
        node := D4Box_DetectorNew()
        node.id := m[1]
        for key in ["name", "image", "present", "absent"]
            node.%key% := IniRead(file, section, key, "")
        for key in ["x", "y", "w", "h", "frameW", "frameH", "dpi", "variation", "margin"]
            node.%key% := Integer(IniRead(file, section, key, "0"))
        D4Box_DetectorValidate(node)
        nodes[node.id] := node
    }
    return nodes
}

D4Box_DetectorSave(node, root := "") {
    if root = ""
        root := D4Box_DetectorRoot()
    D4Box_DetectorValidate(node)
    nodes := D4Box_DetectorRead(root)
    for id, other in nodes
        if id != node.id && other.name = node.name
            throw Error(L("detect.duplicate"))
    if node.image != "" && !FileExist(root "\" node.image)
        throw Error(L("detect.missingImage", node.name))
    if !D4Box_DetectorPassed(node) {
        signature := D4Box_DetectorSignature(node)
        if node.present != signature
            node.present := ""
        if node.absent != signature
            node.absent := ""
    }
    DirCreate(root)
    file := root "\regions.ini"
    temp := file ".tmp"
    try {
        if FileExist(temp)
            FileDelete(temp)
        if FileExist(file)
            FileCopy(file, temp)
        for key in ["name", "image", "present", "absent", "x", "y", "w", "h", "frameW", "frameH", "dpi", "variation", "margin"]
            IniWrite(node.%key%, temp, "Detector." node.id, key)
        FileMove(temp, file, 1)
    } finally {
        if FileExist(temp)
            FileDelete(temp)
    }
}

D4Box_DetectorId(step) {
    return step.HasProp("detectorId") ? step.detectorId : ""
}

D4Box_DetectorRequire(id, nodes, root := "") {
    if !nodes.Has(id)
        throw Error(L("detect.missing", id))
    node := nodes[id]
    D4Box_DetectorValidate(node, root, true)
    return node
}

D4Box_DetectorReferences(id, profiles) {
    names := []
    for , profile in profiles {
        referenced := false
        for step in profile.steps
            if D4Box_DetectorId(step) = id
                referenced := true
        if profile.ready != "" && D4Box_DetectorId(D4Box_CubeStep(profile.ready)) = id
            referenced := true
        if referenced
            names.Push(profile.name)
    }
    return names
}

D4Box_DetectorDelete(id, profiles, root := "") {
    if D4Box_DetectorReferences(id, profiles).Length
        throw Error(L("detect.inUse"))
    if !RegExMatch(id, "^region_[a-zA-Z0-9_]+$")
        throw Error(L("detect.invalid"))
    if root = ""
        root := D4Box_DetectorRoot()
    file := root "\regions.ini"
    temp := file ".tmp"
    if !FileExist(file)
        return
    try {
        FileCopy(file, temp, 1)
        IniDelete(temp, "Detector." id)
        FileMove(temp, file, 1)
    } finally {
        if FileExist(temp)
            FileDelete(temp)
    }
    ; Immutable images are retained; running snapshots never lose their template.
}

D4Box_DetectorCheckFrame(node, frame) {
    if node.frameW != frame.w || node.frameH != frame.h || node.dpi != frame.dpi
        throw Error(L("detect.frame", node.name))
}

D4Box_DetectorMatch(node, frame, root := "") {
    D4Box_DetectorCheckFrame(node, frame)
    if root = ""
        root := D4Box_DetectorRoot()
    path := root "\" node.image
    if !FileExist(path)
        throw Error(L("detect.missingImage", node.name))
    oldMode := A_CoordModePixel
    try {
        CoordMode "Pixel", "Screen"
        x1 := frame.x + Max(0, node.x - node.margin)
        y1 := frame.y + Max(0, node.y - node.margin)
        x2 := frame.x + Min(frame.w - 1, node.x + node.w - 1 + node.margin)
        y2 := frame.y + Min(frame.h - 1, node.y + node.h - 1 + node.margin)
        return ImageSearch(&foundX, &foundY, x1, y1, x2, y2, "*" node.variation " " path)
    } finally {
        CoordMode "Pixel", oldMode
    }
}

; Save a standard bottom-up 24-bit BMP crop from the top-down capture buffer.
D4Box_DetectorWriteBitmap(capture, x, y, w, h, path) {
    if !capture.ok || x < 0 || y < 0 || w < 1 || h < 1
        || x + w > capture.width || y + h > capture.height
        throw Error(L("detect.bounds"))
    stride := ((w * 3 + 3) // 4) * 4
    header := Buffer(54, 0)
    NumPut("UShort", 0x4D42, header, 0)
    NumPut("UInt", 54 + stride * h, header, 2)
    NumPut("UInt", 54, header, 10)
    NumPut("UInt", 40, header, 14)
    NumPut("Int", w, header, 18)
    NumPut("Int", h, header, 22)
    NumPut("UShort", 1, header, 26)
    NumPut("UShort", 24, header, 28)
    pixels := Buffer(stride * h, 0)
    Loop h
        DllCall("RtlMoveMemory", "Ptr", pixels.Ptr + (h - A_Index) * stride
            , "Ptr", capture.bits.Ptr + (y + A_Index - 1) * capture.stride + x * 3, "UPtr", w * 3)
    file := FileOpen(path, "w")
    if !file
        throw Error(L("detect.writeFailed"))
    try {
        if file.RawWrite(header) != header.Size || file.RawWrite(pixels) != pixels.Size
            throw Error(L("detect.writeFailed"))
    } finally {
        file.Close()
    }
}

D4Box_DetectorBind(profile, frame) {
    nodes := false
    steps := profile.steps.Clone()
    guard := profile.ready != "" ? D4Box_CubeStep(profile.ready) : false
    if IsObject(guard)
        steps.Push(guard)
    for step in steps {
        id := D4Box_DetectorId(step)
        if id = ""
            continue
        if !IsObject(nodes)
            nodes := D4Box_DetectorRead()
        node := D4Box_DetectorRequire(id, nodes)
        D4Box_DetectorCheckFrame(node, frame)
        step.detector := D4Box_CubeCopy(node)
    }
    return guard
}
