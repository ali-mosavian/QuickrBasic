' gfx.display: the canvas and the screen it is shown on
' (docs/gfx-extension-design.md). A colour is the
' canvas's: an index at 8 bits, &HRRGGBB at 15, 16 and 32.

' open's flags
const VSYNC = 1
const NATIVE = 2
const FIT = 4
const STRETCH = 8

' caps' bits
const HAS_GPU = 1
const HAS_SHADERS = 2
const HAS_SMOOTH = 4

' HostEvent.kind
const RESIZED = 1
const MOVED = 2
const FOCUS = 3
const LOST = 4
const QUIT = 5

' Command.kind
const DREW_CLEAR = 1
const DREW_PSET = 2
const DREW_LINE = 3
const DREW_RECT = 4
const DREW_CIRCLE = 5
const DREW_ARC = 6
const DREW_POLY = 7
const DREW_SPRITE = 8
const DREW_QUAD = 9
const DREW_TILES = 10
const DREW_STRIP = 11
const DREW_TEXT = 12
const DREW_MESH = 13
const DREW_BILLBOARD = 14

type ScreenMode
    w as integer
    h as integer
    bits as integer
    hz as integer
end type

type HostEvent
    kind as integer
    w as integer
    h as integer
    monitor as integer
    focused as integer
end type

type FrameStats
    draws as long
    batches as long
    uploads as long
    waits as long
end type

' One draw call, after the camera: x, y its anchor, centre or
' first point; x2, y2 its other end, last point, or a point
' on its edge (an arc's start, angle its start's angle).
type Command
    kind as integer
    target as long
    source as long
    x as single
    y as single
    x2 as single
    y2 as single
    angle as single
    scale as single
    flags as integer
    colour as long
    light as integer
    tint as long
    alpha as integer
    recolor as integer
end type

declare sub open cdecl ( _
    byval w as integer, _
    byval h as integer, _
    byval depth as integer, _
    byval flags as integer _
)
declare sub close cdecl ()
declare sub flip cdecl ()
' The monitors.
declare function count cdecl () as integer
declare function modes cdecl (byval monitor as integer) as integer
declare sub mode cdecl ( _
    byval monitor as integer, _
    byval n as integer, _
    m as ScreenMode _
)
' n 0 is the desktop's own mode, 1 on the list's.
declare sub fullscreen cdecl ( _
    byval monitor as integer, _
    byval n as integer _
)
declare sub windowed cdecl ()
' 0 when none is waiting.
declare function poll cdecl (e as HostEvent) as integer
' Seconds, the host's own clock; refresh 0 when unknown.
declare function clock cdecl () as double
declare function refresh cdecl () as single
declare function caps cdecl () as long

' Row 0 is the canvas's; colours are &HRRGGBB.
declare sub palette cdecl ( _
    byval row as integer, _
    colours() as long _
)
' The palette the image was loaded with.
declare sub palette_of cdecl ( _
    byval row as integer, _
    byval source as long _
)
declare function rgb cdecl ( _
    byval r as integer, _
    byval g as integer, _
    byval b as integer _
) as long

' What the frame cost, and what it drew since the last
' flip: drawn's n from 1 to draws.
declare sub stats cdecl (s as FrameStats)
declare function draws cdecl () as integer
declare sub drawn cdecl (byval n as integer, d as Command)
