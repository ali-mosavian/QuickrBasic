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

' s's parts between seps; one part, s, when sep is not there
public function split(s as string, sep as string) as string[]
    var parts as string[]
    var start = 0
    var at = find(s, sep)
    while at >= 0 and len(sep) > 0
        parts.push(s[start..at])
        start = at + len(sep)
        at = find_from(s, sep, start)
    end while
    parts.push(s[start..])
    return parts
end function

public function join(parts as string[], sep as string) as string
    var b as Builder
    for i in 0..len(parts)
        if i > 0 then b.add(sep)
        b.add(parts[i])
    end for
    return b.text()
end function

' a string built in parts: add them, then take text()
public type Builder
    parts as string[]
end type

public function add(mut b as Builder, s as string)
    b.parts.push(s)
end function

public function text(b as Builder) as string
    var s = ""
    for p in b.parts
        s = s + p
    end for
    return s
end function
