' A demo's screen: its size, its centre, and the scale that
' draws a demo the same at any resolution. A demo sizes
' everything in units, a unit a pixel of a 240-line screen.
import gfx.surface, gfx.draw

' the screen a unit is a pixel of: its height
const BASE_H = 240
' the built-in font's height
const GLYPH = 8

' :::::::::::
' A screen's measures.
'
'     Attributes:
'       w, h:
'         Its size in pixels.
'       cx, cy:
'         Its centre.
'       unit:
'         A unit in its pixels.
'       zoom:
'         Text's scale: unit, whole, at least 1.
'       text_h:
'         A line of text's height at zoom.
' :::::::::::
type Stage
    w as integer
    h as integer
    cx as single
    cy as single
    unit as single
    zoom as integer
    text_h as integer
end type

' :::::::::::
' Measures a surface.
'
'     Args:
'       dst:
'         The surface, as a rule the screen's.
'       st:
'         Set to its measures.
' :::::::::::
sub measure (byval dst as long, st as Stage)
    st.w = gfx.surface.width(dst)
    st.h = gfx.surface.height(dst)
    st.cx = st.w / 2
    st.cy = st.h / 2
    least% = st.w

    if st.h < least% then least% = st.h

    st.unit = least% / BASE_H
    st.zoom = int(st.unit)

    if st.zoom < 1 then st.zoom = 1

    st.text_h = GLYPH * st.zoom
end sub

' :::::::::::
' Draws text in the built-in font, scaled.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         The text's top left.
'       t:
'         The text.
'       colour:
'         Its colour.
'       zoom:
'         Its scale, whole.
' :::::::::::
sub label ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval t as string, _
    byval colour as long, _
    byval zoom as integer _
)
    if t = "" then exit sub

    if zoom <= 1 then
        gfx.draw.text dst, x, y, t, colour
        exit sub
    end if

    s& = gfx.surface.new(gfx.draw.text_width(t), GLYPH)
    gfx.draw.text s&, 0, 0, t, colour
    ' the first texel's centre, so each texel covers
    ' whole pixels of dst
    half! = (zoom - 1) / 2
    gfx.draw.sprite_turned dst, x + half!, y + half!, 0, zoom, s&, 0
    gfx.surface.free s&
end sub

' :::::::::::
' Draws text over its shadow, so it reads on any picture.
'
'     Args:
'       dst:
'         The surface drawn into.
'       x, y:
'         The text's top left.
'       t:
'         The text.
'       colour:
'         Its colour.
'       shade:
'         Its shadow's, a pixel of zoom down and right.
'       zoom:
'         Its scale, whole.
' :::::::::::
sub label_shaded ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval t as string, _
    byval colour as long, _
    byval shade as long, _
    byval zoom as integer _
)
    label dst, x + zoom, y + zoom, t, shade, zoom
    label dst, x, y, t, colour, zoom
end sub

' :::::::::::
' Names a setting's state.
'
'     Args:
'       v:
'         The setting: 0 off, else on.
'
'     Returns:
'       "on" or "off".
' :::::::::::
function on_off$ (byval v as integer)
    on_off$ = "off"

    if v then on_off$ = "on"
end function
