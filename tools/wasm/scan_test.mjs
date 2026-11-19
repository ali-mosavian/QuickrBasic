// node tools/wasm/scan_test.mjs MODULE.wasm: port 60h is the keys down and up, a byte a look, the last kept.
// It read FFh on the page: a game that reads held keys there fell back to INKEY$'s repeats, and a key held
// was forgotten as a second went down.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { scanCode } from '../../src/plat/wasm/web/keys.mjs';

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'scan-'));
const bas = path.join(dir, 'SCAN.BAS');
fs.writeFileSync(bas, 't# = TIMER\r\nDO: LOOP WHILE TIMER - t# < 1\r\nPRINT INP(&H60); INP(&H60); INP(&H60); INP(&H60)\r\n');
const run = new URL('./run.mjs', import.meta.url).pathname;
const keys = ['ArrowRight', 'Space'].map((code) => scanCode({ code }).toString(16));
const QB_SCANS = [...keys, (parseInt(keys[0], 16) | 0x80).toString(16)].join(',');
const r = spawnSync('node', [run, process.argv[2], bas], { env: { ...process.env, QB_SCANS }, timeout: 20000 });
fs.rmSync(dir, { recursive: true });
const out = r.stdout.toString();
const ok = out.includes(' 77  57  205  205');
console.log(ok ? 'scan ok' : `FAIL out ${JSON.stringify(out)}, scans ${QB_SCANS}`);
process.exit(ok ? 0 : 1);
