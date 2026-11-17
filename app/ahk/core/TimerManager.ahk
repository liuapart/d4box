; ========== 定时器管理 ==========
/**
 * 启动所有定时器
 */
StartAllTimers() {
    global D4Box_CubeBusy
    if D4Box_CubeBusy
        return
    ; 先停止所有定时器，确保清理
    StopAllTimers()

    ; 启动技能定时器
    StartSkillTimers()

    ; 启动左右键技能定时器
    D4Box_StartMouseTimers()

    DebugLog("所有定时器已启动")
}

/**
 * 计算策略型控件的最终定时器间隔
 * 包含 interval + delay/random 计算和最小20ms下限
 * @param {Object} control - 含interval、delay、random属性的控件
 * @returns {Integer} 最终定时器间隔（毫秒），未启用时返回0
 */
CalcStrategyInterval(control) {
    if (control.strategy.Value <= 1)
        return 0

    interval := ToInt(control.interval.Value)
    delay := ToInt(control.delay.Value)
    isRandom := control.random.Value

    if (delay != 0) {
        if (isRandom)
            interval += Random(0, Abs(delay))
        else
            interval += delay
    }

    if (interval < 20)
        interval := 20

    return interval
}

/**
 * 启动技能定时器
 */
StartSkillTimers() {
    global skillControls, boundSkillTimers, timerStates

    for i in [1, 2, 3, 4] {
        interval := CalcStrategyInterval(skillControls[i])
        if (interval > 0) {
            boundSkillTimers[i] := PressSkill.Bind(i)
            SetTimer(boundSkillTimers[i], interval)
            timerStates[i] := true
            DebugLog("启动技能" i "定时器，间隔: " interval, 2)
        }
    }
}

/**
 * 启动左右键技能定时器
 */
D4Box_StartMouseTimers() {
    ; 鼠标控件使用策略模式
    StartStrategyTimer("leftClick", mouseControls.left, PressLeftClick)
    StartStrategyTimer("rightClick", mouseControls.right, PressRightClick)
}

/**
 * 启动策略型定时器（用于鼠标控件，使用strategy代替enable）
 * @param {String} name - 定时器名称
 * @param {Object} control - 控件对象（含strategy、interval、delay、random属性）
 * @param {Function} timerFunc - 定时器函数
 */
StartStrategyTimer(name, control, timerFunc) {
    global timerStates

    interval := CalcStrategyInterval(control)
    if (interval > 0) {
        SetTimer(timerFunc, interval)
        timerStates[name] := true
        DebugLog("启动" name "定时器 - 间隔: " interval)
    }
}

/**
 * 停止所有定时器
 */
StopAllTimers() {
    global boundSkillTimers, skillControls

    ; 停止技能定时器
    Loop 4 {
        if boundSkillTimers.Has(A_Index) {
            SetTimer(boundSkillTimers[A_Index], 0)
            boundSkillTimers.Delete(A_Index)
            DebugLog("停止技能" A_Index "定时器", 2)
        }

        ; 如果是按住模式，确保释放按键
        key := skillControls[A_Index].key.Value
        if (key != "") {
            Send "{" key " up}"
        }
    }

    ; 停止所有其他定时器
    SetTimer PressLeftClick, 0
    SetTimer PressRightClick, 0

    ; 重置所有按住模式的按键状态
    ResetAllHoldKeyStates()

    ; 重置鼠标按键状态
    ResetMouseButtonStates()

    DebugLog("已停止所有定时器并释放按键")
}
