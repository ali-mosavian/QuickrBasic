// node tools/wasm/retrace_test.mjs MODULE.wasm: port 3DAh's retrace begins once for the frames a page has shown.
// It began and ended at every look: a game waiting for it to step with the screen ran as fast as it could look.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'retrace-'));
const bas = path.join(dir, 'RETRACE.BAS');
const looks = 'INP(&H3DA) AND 8; INP(&H3DA) AND 8; INP(&H3DA) AND 8';
fs.writeFileSync(bas, `t# = TIMER\r\nDO: LOOP WHILE TIMER - t# < 1\r\nPRINT ${looks}\r\n`);
const run = new URL('./run.mjs', import.meta.url).pathname;
const r = spawnSync('node', [run, process.argv[2], bas], { env: { ...process.env, QB_FRAMES: '2' }, timeout: 20000 });
fs.rmSync(dir, { recursive: true });
const out = r.stdout.toString();
const ok = out.includes(' 8  0  0');
console.log(ok ? 'retrace ok' : `FAIL out ${JSON.stringify(out)}`);
process.exit(ok ? 0 : 1);
