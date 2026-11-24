' OPTION DIALECT QUICKR
' keys: the keyboard, as INKEY$ reads it, and the mouse, as PEN does
' The platforms report presses and their repeats, never a release: down(k) holds for
' held_for seconds after k's last press or repeat.
import time

native fn inkey() as string
native fn pen(n as int) as int

public const held_for = 0.55

public enum Key
    escape
    enter
    space
    tab
    backspace
    up
    down
    left
    right
    a
    b
    c
    d
    e
    f
    g
    h
    i
    j
    k
    l
    m
    n
    o
    p
    q
    r
    s
    t
    u
    v
    w
    x
    y
    z
end enum

' a key's code as next() gives it: a character's, 256 + an extended key's scan code
public fn code(k as Key) as int
    match k
        case Key.escape
            return 27
        case Key.enter
            return 13
        case Key.space
            return 32
        case Key.tab
            return 9
        case Key.backspace
            return 8
        case Key.up
            return 256 + 72
        case Key.down
            return 256 + 80
        case Key.left
            return 256 + 75
        case Key.right
            return 256 + 77
        case else
            return 97 + int(k) - int(Key.a)
    end match
end fn

var waiting as int[]
var seen as double[512]
var fresh as bool[512]

' the keys typed since the last look, taken
fn poll()
    var s = inkey()
    while len(s) > 0
        var c = int(s[0])
        if len(s) = 2 then c = 256 + int(s[1])
        if c >= 65 and c <= 90 then c += 32
        waiting.push(c)
        seen[c] = time.ticks()
        fresh[c] = true
        s = inkey()
    end while
end fn

' the next key's code, 0 none waiting
public fn next() as int
    poll()
    if len(waiting) = 0 then return 0
    let c = waiting[0]
    waiting.remove(0)
    return c
end fn

' k pressed since the last pressed(k)
public fn pressed(k as Key) as bool
    poll()
    let c = code(k)
    let was = fresh[c]
    fresh[c] = false
    return was
end fn

' k held: pressed or repeated in the last held_for seconds
public fn down(k as Key) as bool
    poll()
    let c = code(k)
    if seen[c] = 0 then return false
    var d = time.ticks() - seen[c]
    if d < 0 then d += 86400
    return d < held_for
end fn

' the mouse: its pixel, its buttons down now
public type Mouse
    x as int
    y as int
    left as bool
    right as bool
end type

public fn mouse() as Mouse
    return Mouse(x: pen(4), y: pen(5), left: pen(3) <> 0, right: pen(13) <> 0)
end fn

' the left button, or the right, pressed since the last clicked() of it
public fn clicked(right as bool = false) as bool
    if right then return pen(10) <> 0
    return pen(0) <> 0
end fn
