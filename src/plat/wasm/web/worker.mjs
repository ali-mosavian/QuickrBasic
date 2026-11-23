// The module's thread: the IDE on an in-memory file system. The page gives it the module, the shared
// memory, the command line, the files and its GPU's counters, if it has one; it tells the page the irq
// block's address, each file written (to keep), each tone and voice, each audio stream opened, each
// GPU screen, list and read (gpu.mjs), and the exit.
import { GpuLink } from './gpu.mjs';
import { MemFs } from './memfs.mjs';
import { imports, Irq } from './wasm.mjs';

class Exit extends Error {}

onmessage = ({ data: { module, memory, args, files, gpu } }) => {
  const fs = new MemFs((path, bytes) => postMessage({ file: path, bytes }));
  for (const [path, bytes] of files) fs.put(path, bytes);
  const host = {
    fs, cwd: '/', args,
    write: (fd, bytes) => postMessage({ out: new TextDecoder('latin1').decode(bytes) }),
    stdin: () => new Uint8Array(0),
    stdio: () => ({ mode: 0o020666, size: 0, mtimeMs: 0 }),
    isatty: () => true, // the page
    exit: (code) => { throw new Exit(String(code)); },
    tone: (hz) => postMessage({ tone: hz, at: performance.now() }),
    voice: (n, bytes) => postMessage({ voice: n, bytes }),
    audio: (spec) => postMessage({ audio: spec }),
    gpu: gpu ? new GpuLink(memory, gpu, (m, moved) => postMessage(m, moved)) : null,
  };
  const ex = (host.ex = new WebAssembly.Instance(module, imports(memory, host)).exports);
  new Irq(memory, ex.irq()).keyboard = true; // before main: the runtime's console is the screen
  postMessage({ irq: ex.irq(), scans: ex.scans(), frames: ex.frames() });
  let code = 0;
  try {
    ex.__wasm_call_ctors();
    code = ex.__main_void();
  } catch (e) {
    if (!(e instanceof Exit)) {
      postMessage({ error: String(e.stack ?? e) });
      return;
    }
    code = Number(e.message);
  }
  postMessage({ exit: code });
};
