// The page's GPU: the lists (src/qx/qxlist.h) src/plat/wasm/player.c plays, played with WebGL2 on the
// page's thread.
//
// # How it works
// The module's thread (GpuLink) copies each list and the memory it points at into one buffer, its
// pointers made offsets there, and posts it; it goes on while the GPU has at most FLIGHT unfinished, as
// a swapchain holds a native program without vsync, and a read waits for the page. The page (Gpu) plays
// them on an OffscreenCanvas's context. Every texture is R8, RGBA8 or DEPTH16, zeros as WebGL makes it,
// an RGBA one with a framebuffer to draw in, a depth one attached to it by the first pass that names
// both. A pipeline is a program, its blend, its depth and its vertices' layout; a draw's alpha the
// blend colour. GL's row 0 is a texture's first, so clip space's y runs down its rows (y_down), and
// uploads, clips and reads take rows as they are. SHOW copies the texture to the one shown; at each
// refresh the page draws that, flipped to put row 0 on top (present).

const OP = { BUFFER: 1, WRITE: 2, TEXTURE: 3, FREE: 4, UPLOAD: 5, PIPELINE: 6, PASS: 7, CLIP: 8, UNIFORMS: 9,
  DRAW: 10, SHOW: 11 };
const SIZE = 8; // QX_LIST_SIZE
const INDEX = 2; // QX_BUFFER_INDEX
const RGBA = 2, DEPTH = 3; // QX_TEXTURE_*
const MIX = 1, ADD = 2, KEEP = 3; // QX_BLEND_*
const TEST = 1, WRITE = 2; // QX_DEPTH_*
const DRAW_TEXTURES = 6; // a DRAW's first texture, its argument
const PIPELINE_HEAD = 10; // a PIPELINE's first attribute, its argument
const FLIGHT = 3;
// The shared counters: the lists played, the reads answered.
const PLAYED = 0, READ = 1;

// The pointers of op's arguments at a in w: each one's word and its bytes.
function pointers(op, w, a) {
  switch (op) {
    case OP.WRITE: return [[a + 2, w[a + 1]]];
    case OP.UPLOAD: return [[a + 6, w[a + 5] * w[a + 4]]];
    case OP.PIPELINE: return [[a + 5, w[a + 4]], [a + 8, w[a + 7]]];
    case OP.UNIFORMS: return [[a + 1, w[a]]];
    default: return [];
  }
}

// Wait while the counter at i is behind by more than most.
function behind(sync, i, want, most) {
  for (let v; ((want - (v = Atomics.load(sync, i))) | 0) > most;) Atomics.wait(sync, i, v, 100);
}

// The module's thread's side: sync the page's counters (an Int32Array of 2 on shared memory).
export class GpuLink {
  constructor(memory, sync, post) {
    this.memory = memory;
    this.sync = sync;
    this.post = post;
    this.sent = Atomics.load(sync, PLAYED);
    this.asked = Atomics.load(sync, READ);
  }

  open(w, h) {
    this.post({ gpu: { w, h } });
    return true;
  }

  close() { this.post({ gpu: null }); }

  // The list posted: true when it shows a texture.
  play(list, words) {
    const w = new Uint32Array(this.memory.buffer, list, words);
    const blocks = [];
    let bytes = words * 4, showed = false;
    for (let at = 0, size; at < words && (size = w[at] >>> SIZE); at += size) {
      showed ||= (w[at] & 0xff) === OP.SHOW;
      for (const [word, n] of pointers(w[at] & 0xff, w, at + 1)) {
        blocks.push([word, n, bytes]);
        bytes += (n + 3) & ~3;
      }
    }
    const buf = new ArrayBuffer(bytes), out = new Uint32Array(buf, 0, words), out8 = new Uint8Array(buf);
    const u8 = new Uint8Array(this.memory.buffer);
    out.set(w);
    for (const [word, n, at] of blocks) {
      out8.set(u8.subarray(w[word], w[word] + n), at);
      out[word] = at;
      out[word + 1] = 0;
    }
    this.post({ list: buf }, [buf]);
    behind(this.sync, PLAYED, ++this.sent, FLIGHT);
    return showed;
  }

  read(id, x, y, w, h, out) {
    this.post({ read: [id, x, y, w, h, out] });
    behind(this.sync, READ, ++this.asked, 0);
  }
}

// The page's side: null, no WebGL2.
export function gpu(memory, sync) {
  const canvas = new OffscreenCanvas(1, 1);
  const gl = canvas.getContext('webgl2', { alpha: false, antialias: false, depth: false, stencil: false,
    powerPreference: 'high-performance' });
  return gl && new Gpu(gl, memory, sync);
}

export class Gpu {
  constructor(gl, memory, sync) {
    this.gl = gl;
    this.memory = memory;
    this.sync = sync;
    this.textures = new Map();
    this.buffers = new Map();
    this.pipelines = new Map();
    this.uniforms = gl.createBuffer();
    this.shown = null; // the texture shown
    this.fresh = false; // shown since present
    this.fences = []; // after each list the GPU has not finished
    gl.bindVertexArray(gl.createVertexArray());
  }

  get canvas() { return this.gl.canvas; }

  open(w, h) {
    this.close();
    this.canvas.width = this.w = w;
    this.canvas.height = this.h = h;
    this.state();
    this.shown = this.made(w, h, RGBA, 1);
    this.showing = false;
  }

  close() {
    const gl = this.gl;
    for (const t of this.textures.values()) this.drop(t);
    for (const b of this.buffers.values()) gl.deleteBuffer(b.b);
    for (const p of this.pipelines.values()) gl.deleteProgram(p.p);
    if (this.shown) this.drop(this.shown);
    this.textures.clear();
    this.buffers.clear();
    this.pipelines.clear();
    this.shown = null;
    this.showing = false;
  }

  // What the draws last bound, none.
  state() {
    this.target = null; // the pass's texture
    this.bound = undefined; // the framebuffer drawn in
    this.program = null;
    this.layout = null; // the pipeline and buffer the attributes point at
    this.indices = null;
    this.enabled = new Set(); // the attributes' locations
    this.gl.enable(this.gl.SCISSOR_TEST);
  }

  // The shown texture drawn on the canvas, row 0 on top: false, none since the last.
  present(again) {
    const gl = this.gl;
    if (!this.showing || !(this.fresh || again)) return false;
    gl.bindFramebuffer(gl.READ_FRAMEBUFFER, this.shown.fb);
    gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, null);
    gl.disable(gl.SCISSOR_TEST);
    gl.blitFramebuffer(0, 0, this.w, this.h, 0, this.h, this.w, 0, gl.COLOR_BUFFER_BIT, gl.NEAREST);
    gl.enable(gl.SCISSOR_TEST);
    this.bound = undefined;
    this.fresh = false;
    return true;
  }

  bytes(at, n) { return new Uint8Array(this.list, at, n); }

  // A list (GpuLink's), played.
  play(list) {
    this.list = list;
    const w = new Uint32Array(list, 0, list.byteLength >> 2);
    for (let at = 0, size; at < w.length && (size = w[at] >>> SIZE); at += size) {
      const a = w.subarray(at + 1, at + size);
      switch (w[at] & 0xff) {
        case OP.BUFFER: this.buffer(a[0], a[1], a[2]); break;
        case OP.WRITE: this.write(a[0], a[1], a[2]); break;
        case OP.TEXTURE: this.texture(a[0], a[1], a[2], a[3], a.length > 4 ? a[4] : 1); break;
        case OP.FREE: this.free(a[0]); break;
        case OP.UPLOAD: this.upload(a); break;
        case OP.PIPELINE: this.pipeline(a); break;
        case OP.PASS: this.pass(a[0], a.length > 1 ? a[1] : 0); break;
        case OP.CLIP: this.gl.scissor(a[0], a[1], a[2], a[3]); break;
        case OP.UNIFORMS: this.uniform(a[0], a[1]); break;
        case OP.DRAW: this.draw(a); break;
        case OP.SHOW: this.show(a[0]); break;
      }
    }
    this.list = null;
    this.fences.push(this.gl.fenceSync(this.gl.SYNC_GPU_COMMANDS_COMPLETE, 0));
    this.gl.flush();
    if (this.fences.length === 1) setTimeout(() => this.finished());
  }

  // The lists the GPU has finished, counted played; looked at again while one is not. A fence
  // signals only in a later task.
  finished() {
    const gl = this.gl;
    let n = 0;
    for (; n < this.fences.length && gl.clientWaitSync(this.fences[n], 0, 0) !== gl.TIMEOUT_EXPIRED; n++)
      gl.deleteSync(this.fences[n]);
    this.fences.splice(0, n);
    if (n) {
      Atomics.add(this.sync, PLAYED, n);
      Atomics.notify(this.sync, PLAYED);
    }
    if (this.fences.length) setTimeout(() => this.finished());
  }

  // Buffer id at least bytes of kind, made again twice that when smaller.
  buffer(id, bytes, kind) {
    const gl = this.gl;
    let b = this.buffers.get(id);
    if (!b) {
      b = { b: gl.createBuffer(), cap: 0, target: kind === INDEX ? gl.ELEMENT_ARRAY_BUFFER : gl.ARRAY_BUFFER };
      this.buffers.set(id, b);
    }
    if (bytes <= b.cap) return;
    b.cap = bytes * 2;
    this.bind(b);
    gl.bufferData(b.target, b.cap, gl.DYNAMIC_DRAW);
  }

  // b bound to its target: an index buffer's binding is the vertex array's.
  bind(b) {
    this.gl.bindBuffer(b.target, b.b);
    if (b.target === this.gl.ELEMENT_ARRAY_BUFFER) this.indices = b;
  }

  write(id, bytes, at) {
    const b = this.buffers.get(id);
    if (!b || !bytes || bytes > b.cap) return;
    this.bind(b);
    this.gl.bufferSubData(b.target, 0, this.bytes(at, bytes));
  }

  // A texture w by h of format (QX_TEXTURE_*) and its levels, RGBA with a framebuffer. A filter
  // of levels, though texelFetch reads without one: so every level is the texture's.
  made(w, h, format, levels) {
    const gl = this.gl, rgba = format === RGBA;
    const t = { t: gl.createTexture(), fb: null, w, h, rgba, depth: null };
    gl.bindTexture(gl.TEXTURE_2D, t.t);
    gl.texStorage2D(gl.TEXTURE_2D, levels, rgba ? gl.RGBA8 : format === DEPTH ? gl.DEPTH_COMPONENT16 : gl.R8, w,
      h);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, levels > 1 ? gl.NEAREST_MIPMAP_NEAREST : gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    if (rgba) {
      t.fb = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, t.t, 0);
      this.bound = undefined;
    }
    return t;
  }

  texture(id, w, h, format, levels) {
    this.free(id);
    this.textures.set(id, this.made(w, h, format, levels));
  }

  drop(t) {
    this.gl.deleteTexture(t.t);
    this.gl.deleteFramebuffer(t.fb);
    if (this.target === t) this.target = null;
  }

  free(id) {
    const t = this.textures.get(id);
    if (!t) return;
    this.drop(t);
    this.textures.delete(id);
  }

  // UPLOAD id, x, y, w, h, pitch, pointer, level.
  upload(a) {
    const gl = this.gl, t = this.textures.get(a[0]);
    if (!t) return;
    gl.bindTexture(gl.TEXTURE_2D, t.t);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.pixelStorei(gl.UNPACK_ROW_LENGTH, a[5] / (t.rgba ? 4 : 1));
    gl.texSubImage2D(gl.TEXTURE_2D, a.length > 8 ? a[8] : 0, a[1], a[2], a[3], a[4], t.rgba ? gl.RGBA : gl.RED,
      gl.UNSIGNED_BYTE, this.bytes(a[6], a[5] * a[4]));
  }

  // The stage's ESSL compiled: null, it would not.
  shader(type, n, at) {
    const gl = this.gl, s = gl.createShader(type);
    gl.shaderSource(s, new TextDecoder().decode(this.bytes(at, n)));
    gl.compileShader(s);
    if (gl.getShaderParameter(s, gl.COMPILE_STATUS)) return s;
    console.error('gpu: shader:', gl.getShaderInfoLog(s));
    gl.deleteShader(s);
    return null;
  }

  // PIPELINE id, blend and depth, stride, samplers, vertex code, fragment code, attributes.
  pipeline(a) {
    const gl = this.gl;
    const vs = this.shader(gl.VERTEX_SHADER, a[4], a[5]), fs = this.shader(gl.FRAGMENT_SHADER, a[7], a[8]);
    if (!vs || !fs) return;
    const p = gl.createProgram();
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.linkProgram(p);
    gl.deleteShader(vs);
    gl.deleteShader(fs);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
      console.error('gpu: pipeline:', gl.getProgramInfoLog(p));
      gl.deleteProgram(p);
      return;
    }
    for (let i = 0; i < gl.getProgramParameter(p, gl.ACTIVE_UNIFORM_BLOCKS); i++) gl.uniformBlockBinding(p, i, 0);
    gl.useProgram(p);
    this.program = null;
    for (let s = 0; s < a[3]; s++) gl.uniform1i(gl.getUniformLocation(p, `s${s}`), s);
    const attributes = [];
    for (let i = PIPELINE_HEAD; i + 3 <= a.length; i += 3) {
      attributes.push({ at: a[i], format: a[i + 1], offset: a[i + 2] });
    }
    if (this.pipelines.has(a[0])) gl.deleteProgram(this.pipelines.get(a[0]).p);
    this.pipelines.set(a[0], { p, blend: a[1] & 0xff, depth: a[1] >> 8 & 0xff, stride: a[2], samplers: a[3],
      attributes });
  }

  // The pass into texture id, with depth texture deep (0 none) attached to its framebuffer.
  pass(id, deep) {
    const gl = this.gl, t = this.textures.get(id), d = deep && this.textures.get(deep);
    this.target = t?.fb ? t : null;
    if (!this.target) return;
    gl.scissor(0, 0, t.w, t.h);
    if (!d || t.depth === d) return;
    gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, d.t, 0);
    t.depth = d;
    this.bound = undefined;
  }

  // The pipeline's blend and depth.
  blend(pl) {
    const gl = this.gl;
    if (pl.blend === MIX || pl.blend === ADD) {
      gl.enable(gl.BLEND);
      gl.blendEquation(gl.FUNC_ADD);
      const under = pl.blend === MIX ? gl.ONE_MINUS_CONSTANT_COLOR : gl.ONE;
      gl.blendFuncSeparate(gl.CONSTANT_COLOR, under, gl.ONE, gl.ZERO);
    } else {
      gl.disable(gl.BLEND);
    }
    const colour = pl.blend !== KEEP;
    gl.colorMask(colour, colour, colour, colour);
    if (!pl.depth) {
      gl.disable(gl.DEPTH_TEST);
      return;
    }
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(pl.depth & TEST ? gl.GREATER : gl.ALWAYS);
    gl.depthMask((pl.depth & WRITE) !== 0);
  }

  uniform(bytes, at) {
    const gl = this.gl;
    gl.bindBuffer(gl.UNIFORM_BUFFER, this.uniforms);
    gl.bufferData(gl.UNIFORM_BUFFER, this.bytes(at, bytes), gl.DYNAMIC_DRAW);
    gl.bindBufferBase(gl.UNIFORM_BUFFER, 0, this.uniforms);
  }

  // The pipeline's attributes pointed at buffer b, those of others off.
  point(pl, b) {
    const gl = this.gl;
    // QX_ATTRIBUTE_*: its count, its type, a float
    const formats = [null, [2, gl.FLOAT, true], [4, gl.FLOAT, true], [4, gl.UNSIGNED_SHORT, false],
      [4, gl.UNSIGNED_BYTE, false]];
    const want = new Set(pl.attributes.map((x) => x.at));
    for (const at of this.enabled) if (!want.has(at)) gl.disableVertexAttribArray(at);
    gl.bindBuffer(gl.ARRAY_BUFFER, b.b);
    for (const { at, format, offset } of pl.attributes) {
      const [n, type, float] = formats[format];
      gl.enableVertexAttribArray(at);
      if (float) gl.vertexAttribPointer(at, n, type, false, pl.stride, offset);
      else gl.vertexAttribIPointer(at, n, type, pl.stride, offset);
    }
    this.enabled = want;
  }

  // DRAW pipeline, vertices, indices, first, count, alpha, textures.
  draw(a) {
    const gl = this.gl, t = this.target;
    const pl = this.pipelines.get(a[0]), vb = this.buffers.get(a[1]), ib = this.buffers.get(a[2]);
    if (!pl || !vb || !ib || !t || a.length !== DRAW_TEXTURES + pl.samplers || (a[3] + a[4]) * 4 > ib.cap) return;
    const sources = [...a.subarray(DRAW_TEXTURES)].map((id) => this.textures.get(id));
    if (sources.some((s) => !s)) return;
    sources.forEach((s, i) => {
      gl.activeTexture(gl.TEXTURE0 + i);
      gl.bindTexture(gl.TEXTURE_2D, s.t);
    });
    if (this.bound !== t.fb) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
      gl.viewport(0, 0, t.w, t.h);
      this.bound = t.fb;
    }
    if (this.program !== pl) {
      gl.useProgram(pl.p);
      this.blend(pl);
      this.program = pl;
    }
    if (this.layout?.pl !== pl || this.layout?.vb !== vb) {
      this.point(pl, vb);
      this.layout = { pl, vb };
    }
    if (this.indices !== ib) this.bind(ib);
    const alpha = a[5] / 255;
    gl.blendColor(alpha, alpha, alpha, alpha);
    gl.drawElements(gl.TRIANGLES, a[4], gl.UNSIGNED_INT, a[3] * 4);
  }

  // Texture id copied to the one shown, at its top left.
  show(id) {
    const gl = this.gl, t = this.textures.get(id);
    if (!t?.fb) return;
    const w = Math.min(t.w, this.w), h = Math.min(t.h, this.h);
    gl.bindFramebuffer(gl.READ_FRAMEBUFFER, t.fb);
    gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, this.shown.fb);
    gl.disable(gl.SCISSOR_TEST);
    gl.blitFramebuffer(0, 0, w, h, 0, 0, w, h, gl.COLOR_BUFFER_BIT, gl.NEAREST);
    gl.enable(gl.SCISSOR_TEST);
    this.bound = undefined;
    this.showing = this.fresh = true;
  }

  // w by h pixels at x, y of texture id into the module's memory at out, RGBA.
  read(id, x, y, w, h, out) {
    const gl = this.gl, t = this.textures.get(id), to = new Uint8Array(this.memory.buffer, out, w * h * 4);
    if (t?.fb) {
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER, t.fb);
      gl.readPixels(x, y, w, h, gl.RGBA, gl.UNSIGNED_BYTE, to);
    } else {
      to.fill(0);
    }
    Atomics.add(this.sync, READ, 1);
    Atomics.notify(this.sync, READ);
  }
}
