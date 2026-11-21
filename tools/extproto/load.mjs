// The browser's loader, in node: the host (the IDE's stand-in), then the side module into the host's
// own memory and table. worklet.mjs is the AudioWorklet's stand-in.
import { readFileSync } from 'node:fs';
import { Worker } from 'node:worker_threads';

// The side module's dylink.0: the memory and table it needs.
function dylink(mod) {
  const [section] = WebAssembly.Module.customSections(mod, 'dylink.0');
  const b = new Uint8Array(section);
  let at = 0;
  const leb = () => {
    let r = 0, s = 0, x;
    do { x = b[at++]; r |= (x & 0x7f) << s; s += 7; } while (x & 0x80);
    return r >>> 0;
  };
  while (at < b.length) {
    const kind = b[at++], len = leb(), end = at + len;
    if (kind === 1) return { mem: leb(), align: 1 << leb(), table: leb() };
    at = end;
  }
}

const i32 = (v) => new WebAssembly.Global({ value: 'i32', mutable: false }, v);
const memory = new WebAssembly.Memory({ initial: 32, maximum: 256, shared: true });
const bytes = readFileSync(process.argv[3]);
const slice = new WebAssembly.Module(bytes); // synchronous: a worker may
const need = dylink(slice);
let host, memoryBase, tableBase;

const env = {
  memory,
  js_say: (p, n) => console.log('say:', new TextDecoder().decode(new Uint8Array(memory.buffer).slice(p, p + n))),
  js_load: (table) => {
    const ex = host.exports, functions = ex.__indirect_function_table;
    memoryBase = ex.host_alloc(need.mem, need.align);
    tableBase = functions.grow(need.table);
    const ext = new WebAssembly.Instance(slice, {
      env: {
        memory, __indirect_function_table: functions, __stack_pointer: ex.__stack_pointer,
        __memory_base: i32(memoryBase), __table_base: i32(tableBase),
      },
    }).exports;
    ext.__wasm_apply_data_relocs?.();
    ext.__wasm_call_ctors?.();
    return ext.qx_init_ext(table);
  },
};
host = new WebAssembly.Instance(new WebAssembly.Module(readFileSync(process.argv[2])), { env });
const failed = host.exports.run();
console.log('drive:', failed ? `failed at step ${failed}` : 'all as expected');

const ex = host.exports;
const worklet = new Worker(new URL('./worklet.mjs', import.meta.url), {
  workerData: {
    memory, bytes, memoryBase, tableBase, size: ex.__indirect_function_table.length,
    render: ex.render_slot(), out: ex.host_alloc(8, 16), stack: ex.host_alloc(4096, 16) + 4096,
  },
});
worklet.on('message', (m) => { console.log('worklet render:', m.join(' ')); process.exit(0); });
worklet.on('error', (e) => { console.log('worklet:', e); process.exit(1); });
