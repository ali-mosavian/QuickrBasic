# Graphics extension: design

`gfx` is a native extension ([native-extensions-design.md](native-extensions-design.md)) for 2D and
3D games. The program draws through one API; each platform's backend draws its own best way: the GPU
where there is one, spans in software on DOS. The pixels are not the same everywhere, and are not
meant to be: the rules below are.

Status: a design. The interface is drafted in [src/ext/gfx/gfx.bas](../src/ext/gfx/gfx.bas); nothing
is built.

## What the program sees

```basic
import gfx
gfx.open 320, 200, 8, gfx.VSYNC
sheet = gfx.load_atlas("CHARLIE.GFX", pic())
walls = gfx.new_material(gfx.LIGHTMAPPED, stone, lightmap)
do
    gfx.camera_3d eye, at, up, 1.05
    gfx.mesh level, walls, gfx.ORDERED
    gfx.place m, dog.at, dog.turn, 1
    gfx.model dog.shape, fur, dog.frame, m, 0
    gfx.camera cam.x, cam.y
    gfx.tiles lv.map(), lv.w, 0, 0, TS, TS, 0
    gfx.sprite pic(IHERO + f), p.at.x, p.at.y, gfx.FLIPX * p.face
    gfx.rect 0, 0, 319, 9, CINK, gfx.FILL
    gfx.text 4, 1, "SCORE" + str$(score), CWHITE
    gfx.flip
loop
```

## The interface

[src/ext/gfx/gfx.bas](../src/ext/gfx/gfx.bas) is the `.qbx`'s interface: every procedure, record and
constant.

- **QuickrBasic's dialect.** It has no `SCREEN`, `PRINT`, `LINE`, `CIRCLE`, `PSET`, `OPEN` or
  `CLOSE` statements: those are QB's dialect's, and here the standard library's extensions. So
  `gfx.line`, `gfx.open` and `gfx.palette` are plain names, and the interface is read in this dialect.
- **Neither too low nor too high.** A call is a thing a game draws: a sprite, a tile layer, a mesh
  with its material. Stages, matrices set by hand, depth modes and the surface cache stay inside.
- **Everything is a surface**: the canvas (`gfx.CANVAS`), a loaded picture, a sheet of them, a view
  over part of any of these. A sprite, a tile, a glyph sheet and a texture are surfaces; an atlas is
  a sheet and its views.
- **Handles** are longs: a kind (4 bits), a generation (8) and a slot (20), so a freed or wrong
  handle is an error, never a stray pointer. 0 is none.
- **Constructors** are `new_*` and `load_*`, as µGL's `uglNew*`. `gfx.free` frees any handle.
- **Flags pick the effect, the pen holds its numbers**, as µGL's `tfxBlit` and `tfxSet*`. One set of
  flags for every draw call: `FILL`, `FLIPX`, `FLIPY`, `OPAQUE`, `BLEND`, `ADD`, `SOLID`, `SMOOTH`;
  for 3D `ORDERED`, `NO_ZWRITE`, `NO_ZTEST`. The pen is `target`, `clip`, `light`, `tint`, `alpha`,
  `recolor`, `font` and the cameras; `reset` sets it back. Its numbers land in the vertices, so a
  change does not end a batch.
- **No optional arguments**, so the common case is its own call, as µGL's `uglPut` and
  `uglPutRotScl`: `sprite` and `sprite_turned`, `camera` and `camera_turned`.
- **A colour is the canvas's**: an index at 8 bits, `&HRRGGBB` at 15, 16 and 32. `gfx.rgb` gives one
  from red, green and blue.
- **Records** cross as the ABI has them: by reference, valid for the call, laid out as BASIC lays
  them (the glue asserts each size). An array (`Vec2`, `Vertex`, a map's handles) is read during the
  call, or copied when kept (a mesh). `Matrix` is qgl's `Mat4`: row vectors, the move in `m41` to
  `m43`; `gfx.place` and `gfx.join` make them. A list (modes, commands drawn) is a count and a call
  for each: `modes` then `mode`, `draws` then `drawn`.
- **Names do not collide.** A type is never named as a procedure (`ScreenMode` beside `mode`), and
  a parameter never as a procedure or a constant.

| Error | When |
|---|---|
| 5 Illegal function call | a handle freed or of another kind; an argument out of its range; drawing before `open`; a view outside its parent; a file not a picture's |
| 7 Out of memory | the host has none left |
| 9 Subscript out of range | `first` and `n` past an array; `views()` shorter than the atlas; a mode past the list |
| 53 File not found | `load_surface`, `load_atlas` |
| 73 Advanced feature unavailable | the program's own shaders on software |

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

`gfx.open w, h, depth, flags` asks for a canvas; the backend gives the nearest it has, and
`gfx.width`, `gfx.height` and `gfx.bits` of `gfx.CANVAS` say what it gave. Depth is 8 (indexed), 15,
16 or 32 bits.

The canvas is what the program draws in; the window is the host's. The host scales the canvas to the
window: by whole multiples, to fit (`gfx.FIT`), or stretched (`gfx.STRETCH`). With `gfx.NATIVE` a GPU
backend draws at the window's resolution instead: the same commands, more pixels, and sprites turn
smoothly rather than a canvas pixel at a time. Software ignores it.

On DOS, 320×200 at 8 bits is VGA's mode 13h; the rest are VBE 2.0 modes through a linear framebuffer,
with banked modes for VBE 1.2 cards. The DOS platform has neither yet.

In software the polygon scanner knows nothing of pixels and a filler knows nothing of polygons: one set
of span fillers for each depth, as µGL's `cfmt/b8`, `b15`, `b16` and `b32`. A new depth is a new set of
fillers, never a flag in the scanner.

## Displays and modes

A display mode is what the screen shows; the canvas is shown in it, scaled. So a 320×200 canvas can be
shown in any mode.

- `gfx.displays` counts the displays; `gfx.modes(display)` counts its modes and `gfx.mode` gives
  each one's width, height, depth and refresh.
- `gfx.open` opens a window. `gfx.fullscreen display, n` shows the canvas full screen: n 0 in the
  desktop's own mode, 1 on in the list's, switched to. `gfx.windowed` goes back.
- With `gfx.VSYNC`, `gfx.flip` waits for the display's refresh. `gfx.clock` is the host's own clock
  in seconds, and `gfx.refresh` the display's rate.
- `gfx.poll` reports what the host saw: the window resized, the display changed, focus lost, the GPU
  lost and back, the window closed. The extension holds every texture, mesh and canvas, so it
  uploads them again itself; a canvas drawn into is lost, and the program draws it again.

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
- `gfx.palette row, colours()` sets a palette row: row 0 is the canvas's, and `gfx.recolor row`
  draws what follows through another, as PICO-8's `pal`. On DOS at 8 bits the VGA's palette changes
  the whole screen at once; the program does not rely on that.
- Index 0 of an indexed texture, and alpha under one half of a direct one, are not drawn; `OPAQUE`
  draws them.
- `gfx.rgb(r, g, b)` is a colour in the canvas's own terms: the nearest index at 8 bits.
- `SOLID` draws a surface's shape in the pen's tint: a hit's flash, a shadow, a one-colour font.

Blending is keyed (as above), opaque, translucent (`BLEND`) or additive (`ADD`), by the pen's alpha. A
GPU blends in hardware; software at 8 bits reads a 64 KB table for each mode, index by index; at 15, 16
and 32 bits it adds and multiplies.

## Drawing

The program draws as with µGL or Allegro: any call, at any time, into the screen or any surface, in the
order written. `gfx.flip` shows the frame. Software draws each call as it comes; for a GPU the
extension records them, and the host plays them at the flip, or when a surface drawn into is read.

Textures, meshes and canvases are uploaded once and named by a handle. A canvas is a texture that can be
drawn into: a parallax layer, a HUD kept between frames, a mirror.

One frame may hold a 3D pass and then a 2D pass: 3D calls take the 3D camera, the rest the 2D one
(`gfx.camera`, `gfx.camera_turned`), applied as commands are recorded.

## Friendly, and kind to a GPU

Each convenience of µGL and Allegro is kept, and given a shape a GPU draws in few batches.

| The program writes | The backend does |
|---|---|
| a picture loaded on its own | packs it into an atlas page as it loads: the handle is a view, and sprites of one page draw as one batch |
| `gfx.pset`, `gfx.line`, `gfx.rect`, `gfx.circle` | draws them as triangles from a white texel kept in every page, so they join the sprites' batch |
| a light, tint, palette or flip for one sprite | puts it in the sprite's vertices; a palette is a row of the palette texture, so a swap does not split the batch |
| a surface made or freed mid-frame | takes it from a pool of views in pages; nothing is allocated on the GPU in the frame |
| pixels written one by one into a surface | keeps that surface in memory and uploads its changed rows once, when it is next drawn from |

A batch ends at another page, material, blend, target or clip. Order is kept, so the batches are the
program's to shape, and `gfx.stats` counts them.

What would make a GPU wait is a call of its own, never a side effect of drawing:

- `gfx.point` and `gfx.read`, on a surface the GPU drew, wait for it to finish. The canvas read after
  `gfx.flip` is the frame shown.
- `gfx.stats` counts the frame's draws, batches, uploads and waits, so a slow frame says why.

Allegro 5's traps are the reverse of these: a memory bitmap drawn to the screen was uploaded at every
draw, unseen, and its sprites batched only when held. Here where a surface lives is the backend's
choice, and batching is always on.

`PUT`'s `XOR`, `AND` and `OR` are not offered on a `gfx` surface: Metal and WebGL2 have no logic
operations. They stay on `SCREEN`'s page.

## Views

A view is a surface over another surface's pixels, as µGL's `uglNewView` and qgl's `qglNewView`: its
own shape and clipping, no memory of its own, nothing copied.

- A picture is a view of its sheet, with an anchor. `gfx.load_atlas` loads a sheet and fills an array
  with its views; `gfx.new_view` makes one by hand.
- Drawing into a view of a canvas draws into its rectangle, clipped to it: a split screen, a minimap,
  the HUD's strip.
- `gfx.aim` moves a view within its parent without allocating. The backend's surface cache is
  built on it, as qgl's: n surfaces of one size, and one view aimed at each in turn.
- Wrapping repeats a view, not its parent, so a tile in an atlas tiles.
- A surface's views are freed with it. A handle to one is then stale, and using it is an error.

On a GPU a view of a texture is a rectangle of texture coordinates, and drawing into one sets the
viewport and scissor to it; a wrapping view is repeated in the shader, within its rectangle. In software
a view is an offset and a stride, as µGL's.

The cache's views own their stride: in software its surfaces are packed end to end, with no gaps.
On a GPU a lightmapped surface is two textures multiplied, so there is no cache.

## 2D

Everything 2D is a textured quad or a strip of them, so the backends have one primitive.

- `gfx.sprite surface, x, y, flags`: a surface at its anchor. `gfx.sprite_turned` adds an angle and
  a scale, about the anchor. `FLIPX` and `FLIPY` swap its u or v.
- `gfx.quad`: a surface onto any four corners.
- `gfx.tiles`: a tile layer, a cell a surface, drawn a row of quads at a time; only the cells in
  view are visited.
- `gfx.strip`: a textured strip along a line of points: the ground under a platformer's feet, drawn
  from the same points its physics walks, at any slope.
- `gfx.pset`, `gfx.line`, `gfx.arc`; `gfx.rect`, `gfx.circle` and `gfx.poly`, outlined or with
  `FILL`.
- `gfx.text x, y, s$, colour` in the pen's font: a sheet of glyphs (`gfx.new_font`), or the built-in
  8 by 8.

The software backend picks fast paths itself: an upright sprite at 1:1 is a keyed row copy, a tile row
a tight loop. The program never chooses.

An atlas's pictures, their sizes and anchors come from the tool that builds it, which also writes the
BASIC constants numbering them: `pic(IHERO + f)`.

## 3D

- **Meshes**: a position, the texture's u and v, the lightmap's, a light and a tint for each
  vertex; triangles by index.
- **Cameras**: `gfx.camera_3d eye, at, up, fov` for `mesh`, `model` and `billboard`; the 2D camera
  for the rest. So one frame holds both with no switch between. `gfx.place` makes a model's matrix
  from its position and angles. The near plane is clipped by the GPU, and in software by clipping
  each polygon before it is scanned.
- **Depth** is tested and written by every 3D call. `NO_ZTEST` and `NO_ZWRITE` leave one out.
  `ORDERED` says the calls come nearest first: a GPU rejects hidden pixels early; software keeps a
  coverage buffer and skips spans already drawn, which on a 486 replaces testing each pixel's depth.
- **Materials**: a kind and its textures, made once (`gfx.new_material`). The kinds are lists of
  pixel stages ([Shaders](#shaders)), the same on every backend: flat, textured, vertex-lit,
  lightmapped, warped (water) and sky; keyed and translucent are flags. A lightmapped surface is two
  textures multiplied on a GPU, and a cache of surfaces lit once in software, as Quake's.
- **Mip levels**: built when a texture is loaded; a GPU picks one for each pixel, software for each
  polygon.
- **Models**: keyframes, blended between two by a fractional frame: in the vertex shader on a GPU,
  on the CPU in software.
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

The GPU backends are the host's. A slice imports nothing, so it cannot call SDL_GPU or WebGL2, and
both hosts show the screen from another thread: SDL's main thread, and the page's, which reads the
shared memory (`frame()`). So the extension records a frame as a list in its own memory: uploads,
passes and draws, already batched. At `gfx.flip` the host's display thread plays it with SDL_GPU, or
with WebGL2 from the page's JavaScript. The list's format is part of the ABI. Software stays in the
extension, and hands the host pixels.

On a GPU an indexed texture is an 8-bit texture read exactly (`texelFetch` on WebGL2); the shader
looks it up in the light table and the palette, both textures. The canvas at 8 bits is an 8-bit
target, shown through the palette by a last pass.

The shaders are built for each backend (MSL, SPIR-V, DXIL, GLSL ES 3.0) and kept in the `.qbx` beside
its code.

## The host's side

`QxHost` gains `gfx_open`, as it has `audio_open`: the host keeps the window, the GPU device and the
frame's timing, and hands the backend what it needs to draw and show. No GPU: the software backend.

`SCREEN`'s page, with `PSET`, `LINE` and `PRINT`, is QB's dialect's alone. A program in
QuickrBasic's dialect has `gfx` instead, so there is one canvas and nothing to layer: `gfx` has the
display from `open` to `close`.

## Tests

`gfx.draws` counts the frame's commands and `gfx.drawn` gives each after every transform: what was
drawn, where, how turned, with which pen. Tests assert on that, and pass on every backend.

`gfx.read` of the canvas after `gfx.flip` gives a finished frame's pixels. Tests that compare pixels
run on the software backend.

## Order

1. The core: canvas, displays and modes, colour, drawing and batching, textured triangles, pixel
   stages; the software backend in C at 8 and 32 bits.
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
| Allegro 5 | the same library moved to GPUs: `al_draw_prim`'s vertices, shaders, display modes; and its traps, above |
| Quake 2's refresh | one game, a software and a GL renderer behind one scene-level interface, so each keeps its own way of hiding surfaces |
| Glide, Direct3D 7's stages, the GameCube's TEV | fixed combine stages that hardware and software both run: the pixel stages |
| sokol_gfx | resources, pipelines and passes over GL, WebGL2, Metal and D3D11; one shader source compiled for each |
| bgfx | a sort key on each draw, for batching; a backend that draws nothing, for tests |
| raylib | `Camera2D`; `DrawTexturePro`'s source, origin and rotation; batching over GL 1.1 to ES3 |
| LÖVE | `Quad`, `Canvas`, `SpriteBatch`, `Mesh`, `Shader` and the fullscreen modes, behind a scripting language |
| PICO-8, TIC-80 | palette swaps, `camera`, `clip`, flipped sprites, `tline` for turned and mode-7 drawing |
| TinyGL | an OpenGL subset in software, small and in C, at 8, 16, 24 and 32 bits: the C backend's size |

## Open questions

1. Decided: `SCREEN`'s page is QB's dialect's alone, so there is nothing to layer.
2. The program's own shaders: GLSL ES as it is, or a BASIC subset translated to it?
3. Depth in software: a 16-bit z, or 1/z as Quake's?
4. 15 bits: worth its fillers, or 16 alone?
5. Vertex procedures: on the CPU everywhere (as above), or translated to vertex shaders on a GPU?
6. 3D worlds: a scene-level call as Quake 2's, so software keeps its own way of hiding surfaces, or
   triangles with `ORDERED` alone?
