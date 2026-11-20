' A data file read: its LONGs and its strings.
defint A-Z
option explicit

' CHARLIE.DAT open as #1: where its tunes and its levels start, then its pictures.
sub OpenData
    open "CHARLIE.DAT" for binary as #1
    if lof(1) = 0 then
        close #1
        kill "CHARLIE.DAT"
        screen 0
        print "CHARLIE.DAT, the game's pictures and tunes, must be beside CHARLIE.BAS."
        end
    end if
end sub

' CHARLIE.DAT's next LONG.
function Word&
    dim v as long
    get #1, , v
    Word& = v
end function

' CHARLIE.DAT's next string: its length, then its characters.
function Chars$
    dim s as string
    s = space$(Word&)
    if len(s) then get #1, , s
    Chars$ = s
end function
