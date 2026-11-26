' qxtest: the native ABI's test extension (tests/host/native.sh)
type pt
    x as short
    y as short
end type
declare function add% cdecl (byval a%, byval b%)
declare function half# cdecl (byval d#)
declare function shout$ cdecl alias "shout_text" (byval s$)
declare sub bump cdecl (n%)
declare sub move cdecl (p as pt, byval dx%)
declare function total! cdecl (a() as single)
declare function items% cdecl (a() as single)
declare sub fail cdecl (byval code%)
declare function runs% cdecl ()
declare function audio% cdecl ()
declare function played& cdecl ()

function twice% (n%)
    twice% = add%(n%, n%)
end function
