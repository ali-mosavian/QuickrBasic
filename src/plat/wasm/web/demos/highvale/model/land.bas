' The land: its heights and its light, a texel each, and
' after them the same averaged at each halving of the
' map's side. Each level is one wider and taller than its
' side, the last row and column the first again, so a
' texel's right and lower neighbours need no wrapping.
option explicit

' the map's side in texels, a power of two, and its levels
const MAP = 256
const MAP_MASK = MAP - 1
const LEVELS = 5
' a row of the first level
const SIDE = MAP + 1
' the highest a texel stands, in height steps, a step's
' height in texels, and the parts of a step heights hold
const PEAK = 200
const SCALE = 0.5
const STEP_PARTS = 16
' a texel's light, 0 dark to LIGHTS - 1
const LIGHTS = 24

type Land
    heights() as integer
    lights() as byte
    sea as integer
end type
