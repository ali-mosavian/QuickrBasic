// node tools/wasm/run.mjs MODULE.wasm [ARGS...]: the wasm build as a command, as build/host/basic
// is. The module runs in a worker; this thread is its interrupts: the tick, Ctrl+C as STOP.
import fs from 'node:fs';
import { Worker, isMainThread, workerData, parentPort } from 'node:worker_threads';
import { imports, memory as newMemory, Irq } from '../../src/plat/wasm/web/wasm.mjs';

const TICK_MS = 1000 / 18.2065;

class Exit extends Error {}

if (isMainThread) {
  const [file, ...args] = process.argv.slice(2);
  const memory = newMemory();
  const worker = new Worker(new URL(import.meta.url), {
    workerData: { file, args: args.join(' '), memory, cwd: process.cwd() },
  });
  let irq, timer;
  worker.on('message', (m) => {
    if ('irq' in m) {
      irq = new Irq(memory, m.irq);
      timer = setInterval(() => irq.raise(0), TICK_MS);
    } else if ('exit' in m) {
      clearInterval(timer);
      process.exitCode = m.exit;
    }
  });
  worker.on('error', (e) => {
    clearInterval(timer);
    console.error(e);
    process.exitCode = 70;
  });
  process.on('SIGINT', () => irq?.raise(4));
  worker.on('exit', () => {
    clearInterval(timer);
    process.removeAllListeners('SIGINT');
  });
} else {
  const { file, args, memory, cwd } = workerData;
  const host = {
    fs, cwd, args,
    write: (fd, bytes) => fs.writeSync(fd, bytes),
    stdin: (n) => {
      const b = Buffer.alloc(n);
      try {
        return b.subarray(0, fs.readSync(0, b, 0, n, null));
      } catch {
        return b.subarray(0, 0);
      }
    },
    stdio: (fd) => fs.fstatSync(fd),
    exit: (code) => {
      throw new Exit(String(code));
    },
  };
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(file), imports(memory, host));
  const ex = instance.exports;
  parentPort.postMessage({ irq: ex.irq() });
  let code = 0;
  try {
    ex.__wasm_call_ctors();
    code = ex.__main_void();
  } catch (e) {
    if (!(e instanceof Exit)) throw e;
    code = Number(e.message);
  }
  parentPort.postMessage({ exit: code });
}
