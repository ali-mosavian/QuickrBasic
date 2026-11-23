' gfx.surface: everything drawn from or into
' (docs/gfx-extension-design.md). A surface is a long; 0 is
' none. A view is a surface over part of another's pixels.

const CANVAS = &H10000000

' In the canvas's format; 8 bits before a screen opens.
declare function new cdecl ( _
    byval w as integer, _
    byval h as integer _
) as long
' An image: a BMP, a PNG.
declare function load cdecl (byval file as string) as long
declare function view cdecl ( _
    byval parent as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
) as long
' views() filled with the parent's w by h cells, row by
' row; how many.
declare function grid cdecl ( _
    byval parent as long, _
    byval w as integer, _
    byval h as integer, _
    views() as long _
) as integer
declare sub aim cdecl ( _
    byval v as long, _
    byval x as integer, _
    byval y as integer _
)
' The point a sprite's x, y names.
declare sub anchor cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer _
)
declare function width cdecl (byval s as long) as integer
declare function height cdecl (byval s as long) as integer
declare function bits cdecl (byval s as long) as integer
' Its views go with it.
declare sub free cdecl (byval s as long)

' Pixels read and written: a GPU waits. The canvas read
' after a flip is the frame shown. point is an index at 8
' bits, else &HRRGGBB; pixels() holds the pixels' bytes, 1,
' 2 or 4 each, little-endian.
declare function point cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer _
) as long
declare sub read cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)
declare sub write cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)
