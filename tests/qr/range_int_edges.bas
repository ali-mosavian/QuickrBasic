option dialect quickr
' chains over ranges at an int's ends: an empty range ending at its
' first value, and a window or zip wider than an int, stopped with
' Overflow (the end less one, the span) before the fix
let low = -2147483647 - 1
let hi = 2147483647
print (0..low).count(); " "; (low..low).rev().count()
print (low..low + 2).rev().collect()[1]
for i in (0..low).rev()
    print i
next
for i in (low..low).rev()
    print i
next
print (0..3).zip(low..low).count()
for x, y in (0..3).zip(hi - 2..hi)
    print x; " "; y
next
for i in (low..hi).take(2).rev()
    print i
next
for i in (low..hi).skip(hi).rev().take(2)
    print i
next
for x, y in (0..2).zip(low..hi).rev()
    print x; " "; y
next
for x, y in (low..low + 2).zip(low..low).rev()
    print x; " "; y
next
print (hi - 1..hi).rev().count(); " "; (low..low + 1).count()
print "done"
