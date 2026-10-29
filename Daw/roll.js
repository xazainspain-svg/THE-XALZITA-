'use strict';
// Piano roll con herramientas y funciones al estilo de FL Studio: dibujar / seleccionar / borrar / cortar / silenciar,
// snap, zoom, carril de velocidad, resaltado de escala, estampador de acordes, notas fantasma, mover/alargar/duplicar.

const ROLL = { tool: 'draw', snap: 1, stepW: 22, root: 0, scale: 'chromatic', chord: 'none', ghost: true, sel: new Set(), lastLen: 1,
  drag: null, big: false, cv: null, kcv: null, rcv: null, vcv: null, scrollL: 0, scrollT: null, lastPlay: -2 };
const SCALES = { chromatic: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], major: [0, 2, 4, 5, 7, 9, 11], minor: [0, 2, 3, 5, 7, 8, 10],
  harmonic: [0, 2, 3, 5, 7, 8, 11], pentMaj: [0, 2, 4, 7, 9], pentMin: [0, 3, 5, 7, 10], dorian: [0, 2, 3, 5, 7, 9, 10] };
const SCALE_NAMES = { chromatic: 'Cromática', major: 'Mayor', minor: 'Menor', harmonic: 'Menor armónica', pentMaj: 'Pentat. mayor', pentMin: 'Pentat. menor', dorian: 'Dórica' };
const CHORDS = { none: [0], major: [0, 4, 7], minor: [0, 3, 7], sus4: [0, 5, 7], maj7: [0, 4, 7, 11], min7: [0, 3, 7, 10], dom7: [0, 4, 7, 10], octave: [0, 12] };
const CHORD_NAMES = { none: 'Sin acorde', major: 'Mayor', minor: 'Menor', sus4: 'Sus4', maj7: 'Maj7', min7: 'Min7', dom7: 'Dom7', octave: 'Octava' };
const SNAPS = [['Compás', 16], ['Tiempo', 4], ['1/2 tiempo', 2], ['Paso', 1], ['1/2 paso', 0.5], ['1/4 paso', 0.25], ['Sin snap', 0]];
const TOOLS = [['draw', '✎', '1', 'Dibujar'], ['select', '▭', '2', 'Seleccionar'], ['del', '✕', '3', 'Borrar'], ['slice', '✂', '4', 'Cortar'], ['mute', '⊘', '5', 'Silenciar'], ['paint', '▮', '6', 'Pintar (arrastra para dibujar varias notas)']];
const RK_W = 54, RULER_H = 20, VEL_H = 64;

const isBlackKey = n => [1, 3, 6, 8, 10].includes(n % 12);
function rollGeom(t) { return t.kind === 'drums' ? { hi: 7, lo: 0, h: 26 } : { hi: 96, lo: 24, h: 14 }; }
const inScale = n => SCALES[ROLL.scale].includes(((n - ROLL.root) % 12 + 12) % 12);
const clipLen = c => c.bars * 16;

function renderClipPane() {
  const pane = $('#clipPane'); pane.innerHTML = '';
  const t = song.tracks[selTrack], c = curClip();
  if (!c) { pane.appendChild(el('div', '', 'Selecciona un clip en la vista Session (o haz clic en una celda vacía para crear uno). Clic derecho en un clip: borrarlo.')); ROLL.cv = null; return; }
  if (ROLL.scrollT === null && ROLL.cv) { ROLL.scrollL = ROLL.cv.parentNode.scrollLeft; ROLL.scrollT = ROLL.cv.parentNode.scrollTop; }
  if (c.audio) { renderAudioPane(pane, c, t); ROLL.cv = null; return; }
  const drums = t.kind === 'drums';

  const bar = el('div', 'rollbar');
  const tp = el('div', 'tools'); TOOLS.forEach(([k, label, key, nm]) => { const b = el('button', ROLL.tool === k ? 'on' : '', label); b.title = nm + ' (atajo ' + key + ')'; b.onclick = () => { ROLL.tool = k; renderClipPane(); }; tp.appendChild(b); }); bar.appendChild(tp);
  const selOf = (opts, val, fn) => { const s = el('select'); opts.forEach(([v, l]) => { const o = el('option', '', l); o.value = v; s.appendChild(o); }); s.value = val; s.onchange = () => fn(s.value); return s; };
  bar.appendChild(el('span', 'sep', 'Snap'));
  bar.appendChild(selOf(SNAPS.map(([l, v]) => [v, l]), ROLL.snap, v => { ROLL.snap = +v; }));
  if (!drums) {
    bar.appendChild(el('span', 'sep', 'Escala'));
    bar.appendChild(selOf(NOTE_NAMES.map((n, i) => [i, n]), ROLL.root, v => { ROLL.root = +v; drawRoll(); }));
    bar.appendChild(selOf(Object.keys(SCALES).map(k => [k, SCALE_NAMES[k]]), ROLL.scale, v => { ROLL.scale = v; drawRoll(); }));
    bar.appendChild(el('span', 'sep', 'Acorde'));
    bar.appendChild(selOf(Object.keys(CHORDS).map(k => [k, CHORD_NAMES[k]]), ROLL.chord, v => { ROLL.chord = v; }));
  }
  const gl = el('label', 'sep'), gc = el('input'); gc.type = 'checkbox'; gc.checked = ROLL.ghost; gc.onchange = () => { ROLL.ghost = gc.checked; drawRoll(); };
  gl.append(gc, ' Fantasma'); bar.appendChild(gl);
  bar.appendChild(el('span', 'sep', 'Zoom'));
  const z = el('input'); z.type = 'range'; z.min = 6; z.max = 60; z.value = ROLL.stepW; z.style.width = '70px';
  z.oninput = () => { ROLL.stepW = +z.value; ROLL.scrollL = null; renderClipPane(); }; bar.appendChild(z);
  bar.appendChild(el('span', 'sep', 'Longitud'));
  [1, 2, 4, 8].forEach(b => {
    const x = el('button', c.bars === b ? 'on' : '', String(b));
    x.onclick = () => { c.bars = b; const L = b * 16; c.notes = c.notes.filter(n => n.s < L); c.notes.forEach(n => { n.l = Math.min(n.l, L - n.s); }); ROLL.sel.clear(); renderAll(); };
    bar.appendChild(x);
  });
  const clr = el('button', '', 'Vaciar'); clr.onclick = () => { c.notes = []; ROLL.sel.clear(); drawRoll(); }; bar.appendChild(clr);
  const tg = el('button', ROLL.tools ? 'on' : '', '⚙'); tg.title = 'Herramientas: cuantizar, humanizar, arpegio, strum, voltear, pan y tono por nota'; tg.onclick = () => { ROLL.tools = !ROLL.tools; renderClipPane(); }; bar.appendChild(tg);
  const mx = el('button', ROLL.big ? 'on' : '', '⤢'); mx.title = 'Maximizar / restaurar panel'; mx.onclick = () => { ROLL.big = !ROLL.big; $('#win-roll').classList.toggle('max', ROLL.big); renderClipPane(); };
  bar.appendChild(mx);
  pane.appendChild(bar);
  rollToolsRow(pane, c, t);
  if (!ROLL.tools) { const rr = pane.querySelectorAll('.rollbar'); rr[rr.length - 1].hidden = true; }

  const R = rollGeom(t), sw = ROLL.stepW, W = clipLen(c) * sw, H = (R.hi - R.lo + 1) * R.h;
  const grid = el('div', 'roll'); pane.appendChild(grid);
  grid.style.height = Math.max(130, pane.clientHeight - bar.offsetHeight * 2 - 20) + 'px';
  const cell = (cls, child) => { const d = el('div', cls); if (child) d.appendChild(child); grid.appendChild(d); return d; };
  const mk = (w, h) => { const cv = el('canvas'); cv.width = w; cv.height = h; return cv; };
  ROLL.rcv = mk(W, RULER_H); ROLL.kcv = mk(RK_W, H); ROLL.cv = mk(W, H); ROLL.vcv = mk(W, VEL_H);
  cell('corner');
  const rw = cell('rwrap', ROLL.rcv), kw = cell('kwrap', ROLL.kcv), nw = cell('nwrap', ROLL.cv);
  cell('corner vlab').textContent = 'Velocidad';
  const vw = cell('vwrap', ROLL.vcv);
  nw.onscroll = () => { rw.scrollLeft = nw.scrollLeft; vw.scrollLeft = nw.scrollLeft; kw.scrollTop = nw.scrollTop; ROLL.scrollL = nw.scrollLeft; ROLL.scrollT = nw.scrollTop; };
  if (ROLL.scrollT === null || ROLL.scrollT === undefined) ROLL.scrollT = drums ? 0 : (R.hi - 84) * R.h;
  nw.scrollTop = ROLL.scrollT; nw.scrollLeft = ROLL.scrollL || 0;
  ROLL.scrollT = null;
  bindRoll(t, c, R, nw);
  drawRoll();
}

// ----------------------------------------------------------------- entrada de ratón
function rollPt(e, R) {
  const r = ROLL.cv.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
  return { x, y, stepF: x / ROLL.stepW, n: R.hi - Math.floor(y / R.h) };
}
const snapFloor = v => ROLL.snap > 0 ? Math.floor(v / ROLL.snap + 1e-9) * ROLL.snap : v;
const snapRound = v => ROLL.snap > 0 ? Math.round(v / ROLL.snap) * ROLL.snap : v;
const noteAtPt = (c, p) => { for (let i = c.notes.length - 1; i >= 0; i--) { const n = c.notes[i]; if (n.n === p.n && p.stepF >= n.s && p.stepF < n.s + n.l) return n; } return null; };
function previewNote(n, v = 0.8) { ensureEngine(); eng.note(song.tracks[selTrack], selTrack, n, eng.ctx.currentTime, 0.25, v); }

function bindRoll(t, c, R, nw) {
  const cv = ROLL.cv, drums = t.kind === 'drums', L = clipLen(c);
  cv.oncontextmenu = e => e.preventDefault();
  ROLL.kcv.onmousedown = e => { const r = ROLL.kcv.getBoundingClientRect(); const n = R.hi - Math.floor((e.clientY - r.top) / R.h); if (n >= R.lo && n <= R.hi) previewNote(n); };
  ROLL.rcv.onmousedown = e => { const r = ROLL.rcv.getBoundingClientRect(); ROLL.cursor = snapFloor((e.clientX - r.left) / ROLL.stepW); drawRoll(); };

  cv.onmousedown = e => {
    const p = rollPt(e, R); ensureEngine();
    if (p.n < R.lo || p.n > R.hi || p.stepF < 0 || p.stepF >= L) return;
    const hit = noteAtPt(c, p);
    if (e.button === 2) { if (hit) { c.notes.splice(c.notes.indexOf(hit), 1); ROLL.sel.delete(hit); drawRoll(); } return; }
    if (ROLL.tool === 'slice') {
      if (hit) { const at = snapRound(p.stepF); if (at > hit.s + 1e-6 && at < hit.s + hit.l - 1e-6) { c.notes.push(Object.assign({}, hit, { s: at, l: hit.s + hit.l - at })); hit.l = at - hit.s; drawRoll(); } }
      return;
    }
    if (ROLL.tool === 'mute') { if (hit) { hit.m = !hit.m; drawRoll(); } return; }
    if (ROLL.tool === 'paint') {
      const sn = ROLL.snap > 0 ? ROLL.snap : 0.5; ROLL.drag = { type: 'paint', last: '', sn };
      const put = pp => { const s0 = snapFloor(pp.stepF), key = s0 + ':' + pp.n; if (key === ROLL.drag.last || pp.n < R.lo || pp.n > R.hi || s0 < 0 || s0 >= L) return; ROLL.drag.last = key;
        if (!c.notes.some(n => n.n === pp.n && Math.abs(n.s - s0) < 1e-6)) { c.notes.push({ s: s0, l: sn, n: pp.n, v: 0.8 }); previewNote(pp.n); } };
      ROLL.drag.put = put; put(p); drawRoll(); return;
    }
    if (ROLL.tool === 'del') { ROLL.drag = { type: 'del' }; if (hit) { c.notes.splice(c.notes.indexOf(hit), 1); ROLL.sel.delete(hit); drawRoll(); } return; }
    if (hit) {
      const right = (hit.s + hit.l) * ROLL.stepW, edge = !drums && right - p.x < 7 && hit.l * ROLL.stepW > 14;
      if (ROLL.tool === 'select') { if (e.shiftKey) { ROLL.sel.has(hit) ? ROLL.sel.delete(hit) : ROLL.sel.add(hit); } else if (!ROLL.sel.has(hit)) { ROLL.sel.clear(); ROLL.sel.add(hit); } }
      else { ROLL.sel.clear(); ROLL.sel.add(hit); }
      const group = ROLL.sel.has(hit) ? [...ROLL.sel] : [hit];
      ROLL.drag = { type: edge ? 'resize' : 'move', hit, group, orig: group.map(n => ({ n, s: n.s, l: n.l, p: n.n })), startF: p.stepF, startN: p.n, lastN: p.n };
      if (!edge) previewNote(hit.n, hit.v);
    } else if (ROLL.tool === 'draw') {
      const s0 = Math.max(0, Math.min(L - 0.0625, snapFloor(p.stepF))), len = drums ? 1 : Math.min(ROLL.lastLen, L - s0), made = [];
      (drums ? [0] : CHORDS[ROLL.chord]).forEach(iv => { const nn = p.n + iv; if (nn <= R.hi) { const n = { s: s0, l: len, n: nn, v: 0.8 }; c.notes.push(n); made.push(n); } });
      ROLL.sel.clear(); made.forEach(n => ROLL.sel.add(n));
      made.forEach(n => previewNote(n.n));
      ROLL.drag = { type: 'move', hit: made[0], group: made, orig: made.map(n => ({ n, s: n.s, l: n.l, p: n.n })), startF: p.stepF, startN: p.n, lastN: p.n, fresh: true };
    } else {                                            // herramienta seleccionar sobre vacío: marquesina
      if (!e.shiftKey) ROLL.sel.clear();
      ROLL.drag = { type: 'marquee', x0: p.x, y0: p.y, x1: p.x, y1: p.y };
    }
    drawRoll();
  };

  ROLL.vcv.onmousedown = e => {
    if (e.button !== 0) return;
    const r = ROLL.vcv.getBoundingClientRect(), x = e.clientX - r.left; let pick = null;
    c.notes.forEach(n => { const nx = n.s * ROLL.stepW; if (x >= nx && x <= nx + Math.max(8, n.l * ROLL.stepW)) pick = n; });
    if (!pick) return; ROLL.drag = { type: 'vel', note: pick }; velSet(e, pick);
  };
  const velSet = (e, n) => { const r = ROLL.vcv.getBoundingClientRect(); n.v = Math.max(0.03, Math.min(1, 1 - (e.clientY - r.top) / VEL_H)); drawRoll(); };

  window.onmousemove = e => {
    const d = ROLL.drag; if (!d || !ROLL.cv) return;
    if (d.type === 'vel') { velSet(e, d.note); return; }
    const p = rollPt(e, R), cl = curClip(); if (!cl) return;
    if (d.type === 'paint') { d.put(p); drawRoll(); return; }
    if (d.type === 'del') { const h = noteAtPt(cl, p); if (h) { cl.notes.splice(cl.notes.indexOf(h), 1); ROLL.sel.delete(h); drawRoll(); } return; }
    if (d.type === 'marquee') { d.x1 = p.x; d.y1 = p.y; drawRoll(); return; }
    if (d.type === 'move') {
      let dS = ROLL.snap > 0 ? snapRound(p.stepF - d.startF) : p.stepF - d.startF, dN = p.n - d.startN;
      const minS = Math.min(...d.orig.map(o => o.s)), maxE = Math.max(...d.orig.map(o => o.s + o.l)), minN = Math.min(...d.orig.map(o => o.p)), maxN = Math.max(...d.orig.map(o => o.p));
      dS = Math.max(-minS, Math.min(clipLen(cl) - maxE, dS)); dN = Math.max(R.lo - minN, Math.min(R.hi - maxN, dN));
      d.orig.forEach(o => { o.n.s = o.s + dS; o.n.n = o.p + dN; });
      if (p.n !== d.lastN) { d.lastN = p.n; previewNote(d.hit.n, d.hit.v); }
    } else if (d.type === 'resize') {
      const o0 = d.orig.find(o => o.n === d.hit), minL = ROLL.snap > 0 ? ROLL.snap : 0.0625;
      let newL = Math.max(minL, snapRound(p.stepF) - o0.s); newL = Math.min(newL, clipLen(cl) - o0.s);
      const dL = newL - o0.l;
      d.orig.forEach(o => { o.n.l = Math.max(minL, Math.min(clipLen(cl) - o.s, o.l + dL)); });
      ROLL.lastLen = d.hit.l;
    }
    drawRoll();
  };
  window.onmouseup = () => {
    const d = ROLL.drag; ROLL.drag = null; if (!d) return;
    const cl = curClip();
    if (d.type === 'marquee' && cl) {
      const x0 = Math.min(d.x0, d.x1), x1 = Math.max(d.x0, d.x1), y0 = Math.min(d.y0, d.y1), y1 = Math.max(d.y0, d.y1);
      cl.notes.forEach(n => { const nx = n.s * ROLL.stepW, nw2 = n.l * ROLL.stepW, ny = (R.hi - n.n) * R.h; if (nx < x1 && nx + nw2 > x0 && ny < y1 && ny + R.h > y0) ROLL.sel.add(n); });
    }
    if (d.type === 'move' && d.fresh && !drums) ROLL.lastLen = d.hit.l;
    drawRoll();
  };
}

// ----------------------------------------------------------------- teclado del piano roll
function rollKey(e) {
  const c = curClip(); if (!c || !winOpen('roll') || !ROLL.cv || FOCUS !== 'roll') return false;
  const k = e.key, ctrl = e.ctrlKey || e.metaKey;
  if (!ctrl && ['1', '2', '3', '4', '5', '6'].includes(k)) { ROLL.tool = TOOLS[+k - 1][0]; renderClipPane(); return true; }
  if (k === 'Delete' || k === 'Backspace') { if (!ROLL.sel.size) return false; c.notes = c.notes.filter(n => !ROLL.sel.has(n)); ROLL.sel.clear(); drawRoll(); return true; }
  if (ctrl && k.toLowerCase() === 'a') { ROLL.sel = new Set(c.notes); drawRoll(); return true; }
  if (k === 'Escape') { ROLL.sel.clear(); drawRoll(); return true; }
  if (ctrl && (k.toLowerCase() === 'b' || k.toLowerCase() === 'd')) {            // duplicar a continuación
    if (!ROLL.sel.size) return true;
    const sel = [...ROLL.sel], minS = Math.min(...sel.map(n => n.s)), maxE = Math.max(...sel.map(n => n.s + n.l)), span = maxE - minS;
    if (maxE + span > clipLen(c)) { status('No cabe: amplía la longitud del clip.'); return true; }
    const copies = sel.map(n => Object.assign({}, n, { s: n.s + span })); c.notes.push(...copies); ROLL.sel = new Set(copies); drawRoll(); return true;
  }
  if (k.startsWith('Arrow') && ROLL.sel.size) {
    const R = rollGeom(song.tracks[selTrack]), sel = [...ROLL.sel], step = ROLL.snap > 0 ? ROLL.snap : 0.25; let dS = 0, dN = 0;
    if (k === 'ArrowLeft') dS = -step; if (k === 'ArrowRight') dS = step; if (k === 'ArrowUp') dN = e.shiftKey ? 12 : 1; if (k === 'ArrowDown') dN = e.shiftKey ? -12 : -1;
    if (sel.some(n => n.s + dS < 0 || n.s + n.l + dS > clipLen(c) || n.n + dN > R.hi || n.n + dN < R.lo)) return true;
    sel.forEach(n => { n.s += dS; n.n += dN; }); if (dN) previewNote(sel[0].n); drawRoll(); return true;
  }
  return false;
}

// ----------------------------------------------------------------- dibujo
function drawRoll(playStep) {
  if (!ROLL.cv) return; const t = song.tracks[selTrack], c = curClip(); if (!c) return;
  if (playStep === undefined) playStep = ROLL.lastPlay; else ROLL.lastPlay = playStep;
  const R = rollGeom(t), sw = ROLL.stepW, L = clipLen(c), W = L * sw, H = (R.hi - R.lo + 1) * R.h, drums = t.kind === 'drums';

  // reglas
  let g = ROLL.rcv.getContext('2d'); g.fillStyle = '#26262b'; g.fillRect(0, 0, W, RULER_H); g.font = '10px sans-serif';
  for (let s = 0; s <= L; s += 4) { const x = s * sw + 0.5; g.fillStyle = s % 16 === 0 ? '#aaa' : '#555'; g.fillRect(x, s % 16 === 0 ? 2 : 10, 1, RULER_H); if (s % 16 === 0 && s < L) { g.fillStyle = '#ddd'; g.fillText(String(s / 16 + 1), x + 3, 11); } }
  if (ROLL.cursor !== undefined) { g.fillStyle = '#ffb400'; g.fillRect(ROLL.cursor * sw, 0, 2, RULER_H); }
  if (playStep >= 0) { g.fillStyle = '#4fd37a'; g.fillRect(playStep * sw, 0, 2, RULER_H); }

  // teclas
  g = ROLL.kcv.getContext('2d'); g.fillStyle = '#1b1b1e'; g.fillRect(0, 0, RK_W, H); g.font = '10px sans-serif';
  for (let n = R.hi; n >= R.lo; n--) {
    const y = (R.hi - n) * R.h;
    if (drums) { g.fillStyle = '#d9d9de'; g.fillRect(0, y, RK_W - 1, R.h - 1); g.fillStyle = '#222'; g.fillText(DRUM_NAMES[n], 4, y + R.h / 2 + 3); continue; }
    g.fillStyle = isBlackKey(n) ? '#2a2a2e' : '#e4e4e8'; g.fillRect(0, y, RK_W - 1, R.h - 1);
    if (isBlackKey(n)) { g.fillStyle = '#111'; g.fillRect(0, y, RK_W * 0.62, R.h - 1); }
    if (n % 12 === 0) { g.fillStyle = '#222'; g.fillText(noteName(n), RK_W - 26, y + R.h - 3); }
  }

  // cuadrícula + notas
  g = ROLL.cv.getContext('2d'); g.fillStyle = '#17171a'; g.fillRect(0, 0, W, H);
  for (let n = R.hi; n >= R.lo; n--) {
    const y = (R.hi - n) * R.h;
    if (drums) g.fillStyle = n % 2 ? '#1c1c20' : '#212126';
    else if (n % 12 === ROLL.root) g.fillStyle = '#34342c';
    else g.fillStyle = inScale(n) ? (isBlackKey(n) ? '#25252a' : '#2b2b31') : '#151517';
    g.fillRect(0, y, W, R.h - 1);
  }
  const fine = ROLL.snap > 0 && ROLL.snap < 1 ? ROLL.snap : 1;
  for (let s = 0; s <= L + 1e-6; s += fine) {
    const x = Math.round(s * sw) + 0.5; g.strokeStyle = s % 16 === 0 ? '#777' : s % 4 === 0 ? '#44444b' : Number.isInteger(s) ? '#2c2c32' : '#232328';
    if (fine * sw < 4 && !Number.isInteger(s)) continue; g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
  }
  if (ROLL.ghost) song.tracks.forEach((ot, oi) => {
    const oc = oi !== selTrack && ot.kind === t.kind && ot.clips[selSlot]; if (!oc) return;
    g.strokeStyle = ot.color; g.globalAlpha = 0.35;
    oc.notes.forEach(n => { if (n.s >= L || n.n > R.hi || n.n < R.lo) return; g.strokeRect(n.s * sw + 1.5, (R.hi - n.n) * R.h + 2.5, Math.max(2, n.l * sw - 3), R.h - 5); });
    g.globalAlpha = 1;
  });
  c.notes.forEach(n => {
    if (n.n > R.hi || n.n < R.lo) return; const x = n.s * sw, w = Math.max(2, n.l * sw - 1), y = (R.hi - n.n) * R.h, sel = ROLL.sel.has(n);
    g.globalAlpha = n.m ? 0.35 : 0.5 + 0.5 * (n.v === undefined ? 0.8 : n.v); g.fillStyle = n.m ? '#888' : t.color; g.fillRect(x, y + 1, w, R.h - 2); g.globalAlpha = 1;
    g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(x + w - 3, y + 1, 3, R.h - 2);
    if (sel) { g.strokeStyle = '#fff'; g.lineWidth = 1.5; g.strokeRect(x + 0.5, y + 1.5, w - 1, R.h - 3); g.lineWidth = 1; }
    if (R.h >= 14 && w > 26 && !drums) { g.fillStyle = 'rgba(0,0,0,.65)'; g.font = '9px sans-serif'; g.fillText(noteName(n.n), x + 3, y + R.h - 4); }
  });
  if (ROLL.cursor !== undefined) { g.fillStyle = 'rgba(255,180,0,.5)'; g.fillRect(ROLL.cursor * sw, 0, 1, H); }
  if (playStep >= 0) { g.fillStyle = '#4fd37a'; g.fillRect(playStep * sw, 0, 2, H); }
  const d = ROLL.drag; if (d && d.type === 'marquee') { g.strokeStyle = '#fff'; g.fillStyle = 'rgba(255,255,255,.12)'; const x = Math.min(d.x0, d.x1), y = Math.min(d.y0, d.y1), w = Math.abs(d.x1 - d.x0), h = Math.abs(d.y1 - d.y0); g.fillRect(x, y, w, h); g.strokeRect(x + 0.5, y + 0.5, w, h); }

  // velocidad
  g = ROLL.vcv.getContext('2d'); g.fillStyle = '#17171a'; g.fillRect(0, 0, W, VEL_H);
  for (let s = 0; s <= L; s += 4) { g.fillStyle = s % 16 === 0 ? '#555' : '#2a2a2f'; g.fillRect(s * sw, 0, 1, VEL_H); }
  c.notes.forEach(n => { const v = n.v === undefined ? 0.8 : n.v, x = n.s * sw, h = v * (VEL_H - 6); g.fillStyle = ROLL.sel.has(n) ? '#fff' : t.color; g.fillRect(x + 1, VEL_H - h, 3, h); g.fillRect(x + 1, VEL_H - h, 7, 3); });
}

// llamado cada fotograma: solo redibuja si cambió la posición del cabezal
function rollTick(playStep) { if (ROLL.cv && playStep !== ROLL.lastPlay) drawRoll(playStep); }
