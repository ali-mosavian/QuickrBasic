' Addresses into the heap on the stack while a FUNCTION later
' in the statement frees and makes arrays: its sweep moved
' what they point at. Stale values: an element by reference,
' one passed on from a parameter, a field's element stored
' to, a record copied.
type rec
    v(1 to 3) as integer
end type
declare sub show (x as integer, y as integer)
declare sub again (x as integer)
declare function churn% (n as integer)

redim shared s1(100) as integer
redim shared s2(100) as integer
redim shared s3(100) as integer
redim shared s4(100) as integer
redim shared a(1 to 3) as integer
redim shared r(1 to 2) as rec
redim shared g(1) as integer
dim shared turn as integer
a(2) = 22
a(3) = 33
show a(2), churn%(7)
print a(2)
again a(3)
print a(3)
r(1).v(churn%(3)) = 6
print r(1).v(3)
r(1).v(2) = 5
r(2) = r(churn%(1))
print r(2).v(2); r(2).v(3)

sub show (x as integer, y as integer)
    print x; y
    x = 99
end sub

sub again (x as integer)
    show x, churn%(8)
end sub

' The arrays before a and r freed, one a call: they move.
function churn% (n as integer)
    turn = turn + 1
    select case turn
        case 1
            erase s1
        case 2
            erase s2
        case 3
            erase s3
        case else
            erase s4
    end select
    redim g(200) as integer
    for i = 0 to 200
        g(i) = -1
    next
    churn% = n
end function
