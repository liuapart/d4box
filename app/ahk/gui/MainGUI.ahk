; ========== GUI创建（现代卡片风格） ==========
/**
 * 创建主GUI界面
 */
CreateMainGUI() {
    global myGui, statusText, statusBar, presetTab, presetNames, currentPreset, D4Box_MainTabs
    global startStopHotkeyCtrl, startStopMouseCtrl, startStopButton
    global D4Box_RuntimeDetail, D4Box_RuntimeStop, D4Box_MacroSave
    global MUI_FontName, MUI_T, MUI_Theme
    MUI_SetTheme(MUI_LoadThemeSetting(A_ScriptDir "\settings.ini"))
    MUI_Startup()
    myGui := Gui("", L("app.title"))
    myGui.BackColor := MUI_Hex(MUI_T.window)
    myGui.SetFont("s10 c" MUI_Hex(MUI_T.text), MUI_FontName)
    myGui.MarginX := 12
    myGui.MarginY := 12

    D4Box_MainTabs := myGui.AddTab3("x12 y12 w696 h386 -Theme Background" MUI_Hex(MUI_T.window), [L("binding.title"), L("main.section.skills")])
    D4Box_MainTabs.UseTab(1)
    D4Box_CreateBindingsGUI()
    D4Box_MainTabs.UseTab(2)
    myGui.SetFont("s9 norm")
    presetTab := myGui.AddDropDownList("x32 y54 w250 Choose1", presetNames)
    presetTab.OnEvent("Change", OnPresetTabChange)
    presetTab.OnEvent("ContextMenu", OnPresetTabRightClick)
    myGui.AddText("x294 y58 w270 h22", L("main.renameHint"))
    myGui.AddButton("x580 y50 w108 h28", L("binding.rename")).OnEvent("Click", (*) => RenamePreset(currentPreset))
    myGui.AddText("x130 y94 w75 Center", L("main.col.hotkey"))
    myGui.AddText("x215 y94 w95 Center", L("main.col.strategy"))
    myGui.AddText("x320 y94 w125 Center", L("main.col.interval"))
    myGui.AddText("x450 y94 w100 Center", L("main.col.delay"))
    myGui.AddText("x555 y94 w90 Center", L("main.col.random"))
    myGui.SetFont("s10")
    CreateSkillRows()
    ; Configuration and start actions belong to the macro tab.
    myGui.SetFont("s9 norm")
    myGui.AddText("x32 y362 w92 h22", L("main.toggle.label"))
    startStopHotkeyCtrl := myGui.AddHotkey("x128 y356 w96", "F1")
    startStopMouseCtrl := myGui.AddDropDownList("x232 y356 w104 Choose1"
        , [L("main.toggle.keyboard"), L("main.toggle.middle"), L("main.toggle.side1"), L("main.toggle.side2")])
    startStopButton := ModernButton(myGui, 388, 350, 144, 36, L("main.start") " (F1)", "primary"
        , {bg: MUI_T.window, fontSize: 10, radius: 8})
    startStopButton.OnEvent("Click", ToggleMacro)
    D4Box_MacroSave := myGui.AddButton("x544 y350 w144 h36", L("main.saveMacro"))
    D4Box_MacroSave.OnEvent("Click", SaveSettings)
    startStopHotkeyCtrl.OnEvent("Change", OnStartStopKeyboardChanged)
    startStopMouseCtrl.OnEvent("Change", OnStartStopMouseChanged)
    D4Box_MainTabs.UseTab()

    myGui.SetFont("s10 bold")
    statusText := myGui.AddText("x20 y410 w528 h24 +0x4000", "")
    myGui.SetFont("s9 norm")
    D4Box_RuntimeStop := myGui.AddButton("x560 y406 w148 h32 Hidden", "")
    D4Box_RuntimeStop.OnEvent("Click", D4Box_StopFromFooter)
    D4Box_RuntimeDetail := myGui.AddText("x20 y438 w688 h34", "")
    statusBar := myGui.AddText("x20 y476 w688 h20 +0x4000", "")
    myGui.AddText("x20 y506 w496 h22", L("main.hint"))
    myGui.AddButton("x532 y500 w92 h28", MUI_Theme = "dark" ? L("main.theme.toLight") : L("main.theme.toDark")).OnEvent("Click", ToggleTheme)
    myGui.AddButton("x632 y500 w76 h28", Lang_SwitchButtonText()).OnEvent("Click", ToggleLanguage)
    D4Box_RefreshRuntimeStatus()
}

/**
 * 初始化GUI
 */
InitializeGUI() {
    global myGui, statusBar, MUI_T, MUI_FontName

    ; 日志开关与轮转要在最早的日志写入之前初始化
    InitLogging()

    ; 全局异常兜底：记录并抑制错误弹窗，避免定时器线程因异常弹出对话框
    OnError(GlobalErrorHandler)

    ; 创建主GUI
    CreateMainGUI()

    MUI_ApplyNativeTheme(myGui)

    ; 先加载设置（在Show之前，避免Tab2内的DropDownList渲染不刷新）
    LoadSettings()
    RegisterStartStopHotkey()
    D4Box_BindingsInit()

    ; 显示GUI（此时下拉框值已正确设置，首次渲染即为正确状态）
    myGui.Show("w720 h532")

    ; 强制重绘窗口，确保所有控件（尤其是Tab2内的DropDownList）正确显示
    WinRedraw(myGui.Hwnd)

    ; 设置窗口事件处理 - 退出时自动保存
    myGui.OnEvent("Close", OnGuiClose)
    myGui.OnEvent("Escape", OnGuiClose)

    ; 退出兜底：停止定时器并释放所有被按住的按键，避免退出后游戏里按键卡住
    OnExit(CleanupOnExit)
}

/**
 * 全局异常处理：写日志并抑制错误弹窗
 * 定时器/热键线程里未捕获的异常如果冒泡到默认处理器，会弹窗打断宏的运行
 * @param {Object} err - Error 对象
 * @param {String} mode - 出错时的执行模式
 * @returns {Boolean} - true 表示已处理，不再弹窗
 */
GlobalErrorHandler(err, mode) {
    global isRunning

    try {
        LogError("未处理异常 [" mode "] " err.Message
            . " | 位置: " (err.File = "" ? "?" : err.File) ":" err.Line
            . " | 宏运行中: " (isRunning ? "是" : "否"))
    } catch {
        ; 记录异常时再次出错，忽略（避免死循环）
    }

    return true
}

/**
 * 根据运行状态更新开始/停止按钮（运行中显示红色“停止”）
 */
UpdateStartStopButton() {
    global startStopButton, isRunning
    if (startStopButton = "")
        return
    key := GetStartStopHotkeyDisplay()
    startStopButton.Text := (isRunning ? L("main.stop") : L("main.start")) . "  (" . key . ")"
    startStopButton.SetStyle(isRunning ? "danger" : "primary")
    D4Box_RefreshRuntimeStatus()
}

; Derive live status from the executor state, independently of transient save messages.
D4Box_RefreshRuntimeStatus() {
    global statusText, D4Box_RuntimeDetail, D4Box_RuntimeStop, D4Box_RuntimeAction, MUI_T
    global isRunning, isPaused, D4Box_CubeBusy, isAutoTransmuting, presetNames, currentPreset
    global D4Box_WorkflowName, D4Box_WorkflowStep
    if !IsObject(D4Box_RuntimeStop)
        return
    detail := "", action := "", color := MUI_Hex(MUI_T.muted)
    if D4Box_CubeBusy {
        name := D4Box_WorkflowName != "" ? D4Box_WorkflowName : L("runtime.preparing")
        title := L(isAutoTransmuting ? "runtime.workflow" : "runtime.cancelling", name)
        detail := D4Box_WorkflowStep != "" ? L("runtime.step", D4Box_WorkflowStep) : L("runtime.preparing")
        if isRunning
            detail .= "`n" L("runtime.macroSuspended", presetNames[currentPreset])
        action := "workflow"
        D4Box_RuntimeStop.Text := L("runtime.cancelWorkflow")
        D4Box_RuntimeStop.Enabled := isAutoTransmuting
        color := "F59E0B"
    } else if isRunning {
        title := L(isPaused ? "runtime.macroPaused" : "runtime.macroRunning", presetNames[currentPreset])
        detail := L("runtime.macroKey", GetStartStopHotkeyDisplay())
        action := "macro"
        D4Box_RuntimeStop.Text := L("runtime.stopMacro")
        D4Box_RuntimeStop.Enabled := true
        color := isPaused ? "F59E0B" : "10B981"
    } else {
        title := L("main.ready")
        detail := L("runtime.idleHint")
    }
    statusText.Text := title
    statusText.ToolTip := title
    statusText.SetFont("c" color)
    D4Box_RuntimeDetail.Text := detail
    D4Box_RuntimeDetail.ToolTip := detail
    D4Box_RuntimeAction := action
    D4Box_RuntimeStop.Visible := action != ""
}

; This control can only stop/cancel. A late queued click must never start a macro.
D4Box_StopFromFooter(*) {
    global D4Box_RuntimeAction, D4Box_CubeBusy, isRunning
    Critical "On"
    try {
        if D4Box_RuntimeAction = "workflow" && D4Box_CubeBusy
            D4Box_InterruptCube()
        else if D4Box_RuntimeAction = "macro" && isRunning && !D4Box_CubeBusy
            ToggleMacro()
        D4Box_RefreshRuntimeStatus()
    } finally {
        Critical "Off"
    }
}

/**
 * 切换浅色/深色主题（保存后重新加载脚本生效）
 */
ToggleTheme(*) {
    global isRunning, statusBar, MUI_Theme, D4Box_CubeBusy, D4Box_CubeCapturePending
    if (isRunning || D4Box_CubeBusy || D4Box_CubeCapturePending) {
        statusBar.Text := L("status.themeStopFirst")
        return
    }
    try {
        SaveSettings()
        IniWrite(MUI_Theme = "dark" ? "light" : "dark", A_ScriptDir "\settings.ini", "UI", "Theme")
    } catch as err {
        statusBar.Text := L("status.themeFailed", err.Message)
        DebugLog("切换主题失败: " . err.Message)
        return
    }
    Reload()
}

/**
 * 切换界面语言（保存后重新加载脚本生效）
 * 与切换主题同一套路：宏运行中不允许，避免运行中被重载打断
 */
ToggleLanguage(*) {
    global isRunning, statusBar, appLang, D4Box_CubeBusy, D4Box_CubeCapturePending
    if (isRunning || D4Box_CubeBusy || D4Box_CubeCapturePending) {
        statusBar.Text := L("status.langStopFirst")
        return
    }
    try {
        appLang := (appLang = "zh") ? "en" : "zh"
        SaveSettings()   ; SaveGeneralSettings 会把 [UI] Language 一起写入
    } catch as err {
        statusBar.Text := L("status.langFailed", err.Message)
        DebugLog("切换语言失败: " . err.Message)
        return
    }
    Reload()
}

/**
 * GUI关闭事件处理 - 退出前自动保存设置
 */
OnGuiClose(*) {
    try {
        SaveSettings()
        DebugLog("退出前已自动保存设置")
    } catch as err {
        DebugLog("退出前保存设置失败: " err.Message)
    }
    ExitApp()
}
