' :::::::::::
' gfx.surface: everything drawn from or into.
'
'     A surface is a long; 0 is none. The screen is one
'     (display.open's), an image is one, and a view is
'     one over part of another's pixels, its parent's,
'     with no memory of its own. Each has its own clip,
'     camera and anchor, and a depth for DEPTH_TEST and
'     DEPTH_WRITE draws. Its format is its root's: the
'     screen's, an image's, or new's.
' :::::::::::

' :::::::::::
' Makes a surface, all colour 0.
'
'     Args:
'       w, h:
'         The size in pixels, 1 to 16384.
'
'     Returns:
'       The surface, in the screen's format; 8 bits
'       when no screen is open.
'
'     Raises:
'       Illegal function call: a size out of range.
'       Out of memory.
' :::::::::::
declare function new cdecl ( _
    byval w as integer, _
    byval h as integer _
) as long

' :::::::::::
' Loads an image.
'
'     Args:
'       file:
'         A BMP or a PNG.
'
'     Returns:
'       The surface, in the image's format; an 8-bit one
'       keeps its palette (display.set_palette_from).
'
'     Raises:
'       File not found.
'       Illegal function call: not an image gfx reads.
'       Out of memory.
' :::::::::::
declare function load cdecl (byval file as string) as long

' :::::::::::
' Makes a view over part of a surface.
'
'     Args:
'       parent:
'         The surface whose pixels it shows.
'       x, y, w, h:
'         Its rectangle in parent.
'
'     Returns:
'       The view.
'
'     Raises:
'       Illegal function call: the rectangle is not
'       inside parent.
'       Out of memory.
' :::::::::::
declare function new_view cdecl ( _
    byval parent as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
) as long

' :::::::::::
' Cuts a surface into views, a cell each.
'
'     Args:
'       parent:
'         The surface cut.
'       w, h:
'         A cell's size.
'       views:
'         Set to the cells' views, row by row from the
'         top left.
'
'     Returns:
'       The number of views.
'
'     Raises:
'       Illegal function call: a cell bigger than parent.
'       Subscript out of range: views() too small.
'       Out of memory.
' :::::::::::
declare function new_views cdecl ( _
    byval parent as long, _
    byval w as integer, _
    byval h as integer, _
    views() as long _
) as integer

' :::::::::::
' Frees a surface and its views.
'
'     Args:
'       s:
'         The surface.
'
'     Raises:
'       Illegal function call: s is the screen's;
'       display.close frees it.
' :::::::::::
declare sub free cdecl (byval s as long)

' :::::::::::
' Moves a view within its parent.
'
'     Args:
'       v:
'         The view.
'       x, y:
'         Its top left in its parent.
'
'     Raises:
'       Illegal function call: v is not a view, or would
'       not lie inside its parent.
' :::::::::::
declare sub set_view cdecl ( _
    byval v as long, _
    byval x as integer, _
    byval y as integer _
)

' :::::::::::
' Sets the point of a surface that draw.sprite places.
'
'     Args:
'       s:
'         The surface.
'       x, y:
'         The point in its pixels; 0, 0 at first.
' :::::::::::
declare sub set_anchor cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer _
)

' :::::::::::
' Gets a surface's anchor.
'
'     Args:
'       s:
'         The surface.
'       x, y:
'         Set to the point.
' :::::::::::
declare sub get_anchor cdecl ( _
    byval s as long, _
    x as integer, _
    y as integer _
)

' :::::::::::
' Sets the rectangle drawing into a surface keeps to.
'
'     Args:
'       s:
'         The surface.
'       x, y, w, h:
'         The rectangle in its pixels; the part outside
'         the surface is ignored.
'
'     Raises:
'       Illegal function call: w or h negative.
' :::::::::::
declare sub set_clip cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
)

' :::::::::::
' Gets the rectangle drawing into a surface keeps to.
'
'     Args:
'       s:
'         The surface.
'       x, y, w, h:
'         Set to the rectangle, inside the surface.
' :::::::::::
declare sub get_clip cdecl ( _
    byval s as long, _
    x as integer, _
    y as integer, _
    w as integer, _
    h as integer _
)

' :::::::::::
' Lets drawing into a surface cover all of it.
'
'     Args:
'       s:
'         The surface.
' :::::::::::
declare sub unclip cdecl (byval s as long)

' :::::::::::
' Sets a surface's depth to far.
'
'     Args:
'       s:
'         The surface.
'
'     Raises:
'       Out of memory.
' :::::::::::
declare sub clear_depth cdecl (byval s as long)

' :::::::::::
' Sets the world's point at a surface's top left.
'
'     Points drawn into the surface are in the world;
'     0, 0 at first.
'
'     Args:
'       s:
'         The surface.
'       x, y:
'         The point.
' :::::::::::
declare sub set_camera cdecl ( _
    byval s as long, _
    byval x as single, _
    byval y as single _
)

' :::::::::::
' Sets the world's point at a surface's centre, the
' camera turned and zoomed about it.
'
'     Args:
'       s:
'         The surface.
'       x, y:
'         The point.
'       angle:
'         The camera's turn, radians clockwise.
'       zoom:
'         The world's scale; 1 its own size.
'
'     Raises:
'       Illegal function call: zoom not above 0.
' :::::::::::
declare sub set_camera_turned cdecl ( _
    byval s as long, _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval zoom as single _
)

' :::::::::::
' Gets a surface's width in pixels.
' :::::::::::
declare function width cdecl (byval s as long) as integer

' :::::::::::
' Gets a surface's height in pixels.
' :::::::::::
declare function height cdecl (byval s as long) as integer

' :::::::::::
' Gets a surface's bits a pixel: 8, 15, 16 or 32.
' :::::::::::
declare function bits cdecl (byval s as long) as integer

' :::::::::::
' Reads a pixel.
'
'     A GPU first finishes drawing. The screen read after
'     a flip is the frame shown.
'
'     Args:
'       s:
'         The surface.
'       x, y:
'         The pixel.
'
'     Returns:
'       An index at 8 bits, else &HRRGGBB.
'
'     Raises:
'       Illegal function call: the pixel is outside s.
' :::::::::::
declare function point cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer _
) as long

' :::::::::::
' Reads a rectangle of pixels.
'
'     Args:
'       s:
'         The surface.
'       x, y, w, h:
'         The rectangle.
'       pixels:
'         Set to its bytes row by row: 1, 2 or 4 a
'         pixel, little-endian.
'
'     Raises:
'       Illegal function call: the rectangle is not
'       inside s.
'       Subscript out of range: pixels() too small.
' :::::::::::
declare sub read cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)

' :::::::::::
' Writes a rectangle of pixels.
'
'     Args:
'       s:
'         The surface.
'       x, y, w, h:
'         The rectangle.
'       pixels:
'         Its bytes row by row, as read's.
'
'     Raises:
'       Illegal function call: the rectangle is not
'       inside s.
'       Subscript out of range: pixels() too small.
' :::::::::::
declare sub write cdecl ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)
