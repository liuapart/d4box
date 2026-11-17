; Strict, bounded JSON reader for GitHub release metadata. Never evaluates code.
class D4Box_JsonReader {
    static Parse(text) {
        if StrLen(text) > 4000000
            throw Error("JSON response too large")
        reader := D4Box_JsonReader()
        reader.text := text, reader.pos := 1
        value := reader.Value(0)
        reader.White()
        if reader.pos <= StrLen(text)
            throw Error("Unexpected JSON suffix")
        return value
    }

    White() {
        while this.pos <= StrLen(this.text) && InStr(" `t`r`n", SubStr(this.text, this.pos, 1))
            this.pos++
    }

    Value(depth) {
        if depth > 64
            throw Error("JSON nesting limit")
        this.White()
        ch := SubStr(this.text, this.pos, 1)
        if ch = '"'
            return this.String()
        if ch = "{" || ch = "[" {
            object := ch = "{", result := object ? Map() : []
            close := object ? "}" : "]"
            this.pos++, this.White()
            if SubStr(this.text, this.pos, 1) = close {
                this.pos++
                return result
            }
            loop {
                if object {
                    this.White()
                    key := this.String()
                    this.White()
                    if SubStr(this.text, this.pos++, 1) != ":" || result.Has(key)
                        throw Error("Invalid JSON object")
                    result[key] := this.Value(depth + 1)
                } else
                    result.Push(this.Value(depth + 1))
                this.White()
                next := SubStr(this.text, this.pos++, 1)
                if next = close
                    return result
                if next != ","
                    throw Error("Invalid JSON separator")
            }
        }
        for literal, value in Map("true", true, "false", false, "null", "") {
            if SubStr(this.text, this.pos, StrLen(literal)) == literal {
                this.pos += StrLen(literal)
                return value
            }
        }
        if RegExMatch(SubStr(this.text, this.pos), "^-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", &match) {
            this.pos += StrLen(match[0])
            return Number(match[0])
        }
        throw Error("Invalid JSON value")
    }

    String() {
        if SubStr(this.text, this.pos++, 1) != '"'
            throw Error("Expected JSON string")
        value := ""
        while this.pos <= StrLen(this.text) {
            ch := SubStr(this.text, this.pos++, 1)
            if ch = '"'
                return value
            if Ord(ch) < 32
                throw Error("JSON string contains control character")
            if ch != "\" {
                value .= ch
                continue
            }
            escape := SubStr(this.text, this.pos++, 1)
            escapes := Map('"', '"', "\", "\", "/", "/", "b", Chr(8), "f", Chr(12), "n", "`n", "r", "`r", "t", "`t")
            if escapes.Has(escape) {
                value .= escapes[escape]
                continue
            }
            if !(escape == "u")
                throw Error("Invalid JSON escape")
            code := this.Hex4()
            if code >= 0xD800 && code <= 0xDBFF {
                if SubStr(this.text, this.pos, 2) != "\u"
                    throw Error("Missing JSON low surrogate")
                this.pos += 2
                low := this.Hex4()
                if low < 0xDC00 || low > 0xDFFF
                    throw Error("Invalid JSON low surrogate")
                code := 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00
            } else if code >= 0xDC00 && code <= 0xDFFF
                throw Error("Unexpected JSON low surrogate")
            value .= Chr(code)
        }
        throw Error("Unterminated JSON string")
    }

    Hex4() {
        hex := SubStr(this.text, this.pos, 4)
        if !RegExMatch(hex, "^[0-9a-fA-F]{4}$")
            throw Error("Invalid JSON unicode escape")
        this.pos += 4
        return Integer("0x" hex)
    }
}
