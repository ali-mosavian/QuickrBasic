' OPTION DIALECT QUICKR
' text: searching, cases, splitting and joining strings
public native function find(s as string, sub as string) as int
public native function find_from(s as string, sub as string, start as int) as int
public native function upper(s as string) as string
public native function lower(s as string) as string
public native function trim(s as string) as string
public native function chr(code as int) as string
public native function code(s as string) as int
public native function hex(n as int) as string
public native function number(s as string) as double

' s as a whole decimal int, an optional - first; anything
' else an Error, Illegal function call (5); past an int,
' Overflow (6)
public function to_int(s as string) as int or Error
    let first = 1 if len(s) > 0 and s[0] = "-"[0] else 0
    if len(s) = first then return Error(5, f"no number: {s}")
    var n as long = 0
    for i in first..len(s)
        let c = s[i]
        if c < "0"[0] or c > "9"[0] then
            return Error(5, f"no number: {s}")
        end if
        n = n * 10 + (c - "0"[0])
        if n > 2147483648 then return Error(6, f"past an int: {s}")
    next
    if first = 1 then return int(-n)
    if n > 2147483647 then return Error(6, f"past an int: {s}")
    return int(n)
end function

' s's parts between seps; one part, s, when sep is not there
public function split(s as string, sep as string) as string[]
    var parts as string[]
    var start = 0
    var at = find(s, sep)
    while at >= 0 and len(sep) > 0
        parts.push(s[start..at])
        start = at + len(sep)
        at = find_from(s, sep, start)
    wend
    parts.push(s[start..])
    return parts
end function

public function join(parts as string[], sep as string) as string
    var b as Builder
    for i in 0..len(parts)
        if i > 0 then b.add(sep)
        b.add(parts[i])
    next
    return b.text()
end function

' a string built in parts: add them, then take text()
public type Builder
    parts as string[]

    fn add(mut self, s as string)
        self.parts.push(s)
    end fn

    fn text(self) as string
        var s = ""
        for p in self.parts
            s = s + p
        next
        return s
    end fn
end type
