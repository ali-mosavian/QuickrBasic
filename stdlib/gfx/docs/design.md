# Graphics extension: design

`gfx` is a native extension
([native-extensions-design.md](../../../docs/native-extensions-design.md)) for 2D and 3D games. The
program draws through one API; each platform's backend draws its own best way: the GPU where there
is one, spans in software on DOS. The pixels are not the same everywhere, and are not meant to be:
the rules below are.

The rules here hold on every backend. How the source is laid out is in
[architecture.md](architecture.md); the API is the interfaces in [src/api](../src/api). 2D is built;
`scene` is declared only.

## What the program sees

```basic
from gfx import display, surface, draw, scene

canvas& = display.open(320, 200, 8, display.VSYNC)
sheet = surface.load("CHARLIE.BMP")
display.set_palette_from 0, sheet
n = surface.new_views(sheet, 16, 16, tile())
hero = surface.new_view(sheet, 0, 96, 30, 42)
walls = scene.new_material(scene.LIGHTMAPPED, stone, lightmap)
do
    scene.set_camera eye, at, up, 1.05
    scene.mesh canvas&, level, walls, scene.ORDERED
    scene.place m, dog.at, dog.turn, 1
    scene.mesh_placed canvas&, m, dog.frame, dog.shape, fur, 0
    surface.set_camera canvas&, cam.x, cam.y
    draw.tiles canvas&, 0, 0, lv.map(), lv.w, 16, 16, 0
    draw.sprite canvas&, p.at.x, p.at.y, hero, draw.FLIP_X
    draw.rect canvas&, 0, 0, 319, 9, CINK, draw.FILL
    draw.text canvas&, 4, 1, "SCORE" + str$(score), CWHITE
    display.flip
loop
```

## The interface

`gfx` is a package of four modules, each an interface file in [src/api](../src/api): every
procedure, record and constant, each procedure with its docstring.

| Module | Holds |
|---|---|
| `gfx.display` | the screen, the palette and the host: `open`, `flip`, monitors and modes, events, the clock; what a frame cost and drew |
| `gfx.surface` | everything drawn from or into: `new`, `load`, `new_view`, `new_views`, `free`; each one's clip, camera, anchor and depth; pixels read and written |
| `gfx.draw` | 2D: shapes, sprites, tiles, strips, text; the settings every call draws with (`set_light`, `set_tint`, `set_alpha`, `set_palette_row`, `set_font`) |
| `gfx.scene` | 3D: meshes, materials, billboards, its camera, matrices (declared, not built) |

No module imports another; everything sits over `surface`.

- **QB's words as names.** `OPTION DISABLE LINE, CIRCLE, ...` makes QB's statements for devices
  names in a module's lines after it. Each interface disables the ones it names, so `draw.line` and
  `display.open` are its procedures; a program keeps QB's `LINE` and `OPEN`.
- **Names by rule.** The module is the noun, a procedure the verb: `surface.load`, `display.flip`,
  `draw.sprite`. A maker is `new_*` with a `free*`; a setting is `set_*` with a `get_*`. No
  metaphors, no abbreviations.
- **The destination first**, as µGL's `uglPut dst, x, y, src`: every draw call names the surface it
  draws into, then where, then what. A source over the destination's pixels (the surface, or views
  of one) is an illegal function call: a GPU cannot draw a texture into itself.
- **Neither too low nor too high.** A call is a thing a game draws: a sprite, a tile layer, a mesh
  with its material. Stages, matrices set by hand, depth modes and the surface cache stay inside.
- **Everything is a surface**: the screen (`display.open`'s), a loaded image, a view over part of
  either. A sprite, a tile, a glyph sheet and a texture are surfaces; an atlas is an image and the
  views cut from it, by hand (`surface.new_view`) or as a grid (`surface.new_views`). A surface
  carries its own clip, camera, anchor and depth.
- **Handles** are longs: a kind (4 bits), a generation (8) and a slot (20), so a freed or wrong
  handle is an error, never a stray pointer. 0 is none.
- **Flags pick the effect, the settings hold its numbers**, as µGL's `tfxBlit` and `tfxSet*`.
  `draw`'s flags are `FLIP_X`, `FLIP_Y`, `FILL`, `OPAQUE`, `BLEND`, `ADD`, `SOLID`, `SMOOTH`,
  `PERSPECTIVE`, `DEPTH_TEST` and `DEPTH_WRITE`; `scene`'s add `ORDERED`. The settings land in the
  vertices, so a change does not end a batch; `draw.reset` sets them back.
- **No optional arguments**, so the common case is its own call, as µGL's `uglPut` and
  `uglPutRotScl`: `sprite` and `sprite_turned`, `set_camera` and `set_camera_turned`, `mesh` and
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
- A texel is the nearest unless the program asks for `SMOOTH`: the four texels round the pixel's
  centre mixed, a keyed one adding nothing, drawn where half or more is kept. Software mixes them
  as the GPU's shader does.
- A polygon's u and v past a texture's edge repeat it when both its sides are powers of two and it
  is not drawn `SMOOTH`; any other texture is held at its edges.
- Depth is tested and written as the program sets it; commands are drawn in the order given.

What may differ: rounding where a pixel's centre or a sample falls within a precision step of an
edge; perspective, corrected every pixel on a GPU and every 8 or 16 in software; filtering; the
resolution drawn at.

## The canvas: any resolution and depth

`display.open w, h, depth, flags` asks for a screen and returns its surface; the backend gives the
nearest it has, and `surface.width`, `height` and `bits` of that surface say what it gave. Depth is
8 (indexed), 15, 16 or 32 bits.

The canvas is what the program draws in; the window is the host's. The host scales the canvas to the
window: by whole multiples, to fit (`FIT`), or stretched (`STRETCH`). With `NATIVE` a GPU
backend draws at the window's resolution instead: the same commands, more pixels, and sprites turn
smoothly rather than a canvas pixel at a time. Software ignores it.

With `DISCARD` the canvas's pixels are undefined after a flip, for a program that draws all of it
each frame. A 3D card then draws the next frame without first copying the one it shows. Software
keeps them anyway.

On DOS, 320×200 at 8 bits is VGA's mode 13h; the rest are VBE 2.0 modes through a linear
framebuffer, with banked modes for VBE 1.2 cards. A canvas shows in the mode of its depth it fills
most, by whole multiples, centred.

In software the polygon scanner knows nothing of pixels and a filler knows nothing of polygons: one
set of row fillers for each depth, as µGL's `cfmt/b8`, `b15`, `b16` and `b32`, picked once a call.
A new depth is a new set of fillers, never a flag in the scanner.

## Displays and modes

A display mode is what the screen shows; the canvas is shown in it, scaled. So a 320×200 canvas can
be shown in any mode.

- `display.count` counts the monitors; `display.modes(monitor)` counts its modes and `display.mode`
  gives each one's width, height, depth and refresh.
- `display.open` opens a window. `display.set_fullscreen monitor, n` shows the canvas full screen: n
  0 in the desktop's own mode, 1 on in the list's, switched to. `display.set_windowed` goes back.
- With `VSYNC`, `display.flip` waits for the screen's refresh. `display.clock` is the host's own
  clock in seconds, and `display.refresh` the screen's rate.
- `display.poll` reports what the host saw: the window resized, the display changed, focus lost, the
  GPU lost and back, the window closed. The extension holds every texture, mesh and canvas, so it
  uploads them again itself; a canvas drawn into is lost, and the program draws it again.

| | Modes listed | Set by |
|---|---|---|
| DOS | mode 13h, and VBE's list (4F00h, 4F01h) | 4F02h; pages flipped by 4F07h at the retrace |
| Mac, Linux, Windows | `SDL_GetFullscreenDisplayModes` | `SDL_SetWindowFullscreenMode`, or a window |
| The browser | one, the page's | none; full screen by the Fullscreen API, after a click or a key |

On DOS a mode is the whole screen and the one display. A 3D card shows the screen centred in the
smallest mode that holds it: a Voodoo 1 or 2 in a timing of its own, from Glide's tables; a Banshee
or Voodoo 3 in the VESA mode its BIOS sets.

## Colour

A texture is indexed (8 bits and a palette) or direct (16 or 32 bits).

| | 8-bit canvas | 15, 16 or 32-bit canvas |
|---|---|---|
| indexed texture | its index through the light table; stays an index | index, light table, then the palette, as it is drawn |
| direct texture | matched to the palette as it is drawn, through a 32 KB map | its colour, multiplied by the light |

- Light is a level from 0 to 63. On an 8-bit canvas it reads a light table, 64 rows of 256 indices:
  Quake's colormap. On the others it multiplies.
- `display.set_palette row, colours()` sets a palette row, and `display.set_palette_from` takes an
  image's: row 0 is the screen's, and `draw.set_palette_row row` draws what follows through another,
  as PICO-8's `pal`. On DOS at 8 bits the VGA's palette changes the whole screen at once; the
  program does not rely on that.
- Index 0 of an indexed texture, 0 of a 15 or 16-bit one, and alpha under one half of a 32-bit
  one, are not drawn; `OPAQUE` draws them.
- `display.rgb(r, g, b)` is a colour in the canvas's own terms: the nearest index at 8 bits.
- `SOLID` draws a surface's shape in `set_tint`'s colour: a hit's flash, a shadow, a one-colour
  font.

Blending is keyed (as above), opaque, translucent (`BLEND`) or additive (`ADD`), by `set_alpha`'s
level. A GPU blends in hardware; software at 8 bits reads a 64 KB table for each mode, index by
index; at 15, 16 and 32 bits it adds and multiplies.

## Drawing

The program draws as with µGL or Allegro: any call, at any time, into the screen or any surface, in
the order written. `display.flip` shows the frame. Software draws each call as it comes; for a GPU
the extension records them, and the host plays them at the flip, or when a surface drawn into is
read.

Textures, meshes and canvases are uploaded once and named by a handle. A canvas is a texture that
can be drawn into: a parallax layer, a HUD kept between frames, a mirror.

One frame may hold a 3D pass and then a 2D pass: 3D calls take the 3D camera, the rest the 2D one
(`surface.set_camera`, `surface.set_camera_turned`), a surface's own,, applied as commands are
recorded.

## Friendly, and kind to a GPU

Each convenience of µGL and Allegro is kept, and given a shape a GPU draws in few batches.

| The program writes | The backend does |
|---|---|
| an image loaded on its own | packs it into an atlas page as it loads: the handle is a view, and sprites of one page draw as one batch |
| `draw.pset`, `line`, `rect`, `circle` | draws them as triangles from a white texel kept in every page, so they join the sprites' batch |
| a light, tint, palette or flip for one sprite | puts it in the sprite's vertices; a palette is a row of the palette texture, so a swap does not split the batch |
| a surface made or freed mid-frame | takes it from a pool of views in pages; nothing is allocated on the GPU in the frame |
| pixels written one by one into a surface | keeps that surface in memory and uploads its changed rows once, when it is next drawn from |

A batch ends at another page, material, blend, target or clip. Order is kept, so the batches are the
program's to shape, and `display.get_stats` counts them.

What would make a GPU wait is a call of its own, never a side effect of drawing:

- `surface.point` and `surface.read`, on a surface the GPU drew, wait for it to finish. The canvas
  read after `display.flip` is the frame shown.
- `display.get_stats` counts the frame's draws, batches, uploads and waits, so a slow frame says
  why.

Allegro 5's traps are the reverse of these: a memory bitmap drawn to the screen was uploaded at
every draw, unseen, and its sprites batched only when held. Here where a surface lives is the
backend's choice, and batching is always on.

`PUT`'s `XOR`, `AND` and `OR` are not offered: Metal and WebGL2 have no logic operations. They stay
QB's `PUT`'s.

## Views

A view is a surface over another surface's pixels, as µGL's `uglNewView` and qgl's `qglNewView`: its
own shape and clipping, no memory of its own, nothing copied.

- A sprite is a view of its sheet, with an anchor (`surface.set_anchor`). `surface.new_view` cuts
  one by hand; `surface.new_views` fills an array with a sheet's equal cells: tiles, glyphs, an
  animation.
- Drawing into a view of a canvas draws into its rectangle, clipped to it: a split screen, a
  minimap, the HUD's strip.
- `surface.set_view` moves a view within its parent without allocating. The backend's surface cache
  is built on it, as qgl's: n surfaces of one size, and one view aimed at each in turn.
- Wrapping repeats a view, not its parent, so a tile in an atlas tiles.
- A surface's views are freed with it. A handle to one is then stale, and using it is an error.

On a GPU a view of a texture is a rectangle of texture coordinates, and drawing into one sets the
viewport and scissor to it; a wrapping view is repeated in the shader, within its rectangle. In
software a view is an offset and a stride, as µGL's.

The cache's views own their stride: in software its surfaces are packed end to end, with no gaps.
On a GPU a lightmapped surface is two textures multiplied, so there is no cache.

## 2D

Everything 2D is a textured quad or a strip of them, so the backends have one primitive.

- `draw.sprite dst, x, y, src, flags`: src at its anchor. `draw.sprite_turned` adds an angle and a
  scale, about the anchor. `FLIP_X` and `FLIP_Y` swap its u or v.
- `draw.quad`: a surface onto any four corners.
- `draw.tiles`: a tile layer, a cell a surface, drawn a row of quads at a time; only the cells in
  view are visited.
- `draw.strip`: a textured strip along a line of points: the ground under a platformer's feet, drawn
  from the same points its physics walks, at any slope.
- `draw.pset`, `line` and `arc`; `rect`, `circle` and `poly`, outlined or with `FILL`.
- `draw.poly s, points(), first, n, colour, flags`: a convex polygon of up to 32 `Vertex`es, each x,
  y, z (its distance, 1 or more) and u, v. `s` 0 draws in colour, else `s`'s texels, affine or with
  `PERSPECTIVE` in perspective. A texture whose sides are both powers of two repeats past its edges,
  unless `SMOOTH`; any other is held at them. `DEPTH_TEST` draws only what is nearer than the
  target's depth there and `DEPTH_WRITE` writes it; a keyed texel writes none.
  `surface.clear_depth s` makes s's depth all far. Software only so far: a GPU draws them affine,
  with no depth.
- `draw.text dst, x, y, s$, colour` in `set_font`'s font: a sheet of glyphs (`draw.new_font`), or
  the built-in 8 by 8. Its glyphs are sprites, in `colour`, or as the sheet has them with
  `draw.AS_DRAWN`.

The software backend picks fast paths itself: an upright sprite at 1:1 is a keyed row copy, a tile
row a tight loop. The program never chooses.

An atlas's image comes from the tool that packs it, which also writes the BASIC that cuts its views
and sets their anchors.

## 3D

- **Meshes**: a position, the texture's u and v, the lightmap's, a light and a tint for each
  vertex; triangles by index.
- **Cameras**: `scene.set_camera eye, at, up, fov` for `scene`'s calls; a surface's own
  (`surface.set_camera`) for `draw`'s. So one frame holds both with no switch between. `scene.place`
  makes a matrix from a position and angles. The near plane is clipped by the GPU, and in software
  by clipping each polygon before it is scanned.
- **Depth** is a surface's: tested with `DEPTH_TEST`, written with `DEPTH_WRITE`, cleared by
  `surface.clear_depth`. `ORDERED` says the calls come nearest first: a GPU rejects hidden pixels
  early; software keeps a coverage buffer and skips spans already drawn, which on a 486 replaces
  testing each pixel's depth.
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
surface builder (`qglSbBuild`), coverage and depth. It moves from 16-bit real mode with EMS to the
DOS build's 32-bit flat memory.

## Shaders

QuickrBasic runs BASIC as bytecode, so a shader written in BASIC would be interpreted at every
pixel: too slow in software. Shaders come in three kinds, by where they can run.

- **Vertex procedures, everywhere.** A BASIC `SUB` given a mesh's vertices and the frame's values:
  a wave, a wobble, a bend. In software the interpreter runs it before the mesh is drawn, and a 486
  affords it for hundreds of vertices a frame. On a GPU it is translated to a vertex shader, so it
  is written in the BASIC that translates: arithmetic and the built-in functions on its own
  arguments. The built-in vertex work, transform, keyframe blend and billboards, is the backend's.
- **Pixel stages, everywhere.** A draw's stages are bits, each a step every backend has: read a
  texture, its u and v warped or not, nearest or smooth; through a palette row; through the light;
  times a tint, or the tint alone; times a second texture; key; blend or add. A kind of material is
  a fixed set of them. Software picks a filler by the bits ([Software](architecture.md#software)).
  On a GPU the bits ride in the vertices beside the light, tint and palette row, and the host has
  one shader a backend that reads them: nothing compiles as the program runs, and a batch ends only
  at another blend, page or target.
- **The program's own shaders, on a GPU only.** GLSL ES 3.0 as it is, as WebGL2 takes it; an SDL
  host crosses it to SPIR-V, MSL and DXIL (glslang, SDL_shadercross). A program that uses one also
  names a kind, drawn where there are no shaders.

`display.features` says what the backend has: a GPU, the program's own shaders, smooth sampling.
`scene.cost` gives a cost for each kind of material, so the program picks its own detail.

## Backends

| Platform | Backend |
|---|---|
| DOS | software, VGA and VBE; a 3D card's fixed-function pipeline: 3dfx's Voodoo 1, 2, Banshee and 3 |
| Mac, Linux, Windows | SDL_GPU: Metal, Vulkan, Direct3D 12 |
| The browser | WebGL2 |
| Tests, and a machine with no GPU | software in C, drawing into a texture the host shows |

SDL's 2D renderer is not enough: `SDL_RenderGeometry` has no depth and no vertex shader, so 3D needs
SDL_GPU, and one backend serves both.

The GPU backends are the extension's; the host lends a GPU, knowing nothing of gfx. A slice imports
nothing, so it cannot call SDL_GPU or WebGL2, and both hosts show the screen from another thread:
SDL's main thread, and the page's, which reads the shared memory (`frame()`). So the extension
records a frame as a GPU list in its own memory: buffers and textures made and written, pipelines
from its own shaders, passes and draws, already batched. At `display.flip` the host plays it with
SDL_GPU on the interpreter's thread, or with WebGL2 from the page's JavaScript. The list's format is
part of the ABI (`src/qx/qxlist.h`) and generic: another extension could draw through it. Software
stays in the extension, and draws in memory the host lends.

On a GPU an indexed texture is an 8-bit texture read exactly (`texelFetch`); the shader looks it up
in the palette, a texture, and multiplies by the light and tint. Every target is RGBA, the canvas at
8 bits too, so a GPU blends in hardware: a palette set later colours only what is drawn after, and
reading an 8-bit canvas gives each pixel's nearest index.

The one shader is written once, in Vulkan GLSL (`src/backend/gpu/shaders`), its vertex stage built
twice: for plain vertices and, with `DISC`, for a circle's. `make shaders` crosses it with glslang
and SPIRV-Cross, pinned in `tools/deps.sh`, to SPIR-V for SDL_GPU's Vulkan, MSL for its Metal and
ESSL 3.00 for WebGL2 (`shaders.h`), committed so a build needs neither. `display.open` names the
languages it brings, and the host answers with the one it takes.

A card of DOS's time has no shaders: its pipeline is fixed, a texel times a colour, keyed, blended
and depth tested. The list names that pipeline too (minor 5), and gfx works out on the CPU what its
shader would: a vertex's colour lit, a disc as its rows. DOS plays the list through the driver for
the card on the bus, loaded from `lib/drivers/` as the screen opens: only that one
(`src/plat/dos32/accel`).

From minor 6 a fixed host draws 8 bits too. An 8-bit root is an INDEX texture, a byte a texel on
the card, drawn through the card's 256-entry texture palette. gfx sends that palette's words per
draw (LOOKUP): the lut's indices into an 8-bit target, the row's colours into any other. A palette
change costs 1 KB, not the textures sent again, so fades and cycling stay cheap. A card shows
colours, so the 8-bit screen is held as row 0's colours (COLOURS), exact once redrawn. Other 8-bit
targets hold each index as a 565 word both of whose bytes are the index, read back exactly. Blends
at 8 bits are the card's, not the palette's: on the screen in colour, elsewhere drawn where alpha is
at least a half. SMOOTH at 8 bits is nearest. Read back, the screen is the nearest index.

## The host's side

`QxHost` grows by a minor, as it has `audio_open`:

```c
typedef struct {            /* asked, then what the host gives */
    uint16_t w, h;          /* the canvas */
    uint8_t depth, flags;   /* 8, 15, 16 or 32; display's flags */
    uint8_t shaders;        /* the shading languages the extension brings */
    bool gpu;               /* given: the host plays GPU lists, else it lends pages */
    QxShader shader;        /* given with gpu: the language it takes */
    bool y_down;            /* given with gpu: clip space's y runs down a texture's rows */
    uint16_t out_w, out_h;  /* given: the pixels a NATIVE canvas is drawn at */
    float hz;               /* given: the refresh, 0 unknown */
} QxScreen;
bool (*gfx_open)(QxScreen *s);
void (*gfx_close)(void);
void *(*gfx_page)(uint32_t *pitch);           /* the page to draw the next frame in */
void (*gfx_show)(const uint32_t *palette);    /* it is shown; at 8 bits, with these 256 colours */
void (*gpu_play)(const uint32_t *list, uint32_t words);
void (*gpu_read)(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
uint32_t (*gfx_monitors)(void);              /* minor 3: monitors and modes from 1 */
uint32_t (*gfx_modes)(uint32_t monitor, QxMode *out, uint32_t most);
bool (*gfx_mode)(uint32_t monitor, uint32_t n); /* n 0 the desktop's; monitor 0 a window */
bool (*gfx_event)(QxEvent *e);
uint64_t (*clock_us)(void);
void *(*file_read)(const char *path, uint32_t len, uint32_t *bytes);
```

- **Software draws in memory the host lends.** `gfx_page` is the page to draw in; `gfx_show` shows
  it. The hosts copy the page shown and lend it again, its pixels kept; DOS's copy goes to video
  memory. No struct crosses by value: i386's conventions for one differ.
- **A GPU's host plays lists.** `gpu_play` returns when the host is done with the list's bytes;
  `gpu_read` waits for what was played.
- **Calls come from the interpreter's thread.** `gfx_show` and `gpu_play` hand the frame to the
  display's thread and wait for it: with `VSYNC`, until the refresh.
- **The host shows one screen**: the runtime's (the editor, a QB program's `SCREEN`) or `gfx`'s,
  from `display.open` to `display.close` or the run's end.
- **`file_read`** gives a file whole, for `surface.load`: a slice has no C library to open one.

| | Gives | Shows by | Clock |
|---|---|---|---|
| DOS | pages in memory; a 3D card, lists played by its driver | a copy to the VBE page not shown, which 4F07h shows at the retrace; else to the one shown, linear or bank by bank; mode 13h's at the retrace. A card's frame by its swap at the retrace, queued behind its draws: its colour buffer's (Voodoo 1, 2), or an overlay's over the mode (Banshee, 3) | the PIT's count |
| Mac, Linux, Windows | a GPU, lists played by SDL_GPU on the interpreter's thread; with no device, pages | the texture `SHOW` names, copied to one SDL's GPU renderer draws; a page as today's frame | `SDL_GetTicksNS` |
| The browser | a GPU, lists copied to the page and played there with WebGL2; else pages | `requestAnimationFrame` | `performance.now` |
| `basic` in a terminal, unit tests | pages | nothing: `surface.read` is the tests' eye | `pclk` |

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
| `SCREEN`'s page and a `gfx` canvas | `SCREEN` is QB's alone: one canvas, nothing layered |
| The program's own shaders | GLSL ES 3.0 as it is |
| Depth in software | 1/z in 16 bits |
| 15 bits | one template with 16 |
| Vertex procedures | the interpreter in software; translated to a vertex shader on a GPU |
| 3D worlds | meshes drawn nearest first with `ORDERED`; a scene-level call is weighed again at step 6 |
| Names that are QB's keywords | names where a module's `OPTION DISABLE` lists them |
| Where the GPU backends live | in the extension, its list played by the host's generic GPU |
| A sprite's arguments | `sprite` and `sprite_turned`, as µGL's pairs |
| Effects | flags on each draw call; `draw`'s settings hold their numbers |
| What is drawn | everything is a surface; an atlas is an image and views over it |
| The package | four modules, `display`, `surface`, `draw`, `scene`, in one `gfx.qbx` |
| Software's frame | drawn in memory the host lends |
| Images | BMP and PNG |
| Software's pixels | a row filler for each variant, picked once a call; a general one for an ink with depth or perspective |
| Stages on a GPU | one shader a backend, the stages bits in the vertices |
