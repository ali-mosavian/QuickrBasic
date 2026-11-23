' gfx.scene: 3D, into the pen's target
' (docs/gfx-extension-design.md). Depth is tested and
' written unless a flag says not; angles are radians.

' A draw call's flags: these and gfx.pen's.
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

type Vec3
    x as single
    y as single
    z as single
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

declare sub camera cdecl ( _
    eye as Vec3, _
    at as Vec3, _
    up as Vec3, _
    byval fov as single _
)
' The camera's planes: 1 and 4096 till set.
declare sub depth_range cdecl ( _
    byval near as single, _
    byval far as single _
)
declare sub clear_depth cdecl ()

' tris: three vertex numbers a triangle, from 0. v: frames
' of vertices end to end; 1 a still mesh.
declare function new_mesh cdecl ( _
    v() as Vertex, _
    tris() as integer, _
    byval frames as integer _
) as long
declare function new_material cdecl ( _
    byval kind as integer, _
    byval texture as long, _
    byval lightmap as long _
) as long
' What a kind costs here, for the program to pick by.
declare function cost cdecl (byval kind as integer) as integer

' In the world, at its first frame.
declare sub mesh cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval flags as integer _
)
' frame 3.25: a quarter of the way from 3 to 4.
declare sub mesh_placed cdecl ( _
    byval handle as long, _
    byval material as long, _
    byval frame as single, _
    where as Matrix, _
    byval flags as integer _
)
' A surface facing the camera.
declare sub billboard cdecl ( _
    byval s as long, _
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
