' 1,000 live dynamic arrays, a record's each, and 500 live
' field cells, a local record's fixed array passed down each
' level. The arrays' headers had a fixed room of about 360:
' Out of memory partway.
type row
    v() as integer
end type
type box
    f(1 to 2) as integer
end type
declare sub down (a() as integer, n as integer)

redim t(1 to 1000) as row
for i = 1 to 1000
    redim t(i).v(1 to 2)
    t(i).v(2) = i
next
s& = 0
for i = 1 to 1000
    s& = s& + t(i).v(2)
next
print s&
dim b as box
down b.f(), 500
print b.f(1)

sub down (a() as integer, n as integer)
    dim r as box
    if n > 0 then down r.f(), n - 1
    a(1) = r.f(1) + 1
end sub
