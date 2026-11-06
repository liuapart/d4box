; ========== 功能键动作 ==========
/**
 * 统一的按键发送函数
 * @param {String} key - 要发送的按键
 */
SendKey(key) {
    global shiftEnabled

    if (shiftEnabled) {
        Send "{Shift down}"
        Sleep 10
        Send "{" key "}"
        Sleep 10
        Send "{Shift up}"
    } else {
        Send "{" key "}"
    }
}

/**
 * 按下翻滚键(空格)
 */
PressDodge() {
    global isRunning, isPaused, utilityControls

    if (isRunning && !isPaused && utilityControls.dodge.enable.Value = 1) {
        SendKey("Space")
        DebugLog("按下翻滚键", 2)
    }
}

/**
 * 按下喝药键
 * 启用"血量检测"时，先读血球液面估算血量，低于阈值才喝
 */
PressPotion() {
    global isRunning, isPaused, utilityControls, healthCheckEnabled, lastPotionTick

    if (!isRunning || isPaused || utilityControls.potion.enable.Value != 1)
        return

    if (healthCheckEnabled) {
        if (!ShouldDrinkPotion()) {
            DebugLog("血量未低于阈值，跳过喝药", 2)
            return
        }

        ; 血量持续偏低时不要连续按键（游戏内药水也有冷却）
        if (A_TickCount - lastPotionTick < 1500) {
            DebugLog("喝药冷却中，跳过", 2)
            return
        }
    }

    key := utilityControls.potion.key.Value
    if (key = "")
        return

    SendKey(key)
    lastPotionTick := A_TickCount
    DebugLog("按下喝药键: " key, 2)
}

/**
 * 按下强制移动键
 */
PressForceMove() {
    global isRunning, isPaused, utilityControls

    if (isRunning && !isPaused && utilityControls.forceMove.enable.Value = 1) {
        key := utilityControls.forceMove.key.Value
        if (key != "") {
            SendKey(key)
            DebugLog("按下强制移动键: " key, 2)
        }
    }
}

/**
 * 根据当前屏幕尺寸计算截图中的相对坐标
 * @param {Float} xRatio - X坐标占屏幕宽度的比例
 * @param {Float} yRatio - Y坐标占屏幕高度的比例
 * @returns {Object} 屏幕坐标
 */
ScalePoint(xRatio, yRatio) {
    return {x: Round(A_ScreenWidth * xRatio), y: Round(A_ScreenHeight * yRatio)}
}

/**
 * 从内存位图读取像素 RGB
 */
ReadCapturedPixel(capture, screenX, screenY) {
    localX := screenX - capture.originX
    localY := screenY - capture.originY
    if (localX < 0 || localY < 0 || localX >= capture.width || localY >= capture.height)
        return {r: 0, g: 0, b: 0}

    offset := localY * capture.stride + localX * 3
    return {
        b: NumGet(capture.bits, offset, "UChar"),
        g: NumGet(capture.bits, offset + 1, "UChar"),
        r: NumGet(capture.bits, offset + 2, "UChar")
    }
}

/**
 * 从内存位图读取以 (screenX, screenY) 为中心、半径 radius 的小块平均 RGB
 * 单像素易被物品图标或网格线干扰，小块平均显著提升识别稳定性
 */
ReadCapturedPatchAvg(capture, screenX, screenY, radius) {
    sumR := 0, sumG := 0, sumB := 0, count := 0

    Loop (2 * radius + 1) {
        dy := A_Index - 1 - radius
        localY := screenY + dy - capture.originY
        if (localY < 0 || localY >= capture.height)
            continue

        Loop (2 * radius + 1) {
            dx := A_Index - 1 - radius
            localX := screenX + dx - capture.originX
            if (localX < 0 || localX >= capture.width)
                continue

            offset := localY * capture.stride + localX * 3
            sumB += NumGet(capture.bits, offset, "UChar")
            sumG += NumGet(capture.bits, offset + 1, "UChar")
            sumR += NumGet(capture.bits, offset + 2, "UChar")
            count += 1
        }
    }

    if (count = 0)
        return {r: 0, g: 0, b: 0}

    return {r: sumR // count, g: sumG // count, b: sumB // count}
}

/**
 * 抓取屏幕区域到内存位图（BGR 24 位，行按 4 字节对齐）
 * @param {Integer} originX, originY - 区域左上角屏幕坐标
 * @param {Integer} width, height - 区域尺寸
 * @returns {Object} {bits, stride, width, height, originX, originY, ok}
 */
CaptureScreenRegion(originX, originY, width, height) {
    if (width <= 0 || height <= 0)
        return {bits: Buffer(8), stride: 0, width: 0, height: 0
            , originX: originX, originY: originY, ok: 0}

    hdcScreen := DllCall("GetDC", "Ptr", 0, "Ptr")
    hdcMem := DllCall("CreateCompatibleDC", "Ptr", 0, "Ptr")
    hBitmap := DllCall("CreateCompatibleBitmap", "Ptr", hdcScreen, "Int", width, "Int", height, "Ptr")
    oldBitmap := DllCall("SelectObject", "Ptr", hdcMem, "Ptr", hBitmap, "Ptr")
    DllCall("gdi32\BitBlt", "Ptr", hdcMem, "Int", 0, "Int", 0, "Int", width, "Int", height
        , "Ptr", hdcScreen, "Int", originX, "Int", originY, "UInt", 0x00CC0020)

    bi := Buffer(40, 0)
    NumPut("UInt", 40, bi, 0)
    NumPut("Int", width, bi, 4)
    NumPut("Int", -height, bi, 8)
    NumPut("UShort", 1, bi, 12)
    NumPut("UShort", 24, bi, 14)

    stride := ((width * 3 + 3) // 4) * 4
    ; 多分配 8 字节：允许按 4 字节整块读取行末像素
    bits := Buffer(stride * height + 8)
    ok := DllCall("gdi32\GetDIBits", "Ptr", hdcMem, "Ptr", hBitmap, "UInt", 0, "Int", height
        , "Ptr", bits, "Ptr", bi, "UInt", 0)

    DllCall("SelectObject", "Ptr", hdcMem, "Ptr", oldBitmap)
    DllCall("DeleteObject", "Ptr", hBitmap)
    DllCall("DeleteDC", "Ptr", hdcMem)
    DllCall("ReleaseDC", "Ptr", 0, "Ptr", hdcScreen)

    return {bits: bits, stride: stride, width: width, height: height
        , originX: originX, originY: originY, ok: ok}
}

/**
 * 重置所有按住模式的按键状态
 * 释放任何仍处于按下状态的技能键、停止对应的检查定时器
 */
ResetAllHoldKeyStates() {
    global holdKeyStates, boundCheckHoldTimers, skillControls

    ; 释放所有仍在按下的Hold模式按键并停止其检查定时器
    for skillNum, isHeld in holdKeyStates {
        if (isHeld && skillControls.Has(skillNum)) {
            key := skillControls[skillNum].key.Value
            if (key != "") {
                Send "{" key " up}"
                DebugLog("ResetAllHoldKeyStates 释放技能" skillNum " 键: " key, 2)
            }
        }
        if (boundCheckHoldTimers.Has(skillNum)) {
            SetTimer(boundCheckHoldTimers[skillNum], 0)
        }
    }

    holdKeyStates := Map()
    boundCheckHoldTimers := Map()
    DebugLog("重置所有按住模式的按键状态")
}

/**
 * 释放所有可能被按住的按键
 */
ReleaseAllKeys() {
    global skillControls

    ; 释放修饰键
    Send "{Shift up}"
    Send "{Ctrl up}"
    Send "{Alt up}"

    ; 释放技能键
    Loop 4 {
        key := skillControls[A_Index].key.Value
        if key != "" {
            Send "{" key " up}"
            DebugLog("释放技能" A_Index " 键: " key, 2)
        }
    }

    ; 释放鼠标按键
    SetMouseDelay -1
    Click "up left"
    Click "up right"

    ; 重置所有按住模式的按键状态
    ResetAllHoldKeyStates()

    ; 重置鼠标按键状态
    ResetMouseButtonStates()

    DebugLog("已释放所有按键")
}

/**
 * 退出前的兜底清理
 * 脚本正常退出、窗口关闭、被 #SingleInstance Force 顶掉时都会走这里，
 * 确保不会把按住状态的按键留在游戏里
 */
CleanupOnExit(*) {
    global isRunning

    try {
        StopAllTimers()      ; 内含释放技能键与鼠标键
        ReleaseAllKeys()
        isRunning := false
        DebugLog("退出清理完成：已停止所有定时器并释放按键")
    } catch as err {
        ; 退出阶段不再弹错误框，只记录
        LogError("退出清理失败: " err.Message)
    }
}
