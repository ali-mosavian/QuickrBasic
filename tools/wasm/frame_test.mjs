// node tools/wasm/frame_test.mjs MODULE.wasm: the display's frame of the text screen is the VGA's,
// 720x400 (9-dot cells), as DOSBox and the README's clips show it. It was 640x400 shown at 4:3:
// the glyphs tall and narrow beside the clips.
import fs from 'node:fs';
import { Irq, imports, memory as newMemory } from '../../src/plat/wasm/web/wasm.mjs';

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

// The mouse pointer: the cell under it inverted while shown, nothing else. There was none: the
// page's mouse did nothing and showed nothing.
const irq = new Irq(memory, ex.irq());
const pixels = () => new Uint32Array(memory.buffer, ex.frame(1) + 16, w * h).slice();
const cellAt = (p, row, col) => p[(row * 16 + 8) * w + col * 9 + 4];
irq.mouse(5, 10, 0);
const hidden = pixels();
new Int32Array(memory.buffer)[(ex.irq() + 152) >> 2] = 1; // pmou_show
const shown = pixels();
const changed = shown.reduce((n, v, i) => n + (v !== hidden[i]), 0);
if (cellAt(shown, 5, 10) === cellAt(hidden, 5, 10) || changed > 9 * 16) {
  console.log(`FAIL pointer: ${changed} pixels changed, the cell's ${cellAt(hidden, 5, 10)} -> ${cellAt(shown, 5, 10)}`);
  process.exit(1);
}
console.log('frame ok');
