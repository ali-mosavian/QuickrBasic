// The AudioWorklet's stand-in: another thread instantiates the same slice against the same memory,
// with its own table at the same base and its own stack. Data relocations and constructors are not
// run again: the data is already in the shared memory.
import { workerData, parentPort } from 'node:worker_threads';

const { memory, bytes, memoryBase, tableBase, size, render, out, stack } = workerData;
const i32 = (v, mutable = false) => new WebAssembly.Global({ value: 'i32', mutable }, v);
const table = new WebAssembly.Table({ initial: size, element: 'anyfunc' });
new WebAssembly.Instance(new WebAssembly.Module(bytes), {
  env: {
    memory, __indirect_function_table: table, __stack_pointer: i32(stack, true),
    __memory_base: i32(memoryBase), __table_base: i32(tableBase),
  },
});
table.get(render)(out, 4);
parentPort.postMessage(Array.from(new Int16Array(memory.buffer, out, 4)));
