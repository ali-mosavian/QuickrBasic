' OPTION DIALECT QUICKR
' select case, as QBasic writes it; match names it
enum Mode
    walk
    jump
end enum
let m = Mode.walk
match m
    case walk
        print(1)
    case jump
        print(2)
end match
select m
    case else
        print(3)
end select
select case m
    case fly
        print(4)
end select
