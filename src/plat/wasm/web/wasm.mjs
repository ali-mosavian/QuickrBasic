// The module's imports (src/plat/wasm/include/wasm.h, math.h), on a file system with node's fs
// calls (node's own, or memfs.mjs's), and the shared block the page's thread writes (irq).

const ERRNO = { ENOENT: 2, EBADF: 9, EACCES: 13, EPERM: 13, EEXIST: 17, ENOTDIR: 20, EISDIR: 21, EINVAL: 22,
  EMFILE: 24, ENOSPC: 28, ENOTEMPTY: 39 };
const O = { CREAT: 0x40, TRUNC: 0x200, APPEND: 0x400 };
const FIRST_FD = 3;

// run(fn): fn's result, or a thrown error as -errno.
function run(fn) {
  try {
    return fn();
  } catch (e) {
    if (!(e.code in ERRNO)) throw e;
    return -ERRNO[e.code];
  }
}

// A path's parts with . and .. taken out, from / .
function normal(path) {
  const out = [];
  for (const part of path.split('/')) {
    if (part === '..') out.pop();
    else if (part && part !== '.') out.push(part);
  }
  return '/' + out.join('/');
}

// A DevVoice's bytes (qbrt's dev/voice.h), as the synthesizer's AudioWorklet takes them (synth.mjs): a note is one
// and its layers'.
export const VOICE_BYTES = 44;

// A side module's dylink.0: the memory (bytes, alignment) and table entries it needs.
function dylink(mod) {
  const [section] = WebAssembly.Module.customSections(mod, 'dylink.0');
  const b = new Uint8Array(section ?? new ArrayBuffer(0));
  let at = 0;
  const leb = () => {
    let r = 0, s = 0, x;
    do { x = b[at++]; r |= (x & 0x7f) << s; s += 7; } while (x & 0x80);
    return r >>> 0;
  };
  while (at < b.length) {
    const kind = b[at++], len = leb(), end = at + len;
    if (kind === 1) return { mem: leb(), align: 1 << leb(), table: leb() };
    at = end;
  }
  return null;
}

// Each side module loaded: { mod, memoryBase, tableBase, need }, for an audio stream's other thread.
const loaded = [];

// A native extension's side module (plat/wasm/pext.c) on the instance ex's memory, table and stack
// pointer: qx_init_<name>'s table index, 0 if it will not load.
function loadExt(memory, ex, bytes, name) {
  const mod = new WebAssembly.Module(bytes); // synchronous: the module's own thread may
  const need = dylink(mod);
  if (!need) throw new Error('not a side module');
  const memoryBase = need.mem ? ex.pext_alloc(need.mem, need.align) : 0;
  if (need.mem && !memoryBase) throw new Error('no memory');
  const table = ex.__indirect_function_table;
  const tableBase = table.grow(need.table + 1);
  const global = (v) => new WebAssembly.Global({ value: 'i32', mutable: false }, v);
  const ext = new WebAssembly.Instance(mod, {
    env: {
      memory, __indirect_function_table: table, __stack_pointer: ex.__stack_pointer,
      __memory_base: global(memoryBase), __table_base: global(tableBase),
    },
  }).exports;
  ext.__wasm_apply_data_relocs?.();
  ext.__wasm_call_ctors?.();
  const init = ext[`qx_init_${name}`];
  if (!init) throw new Error(`no qx_init_${name}`);
  table.set(tableBase + need.table, init);
  loaded.push({ mod, memoryBase, tableBase, need });
  return tableBase + need.table;
}

// The audio rate a device plays at: the page's AudioContext is made at it (main.mjs).
export const AUDIO_RATE = 48000;

// An audio stream on another thread (plat/wasm/paud.c): the side module of render's table index
// instantiated again on the same memory, with a table of its own at the same base and its own
// stack; its data is the first instance's, so nothing is relocated or constructed again. The
// render is called under the lock word the module's side holds: frames -> the stereo samples at
// out. Its calls into the host would find empty slots, and trap.
export function stream({ module, memory, memoryBase, tableBase, size, render, out, stack, lock }) {
  const global = (v, mutable = false) => new WebAssembly.Global({ value: 'i32', mutable }, v);
  const table = new WebAssembly.Table({ initial: size, element: 'anyfunc' });
  new WebAssembly.Instance(module, {
    env: {
      memory, __indirect_function_table: table, __stack_pointer: global(stack, true),
      __memory_base: global(memoryBase), __table_base: global(tableBase),
    },
  });
  const fn = table.get(render), i32 = new Int32Array(memory.buffer);
  return (frames) => {
    while (Atomics.compareExchange(i32, lock >> 2, 0, 1) !== 0);
    try {
      fn(out, frames);
    } finally {
      Atomics.store(i32, lock >> 2, 0);
    }
    return new Int16Array(memory.buffer, out, frames * 2);
  };
}

// host: { fs, cwd, args, write(fd, bytes), exit(code), stdin(n) -> bytes, stdio(fd) -> a stat,
// isatty(fd), tone(hz), voice(n, bytes), audio(spec): a stream's spec (stream()) to the device's thread, none
// with no device; ex: the instance's exports, set once it is made (a native extension loads on
// them) }; cwd is the module's own (a worker has no chdir), its paths resolved against it.
export function imports(memory, host) {
  const { fs } = host;
  let cwd = normal(host.cwd);
  const u8 = () => new Uint8Array(memory.buffer);
  const dv = () => new DataView(memory.buffer);
  const cstr = (p) => {
    const m = u8();
    let e = p;
    while (m[e]) e++;
    return new TextDecoder().decode(m.slice(p, e));
  };
  const at = (p) => {
    const path = cstr(p);
    return normal(path.startsWith('/') ? path : cwd + '/' + path);
  };
  const files = []; // the module's fd - FIRST_FD: { fd, pos, append }
  const file = (fd) => {
    const f = files[fd - FIRST_FD];
    if (!f) throw Object.assign(new Error('EBADF'), { code: 'EBADF' });
    return f;
  };
  const put = (p, cap, text) => {
    const b = new TextEncoder().encode(text).subarray(0, cap);
    u8().set(b, p);
    return b.length;
  };
  const listings = new Map(); // path -> its entries, from index 0's call

  const qb = {
    fs_open: (p, flags) => run(() => {
      const path = at(p);
      const c = fs.constants;
      const f = [c.O_RDONLY, c.O_WRONLY, c.O_RDWR][flags & 3] | (flags & O.CREAT ? c.O_CREAT : 0)
        | (flags & O.TRUNC ? c.O_TRUNC : 0);
      const entry = { fd: fs.openSync(path, f), pos: 0, append: !!(flags & O.APPEND) };
      let i = files.indexOf(undefined);
      if (i < 0) i = files.length;
      files[i] = entry;
      return i + FIRST_FD;
    }),
    fs_read: (fd, p, n) => run(() => {
      if (fd < FIRST_FD) {
        const b = host.stdin(n);
        u8().set(b, p);
        return b.length;
      }
      const f = file(fd);
      const got = fs.readSync(f.fd, u8(), p, n, f.pos);
      f.pos += got;
      return got;
    }),
    fs_write: (fd, p, n) => run(() => {
      const bytes = u8().slice(p, p + n);
      if (fd < FIRST_FD) {
        host.write(fd, bytes);
        return n;
      }
      const f = file(fd);
      if (f.append) f.pos = fs.fstatSync(f.fd).size;
      const put = fs.writeSync(f.fd, bytes, 0, n, f.pos);
      f.pos += put;
      return put;
    }),
    fs_seek: (fd, off, whence) => run(() => {
      const f = file(fd);
      const base = whence === 0 ? 0 : whence === 1 ? f.pos : fs.fstatSync(f.fd).size;
      if (base + off < 0) return -ERRNO.EINVAL;
      return (f.pos = base + off);
    }),
    fs_close: (fd) => run(() => {
      if (fd < FIRST_FD) return 0;
      fs.closeSync(file(fd).fd);
      files[fd - FIRST_FD] = undefined;
      return 0;
    }),
    fs_truncate: (fd, size) => run(() => (fs.ftruncateSync(file(fd).fd, size), 0)),
    fs_stat: (p, fd, out) => run(() => {
      const st = p ? fs.statSync(at(p)) : fd < FIRST_FD ? host.stdio(fd) : fs.fstatSync(file(fd).fd);
      const d = dv();
      d.setUint32(out, st.mode, true);
      d.setUint32(out + 4, st.size, true);
      d.setFloat64(out + 8, st.mtimeMs, true);
      return 0;
    }),
    fs_isatty: (fd) => (fd < FIRST_FD && host.isatty(fd) ? 1 : 0),
    fs_mkdir: (p) => run(() => (fs.mkdirSync(at(p)), 0)),
    fs_rmdir: (p) => run(() => (fs.rmdirSync(at(p)), 0)),
    fs_unlink: (p) => run(() => (fs.unlinkSync(at(p)), 0)),
    fs_rename: (a, b) => run(() => (fs.renameSync(at(a), at(b)), 0)),
    fs_chmod: (p, mode) => run(() => (fs.chmodSync(at(p), mode), 0)),
    fs_chdir: (p) => run(() => {
      const path = at(p);
      if (!fs.statSync(path).isDirectory()) return -ERRNO.ENOTDIR;
      cwd = path;
      return 0;
    }),
    fs_getcwd: (buf, cap) => put(buf, cap - 1, cwd),
    fs_readdir: (p, index, buf, cap) => run(() => {
      const path = at(p);
      if (index === 0) listings.set(path, ['.', '..', ...fs.readdirSync(path)]);
      const names = listings.get(path) ?? [];
      return index < names.length ? put(buf, cap, names[index]) : -1;
    }),
    now: () => Date.now(),
    tz: (ms) => -new Date(ms).getTimezoneOffset(),
    args: (buf, cap) => put(buf, cap - 1, host.args),
    exit: (code) => host.exit(code),
    tone: (hz) => host.tone?.(hz),
    voice: (n, p, count) => host.voice?.(n, u8().slice(p, p + VOICE_BYTES * count)),
    ext_load: (p, n, name, len) => {
      try {
        return loadExt(memory, host.ex, u8().slice(p, p + n), new TextDecoder().decode(u8().slice(name, name + len)));
      } catch (e) {
        console.error(`native extension: ${e.message}`);
        return 0;
      }
    },
    gpu_open: (w, h) => (host.gpu?.open(w, h) ? 1 : 0),
    gpu_close: () => host.gpu?.close(),
    gpu_play: (list, words) => (host.gpu?.play(list, words) ? 1 : 0),
    gpu_read: (id, x, y, w, h, out) => host.gpu?.read(id, x, y, w, h, out),
    audio_open: (render, out, stack, lock) => {
      const ext = loaded.find((x) => render >= x.tableBase && render < x.tableBase + x.need.table);
      if (!host.audio || !ext) return 0;
      const { mod: module, memoryBase, tableBase, need } = ext;
      host.audio({ module, memory, memoryBase, tableBase, size: tableBase + need.table + 1, render, out, stack, lock });
      return AUDIO_RATE;
    },
  };
  const math = { sin: Math.sin, cos: Math.cos, tan: Math.tan, atan: Math.atan, atan2: Math.atan2, exp: Math.exp,
    exp2: (x) => 2 ** x, log: Math.log, log2: Math.log2, log10: Math.log10, pow: Math.pow, fmod: (x, y) => x % y };
  return { env: { memory }, qb, math };
}

// The music's tick, a second (the Makefile's WASM_MUSIC_HZ); the clock's interrupt, 18.2.
export const MUSIC_HZ = 100;
export const TICK_MS = 1000 / 18.2065;

// Every music tick due since the last call, and the clock's interrupt when one is due: on a timer,
// whose late calls catch up, so the notes last as long as they should.
export function ticker(irq, ex) {
  const start = performance.now();
  let music = 0, clock = 0;
  return () => {
    const ms = performance.now() - start;
    for (const due = Math.floor(ms * MUSIC_HZ / 1000); music < due; music++) ex.host_tick_fire();
    for (const due = Math.floor(ms / TICK_MS); clock < due; clock++) irq.raise(0);
  };
}

// A second instance on the module's memory for another thread, with its own stack: the display's
// frame() and the music's tick (host_tick_fire), as a VGA card and a timer interrupt would. Its host
// gives only tone(hz) and voice(n, bytes), for the notes the tick starts.
export function sidecar(module, memory, tone, voice) {
  const ex = new WebAssembly.Instance(module, imports(memory, { fs: null, cwd: '/', args: '', tone, voice })).exports;
  ex.__stack_pointer.value = ex.render_stack();
  return ex;
}

// The module's memory: shared, so the page's thread can write irq's block while it runs.
const REFRESH_GAPS = 31;

export const memory = () => new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });

// The shared block (wasm.h's WasmIrq) at the address irq() returns, port 60h's bytes (host.h's
// HostScans) at the one scans() returns, and what the page shows (HostDisplay) at the one display() does.
export class Irq {
  constructor(memory, at, scans = 0, display = 0) {
    this.memory = memory;
    this.at = at;
    this.scans = scans;
    this.display = display;
    this.gaps = []; // between the last frames drawn, in ms
    this.last = 0;
  }

  // A frame is on the screen: port 3DAh's vertical retrace, once. Drawn at now (a refresh's time), the
  // refresh is the median gap of each REFRESH_GAPS.
  frame(now) {
    if (!this.display) return;
    const i32 = this.i32, frames = this.display >> 2;
    Atomics.add(i32, frames, 1);
    Atomics.notify(i32, frames);
    if (now === undefined) return;
    if (this.last) this.gaps.push(now - this.last);
    this.last = now;
    if (this.gaps.length < REFRESH_GAPS) return;
    const gap = this.gaps.sort((a, b) => a - b)[REFRESH_GAPS >> 1];
    this.gaps = [];
    if (gap > 0) Atomics.store(i32, frames + 1, Math.round(1e6 / gap));
  }

  // A byte to port 60h: a key's scan code as it goes down, with 80h as it comes up.
  scan(code) {
    if (!this.scans || !code) return;
    const i32 = this.i32, pushed = (this.scans + 32) >> 2;
    const at = Atomics.load(i32, pushed);
    if (((at - Atomics.load(i32, pushed + 1)) >>> 0) >= 32) return;
    this.u8[this.scans + (at >>> 0) % 32] = code;
    Atomics.store(i32, pushed, at + 1);
  }

  get i32() { return new Int32Array(this.memory.buffer); }
  get u8() { return new Uint8Array(this.memory.buffer); }

  // OR the flag's bit in: tick (0) or stop (4); none until the module set the flag's address.
  raise(which) {
    const flag = Atomics.load(this.i32, (this.at + which) >> 2);
    if (flag) Atomics.or(this.u8, flag, this.u8[this.at + 8 + (which >> 2)]);
  }

  set keyboard(on) { this.u8[this.at + 11] = on ? 1 : 0; }

  // The mouse: the text cell and the pixel under it, its buttons (bit 0 the left, 1 the right, 2 the
  // middle). A button going down is a press: its place, then its count.
  mouse(row, col, x, y, buttons) {
    const u8 = this.u8, at = this.at + 20 + 2 * 64;
    const was = u8[at + 2];
    u8[at] = row;
    u8[at + 1] = col;
    u8[at + 2] = buttons;
    const u16 = new Uint16Array(this.memory.buffer), u32 = new Uint32Array(this.memory.buffer);
    u16[(at + 8) >> 1] = x;
    u16[(at + 10) >> 1] = y;
    for (let b = 0; b < 2; b++) {
      if (!(buttons & ~was & (1 << b))) continue;
      u32[((at + 20) >> 2) + b] = x | (y << 16);
      u16[((at + 28) >> 1) + b] = row | (col << 8);
      Atomics.add(this.i32, ((at + 12) >> 2) + b, 1);
    }
  }
  set shift(flags) { this.u8[this.at + 10] = flags; }

  // A key: scan code << 8 | character.
  type(key) {
    const i32 = this.i32, head = (this.at + 12) >> 2, tail = head + 1;
    const t = Atomics.load(i32, tail);
    if (t - Atomics.load(i32, head) >= 64) return false;
    new Uint16Array(this.memory.buffer)[((this.at + 20) >> 1) + (t % 64)] = key;
    Atomics.store(i32, tail, t + 1);
    Atomics.notify(i32, tail);
    return true;
  }
}
