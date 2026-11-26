' The palette, laid out by rule: the sky's bands, each
' ground's ramp from shade to light, all of them again
' through each step of the fog, then the panel's and the
' sprites' colours.
option explicit

import gfx.display
from highvale.model.land import LIGHTS

' the sky's bands, top to horizon; the last is the fog
const SKY = 1
const SKY_BANDS = 6
const FOG = SKY + SKY_BANDS - 1
' the grounds, a ramp of LIGHTS tones each, and the fog's
' steps, each a mix of a third more fog
const GROUND = 8
const WATER = 0
const GRASS = 1
const ROCK = 2
const GROUNDS = 3
const FOG_STEPS = 3
' the panel's colours
const PANEL = 224
const PANEL_EDGE = 225
const SHADE = 226
const AMBER = 227
const FAINT = 228
const HULL_BAR = 229
const ALERT = 230
const SCOPE = 231
const SCOPE_GRID = 232
const PALE = 233
const TAPE = 234
const TICK = 235
' the sprites' colours
const METAL = 236
const FOE_DARK = 237
const FOE_RED = 238
const BUNKER_DARK = 239
const BUNKER = 240
const FLAME = 241
const FIRE = 242
const HOT = 243
const SMOKE = 244
const SMOKE_DARK = 245
const TRACER = 246
const WHITE = 247

' hues as &HRRGGBB, six digits each, in index order
const SKY_HUES = "5A86C46C97CC80A8D395B8DAABC8DFC2D6E2"
' each ground's shade and light
const GROUND_HUES = "163A5C6AAAD01C2E16AECC743830289C9080"
const PANEL_HUES = "2B2F2A4A5146171A17F2C14E8C7A457FB069" + _
    "E8743B1F3A2E2A4D3CE6F2D81D2A338FD4C8"
const SPRITE_HUES = "2A2A335B2230A8323A4A3B3F8C6A55E8743B" + _
    "F2A94EFFF2C09A9A925E5E58FFE08AFFFFFF"

' The hue at place n of a list of them.
function hue_of (hues as string, byval n as integer) as long
    hue_of = val("&H" + mid$(hues, n * 6 + 1, 6))
end function

' a's channels toward b's by k parts of n.
function mixed ( _
    byval a as long, _
    byval b as long, _
    byval k as integer, _
    byval n as integer _
) as long
    dim sum as long
    dim shift as long
    dim channel as integer
    dim x as long
    dim y as long

    shift = 1

    for channel = 1 to 3
        x = (a \ shift) and 255
        y = (b \ shift) and 255
        sum = sum + ((x * (n - k) + y * k) \ n) * shift
        shift = shift * 256
    next

    mixed = sum
end function

' The palette index of ground g's tone at light, through
' fog step s.
function ground_tone ( _
    byval g as integer, _
    byval light as integer, _
    byval s as integer _
) as integer
    ground_tone = GROUND + (s * GROUNDS + g) * LIGHTS + light
end function

' Every colour set in the screen's palette.
sub set_colours ()
    dim colours(255) as long
    dim i as integer
    dim g as integer
    dim light as integer
    dim s as integer
    dim fog_hue as long
    dim dark as long
    dim lit as long
    dim tone as long

    for i = 0 to SKY_BANDS - 1
        colours(SKY + i) = hue_of(SKY_HUES, i)
    next

    fog_hue = colours(FOG)

    for g = 0 to GROUNDS - 1
        dark = hue_of(GROUND_HUES, g * 2)
        lit = hue_of(GROUND_HUES, g * 2 + 1)

        for light = 0 to LIGHTS - 1
            tone = mixed(dark, lit, light, LIGHTS - 1)

            for s = 0 to FOG_STEPS - 1
                colours(ground_tone(g, light, s)) = _
                    mixed(tone, fog_hue, s, FOG_STEPS)
            next
        next
    next

    for i = 0 to 11
        colours(PANEL + i) = hue_of(PANEL_HUES, i)
        colours(METAL + i) = hue_of(SPRITE_HUES, i)
    next

    gfx.display.set_palette 0, colours()
end sub
