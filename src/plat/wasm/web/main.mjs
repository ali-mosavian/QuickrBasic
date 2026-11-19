// The page: the IDE's screen on a canvas, the keyboard and the speaker. The module runs in a worker
// (worker.mjs); this thread is its VGA card and its interrupts: it renders the screen from the
// shared memory in an instance of its own, ORs in the tick, types the keys.
import { biosKey, scanCode, shiftFlags } from './keys.mjs';
import { memory as newMemory, Irq, sidecar, ticker, MUSIC_HZ } from './wasm.mjs';

const BLINK_MS = 267;
const DEMOS = ['WELCOME.BAS', 'GORILLA.BAS', 'NIBBLES.BAS', 'MOTHLITE.BAS', 'RUSTLINE.BAS', 'CHARLIE.BAS'];
// F1's help file and a game's pictures, beside the programs, as on DOS
const SUPPORT = ['BASIC.HLP', 'demos/CHARLIE.DAT'];
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
  await Promise.all([...DEMOS.map((n) => `demos/${n}`), ...SUPPORT].map(async (src) => {
    const name = src.split('/').pop();
    try { // a file that will not load is not worth the page
      const r = await fetch(src);
      if (r.ok) out.set('/' + name, new Uint8Array(await r.arrayBuffer()));
    } catch (e) {
      console.error(`no ${name}:`, e);
    }
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

// The speaker: a square wave, from the first key or click (a browser wants a gesture first). The
// synthesizer's voices: an AudioWorklet (synth.mjs); those set before it is ready wait for it.
let audio, osc, gain, synth;
const voices = [];
const synthModule = WebAssembly.compileStreaming(fetch('synth.wasm'));
function wake() {
  if (!audio) {
    audio = new AudioContext();
    osc = audio.createOscillator();
    gain = audio.createGain();
    gain.gain.value = 0;
    osc.type = 'square';
    osc.connect(gain).connect(audio.destination);
    osc.start();
    Promise.all([synthModule, audio.audioWorklet.addModule('synth.mjs')]).then(([module]) => {
      synth = new AudioWorkletNode(audio, 'synth', { outputChannelCount: [2], processorOptions: { module } });
      synth.connect(audio.destination);
      for (const v of voices.splice(0)) synth.port.postMessage(v);
    }).catch((e) => console.error('no synthesizer:', e));
  }
  if (audio.state !== 'running') audio.resume().catch(() => {});
}
function voice(n, bytes) {
  if (!audio) return;
  if (synth) synth.port.postMessage([n, bytes]);
  else voices.push([n, bytes]);
}
function tone(hz) {
  if (!audio) return;
  const t = audio.currentTime;
  gain.gain.setTargetAtTime(hz ? 0.05 : 0, t, 0.002);
  if (hz) osc.frequency.setValueAtTime(hz, t);
}

// ?open=NAME opens a file, ?run=NAME opens and runs it.
const params = new URLSearchParams(location.search);
const run = params.get('run');
const open = run ?? params.get('open') ?? 'WELCOME.BAS';
const F5 = 0x3f00;
let begun = !run; // ?run= waits for a key or click, a browser's leave to sound: its title would be silent
const module = await WebAssembly.compileStreaming(fetch('ide.wasm'));
const memory = newMemory();
const worker = new Worker(new URL('./worker.mjs', import.meta.url), { type: 'module' });
let irq, display;

worker.onmessage = ({ data }) => {
  if ('irq' in data) {
    irq = new Irq(memory, data.irq, data.scans, data.frames);
    // a hidden page draws nothing: its retraces from a timer, so a program waiting for one goes on
    setInterval(() => document.hidden && irq.frame(), 1000 / 60);
    display = sidecar(module, memory, tone, voice);
    window.quickr = { display, irq, memory }; // for a look from the console, and the tests
    setInterval(ticker(irq, display), 1000 / MUSIC_HZ);
    requestAnimationFrame(draw);
    status.textContent = run ? 'Press a key or click to start' : '';
  } else if ('file' in data) {
    keep(data.file, data.bytes);
  } else if ('tone' in data) {
    tone(data.tone);
  } else if ('voice' in data) {
    voice(data.voice, data.bytes);
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

let image, cell = 16;

// The pointer's cell and pixel: the canvas is scaled to fit (object-fit: contain), so the screen's
// own pixels are found first. A text mode's pixel is the cell's times 8 (INT 33h's); a graphics
// mode's cell is 8 wide and as high as its height has it.
let buttons = 0;
function point(e) {
  if (!irq) return;
  const r = canvas.getBoundingClientRect();
  const scale = Math.min(r.width / canvas.width, r.height / canvas.height);
  const clamp = (v, n) => Math.max(0, Math.min(n - 1, Math.floor(v)));
  const x = clamp((e.clientX - r.left - (r.width - canvas.width * scale) / 2) / scale, canvas.width);
  const y = clamp((e.clientY - r.top - (r.height - canvas.height * scale) / 2) / scale, canvas.height);
  const cellW = cell ? canvas.width / 80 : 8;
  const cellH = cell || (canvas.height >= 480 ? 16 : canvas.height >= 350 ? 14 : 8);
  const row = Math.floor(y / cellH), col = Math.floor(x / cellW);
  irq.mouse(row, col, cell ? col * 8 : x, cell ? row * 8 : y, buttons);
}
const BUTTON = [1, 4, 2]; // the browser's left, middle, right: INT 33h's bits
canvas.addEventListener('pointermove', point);
canvas.addEventListener('pointerdown', (e) => {
  buttons |= BUTTON[e.button] ?? 0;
  canvas.setPointerCapture(e.pointerId);
  point(e);
});
canvas.addEventListener('pointerup', (e) => {
  buttons &= ~(BUTTON[e.button] ?? 0);
  point(e);
});
canvas.addEventListener('contextmenu', (e) => e.preventDefault());
function draw(now) {
  const at = display.frame(Math.floor(now / BLINK_MS) % 2 === 0 ? 1 : 0);
  const [w, h, cellH] = new Uint32Array(memory.buffer, at, 3);
  cell = cellH;
  canvas.style.cursor = cell ? '' : 'none'; // a graphics mode's program draws its own (PEN)
  if (canvas.width !== w || canvas.height !== h || !image) {
    canvas.width = w;
    canvas.height = h;
    image = ctx.createImageData(w, h);
  }
  image.data.set(new Uint8Array(memory.buffer, at + 16, w * h * 4));
  ctx.putImageData(image, 0, 0);
  irq.frame();
  requestAnimationFrame(draw);
}

// ?run='s program, started at the first key or click: true as it starts.
function begin() {
  if (begun) return false;
  begun = true;
  status.textContent = '';
  irq.type(F5);
  return true;
}

// The keys down, by scan code: port 60h's bytes as each goes down and comes up.
const held = new Set();
function release(code) {
  if (held.delete(code)) irq.scan(code | 0x80);
}
addEventListener('keydown', (e) => {
  wake();
  if (!irq || e.metaKey) return;
  if (begin()) return e.preventDefault();
  irq.shift = shiftFlags(e);
  held.add(scanCode(e));
  irq.scan(scanCode(e));
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
  release(scanCode(e));
  if (e.key === 'Alt') setTimeout(() => (irq.shift = shiftFlags(e)), ALT_TAP_MS);
  else irq.shift = shiftFlags(e);
});
addEventListener('blur', () => {
  if (!irq) return;
  irq.shift = 0;
  for (const code of [...held]) release(code); // no key comes up for a page that lost the keyboard
});
addEventListener('pointerdown', () => {
  wake();
  if (irq) begin();
});

// An upload is kept and opened: the IDE starts again with it.
document.getElementById('upload').addEventListener('change', async (e) => {
  const f = e.target.files[0];
  if (!f) return;
  const name = f.name.toUpperCase().replace(/[^A-Z0-9_.]/g, '').slice(0, 12);
  keep('/' + name, new Uint8Array(await f.arrayBuffer()));
  location.search = '?open=' + encodeURIComponent(name);
});
