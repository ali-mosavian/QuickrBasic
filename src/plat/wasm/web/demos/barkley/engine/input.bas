' The keyboard's port: the keys held.
defint A-Z
option explicit

' The keys held and since when; the last scan code, and whether the keyboard's port answers.
type Keys
    held(127) as integer
    t(127) as single
    last as integer
    port as integer
end type

' The keyboard port's last scan code: a press sets kb.held, a release clears.
sub Poll (kb as Keys)
    dim s as integer
    s = inp(&H60)
    if s = kb.last then exit sub
    kb.last = s
    if s = 255 or s = 224 then exit sub
    kb.port = -1
    if s and 128 then
        kb.held(s and 127) = 0
    else
        kb.held(s) = -1
    end if
end sub

' Key s held: by the port, or, without it, typed within a repeat's wait.
function Held2% (kb as Keys, s as integer, t as single)
    if kb.port then
        Held2% = kb.held(s)
    else
        Held2% = t - kb.t(s) < .55 and kb.t(s) > 0
    end if
end function
