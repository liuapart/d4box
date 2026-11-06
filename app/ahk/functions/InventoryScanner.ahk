; Inventory scanning is separate from cube workflow execution.
/**
 * 按像素 RGB 快速分类装备品质（只看右下角采样点）
 * 颜色簇（2K 实测，取右下角小块平均值）：
 *   红装(传奇/暗金) R≈120-130 G≈55-74 B≈20-30  → R≫G≫B
 *   黄装(稀有)      R≈84-92  G≈63-68 B≈10      → R≈G，B 极低
 *   蓝装(魔法)      R≈31-34  G≈44-50 B≈70-77   → B>G>R
 *   空格/暗背景      各通道都低且接近
 * @returns {String} red / yellow / blue / dark / other
 */
ClassifyInventoryPixelFast(r, g, b) {
    brightness := Max(r, g, b)
    contrast := brightness - Min(r, g, b)

    ; 蓝装（魔法）：蓝通道明显高于红/绿
    if (b >= r + 15 && b >= g + 12 && b >= 55)
        return "blue"

    ; 空格 / 暗背景
    if (brightness < 48 || (brightness < 60 && contrast < 20))
        return "dark"

    ; 红装（传奇/暗金）：红通道占绝对主导
    if (r >= 100 && (r - g) >= 34 && r >= b + 40)
        return "red"

    ; 黄装（稀有）：红绿都较高且接近，蓝很低
    if (r >= 58 && g >= 45 && b <= 40 && (r - g) <= 34 && r >= b + 25)
        return "yellow"

    return "other"
}

/**
 * 装备格右下角采样偏移（品质色边框在此处最稳定）
 * 格子为长方形（约 73 宽 x 108 高），X/Y 采用不同比例
 */
GetInventoryBottomRightOffset(stepX, stepY) {
    return {x: Round(stepX * 0.36), y: Round(stepY * 0.39)}
}

/**
 * 右下角采样小块半径（取 7x7 平均，抗物品图标/网格线干扰）
 */
GetInventorySampleRadius() {
    return 3
}

/**
 * 一次性截取装备栏区域（11x3 格），避免逐点 PixelGetColor
 */
CaptureInventoryGridBitmap(startX, startY, stepX, stepY) {
    offset := GetInventoryBottomRightOffset(stepX, stepY)
    margin := GetInventorySampleRadius() + 4
    originX := Round(startX) - margin
    originY := Round(startY) - margin
    endX := Round(startX + 10 * stepX + offset.x) + margin
    endY := Round(startY + 2 * stepY + offset.y) + margin

    return CaptureScreenRegion(originX, originY, endX - originX + 1, endY - originY + 1)
}

/**
 * 从已截取的位图判断格子品质（仅取右下角小块平均）
 * @returns {String} red=传奇/暗金 yellow=稀有 blue=魔法 dark=空格 other=未知
 */
ClassifyInventorySlotFromCapture(capture, centerX, centerY, stepX, stepY) {
    offset := GetInventoryBottomRightOffset(stepX, stepY)
    rgb := ReadCapturedPatchAvg(capture, centerX + offset.x, centerY + offset.y, GetInventorySampleRadius())
    return ClassifyInventoryPixelFast(rgb.r, rgb.g, rgb.b)
}

/**
 * 查找右下装备栏/背包 11列x3行中的传奇物品（单次截图 + 内存取色，目标 <2s）
 *
 * 网格参数按 2K(2560x1440) 实测标定（取每格右下角品质色）：
 *   列：首格中心 x=1726.7，列间距 73.36（11 列）
 *   行：首行中心 y=1018，  行间距 108  （3 行，格子为长方形）
 * @param {String} targetQuality 目标品质：red(默认,传奇/暗金)、yellow、blue、redyellow(红+黄)、any
 */
FindInventoryItems(targetQuality := "red", frame := "") {
    global isAutoTransmuting

    itemPositions := []
    if !IsObject(frame)
        frame := {x: 0, y: 0, w: A_ScreenWidth, h: A_ScreenHeight}
    startX := frame.x + frame.w * 0.674484
    startY := frame.y + frame.h * 0.706944
    stepX := frame.w * 0.0286563
    stepY := frame.h * 0.075
    scanStart := A_TickCount

    capture := CaptureInventoryGridBitmap(startX, startY, stepX, stepY)
    if !capture.ok {
        DebugLog("自动嬗变：装备栏区域截图失败")
        return itemPositions
    }

    Loop 3 {
        row := A_Index
        Loop 11 {
            if !isAutoTransmuting
                return itemPositions

            col := A_Index
            x := Round(startX + (col - 1) * stepX)
            y := Round(startY + (row - 1) * stepY)

            quality := ClassifyInventorySlotFromCapture(capture, x, y, stepX, stepY)
            if (targetQuality = "any")
                isTarget := (quality = "red" || quality = "yellow" || quality = "blue")
            else if (targetQuality = "redyellow")
                isTarget := (quality = "red" || quality = "yellow")
            else
                isTarget := (quality = targetQuality)

            if (isTarget)
                itemPositions.Push({x: x, y: y, row: row, col: col, quality: quality})
        }
    }

    DebugLog("自动嬗变：扫描完成，耗时 " (A_TickCount - scanStart) "ms，找到 "
        itemPositions.Length " 个目标物品(品质=" targetQuality ")")
    return itemPositions
}
