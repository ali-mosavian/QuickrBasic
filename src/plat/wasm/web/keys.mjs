// A keyboard event as the BIOS's key (INT 16h AH=10h): scan code << 8 | character, and its shift
// flags (pkey.h's PKEY_*).

// code -> [scan, normal, shift] characters; the scan codes of the US layout's keys.
const TYPED = {
  Backquote: [0x29, '`', '~'], Digit1: [0x02, '1', '!'], Digit2: [0x03, '2', '@'], Digit3: [0x04, '3', '#'],
  Digit4: [0x05, '4', '$'], Digit5: [0x06, '5', '%'], Digit6: [0x07, '6', '^'], Digit7: [0x08, '7', '&'],
  Digit8: [0x09, '8', '*'], Digit9: [0x0A, '9', '('], Digit0: [0x0B, '0', ')'], Minus: [0x0C, '-', '_'],
  Equal: [0x0D, '=', '+'], BracketLeft: [0x1A, '[', '{'], BracketRight: [0x1B, ']', '}'], Backslash: [0x2B, '\\', '|'],
  Semicolon: [0x27, ';', ':'], Quote: [0x28, "'", '"'], Comma: [0x33, ',', '<'], Period: [0x34, '.', '>'],
  Slash: [0x35, '/', '?'], Space: [0x39, ' ', ' '],
};
const LETTERS = 'QWERTYUIOPASDFGHJKLZXCVBNM';
const LETTER_SCANS = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
  0x24, 0x25, 0x26, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32];
for (let i = 0; i < LETTERS.length; i++)
  TYPED['Key' + LETTERS[i]] = [LETTER_SCANS[i], LETTERS[i].toLowerCase(), LETTERS[i]];

// The grey keys: [normal, ctrl, alt] scan codes, the character E0h (0 for Alt's).
const GREY = {
  Home: [0x47, 0x77, 0x97], ArrowUp: [0x48, 0x8D, 0x98], PageUp: [0x49, 0x84, 0x99], ArrowLeft: [0x4B, 0x73, 0x9B],
  ArrowRight: [0x4D, 0x74, 0x9D], End: [0x4F, 0x75, 0x9F], ArrowDown: [0x50, 0x91, 0xA0], PageDown: [0x51, 0x76, 0xA1],
  Insert: [0x52, 0x92, 0xA2], Delete: [0x53, 0x93, 0xA3],
};

// F1..F10: 3Bh.., shifted 54h.., Ctrl 5Eh.., Alt 68h..; F11, F12 their own.
function fkey(n, e) {
  if (n <= 10) {
    const base = e.altKey ? 0x68 : e.ctrlKey ? 0x5E : e.shiftKey ? 0x54 : 0x3B;
    return (base + n - 1) << 8;
  }
  const base = e.altKey ? 0x8B : e.ctrlKey ? 0x89 : e.shiftKey ? 0x87 : 0x85;
  return (base + n - 11) << 8;
}

const ALT_DIGIT = { Digit1: 0x78, Digit2: 0x79, Digit3: 0x7A, Digit4: 0x7B, Digit5: 0x7C, Digit6: 0x7D,
  Digit7: 0x7E, Digit8: 0x7F, Digit9: 0x80, Digit0: 0x81, Minus: 0x82, Equal: 0x83 };

// The key's scan code as port 60h has it, 0 none.
export function scanCode(e) {
  const { code } = e;
  const m = /^F(\d+)$/.exec(code);
  if (m && +m[1] <= 10) return 0x3B + +m[1] - 1;
  if (code in GREY) return GREY[code][0];
  if (code in TYPED) return TYPED[code][0];
  return { Enter: 0x1C, NumpadEnter: 0x1C, Escape: 0x01, Backspace: 0x0E, Tab: 0x0F, F11: 0x57, F12: 0x58 }[code] ?? 0;
}

// The key an event types, or null (a modifier alone, a key the BIOS has none for).
export function biosKey(e) {
  const { code } = e;
  const m = /^F(\d+)$/.exec(code);
  if (m && +m[1] <= 12) return fkey(+m[1], e);
  if (code in GREY) {
    const [n, c, a] = GREY[code];
    return e.altKey ? a << 8 : (e.ctrlKey ? c : n) << 8 | 0xE0;
  }
  switch (code) {
    case 'Enter': case 'NumpadEnter': return e.altKey ? 0xA600 : 0x1C00 | (e.ctrlKey ? 0x0A : 0x0D);
    case 'Escape': return e.altKey ? 0x0100 : 0x011B;
    case 'Backspace': return e.altKey ? 0x0E00 : 0x0E00 | (e.ctrlKey ? 0x7F : 0x08);
    case 'Tab': return e.altKey ? 0xA500 : e.ctrlKey ? 0x9400 : e.shiftKey ? 0x0F00 : 0x0F09;
  }
  if (code in TYPED) {
    const [scan, lo, hi] = TYPED[code];
    if (e.altKey) return (ALT_DIGIT[code] ?? scan) << 8;
    if (e.ctrlKey) {
      if (code.startsWith('Key')) return scan << 8 | (lo.charCodeAt(0) - 96);
      if (code === 'BracketLeft') return scan << 8 | 0x1B;
      if (code === 'Backslash') return scan << 8 | 0x1C;
      if (code === 'BracketRight') return scan << 8 | 0x1D;
      if (code === 'Space') return scan << 8 | 0x20;
      return null;
    }
    // The character the layout typed, when it is one byte of CP437's ASCII half.
    const ch = e.key.length === 1 && e.key.charCodeAt(0) < 128 ? e.key : e.shiftKey ? hi : lo;
    return scan << 8 | ch.charCodeAt(0);
  }
  if (e.key.length === 1 && e.key.charCodeAt(0) < 128) return e.key.charCodeAt(0);
  return null;
}

// The BIOS's shift flags: right and left Shift, Ctrl, Alt, Num and Caps Lock.
export function shiftFlags(e) {
  return (e.shiftKey ? 0x02 : 0) | (e.ctrlKey ? 0x04 : 0) | (e.altKey ? 0x08 : 0)
    | (e.getModifierState?.('NumLock') ? 0x20 : 0) | (e.getModifierState?.('CapsLock') ? 0x40 : 0);
}
