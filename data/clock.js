// clock.js — PongClock browser clock engine
// Faithful port of the C++ clock.cpp / fonts.h logic to JavaScript.
// Renders on an HTML5 canvas (288×192 px, 48×32 virtual LEDs × 6 px each).

'use strict';

// ── Font data ─────────────────────────────────────────────────────────────────
// Stored as flat Uint8Arrays. Access: MYFONT[idx*5+col], etc.
// Bit layout matches C++ PROGMEM arrays — see fonts.h for documentation.

// 5×7 font — 68 glyphs × 5 bytes. bit(row) set → LED on. bit0=row0(top).
const MYFONT = new Uint8Array([
  0x00,0x00,0x00,0x00,0x00, // 0: space
  0x7E,0x11,0x11,0x11,0x7E, // 1: A
  0x7F,0x49,0x49,0x49,0x36, // 2: B
  0x3E,0x41,0x41,0x41,0x22, // 3: C
  0x7F,0x41,0x41,0x41,0x3E, // 4: D
  0x7F,0x49,0x49,0x49,0x41, // 5: E
  0x7F,0x09,0x09,0x09,0x01, // 6: F
  0x3E,0x41,0x49,0x49,0x7A, // 7: G
  0x7F,0x08,0x08,0x08,0x7F, // 8: H
  0x00,0x41,0x7F,0x41,0x00, // 9: I
  0x20,0x40,0x41,0x3F,0x01, //10: J
  0x7F,0x08,0x14,0x22,0x41, //11: K
  0x7F,0x40,0x40,0x40,0x40, //12: L
  0x7F,0x02,0x0C,0x02,0x7F, //13: M
  0x7F,0x04,0x08,0x10,0x7F, //14: N
  0x3E,0x41,0x41,0x41,0x3E, //15: O
  0x7F,0x09,0x09,0x09,0x06, //16: P
  0x3E,0x41,0x51,0x21,0x5E, //17: Q
  0x7F,0x09,0x19,0x29,0x46, //18: R
  0x46,0x49,0x49,0x49,0x31, //19: S
  0x01,0x01,0x7F,0x01,0x01, //20: T
  0x3F,0x40,0x40,0x40,0x3F, //21: U
  0x1F,0x20,0x40,0x20,0x1F, //22: V
  0x3F,0x40,0x38,0x40,0x3F, //23: W
  0x63,0x14,0x08,0x14,0x63, //24: X
  0x07,0x08,0x70,0x08,0x07, //25: Y
  0x61,0x51,0x49,0x45,0x43, //26: Z
  0x00,0x00,0x00,0x00,0x40, //27: .
  0x00,0x00,0x03,0x00,0x00, //28: '
  0x00,0x00,0x36,0x00,0x00, //29: :
  0x00,0x41,0x22,0x14,0x08, //30: >
  0x3E,0x51,0x49,0x45,0x3E, //31: 0
  0x00,0x42,0x7F,0x40,0x00, //32: 1
  0x42,0x61,0x51,0x49,0x46, //33: 2
  0x21,0x41,0x45,0x4B,0x31, //34: 3
  0x18,0x14,0x12,0x7F,0x10, //35: 4
  0x27,0x45,0x45,0x45,0x39, //36: 5
  0x3C,0x4A,0x49,0x49,0x30, //37: 6
  0x01,0x71,0x09,0x05,0x03, //38: 7
  0x36,0x49,0x49,0x49,0x36, //39: 8
  0x06,0x49,0x49,0x29,0x1E, //40: 9
  0x20,0x54,0x54,0x54,0x78, //41: a
  0x7F,0x48,0x44,0x44,0x38, //42: b
  0x38,0x44,0x44,0x44,0x20, //43: c
  0x38,0x44,0x44,0x48,0x7F, //44: d
  0x38,0x54,0x54,0x54,0x18, //45: e
  0x08,0x7E,0x09,0x01,0x02, //46: f
  0x0C,0x52,0x52,0x52,0x3E, //47: g
  0x7F,0x08,0x04,0x04,0x78, //48: h
  0x00,0x44,0x7D,0x40,0x00, //49: i
  0x20,0x40,0x44,0x3D,0x00, //50: j
  0x7F,0x10,0x28,0x44,0x00, //51: k
  0x00,0x41,0x7F,0x40,0x00, //52: l
  0x7C,0x04,0x18,0x04,0x78, //53: m
  0x7C,0x08,0x04,0x04,0x78, //54: n
  0x38,0x44,0x44,0x44,0x38, //55: o
  0x7C,0x14,0x14,0x14,0x08, //56: p
  0x08,0x14,0x14,0x18,0x7C, //57: q
  0x7C,0x08,0x04,0x04,0x08, //58: r
  0x48,0x54,0x54,0x54,0x20, //59: s
  0x04,0x3F,0x44,0x40,0x20, //60: t
  0x3C,0x40,0x40,0x20,0x7C, //61: u
  0x1C,0x20,0x40,0x20,0x1C, //62: v
  0x3C,0x40,0x30,0x40,0x3C, //63: w
  0x44,0x28,0x10,0x28,0x44, //64: x
  0x0C,0x50,0x50,0x50,0x3C, //65: y
  0x44,0x64,0x54,0x4C,0x44, //66: z
  0x00,0x00,0x00,0x00,0x00, //67: (padding)
]);

// 10×14 big font — 10 glyphs × 20 bytes. MSB = row 0 (top). See fonts.h.
const MYBIGFONT = new Uint8Array([
  // 0
  0x3C,0xFC,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFC,0x3C,
  0xF0,0xFC,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
  // 1
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x3C,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0xF0,
  // 2
  0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0x3C,
  0xF0,0xFC,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00,
  // 3
  0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0x3C,
  0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
  // 4
  0x3C,0x3F,0x03,0x03,0x03,0x03,0x03,0x03,0x3F,0x3C,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0xF0,
  // 5
  0x3C,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x00,
  0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
  // 6
  0x3C,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x00,
  0xF0,0xFC,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
  // 7
  0x00,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFC,0x3C,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0xF0,
  // 8
  0x3C,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0x3C,
  0xF0,0xFC,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
  // 9
  0x3C,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0x3C,
  0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFC,0xF0,
]);

// 3×5 tiny font — 41 glyphs × 3 bytes. bit(row) set → LED on. bit0=row0(top).
const MYTINYFONT = new Uint8Array([
  0x00,0x00,0x00, // 0: space
  0x1F,0x05,0x1F, // 1: A
  0x1F,0x15,0x0A, // 2: B
  0x0E,0x11,0x11, // 3: C
  0x1F,0x11,0x0E, // 4: D
  0x1F,0x15,0x11, // 5: E
  0x1F,0x05,0x01, // 6: F
  0x0E,0x11,0x1D, // 7: G
  0x1F,0x04,0x1F, // 8: H
  0x11,0x1F,0x11, // 9: I
  0x08,0x10,0x0F, //10: J
  0x1F,0x04,0x1B, //11: K
  0x1F,0x10,0x10, //12: L
  0x1F,0x02,0x1F, //13: M
  0x1F,0x01,0x1F, //14: N
  0x0E,0x11,0x0E, //15: O
  0x1F,0x05,0x02, //16: P
  0x0E,0x19,0x1E, //17: Q
  0x1F,0x05,0x1A, //18: R
  0x12,0x15,0x09, //19: S
  0x01,0x1F,0x01, //20: T
  0x0F,0x10,0x1F, //21: U
  0x07,0x18,0x07, //22: V
  0x1F,0x0C,0x1F, //23: W
  0x1B,0x04,0x1B, //24: X
  0x03,0x1C,0x03, //25: Y
  0x19,0x15,0x13, //26: Z
  0x00,0x10,0x00, //27: .
  0x00,0x03,0x00, //28: '
  0x00,0x17,0x00, //29: !
  0x02,0x15,0x02, //30: ?
  0x0E,0x11,0x0E, //31: 0
  0x00,0x1F,0x00, //32: 1
  0x19,0x15,0x12, //33: 2
  0x11,0x15,0x0E, //34: 3
  0x07,0x04,0x1F, //35: 4
  0x17,0x15,0x09, //36: 5
  0x1E,0x15,0x1C, //37: 6
  0x01,0x1D,0x03, //38: 7
  0x1F,0x15,0x1F, //39: 8
  0x07,0x05,0x1F, //40: 9
]);

// ── Matrix state ──────────────────────────────────────────────────────────────
const LED_W       = 48;
const LED_H       = 32;
const PIXEL_SIZE  = 6;
const PIXEL_INNER = 4;
const PIXEL_MARGIN = 1;

let cells = new Uint8Array(LED_W * LED_H);  // 0=off 1=on

// ── Config ────────────────────────────────────────────────────────────────────
let cfg = {
  mode: 1,
  brightness: 180,
  ampm: false,
  ledOnR: 255, ledOnG: 140, ledOnB: 0,
  ledOffR: 20, ledOffG: 8,  ledOffB: 0,
  timezone: 'Australia/Sydney',
  ntpServer: 'pool.ntp.org',
  dateInterval: 10,
  ldrEnabled: false,
};

// ── Canvas ────────────────────────────────────────────────────────────────────
let canvas, ctx;

function initCanvas() {
  canvas = document.getElementById('ledCanvas');
  ctx    = canvas.getContext('2d');
}

// ── Helpers ───────────────────────────────────────────────────────────────────
function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function rnd(min, max) {   // integer in [min, max)  — matches Arduino random(min,max)
  return Math.floor(Math.random() * (max - min)) + min;
}

function toHex(r, g, b) {
  return '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
}

// ── LED matrix operations ─────────────────────────────────────────────────────
function plot(x, y, on) {
  if (x < 0 || x >= LED_W || y < 0 || y >= LED_H) return;
  cells[y * LED_W + x] = on ? 1 : 0;
}

function cls() {
  cells.fill(0);
}

function pushMatrix() {
  const onColor  = toHex(cfg.ledOnR,  cfg.ledOnG,  cfg.ledOnB);
  const offColor = toHex(cfg.ledOffR, cfg.ledOffG, cfg.ledOffB);

  // Dark background fill (shows in 1 px gaps between cells)
  ctx.fillStyle = '#050200';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  for (let y = 0; y < LED_H; y++) {
    for (let x = 0; x < LED_W; x++) {
      ctx.fillStyle = cells[y * LED_W + x] ? onColor : offColor;
      const px = x * PIXEL_SIZE + PIXEL_MARGIN;
      const py = y * PIXEL_SIZE + PIXEL_MARGIN;
      ctx.beginPath();
      ctx.roundRect(px, py, PIXEL_INNER, PIXEL_INNER, 1);
      ctx.fill();
    }
  }
}

// ── Time ──────────────────────────────────────────────────────────────────────
function getTime() {
  const now = new Date();
  try {
    const parts = {};
    new Intl.DateTimeFormat('en-AU', {
      timeZone: cfg.timezone,
      hour:    '2-digit', minute: '2-digit', second: '2-digit',
      day:     '2-digit', month:  '2-digit', year:   '2-digit',
      weekday: 'short',   hour12: false,
    }).formatToParts(now).forEach(p => { parts[p.type] = p.value; });

    let hr = parseInt(parts.hour, 10);
    if (hr === 24) hr = 0;  // some engines return 24 for midnight

    const dayShorts = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
    const wd = (parts.weekday || '').slice(0, 3);
    const dow = Math.max(0, dayShorts.findIndex(d => d === wd));

    return {
      seconds: parseInt(parts.second, 10),
      minutes: parseInt(parts.minute, 10),
      hours:   hr,
      dow:     dow,
      day:     parseInt(parts.day,   10),
      month:   parseInt(parts.month, 10),
      year:    parseInt(parts.year,  10),
    };
  } catch (_) {
    return {
      seconds: now.getSeconds(),
      minutes: now.getMinutes(),
      hours:   now.getHours(),
      dow:     now.getDay(),
      day:     now.getDate(),
      month:   now.getMonth() + 1,
      year:    now.getFullYear() % 100,
    };
  }
}

// ── Font index helpers ────────────────────────────────────────────────────────
function charIdx5x7(c) {
  const code = c.charCodeAt(0);
  if (code >= 65 && code <= 90)  return code & 0x1F;          // A-Z
  if (code >= 97 && code <= 122) return (code - 97) + 41;     // a-z
  if (code >= 48 && code <= 57)  return (code - 48) + 31;     // 0-9
  if (c === ' ')  return 0;
  if (c === '.')  return 27;
  if (c === "'")  return 28;
  if (c === ':')  return 29;
  if (c === '>')  return 30;
  return 0;
}

function charIdx3x5(c) {
  const code = c.charCodeAt(0);
  if (code >= 65 && code <= 90)  return code & 0x1F;   // A-Z
  if (code >= 97 && code <= 122) return code & 0x1F;   // a-z → same glyphs as A-Z
  if (code >= 48 && code <= 57)  return (code - 48) + 31;
  if (c === ' ')  return 0;
  if (c === '.')  return 27;
  if (c === "'")  return 28;
  if (c === '!')  return 29;
  if (c === '?')  return 30;
  return 0;
}

// ── Character renderers ───────────────────────────────────────────────────────
function putChar(x, y, c) {       // 5×7
  const idx = charIdx5x7(c);
  for (let col = 0; col < 5; col++) {
    const dots = MYFONT[idx * 5 + col];
    for (let row = 0; row < 7; row++) {
      plot(x + col, y + row, (dots & (1 << row)) !== 0);
    }
  }
}

function putTinyChar(x, y, c) {   // 3×5
  const idx = charIdx3x5(c);
  for (let col = 0; col < 3; col++) {
    const dots = MYTINYFONT[idx * 3 + col];
    for (let row = 0; row < 5; row++) {
      plot(x + col, y + row, (dots & (1 << row)) !== 0);
    }
  }
}

function putBigChar(x, y, c) {    // 10×14 digits only
  if (c < '0' || c > '9') return;
  const idx = c.charCodeAt(0) - 48;
  for (let col = 0; col < 10; col++) {
    let dots = MYBIGFONT[idx * 20 + col];         // top 8 rows, MSB=row0
    for (let row = 0; row < 8; row++) {
      plot(x + col, y + row, (dots & (128 >> row)) !== 0);
    }
    dots = MYBIGFONT[idx * 20 + col + 10];        // bottom 6 rows, MSB=row0
    for (let row = 0; row < 6; row++) {
      plot(x + col, y + 8 + row, (dots & (128 >> row)) !== 0);
    }
  }
}

// ── Slide animation — one frame (seq 0-8) ────────────────────────────────────
function slideanim(x, y, seq, oldC, newC) {
  const oi = charIdx5x7(oldC);
  const ni = charIdx5x7(newC);

  // Old char slides down
  if (seq < 7) {
    const maxRow = 6 - seq;
    const drawY  = y + seq;
    for (let col = 0; col < 5; col++) {
      const dots = MYFONT[oi * 5 + col];
      for (let row = 0; row <= maxRow; row++) {
        plot(x + col, drawY + row, (dots & (1 << row)) !== 0);
      }
    }
  }

  // Blank gap row above old char
  if (seq >= 1 && seq <= 7) {
    const blankY = y + seq - 1;
    for (let col = 0; col < 5; col++) plot(x + col, blankY, false);
  }

  // New char slides in from top
  if (seq >= 2) {
    const minRow = 6 - (seq - 2);
    for (let col = 0; col < 5; col++) {
      const dots = MYFONT[ni * 5 + col];
      let sy = 0;
      for (let row = minRow; row <= 6; row++) {
        plot(x + col, y + sy, (dots & (1 << row)) !== 0);
        sy++;
      }
    }
  }
}

// ── Date row — "WED 18 MAR" in tiny font ─────────────────────────────────────
const DAY_ABBR = ['SUN','MON','TUE','WED','THU','FRI','SAT'];
const MON_ABBR = ['JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC'];

function drawDateRow(y) {
  const t   = getTime();
  const d   = DAY_ABBR[t.dow]       || 'MON';
  const m   = MON_ABBR[t.month - 1] || 'JAN';
  const day = t.day;

  let str = d[0] + d[1] + d[2] + ' ';
  str += (day >= 10 ? String(Math.floor(day / 10)) : ' ');
  str += String(day % 10) + ' ';
  str += m[0] + m[1] + m[2];  // 10 chars total

  for (let i = 0; i < 10; i++) putTinyChar(4 + i * 4, y, str[i]);
}

// ── Mode controller ───────────────────────────────────────────────────────────
let activeMode = null;   // 'slide' | 'pong' | 'digits' | 'word' | null

const MODE_KEYS  = ['slide', 'pong', 'digits', 'word'];
const MODE_NAMES = ['Slide', 'Pong', 'Digits', 'Word Clock'];

function startMode(modeName) {
  activeMode = modeName;
  cls();
  pushMatrix();
  applyBrightness();

  switch (modeName) {
    case 'slide':  slideModeRun().catch(console.error);  break;
    case 'pong':   pongModeRun().catch(console.error);   break;
    case 'digits': digitsModeRun().catch(console.error); break;
    case 'word':   wordModeRun().catch(console.error);   break;
  }

  // Sync mode pills
  document.querySelectorAll('.mode-pill').forEach(el => {
    el.classList.toggle('active', el.dataset.mode === modeName);
  });

  // Sync config form radio
  const radio = document.querySelector(`input[name="mode"][value="${MODE_KEYS.indexOf(modeName)}"]`);
  if (radio) radio.checked = true;
}

// Switch mode (called from pill click and config radio)
function switchMode(newMode) {
  activeMode = null;           // signals current async loop to exit
  setTimeout(() => startMode(newMode), 120);

  // Persist to device
  fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: MODE_KEYS.indexOf(newMode) }),
  }).catch(() => {});
}

function applyBrightness() {
  if (canvas) canvas.style.filter = `brightness(${cfg.brightness / 255})`;
}

// ── SLIDE MODE ────────────────────────────────────────────────────────────────
async function slideModeRun() {
  const XPOS   = [42, 36, 24, 18, 6, 0];
  const TIME_Y = 9;
  const DATE_Y = 18;
  const SLIDE_DELAY_MS = 20;

  function digits(t) {
    let h = t.hours;
    if (cfg.ampm) { if (h > 12) h -= 12; if (h < 1) h += 12; }
    return [t.seconds % 10, Math.floor(t.seconds / 10),
            t.minutes % 10, Math.floor(t.minutes / 10),
            h % 10,          Math.floor(h / 10)];
  }

  let t      = getTime();
  let dOld   = digits(t);
  let oldSec = t.seconds;

  cls();
  for (let i = 0; i < 6; i++) {
    const ch = (cfg.ampm && i === 5 && dOld[5] === 0) ? ' ' : String(dOld[i]);
    putChar(XPOS[i], TIME_Y, ch);
  }
  putChar(12, TIME_Y, ':');
  putChar(30, TIME_Y, ':');
  drawDateRow(DATE_Y);
  pushMatrix();

  while (activeMode === 'slide') {
    await sleep(20);
    if (activeMode !== 'slide') break;

    t = getTime();
    if (t.seconds === oldSec) continue;
    oldSec = t.seconds;

    const dNew = digits(t);

    for (let i = 0; i < 6; i++) {
      if (dOld[i] === dNew[i]) continue;
      if (activeMode !== 'slide') break;

      const oldCh = (cfg.ampm && i === 5 && dOld[5] === 0) ? ' ' : String(dOld[i]);
      const newCh = (cfg.ampm && i === 5 && dNew[5] === 0) ? ' ' : String(dNew[i]);

      for (let seq = 0; seq <= 8; seq++) {
        if (activeMode !== 'slide') return;
        slideanim(XPOS[i], TIME_Y, seq, oldCh, newCh);
        putChar(12, TIME_Y, ':');
        putChar(30, TIME_Y, ':');
        pushMatrix();
        await sleep(SLIDE_DELAY_MS);
      }
    }

    drawDateRow(DATE_Y);
    pushMatrix();
    for (let i = 0; i < 6; i++) dOld[i] = dNew[i];
  }
}

// ── PONG MODE ─────────────────────────────────────────────────────────────────
async function pongModeRun() {
  const SCORE_Y   = 0;
  const DATE_Y    = 27;   // LED_H - 5
  const BAT_H     = 8;    // BAT_HEIGHT
  const BAT_MAX_Y = 24;   // LED_H - BAT_H
  const AI_MID    = 16;
  const BAT1_X    = 2;
  const BAT2_X    = 45;
  const BALL_MS   = 20;

  function ballEndpoint(bx, by, vx, vy) {
    let cx = bx, cy = by;
    while (cx > BAT1_X && cx < BAT2_X) {
      cx += vx; cy += vy;
      if (cy <= 0)           { vy = -vy; cy = 0; }
      if (cy >= LED_H - 1)   { vy = -vy; cy = LED_H - 1; }
    }
    return Math.trunc(cy);
  }

  // Intro sequence
  await pongIntro();
  if (activeMode !== 'pong') return;

  let bx = 23, by = rnd(8, 24);
  let vx = rnd(0,2) > 0 ? 1.0 : -1.0;
  let vy = rnd(0,2) > 0 ? 0.5 : -0.5;
  let ex = 10, ey = 10;
  let bat1_y = 12, bat2_y = 12, bat1_t = 12, bat2_t = 12;
  let bat1upd = true, bat2upd = true;
  let bat1miss = false, bat2miss = false;
  let restart = true;
  let sc_h0='0', sc_h1='0', sc_m0='0', sc_m1='0';
  let sc_date = '          ';

  while (activeMode === 'pong') {
    if (restart) {
      plot(ex, ey, false);

      const t = getTime();
      let hrs = t.hours, mins = t.minutes;
      if (cfg.ampm) { if (hrs > 12) hrs -= 12; if (hrs < 1) hrs += 12; }

      sc_h0 = hrs < 10 ? '0' : String(Math.floor(hrs / 10));
      sc_h1 = String(hrs % 10);
      sc_m0 = mins < 10 ? '0' : String(Math.floor(mins / 10));
      sc_m1 = String(mins % 10);

      const d = DAY_ABBR[t.dow] || 'MON';
      const m = MON_ABBR[t.month - 1] || 'JAN';
      let ds = d[0]+d[1]+d[2]+' ';
      ds += (t.day >= 10 ? String(Math.floor(t.day/10)) : ' ');
      ds += String(t.day%10)+' '+m[0]+m[1]+m[2];
      sc_date = ds;

      putTinyChar(14,SCORE_Y,sc_h0); putTinyChar(18,SCORE_Y,sc_h1);
      putTinyChar(28,SCORE_Y,sc_m0); putTinyChar(32,SCORE_Y,sc_m1);
      for (let i=0;i<10;i++) putTinyChar(4+i*4,DATE_Y,sc_date[i]);
      pushMatrix();

      bx=23; by=rnd(8,24);
      vx=rnd(0,2)>0?1.0:-1.0; vy=rnd(0,2)>0?0.5:-0.5;
      bat1miss=false; bat2miss=false;
      restart=false;
      await sleep(400);
    }

    if (activeMode !== 'pong') break;

    const t = getTime();

    if (t.seconds === 59 && t.minutes < 59)  bat1miss = true;
    if (t.seconds === 59 && t.minutes === 59) bat2miss = true;

    // AI basic tracking
    const ibx = Math.trunc(bx);
    if (ibx === rnd(30, 41)) bat1_t = Math.trunc(by);
    if (ibx === rnd(8,  19)) bat2_t = Math.trunc(by);

    // Left bat predict (ball heading left, halfway across field)
    if (ibx === 23 && vx < 0) {
      const ep = ballEndpoint(bx, by, vx, vy);
      if (bat1miss) {
        bat1miss = false;
        bat1_t = ep > AI_MID ? rnd(0,3) : BAT_MAX_Y - 2 + rnd(0,3);
      } else {
        const s = ep < BAT_H ? 0 : ep - (BAT_H - 1);
        const e = ep <= BAT_MAX_Y ? ep : BAT_MAX_Y;
        bat1_t = rnd(s, e + 1);
      }
    }
    // Right bat predict
    if (ibx === 25 && vx > 0) {
      const ep = ballEndpoint(bx, by, vx, vy);
      if (bat2miss) {
        bat2miss = false;
        bat2_t = ep > AI_MID ? rnd(0,3) : BAT_MAX_Y - 2 + rnd(0,3);
      } else {
        const s = ep < BAT_H ? 0 : ep - (BAT_H - 1);
        const e = ep <= BAT_MAX_Y ? ep : BAT_MAX_Y;
        bat2_t = rnd(s, e + 1);
      }
    }

    // Move bats
    if (bat1_y > bat1_t && bat1_y > 0)         { bat1_y--; bat1upd=true; }
    if (bat1_y < bat1_t && bat1_y < BAT_MAX_Y) { bat1_y++; bat1upd=true; }
    if (bat2_y > bat2_t && bat2_y > 0)         { bat2_y--; bat2upd=true; }
    if (bat2_y < bat2_t && bat2_y < BAT_MAX_Y) { bat2_y++; bat2upd=true; }

    // Draw bat 1 (cols 0-1)
    if (bat1upd) {
      for (let i=0;i<LED_H;i++) {
        const on=(i>=bat1_y&&i<bat1_y+BAT_H);
        plot(BAT1_X-1,i,on); plot(BAT1_X-2,i,on);
      }
      bat1upd=false;
    }
    // Draw bat 2 (cols 46-47)
    if (bat2upd) {
      for (let i=0;i<LED_H;i++) {
        const on=(i>=bat2_y&&i<bat2_y+BAT_H);
        plot(BAT2_X+1,i,on); plot(BAT2_X+2,i,on);
      }
      bat2upd=false;
    }

    bx += vx; by += vy;
    if (by <= 0)          { vy=-vy; by=0; }
    if (by >= LED_H-1)    { vy=-vy; by=LED_H-1; }

    // Bat collisions
    if (Math.trunc(bx)===BAT1_X && bat1_y<=Math.trunc(by) && Math.trunc(by)<=bat1_y+BAT_H-1) {
      vx=-vx;
      if (rnd(0,3)!==0) {
        const fl=bat1_y<=2?0:bat1_y>=BAT_MAX_Y-2?1:rnd(0,2);
        if (fl===0){bat1_t+=rnd(1,3);if(vy<2.0)vy+=0.2;}
        else       {bat1_t-=rnd(1,3);if(vy>0.2)vy-=0.2;}
      }
    }
    if (Math.trunc(bx)===BAT2_X && bat2_y<=Math.trunc(by) && Math.trunc(by)<=bat2_y+BAT_H-1) {
      vx=-vx;
      if (rnd(0,3)!==0) {
        const fl=bat2_y<=2?0:bat2_y>=BAT_MAX_Y-2?1:rnd(0,2);
        if (fl===0){bat2_t+=rnd(1,3);if(vy<2.0)vy+=0.2;}
        else       {bat2_t-=rnd(1,3);if(vy>0.2)vy-=0.2;}
      }
    }

    // Draw ball
    const px=Math.trunc(bx), py=Math.trunc(by);
    plot(ex,ey,false); plot(px,py,true);
    ex=px; ey=py;

    // Redraw score and date every frame
    putTinyChar(14,SCORE_Y,sc_h0); putTinyChar(18,SCORE_Y,sc_h1);
    putTinyChar(28,SCORE_Y,sc_m0); putTinyChar(32,SCORE_Y,sc_m1);
    for (let i=0;i<10;i++) putTinyChar(4+i*4,DATE_Y,sc_date[i]);
    pushMatrix();

    if (px===0||px===47) restart=true;

    await sleep(BALL_MS);
  }
}

async function pongIntro() {
  const INTRO_Y = 13;  // (32-5)/2
  cls(); pushMatrix();

  const intro1 = 'Ready';
  for (let i=0;i<intro1.length;i++) {
    await sleep(25);
    if (activeMode!=='pong') return;
    putTinyChar(i*4+12, INTRO_Y, intro1[i]);
    pushMatrix();
  }
  for (let f=0;f<2;f++) {
    putTinyChar(34,INTRO_Y,'?'); pushMatrix(); await sleep(200);
    if (activeMode!=='pong') return;
    putTinyChar(34,INTRO_Y,' '); pushMatrix(); await sleep(400);
    if (activeMode!=='pong') return;
  }

  cls();
  const intro2='Play Pong!';
  for (let i=0;i<intro2.length;i++) {
    putTinyChar(i*4+6,INTRO_Y,intro2[i]); pushMatrix();
  }
  await sleep(800);
  if (activeMode!=='pong') return;
  cls();

  // Draw dotted centre net (col 24)
  const offset=rnd(0,2);
  for (let j=0;j<LED_H;j++) {
    if (activeMode!=='pong') return;
    if (j%2===0 && j+offset<LED_H) {
      plot(24,j+offset,true); pushMatrix();
      await sleep(30);
    }
  }
  await sleep(120);
}

// ── DIGITS MODE ───────────────────────────────────────────────────────────────
async function digitsModeRun() {
  const BIG_Y  = 9;
  const COL_Y1 = 12, COL_Y2 = 13, COL_Y3 = 18, COL_Y4 = 19;

  let oldMins = -1, oldSecs = -1;
  cls(); pushMatrix();

  while (activeMode === 'digits') {
    await sleep(50);
    if (activeMode !== 'digits') break;

    const t = getTime();
    let hrs = t.hours;
    if (cfg.ampm) { if (hrs > 12) hrs -= 12; if (hrs < 1) hrs += 12; }

    // Flash colon on each seconds tick
    if (t.seconds !== oldSecs) {
      oldSecs = t.seconds;
      const on = (oldSecs % 2 === 0);
      plot(23,COL_Y1,on); plot(24,COL_Y1,on);
      plot(23,COL_Y2,on); plot(24,COL_Y2,on);
      plot(23,COL_Y3,on); plot(24,COL_Y3,on);
      plot(23,COL_Y4,on); plot(24,COL_Y4,on);
      pushMatrix();
    }

    if (t.minutes === oldMins) continue;
    oldMins = t.minutes;

    const offset = (cfg.ampm && hrs < 10) ? 5 : 0;
    if (offset === 0) {
      putBigChar(0, BIG_Y, hrs < 10 ? '0' : String(Math.floor(hrs/10)));
    }
    putBigChar(12 - offset, BIG_Y, String(hrs % 10));
    putBigChar(26 - offset, BIG_Y, t.minutes < 10 ? '0' : String(Math.floor(t.minutes/10)));
    putBigChar(38 - offset, BIG_Y, String(t.minutes % 10));
    pushMatrix();
  }
}

// ── WORD CLOCK MODE ───────────────────────────────────────────────────────────
async function wordModeRun() {
  const WC_TOP_Y  = 6;
  const WC_BOT_Y  = 13;
  const WC_DATE_Y = 21;

  const NUMBERS = [
    'one','two','three','four','five','six','seven','eight','nine','ten',
    'eleven','twelve','thirteen','fourteen','fifteen','sixteen',
    'seventeen','eighteen','nineteen',
  ];
  const TENS = ['ten','twenty','thirty','forty','fifty'];

  let oldMins = -1;

  while (activeMode === 'word') {
    await sleep(50);
    if (activeMode !== 'word') break;

    const t = getTime();
    let hrs = t.hours;
    if (hrs > 12) hrs -= 12;
    if (hrs === 0) hrs = 12;

    if (t.minutes === oldMins) continue;
    oldMins = t.minutes;

    const mins = t.minutes;
    const md   = mins % 10;
    const mdt  = Math.floor(mins / 10) % 10;

    let top, bot;

    if (mins < 10 && mins > 0) {
      top = NUMBERS[md - 1].toUpperCase() + ' PAST';
      bot = NUMBERS[hrs - 1].toUpperCase();
    } else if (mins === 10) {
      top = 'TEN PAST';
      bot = NUMBERS[hrs - 1].toUpperCase();
    } else if (mdt !== 0 && md !== 0) {
      top = NUMBERS[hrs - 1].toUpperCase();
      bot = (mins >= 11 && mins <= 19)
        ? NUMBERS[mins - 1].toUpperCase()
        : TENS[mdt - 1].toUpperCase() + ' ' + NUMBERS[md - 1].toUpperCase();
    } else if (mdt !== 0 && md === 0) {
      top = NUMBERS[hrs - 1].toUpperCase();
      bot = TENS[mdt - 1].toUpperCase();
    } else {
      top = NUMBERS[hrs - 1].toUpperCase();
      bot = "O'CLOCK";
    }

    // Fade effect — just clear
    cls();

    // Centre and render (formula matches C++: ox = (45 - (len-1)*4) / 2)
    const topX = Math.floor((45 - (top.length - 1) * 4) / 2);
    for (let i = 0; i < top.length; i++) putTinyChar(topX + i * 4, WC_TOP_Y,  top[i]);

    const botX = Math.floor((45 - (bot.length - 1) * 4) / 2);
    for (let i = 0; i < bot.length; i++) putTinyChar(botX + i * 4, WC_BOT_Y,  bot[i]);

    drawDateRow(WC_DATE_Y);
    pushMatrix();
  }
}

// ── Config API ────────────────────────────────────────────────────────────────
async function loadDeviceConfig() {
  try {
    const [cfgRes, infoRes] = await Promise.all([
      fetch('/api/config'),
      fetch('/api/info'),
    ]);
    if (cfgRes.ok) {
      const data = await cfgRes.json();
      Object.assign(cfg, data);
      populateForm();
      applyBrightness();
    }
    if (infoRes.ok) {
      const info = await infoRes.json();
      // Start in whatever mode the device is currently showing
      const modeIdx = typeof info.mode === 'number' ? info.mode : 1;
      startMode(MODE_KEYS[modeIdx] || 'pong');
    } else {
      startMode(MODE_KEYS[cfg.mode] || 'pong');
    }
  } catch (_) {
    // No device — start with defaults
    startMode(MODE_KEYS[cfg.mode] || 'pong');
  }
}

async function saveDeviceConfig(patch) {
  try {
    const res = await fetch('/api/config', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(patch),
    });
    return res.ok;
  } catch (_) { return false; }
}

// ── Form population and interaction ──────────────────────────────────────────
function populateForm() {
  // Mode radios
  const modeRadio = document.querySelector(`input[name="mode"][value="${cfg.mode}"]`);
  if (modeRadio) modeRadio.checked = true;

  // Brightness
  const bright = document.getElementById('cfg-brightness');
  if (bright) { bright.value = cfg.brightness; updateRange(bright); }

  // AMPM
  const ampmEl = document.getElementById('cfg-ampm');
  if (ampmEl) ampmEl.checked = cfg.ampm;

  // LED colours
  setColorInput('cfg-led-on',  cfg.ledOnR,  cfg.ledOnG,  cfg.ledOnB);
  setColorInput('cfg-led-off', cfg.ledOffR, cfg.ledOffG, cfg.ledOffB);

  // Timezone + NTP
  const tzEl  = document.getElementById('cfg-timezone');
  const ntpEl = document.getElementById('cfg-ntp');
  if (tzEl)  tzEl.value  = cfg.timezone;
  if (ntpEl) ntpEl.value = cfg.ntpServer;

  // Date interval + LDR
  const dateEl = document.getElementById('cfg-date-interval');
  const ldrEl  = document.getElementById('cfg-ldr');
  if (dateEl) dateEl.value   = cfg.dateInterval;
  if (ldrEl)  ldrEl.checked  = cfg.ldrEnabled;
}

function setColorInput(id, r, g, b) {
  const el = document.getElementById(id);
  if (el) el.value = toHex(r, g, b);
}

function hexToRgb(hex) {
  const n = parseInt(hex.replace('#',''), 16);
  return { r: (n >> 16) & 0xFF, g: (n >> 8) & 0xFF, b: n & 0xFF };
}

function updateRange(el) {
  const out = document.getElementById(el.id + '-val');
  if (out) out.textContent = el.value;
}

// ── UI wiring (called after DOM ready) ───────────────────────────────────────
function wireUI() {
  // Tabs
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
      btn.classList.add('active');
      document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
    });
  });

  // Mode pills (on clock tab)
  document.querySelectorAll('.mode-pill').forEach(pill => {
    pill.addEventListener('click', () => switchMode(pill.dataset.mode));
  });

  // Mode radios (on config tab)
  document.querySelectorAll('input[name="mode"]').forEach(radio => {
    radio.addEventListener('change', () => {
      if (radio.checked) switchMode(MODE_KEYS[parseInt(radio.value, 10)]);
    });
  });

  // Brightness slider — live preview, saves on release
  const bright = document.getElementById('cfg-brightness');
  if (bright) {
    bright.addEventListener('input', () => {
      updateRange(bright);
      cfg.brightness = parseInt(bright.value, 10);
      applyBrightness();
    });
    bright.addEventListener('change', () => {
      cfg.brightness = parseInt(bright.value, 10);
      saveDeviceConfig({ brightness: cfg.brightness });
    });
  }

  // AMPM toggle
  const ampmEl = document.getElementById('cfg-ampm');
  if (ampmEl) {
    ampmEl.addEventListener('change', () => {
      cfg.ampm = ampmEl.checked;
    });
  }

  // Range display helpers
  document.querySelectorAll('input[type=range]').forEach(r => {
    r.addEventListener('input', () => updateRange(r));
  });

  // Config form Save
  const form = document.getElementById('config-form');
  if (form) {
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      await submitConfig();
    });
  }

  // WiFi Reset
  const wifiBtn = document.getElementById('btn-wifi-reset');
  if (wifiBtn) {
    wifiBtn.addEventListener('click', async () => {
      if (!confirm('Reset WiFi credentials?\n\nThe device will restart and create an AP named "CYD-PongClock". Connect to it to set new WiFi credentials.')) return;
      setStatus('Resetting WiFi — device restarting…');
      try {
        await fetch('/api/wifi-reset', { method: 'POST' });
      } catch (_) {}
      setStatus('Device restarting. Reconnect when WiFi is reconfigured.');
    });
  }

  // Populate timezone datalist if browser supports it
  if (typeof Intl.supportedValuesOf === 'function') {
    const list = document.getElementById('tz-list');
    if (list) {
      for (const tz of Intl.supportedValuesOf('timeZone')) {
        const opt = document.createElement('option');
        opt.value = tz;
        list.appendChild(opt);
      }
    }
  }
}

async function submitConfig() {
  const patch = {};

  const ampmEl   = document.getElementById('cfg-ampm');
  const onColor  = document.getElementById('cfg-led-on');
  const offColor = document.getElementById('cfg-led-off');
  const tzEl     = document.getElementById('cfg-timezone');
  const ntpEl    = document.getElementById('cfg-ntp');
  const dateEl   = document.getElementById('cfg-date-interval');
  const ldrEl    = document.getElementById('cfg-ldr');

  if (ampmEl)  { patch.ampm = ampmEl.checked; cfg.ampm = patch.ampm; }

  if (onColor)  {
    const c = hexToRgb(onColor.value);
    patch.ledOnR = cfg.ledOnR = c.r;
    patch.ledOnG = cfg.ledOnG = c.g;
    patch.ledOnB = cfg.ledOnB = c.b;
  }
  if (offColor) {
    const c = hexToRgb(offColor.value);
    patch.ledOffR = cfg.ledOffR = c.r;
    patch.ledOffG = cfg.ledOffG = c.g;
    patch.ledOffB = cfg.ledOffB = c.b;
  }
  if (tzEl)   { patch.timezone   = cfg.timezone   = tzEl.value.trim(); }
  if (ntpEl)  { patch.ntpServer  = cfg.ntpServer  = ntpEl.value.trim(); }
  if (dateEl) { patch.dateInterval = cfg.dateInterval = parseInt(dateEl.value, 10); }
  if (ldrEl)  { patch.ldrEnabled   = cfg.ldrEnabled   = ldrEl.checked; }

  setStatus('Saving…');
  const ok = await saveDeviceConfig(patch);
  setStatus(ok ? 'Saved.' : 'Saved (device offline — settings applied locally).');
  setTimeout(() => setStatus(''), 3000);
}

function setStatus(msg) {
  const el = document.getElementById('status-msg');
  if (el) el.textContent = msg;
}

// ── Entry point ───────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  initCanvas();
  wireUI();
  loadDeviceConfig();
});
