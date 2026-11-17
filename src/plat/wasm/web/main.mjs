// The page: the IDE's screen on a canvas, the keyboard and the speaker. The module runs in a worker
// (worker.mjs); this thread is its VGA card and its interrupts: it renders the screen from the
// shared memory in an instance of its own, ORs in the tick, types the keys.
import { biosKey, shiftFlags } from './keys.mjs';
import { imports, memory as newMemory, Irq } from './wasm.mjs';

const TICK_MS = 1000 / 18.2065, BLINK_MS = 267;
const DEMOS = ['WELCOME.BAS', 'GORILLA.BAS'];
const STORE = 'qb:'; // localStorage: STORE + path -> the file in base64

const canvas = document.getElementById('screen');
const status = document.getElementById('status');
const ctx = canvas.getContext('2d');

if (!crossOriginIsolated) {
  status.textContent = 'Loading…';
  throw new Error('not cross-origin isolated yet: coi.js reloads the page');
}

const b64 = (bytes) => btoa(String.fromCharCode(...bytes));
const unb64 = (text) => Uint8Array.from(atob(text), (c) => c.charCodeAt(0));

// The files: the demos, then what the visitor saved or uploaded (theirs win).
async function files() {
  const out = new Map();
  await Promise.all(DEMOS.map(async (name) => {
    const r = await fetch(`demos/${name}`);
    if (r.ok) out.set('/' + name, new Uint8Array(await r.arrayBuffer()));
  }));
  for (let i = 0; i < localStorage.length; i++) {
    const k = localStorage.key(i);
    if (k.startsWith(STORE)) out.set(k.slice(STORE.length), unb64(localStorage.getItem(k)));
  }
  return [...out];
}

function keep(path, bytes) {
  try {
    if (bytes) localStorage.setItem(STORE + path, b64(bytes));
    else localStorage.removeItem(STORE + path);
  } catch (e) {
    status.textContent = `Not kept: ${path} (${e.message})`;
  }
}

// The speaker: a square wave, from the first key or click (a browser wants a gesture first).
let audio, osc, gain;
function wake() {
  if (!audio) {
    audio = new AudioContext();
    osc = audio.createOscillator();
    gain = audio.createGain();
    gain.gain.value = 0;
    osc.type = 'square';
    osc.connect(gain).connect(audio.destination);
    osc.start();
  }
  if (audio.state !== 'running') audio.resume().catch(() => {});
}
function tone(hz) {
  if (!audio) return;
  const t = audio.currentTime;
  gain.gain.setTargetAtTime(hz ? 0.05 : 0, t, 0.002);
  if (hz) osc.frequency.setValueAtTime(hz, t);
}

const open = new URLSearchParams(location.search).get('open') ?? 'WELCOME.BAS';
const module = await WebAssembly.compileStreaming(fetch('ide.wasm'));
const memory = newMemory();
const worker = new Worker(new URL('./worker.mjs', import.meta.url), { type: 'module' });
let irq, display;

worker.onmessage = ({ data }) => {
  if ('irq' in data) {
    irq = new Irq(memory, data.irq);
    // The display: its own instance on the same memory, its own stack; it only reads.
    display = new WebAssembly.Instance(module, imports(memory, { fs: null, cwd: '/', args: '' })).exports;
    display.__stack_pointer.value = display.render_stack();
    window.quickr = { display, irq, memory }; // for a look from the console, and the tests
    setInterval(() => irq.raise(0), TICK_MS);
    requestAnimationFrame(draw);
    status.textContent = '';
  } else if ('file' in data) {
    keep(data.file, data.bytes);
  } else if ('tone' in data) {
    tone(data.tone);
  } else if ('out' in data) {
    console.log(data.out);
  } else if ('exit' in data) {
    tone(0);
    status.textContent = 'QuickrBasic ended. Reload the page to start it again.';
  } else if ('error' in data) {
    status.textContent = 'QuickrBasic stopped: ' + data.error.split('\n')[0];
    console.error(data.error);
  }
};
worker.onerror = (e) => {
  status.textContent = 'QuickrBasic did not start: ' + e.message;
  console.error(e);
};
worker.postMessage({ module, memory, args: open, files: await files() });

let image;
function draw(now) {
  const at = display.frame(Math.floor(now / BLINK_MS) % 2 === 0 ? 1 : 0);
  const head = new Uint32Array(memory.buffer, at, 2);
  const [w, h] = head;
  if (canvas.width !== w || canvas.height !== h || !image) {
    canvas.width = w;
    canvas.height = h;
    image = ctx.createImageData(w, h);
  }
  image.data.set(new Uint8Array(memory.buffer, at + 8, w * h * 4));
  ctx.putImageData(image, 0, 0);
  requestAnimationFrame(draw);
}

addEventListener('keydown', (e) => {
  wake();
  if (!irq || e.metaKey) return;
  irq.shift = shiftFlags(e);
  if (e.ctrlKey && (e.code === 'Pause' || e.code === 'Cancel' || e.code === 'KeyC')) irq.raise(4);
  const key = e.ctrlKey && (e.code === 'Pause' || e.code === 'Cancel') ? 0 : biosKey(e);
  e.preventDefault();
  if (key !== null) irq.type(key);
});
// The IDE sees Alt by polling the shift flags: a tap's release waits a little, so the poll sees it.
const ALT_TAP_MS = 60;
addEventListener('keyup', (e) => {
  if (!irq) return;
  e.preventDefault();
  if (e.key === 'Alt') setTimeout(() => (irq.shift = shiftFlags(e)), ALT_TAP_MS);
  else irq.shift = shiftFlags(e);
});
addEventListener('blur', () => irq && (irq.shift = 0));
addEventListener('pointerdown', wake);

// An upload is kept and opened: the IDE starts again with it.
document.getElementById('upload').addEventListener('change', async (e) => {
  const f = e.target.files[0];
  if (!f) return;
  const name = f.name.toUpperCase().replace(/[^A-Z0-9_.]/g, '').slice(0, 12);
  keep('/' + name, new Uint8Array(await f.arrayBuffer()));
  location.search = '?open=' + encodeURIComponent(name);
});
