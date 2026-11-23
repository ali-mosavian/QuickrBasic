' gfx.draw: 2D, into the pen's target
' (docs/gfx-extension-design.md). Coordinates name a
' pixel's centre; angles are radians.

' A draw call's flags: these and gfx.pen's.
const FLIPX = 1
const FLIPY = 2
const FILL = 4

type Vec2
    x as single
    y as single
end type

' x, y the world's point at the target's top left.
declare sub camera cdecl (byval x as single, byval y as single)
' x, y the world's point at the target's centre.
declare sub camera_turned cdecl ( _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval zoom as single _
)

declare sub clear cdecl (byval colour as long)
declare sub pset cdecl ( _
    byval x as single, _
    byval y as single, _
    byval colour as long _
)
declare sub line cdecl ( _
    byval x1 as single, _
    byval y1 as single, _
    byval x2 as single, _
    byval y2 as single, _
    byval colour as long _
)
declare sub rect cdecl ( _
    byval x1 as single, _
    byval y1 as single, _
    byval x2 as single, _
    byval y2 as single, _
    byval colour as long, _
    byval flags as integer _
)
declare sub circle cdecl ( _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval colour as long, _
    byval flags as integer _
)
declare sub arc cdecl ( _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval start as single, _
    byval finish as single, _
    byval colour as long _
)
' Convex.
declare sub poly cdecl ( _
    points() as Vec2, _
    byval first as integer, _
    byval n as integer, _
    byval colour as long, _
    byval flags as integer _
)

declare sub sprite cdecl ( _
    byval s as long, _
    byval x as single, _
    byval y as single, _
    byval flags as integer _
)
declare sub sprite_turned cdecl ( _
    byval s as long, _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval scale as single, _
    byval flags as integer _
)
' The surface onto four corners, from its top left round.
declare sub quad cdecl ( _
    byval s as long, _
    corners() as Vec2, _
    byval first as integer, _
    byval flags as integer _
)
' A cell a surface, 0 empty; columns a row; w by h a cell.
declare sub tiles cdecl ( _
    map() as long, _
    byval columns as integer, _
    byval x as single, _
    byval y as single, _
    byval w as integer, _
    byval h as integer, _
    byval flags as integer _
)
' The surface repeated along the points, hanging below.
declare sub strip cdecl ( _
    byval s as long, _
    points() as Vec2, _
    byval first as integer, _
    byval n as integer, _
    byval flags as integer _
)

' A sheet of glyphs w by h, from 32 on, row by row.
declare function new_font cdecl ( _
    byval sheet as long, _
    byval w as integer, _
    byval h as integer _
) as long
' In the pen's font.
declare sub text cdecl ( _
    byval x as single, _
    byval y as single, _
    byval s as string, _
    byval colour as long _
)
declare function text_width cdecl (byval s as string) as integer
