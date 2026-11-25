# gfx: history

What was built in which order, as it was written at the time: the names are those of then.

## Order

1. The first slice, below.
2. BASIC's binding: the package's loader; the interfaces read in QuickrBasic's dialect (`OPTION
   DIALECT`). Done: `gfx.qbx` in `lib/` beside `basic` and in the Mac's app, `demo/GFXDEMO.BAS`
   its program, `tests/host/gfx.sh` its test.
3. The rest of 2D and the pen: turned sprites, quads, strips, light, tint, blend, recolour; 16 and 32
   bits; PNG; fonts. Done.
4. The hosts' players, SDL_GPU and WebGL2, and the list.
5. DOS: VBE's pages. CHARLIE moves onto `gfx`, cutting its views with
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
- **Tests, native** (`stdlib/gfx/tests`, in the unit tests): each rule by its commands drawn and its
  pixels read. A frame of CHARLIE's sprites by its commands, and against its steps pasted by hand.
  `make gfx-shot` draws that frame in the Mac's window, and `QB_SHOT` takes it.

### The rest of 2D

Step 3, in parts:

- **Shapes and the turned camera**: `circle`, `arc` and `poly`, outlined or filled;
  `camera_turned`. A circle covers the centres within its radius and a half, its outline those
  from the radius less a half, so radius 0 is a pixel. An arc runs clockwise on the screen from
  its start to its finish. An outline draws each pixel once. Under a turned or zoomed camera a
  rect is a polygon.
- **Turned sprites**: `sprite_turned`, `quad` and `strip`, and sprites and tiles under a turned
  camera. `shape` gives a sprite's corners with their texels; software fills the polygon, each
  pixel taking the texel at its centre through the affine map three corners give. A quad is two
  triangles, as a GPU draws it. A strip's points are its top edge, its texels upright below,
  repeating along x.
- **The pen at 8 bits**: `light`, `tint`, `alpha` with `BLEND` and `ADD`, `SOLID`, `recolour`; a
  shape's colour goes through them too. The tint is `&HRRGGBB` at every depth. Row 0 gives the
  tables when first needed: a 32 KB map from 5 bits a channel to the nearest index, the light
  table, and a 64 KB blend table for each mode and alpha, the last four kept. A call's light, tint
  and row mix into 256 indices. With nothing to do the fused fillers draw; else the ink does,
  `FILL_CHUNK` texels at a time: fetched, coloured, keyed, blended, stored. Software ignores
  `SMOOTH`.
- **15, 16 and 32 bits**: the canvas at the depth asked, and the hosts' pages and frames too. A
  surface has a format: `surface.new` the canvas's (8 bits before a screen), a view its parent's.
  `fill16.c` is the 15 and 16-bit template, built twice; `fill32.c` its own. The fused fillers copy
  a texture of the target's format; any other goes through the ink: an index through a palette
  row, lit and tinted (`pal_argb`), a direct texel multiplied, then the blend in arithmetic. On an
  8-bit canvas a direct texture is matched to row 0 through the map as it is drawn. A 15 or 16-bit
  texel 0 is the key, as an index 0 is; a 32-bit one's alpha under a half. What is drawn into a
  32-bit surface is opaque, so a new one is clear until drawn on. `surface.point` gives
  `&HRRGGBB`; `read` and `write` the pixels' bytes.
- **PNG**: `surface.load` reads a PNG of any type and depth, not interlaced. An indexed one with at
  most one transparent colour stays 8 bits, that colour swapped with index 0, the key's; the rest
  load at 32 bits, alpha the file's or tRNS's. `inflate.c` reads the zlib stream as zlib's `puff`
  does, a code a bit at a time: small, and fast enough for a game's images.
- **Fonts**: `draw.new_font` cuts a sheet into w by h glyphs from code 32, row by row;
  `draw.set_font` picks one, 0 the built-in; `draw.free_font` frees one. The built-in is the runtime's 8 by 8 drawn into a sheet of its own on
  first use. Each glyph is drawn as a sprite, so text turns and zooms with the camera; in a colour
  it is `SOLID`, that colour through the style as a shape's.

### The GPU's players

Step 4, in parts:

- **The list**: QxHost's minor 2, `gpu_play` and `gpu_read`. A screen the host gives a GPU draws
  through `backend_gpu`: each call's triangles into the frame's list, played at `display.flip`
  and before a read. A texture is named by its root surface's handle, made and sent when first
  drawn from or into, and sent again when the CPU wrote its pixels since (the root's version). A
  root drawn into is ahead on the GPU until a read brings it back. Shapes draw from a white
  texel. Tests run on a host that records the lists.
- **The Mac's player** (`plat/sdl/player.c`): SDL's GPU renderer draws the window, and the player
  shares its device. It plays any GPU list: it knows buffers, textures and pipelines by the list's
  numbers, and nothing of gfx. A list's writes go into one staging buffer kept between frames, so
  its memory is free when `gpu_play` returns; its command buffer is not waited for, the GPU up to
  two frames behind, and a read waits for its own. `SHOW` copies a texture to one the window
  draws, cycled so the copy never waits on the frame drawing the last. `QB_GFX=soft` draws in
  pages; the offscreen driver has no GPU and does too.
- **Batches**: a shape is `SOLID`, its texel unread, so it joins whatever batch is open, and a
  batch of shapes takes the next sprite's texture. A root drawn from, half a page or smaller, is
  packed into a 1024 by 1024 atlas page of its format (indices or RGBA), rows of places, a place
  freed with its root and a page empty again started over; drawn into, a root has a texture of
  its own. So separate textures draw as one batch once sent. A root keeps the rows written since
  the GPU took it, and only those are sent again. `display.get_stats` gives the frame
  shown last's draws, batches, uploads and reads waited for. On the Mac, 300 sprites and 300
  rects interleaved went from 601 batches to 1 (1764 to 2891 frames a second); 1000 sprites from
  16 textures from 1000 batches to 1 (1371 to 1996).
- **`SMOOTH`**: the shader mixes the four texels round a pixel's centre, each kept in the
  texture's rectangle, so a page's neighbours never bleed in; a keyed texel adds nothing, and a
  pixel mostly keyed is not drawn. `display.features` gives `HAS_SMOOTH` on a GPU.
- **The page's player** (`plat/wasm/player.c`, `web/gpu.mjs`): the module's thread copies each
  list and the memory it points at to the page, going on while the page is at most three behind;
  a read waits for the page. The page plays it with WebGL2 on its own thread: one with an event
  loop, so each refresh draws the latest `SHOW`, the last one included. Sending each frame as a
  bitmap from the module's thread cost 0.5 ms a frame. ESSL has no bindings: a sampler is named by
  its binding (`s0`), and the uniform block is the program's one. `?gfx=soft` draws in pages; so
  does a browser without WebGL2. 640 by 480 at 32 bits, in Chrome on an M-series Mac, the GPU
  draws bench's frames 1.3 (flip) to 8.8 (lines) times software's rate, the same pixels.
- **`VSYNC`**: `gfx_show`, and a list with a `SHOW`, wait for the display to show a frame after:
  the window's thread counts one at each present, the page at each `requestAnimationFrame`
  (`plat/host/host.h`). At most a tenth of a second, as a hidden window shows none.
  `display.refresh` is SDL's display mode's, or the page's median gap over 31 frames. On the Mac
  and in Chrome at 60 Hz, 60 flips a second.
- **Monitors and modes** (`plat/sdl/display.c`): SDL's displays and their full-screen modes,
  asked of the window's thread by `SDL_RunOnMainThread`. A screen full screen is in a window
  again when it closes. The window's resizes, moves and focus wait in a ring for `display.poll`;
  a closed window ends the run unless the program has polled since `display.open`, which then
  gets `QUIT`, and a second close ends it. On the Mac, SDL's full screen in a Space of its own
  needs the app active.
- **Vulkan** (`make linux-app`): the player takes SPIR-V where SDL_GPU has no Metal. On a Linux box
  with no X11 or Wayland, `SDL_HEADLESS=1 tools/deps.sh sdl` builds SDL with the offscreen driver
  only: NVIDIA's Vulkan cannot present to its windows, Mesa's lavapipe can, and there bench's
  frames are software's pixels.
- **The page's display** (`plat/wasm/display.c`): one monitor, its one mode the screen's in
  device pixels; full screen by the Fullscreen API at the next click or key, its rule. The page
  reports the canvas's shown size and its focus.
- **Circles and arcs as shader coverage**: each is one box, its four vertices `GpuDisc`s in a
  buffer of their own, so a plain vertex stays 32 bytes; the fragment shader keeps the pixels
  `disc.h`'s rule covers, centres within the radius by squared distance, every product and sum
  rounded once (`precise`). Software's rows end where the same rule says, so both draw the same
  pixels.

### DOS

Step 5, in parts:

- **The screen** (`plat/dos32/pgfx.c`, `vbe.c`): mode 13h and the card's VBE modes of gfx's
  depths, listed once; `vbe.c`, which the unit tests run, reads a mode's information, picks the
  mode and places the canvas. A linear frame buffer is mapped once (DPMI 0800h) and kept. The text
  screen's memory, size and colours are kept while gfx's is open; a program's graphics `SCREEN` is
  not. The refresh is timed over four retraces as a mode is set.
- **The slice**: an i386 DLL, its own `memcpy` and `memset` and the runtime's `lib/lldiv.c`, as a
  slice imports nothing. Native code lives in the C side's heap, a quarter of DOS's memory where
  that is more than the front end's ask: 16 MB gives a 640×480 canvas at 32 bits.
- **The clock**: the BIOS's ticks and the timer chip's count within one, read back with its status.
- **The renderer** is the one C renderer every platform draws with: `tools/gfx_test.py`'s
  `same_pixels` holds the DOS slice's pixels, the x87's arithmetic, to the host build's.

