// node tools/wasm/gpu_test.mjs: a list counts as played once the GPU has finished it. It counted once
// its GL calls were made: a program without vsync ran ~1600 frames a second ahead of the GPU, and the
// page's refreshes, behind them, fell to ~15 a second, its motion stuttering.
import assert from 'node:assert/strict';
import { Gpu } from '../../src/plat/wasm/web/gpu.mjs';

const TIMEOUT_EXPIRED = 0x911b, CONDITION_SATISFIED = 0x911c;
const fences = [];
// WebGL2 as the page's Gpu calls it: every call nothing, but fences, which signal when the test says.
const gl = new Proxy({
  TIMEOUT_EXPIRED,
  fenceSync: () => { const f = { done: false }; fences.push(f); return f; },
  clientWaitSync: (f) => (f.done ? CONDITION_SATISFIED : TIMEOUT_EXPIRED),
}, { get: (o, k) => (k in o ? o[k] : () => ({})) });
const sync = new Int32Array(new SharedArrayBuffer(8));
const gpu = new Gpu(gl, new WebAssembly.Memory({ initial: 1 }), sync);
const CLIP = 8, SIZE = 8;
const tick = () => new Promise((r) => setTimeout(r, 5));

gpu.play(new Uint32Array([CLIP | 5 << SIZE, 0, 0, 1, 1]).buffer);
await tick();
assert.equal(Atomics.load(sync, 0), 0, 'played before the GPU finished it');
for (const f of fences) f.done = true;
await tick();
assert.equal(Atomics.load(sync, 0), 1, 'not played once the GPU finished it');
console.log('gpu ok');
