// The page's upload (web/upload.mjs): a program is opened; a native module is kept beside it for an
// IMPORT to find, not opened. A .QBX was once opened as the program, its bytes as BASIC.
import assert from 'node:assert/strict';
import { uploadAs } from '../../src/plat/wasm/web/upload.mjs';

assert.deepEqual(uploadAs('my game.bas'), { path: '/MYGAME.BAS', open: true });
assert.deepEqual(uploadAs('qxtest.qbx'), { path: '/QXTEST.QBX', open: false });
console.log('upload ok');
