// node tools/wasm/play_test.mjs MODULE.wasm: PLAY in the background plays every note and ends the
// last while the program runs on. On wasm the music's tick came only from waits for the music: a
// program that went on after PLAY "MB..." left its first note sounding for good.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'play-'));
const bas = path.join(dir, 'PLAY.BAS');
fs.writeFileSync(bas, 'PLAY "MB T255 L32 CDE"\r\nt# = TIMER\r\nDO: LOOP WHILE TIMER - t# < 1\r\nPRINT "done"\r\n');
const run = new URL('./run.mjs', import.meta.url).pathname;
const r = spawnSync('node', [run, process.argv[2], bas], { env: { ...process.env, QB_TONES: '1' }, timeout: 20000 });
fs.rmSync(dir, { recursive: true });
const tones = [...r.stderr.toString().matchAll(/^tone (\d+) /gm)].map((m) => +m[1]);
const ok = r.stdout.toString().includes('done') && tones.filter((t) => t > 0).length === 3 && tones.at(-1) === 0;
console.log(ok ? 'play ok' : `FAIL tones ${JSON.stringify(tones)}, out ${JSON.stringify(r.stdout.toString())}`);
process.exit(ok ? 0 : 1);
