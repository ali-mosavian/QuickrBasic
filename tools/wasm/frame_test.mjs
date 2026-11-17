// node tools/wasm/frame_test.mjs MODULE.wasm: the display's frame of the text screen is the VGA's,
// 720x400 (9-dot cells), as DOSBox and the README's clips show it. It was 640x400 shown at 4:3:
// the glyphs tall and narrow beside the clips.
import fs from 'node:fs';
import { imports, memory as newMemory } from '../../src/plat/wasm/web/wasm.mjs';

const memory = newMemory();
const host = { fs, cwd: process.cwd(), args: '' };
const ex = (await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), imports(memory, host))).instance.exports;
ex.__stack_pointer.value = ex.render_stack();
const at = ex.frame(1);
const [w, h] = new Uint32Array(memory.buffer, at, 2);
if (w !== 720 || h !== 400) {
  console.log(`FAIL frame ${w}x${h}, want 720x400`);
  process.exit(1);
}
console.log('frame ok');
