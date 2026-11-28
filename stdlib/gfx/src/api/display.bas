' :::::::::::
' gfx.display: the screen, the palette and the host.
'
'     The screen is a surface (gfx.surface) the host shows
'     in a window or full screen. The palette is global:
'     row 0 is the colours of every 8-bit surface, the
'     other rows draw.set_palette_row's.
'     docs/gfx-extension-design.md has the design.
' :::::::::::

option disable open, close

' open's flags: VSYNC, and how the screen is scaled to the
' window: whole multiples (none), FIT or STRETCH. NATIVE: a
' GPU draws at the window's own resolution.
const VSYNC = 1
const NATIVE = 2
const FIT = 4
const STRETCH = 8

' features' bits
const HAS_GPU = 1
const HAS_SHADERS = 2
const HAS_SMOOTH = 4

' HostEvent.kind
const RESIZED = 1
const MOVED = 2
const FOCUS = 3
const LOST = 4
const QUIT = 5

' Command.kind: the draw.* or scene.* procedure that drew.
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

' :::::::::::
' A monitor's mode.
'
'     Attributes:
'       w, h:
'         Its size in pixels.
'       bits:
'         Its colour depth.
'       hz:
'         Its refresh rate, rounded.
' :::::::::::
type ScreenMode
    w as integer
    h as integer
    bits as integer
    hz as integer
end type

' :::::::::::
' Something the host saw.
'
'     Attributes:
'       kind:
'         RESIZED: the screen shown at w by h pixels.
'         MOVED: the window moved to monitor.
'         FOCUS: the window focused or not.
'         LOST: the GPU lost its textures. gfx sends
'         images again; what was drawn is drawn again by
'         the program.
'         QUIT: the window was closed.
'       w, h:
'         RESIZED's size.
'       monitor:
'         MOVED's monitor.
'       focused:
'         FOCUS's: -1 focused, 0 not.
' :::::::::::
type HostEvent
    kind as integer
    w as integer
    h as integer
    monitor as integer
    focused as integer
end type

' :::::::::::
' What the frame shown last cost.
'
'     Attributes:
'       draws:
'         Draw calls.
'       batches:
'         A GPU's batches: the calls it drew together.
'       uploads:
'         A GPU's textures sent.
'       waits:
'         A GPU's reads that waited for it to draw.
' :::::::::::
type FrameStats
    draws as long
    batches as long
    uploads as long
    waits as long
end type

' :::::::::::
' One draw call, as drawn: its points after the camera.
'
'     Attributes:
'       kind:
'         DREW_*.
'       target:
'         The surface drawn into.
'       source:
'         The surface drawn from; 0 none.
'       x, y:
'         Its anchor, centre or first point.
'       x2, y2:
'         Its other end, its last point, or the point
'         on its edge where an arc starts.
'       angle, scale:
'         Its turn and size; an arc's start angle.
'       flags, colour:
'         The call's.
'       light, tint, alpha, palette_row:
'         The draw.set_* settings it was drawn with.
' :::::::::::
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
    palette_row as integer
end type

' :::::::::::
' Opens the screen.
'
'     The host may give a nearest size or depth: the
'     surface's width, height and bits say what it gave.
'     An open screen is closed first.
'
'     Args:
'       w, h:
'         The size in pixels.
'       depth:
'         8 (palette indexes), 15, 16 or 32 bits.
'       flags:
'         VSYNC, NATIVE, FIT, STRETCH.
'
'     Returns:
'       The screen's surface.
'
'     Raises:
'       Illegal function call: a size, depth or flag
'       gfx does not have.
'       Advanced feature unavailable: the host has no
'       screen.
' :::::::::::
declare function open cdecl ( _
    byval w as integer, _
    byval h as integer, _
    byval depth as integer, _
    byval flags as integer _
) as long

' :::::::::::
' Closes the screen and frees its surface and views.
' :::::::::::
declare sub close cdecl ()

' :::::::::::
' Shows the screen's surface.
'
'     Raises:
'       Illegal function call: no screen is open.
' :::::::::::
declare sub flip cdecl ()

' :::::::::::
' Counts the monitors.
'
'     Returns:
'       The number of monitors; 0 if the host cannot
'       say.
' :::::::::::
declare function monitors cdecl () as integer

' :::::::::::
' Counts a monitor's modes.
'
'     Args:
'       monitor:
'         1 to monitors().
'
'     Returns:
'       The number of modes.
'
'     Raises:
'       Illegal function call: no such monitor.
' :::::::::::
declare function modes cdecl (byval monitor as integer) as integer

' :::::::::::
' Gets one of a monitor's modes.
'
'     Args:
'       monitor:
'         1 to monitors().
'       n:
'         1 to modes(monitor).
'       m:
'         Set to the mode.
'
'     Raises:
'       Illegal function call: no such monitor.
'       Subscript out of range: no such mode.
' :::::::::::
declare sub get_mode cdecl ( _
    byval monitor as integer, _
    byval n as integer, _
    m as ScreenMode _
)

' :::::::::::
' Shows the screen full screen.
'
'     Args:
'       monitor:
'         1 to monitors().
'       n:
'         The mode, 1 to modes(monitor); 0 the desktop's.
'
'     Raises:
'       Illegal function call: no screen is open, or no
'       such monitor or mode.
'       Advanced feature unavailable: the host cannot.
' :::::::::::
declare sub set_fullscreen cdecl ( _
    byval monitor as integer, _
    byval n as integer _
)

' :::::::::::
' Shows the screen in a window.
'
'     Raises:
'       Illegal function call: no screen is open.
'       Advanced feature unavailable: the host cannot.
' :::::::::::
declare sub set_windowed cdecl ()

' :::::::::::
' Takes the next thing the host saw.
'
'     Args:
'       e:
'         Set to it.
'
'     Returns:
'       Its kind; 0 when none is waiting.
' :::::::::::
declare function poll cdecl (e as HostEvent) as integer

' :::::::::::
' Reads the host's clock.
'
'     Returns:
'       Seconds.
' :::::::::::
declare function clock cdecl () as double

' :::::::::::
' Gets the monitor's refresh rate.
'
'     Returns:
'       Hertz; 0 when unknown.
' :::::::::::
declare function refresh cdecl () as single

' :::::::::::
' Gets what the screen's renderer can do.
'
'     Returns:
'       HAS_* bits.
' :::::::::::
declare function features cdecl () as long

' :::::::::::
' Sets a palette row.
'
'     Args:
'       row:
'         0 to 15.
'       colours:
'         &HRRGGBB each, from colour 0; past 256 unused.
'
'     Raises:
'       Illegal function call: no such row.
' :::::::::::
declare sub set_palette cdecl ( _
    byval row as integer, _
    colours() as long _
)

' :::::::::::
' Gets a palette row.
'
'     Args:
'       row:
'         0 to 15.
'       colours:
'         Set to &HRRGGBB each, from colour 0, as many
'         as it holds up to 256.
'
'     Raises:
'       Illegal function call: no such row.
' :::::::::::
declare sub get_palette cdecl ( _
    byval row as integer, _
    colours() as long _
)

' :::::::::::
' Sets a palette row to an image's palette.
'
'     Args:
'       row:
'         0 to 15.
'       image:
'         A surface surface.load made from an 8-bit
'         image.
'
'     Raises:
'       Illegal function call: no such row, or the
'       image has no palette.
' :::::::::::
declare sub set_palette_from cdecl ( _
    byval row as integer, _
    byval image as long _
)

' :::::::::::
' Makes a colour for the screen.
'
'     Args:
'       r, g, b:
'         0 to 255 each.
'
'     Returns:
'       At 8 bits the nearest index in row 0, else
'       &HRRGGBB.
'
'     Raises:
'       Illegal function call: a part out of range.
' :::::::::::
declare function rgb cdecl ( _
    byval r as integer, _
    byval g as integer, _
    byval b as integer _
) as long

' :::::::::::
' Gets what the frame shown last cost.
'
'     Args:
'       s:
'         Set to it.
' :::::::::::
declare sub get_stats cdecl (s as FrameStats)

' :::::::::::
' Counts the draw calls since the last flip, kept
' up to 32767.
'
'     Returns:
'       The number of calls kept.
' :::::::::::
declare function draws cdecl () as integer

' :::::::::::
' Gets a draw call made since the last flip.
'
'     Args:
'       n:
'         1 to draws(), in the order drawn.
'       d:
'         Set to the call.
'
'     Raises:
'       Subscript out of range: no such call.
' :::::::::::
declare sub get_drawn cdecl (byval n as integer, d as Command)
