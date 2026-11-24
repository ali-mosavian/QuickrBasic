' :::::::::::
' gfx.scene: 3D drawing into surfaces. Declared, not yet
' built.
'
'     Every call draws into dst, any surface, within its
'     clip, as gfx.draw's do; the camera is the scene's.
'     Angles are radians.
' :::::::::::

' Drawn after the rest, farthest first.
const ORDERED = 256
' As gfx.draw's.
const OPAQUE = 8
const BLEND = 16
const ADD = 32
const SOLID = 64
const SMOOTH = 128
const DEPTH_TEST = 4096
const DEPTH_WRITE = 8192

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

' :::::::::::
' A placing: row vectors, v' = v * m, the move in m41 to
' m43, as qgl's.
' :::::::::::
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

' :::::::::::
' A mesh's point.
'
'     Attributes:
'       x, y, z:
'         The point.
'       u, v:
'         The texture's texel there.
'       u2, v2:
'         The lightmap's texel there.
'       light:
'         0 black to 63 the texel's own.
'       tint:
'         &HRRGGBB, multiplied.
' :::::::::::
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

' :::::::::::
' Sets the camera.
'
'     Args:
'       eye:
'         Where it is.
'       at:
'         What it looks at.
'       up:
'         Its up.
'       fov:
'         Its field of view, across.
' :::::::::::
declare sub set_camera cdecl ( _
    eye as Vec3, _
    at as Vec3, _
    up as Vec3, _
    byval fov as single _
)

' :::::::::::
' Sets the camera's near and far planes: 1 and 4096 at
' first.
' :::::::::::
declare sub set_depth_range cdecl ( _
    byval near as single, _
    byval far as single _
)

' :::::::::::
' Makes a mesh.
'
'     Args:
'       v:
'         Its frames' vertices, end to end.
'       tris:
'         Three vertex numbers a triangle, from 0.
'       frames:
'         How many frames v holds; 1 a still mesh.
'
'     Returns:
'       The mesh.
' :::::::::::
declare function new_mesh cdecl ( _
    v() as Vertex, _
    tris() as integer, _
    byval frames as integer _
) as long

' :::::::::::
' Frees a mesh.
' :::::::::::
declare sub free_mesh cdecl (byval m as long)

' :::::::::::
' Makes a material.
'
'     Args:
'       kind:
'         FLAT to SKY.
'       texture, lightmap:
'         Surfaces; 0 none.
'
'     Returns:
'       The material.
' :::::::::::
declare function new_material cdecl ( _
    byval kind as integer, _
    byval texture as long, _
    byval lightmap as long _
) as long

' :::::::::::
' Frees a material.
' :::::::::::
declare sub free_material cdecl (byval material as long)

' :::::::::::
' Gets what a material kind costs here, for a program
' to pick by.
' :::::::::::
declare function cost cdecl (byval kind as integer) as integer

' :::::::::::
' Draws a mesh's first frame in the world as it is.
'
'     Args:
'       dst:
'         The surface drawn into.
'       m:
'         The mesh.
'       material:
'         Its material.
'       flags:
'         ORDERED, OPAQUE, BLEND, ADD, SOLID, SMOOTH,
'         DEPTH_TEST, DEPTH_WRITE.
' :::::::::::
declare sub mesh cdecl ( _
    byval dst as long, _
    byval m as long, _
    byval material as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a mesh placed, between two frames.
'
'     Args:
'       dst:
'         The surface drawn into.
'       where:
'         Its placing.
'       frame:
'         The frame, from 0: 3.25 a quarter of the way
'         from 3 to 4.
'       m:
'         The mesh.
'       material:
'         Its material.
'       flags:
'         As mesh's.
' :::::::::::
declare sub mesh_placed cdecl ( _
    byval dst as long, _
    where as Matrix, _
    byval frame as single, _
    byval m as long, _
    byval material as long, _
    byval flags as integer _
)

' :::::::::::
' Draws a surface facing the camera.
'
'     Args:
'       dst:
'         The surface drawn into.
'       at:
'         Where src's anchor goes.
'       scale:
'         Its size; 1 a texel a unit.
'       src:
'         The surface drawn.
'       flags:
'         As mesh's.
' :::::::::::
declare sub billboard cdecl ( _
    byval dst as long, _
    at as Vec3, _
    byval scale as single, _
    byval src as long, _
    byval flags as integer _
)

' :::::::::::
' Makes a placing.
'
'     Args:
'       m:
'         Set to it.
'       at:
'         The position.
'       turn:
'         Yaw, pitch and roll.
'       scale:
'         The size; 1 its own.
' :::::::::::
declare sub place cdecl ( _
    m as Matrix, _
    at as Vec3, _
    turn as Vec3, _
    byval scale as single _
)

' :::::::::::
' Joins two placings: m is a, then b.
' :::::::::::
declare sub join cdecl (m as Matrix, a as Matrix, b as Matrix)
