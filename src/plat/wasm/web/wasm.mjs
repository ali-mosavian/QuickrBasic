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

// host: { fs, cwd, args, write(fd, bytes), exit(code), stdin(n) -> bytes, stdio(fd) -> a stat,
// tone(hz) }; cwd is
// the module's own (a worker has no chdir), its paths resolved against it.
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
// gives only tone(hz), for the notes the tick starts.
export function sidecar(module, memory, tone) {
  const ex = new WebAssembly.Instance(module, imports(memory, { fs: null, cwd: '/', args: '', tone })).exports;
  ex.__stack_pointer.value = ex.render_stack();
  return ex;
}

// The module's memory: shared, so the page's thread can write irq's block while it runs.
export const memory = () => new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });

// The shared block (wasm.h's WasmIrq) at the address irq() returns.
export class Irq {
  constructor(memory, at) {
    this.memory = memory;
    this.at = at;
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
