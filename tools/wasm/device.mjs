// node's stand-in for the page's AudioWorklet (web/synth.mjs): a thread that plays a stream
// (wasm.mjs's stream) a quantum of 128 frames at a time, as fast as the rate wants them.
import { workerData } from 'node:worker_threads';
import { stream, AUDIO_RATE } from '../../src/plat/wasm/web/wasm.mjs';

const QUANTUM = 128;
const render = stream(workerData);
const start = performance.now();
let played = 0;
setInterval(() => {
  for (const due = ((performance.now() - start) * AUDIO_RATE) / 1000; played + QUANTUM <= due; played += QUANTUM)
    render(QUANTUM);
}, 2);
