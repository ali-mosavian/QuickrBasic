' ext: the prototype extension's interface
type level
    now as short
    peak as short
end type
declare function add% cdecl (byval a%, byval b%)
declare sub hello cdecl ()
declare sub loud cdecl (byval n%, l as level)
declare sub bump cdecl (n%)
declare function shout$ cdecl alias "shout_text" (byval s$)
