; ========== 窗口管理 ==========
/**
 * 窗口切换检查函数
 * 检测暗黑4窗口是否激活，并在状态变化时触发相应事件
 */
CheckWindow() {
    static lastState := false
    currentState := WinActive("ahk_class Diablo IV Main Window Class")

    if (currentState != lastState) {
        OnWindowChange(currentState)
        lastState := currentState
    }
}

/**
 * 窗口切换事件处理
 * @param {Boolean} isActive - 暗黑4窗口是否激活
 */
OnWindowChange(isActive) {
    global isRunning, isPaused, previouslyPaused, statusText, statusBar

    global D4Box_CubeBusy
    if D4Box_CubeBusy {
        if !isActive
            D4Box_InterruptCube()
        return
    }

    if (!isActive) {  ; 窗口失去焦点
        if (isRunning) {
            previouslyPaused := isPaused
            if (!isPaused) {
                StopAllTimers()
                isPaused := true
                UpdateStatus("pausedWindow", L("bar.windowInactive"))
            }
        }
    } else if (isRunning && isPaused && !previouslyPaused) {  ; 窗口获得焦点且之前不是手动暂停
        StartAllTimers()
        isPaused := false
        UpdateStatus("running", L("bar.windowActive"))
    }
}

/**
 * 更新状态显示
 * @param {String} stateKey - 状态键（state.running / state.paused / state.stopped …），
 *                            用于取状态文字与配色，见 utils/Lang.ahk
 * @param {String} barText - 状态栏文本（调用方先按当前语言取好）
 * @param {String} statusLabel - 可选：覆盖状态文字（默认取 state.<stateKey> 的译文）
 */
UpdateStatus(stateKey, barText, statusLabel := "") {
    global statusText, statusBar
    label := (statusLabel != "") ? statusLabel : L("state." stateKey)
    statusBar.Text := barText
    statusBar.ToolTip := barText
    D4Box_RefreshRuntimeStatus()
    DebugLog("状态更新: " label " | " barText)
}
