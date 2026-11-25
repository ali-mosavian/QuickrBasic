' An event's handler run at a call, an element by reference
' waiting on the stack, frees and makes arrays: its sweep moved
' the element. Stale value. F1, pushed through the keyboard
' controller, fires it at the call: one line, no line's end
' between to fire it sooner.
declare sub show (x as integer, y as integer)

redim shared s1(100) as integer
redim shared a(1 to 3) as integer
redim shared g(1) as integer
a(2) = 22
on key(1) gosub churn
key(1) on
out &h64, &hd2: out &h60, &h3b: show a(2), 7
print a(2)
end

churn:
    erase s1
    redim g(200) as integer
    for i = 0 to 200
        g(i) = -1
    next
    return

sub show (x as integer, y as integer)
    print x; y
    x = 99
end sub
