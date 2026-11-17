#Requires AutoHotkey v2.0
#SingleInstance Off
#NoTrayIcon
#Include "..\core\Version.ahk"
#Include "..\utils\Json.ahk"
#Include "..\core\Updates.ahk"

report := A_ScriptDir "\update_result.txt"
if FileExist(report)
    FileDelete(report)
failures := 0
Check(condition, name) {
    global report, failures
    FileAppend((condition ? "PASS " : "FAIL ") name "`n", report, "UTF-8")
    if !condition
        failures++
}
Reject(callback, name) {
    try {
        callback.Call()
        Check(false, name)
    } catch
        Check(true, name)
}
Release(version, draft := false, prerelease := false) {
    return Map("tag_name", "d4box-v" version, "draft", draft, "prerelease", prerelease, "assets", [
        Map("name", "D4Box-v" version "-windows-x64.exe", "state", "uploaded", "size", 1500000),
        Map("name", "SHA256SUMS.txt", "state", "uploaded", "size", 220)])
}

class FakeRequest {
    __New(text := "[]", status := 200, ready := true) {
        this.ResponseText := text, this.Status := status, this.ready := ready, this.aborted := false
    }
    WaitForResponse(seconds) {
        return this.ready
    }
    Abort() {
        this.aborted := true
    }
}

try {
    Check(D4Box_CompareVersion("0.10.0", "0.9.9") > 0, "numeric version ordering")
    Check(D4Box_CompareVersion("1.0.0", "0.99.99") > 0, "major ordering")
    Check(D4Box_CompareVersion("1.0.0", "1.0.0") = 0, "equal versions")
    Check(D4Box_CompareVersion("0.3.0", "0.3.1") < 0, "patch ordering")
    for version in ["01.2.3", "1.2", "1.2.3-beta", "1.2.3.4", "1.2.3`n", "10000.0.0"]
        Reject(() => D4Box_CompareVersion(version, "0.3.0"), "invalid stable version " version)

    parsed := D4Box_JsonReader.Parse('{"text":"\u4e2d\u6587 \ud83d\ude00 \" \\ \n","items":[false,true,null,-1.25e2]}')
    Check(parsed["text"] = "中文 " Chr(0x1F600) ' " \ ' "`n", "JSON unicode and escaped strings")
    Check(parsed["items"][1] = 0 && parsed["items"][2] = 1 && parsed["items"][4] = -125, "JSON scalar values")
    for invalid in ['{"a":1,"a":2}', '[1,]', '{"a":}', '[01]', '{bad}', '[TRUE]', '"\U1234"', '[] trailing', '"\ud800"', '"\uDC00"', '"\x00"', '"unterminated']
        Reject(() => D4Box_JsonReader.Parse(invalid), "malformed JSON rejected")
    nested := "0"
    loop 66
        nested := "[" nested "]"
    Reject(() => D4Box_JsonReader.Parse(nested), "JSON nesting bound")

    editor := Release("99.0.0")
    editor["tag_name"] := "editor-v99.0.0"
    selected := D4Box_SelectRelease([editor, Release("2.0.0", true), Release("3.0.0", false, true), Release("0.9.9"), Release("0.10.0")])
    Check(selected.version = "0.10.0", "ignore editor, draft and prerelease; choose numeric maximum")
    Check(selected.url = "https://github.com/liuapart/d4box/releases/tag/d4box-v0.10.0", "trusted release URL")
    Check(!D4Box_SelectRelease([editor]), "editor-only repository has no app update")
    missing := Release("9.0.0")
    missing["assets"].Pop()
    Check(!D4Box_SelectRelease([missing]), "missing checksum asset excluded")
    pending := Release("9.0.0")
    pending["assets"][1]["state"] := "new"
    Check(!D4Box_SelectRelease([pending]), "unfinished executable upload excluded")
    preview := Release("9.0.0-beta")
    Check(!D4Box_SelectRelease([preview]), "prerelease tag excluded even when flag is false")
    badTag := Release("9.0.0")
    badTag["tag_name"] .= "`n"
    Check(!D4Box_SelectRelease([badTag]), "newline suffix cannot enter release URL")
    selected := D4Box_SelectRelease([Release("0.3.0")], selected)
    Check(selected.version = "0.10.0", "pagination retains highest eligible release")
    Reject(() => D4Box_SelectRelease(Map("message", "API error")), "API error object rejected")
    Reject(() => D4Box_SelectRelease([Map()]), "malformed release rejected")

    outcome := {called: false, error: "", release: false}
    Done(release, errorText) {
        global outcome
        outcome.called := true, outcome.error := errorText, outcome.release := release
    }
    job := D4Box_UpdateCheck(Done)
    request := FakeRequest(), job.request := request
    job.Poll()
    Check(outcome.called && outcome.error = "" && !outcome.release && request.aborted, "empty API response completed and disposed")
    outcome.called := false
    job := D4Box_UpdateCheck(Done), job.request := FakeRequest("{}", 403)
    job.Poll()
    Check(outcome.called && InStr(outcome.error, "403"), "rate limit reported as failure")
    outcome.called := false
    job := D4Box_UpdateCheck(Done), job.request := FakeRequest("not JSON")
    job.Poll()
    Check(outcome.called && outcome.error != "", "invalid server response reported as failure")
    outcome.called := false
    job := D4Box_UpdateCheck(Done), request := FakeRequest("[]", 200, false), job.request := request
    job.Poll()
    Check(!outcome.called, "pending network request does not complete early")
    job.started := A_TickCount - 31000
    job.Poll()
    Check(outcome.called && InStr(outcome.error, "timed out") && request.aborted, "network deadline cancels request")
    outcome.called := false
    job := D4Box_UpdateCheck(Done), request := FakeRequest(), job.request := request
    job.Cancel()
    Check(request.aborted && !outcome.called, "closing dialog cancels without late callback")
} catch as err {
    Check(false, err.Message " @" err.File ":" err.Line)
}
FileAppend("done`n", report, "UTF-8")
ExitApp(failures ? 1 : 0)
