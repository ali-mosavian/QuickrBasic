' gfx.pen: where drawing goes and the numbers its flags
' use, for what is drawn after, in 2D and 3D
' (docs/gfx-extension-design.md).

' A draw call's flags: these, gfx.draw's and gfx.scene's.
const OPAQUE = 8
const BLEND = 16
const ADD = 32
const SOLID = 64
const SMOOTH = 128

declare sub target cdecl (byval s as long)
declare sub clip cdecl ( _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
)
declare sub unclip cdecl ()
' 0 to 63, 63 a texel's own.
declare sub light cdecl (byval level as integer)
' Multiplied; SOLID's colour.
declare sub tint cdecl (byval colour as long)
' BLEND's and ADD's: 0 to 63.
declare sub alpha cdecl (byval level as integer)
' A palette row to draw through; 0 none.
declare sub recolor cdecl (byval row as integer)
' text's; 0 the built-in 8 by 8.
declare sub font cdecl (byval handle as long)
declare sub reset cdecl ()
