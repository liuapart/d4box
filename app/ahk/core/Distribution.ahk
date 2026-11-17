D4Box_PrepareDistribution() {
    if !A_IsCompiled
        return
    directory := A_ScriptDir "\.runtime\licenses"
    DirCreate(directory)
    ; Sources are placed here by build-windows.ps1 before compilation.
    FileInstall "licenses\D4KeyHelp-MIT.txt", directory "\D4KeyHelp-MIT.txt", 1
    FileInstall "licenses\AutoHotkey-license.txt", directory "\AutoHotkey-license.txt", 1
    FileInstall "licenses\THIRD_PARTY_NOTICES.md", directory "\THIRD_PARTY_NOTICES.md", 1
}

; A compiled-package probe: no game input, hotkey registration or user settings writes.
D4Box_DistributionSelfTest() {
    global myGui
    result := A_ScriptDir "\.runtime\self-test.txt"
    try {
        D4Box_PrepareDistribution()
        for name in ["D4KeyHelp-MIT.txt", "AutoHotkey-license.txt", "THIRD_PARTY_NOTICES.md"]
            if FileGetSize(A_ScriptDir "\.runtime\licenses\" name) < 100
                throw Error("Embedded license missing: " name)
        profiles := D4Box_CubeReadFile(D4Box_CubeBuiltinsPath(), true)
        if !profiles.Has("inventory_transmute") || profiles["inventory_transmute"].steps.Length = 0
            throw Error("Embedded built-in workflow missing")
        CreateMainGUI()
        D4Box_OpenCubeEditor()
        D4Box_CubeCloseEditor()
        D4Box_ShowUpdates(false)
        D4Box_CloseUpdates()
        myGui.Destroy()
        FileAppend("PASS " D4Box_Version() " compiled=" A_IsCompiled "`n", result, "UTF-8")
        ExitApp(0)
    } catch as err {
        DirCreate(A_ScriptDir "\.runtime")
        FileAppend("FAIL " err.Message " @" err.File ":" err.Line "`n", result, "UTF-8")
        ExitApp(1)
    }
}
