// A file system in memory with the node fs calls wasm.mjs makes. Each change is told to changed(path,
// bytes or null), which keeps it (the page's storage).

const err = (code) => Object.assign(new Error(code), { code });
const DIR = 0o040755, FILE = 0o100644, WRITABLE = 0o200;

export class MemFs {
  constructor(changed = () => {}) {
    this.nodes = new Map([['/', { mode: DIR, mtimeMs: Date.now() }]]);
    this.fds = [];
    this.changed = changed;
    this.constants = { O_RDONLY: 0, O_WRONLY: 1, O_RDWR: 2, O_CREAT: 0x40, O_TRUNC: 0x200 };
  }

  // A path's node and its directory's, by DOS's rules: names in any case are one.
  key(path) { return path.toUpperCase(); }
  parent(path) { return path.slice(0, path.lastIndexOf('/')) || '/'; }

  node(path) {
    const n = this.nodes.get(this.key(path));
    if (!n) throw err('ENOENT');
    return n;
  }

  dir(path) {
    const d = this.nodes.get(this.key(this.parent(path)));
    if (!d) throw err('ENOENT');
    if (d.mode !== DIR) throw err('ENOTDIR');
  }

  stat(n) {
    return { mode: n.mode, size: n.data?.length ?? 0, mtimeMs: n.mtimeMs, isDirectory: () => n.mode === DIR };
  }

  // A file put in from outside (a demo, an upload): not told to changed.
  put(path, bytes) {
    const parts = path.split('/').filter(Boolean);
    for (let i = 1; i < parts.length; i++) {
      const d = '/' + parts.slice(0, i).join('/');
      if (!this.nodes.has(this.key(d))) this.nodes.set(this.key(d), { mode: DIR, mtimeMs: Date.now(), name: d });
    }
    this.nodes.set(this.key(path), { mode: FILE, data: bytes, mtimeMs: Date.now(), name: path });
  }

  openSync(path, flags) {
    const c = this.constants;
    let n = this.nodes.get(this.key(path));
    if (!n) {
      if (!(flags & c.O_CREAT)) throw err('ENOENT');
      this.dir(path);
      n = { mode: FILE, data: new Uint8Array(0), mtimeMs: Date.now(), name: path };
      this.nodes.set(this.key(path), n);
    } else if (n.mode === DIR) {
      throw err('EISDIR');
    } else if ((flags & 3) !== c.O_RDONLY && !(n.mode & WRITABLE)) {
      throw err('EACCES');
    }
    if (flags & c.O_TRUNC) n.data = new Uint8Array(0);
    const fd = { node: n, path, dirty: !!(flags & (c.O_CREAT | c.O_TRUNC)) };
    let i = this.fds.indexOf(undefined);
    if (i < 0) i = this.fds.length;
    this.fds[i] = fd;
    return i;
  }

  fd(i) {
    const f = this.fds[i];
    if (!f) throw err('EBADF');
    return f;
  }

  readSync(i, buf, off, len, pos) {
    const { data } = this.fd(i).node;
    const n = Math.max(0, Math.min(len, data.length - pos));
    buf.set(data.subarray(pos, pos + n), off);
    return n;
  }

  writeSync(i, bytes, off, len, pos) {
    const f = this.fd(i), n = f.node;
    if (pos + len > n.data.length) {
      const grown = new Uint8Array(Math.max(pos + len, n.data.length * 2));
      grown.set(n.data);
      n.data = grown.subarray(0, pos + len);
    }
    n.data.set(bytes.subarray(off, off + len), pos);
    n.mtimeMs = Date.now();
    f.dirty = true;
    return len;
  }

  ftruncateSync(i, size) {
    const f = this.fd(i), n = f.node;
    const d = new Uint8Array(size);
    d.set(n.data.subarray(0, size));
    n.data = d;
    f.dirty = true;
  }

  closeSync(i) {
    const f = this.fd(i);
    this.fds[i] = undefined;
    if (f.dirty) this.changed(f.node.name, f.node.data.slice());
  }

  fstatSync(i) { return this.stat(this.fd(i).node); }
  statSync(path) { return this.stat(this.node(path)); }

  mkdirSync(path) {
    if (this.nodes.has(this.key(path))) throw err('EEXIST');
    this.dir(path);
    this.nodes.set(this.key(path), { mode: DIR, mtimeMs: Date.now(), name: path });
  }

  children(path) {
    const k = this.key(path === '/' ? '' : path) + '/';
    return [...this.nodes.entries()].filter(([p]) => p.startsWith(k) && p !== k && !p.slice(k.length).includes('/'));
  }

  rmdirSync(path) {
    if (this.node(path).mode !== DIR) throw err('ENOTDIR');
    if (this.children(path).length) throw err('ENOTEMPTY');
    this.nodes.delete(this.key(path));
  }

  unlinkSync(path) {
    const n = this.node(path);
    if (n.mode === DIR) throw err('EISDIR');
    this.nodes.delete(this.key(path));
    this.changed(n.name, null);
  }

  renameSync(from, to) {
    const n = this.node(from);
    this.dir(to);
    if (n.mode === DIR && this.children(from).length) throw err('EACCES');
    this.nodes.delete(this.key(from));
    n.name = to;
    this.nodes.set(this.key(to), n);
    if (n.data) {
      this.changed(from, null);
      this.changed(to, n.data.slice());
    }
  }

  chmodSync(path, mode) {
    const n = this.node(path);
    n.mode = (n.mode & ~0o777) | (mode & 0o777);
  }

  readdirSync(path) {
    if (this.node(path).mode !== DIR) throw err('ENOTDIR');
    return this.children(path).map(([, n]) => n.name.slice(n.name.lastIndexOf('/') + 1));
  }
}
