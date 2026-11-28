' A demo's screen: its size, its centre, and the scale that
' draws a demo the same at any resolution. A demo sizes
' everything in units, a unit a pixel of a 240-line screen.
import gfx.surface, gfx.draw

' the screen a unit is a pixel of: its height
const BASE_H = 240
' the built-in font's height
const GLYPH = 8

' :::::::::::
' A line of text drawn scaled, kept between frames: made
' again only when its text or colours change.
'
'     Attributes:
'       text:
'         The text it holds.
'       colour, shade:
'         The text's colour and its shadow's.
'       surface:
'         The text in colour at scale 1: 0, none yet.
'       shadow:
'         The text in shade: 0, none.
' :::::::::::
type Label
    text as string
    colour as long
    shade as long
    surface as long
    shadow as long
end type

' :::::::::::
' A screen's measures, and its lines of text.
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
'       top, foot:
'         The lines of text at its top and its foot.
' :::::::::::
type Stage
    w as integer
    h as integer
    cx as single
    cy as single
    unit as single
    zoom as integer
    text_h as integer
    top as Label
    foot as Label
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
' Makes a surface of text in the built-in font.
'
'     Args:
'       t:
'         The text.
'       colour:
'         Its colour.
'
'     Returns:
'       The surface, the text's size.
' :::::::::::
function lettered& (byval t as string, byval colour as long)
    s& = gfx.surface.new(gfx.draw.text_width(t), GLYPH)
    gfx.draw.text s&, 0, 0, t, colour
    lettered& = s&
end function

' :::::::::::
' Makes a label hold text in colour, and in shade when
' shaded, unless it already does.
'
'     Args:
'       l:
'         The label.
'       t:
'         The text.
'       colour, shade:
'         The text's colour and its shadow's.
'       shaded:
'         Whether it has a shadow.
' :::::::::::
sub kept ( _
    l as Label, _
    byval t as string, _
    byval colour as long, _
    byval shade as long, _
    byval shaded as integer _
)
    same% = l.surface <> 0 and t = l.text
    same% = same% and colour = l.colour and shade = l.shade
    same% = same% and shaded = (l.shadow <> 0)

    if same% then exit sub

    if l.surface then gfx.surface.free l.surface

    if l.shadow then gfx.surface.free l.shadow

    l.text = t
    l.colour = colour
    l.shade = shade
    l.surface = lettered&(t, colour)
    l.shadow = 0

    if shaded then l.shadow = lettered&(t, shade)
end sub

' :::::::::::
' Draws a surface scaled.
'
'     Args:
'       dst:
'         The surface drawn into.
'       s:
'         The surface drawn.
'       x, y:
'         Its top left in dst.
'       zoom:
'         Its scale, whole.
' :::::::::::
sub stamped ( _
    byval dst as long, _
    byval s as long, _
    byval x as single, _
    byval y as single, _
    byval zoom as integer _
)
    ' the first texel's centre, so each texel covers
    ' whole pixels of dst
    half! = (zoom - 1) / 2
    gfx.draw.sprite_turned dst, x + half!, y + half!, 0, zoom, s, 0
end sub

' :::::::::::
' Draws text in the built-in font, scaled.
'
'     Args:
'       dst:
'         The surface drawn into.
'       l:
'         The text's label, kept for the next frame.
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
    l as Label, _
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

    kept l, t, colour, 0, 0
    stamped dst, l.surface, x, y, zoom
end sub

' :::::::::::
' Draws text over its shadow, so it reads on any background.
'
'     Args:
'       dst:
'         The surface drawn into.
'       l:
'         The text's label, kept for the next frame.
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
    l as Label, _
    byval x as single, _
    byval y as single, _
    byval t as string, _
    byval colour as long, _
    byval shade as long, _
    byval zoom as integer _
)
    if t = "" then exit sub

    if zoom <= 1 then
        gfx.draw.text dst, x + zoom, y + zoom, t, shade
        gfx.draw.text dst, x, y, t, colour
        exit sub
    end if

    kept l, t, colour, shade, -1
    stamped dst, l.shadow, x + zoom, y + zoom, zoom
    stamped dst, l.surface, x, y, zoom
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
