; Diagnostic only: does not send keys, move the mouse, or click.
D4Box_ShowWindowInfo() {
    global D4Box_RuntimeDir

    hwnd := WinActive("ahk_class Diablo IV Main Window Class")
    if !hwnd
        return

    try {
        WinGetClientPos(&x, &y, &width, &height, "ahk_id " hwnd)
        if (width <= 0 || height <= 0)
            throw Error("Game client area is empty.")

        details := Format("HWND={1} client=({2}, {3}) size={4}x{5}", hwnd, x, y, width, height)
        DirCreate(D4Box_RuntimeDir)
        FileAppend(FormatTime(, "yyyy-MM-dd HH:mm:ss") " " details "`n",
            D4Box_RuntimeDir "\window-diagnostics.log", "UTF-8")
        ToolTip("d4box`n" details)
    } catch as err {
        ToolTip("d4box diagnostic failed:`n" err.Message)
    }

    SetTimer(D4Box_ClearToolTip, -5000)
}

D4Box_ClearToolTip() {
    ToolTip()
}
