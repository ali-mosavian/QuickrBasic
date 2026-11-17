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

  process(inputs, [[out]]) {
    const at = this.ex.worklet_render(out.length);
    const samples = new Int16Array(this.ex.memory.buffer, at, out.length);
    for (let i = 0; i < out.length; i++) out[i] = samples[i] / 32768;
    return true;
  }
}
registerProcessor('synth', Synth);
