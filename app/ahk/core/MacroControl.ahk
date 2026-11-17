; ========== 宏控制功能 ==========
/**
 * 切换宏运行状态
 */
ToggleMacro(*) {
    global isRunning, isPaused, previouslyPaused


    global D4Box_CubeBusy
    if D4Box_CubeBusy {
        D4Box_CancelCube()
        isRunning := false
        isPaused := false
        UpdateStartStopButton()
        return
    }

    ; 确保完全停止所有定时器
    StopAllTimers()

    ; 切换运行状态
    isRunning := !isRunning

    if isRunning {
        ; 重置暂停状态
        isPaused := false
        previouslyPaused := false

        ; 启动前先确保没有残留的按下状态（上次异常退出等）
        ReleaseAllKeys()

        ; 只有在暗黑4窗口激活时才启动定时器
        if WinActive("ahk_class Diablo IV Main Window Class") {
            StartAllTimers()
            UpdateStatus("running", L("bar.macroStarted"))
        } else {
            isPaused := true
            UpdateStatus("pausedWindow", L("bar.windowInactive"))
        }
    } else {
        ; 确保重置所有状态
        isPaused := false
        previouslyPaused := false
        UpdateStatus("stopped", L("bar.macroStopped"))

        ; 确保释放所有按键
        ReleaseAllKeys()
    }

    UpdateStartStopButton()
    DebugLog("宏状态切换: " . (isRunning ? "运行" : "停止"))
}
