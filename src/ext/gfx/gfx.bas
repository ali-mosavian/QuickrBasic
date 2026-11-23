' gfx: 2D and 3D drawing (docs/gfx-extension-design.md).
' Native dialect: line, circle, open are names here.
' A handle is a long; 0 is none. Coordinates name a
' pixel's centre; angles are radians.

const CANVAS = &H10000000

' open's flags
const VSYNC = 1
const NATIVE = 2
const SMOOTH = 4

' sprite's flags
const FLIPX = 1
const FLIPY = 2

' blend's modes
const OPAQUE = 0
const KEYED = 1
const TRANSLUCENT = 2
const ADDITIVE = 3

' an atlas's pixels
const INDEXED = 8
const DIRECT = 32

' depth's modes, as uglZMode's
const Z_OFF = 0
const Z_WRITE = 1
const Z_TEST = 2

' order's
const ANY_ORDER = 0
const FRONT_FIRST = 1

' load_matrix's
const MODEL_MATRIX = 0
const VIEW_MATRIX = 1
const PROJECTION_MATRIX = 2

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
const DREW_SPRITE = 1
const DREW_TILE = 2
const DREW_STRIP = 3
const DREW_POLY = 4
const DREW_LINE = 5
const DREW_RECT = 6
const DREW_CIRCLE = 7
const DREW_TEXT = 8
const DREW_MESH = 9
const DREW_MODEL = 10
const DREW_CLEAR = 11

' the built-in materials
const FLAT = &H60000001
const TEXTURED = &H60000002
const VERTEX_LIT = &H60000003
const LIGHTMAPPED = &H60000004
const WARPED = &H60000005
const SKY = &H60000006

' Stage.op
const READ_TEXTURE = 1
const READ_WARPED = 2
const READ_LIGHT = 3
const READ_PALETTE = 4
const MUL_TEXTURE = 5
const MUL_COLOUR = 6
const ADD_COLOUR = 7
const KEY = 8

type Vec2
    x as single
    y as single
end type

type Vec3
    x as single
    y as single
    z as single
end type

' u and v in texels; light 0 to 63, 63 the texel's own;
' tint &HRRGGBB, multiplied.
type Vertex
    x as single
    y as single
    z as single
    u as single
    v as single
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

' anchor: the point a sprite's x, y names.
type PicInfo
    w as integer
    h as integer
    anchor_x as integer
    anchor_y as integer
end type

type ScreenMode
    w as integer
    h as integer
    depth as integer
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

' One command, after the camera: x, y its anchor or first
' end, x2, y2 its other end.
type Command
    kind as integer
    target as long
    source as long
    pic as integer
    x as single
    y as single
    x2 as single
    y2 as single
    angle as single
    scale as single
    flags as integer
    light as integer
    tint as long
    blend as integer
    alpha as integer
    recolor as integer
end type

' A pixel stage: source a surface, colour &HRRGGBB.
type Stage
    op as integer
    source as long
    colour as long
end type

' --- the canvas and the display ---

declare sub open cdecl ( _
    byval w as integer, _
    byval h as integer, _
    byval bits as integer, _
    byval flags as long _
)
declare sub close cdecl ()
declare function width cdecl () as integer
declare function height cdecl () as integer
declare function depth cdecl () as integer
declare function displays cdecl () as integer
declare function modes cdecl (byval display as integer) as integer
declare sub mode cdecl ( _
    byval display as integer, _
    byval n as integer, _
    m as ScreenMode _
)
' mode 0 is the desktop's own, 1 on the list's.
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
' 64 rows of 256 indices; built from row 0 when not given.
declare sub light_table cdecl (map() as byte)

' --- the pen: for what is drawn after ---

declare sub target cdecl (byval surface as long)
declare sub clip cdecl ( _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
)
declare sub unclip cdecl ()
declare sub light cdecl (byval level as integer)
declare sub tint cdecl (byval colour as long)
declare sub blend cdecl (byval how as integer)
declare sub alpha cdecl (byval level as integer)
declare sub recolor cdecl (byval row as integer)
' x, y the world's point at the canvas's top left.
declare sub camera cdecl (byval x as single, byval y as single)
' x, y the world's point at the canvas's centre.
declare sub camera_turned cdecl ( _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval zoom as single _
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
    byval colour as long _
)
declare sub rect_fill cdecl ( _
    byval x1 as single, _
    byval y1 as single, _
    byval x2 as single, _
    byval y2 as single, _
    byval colour as long _
)
declare sub circle cdecl ( _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval colour as long _
)
declare sub circle_fill cdecl ( _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval colour as long _
)
declare sub arc cdecl ( _
    byval x as single, _
    byval y as single, _
    byval r as single, _
    byval start as single, _
    byval finish as single, _
    byval colour as long _
)
' pic 0 is the whole surface: a canvas drawn as a sprite.
declare sub sprite cdecl ( _
    byval source as long, _
    byval pic as integer, _
    byval x as single, _
    byval y as single, _
    byval flags as integer _
)
declare sub sprite_turned cdecl ( _
    byval source as long, _
    byval pic as integer, _
    byval x as single, _
    byval y as single, _
    byval angle as single, _
    byval scale as single, _
    byval flags as integer _
)
' A cell a picture, 0 empty; columns a row; w by h cells.
declare sub tiles cdecl ( _
    byval atlas as long, _
    map() as integer, _
    byval columns as integer, _
    byval x as single, _
    byval y as single, _
    byval w as integer, _
    byval h as integer _
)
' The picture repeated along the points, hanging below.
declare sub strip cdecl ( _
    byval atlas as long, _
    byval pic as integer, _
    points() as Vec2, _
    byval first as integer, _
    byval n as integer _
)
' Convex; atlas 0 flat, in each vertex's tint.
declare sub poly cdecl ( _
    byval atlas as long, _
    byval pic as integer, _
    v() as Vertex, _
    byval first as integer, _
    byval n as integer _
)
declare sub text cdecl ( _
    byval font as long, _
    byval x as single, _
    byval y as single, _
    byval s as string _
)
declare function text_width cdecl ( _
    byval font as long, _
    byval s as string _
) as integer

' --- surfaces, views and atlases ---

declare function new_surface cdecl ( _
    byval w as integer, _
    byval h as integer _
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
' n slots of w by h, one above the next: a slot's view is
' aimed at y = slot * h.
declare function new_cache cdecl ( _
    byval w as integer, _
    byval h as integer, _
    byval n as integer _
) as long
declare function load_atlas cdecl (byval file as string) as long
declare function new_atlas cdecl (byval pixels as integer) as long
' Pixels a byte an index, or 4 a pixel, R G B A; the new
' picture's number.
declare function add cdecl ( _
    byval atlas as long, _
    byval w as integer, _
    byval h as integer, _
    byval anchor_x as integer, _
    byval anchor_y as integer, _
    pixels() as byte _
) as integer
declare function grab cdecl ( _
    byval atlas as long, _
    byval source as long, _
    byval x as integer, _
    byval y as integer, _
    byval w as integer, _
    byval h as integer _
) as integer
declare function pictures cdecl (byval atlas as long) as integer
' A view over one picture, to draw into.
declare function picture cdecl ( _
    byval atlas as long, _
    byval pic as integer _
) as long
declare sub measure cdecl ( _
    byval atlas as long, _
    byval pic as integer, _
    p as PicInfo _
)
' Glyphs 32 to 126 from picture first on.
declare function new_font cdecl ( _
    byval atlas as long, _
    byval first as integer _
) as long
declare sub free cdecl (byval handle as long)

' --- reading and writing pixels: a GPU waits ---

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

' --- what the frame cost, and what it drew ---

declare sub stats cdecl (s as FrameStats)
declare function caps cdecl () as long
declare function cost cdecl (byval material as long) as integer
declare function draws cdecl () as integer
declare sub drawn cdecl (byval n as integer, d as Command)

' --- 3D ---

declare sub depth_mode cdecl (byval how as integer)
declare sub clear_depth cdecl ()
declare sub order cdecl (byval first as integer)
declare sub perspective cdecl ( _
    byval fov as single, _
    byval near as single, _
    byval far as single _
)
declare sub look cdecl (eye as Vec3, at as Vec3, up as Vec3)
declare sub ortho cdecl ()
declare sub load_matrix cdecl ( _
    byval which as integer, _
    m as Matrix _
)
' tris: three vertex numbers a triangle, from 0.
declare function new_mesh cdecl ( _
    v() as Vertex, _
    tris() as integer _
) as long
declare sub mesh_update cdecl ( _
    byval handle as long, _
    v() as Vertex, _
    byval first as integer _
)
declare sub mesh cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval texture as long, _
    byval texture2 as long _
)
' v: frames of vertices end to end.
declare function new_model cdecl ( _
    v() as Vertex, _
    tris() as integer, _
    byval frames as integer _
) as long
declare sub model cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval texture as long, _
    byval frame as integer, _
    byval lerp as single, _
    at as Matrix _
)
declare sub billboard cdecl ( _
    byval atlas as long, _
    byval pic as integer, _
    at as Vec3, _
    byval scale as single _
)
declare function new_material cdecl (stages() as Stage) as long
