' gfx: 2D and 3D drawing (docs/gfx-extension-design.md).
' QuickrBasic's dialect: line, circle, open are names here.
' A handle is a long; 0 is none. Everything drawn from or
' into is a surface. Coordinates name a pixel's centre;
' angles are radians. A colour is the canvas's: an index at
' 8 bits, &HRRGGBB at 15, 16 and 32.

const CANVAS = &H10000000

' open's flags
const VSYNC = 1
const NATIVE = 2
const FIT = 4
const STRETCH = 8

' a draw call's flags
const FLIPX = 1
const FLIPY = 2
const OPAQUE = 4
const BLEND = 8
const ADD = 16
const SOLID = 32
const SMOOTH = 64
const FILL = 128
const ORDERED = 256
const NO_ZWRITE = 512
const NO_ZTEST = 1024

' new_material's kinds
const FLAT = 1
const TEXTURED = 2
const VERTEX_LIT = 3
const LIGHTMAPPED = 4
const WARPED = 5
const SKY = 6

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
const DREW_MODEL = 14
const DREW_BILLBOARD = 15

type Vec2
    x as single
    y as single
end type

type Vec3
    x as single
    y as single
    z as single
end type

' u, v the texture's texels, u2, v2 the lightmap's; light 0
' to 63, 63 the texel's own; tint multiplied.
type Vertex
    x as single
    y as single
    z as single
    u as single
    v as single
    u2 as single
    v2 as single
    light as single
    tint as long
end type

' Row vectors: v' = v * M, the move in m41 to m43, as qgl's.
type Matrix
    m11 as single
    m12 as single
    m13 as single
    m14 as single
    m21 as single
    m22 as single
    m23 as single
    m24 as single
    m31 as single
    m32 as single
    m33 as single
    m34 as single
    m41 as single
    m42 as single
    m43 as single
    m44 as single
end type

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
    display as integer
    focused as integer
end type

type FrameStats
    draws as long
    batches as long
    uploads as long
    waits as long
end type

' One draw call, after the camera: x, y its anchor or first
' end, x2, y2 its other end.
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

' --- the display ---

declare sub open cdecl ( _
    byval w as integer, _
    byval h as integer, _
    byval depth as integer, _
    byval flags as integer _
)
declare sub close cdecl ()
declare function displays cdecl () as integer
declare function modes cdecl (byval display as integer) as integer
declare sub mode cdecl ( _
    byval display as integer, _
    byval n as integer, _
    m as ScreenMode _
)
' n 0 is the desktop's own mode, 1 on the list's.
declare sub fullscreen cdecl ( _
    byval display as integer, _
    byval n as integer _
)
declare sub windowed cdecl ()
declare sub flip cdecl ()
' 0 when none is waiting.
declare function poll cdecl (e as HostEvent) as integer
' Seconds, the host's own clock; refresh 0 when unknown.
declare function clock cdecl () as double
declare function refresh cdecl () as single

' --- surfaces ---

declare function new_surface cdecl ( _
    byval w as integer, _
    byval h as integer _
) as long
declare function load_surface cdecl (byval file as string) as long
' The file's sheet, and views() filled with its pictures.
declare function load_atlas cdecl ( _
    byval file as string, _
    views() as long _
) as long
declare function new_view cdecl ( _
    byval parent as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
) as long
declare sub aim cdecl ( _
    byval v as long, _
    byval x as integer, _
    byval y as integer _
)
' The point a sprite's x, y names.
declare sub anchor cdecl ( _
    byval surface as long, _
    byval x as integer, _
    byval y as integer _
)
declare function width cdecl (byval surface as long) as integer
declare function height cdecl (byval surface as long) as integer
declare function bits cdecl (byval surface as long) as integer
' A surface's views go with it.
declare sub free cdecl (byval handle as long)

' --- pixels read and written: a GPU waits ---

declare function point cdecl ( _
    byval surface as long, _
    byval x as integer, _
    byval y as integer _
) as long
' The canvas read after flip is the frame shown.
declare sub read cdecl ( _
    byval surface as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)
declare sub write cdecl ( _
    byval surface as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer, _
    pixels() as byte _
)

' --- colour ---

' Row 0 is the canvas's; colours are &HRRGGBB.
declare sub palette cdecl ( _
    byval row as integer, _
    colours() as long _
)
declare function rgb cdecl ( _
    byval r as integer, _
    byval g as integer, _
    byval b as integer _
) as long

' --- the pen: for what is drawn after ---

declare sub target cdecl (byval surface as long)
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
' x, y the world's point at the target's top left.
declare sub camera cdecl (byval x as single, byval y as single)
' x, y the world's point at the target's centre.
declare sub camera_turned cdecl ( _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval zoom as single _
)
declare sub camera_3d cdecl ( _
    eye as Vec3, _
    at as Vec3, _
    up as Vec3, _
    byval fov as single _
)
' camera_3d's planes: 1 and 4096 till set.
declare sub depth_range cdecl ( _
    byval near as single, _
    byval far as single _
)
declare sub reset cdecl ()

' --- 2D ---

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
    byval surface as long, _
    byval x as single, _
    byval y as single, _
    byval flags as integer _
)
declare sub sprite_turned cdecl ( _
    byval surface as long, _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval scale as single, _
    byval flags as integer _
)
' The surface onto four corners, from its top left round.
declare sub quad cdecl ( _
    byval surface as long, _
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
    byval surface as long, _
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
declare sub text cdecl ( _
    byval x as single, _
    byval y as single, _
    byval s as string, _
    byval colour as long _
)
declare function text_width cdecl (byval s as string) as integer

' --- 3D: depth tested and written unless a flag says ---

' tris: three vertex numbers a triangle, from 0.
declare function new_mesh cdecl ( _
    v() as Vertex, _
    tris() as integer _
) as long
' v: frames of vertices end to end.
declare function new_model cdecl ( _
    v() as Vertex, _
    tris() as integer, _
    byval frames as integer _
) as long
declare function new_material cdecl ( _
    byval kind as integer, _
    byval texture as long, _
    byval lightmap as long _
) as long
declare sub mesh cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval flags as integer _
)
' frame 3.25: a quarter of the way from 3 to 4.
declare sub model cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval frame as single, _
    where as Matrix, _
    byval flags as integer _
)
declare sub billboard cdecl ( _
    byval surface as long, _
    at as Vec3, _
    byval scale as single, _
    byval flags as integer _
)
' m from a position, its yaw, pitch and roll, and a scale.
declare sub place cdecl ( _
    m as Matrix, _
    at as Vec3, _
    turn as Vec3, _
    byval scale as single _
)
' m = a then b.
declare sub join cdecl (m as Matrix, a as Matrix, b as Matrix)
declare sub clear_depth cdecl ()

' --- what the frame cost, and what it drew ---

declare sub stats cdecl (s as FrameStats)
declare function caps cdecl () as long
declare function cost cdecl (byval kind as integer) as integer
declare function draws cdecl () as integer
declare sub drawn cdecl (byval n as integer, d as Command)
