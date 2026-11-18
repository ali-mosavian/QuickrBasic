// node tools/wasm/help_test.mjs: the browser build ships F1's help file and hands it to the IDE.
// It did neither: the page said help was not available.
import fs from 'node:fs';

const main = fs.readFileSync(new URL('../../src/plat/wasm/web/main.mjs', import.meta.url), 'utf8');
const shipped = fs.existsSync(new URL('../../build/web/BASIC.HLP', import.meta.url));
const bad = [];
if (!/SUPPORT\s*=\s*\[[^\]]*'BASIC\.HLP'/.test(main)) bad.push('main.mjs does not load BASIC.HLP into the files');
if (!shipped) bad.push('build/web/BASIC.HLP is missing (make web)');
if (bad.length) {
  console.log(`FAIL help: ${bad.join('; ')}`);
  process.exit(1);
}
console.log('help ok');
