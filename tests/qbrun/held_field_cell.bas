' A fixed array field passed whole, then an argument whose
' FUNCTION frees and makes arrays: its sweep moved the field
' cell of the call being made. Segfault.
type rec
    v(1 to 3) as integer
end type
declare sub show (x() as integer, y as integer)
declare function churn% ()

dim shared r as rec
redim shared e(100) as integer
redim shared g(1) as integer
r.v(1) = 11
r.v(2) = 22
r.v(3) = 33
show r.v(), churn%
print "done"

sub show (x() as integer, y as integer)
    print lbound(x); ubound(x); y
    for i = lbound(x) to ubound(x)
        print x(i);
    next
    print
end sub

function churn%
    erase e
    redim g(200) as integer
    for i = 0 to 200
        g(i) = -1
    next
    churn% = 7
end function
