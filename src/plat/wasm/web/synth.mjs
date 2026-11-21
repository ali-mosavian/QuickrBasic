// The synthesizer's voices as an AudioWorklet: plat/synth's C (synth.wasm, given as processorOptions.module), so
// the page sounds as the Mac does, and the extensions' audio streams mixed in. A message is [voice, a DevVoice's
// bytes] (wasm.mjs's VOICE_BYTES), or { stream }: a stream's spec, played by wasm.mjs's stream.
import { stream } from './wasm.mjs';

const clip = (v) => Math.max(-1, Math.min(1, v));

class Synth extends AudioWorkletProcessor {
  constructor({ processorOptions: { module } }) {
    super();
    this.ex = new WebAssembly.Instance(module).exports;
    this.in = this.ex.worklet_init(sampleRate);
    this.streams = [];
    this.port.onmessage = ({ data }) => {
      if (!Array.isArray(data)) {
        this.streams.push(stream(data.stream));
        return;
      }
      const [voice, bytes] = data;
      new Uint8Array(this.ex.memory.buffer, this.in, bytes.length).set(bytes);
      this.ex.worklet_voice(voice);
    };
  }

  // the frames are a left sample then a right, each stream's added; an output of one channel is their mean. A
  // stream that traps is dropped, not the voices with it.
  process(inputs, [[left, right]]) {
    const at = this.ex.worklet_render(left.length);
    if (this.mixed?.length !== left.length * 2) this.mixed = new Float32Array(left.length * 2);
    const mixed = this.mixed;
    mixed.set(new Int16Array(this.ex.memory.buffer, at, left.length * 2));
    for (let s = this.streams.length - 1; s >= 0; s--) {
      try {
        const samples = this.streams[s](left.length);
        for (let i = 0; i < mixed.length; i++) mixed[i] += samples[i];
      } catch (e) {
        console.error('audio stream dropped:', e);
        this.streams.splice(s, 1);
      }
    }
    for (let i = 0; i < left.length; i++) {
      const l = clip(mixed[i * 2] / 32768), r = clip(mixed[i * 2 + 1] / 32768);
      if (right) [left[i], right[i]] = [l, r];
      else left[i] = (l + r) / 2;
    }
    return true;
  }
}
registerProcessor('synth', Synth);
