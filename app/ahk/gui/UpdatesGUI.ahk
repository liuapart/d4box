global D4Box_UpdateUI := false

D4Box_OpenUpdates(*) {
    D4Box_ShowUpdates(true)
}

D4Box_ShowUpdates(checkImmediately := true) {
    global D4Box_UpdateUI, myGui, MUI_T, MUI_FontName
    if IsObject(D4Box_UpdateUI) {
        D4Box_UpdateUI.gui.Show()
        return
    }
    ui := {job: false, url: ""}
    ui.gui := Gui("+Owner" myGui.Hwnd, L("update.title"))
    ui.gui.BackColor := MUI_Hex(MUI_T.window)
    ui.gui.SetFont("s10 c" MUI_Hex(MUI_T.text), MUI_FontName)
    ui.gui.AddText("x20 y18 w480 h26", L("update.current", D4Box_Version()))
    ui.status := ui.gui.AddText("x20 y54 w480 h72", L("update.hint"))
    ui.check := ui.gui.AddButton("x20 y140 w150 h32", L("update.check"))
    ui.open := ui.gui.AddButton("x184 y140 w180 h32 Disabled", L("update.open"))
    ui.gui.AddButton("x380 y140 w120 h32", L("cube.close")).OnEvent("Click", D4Box_CloseUpdates)
    ui.check.OnEvent("Click", D4Box_CheckUpdates)
    ui.open.OnEvent("Click", D4Box_OpenRelease)
    ui.gui.OnEvent("Close", D4Box_CloseUpdates)
    ui.gui.OnEvent("Escape", D4Box_CloseUpdates)
    MUI_ApplyNativeTheme(ui.gui)
    D4Box_UpdateUI := ui
    ui.gui.Show("w520 h192")
    if checkImmediately
        D4Box_CheckUpdates()
}

D4Box_CheckUpdates(*) {
    global D4Box_UpdateUI
    if !IsObject(D4Box_UpdateUI) || IsObject(D4Box_UpdateUI.job)
        return
    ui := D4Box_UpdateUI
    ui.status.Text := L("update.checking")
    ui.check.Enabled := false, ui.open.Enabled := false, ui.url := ""
    ui.job := D4Box_UpdateCheck(D4Box_UpdateFinished.Bind(ui))
    ui.job.Start()
}

D4Box_UpdateFinished(ui, release, errorText) {
    global D4Box_UpdateUI
    if !IsObject(D4Box_UpdateUI) || D4Box_UpdateUI != ui
        return
    ui.job := false
    ui.check.Enabled := true
    ui.open.Enabled := false, ui.url := ""
    if errorText != ""
        ui.status.Text := L("update.failed", errorText)
    else if !IsObject(release)
        ui.status.Text := L("update.none")
    else if D4Box_CompareVersion(release.version, D4Box_Version()) > 0 {
        ui.status.Text := L("update.available", release.version)
        ui.url := release.url
        ui.open.Enabled := true
    } else
        ui.status.Text := L("update.latest")
}

D4Box_OpenRelease(*) {
    global D4Box_UpdateUI
    if IsObject(D4Box_UpdateUI) && D4Box_UpdateUI.url != "" {
        try Run(D4Box_UpdateUI.url)
        catch as err
            D4Box_UpdateUI.status.Text := L("update.failed", err.Message)
    }
}

D4Box_CloseUpdates(*) {
    global D4Box_UpdateUI
    if !IsObject(D4Box_UpdateUI)
        return
    if IsObject(D4Box_UpdateUI.job)
        D4Box_UpdateUI.job.Cancel()
    D4Box_UpdateUI.gui.Destroy()
    D4Box_UpdateUI := false
}
