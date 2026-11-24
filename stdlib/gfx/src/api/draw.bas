' :::::::::::
' gfx.draw: 2D drawing into surfaces.
'
'     Every call draws into dst, any surface (gfx.surface),
'     within its clip. Points are in dst's world, placed
'     by its camera, and name pixel centres. Angles are
'     radians clockwise. A colour is dst's: an index at 8
'     bits, &HRRGGBB at 15, 16 and 32. The set_* settings
'     hold for every call after.
'
'     Every call raises Illegal function call for a
'     surface or font that is not one, a flag it does not
'     take, BLEND with ADD, a colour dst cannot hold, or a
'     src over dst's pixels: the same surface, or views of
'     one. A GPU cannot draw a texture into itself.
' :::::::::::

' src mirrored left to right, top to bottom.
const FLIP_X = 1
const FLIP_Y = 2
' A shape filled; else its edges.
const FILL = 4
' src's colour 0, or alpha under a half, drawn too.
const OPAQUE = 8
' Mixed with dst by set_alpha's level, or added to it.
const BLEND = 16
const ADD = 32
' src's shape in set_tint's colour.
const SOLID = 64
' src's four texels round each pixel mixed, where they
' fall between pixels.
const SMOOTH = 128
' poly's texels in perspective, by its points' z.
const PERSPECTIVE = 2048
' Drawn only where nearer than dst's depth; dst's depth
' set where drawn.
const DEPTH_TEST = 4096
const DEPTH_WRITE = 8192
' text's colour: the glyphs' own.
const AS_DRAWN = -1

' :::::::::::
' A point.
' :::::::::::
type Vec2
    x as single
    y as single
end type

' :::::::::::
' A polygon's point.
'
'     Attributes:
'       x, y:
'         The point.
'       z:
'         Its distance, 1 or more; used by PERSPECTIVE,
'         DEPTH_TEST and DEPTH_WRITE.
'       u, v:
'         The texel of src drawn there.
' :::::::::::
type Vertex
    x as single
    y as single
    z as single
    u as single
    v as single
end type

' :::::::::::
' Sets the light colours are drawn at.
'
'     Args:
'       level:
'         0 black to 63 the colours as they are.
'
'     Raises:
'       Illegal function call: level out of range.
' :::::::::::
declare sub set_light cdecl (byval level as integer)

' :::::::::::
' Gets the light colours are drawn at.
' :::::::::::
declare function get_light cdecl () as integer

' :::::::::::
' Sets the colour colours are multiplied by.
'
'     Args:
'       colour:
'         &HRRGGBB at every depth; &HFFFFFF none. SOLID
'         draws in it.
'
'     Raises:
'       Illegal function call: colour out of range.
' :::::::::::
declare sub set_tint cdecl (byval colour as long)

' :::::::::::
' Gets the colour colours are multiplied by.
' :::::::::::
declare function get_tint cdecl () as long

' :::::::::::
' Sets how much of a BLEND or ADD draw shows.
'
'     Args:
'       level:
'         0 none to 63 all.
'
'     Raises:
'       Illegal function call: level out of range.
' :::::::::::
declare sub set_alpha cdecl (byval level as integer)

' :::::::::::
' Gets how much of a BLEND or ADD draw shows.
' :::::::::::
declare function get_alpha cdecl () as integer

' :::::::::::
' Sets the palette row 8-bit pictures are drawn in.
'
'     Args:
'       row:
'         0 to 15 (display.set_palette); 0 the
'         pictures' own colours.
'
'     Raises:
'       Illegal function call: no such row.
' :::::::::::
declare sub set_palette_row cdecl (byval row as integer)

' :::::::::::
' Gets the palette row 8-bit pictures are drawn in.
' :::::::::::
declare function get_palette_row cdecl () as integer

' :::::::::::
' Sets text's font.
'
'     Args:
'       f:
'         new_font's; 0 the built-in 8 by 8.
'
'     Raises:
'       Illegal function call: f is not a font.
' :::::::::::
declare sub set_font cdecl (byval f as long)

' :::::::::::
' Gets text's font.
' :::::::::::
declare function get_font cdecl () as long

' :::::::::::
' Sets every setting as at the start: light 63, tint
' &HFFFFFF, alpha 63, palette row 0, font 0.
' :::::::::::
declare sub reset cdecl ()

' :::::::::::
' Makes a font from a sheet of glyphs.
'
'     A character with no glyph draws nothing but takes
'     its width.
'
'     Args:
'       sheet:
'         The surface: glyphs from character 32, row by
'         row from its top left.
'       w, h:
'         A glyph's size.
'
'     Returns:
'       The font.
'
'     Raises:
'       Illegal function call: a glyph bigger than
'       sheet.
'       Out of memory.
' :::::::::::
declare function new_font cdecl ( _
    byval sheet as long, _
    byval w as integer, _
    byval h as integer _
) as long

' :::::::::::
' Frees a font; text then draws in the built-in one if
' it was set_font's.
'
'     Args:
'       f:
'         new_font's.
'
'     Raises:
'       Illegal function call: f is not new_font's.
' :::::::::::
declare sub free_font cdecl (byval f as long)

' :::::::::::
' Fills dst's clip.
'
'     Args:
'       dst:
'         The surface drawn into.
'       colour:
'         The colour.
' :::::::::::
declare sub clear cdecl (byval dst as long, byval colour as long)

' :::::::::::
' Draws a pixel.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         The point.
'       colour:
'         The colour.
' :::::::::::
declare sub pset cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval colour as long _
)

' :::::::::::
' Draws a line, both ends.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x1, y1, x2, y2:
'         Its ends.
'       colour:
'         The colour.
' :::::::::::
declare sub line cdecl ( _
    byval dst as long, _
    byval x1 as single, _
    byval y1 as single, _
    byval x2 as single, _
    byval y2 as single, _
    byval colour as long _
)

' :::::::::::
' Draws a rectangle.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x1, y1, x2, y2:
'         Opposite corners, both drawn.
'       colour:
'         The colour.
'       flags:
'         FILL, BLEND, ADD.
' :::::::::::
declare sub rect cdecl ( _
    byval dst as long, _
    byval x1 as single, _
    byval y1 as single, _
    byval x2 as single, _
    byval y2 as single, _
    byval colour as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a circle.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         Its centre.
'       r:
'         Its radius, 0 or more.
'       colour:
'         The colour.
'       flags:
'         FILL, BLEND, ADD.
'
'     Raises:
'       Illegal function call: r negative.
' :::::::::::
declare sub circle cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval colour as long, _
    byval flags as integer _
)

' :::::::::::
' Draws part of a circle's edge.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         Its centre.
'       r:
'         Its radius, 0 or more.
'       start, finish:
'         Its ends' angles, 0 to the right; it runs
'         clockwise from start to finish.
'       colour:
'         The colour.
'
'     Raises:
'       Illegal function call: r negative.
' :::::::::::
declare sub arc cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval start as single, _
    byval finish as single, _
    byval colour as long _
)

' :::::::::::
' Draws a convex polygon.
'
'     Args:
'       dst:
'         The surface drawn into.
'       points:
'         Its corners, in order round it.
'       first, n:
'         The corners: points(first) on, 3 to 32.
'       src:
'         The surface whose texels fill it, at the
'         points' u, v; 0 none.
'       colour:
'         With no src, the colour.
'       flags:
'         FILL (with no src), OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH, PERSPECTIVE, DEPTH_TEST, DEPTH_WRITE.
'
'     Raises:
'       Illegal function call: n out of range, or a z
'       under 1 where z is used.
'       Subscript out of range: a corner not in points().
' :::::::::::
declare sub poly cdecl ( _
    byval dst as long, _
    points() as Vertex, _
    byval first as integer, _
    byval n as integer, _
    byval src as long, _
    byval colour as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a surface upright at its size.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         Where src's anchor goes.
'       src:
'         The surface drawn.
'       flags:
'         FLIP_X, FLIP_Y, OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH.
' :::::::::::
declare sub sprite cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval src as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a surface turned and scaled about its anchor.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         Where src's anchor goes.
'       angle:
'         Its turn.
'       scale:
'         Its size; 1 its own.
'       src:
'         The surface drawn.
'       flags:
'         FLIP_X, FLIP_Y, OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH.
' :::::::::::
declare sub sprite_turned cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval scale as single, _
    byval src as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a surface stretched onto four corners.
'
'     Args:
'       dst:
'         The surface drawn into.
'       corners:
'         Where src's corners go, clockwise from its
'         top left.
'       first:
'         The first corner: corners(first) to first + 3.
'       src:
'         The surface drawn.
'       flags:
'         FLIP_X, FLIP_Y, OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH.
'
'     Raises:
'       Subscript out of range: a corner not in
'       corners().
' :::::::::::
declare sub quad cdecl ( _
    byval dst as long, _
    corners() as Vec2, _
    byval first as integer, _
    byval src as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a surface repeated along a line of points, its
' top edge on the line.
'
'     Args:
'       dst:
'         The surface drawn into.
'       points:
'         The line's points.
'       first, n:
'         The points: points(first) on, 2 or more.
'       src:
'         The surface drawn.
'       flags:
'         FLIP_X, FLIP_Y, OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH.
'
'     Raises:
'       Illegal function call: n under 2.
'       Subscript out of range: a point not in points().
' :::::::::::
declare sub strip cdecl ( _
    byval dst as long, _
    points() as Vec2, _
    byval first as integer, _
    byval n as integer, _
    byval src as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a grid of surfaces.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         The grid's top left.
'       map:
'         The cells' surfaces row by row; 0 empty.
'       columns:
'         Cells a row.
'       w, h:
'         A cell's size.
'       flags:
'         FLIP_X, FLIP_Y, OPAQUE, BLEND, ADD, SOLID,
'         SMOOTH.
'
'     Raises:
'       Illegal function call: columns, w or h under 1.
' :::::::::::
declare sub tiles cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    map() as long, _
    byval columns as integer, _
    byval w as integer, _
    byval h as integer, _
    byval flags as integer _
)

' :::::::::::
' Draws text in set_font's font.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         The first glyph's top left.
'       s:
'         The text.
'       colour:
'         The colour, or AS_DRAWN.
' :::::::::::
declare sub text cdecl ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval s as string, _
    byval colour as long _
)

' :::::::::::
' Measures text in set_font's font.
'
'     Args:
'       s:
'         The text.
'
'     Returns:
'       Its width in pixels.
' :::::::::::
declare function text_width cdecl (byval s as string) as integer
