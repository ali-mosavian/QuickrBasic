# Graphics extension: design

`gfx` is a native extension ([native-extensions-design.md](native-extensions-design.md)) for 2D and
3D games. The program draws through one API; each platform's backend draws its own best way: the GPU
where there is one, spans in software on DOS. The pixels are not the same everywhere, and are not
meant to be: the rules below are.

Status: a design. The interface is drafted in [src/ext/gfx](../src/ext/gfx); nothing is built.

## What the program sees

```basic
from gfx import display, surface, pen, draw, scene

display.open 320, 200, 8, display.VSYNC
sheet = surface.load("CHARLIE.BMP")
display.palette_of 0, sheet
n = surface.grid(sheet, 16, 16, tile())
hero = surface.view(sheet, 0, 96, 30, 42)
walls = scene.new_material(scene.LIGHTMAPPED, stone, lightmap)
do
    scene.camera eye, at, up, 1.05
    scene.mesh level, walls, scene.ORDERED
    scene.place m, dog.at, dog.turn, 1
    scene.mesh_placed dog.shape, fur, dog.frame, m, 0
    draw.camera cam.x, cam.y
    draw.tiles lv.map(), lv.w, 0, 0, 16, 16, 0
    draw.sprite hero, p.at.x, p.at.y, draw.FLIPX
    draw.rect 0, 0, 319, 9, CINK, draw.FILL
    draw.text 4, 1, "SCORE" + str$(score), CWHITE
    display.flip
loop
```

## The interface

`gfx` is a package of five modules, each an interface file in [src/ext/gfx](../src/ext/gfx): every
procedure, record and constant.

| Module | Holds |
|---|---|
| `gfx.display` | the canvas and the screen: `open`, `flip`, modes, full screen, events, the clock, palettes; what a frame cost and drew |
| `gfx.surface` | everything drawn from or into: `new`, `load`, `view`, `grid`, `free`; pixels read and written |
| `gfx.pen` | where drawing goes and the numbers its flags use: `target`, `clip`, `light`, `tint`, `alpha`, `recolor`, `font` |
| `gfx.draw` | 2D: shapes, sprites, tiles, strips, text, its camera |
| `gfx.scene` | 3D: meshes, materials, billboards, its camera, matrices |

No module imports another. `draw` and `scene` sit over `pen`, and everything over `surface`.

- **QuickrBasic's dialect.** It has no `SCREEN`, `PRINT`, `LINE`, `CIRCLE`, `PSET`, `OPEN` or
  `CLOSE` statements: those are QB's dialect's, and here the standard library's extensions. So
  `draw.line`, `display.open` and `surface.view` are plain names, and the interfaces are read in this
  dialect.
- **The module is the noun**: `surface.load`, `display.flip`, `draw.sprite`, `scene.mesh`.
- **Neither too low nor too high.** A call is a thing a game draws: a sprite, a tile layer, a mesh
  with its material. Stages, matrices set by hand, depth modes and the surface cache stay inside.
- **Everything is a surface**: the canvas (`surface.CANVAS`), a loaded image, a view over part of
  either. A sprite, a tile, a glyph sheet and a texture are surfaces; an atlas is an image and the
  views cut from it, by hand (`surface.view`) or as a grid (`surface.grid`).
- **Handles** are longs: a kind (4 bits), a generation (8) and a slot (20), so a freed or wrong
  handle is an error, never a stray pointer. 0 is none.
- **Flags pick the effect, the pen holds its numbers**, as µGL's `tfxBlit` and `tfxSet*`. Every
  draw call takes flags from one set: `draw`'s `FILL`, `FLIPX`, `FLIPY`; `pen`'s `OPAQUE`, `BLEND`,
  `ADD`, `SOLID`, `SMOOTH`; `scene`'s `ORDERED`, `NO_ZWRITE`, `NO_ZTEST`. The pen's numbers land in
  the vertices, so a change does not end a batch; `pen.reset` sets it back.
- **No optional arguments**, so the common case is its own call, as µGL's `uglPut` and
  `uglPutRotScl`: `sprite` and `sprite_turned`, `camera` and `camera_turned`, `mesh` and
  `mesh_placed`.
- **A colour is the canvas's**: an index at 8 bits, `&HRRGGBB` at 15, 16 and 32. `display.rgb` gives
  one from red, green and blue.
- **Angles are radians**, the 3D camera's field of view too.
- **Records** cross as the ABI has them: by reference, valid for the call, laid out as BASIC lays
  them (the glue asserts each size). An array (`Vec2`, `Vertex`, a map's handles) is read during the
  call, or copied when kept (a mesh). `Matrix` is qgl's `Mat4`: row vectors, the move in `m41` to
  `m43`; `scene.place` and `scene.join` make them. A list (modes, commands drawn) is a count and a
  call for each: `modes` then `mode`, `draws` then `drawn`.
- **Names do not collide.** A type is never named as a procedure (`ScreenMode` beside `mode`), and
  a parameter never as a procedure or a constant.

| Error | When |
|---|---|
| 5 Illegal function call | a handle freed or of another kind; an argument out of its range; drawing before `open`; a view outside its parent; a file not an image |
| 7 Out of memory | the host has none left |
| 9 Subscript out of range | `first` and `n` past an array; `views()` shorter than the grid; a mode past the list |
| 53 File not found | `surface.load` |
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

`display.open w, h, depth, flags` asks for a canvas; the backend gives the nearest it has, and
`surface.width`, `height` and `bits` of `surface.CANVAS` say what it gave. Depth is 8 (indexed), 15,
16 or 32 bits.

The canvas is what the program draws in; the window is the host's. The host scales the canvas to the
window: by whole multiples, to fit (`FIT`), or stretched (`STRETCH`). With `NATIVE` a GPU
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

- `display.count` counts the monitors; `display.modes(monitor)` counts its modes and `display.mode`
  gives each one's width, height, depth and refresh.
- `display.open` opens a window. `display.fullscreen monitor, n` shows the canvas full screen: n 0
  in the desktop's own mode, 1 on in the list's, switched to. `display.windowed` goes back.
- With `VSYNC`, `display.flip` waits for the screen's refresh. `display.clock` is the host's own
  clock in seconds, and `display.refresh` the screen's rate.
- `display.poll` reports what the host saw: the window resized, the display changed, focus lost, the GPU
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
| direct texture | matched to the palette as it is drawn, through a 32 KB map | its colour, multiplied by the light |

- Light is a level from 0 to 63. On an 8-bit canvas it reads a light table, 64 rows of 256 indices:
  Quake's colormap. On the others it multiplies.
- `display.palette row, colours()` sets a palette row, and `display.palette_of` takes an image's: row 0
  is the canvas's, and `pen.recolor row` draws what follows through another, as PICO-8's `pal`. On DOS
  at 8 bits the VGA's palette changes the whole screen at once; the program does not rely on that.
- Index 0 of an indexed texture, 0 of a 15 or 16-bit one, and alpha under one half of a 32-bit
  one, are not drawn; `OPAQUE` draws them.
- `display.rgb(r, g, b)` is a colour in the canvas's own terms: the nearest index at 8 bits.
- `SOLID` draws a surface's shape in the pen's tint: a hit's flash, a shadow, a one-colour font.

Blending is keyed (as above), opaque, translucent (`BLEND`) or additive (`ADD`), by the pen's alpha. A
GPU blends in hardware; software at 8 bits reads a 64 KB table for each mode, index by index; at 15, 16
and 32 bits it adds and multiplies.

## Drawing

The program draws as with µGL or Allegro: any call, at any time, into the screen or any surface, in the
order written. `display.flip` shows the frame. Software draws each call as it comes; for a GPU the
extension records them, and the host plays them at the flip, or when a surface drawn into is read.

Textures, meshes and canvases are uploaded once and named by a handle. A canvas is a texture that can be
drawn into: a parallax layer, a HUD kept between frames, a mirror.

One frame may hold a 3D pass and then a 2D pass: 3D calls take the 3D camera, the rest the 2D one
(`draw.camera`, `draw.camera_turned`), applied as commands are recorded.

## Friendly, and kind to a GPU

Each convenience of µGL and Allegro is kept, and given a shape a GPU draws in few batches.

| The program writes | The backend does |
|---|---|
| a picture loaded on its own | packs it into an atlas page as it loads: the handle is a view, and sprites of one page draw as one batch |
| `draw.pset`, `line`, `rect`, `circle` | draws them as triangles from a white texel kept in every page, so they join the sprites' batch |
| a light, tint, palette or flip for one sprite | puts it in the sprite's vertices; a palette is a row of the palette texture, so a swap does not split the batch |
| a surface made or freed mid-frame | takes it from a pool of views in pages; nothing is allocated on the GPU in the frame |
| pixels written one by one into a surface | keeps that surface in memory and uploads its changed rows once, when it is next drawn from |

A batch ends at another page, material, blend, target or clip. Order is kept, so the batches are the
program's to shape, and `display.stats` counts them.

What would make a GPU wait is a call of its own, never a side effect of drawing:

- `surface.point` and `surface.read`, on a surface the GPU drew, wait for it to finish. The canvas
  read after `display.flip` is the frame shown.
- `display.stats` counts the frame's draws, batches, uploads and waits, so a slow frame says why.

Allegro 5's traps are the reverse of these: a memory bitmap drawn to the screen was uploaded at every
draw, unseen, and its sprites batched only when held. Here where a surface lives is the backend's
choice, and batching is always on.

`PUT`'s `XOR`, `AND` and `OR` are not offered: Metal and WebGL2 have no logic operations. They stay
QB's dialect's.

## Views

A view is a surface over another surface's pixels, as µGL's `uglNewView` and qgl's `qglNewView`: its
own shape and clipping, no memory of its own, nothing copied.

- A picture is a view of its sheet, with an anchor (`surface.anchor`). `surface.view` cuts one by
  hand; `surface.grid` fills an array with a sheet's equal cells: tiles, glyphs, an animation.
- Drawing into a view of a canvas draws into its rectangle, clipped to it: a split screen, a minimap,
  the HUD's strip.
- `surface.aim` moves a view within its parent without allocating. The backend's surface cache is
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

- `draw.sprite s, x, y, flags`: a surface at its anchor. `draw.sprite_turned` adds an angle and a
  scale, about the anchor. `FLIPX` and `FLIPY` swap its u or v.
- `draw.quad`: a surface onto any four corners.
- `draw.tiles`: a tile layer, a cell a surface, drawn a row of quads at a time; only the cells in
  view are visited.
- `draw.strip`: a textured strip along a line of points: the ground under a platformer's feet, drawn
  from the same points its physics walks, at any slope.
- `draw.pset`, `line` and `arc`; `rect`, `circle` and `poly`, outlined or with `FILL`.
- `draw.text x, y, s$, colour` in the pen's font: a sheet of glyphs (`draw.new_font`), or the
  built-in 8 by 8. Its glyphs are pictures, in `colour`, or as the sheet has them with
  `draw.AS_DRAWN`.

The software backend picks fast paths itself: an upright sprite at 1:1 is a keyed row copy, a tile row
a tight loop. The program never chooses.

An atlas's image comes from the tool that packs it, which also writes the BASIC that cuts its views
and sets their anchors.

## 3D

- **Meshes**: a position, the texture's u and v, the lightmap's, a light and a tint for each
  vertex; triangles by index.
- **Cameras**: `scene.camera eye, at, up, fov` for `scene`'s calls, `draw.camera` for `draw`'s. So
  one frame holds both with no switch between. `scene.place` makes a matrix from a position and
  angles. The near plane is clipped by the GPU, and in software by clipping each polygon before it
  is scanned.
- **Depth** is tested and written by every `scene` call. `NO_ZTEST` and `NO_ZWRITE` leave one out.
  `ORDERED` says the calls come nearest first: a GPU rejects hidden pixels early; software keeps a
  coverage buffer and skips spans already drawn, which on a 486 replaces testing each pixel's depth.
- **Materials**: a kind and its textures, made once (`scene.new_material`). The kinds are lists of
  pixel stages ([Shaders](#shaders)), the same on every backend: flat, textured, vertex-lit,
  lightmapped, warped (water) and sky; keyed and translucent are flags. A lightmapped surface is two
  textures multiplied on a GPU, and a cache of surfaces lit once in software, as Quake's.
- **Mip levels**: built when a texture is loaded; a GPU picks one for each pixel, software for each
  polygon.
- **Models**: a mesh of keyframes, drawn by `scene.mesh_placed` at a matrix and a fractional frame,
  blended between two: in the vertex shader on a GPU, on the CPU in software.
- **Billboards and particles**: quads facing the camera, with depth.

The software side of all of this is qcport's: its scanner (`qglRsPoly`), clipper (`qglClPoly`),
surface builder (`qglSbBuild`), coverage and depth. It moves from 16-bit real mode with EMS to the DOS
build's 32-bit flat memory.

## Shaders

QuickrBasic runs BASIC as bytecode, so a shader written in BASIC would be interpreted at every pixel:
too slow in software. Shaders come in three kinds, by where they can run.

- **Vertex procedures, everywhere.** A BASIC `SUB` given a mesh's vertices and the frame's values:
  a wave, a wobble, a bend. In software the interpreter runs it before the mesh is drawn, and a 486
  affords it for hundreds of vertices a frame. On a GPU it is translated to a vertex shader, so it
  is written in the BASIC that translates: arithmetic and the built-in functions on its own
  arguments. The built-in vertex work, transform, keyframe blend and billboards, is the backend's.
- **Pixel stages, everywhere.** A draw's stages are bits, each a step every backend has: read a
  texture, its u and v warped or not, nearest or smooth; through a palette row; through the light;
  times a tint, or the tint alone; times a second texture; key; blend or add. A kind of material is
  a fixed set of them. Software picks a filler by the bits ([The core](#the-core)). On a GPU the
  bits ride in the vertices beside the light, tint and palette row, and the host has one shader a
  backend that reads them: nothing compiles as the program runs, and a batch ends only at another
  blend, page or target.
- **The program's own shaders, on a GPU only.** GLSL ES 3.0 as it is, as WebGL2 takes it; an SDL
  host crosses it to SPIR-V, MSL and DXIL (glslang, SDL_shadercross). A program that uses one also
  names a kind, drawn where there are no shaders.

`display.caps` says what the backend has: a GPU, the program's own shaders, smooth sampling.
`scene.cost` gives a cost for each kind of material, so the program picks its own detail.

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
passes and draws, already batched. At `display.flip` the host's display thread plays it with SDL_GPU, or
with WebGL2 from the page's JavaScript. The list's format is part of the ABI. Software stays in the
extension, and draws in memory the host lends.

On a GPU an indexed texture is an 8-bit texture read exactly (`texelFetch`); the shader looks it up
in the palette, a texture, and multiplies by the light and tint. Every target is RGBA, the canvas at
8 bits too, so a GPU blends in hardware: a palette set later colours only what is drawn after, and
reading an 8-bit canvas gives each pixel's nearest index.

The one shader is built with each host: MSL, SPIR-V and DXIL for SDL_GPU, GLSL ES 3.0 in the page.

## The host's side

`QxHost` grows by a minor, as it has `audio_open`:

```c
typedef struct {            /* asked, then what the host gives */
    uint16_t w, h;          /* the canvas */
    uint8_t depth, flags;   /* 8, 15, 16 or 32; display's flags */
    bool gpu;               /* given: the host plays lists, else it lends pages */
    uint16_t out_w, out_h;  /* given: the pixels a NATIVE canvas is drawn at */
    float hz;               /* given: the refresh, 0 unknown */
} QxScreen;
bool (*gfx_open)(QxScreen *s);
void (*gfx_close)(void);
void *(*gfx_page)(uint32_t *pitch);           /* the page to draw the next frame in */
void (*gfx_show)(const uint32_t *palette);    /* it is shown; at 8 bits, with these 256 colours */
void (*gfx_play)(const uint32_t *list, uint32_t words);
void (*gfx_read)(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
uint32_t (*gfx_modes)(uint32_t monitor, QxMode *out, uint32_t most);
bool (*gfx_mode)(uint32_t monitor, uint32_t n);
bool (*gfx_event)(QxEvent *e);
uint64_t (*clock_us)(void);
void *(*file_read)(const char *path, uint32_t len, uint32_t *bytes);
```

- **Software draws in memory the host lends.** `gfx_page` is the page to draw in; `gfx_show` shows
  it. Today's hosts copy the page shown and lend it again, its pixels kept; VBE's two pages, on DOS,
  will revisit that. No struct crosses by value: i386's conventions for one differ.
- **A GPU's host plays lists.** `gfx_play` returns when the host is done with the list's bytes;
  `gfx_read` waits for what was played.
- **Calls come from the interpreter's thread.** `gfx_show` and `gfx_play` hand the frame to the
  display's thread and wait for it: with `VSYNC`, until the refresh.
- **The host shows one screen**: the runtime's (the editor, a QB program's `SCREEN`) or `gfx`'s,
  from `display.open` to `display.close` or the run's end.
- **`file_read`** gives a file whole, for `surface.load`: a slice has no C library to open one.

| | Gives | Shows by | Clock |
|---|---|---|---|
| DOS | pages: a VBE 2.0 linear mode's two in video memory; else memory | 4F07h's flip at the retrace; else a copy, linear or bank by bank; mode 13h a copy | the PIT's count |
| Mac, Linux, Windows | a GPU, lists played on SDL's main thread; with no device, pages | the swapchain; a page as today's frame, a texture | `SDL_GetTicksNS` |
| The browser | a GPU, lists played by the page with WebGL2 from the shared memory; else pages | `requestAnimationFrame` | `performance.now` |
| `basic` in a terminal, unit tests | pages | nothing: `surface.read` is the tests' eye | `pclk` |

### One file, five interfaces

A `.qbx` holds one interface today. `gfx.qbx` holds a table of them, each a module's name and
source, over one slice a platform: the modules share the surfaces, the pen and the recorder.

- `IMPORT gfx.draw` looks for `gfx/draw.bas`, `gfx/draw.qbx`, then `gfx.qbx`'s part `draw`.
- One `qx_init_gfx`. An entry is named with its module, `draw.sprite`, and so is a type.
- The glue reads every interface of the package: C functions `gfx_draw_sprite`, records
  `gfx_scene_vertex`. It joins a DECLARE's `_` lines, and no C name can meet the C library's.
- The file format's version is 2. A version 1 file is one unnamed interface.

## The core

C, in `src/ext/gfx`, the same on every platform. `gfx.c` alone holds the state; each module under it
takes its own part. Memory is the host's (`alloc`), taken when first used: nothing is static.

| Module | Owns |
|---|---|
| `gfx.c` | the state; what the entries share: errors raised, handles and the pen's target resolved |
| `entry_display`, `entry_surface`, `entry_pen`, `entry_draw` | the entries the glue calls, a part each: arguments checked |
| `mem` | the host's memory, and copies without a C library |
| `handle` | handles: slots and generations |
| `surface` | surfaces, views and anchors; the pages images are packed in |
| `image`, `bmp`, `png`, `inflate` | a file's bytes to pixels: BMP or PNG, by their first bytes; PNG's zlib streams |
| `colour` | palette rows, the light table, the blend tables, the nearest index |
| `pen` | the pen |
| `font` | fonts: sheets cut into glyphs, and the built-in 8 by 8 as one |
| `command` | a draw call to a command, the camera applied; the frame's log |
| `backend`, `backend_soft` | a backend's table, which the state holds for the open screen: an entry draws through it; software's |
| `shape` | a command to vertices: quads and fans |
| `soft/scan` | a convex polygon to spans, clipped |
| `soft/fill`, `fill8`, `fill16`, `fill32` | spans and rows to pixels: a table for each colour format |
| `soft/soft` | the software backend: a command drawn at once |
| `backend_gpu` | a GPU's backend: shapes and pictures as triangles, the pen in their vertices |
| `gpu/gpu` | the GPU's recorder: textures made and sent, passes, clips, reads back |
| `gpu/list` | the list the host plays: its words, vertices and batches |

### From a call to pixels

1. A draw call becomes a command: its kind, target, source and flags, the pen's numbers, and its
   place after the camera. The frame's commands are logged, and `display.drawn` reads the log: the
   same on every backend.
2. `shape` makes its vertices: a place, u and v, light, tint, palette row and stage bits. In
   software an upright sprite at 1:1 skips this: its rows are copied.
3. Software scans and fills at once. For a GPU, `list` adds the vertices to the frame's store; a
   batch is a run with one target, texture, blend, alpha and clip.
4. At `display.flip` software calls `gfx_show`. A GPU's frame becomes a list, and `gfx_play` plays
   it.

The list is words, an op and its size each: the vertices, make a texture, free one, upload rows of
one, the palette rows, a pass (its target), a clip, a draw (first vertex, count, texture, blend,
alpha), show, the end (`src/qx/qxlist.h`). What it points at stays till `gfx_play` returns. A
vertex carries its texel's rectangle, so sprites of one atlas page batch.

### Scanner and fillers

- **One scanner**, in C, that knows no depth: two edge chains walked down from the top vertex, a
  span a row. Two walks, as qgl's `drawA` and `drawP`: affine, u and v 16.16; and perspective, u/z
  and v/z floats, corrected every 16 pixels.
- **A table for each colour format**, as µGL's `CFMT`, `fill_formats` by format. It holds fused
  fillers for the hot mixes, a row filler for each variant of the flags, indexed by them: copied,
  keyed, flipped, both; a span flat, textured, textured and keyed, lit. And it holds the steps of
  µGL's `tfx` row pipeline for every other mix: unpack, colour, mask, blend, pack.
- **Picked once** for each sprite or polygon, from its stage bits, as qgl's `b8_span`: the fused
  filler when there is one, else the pipeline. Nothing below tests a mode.
- **15 and 16 bits are one template**, built twice with other masks; 8 and 32 have their own.
- **On DOS** assembly fills the same table's slots, 8 bits first; a slot with none keeps C's.
- **Depth is 1/z in 16 bits**, as µGL's `uglZ`: larger is nearer, 0 is far, scaled from the
  camera's near plane. It is linear along a span, so no pixel divides.

## Tests

`display.draws` counts the frame's commands and `display.drawn` gives each after every transform: what
was drawn, where, how turned, with which pen. Tests assert on that, and pass on every backend.

`surface.read` of the canvas after `display.flip` gives a finished frame's pixels. Tests that compare
pixels run on the software backend.

## Order

1. The first slice, below.
2. BASIC's binding: the package's loader; the interfaces read in QuickrBasic's dialect (`OPTION
   DIALECT`). Done: `gfx.qbx` in `lib/` beside `basic` and in the Mac's app, `demo/GFXDEMO.BAS`
   its program, `tests/host/gfx.sh` its test.
3. The rest of 2D and the pen: turned sprites, quads, strips, light, tint, blend, recolor; 16 and 32
   bits; PNG; fonts. Done.
4. The hosts' players, SDL_GPU and WebGL2, and the list.
5. DOS: VBE's pages, fillers in assembly. CHARLIE moves onto `gfx`, cutting its views with
   `barkley/assets/sheet.bas`.
6. 3D: `scene`, from qcport.
7. Vertex procedures, and the program's own shaders.

### The first slice

8 bits, in software, seen in the Mac's window. Done.

- **`tools/qbx.py`**: a package's interfaces in one `.qbx`; a DECLARE's `_` lines; C names with
  their module; numeric CONSTs as C's.
- **`QxHost`, minor 1**: `gfx_open`, `gfx_close`, `gfx_page`, `gfx_show`, `clock_us`, `file_read`,
  over `plat/pgfx.h`. The terminal's host lends memory and shows nothing; the Mac's and the
  browser's show the page in the runtime's place; DOS has none yet.
- **The core at 8 bits**: `handle`, `surface` (`new`, `load` of a BMP, `view`, `grid`, `aim`,
  `anchor`, `free`, `point`, `read`, `write`), `colour` (`palette`, `palette_of`, `rgb`), `pen`
  (`target`, `clip`, `reset`), `command`, `soft` (`scan`, `fill8`), `draw` (`clear`, `pset`,
  `line`, `rect`, `sprite` with `FLIPX`, `FLIPY` and `OPAQUE`, `tiles`, `text` in the built-in
  font), `display` (`open`, `close`, `flip`, `clock`, `draws`, `drawn`). The rest raises Advanced
  feature unavailable. `scene` is not glued yet.
- **CHARLIE**: `tools/charlieart.py` writes `CHARLIE.BMP` and `barkley/assets/sheet.bas`, which cuts
  its views.
- **Tests, native** (`tests/ext/gfx`, in the unit tests): each rule by its commands drawn and its
  pixels read. A frame of CHARLIE's pictures by its commands, and against its steps pasted by hand.
  `make gfx-shot` draws that frame in the Mac's window, and `QB_SHOT` takes it.

### The rest of 2D

Step 3, in parts:

- **Shapes and the turned camera**: `circle`, `arc` and `poly`, outlined or filled;
  `camera_turned`. A circle covers the centres within its radius and a half, its outline those
  from the radius less a half, so radius 0 is a pixel. An arc runs clockwise on the screen from
  its start to its finish. An outline draws each pixel once. Under a turned or zoomed camera a
  rect is a polygon.
- **Turned pictures**: `sprite_turned`, `quad` and `strip`, and sprites and tiles under a turned
  camera. `shape` gives a picture's corners with their texels; software fills the polygon, each
  pixel taking the texel at its centre through the affine map three corners give. A quad is two
  triangles, as a GPU draws it. A strip's points are its top edge, its texels upright below,
  repeating along x.
- **The pen at 8 bits**: `light`, `tint`, `alpha` with `BLEND` and `ADD`, `SOLID`, `recolor`; a
  shape's colour goes through them too. The tint is `&HRRGGBB` at every depth. Row 0 gives the
  tables when first needed: a 32 KB map from 5 bits a channel to the nearest index, the light
  table, and a 64 KB blend table for each mode and alpha, the last four kept. A call's light, tint
  and row mix into 256 indices. With nothing to do the fused fillers draw; else the ink does,
  `FILL_CHUNK` texels at a time: fetched, coloured, keyed, blended, stored. Software ignores
  `SMOOTH`.
- **15, 16 and 32 bits**: the canvas at the depth asked, and the hosts' pages and frames too. A
  surface has a format: `surface.new` the canvas's (8 bits before a screen), a view its parent's.
  `fill16.c` is the 15 and 16-bit template, built twice; `fill32.c` its own. The fused fillers copy
  a picture of the target's format; any other goes through the ink: an index through a palette
  row, lit and tinted (`pal_argb`), a direct texel multiplied, then the blend in arithmetic. On an
  8-bit canvas a direct picture is matched to row 0 through the map as it is drawn. A 15 or 16-bit
  texel 0 is the key, as an index 0 is; a 32-bit one's alpha under a half. What is drawn into a
  32-bit surface is opaque, so a new one is clear until drawn on. `surface.point` gives
  `&HRRGGBB`; `read` and `write` the pixels' bytes.
- **PNG**: `surface.load` reads a PNG of any type and depth, not interlaced. An indexed one with at
  most one transparent colour stays 8 bits, that colour swapped with index 0, the key's; the rest
  load at 32 bits, alpha the file's or tRNS's. `inflate.c` reads the zlib stream as zlib's `puff`
  does, a code a bit at a time: small, and fast enough for a game's pictures.
- **Fonts**: `draw.new_font` cuts a sheet into w by h glyphs from code 32, row by row; `pen.font`
  picks one, 0 the built-in. The built-in is the runtime's 8 by 8 drawn into a sheet of its own on
  first use. Each glyph is drawn as a picture, so text turns and zooms with the camera; in a colour
  it is `SOLID`, that colour through the pen as a shape's.

### The GPU's players

Step 4, in parts:

- **The list**: QxHost's minor 2, `gfx_play` and `gfx_read`. A screen the host gives a GPU draws
  through `backend_gpu`: each call's triangles into the frame's list, played at `display.flip`
  and before a read. A texture is named by its root surface's handle, made and sent when first
  drawn from or into, and sent again when the CPU wrote its pixels since (the root's version). A
  root drawn into is ahead on the GPU until a read brings it back. Shapes draw from a white
  texel. Tests run on a host that records the lists.

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

## Decisions

| Question | Decided |
|---|---|
| `SCREEN`'s page and a `gfx` canvas | `SCREEN` is QB's dialect's alone: one canvas, nothing layered |
| The program's own shaders | GLSL ES 3.0 as it is |
| Depth in software | 1/z in 16 bits |
| 15 bits | one template with 16 |
| Vertex procedures | the interpreter in software; translated to a vertex shader on a GPU |
| 3D worlds | meshes drawn nearest first with `ORDERED`; a scene-level call is weighed again at step 6 |
| Names that are QB's keywords | QuickrBasic's dialect has none of them: they are plain names |
| Where the GPU backends live | in the host, playing a list the extension records |
| A sprite's arguments | `sprite` and `sprite_turned`, as µGL's pairs |
| Effects | flags on each draw call; the pen holds their numbers |
| What is drawn | everything is a surface; an atlas is an image and views over it |
| The package | five modules, `display`, `surface`, `pen`, `draw`, `scene`, in one `gfx.qbx` |
| Software's frame | drawn in memory the host lends |
| Images | BMP and PNG |
| Software's pixels | fused fillers for the hot mixes, µGL's `tfx` rows for the rest |
| Stages on a GPU | one shader a backend, the stages bits in the vertices |
