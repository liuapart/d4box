; Only stable AHK releases are eligible. Old editor releases use another tag prefix.
D4Box_CompareVersion(left, right) {
    pattern := "^(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\z"
    if !RegExMatch(left, pattern) || !RegExMatch(right, pattern)
        throw Error("Invalid stable version")
    a := StrSplit(left, "."), b := StrSplit(right, ".")
    loop 3 {
        if Integer(a[A_Index]) != Integer(b[A_Index])
            return Integer(a[A_Index]) > Integer(b[A_Index]) ? 1 : -1
    }
    return 0
}

D4Box_SelectRelease(releases, best := false) {
    if !(releases is Array)
        throw Error("Expected GitHub release list")
    for release in releases {
        if !(release is Map) || !release.Has("tag_name") || !release.Has("draft") || !release.Has("prerelease")
            throw Error("Invalid GitHub release metadata")
        if release["draft"] != 0 || release["prerelease"] != 0
            continue
        if !RegExMatch(release["tag_name"], "^d4box-v((?:0|[1-9][0-9]{0,3})\.(?:0|[1-9][0-9]{0,3})\.(?:0|[1-9][0-9]{0,3}))\z", &match)
            continue
        version := match[1]
        if !release.Has("assets") || !(release["assets"] is Array)
            continue
        exe := false, sums := false
        for asset in release["assets"] {
            if !(asset is Map) || !asset.Has("name") || !asset.Has("state") || !asset.Has("size")
                continue
            if asset["state"] != "uploaded" || asset["size"] <= 0
                continue
            if asset["name"] = "D4Box-v" version "-windows-x64.exe"
                exe := true
            if asset["name"] = "SHA256SUMS.txt"
                sums := true
        }
        if exe && sums && (!IsObject(best) || D4Box_CompareVersion(version, best.version) > 0)
            ; Construct the only navigable URL locally, never run an API-provided string.
            best := {version: version, url: "https://github.com/" D4Box_Repository() "/releases/tag/d4box-v" version}
    }
    return best
}

; WinHTTP async + polling keeps the AHK GUI and cancellation hotkeys responsive.
class D4Box_UpdateCheck {
    __New(onDone) {
        this.done := onDone, this.page := 0, this.best := false, this.request := false
        this.pollCallback := ObjBindMethod(this, "Poll")
        this.started := A_TickCount
    }

    Start() {
        try this.NextPage()
        catch as err
            this.Finish(false, err.Message)
    }

    NextPage() {
        this.page++
        if this.page > 10
            throw Error("Release listing limit reached")
        this.request := ComObject("WinHttp.WinHttpRequest.5.1")
        this.request.SetTimeouts(5000, 5000, 5000, 10000)
        this.request.Open("GET", "https://api.github.com/repos/" D4Box_Repository() "/releases?per_page=100&page=" this.page, true)
        this.request.SetRequestHeader("Accept", "application/vnd.github+json")
        this.request.SetRequestHeader("User-Agent", "d4box/" D4Box_Version())
        this.request.SetRequestHeader("X-GitHub-Api-Version", "2022-11-28")
        this.request.Send()
        SetTimer(this.pollCallback, 100)
    }

    Poll() {
        try {
            if A_TickCount - this.started > 30000
                throw Error("Request timed out")
            if !this.request.WaitForResponse(0)
                return
            SetTimer(this.pollCallback, 0)
            status := this.request.Status
            if status != 200
                throw Error(status = 403 || status = 429 ? "GitHub rate limit / access denied (HTTP " status ")" : "HTTP " status)
            releases := D4Box_JsonReader.Parse(this.request.ResponseText)
            this.best := D4Box_SelectRelease(releases, this.best)
            if releases.Length = 100
                this.NextPage()
            else
                this.Finish(this.best, "")
        } catch as err
            this.Finish(false, err.Message)
    }

    Cancel() {
        if IsObject(this.pollCallback)
            SetTimer(this.pollCallback, 0)
        if IsObject(this.request)
            try this.request.Abort()
        this.request := false
        ; Release bound callbacks as well: AHK uses reference counting.
        this.pollCallback := false, this.done := false
    }

    Finish(release, errorText) {
        callback := this.done
        this.Cancel()
        if IsObject(callback)
            callback.Call(release, errorText)
    }
}
