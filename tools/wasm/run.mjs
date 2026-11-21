// node tools/wasm/run.mjs MODULE.wasm [ARGS...]: the wasm build as a command, as build/host/basic
// is. The module runs in a worker; this thread is its interrupts: the tick, Ctrl+C as STOP.
import fs from 'node:fs';
import tty from 'node:tty';
import { Worker, isMainThread, workerData, parentPort } from 'node:worker_threads';
import { imports, memory as newMemory, Irq, sidecar, ticker, MUSIC_HZ } from '../../src/plat/wasm/web/wasm.mjs';

class Exit extends Error {}

// QB_SCANS: port 60h's bytes in hex, comma apart, pushed as the module starts, for tools/wasm/scan_test.mjs.
// QB_TONES: each tone and voice (its hertz and gate) and its time (ms) to standard error, for
// tools/wasm/play_test.mjs. QB_AUDIO: an audio device, device.mjs, playing the streams opened.
const log = (line) => fs.writeSync(2, `${line} ${performance.now().toFixed(1)}\n`);
const tone = process.env.QB_TONES ? (hz) => log(`tone ${hz}`) : undefined;
const audio = process.env.QB_AUDIO
  ? (spec) => new Worker(new URL('./device.mjs', import.meta.url), { workerData: spec }).unref()
  : undefined;
const voice = process.env.QB_TONES
  ? (n, b) => log(`voice ${n} ${new DataView(b.buffer).getUint32(0, true)} ${b[14]}`)
  : undefined;

if (isMainThread) {
  const [file, ...args] = process.argv.slice(2);
  const memory = newMemory();
  const module = new WebAssembly.Module(fs.readFileSync(file));
  const worker = new Worker(new URL(import.meta.url), {
    workerData: { module, args: args.join(' '), memory, cwd: process.cwd() },
  });
  let irq, timer;
  worker.on('message', (m) => {
    if ('irq' in m) {
      irq = new Irq(memory, m.irq, m.scans, m.frames);
      for (let n = Number(process.env.QB_FRAMES ?? 0); n > 0; n--) irq.frame();
      for (const code of (process.env.QB_SCANS ?? '').split(',').filter(Boolean)) irq.scan(parseInt(code, 16));
      const tick = sidecar(module, memory, tone, voice);
      timer = setInterval(ticker(irq, tick), 1000 / MUSIC_HZ);
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
  const { module, args, memory, cwd } = workerData;
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
    isatty: (fd) => tty.isatty(fd),
    tone,
    voice,
    audio,
    exit: (code) => {
      throw new Exit(String(code));
    },
  };
  const ex = (host.ex = new WebAssembly.Instance(module, imports(memory, host)).exports);
  parentPort.postMessage({ irq: ex.irq(), scans: ex.scans(), frames: ex.frames() });
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
