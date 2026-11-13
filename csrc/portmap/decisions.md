# Cross-group decisions (phase 2)

1. Interrupts run no C. The jwasm handlers only set bits in `exec.stop` (`SF_BRK` from INT 23h and
   the tick's Ctrl+C scan, `SF_TICK` from the tick) and count ticks; the core polls (`exec_brk` →
   `evnt_poll`). `psys_init` takes no tick callback.
2. vm owns `plat/pkey.h` and `plat/pclk.h` and their dos32 code, including what io needs:
   `pkey_wait`, `pkey_flush`, `pkey_poll`, and the jiffy clock with its offset (`plt_rdtim`,
   `plt_settim`) as `pclk_jiffies` / `pclk_set_jiffies`. io calls them.
3. Handler names: `ops.def` uses the names each module's header declares (io's `file.h`, num's
   `dubl.h`, mem's ...). vm reconciles `ops.def` with them; the owners' names win.
4. Arena regions: each owner carves its own in its init with `allo_carve` (strg: temporaries'
   Descs; exec: data stack, save stack; cons: inbuf, sbuf; file: record buffers; comp: cbuf).
   `main.c` (io) calls `pmem_init`, `allo_init`, then every carving init, then the blocks (cache,
   program store, symbols), in the asm's order.
5. FRE: the suites never compare its value; the regions inside the heap are fine.
6. `gen.inc` (`g_msgs`, `g_kwlist`, `g_kwlist1`): comp's `x86dfa.py` gets a C mode for it too.
7. Op sizes: `flow_opsize` from `ops.def` (vm); comp uses it.
8. 64-bit division: check that llrm's x86_32-dos links `__udivdi3`/`__divdi3` before relying on it.
