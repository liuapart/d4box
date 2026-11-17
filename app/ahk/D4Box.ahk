#Requires AutoHotkey v2.0
#SingleInstance Force
ProcessSetPriority "High"
A_IconTip := D4Box_Title()

; 允许热键中断正在运行的线程
#MaxThreadsPerHotkey 2

; ========== 包含所有模块 ==========
; 界面语言（必须早于 GlobalVars：策略名、预设名在定义时就要按语言取值）
#Include "utils/Lang.ahk"
#Include "core/Version.ahk"
#Include "utils/Json.ahk"
#Include "core/Updates.ahk"
#Include "core/Distribution.ahk"

; 全局变量定义
#Include "core/GlobalVars.ahk"

; 工具类
#Include "utils/Logger.ahk"
#Include "utils/Settings.ahk"

; 核心功能
#Include "core/WindowManager.ahk"
#Include "core/TimerManager.ahk"
#Include "core/MacroControl.ahk"
#Include "core/HotkeyManager.ahk"
#Include "core/WorkflowHotkeys.ahk"

; GUI模块
#Include "gui/ModernUI.ahk"
#Include "gui/MainGUI.ahk"
#Include "gui/UpdatesGUI.ahk"
#Include "gui/WorkflowHotkeysGUI.ahk"
#Include "gui/SkillControls.ahk"
#Include "gui/CubeWorkflowGUI.ahk"

; 功能模块
#Include "functions/SkillSystem.ahk"
#Include "functions/MouseActions.ahk"
#Include "functions/UtilityActions.ahk"
#Include "functions/InventoryScanner.ahk"
#Include "functions/CubeProfiles.ahk"
#Include "functions/DetectionRegions.ahk"
#Include "gui/DetectionRegionsGUI.ahk"
#Include "functions/CubeRunner.ahk"
#Include "functions/WindowDiagnostics.ahk"

; 热键定义
#Include "hotkeys/GameHotkeys.ahk"

; ========== 程序启动 ==========
; 初始化GUI并启动程序
if A_Args.Length = 1 && A_Args[1] = "--self-test"
    D4Box_DistributionSelfTest()
try D4Box_PrepareDistribution()
catch as err {
    MsgBox(L("update.portableError", err.Message), D4Box_Title(), "Iconx")
    ExitApp(1)
}
InitializeGUI()
