// The synthesizer's voices as an AudioWorklet: plat/synth's C (synth.wasm, given as processorOptions.module), so
// the page sounds as the Mac does. Each message is [voice, a DevVoice's bytes] (wasm.mjs's VOICE_BYTES).
class Synth extends AudioWorkletProcessor {
  constructor({ processorOptions: { module } }) {
    super();
    this.ex = new WebAssembly.Instance(module).exports;
    this.in = this.ex.worklet_init(sampleRate);
    this.port.onmessage = ({ data: [voice, bytes] }) => {
      new Uint8Array(this.ex.memory.buffer, this.in, bytes.length).set(bytes);
      this.ex.worklet_voice(voice);
    };
  }

  // worklet_render's frames are a left sample then a right; an output of one channel is their mean
  process(inputs, [[left, right]]) {
    const at = this.ex.worklet_render(left.length);
    const samples = new Int16Array(this.ex.memory.buffer, at, left.length * 2);
    for (let i = 0; i < left.length; i++) {
      if (right) [left[i], right[i]] = [samples[i * 2] / 32768, samples[i * 2 + 1] / 32768];
      else left[i] = (samples[i * 2] + samples[i * 2 + 1]) / 65536;
    }
    return true;
  }
}
registerProcessor('synth', Synth);
