#Requires AutoHotkey v2.0
#SingleInstance Force
#Include WindowDiagnostics.ahk

; Keep the upstream process and this extension independent.
global D4Box_RuntimeDir := A_ScriptDir "\..\..\.runtime"
A_IconTip := "d4box - window diagnostics (Ctrl+Alt+F8)"

#HotIf WinActive("ahk_class Diablo IV Main Window Class")
^!F8::D4Box_ShowWindowInfo()
#HotIf
