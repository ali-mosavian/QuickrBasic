# Graphics extension: design

`gfx` is a native extension ([native-extensions-design.md](native-extensions-design.md)) for 2D and
3D games. The program draws through one API; each platform's backend draws its own best way: the GPU
where there is one, spans in software on DOS. The pixels are not the same everywhere, and are not
meant to be: the rules below are.

Status: a design. Nothing is built.

## What the program sees

```basic
import gfx
gfx.open 320, 200, 8
atlas = gfx.pictures("CHARLIE.DAT", 1)
do
    gfx.begin
    gfx.perspective eye, look, 60
    gfx.mesh level_mesh, LIGHTMAPPED
    gfx.model dog, frame, lerp, at
    gfx.ortho
    gfx.camera cam.x, cam.y
    gfx.tiles ground, lv
    gfx.sprite atlas, IHERO + f, p.at.x, p.at.y, p.ang, FLIPX * p.face
    gfx.end
loop
```

## The rules every backend keeps

- A coordinate names a pixel's centre; a pixel is drawn when its centre is inside the polygon, or on
  its top or left edge.
- Edges and texture coordinates are sub-pixel and sub-texel exact: each span starts at its first
  whole pixel's centre, with u and v stepped to there.
- A texel is the nearest unless the program asks for smooth sampling; a backend that cannot filter
  takes the nearest.
- Depth is tested and written as the program sets it; commands are drawn in the order given.

What may differ: rounding where a pixel's centre or a sample falls within a precision step of an edge;
perspective, corrected every pixel on a GPU and every 8 or 16 in software; filtering; the resolution
drawn at.

## The canvas: any resolution and depth

`gfx.open w, h, depth` asks for a canvas; the backend gives the nearest it has, and `gfx.width`,
`gfx.height` and `gfx.depth` say what it gave. Depth is 8 (indexed), 15, 16 or 32 bits.

The canvas is what the program draws in; the window is the host's. The host scales the canvas to the
window: by whole multiples, to fit, or stretched. With `GFX_NATIVE` a GPU backend draws at the
window's resolution instead: the same commands, more pixels, and sprites turn smoothly rather than a
canvas pixel at a time. Software ignores it.

On DOS, 320×200 at 8 bits is VGA's mode 13h; the rest are VBE 2.0 modes through a linear framebuffer,
with banked modes for VBE 1.2 cards. The DOS platform has neither yet.

In software the polygon scanner knows nothing of pixels and a filler knows nothing of polygons: one set
of span fillers for each depth, as µGL's `cfmt/b8`, `b15`, `b16` and `b32`. A new depth is a new set of
fillers, never a flag in the scanner.

## Displays and modes

A display mode is what the screen shows; the canvas is shown in it, scaled. So a 320×200 canvas can be
shown in any mode.

- `gfx.displays` counts the displays; `gfx.modes(display)` lists each mode's width, height, depth and
  refresh.
- `gfx.open` takes `GFX_WINDOW` (the default), `GFX_FULL` (the desktop's own mode, the canvas scaled
  into it) or `GFX_MODE` with a mode from the list, switched to.
- With `GFX_VSYNC`, `gfx.end` waits for the display's refresh.
- `gfx.event` reports what the host saw: the window resized, the display changed, focus lost, the GPU
  lost and back. The extension holds every texture, mesh and canvas, so it uploads them again itself.

| | Modes listed | Set by |
|---|---|---|
| DOS | mode 13h, and VBE's list (4F00h, 4F01h) | 4F02h; pages flipped by 4F07h at the retrace |
| Mac, Linux, Windows | `SDL_GetFullscreenDisplayModes` | `SDL_SetWindowFullscreenMode`, or a window |
| The browser | one, the page's | none; full screen by the Fullscreen API, after a click or a key |

On DOS a mode is the whole screen and the one display.

## Colour

A texture is indexed (8 bits and a palette) or direct (16 or 32 bits).

| | 8-bit canvas | 15, 16 or 32-bit canvas |
|---|---|---|
| indexed texture | its index through the light table; stays an index | index, light table, then the palette, as it is drawn |
| direct texture | matched to the palette once, as it is loaded | its colour, multiplied by the light |

- Light is a level from 0 to 63. On an 8-bit canvas it reads a light table, 64 rows of 256 indices:
  Quake's colormap. On the others it multiplies.
- `gfx.palette` sets the palette for what is drawn after it. On DOS at 8 bits the VGA's palette
  changes the whole screen at once; the program does not rely on that.
- Index 0 of an indexed texture, and alpha under one half of a direct one, are not drawn.
- `gfx.rgb(r, g, b)` is a colour in the canvas's own terms: the nearest index at 8 bits.

Blending is one of opaque, keyed (as above), translucent and additive. A GPU blends in hardware; software
at 8 bits reads a 64 KB table for each mode, index by index; at 15, 16 and 32 bits it adds and
multiplies.

## Frames and commands

Between `gfx.begin` and `gfx.end` the program records commands. A GPU backend batches them by texture
and state and submits once; software draws them in order. `gfx.end` shows the frame.

Textures, meshes and canvases are uploaded once and named by a handle. A canvas is a texture that can be
drawn into: a parallax layer, a HUD kept between frames, a mirror.

One frame may hold a 3D pass and then a 2D pass. `gfx.perspective` and `gfx.ortho` set the projection;
`gfx.camera`, `gfx.turn` and `gfx.scale` set a 2D transform, applied as commands are recorded.

## Views

A view is a surface over another surface's pixels, as µGL's `uglNewView` and qgl's `qglNewView`: its
own shape and clipping, no memory of its own, nothing copied.

- A picture is a view of its atlas, with an anchor.
- Drawing into a view of a canvas draws into its rectangle, clipped to it: a split screen, a minimap,
  the HUD's strip.
- A cache, `gfx.cache(w, h, n)`, holds n surfaces of one size. `gfx.aim view, slot` moves a view to
  another slot without allocating: one view for each size, aimed at each surface in turn, as qgl's
  surface cache does.
- Wrapping repeats a view, not its parent, so a tile in an atlas tiles.
- A surface with views alive cannot be freed: `gfx.free` raises an error rather than leaving them
  pointing at nothing.

On a GPU a view of a texture is a rectangle of texture coordinates, and drawing into one sets the
viewport and scissor to it; a wrapping view is repeated in the shader, within its rectangle. In software
a view is an offset and a stride, as µGL's.

A cache's views own their stride: in software its surfaces are packed end to end, with no gaps. A GPU
cannot read a texture at another stride, so there a cache is a texture array and a slot is a layer.

## 2D

Everything 2D is a textured quad or a strip of them, so the backends have one primitive.

- `gfx.sprite atlas, picture, x, y, angle, scale, flags`: a picture of the atlas, its size and anchor
  stored with it, turned and scaled about its anchor. `FLIPX` and `FLIPY` swap its u or v.
- `gfx.tiles`: a tile layer, drawn a row of quads at a time.
- `gfx.strip`: a textured strip along a line of points: the ground under a platformer's feet, drawn
  from the same points its physics walks, at any slope.
- `gfx.poly`: a convex polygon, flat or textured.

The software backend picks fast paths itself: an upright sprite at 1:1 is a keyed row copy, a tile row
a tight loop. The program never chooses.

An atlas's pictures, their sizes and anchors come from the tool that builds them, which also writes the
BASIC constants naming them.

## 3D

- **Meshes**: positions, u and v, and a shade or normal for each vertex; triangles by index.
- **Matrices**: model, view and projection, 4 by 4. The near plane is clipped by the GPU, and in
  software by clipping each polygon before it is scanned.
- **Depth**: off, written, or tested and written. `gfx.order FRONT` says commands come nearest first:
  a GPU rejects hidden pixels early; software keeps a coverage buffer and skips spans already drawn,
  which on a 486 replaces testing each pixel's depth.
- **Materials**, lists of pixel stages ([Shaders](#shaders)), the same on every backend: flat, textured,
  vertex-lit, lightmapped, keyed, translucent, warped (water) and sky. A lightmapped surface is two
  textures multiplied on a GPU, and a cache of surfaces lit once in software, as Quake's.
- **Mip levels**: built when a texture is loaded; a GPU picks one for each pixel, software for each
  polygon.
- **Models**: keyframes, blended between two: in the vertex shader on a GPU, on the CPU in software.
- **Billboards and particles**: quads facing the camera, with depth.

The software side of all of this is qcport's: its scanner (`qglRsPoly`), clipper (`qglClPoly`),
surface builder (`qglSbBuild`), coverage and depth. It moves from 16-bit real mode with EMS to the DOS
build's 32-bit flat memory.

## Shaders

QuickrBasic runs BASIC as bytecode, so a shader written in BASIC would be interpreted at every pixel:
too slow in software. Shaders come in three kinds, by where they can run.

- **Vertex procedures, everywhere.** A BASIC `SUB` given a mesh's vertices and the frame's values, run
  on the CPU before the mesh is drawn: a wave, a wobble, a bend. A 486 affords it for hundreds of
  vertices a frame. The built-in vertex work, transform, keyframe blend and billboards, is the
  backend's: a vertex shader on a GPU.
- **Pixel stages, everywhere.** A material is a short list of stages, each a step every backend has:
  read a texture, its u and v warped or not; read the light table; read the palette; multiply or add a
  second texture or a colour; key; blend. A GPU backend joins a material's stages into one shader.
  Software has a hand-written span filler for each built-in material's list, and a slower general
  one that steps through any other.
- **The program's own shaders, on a GPU only.** GLSL ES 3.0, as WebGL2 takes it; for SDL_GPU, compiled
  to SPIR-V by glslang and crossed to MSL and DXIL by SDL_shadercross. A program that uses one also
  gives a material of stages, drawn where there are none.

`gfx.caps` says what the backend has: a GPU, the program's own shaders, smooth sampling, and a cost for
each material, so the program picks its own detail.

## Backends

| Platform | Backend |
|---|---|
| DOS | software, span fillers in assembly, VGA and VBE |
| Mac, Linux, Windows | SDL_GPU: Metal, Vulkan, Direct3D 12 |
| The browser | WebGL2 |
| Tests, and a machine with no GPU | software in C, drawing into a texture the host shows |

SDL's 2D renderer is not enough: `SDL_RenderGeometry` has no depth and no vertex shader, so 3D needs
SDL_GPU, and one backend serves both.

On a GPU an indexed texture is an 8-bit texture read exactly (`texelFetch` on WebGL2); the shader
looks it up in the light table and the palette, both textures. The canvas at 8 bits is an 8-bit
target, shown through the palette by a last pass.

The shaders are built for each backend (MSL, SPIR-V, DXIL, GLSL ES 3.0) and kept in the `.qbx` beside
its code.

## The host's side

`QxHost` gains `gfx_open`, as it has `audio_open`: the host keeps the window, the GPU device and the
frame's timing, and hands the backend what it needs to draw and show. No GPU: the software backend.

The runtime's own screen, `SCREEN`'s page with `PSET`, `LINE` and `PRINT`, is a layer the frame draws
over or under.

## Tests

`gfx.drawn` returns the frame's commands after every transform: what was drawn, where, how turned,
with which material. Tests assert on that, and pass on every backend.

`gfx.capture` returns a finished frame's pixels. Tests that compare pixels run on the software backend.

## Order

1. The core: canvas, displays and modes, colour, frames, textured triangles, pixel stages; the
   software backend in C at 8 and 32 bits.
2. 2D: sprites, tiles, strips; CHARLIE moves onto them.
3. The SDL_GPU and WebGL2 backends.
4. DOS: span fillers in assembly, VBE modes.
5. 3D: meshes, materials, models, from qcport.
6. Vertex procedures, and the program's own shaders.

## Prior art

| | What `gfx` takes from it |
|---|---|
| µGL, qgl | views; a set of span fillers for each depth; the scanner, clipper and surface cache |
| Allegro 4 | sub-bitmaps; `polygon3d`'s types, affine or perspective, masked, lit, translucent; light and translucency tables; mode lists; `scene_polygon3d`'s sorted spans |
| Allegro 5 | the same library moved to GPUs: `al_draw_prim`'s vertices, shaders, held drawing batched, display modes |
| Quake 2's refresh | one game, a software and a GL renderer behind one scene-level interface, so each keeps its own way of hiding surfaces |
| Glide, Direct3D 7's stages, the GameCube's TEV | fixed combine stages that hardware and software both run: the pixel stages |
| sokol_gfx | resources, pipelines and passes over GL, WebGL2, Metal and D3D11; one shader source compiled for each |
| bgfx | a sort key on each draw, for batching; a backend that draws nothing, for tests |
| raylib | `Camera2D`; `DrawTexturePro`'s source, origin and rotation; batching over GL 1.1 to ES3 |
| LÖVE | `Quad`, `Canvas`, `SpriteBatch`, `Mesh`, `Shader` and the fullscreen modes, behind a scripting language |
| PICO-8, TIC-80 | palette swaps, `camera`, `clip`, flipped sprites, `tline` for turned and mode-7 drawing |
| TinyGL | an OpenGL subset in software, small and in C, at 8, 16, 24 and 32 bits: the C backend's size |

## Open questions

1. `SCREEN`'s page and a `gfx` canvas: two layers (as above), or one canvas both draw into?
2. The program's own shaders: GLSL ES as it is, or a BASIC subset translated to it?
3. Depth in software: a 16-bit z, or 1/z as Quake's?
4. 15 bits: worth its fillers, or 16 alone?
5. Vertex procedures: on the CPU everywhere (as above), or translated to vertex shaders on a GPU?
6. 3D worlds: a scene-level call as Quake 2's, so software keeps its own way of hiding surfaces, or
   triangles with `gfx.order FRONT` alone?
