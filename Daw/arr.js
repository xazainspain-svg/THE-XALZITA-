'use strict';
// Vista Arrangement al estilo de Ableton: línea de tiempo con clips que se arrastran, recortan y alargan (en bucle),
// región de loop, cursor de reproducción, volcado de escenas, dividir/duplicar/borrar y envolventes de automatización
// (volumen y panorama) con puntos editables.

const ARR = { z: 6, snap: 4, sel: null, cursor: 0, follow: true, autoOpen: {}, autoParam: {}, slotSel: {}, drag: null, ph: null };
const LANE_H = 62, AUTO_H = 72, HEAD_W = 150, RULER_H2 = 26, ARR_STEPS = ARR_BARS * 16;
const ARR_SNAPS = [['Compás', 16], ['Tiempo', 4], ['1/2 tiempo', 2], ['Paso', 1], ['Sin snap', 0]];

const itemClip = it => { const t = song.tracks[it.t]; return t && t.clips[it.slot]; };
const itemLoopLen = it => { const c = itemClip(it); return c ? c.bars * 16 : 16; };
const aSnapF = v => ARR.snap > 0 ? Math.floor(v / ARR.snap + 1e-9) * ARR.snap : v;
const aSnapR = v => ARR.snap > 0 ? Math.round(v / ARR.snap) * ARR.snap : v;

function resolveOverlap(it) {
  const e = it.s + it.l;
  song.arr = song.arr.filter(o => o === it || o.t !== it.t || !(o.s >= it.s && o.s + o.l <= e));
  [...song.arr].forEach(o => {
    if (o === it || o.t !== it.t) return;
    const oe = o.s + o.l, ll = itemLoopLen(o);
    if (o.s < it.s && oe > it.s) {
      if (oe > e) song.arr.push(Object.assign({}, o, { s: e, l: oe - e, off: ((o.off || 0) + (e - o.s)) % ll }));
      o.l = it.s - o.s;
    } else if (o.s >= it.s && o.s < e && oe > e) { o.off = ((o.off || 0) + (e - o.s)) % ll; o.s = e; o.l = oe - e; }
  });
}

function arrSelect(it) {
  ARR.sel = it;
  if (it) { selTrack = it.t; selSlot = it.slot; }
  document.querySelectorAll('.aclip').forEach(d => d.classList.toggle('sel', d._it === it));
  renderBottom();
}

function renderArr() {
  const root = $('#arr'); root.innerHTML = '';
  const z = ARR.z, W = ARR_STEPS * z;
  const bar = el('div', 'arrbar'); root.appendChild(bar);
  const zl = el('span', '', 'Zoom'), zr = el('input'); zr.type = 'range'; zr.min = 2; zr.max = 16; zr.step = 0.5; zr.value = z; zr.style.width = '90px';
  zr.onchange = () => { ARR.z = +zr.value; renderArr(); };
  const sn = el('select'); ARR_SNAPS.forEach(([l, v]) => { const o = el('option', '', l); o.value = v; sn.appendChild(o); }); sn.value = ARR.snap; sn.onchange = () => { ARR.snap = +sn.value; };
  const loop = el('button', song.loop.on ? 'on' : '', '⟲ Loop'); loop.onclick = () => { song.loop.on = !song.loop.on; renderArr(); };
  const cap = el('button', '', 'Volcar escenas →'); cap.title = 'Coloca las escenas de Session una tras otra desde el cursor'; cap.onclick = captureScenes;
  const dup = el('button', '', 'Duplicar'); dup.title = 'Ctrl+D'; dup.onclick = arrDuplicate;
  const spl = el('button', '', 'Dividir'); spl.title = 'Ctrl+E (en el cursor)'; spl.onclick = arrSplit;
  const del = el('button', '', 'Borrar'); del.title = 'Supr'; del.onclick = arrDelete;
  const fol = el('label', ''), fc = el('input'); fc.type = 'checkbox'; fc.checked = ARR.follow; fc.onchange = () => { ARR.follow = fc.checked; }; fol.append(fc, ' Seguir');
  bar.append(zl, zr, el('span', '', 'Snap'), sn, loop, cap, dup, spl, del, fol,
    el('span', 'hint', 'Doble clic en una pista: insertar clip · arrastra el clip para moverlo · bordes: recortar / alargar (en bucle) · clic en la regla: cursor · Mayús+arrastrar en la regla: loop'));

  const board = el('div', 'arrboard'); board.style.width = (HEAD_W + W) + 'px'; root.appendChild(board);
  const corner = el('div', 'arrcorner', 'Arrangement'); board.appendChild(corner);
  const rcv = el('canvas'); rcv.width = W; rcv.height = RULER_H2; rcv.className = 'arrruler'; board.appendChild(rcv);
  drawRuler(rcv); bindRuler(rcv);

  song.tracks.forEach((t, i) => {
    const head = el('div', 'arrhead'); head.style.setProperty('--c', t.color); head.style.height = LANE_H + 'px';
    const top = el('div', 'hrow'), nm = el('b', '', t.name); nm.style.cursor = 'pointer'; nm.onclick = () => { selTrack = i; renderBottom(); };
    const bm = el('button', 'm' + (t.mute ? ' on' : ''), 'M'), bs = el('button', 's' + (t.solo ? ' on' : ''), 'S');
    bm.onclick = () => { t.mute = !t.mute; bm.classList.toggle('on', t.mute); if (eng) eng.applyMix(); };
    bs.onclick = () => { t.solo = !t.solo; bs.classList.toggle('on', t.solo); if (eng) eng.applyMix(); };
    top.append(nm, bm, bs); head.appendChild(top);
    const row2 = el('div', 'hrow'), sl = el('select'); const slots = t.clips.map((c, k) => c ? k : -1).filter(k => k >= 0);
    slots.forEach(k => { const o = el('option', '', 'Clip ' + (k + 1)); o.value = k; sl.appendChild(o); });
    if (!slots.length) { const o = el('option', '', '(sin clips)'); o.value = -1; sl.appendChild(o); }
    sl.value = ARR.slotSel[i] !== undefined && slots.includes(ARR.slotSel[i]) ? ARR.slotSel[i] : (slots[0] !== undefined ? slots[0] : -1);
    sl.onchange = () => { ARR.slotSel[i] = +sl.value; };
    const ab = el('button', ARR.autoOpen[i] ? 'on' : '', 'Auto'); ab.title = 'Mostrar automatización'; ab.onclick = () => { ARR.autoOpen[i] = !ARR.autoOpen[i]; renderArr(); };
    row2.append(sl, ab); head.appendChild(row2); board.appendChild(head);

    const lane = el('div', 'arrlane'); lane.style.height = LANE_H + 'px'; lane.style.width = W + 'px';
    lane.style.backgroundSize = (16 * z) + 'px 100%, ' + (4 * z) + 'px 100%';
    lane.onmousedown = e => {
      if (e.target !== lane) return;
      ARR.cursor = aSnapF((e.clientX - lane.getBoundingClientRect().left) / z); arrSelect(null); moveCursor();
    };
    lane.ondblclick = e => {
      if (e.target !== lane) return;
      let slot = +sl.value;
      if (slot < 0) { slot = 0; t.clips[0] = t.clips[0] || clipNew(); status('Se creó un clip vacío en la escena 1 de ' + t.name + '.'); }
      const len = t.clips[slot].bars * 16, s0 = Math.max(0, Math.min(ARR_STEPS - len, aSnapF((e.clientX - lane.getBoundingClientRect().left) / z)));
      const it = { t: i, s: s0, l: len, slot, off: 0 }; song.arr.push(it); resolveOverlap(it); ARR.sel = it; selTrack = i; selSlot = slot; renderArr(); renderBottom();
    };
    song.arr.filter(it => it.t === i && itemClip(it)).forEach(it => lane.appendChild(makeClip(it, t, z)));
    board.appendChild(lane);

    if (ARR.autoOpen[i]) {
      const ah = el('div', 'arrhead auto'); ah.style.setProperty('--c', t.color); ah.style.height = AUTO_H + 'px';
      const ps = el('select'); [['vol', 'Volumen'], ['pan', 'Panorama']].forEach(([v, l]) => { const o = el('option', '', l); o.value = v; ps.appendChild(o); });
      ps.value = ARR.autoParam[i] || 'vol'; ps.onchange = () => { ARR.autoParam[i] = ps.value; renderArr(); };
      const clr = el('button', '', 'Borrar curva'); clr.onclick = () => { if (ps.value === 'vol') t.autoVol = []; else t.autoPan = []; renderArr(); };
      ah.append(ps, clr, el('div', 'hint', 'Clic: añadir punto · arrastrar · doble clic: borrar'));
      board.appendChild(ah);
      const cv = el('canvas'); cv.width = W; cv.height = AUTO_H; cv.className = 'autocv'; board.appendChild(cv); bindAuto(cv, i);
    }
  });
  ARR.ph = el('div', 'phead'); ARR.ph.style.display = 'none'; board.appendChild(ARR.ph);
  ARR.cur = el('div', 'curmark'); board.appendChild(ARR.cur); moveCursor();
}
function moveCursor() { if (ARR.cur) ARR.cur.style.left = (HEAD_W + ARR.cursor * ARR.z) + 'px'; }

// ----------------------------------------------------------------- clips
function makeClip(it, t, z) {
  const c = itemClip(it), d = el('div', 'aclip' + (ARR.sel === it ? ' sel' : ''));
  d._it = it; d.style.left = (it.s * z) + 'px'; d.style.width = Math.max(4, it.l * z - 1) + 'px'; d.style.background = t.color;
  const cv = el('canvas'); cv.width = Math.min(4096, Math.ceil(it.l * z)); cv.height = LANE_H - 18; d.appendChild(cv);
  paintClip(cv, it, t, c, z); d.appendChild(el('span', 'lbl', 'Clip ' + (it.slot + 1)));
  d.onmousedown = e => {
    e.stopPropagation();
    if (e.button === 2) { song.arr.splice(song.arr.indexOf(it), 1); if (ARR.sel === it) ARR.sel = null; renderArr(); return; }
    const x = e.offsetX, w = d.offsetWidth, type = x > w - 7 ? 'resize' : (x < 7 ? 'trim' : 'move');
    arrSelect(it); ARR.drag = { type, it, d, x0: e.clientX, s0: it.s, l0: it.l, off0: it.off || 0 };
  };
  d.oncontextmenu = e => e.preventDefault();
  d.ondblclick = e => { e.stopPropagation(); bottomTab = 'clip'; renderBottom(); };
  return d;
}
function paintClip(cv, it, t, c, z) {
  if (c.audio) { drawWave(cv, c.audio.id, (it.off || 0) * stepSec(), it.l * stepSec(), 'rgba(0,0,0,.6)'); return; }
  const g = cv.getContext('2d'), len = c.bars * 16; let lo = 127, hi = 0;
  c.notes.forEach(n => { lo = Math.min(lo, n.n); hi = Math.max(hi, n.n); });
  if (hi < lo) { lo = 0; hi = 1; } const span = Math.max(hi - lo, 7), H = cv.height;
  g.fillStyle = 'rgba(0,0,0,.55)';
  for (let rep = -1; rep * len < it.l + len; rep++) {
    const base = rep * len - (it.off || 0);
    if (base > 0 && base < it.l) { g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(base * z, 0, 1, H); g.fillStyle = 'rgba(0,0,0,.55)'; }
    c.notes.forEach(n => {
      const x = (base + n.s) * z, w = Math.max(1.5, n.l * z - 0.5); if (x + w < 0 || x > it.l * z) return;
      const y = (1 - (n.n - lo) / (span + 1)) * (H - 3);
      g.fillRect(Math.max(0, x), y, Math.min(w, it.l * z - x), 2.5);
    });
  }
}
window.addEventListener('mousemove', e => {
  const d = ARR.drag; if (!d) return; const z = ARR.z, dx = (e.clientX - d.x0) / z, it = d.it;
  const minL = ARR.snap > 0 ? ARR.snap : 1;
  if (d.type === 'move') { it.s = Math.max(0, Math.min(ARR_STEPS - it.l, aSnapR(d.s0 + dx))); d.d.style.left = it.s * z + 'px'; }
  else if (d.type === 'resize') { const e2 = Math.max(it.s + minL, Math.min(ARR_STEPS, aSnapR(d.s0 + d.l0 + dx))); it.l = e2 - it.s; d.d.style.width = Math.max(4, it.l * z - 1) + 'px'; }
  else if (d.type === 'trim') {
    const ns = Math.max(0, Math.min(d.s0 + d.l0 - minL, aSnapR(d.s0 + dx))), ll = itemLoopLen(it);
    it.s = ns; it.l = d.l0 - (ns - d.s0); it.off = (((d.off0 + (ns - d.s0)) % ll) + ll) % ll;
    d.d.style.left = it.s * z + 'px'; d.d.style.width = Math.max(4, it.l * z - 1) + 'px';
  } else if (d.type === 'loopSet' || d.type === 'loopL' || d.type === 'loopR') {
    const r = d.rect, v = Math.max(0, Math.min(ARR_STEPS, aSnapR((e.clientX - r.left) / z))), lp = song.loop;
    if (d.type === 'loopSet') { lp.start = Math.min(d.a, v); lp.end = Math.max(d.a, v); if (lp.end - lp.start < 1) lp.end = lp.start + 1; }
    else if (d.type === 'loopL') lp.start = Math.min(v, lp.end - 1); else lp.end = Math.max(v, lp.start + 1);
    lp.on = true; drawRuler(d.cv);
  }
});
window.addEventListener('mouseup', () => {
  const d = ARR.drag; if (!d) return; ARR.drag = null;
  if (d.type.startsWith('loop')) { renderArr(); return; }
  if (d.type === 'move' || d.type === 'resize' || d.type === 'trim') { resolveOverlap(d.it); renderArr(); }
});

// ----------------------------------------------------------------- acciones
function arrDelete() { if (!ARR.sel) return; song.arr.splice(song.arr.indexOf(ARR.sel), 1); ARR.sel = null; renderArr(); }
function arrDuplicate() {
  const it = ARR.sel; if (!it) return; if (it.s + it.l * 2 > ARR_STEPS) { status('No hay sitio a la derecha.'); return; }
  const cp = Object.assign({}, it, { s: it.s + it.l }); song.arr.push(cp); resolveOverlap(cp); ARR.sel = cp; renderArr();
}
function arrSplit() {
  const it = ARR.sel; if (!it) { status('Selecciona un clip y coloca el cursor sobre él.'); return; }
  const at = ARR.cursor; if (at <= it.s || at >= it.s + it.l) { status('El cursor debe estar dentro del clip (clic en la regla).'); return; }
  const ll = itemLoopLen(it), first = at - it.s, tail = Object.assign({}, it, { s: at, l: it.l - first, off: ((it.off || 0) + first) % ll });
  it.l = first; song.arr.push(tail); ARR.sel = tail; renderArr();
}
function captureScenes() {
  let pos = aSnapF(ARR.cursor), n = 0;
  for (let s = 0; s < SCENES; s++) {
    const cl = song.tracks.map((t, i) => t.clips[s] ? [i, t.clips[s]] : null).filter(Boolean); if (!cl.length) continue;
    const len = Math.max(...cl.map(x => x[1].bars)) * 16; if (pos + len > ARR_STEPS) { status('Se llenó el Arrangement.'); break; }
    cl.forEach(([i]) => { const it = { t: i, s: pos, l: len, slot: s, off: 0 }; song.arr.push(it); resolveOverlap(it); });
    pos += len; n++;
  }
  status(n + ' escena(s) volcadas al Arrangement.'); renderArr();
}
function arrKey(e) {
  if (view !== 'arr') return false; const k = e.key, ctrl = e.ctrlKey || e.metaKey;
  if (k === 'Delete' || k === 'Backspace') { arrDelete(); return true; }
  if (ctrl && k.toLowerCase() === 'd') { arrDuplicate(); return true; }
  if (ctrl && k.toLowerCase() === 'e') { arrSplit(); return true; }
  return false;
}

// ----------------------------------------------------------------- regla y loop
function drawRuler(cv) {
  const g = cv.getContext('2d'), z = ARR.z, W = cv.width, lp = song.loop;
  g.fillStyle = '#26262b'; g.fillRect(0, 0, W, RULER_H2);
  if (lp.end > lp.start) { g.fillStyle = lp.on ? 'rgba(255,180,0,.55)' : 'rgba(150,150,150,.3)'; g.fillRect(lp.start * z, 0, (lp.end - lp.start) * z, 8);
    g.fillStyle = lp.on ? '#ffb400' : '#888'; g.fillRect(lp.start * z, 0, 3, 12); g.fillRect(lp.end * z - 3, 0, 3, 12); }
  g.font = '10px sans-serif';
  for (let s = 0; s <= ARR_STEPS; s += 4) {
    const x = Math.round(s * z) + 0.5; g.fillStyle = s % 16 === 0 ? '#bbb' : '#555'; g.fillRect(x, s % 16 === 0 ? 12 : 19, 1, RULER_H2);
    if (s % 16 === 0 && (z >= 4 || s % 64 === 0)) { g.fillStyle = '#ddd'; g.fillText(String(s / 16 + 1), x + 3, 22); }
  }
}
function bindRuler(cv) {
  cv.onmousedown = e => {
    const r = cv.getBoundingClientRect(), z = ARR.z, x = e.clientX - r.left, lp = song.loop, v = Math.max(0, Math.min(ARR_STEPS, aSnapR(x / z)));
    if (e.shiftKey) { ARR.drag = { type: 'loopSet', rect: r, cv, a: v }; lp.start = v; lp.end = v + 1; return; }
    if (lp.end > lp.start && Math.abs(x - lp.start * z) < 6) { ARR.drag = { type: 'loopL', rect: r, cv }; return; }
    if (lp.end > lp.start && Math.abs(x - lp.end * z) < 6) { ARR.drag = { type: 'loopR', rect: r, cv }; return; }
    ARR.cursor = aSnapF(x / z); moveCursor();
    if (eng && eng.playing && eng.mode === 'arr') { eng.play('arr', ARR.cursor); updatePlayBtn(); }
  };
}

// ----------------------------------------------------------------- automatización
function bindAuto(cv, i) {
  const t = song.tracks[i], z = ARR.z, W = cv.width, H = cv.height;
  const param = () => ARR.autoParam[i] || 'vol', env = () => param() === 'vol' ? t.autoVol : t.autoPan;
  const setEnv = a => { if (param() === 'vol') t.autoVol = a; else t.autoPan = a; };
  const toV = y => param() === 'vol' ? Math.max(0, Math.min(1.25, 1.25 * (1 - y / H))) : Math.max(-1, Math.min(1, 1 - 2 * y / H));
  const toY = v => param() === 'vol' ? H * (1 - v / 1.25) : H * (1 - (v + 1) / 2);
  const paint = () => {
    const g = cv.getContext('2d'); g.fillStyle = '#111'; g.fillRect(0, 0, W, H);
    for (let s = 0; s <= ARR_STEPS; s += 16) { g.fillStyle = '#2a2a2f'; g.fillRect(s * z, 0, 1, H); }
    const base = param() === 'vol' ? 1 : t.pan; g.strokeStyle = '#3a3a40'; g.beginPath(); g.moveTo(0, toY(base)); g.lineTo(W, toY(base)); g.stroke();
    const e = env(); if (!e.length) return;
    g.strokeStyle = t.color; g.lineWidth = 2; g.beginPath(); g.moveTo(0, toY(e[0].v));
    e.forEach(p => g.lineTo(p.s * z, toY(p.v))); g.lineTo(W, toY(e[e.length - 1].v)); g.stroke(); g.lineWidth = 1;
    g.fillStyle = '#fff'; e.forEach(p => { g.beginPath(); g.arc(p.s * z, toY(p.v), 4, 0, 6.3); g.fill(); });
  };
  paint(); let drag = null;
  const pos = ev => { const r = cv.getBoundingClientRect(); return { x: ev.clientX - r.left, y: ev.clientY - r.top }; };
  const hitPt = p => env().find(q => Math.hypot(q.s * z - p.x, toY(q.v) - p.y) < 7);
  cv.oncontextmenu = ev => ev.preventDefault();
  cv.onmousedown = ev => {
    const p = pos(ev), h = hitPt(p);
    if (ev.button === 2) { if (h) { setEnv(env().filter(q => q !== h)); paint(); } return; }
    if (h) drag = h; else { const np = { s: Math.max(0, Math.min(ARR_STEPS, aSnapR(p.x / z))), v: toV(p.y) }; setEnv([...env(), np].sort((a, b) => a.s - b.s)); drag = np; }
    paint();
  };
  cv.ondblclick = ev => { const h = hitPt(pos(ev)); if (h) { setEnv(env().filter(q => q !== h)); paint(); } };
  const mv = ev => { if (!drag) return; const p = pos(ev); drag.s = Math.max(0, Math.min(ARR_STEPS, aSnapR(p.x / z))); drag.v = toV(p.y); paint(); };
  const up = () => { if (drag) { env().sort((a, b) => a.s - b.s); drag = null; paint(); } };
  window.addEventListener('mousemove', mv); window.addEventListener('mouseup', up);
}

// ----------------------------------------------------------------- reproducción
function arrTick(st) {
  if (!ARR.ph) return; const playing = eng && eng.playing && eng.mode === 'arr' && st >= 0;
  ARR.ph.style.display = playing ? 'block' : 'none';
  if (!playing) return;
  const x = HEAD_W + st * ARR.z; ARR.ph.style.left = x + 'px';
  if (ARR.follow) { const m = $('main'); if (x < m.scrollLeft + HEAD_W || x > m.scrollLeft + m.clientWidth - 60) m.scrollLeft = Math.max(0, x - HEAD_W - 100); }
}
