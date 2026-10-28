'use strict';
// Vista Arrangement al estilo de Ableton: línea de tiempo con clips que se arrastran, recortan y alargan (en bucle),
// región de loop, cursor de reproducción, volcado de escenas, dividir/duplicar/borrar y envolventes de automatización
// (volumen y panorama) con puntos editables.

const ARR = { z: 6, snap: 4, sel: null, cursor: 0, follow: true, autoOpen: {}, autoParam: {}, slotSel: {}, drag: null, ph: null,
  chainOpen: {}, more: false, mode: 'slip', tool: 'draw', range: null, amode: 'read', fade: 2, silDb: -40, silMs: 150 };
const LANE_H = 62, AUTO_H = 72, HEAD_W = 150, RULER_H2 = 26, ARR_STEPS = ARR_BARS * 16;
const ARR_SNAPS = [['Compás', 16], ['Tiempo', 4], ['1/2 tiempo', 2], ['Paso', 1], ['Sin snap', 0]];

const laneOf = it => it.lane !== undefined ? it.lane : it.t;
const patLen = slot => { let L = 16; song.tracks.forEach(t => { const c = t.clips[slot]; if (c) L = Math.max(L, c.bars * 16); }); return L; };
const itemClip = it => { if (it.pat !== undefined) return { bars: patLen(it.pat) / 16, notes: [], pattern: true }; const t = song.tracks[it.t]; return t && t.clips[it.slot]; };
const itemLoopLen = it => { const c = itemClip(it); return c ? c.bars * 16 : 16; };
const aSn = () => ARR.mode === 'grid' ? (ARR.snap || 4) : ARR.snap;
const aSnapF = v => aSn() > 0 ? Math.floor(v / aSn() + 1e-9) * aSn() : v;
const aSnapR = v => aSn() > 0 ? Math.round(v / aSn()) * aSn() : v;

function resolveOverlap(it) {
  const e = it.s + it.l;
  song.arr = song.arr.filter(o => o === it || laneOf(o) !== laneOf(it) || !(o.s >= it.s && o.s + o.l <= e));
  [...song.arr].forEach(o => {
    if (o === it || laneOf(o) !== laneOf(it)) return;
    const oe = o.s + o.l, ll = itemLoopLen(o);
    if (o.s < it.s && oe > it.s) {
      if (oe > e) song.arr.push(Object.assign({}, o, { s: e, l: oe - e, off: ((o.off || 0) + (e - o.s)) % ll }));
      o.l = it.s - o.s;
    } else if (o.s >= it.s && o.s < e && oe > e) { o.off = ((o.off || 0) + (e - o.s)) % ll; o.s = e; o.l = oe - e; }
  });
}

function arrSelect(it) {
  ARR.sel = it;
  if (it) { if (it.pat !== undefined) selSlot = it.pat; else { selTrack = it.t; selSlot = it.slot; } }
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
  const cap = el('button', '', 'Colocar patrones →'); cap.title = 'Coloca todos los patrones con contenido, uno tras otro, desde el cursor en la pista seleccionada'; cap.onclick = captureScenes;
  const dup = el('button', '', 'Duplicar'); dup.title = 'Ctrl+D'; dup.onclick = arrDuplicate;
  const spl = el('button', '', 'Dividir'); spl.title = 'Ctrl+E (en el cursor)'; spl.onclick = arrSplit;
  const del = el('button', '', 'Borrar'); del.title = 'Supr'; del.onclick = arrDelete;
  const aa = el('button', '', '+ Automatización'); aa.onclick = () => { song.autos.push({ target: 't' + selTrack + '.vol', pts: [] }); renderArr(); };
  const fol = el('label', ''), fc = el('input'); fc.type = 'checkbox'; fc.checked = ARR.follow; fc.onchange = () => { ARR.follow = fc.checked; }; fol.append(fc, ' Seguir');
  const ttl = el('div', 'ptitle', 'Playlist — Arrangement'); root.insertBefore(ttl, bar);
  makePalette(bar);
  const more = el('button', ARR.more ? 'on' : '', 'Más ▾'); more.title = 'Modos, fundidos, strip silence, marcadores, punch y automatización'; more.onclick = () => { ARR.more = !ARR.more; renderArr(); };
  bar.append(el('span', 'sep', 'Snap'), sn, zl, zr, loop, more, cap, dup, spl, del, aa, fol);
  bar.title = 'Clic en una pista: colocar el patrón seleccionado · arrastra para mover · bordes: recortar · regla: cursor · Mayús+arrastrar en la regla: loop';

  arrBar2(root);
  const board = el('div', 'arrboard'); board.style.width = (HEAD_W + W) + 'px'; root.appendChild(board);
  const corner = el('div', 'arrcorner', 'Arrangement'); board.appendChild(corner);
  const rcv = el('canvas'); rcv.width = W; rcv.height = RULER_H2; rcv.className = 'arrruler'; board.appendChild(rcv);
  drawRuler(rcv); bindRuler(rcv);

  song.lanes.forEach((ln, i) => {
    const head = el('div', 'arrhead'); head.style.setProperty('--c', '#5d6a75'); head.style.height = LANE_H + 'px';
    const top = el('div', 'hrow'), nm = el('b', '', ln.name); nm.style.cursor = 'pointer'; nm.title = 'Doble clic: cambiar el nombre'; ARR.lane = ARR.lane || 0; if (ARR.lane === i) head.classList.add('sel');
    nm.title = 'Doble clic: mostrar / ocultar la cadena de plugins · clic derecho: cambiar el nombre';
    nm.onclick = () => { ARR.lane = i; renderArr(); };
    nm.ondblclick = e => { e.preventDefault(); ARR.chainOpen[i] = !ARR.chainOpen[i]; ARR.lane = i; renderArr(); };
    nm.oncontextmenu = e => { e.preventDefault(); const inp = el('input'); inp.type = 'text'; inp.value = ln.name; inp.style.width = '80px'; nm.replaceWith(inp); inp.focus(); inp.select(); inp.onblur = inp.onchange = () => { ln.name = inp.value || ln.name; renderArr(); }; };
    const chev = el('span', 'chev', ARR.chainOpen[i] ? '▾' : '▸'); chev.title = 'Cadena de plugins'; chev.onclick = nm.ondblclick;
    const led = el('button', 'led' + (ln.mute ? '' : ' on')); led.title = 'Activar / silenciar pista';
    led.onclick = () => { ln.mute = !ln.mute; led.classList.toggle('on', !ln.mute); };
    const arm = el('button', 'arm' + (ln.arm ? ' on' : ''), 'R'); arm.title = 'Armar la pista para grabar audio (luego ● en el transporte)'; arm.onclick = () => { ln.arm = !ln.arm; arm.classList.toggle('on', ln.arm); armChanged(); };
    top.append(led, chev, nm, arm); head.appendChild(top); board.appendChild(head);

    const lane = el('div', 'arrlane'); (ARR.laneEls = ARR.laneEls || [])[i] = lane; lane.style.height = LANE_H + 'px'; lane.style.width = W + 'px';
    lane.style.backgroundSize = (16 * z) + 'px 100%, ' + (4 * z) + 'px 100%';
    const place = (e, pat, extra) => {
      const r = lane.getBoundingClientRect(), slot = pat === undefined ? selSlot : pat;
      if (!song.tracks.some(t => t.clips[slot])) { status('El ' + song.patterns[slot].name + ' está vacío. Dibuja notas en el Channel Rack o el Piano roll primero.'); }
      const len = patLen(slot), s0 = Math.max(0, Math.min(ARR_STEPS - Math.min(len, ARR_STEPS), aSnapF((e.clientX - r.left) / z)));
      const it = Object.assign({ lane: i, pat: slot, s: s0, l: len, off: 0 }, extra || {}); song.arr.push(it); resolveOverlap(it); ARR.sel = it; ARR.lane = i; selSlot = slot; renderArr(); renderBottom();
    };
    lane.onmousedown = e => {
      if (ARR.tool === 'select' && (e.target === lane || e.target.classList.contains('aclip') || e.target.parentNode.classList.contains('aclip'))) {
        const r = lane.getBoundingClientRect(), a = Math.max(0, aSnapF((e.clientX - r.left) / z)), box = el('div', 'rangesel');
        lane.querySelectorAll('.rangesel').forEach(x => x.remove()); box.style.left = a * z + 'px'; box.style.width = '0px'; lane.appendChild(box);
        ARR.range = { t: i, s: a, e: a }; ARR.drag = { type: 'rangeSel', t: i, a, rect: r, box }; ARR.cursor = a; moveCursor(); return;
      }
      if (e.target !== lane) return;
      if (ARR.tool === 'draw' && e.button === 0) { const s0 = aSnapF((e.clientX - lane.getBoundingClientRect().left) / z), slot = selSlot, len = patLen(slot); place(e); if (song.tracks.some(t => t.clips[slot])) ARR.drag = { type: 'paintPat', lane: i, pat: slot, len, last: s0, rect: lane.getBoundingClientRect() }; return; }
      ARR.cursor = aSnapF((e.clientX - lane.getBoundingClientRect().left) / z); arrSelect(null); moveCursor();
    };
    lane.ondblclick = null;
    lane.ondragover = e => { e.preventDefault(); };
    lane.ondrop = e => {                                  // arrastrar desde el navegador: patrón o sample
      e.preventDefault(); const pat = e.dataTransfer.getData('xaza/pat'), smp = e.dataTransfer.getData('xaza/sample');
      if (pat !== '') place(e, +pat); else if (smp) {
        const id = smp, a2 = AUDIO[id]; if (!a2) return; const n = song.tracks.length; if (n >= 16) { status('Máximo 16 pistas.'); return; }
        const t = trackNew(a2.name, 'audio', COLORS[n % COLORS.length], null, Math.min(n, NINS - 1)); t.clips[selSlot] = audioClipFromId(id); song.tracks.push(t); rebuildEngine();
        const r = lane.getBoundingClientRect(), L = t.clips[selSlot].bars * 16, s0 = Math.max(0, aSnapF((e.clientX - r.left) / z)), it = { lane: i, t: n, slot: selSlot, s: s0, l: L, off: 0 };
        song.arr.push(it); resolveOverlap(it); renderAll(); status('Sample colocado en el Playlist (nuevo canal de audio ' + t.name + ').');
      }
    };
    song.arr.filter(it => laneOf(it) === i && itemClip(it)).forEach(it => lane.appendChild(makeClip(it, it.pat !== undefined ? null : song.tracks[it.t], z)));
    if (ARR.range && ARR.range.t === i) { const rb = el('div', 'rangesel'); rb.style.left = ARR.range.s * z + 'px'; rb.style.width = (ARR.range.e - ARR.range.s) * z + 'px'; lane.appendChild(rb); }
    board.appendChild(lane);
    if (ARR.chainOpen[i]) board.appendChild(chainPanel(i));
  });
  song.autos.forEach((au, k) => {                     // carriles de automatización de cualquier parámetro
    const T = autoInfo(au.target, song), ah = el('div', 'arrhead auto'); ah.style.height = AUTO_H + 'px'; ah.style.setProperty('--c', '#ffb400');
    const sel = el('select'); autoTargets(song).forEach(g => { const og = el('optgroup'); og.label = g.group; g.items.forEach(([id, l]) => { const o = el('option', '', l); o.value = id; og.appendChild(o); }); sel.appendChild(og); });
    sel.value = au.target; sel.onchange = () => { au.target = sel.value; renderArr(); };
    const del = el('button', '', 'Quitar'); del.onclick = () => { song.autos.splice(k, 1); renderArr(); };
    ah.append(sel, del, el('div', 'hint', T ? T.label : 'parámetro no disponible')); board.appendChild(ah);
    const cv = el('canvas'); cv.width = W; cv.height = AUTO_H; cv.className = 'autocv'; board.appendChild(cv); bindAuto(cv, au);
  });
  ARR.ph = el('div', 'phead'); ARR.ph.style.display = 'none'; board.appendChild(ARR.ph);
  ARR.cur = el('div', 'curmark'); board.appendChild(ARR.cur); moveCursor();
}
function moveCursor() { if (ARR.cur) ARR.cur.style.left = (HEAD_W + ARR.cursor * ARR.z) + 'px'; }

// ----------------------------------------------------------------- clips
function makeClip(it, t, z) {
  const c = itemClip(it), d = el('div', 'aclip' + (ARR.sel === it ? ' sel' : ''));
  const P = it.pat !== undefined ? song.patterns[it.pat] : null;
  d._it = it; if (it.m) d.classList.add('muted'); d.style.left = (it.s * z) + 'px'; d.style.width = Math.max(4, it.l * z - 1) + 'px'; d.style.background = P ? P.color : t.color;
  const cv = el('canvas'); cv.width = Math.min(4096, Math.ceil(it.l * z)); cv.height = LANE_H - 18; d.appendChild(cv);
  paintClip(cv, it, t, c, z);
  if (c.audio) { const f = el('canvas'); f.width = Math.min(4096, Math.ceil(it.l * z)); f.height = LANE_H; f.style.cssText = 'position:absolute;left:0;top:0;pointer-events:none'; d.appendChild(f); const g = f.getContext('2d'); g.fillStyle = 'rgba(0,0,0,.45)';
    const fi = (it.fi || 0) * z, fo = (it.fo || 0) * z, W = f.width, H = f.height; if (fi > 1) { g.beginPath(); g.moveTo(0, 0); g.lineTo(fi, 0); g.lineTo(0, H); g.fill(); } if (fo > 1) { g.beginPath(); g.moveTo(W, 0); g.lineTo(W - fo, 0); g.lineTo(W, H); g.fill(); } }
  d.appendChild(el('span', 'lbl', P ? P.name : (c.name || ('Clip ' + (it.slot + 1)))));
  d.onmousedown = e => {
    if (ARR.tool !== 'select') e.stopPropagation();
    if (e.button === 2 || ARR.tool === 'del') { song.arr.splice(song.arr.indexOf(it), 1); if (ARR.sel === it) ARR.sel = null; renderArr(); return; }
    if (ARR.tool === 'mute') { it.m = !it.m; renderArr(); return; }
    if (ARR.tool === 'slice') {
      const at = aSnapF(it.s + (e.clientX - d.getBoundingClientRect().left) / z), ll = itemLoopLen(it);
      if (at > it.s && at < it.s + it.l) { const first = at - it.s, tail = Object.assign({}, it, { s: at, l: it.l - first, off: ((it.off || 0) + first) % ll }); it.l = first; song.arr.push(tail); renderArr(); } return;
    }
    const x = e.offsetX, w = d.offsetWidth; let type = x > w - 7 ? 'resize' : (x < 7 ? 'trim' : 'move');
    if (ARR.tool === 'trim') type = x < w / 2 ? 'trim' : 'resize'; else if (ARR.tool === 'select') type = 'none';
    arrSelect(it); if (type === 'none') return; ARR.drag = { type, it, d, x0: e.clientX, s0: it.s, l0: it.l, off0: it.off || 0 };
  };
  d.oncontextmenu = e => e.preventDefault();
  d.ondblclick = e => { e.stopPropagation(); openWin('roll'); };
  return d;
}
function paintPattern(cv, it, z) {
  const g = cv.getContext('2d'), H = cv.height, L = patLen(it.pat), chans = song.tracks.filter(t => t.clips[it.pat] && t.kind !== 'audio'); let lo = 127, hi = 0;
  chans.forEach(t => { if (t.kind === 'drums') return; t.clips[it.pat].notes.forEach(n => { lo = Math.min(lo, n.n); hi = Math.max(hi, n.n); }); }); if (hi < lo) { lo = 36; hi = 60; } const span = Math.max(hi - lo, 12);
  for (let rep = -1; rep * L < it.l + L; rep++) {
    const base = rep * L - (it.off || 0); if (base > 0 && base < it.l) { g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(base * z, 0, 1, H); }
    g.fillStyle = 'rgba(0,0,0,.55)';
    chans.forEach(t => { const c = t.clips[it.pat]; c.notes.forEach(n => { const x = (base + n.s) * z, w = Math.max(1.5, n.l * z - 0.5); if (x + w < 0 || x > it.l * z || n.s >= c.bars * 16) return;
      const y = t.kind === 'drums' ? H - 4 - (n.n % 8) * 1.2 : (1 - (n.n - lo) / (span + 1)) * (H - 3); g.fillRect(Math.max(0, x), y, Math.min(w, it.l * z - x), 2); }); });
  }
}
function paintClip(cv, it, t, c, z) {
  if (c.pattern) { paintPattern(cv, it, z); return; }
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
  const d = ARR.drag; if (!d) return;
  if (d.type === 'paintPat') {
    const x = Math.max(0, aSnapF((e.clientX - d.rect.left) / ARR.z));
    while (x >= d.last + 2 * d.len && d.last + 2 * d.len <= ARR_STEPS) { d.last += d.len; const it = { lane: d.lane, pat: d.pat, s: d.last, l: d.len, off: 0 }; song.arr.push(it); resolveOverlap(it); d.painted = true; }
    return;
  }
  if (d.type === 'rangeSel') { const v = Math.max(0, Math.min(ARR_STEPS, aSnapR((e.clientX - d.rect.left) / ARR.z))); ARR.range = { t: d.t, s: Math.min(d.a, v), e: Math.max(d.a, v) }; d.box.style.left = ARR.range.s * ARR.z + 'px'; d.box.style.width = (ARR.range.e - ARR.range.s) * ARR.z + 'px'; return; }
  const z = ARR.z, dx = (e.clientX - d.x0) / z, it = d.it;
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
  if (d.type === 'rangeSel' || d.type === 'paintPat') { renderArr(); renderBottom(); return; }
  if (d.type === 'move' || d.type === 'resize' || d.type === 'trim') {
    if (ARR.mode === 'shuffle') shufflePack(laneOf(d.it)); else resolveOverlap(d.it); renderArr();
  }
});

// ----------------------------------------------------------------- acciones
function arrDelete() {
  if (ARR.tool === 'select' && ARR.range && ARR.range.e > ARR.range.s) { deleteRange(ARR.range.t, ARR.range.s, ARR.range.e); ARR.range = null; renderArr(); return; }
  if (!ARR.sel) return; song.arr.splice(song.arr.indexOf(ARR.sel), 1); ARR.sel = null; renderArr(); }
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
  let pos = aSnapF(ARR.cursor), n = 0; const lane = ARR.lane || 0;
  for (let s = 0; s < SCENES; s++) {
    if (!song.tracks.some(t => t.clips[s])) continue; const len = patLen(s); if (pos + len > ARR_STEPS) { status('Se llenó el Playlist.'); break; }
    const it = { lane, pat: s, s: pos, l: len, off: 0 }; song.arr.push(it); resolveOverlap(it); pos += len; n++;
  }
  status(n + ' patrón(es) colocados en el Playlist.'); renderArr();
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
  const pn = song.punch; if (pn && pn.end > pn.start) { g.fillStyle = pn.on ? 'rgba(255,80,80,.5)' : 'rgba(150,150,150,.25)'; g.fillRect(pn.start * z, 12, (pn.end - pn.start) * z, 6); }
  g.font = '10px sans-serif';
  song.markers.forEach(m => { const mx = m.s * z; g.fillStyle = '#6d8cff'; g.fillRect(mx, 0, 1, RULER_H2); g.beginPath(); g.moveTo(mx, 0); g.lineTo(mx + 7, 3); g.lineTo(mx, 6); g.fill(); g.fillStyle = '#9db4ff'; g.fillText(m.name, mx + 9, 8); });
  for (let s = 0; s <= ARR_STEPS; s += 4) {
    const x = Math.round(s * z) + 0.5; g.fillStyle = s % 16 === 0 ? '#bbb' : '#555'; g.fillRect(x, s % 16 === 0 ? 12 : 19, 1, RULER_H2);
    if (s % 16 === 0 && (z >= 4 || s % 64 === 0)) { g.fillStyle = '#ddd'; g.fillText(String(s / 16 + 1), x + 3, 22); }
  }
}
function bindRuler(cv) {
  cv.oncontextmenu = e => e.preventDefault();
  cv.onmousedown = e => {
    const r = cv.getBoundingClientRect(), z = ARR.z, x = e.clientX - r.left, lp = song.loop, v = Math.max(0, Math.min(ARR_STEPS, aSnapR(x / z)));
    const mk = song.markers.findIndex(m => Math.abs(m.s * z - x) < 6 && e.offsetY < 14);
    if (mk >= 0) { if (e.button === 2) { song.markers.splice(mk, 1); renderArr(); } else { ARR.cursor = song.markers[mk].s; moveCursor(); } return; }
    if (e.shiftKey) { ARR.drag = { type: 'loopSet', rect: r, cv, a: v }; lp.start = v; lp.end = v + 1; return; }
    if (lp.end > lp.start && Math.abs(x - lp.start * z) < 6) { ARR.drag = { type: 'loopL', rect: r, cv }; return; }
    if (lp.end > lp.start && Math.abs(x - lp.end * z) < 6) { ARR.drag = { type: 'loopR', rect: r, cv }; return; }
    ARR.cursor = aSnapF(x / z); moveCursor();
    if (eng && eng.playing && eng.mode === 'arr') { eng.play('arr', ARR.cursor); updatePlayBtn(); }
  };
}

// ----------------------------------------------------------------- automatización
function bindAuto(cv, au) {
  const z = ARR.z, W = cv.width, H = cv.height, T = () => autoInfo(au.target, song);
  const toY = v => H * (1 - v), toV = y => Math.max(0, Math.min(1, 1 - y / H));
  const paint = () => {
    const g = cv.getContext('2d'), info = T(); g.fillStyle = '#111'; g.fillRect(0, 0, W, H);
    for (let s = 0; s <= ARR_STEPS; s += 16) { g.fillStyle = '#2a2a2f'; g.fillRect(s * z, 0, 1, H); }
    if (info) { const base = autoNorm(info, info.get()); g.strokeStyle = '#3a3a40'; g.beginPath(); g.moveTo(0, toY(base)); g.lineTo(W, toY(base)); g.stroke(); }
    const e = au.pts; if (!e.length) return;
    g.strokeStyle = '#ffb400'; g.lineWidth = 2; g.beginPath(); g.moveTo(0, toY(e[0].v));
    e.forEach(p => g.lineTo(p.s * z, toY(p.v))); g.lineTo(W, toY(e[e.length - 1].v)); g.stroke(); g.lineWidth = 1;
    g.fillStyle = '#fff'; e.forEach(p => { g.beginPath(); g.arc(p.s * z, toY(p.v), 4, 0, 6.3); g.fill(); });
  };
  paint(); let drag = null;
  const pos = ev => { const r = cv.getBoundingClientRect(); return { x: ev.clientX - r.left, y: ev.clientY - r.top }; };
  const hitPt = p => au.pts.find(q => Math.hypot(q.s * z - p.x, toY(q.v) - p.y) < 7);
  cv.oncontextmenu = ev => ev.preventDefault();
  cv.onmousedown = ev => {
    const p = pos(ev), h = hitPt(p);
    if (ev.button === 2) { if (h) { au.pts = au.pts.filter(q => q !== h); paint(); } return; }
    if (h) drag = h; else { const np = { s: Math.max(0, Math.min(ARR_STEPS, aSnapR(p.x / z))), v: toV(p.y) }; au.pts = [...au.pts, np].sort((a, b) => a.s - b.s); drag = np; }
    paint();
  };
  cv.ondblclick = ev => { const h = hitPt(pos(ev)); if (h) { au.pts = au.pts.filter(q => q !== h); paint(); } };
  const mv = ev => { if (!drag) return; const p = pos(ev); drag.s = Math.max(0, Math.min(ARR_STEPS, aSnapR(p.x / z))); drag.v = toV(p.y); paint(); };
  const up = () => { if (drag) { au.pts.sort((a, b) => a.s - b.s); drag = null; paint(); } };
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

// ----------------------------------------------------------------- estilo Pro Tools: modos, herramientas, fundidos, tomas, marcadores, punch
function makePalette(bar) {
  const pal = el('div', 'tools');
  [['draw', '✎', 'Dibujar: clic en una pista coloca el patrón seleccionado; arrastra para mover; bordes para recortar'], ['del', '⌫', 'Borrar clips'], ['mute', '⊘', 'Silenciar clips'], ['slice', '✂', 'Cortar clips'], ['select', '▭', 'Seleccionar un rango (Supr lo borra)']].forEach(([k, g, tip]) => {
    const b = el('button', ARR.tool === k ? 'on' : '', g); b.title = tip; b.onclick = () => { ARR.tool = k; ARR.range = null; renderArr(); }; pal.appendChild(b); });
  bar.appendChild(pal);
}
function arrBar2(root) {
  const bar = el('div', 'arrbar arrmore'); bar.hidden = !ARR.more;
  const sel = (label, opts, val, fn) => { const l = el('label', ''); l.append(label + ' '); const s = el('select'); opts.forEach(([v, t]) => { const o = el('option', '', t); o.value = v; s.appendChild(o); }); s.value = val; s.onchange = () => fn(s.value); l.appendChild(s); bar.appendChild(l); };
  sel('Modo', [['slip', 'Slip'], ['shuffle', 'Shuffle'], ['grid', 'Grid']], ARR.mode, v => { ARR.mode = v; });

  const num = (label, val, w, fn, min = 0, max = 99) => { const l = el('label', ''); l.append(label + ' '); const i = el('input'); i.type = 'number'; i.min = min; i.max = max; i.value = val; i.style.width = w; i.onchange = () => fn(+i.value); l.appendChild(i); bar.appendChild(l); };
  num('Fundido (pasos)', ARR.fade, '46px', v => { ARR.fade = v; });
  const act = (label, tip, fn) => { const b = el('button', '', label); b.title = tip; b.onclick = fn; bar.appendChild(b); };
  const audioSel = () => { const it = ARR.sel, c = it && itemClip(it); if (!c || !c.audio) { status('Selecciona un clip de audio en el Arrangement.'); return null; } return it; };
  act('Fade in', 'Fundido de entrada del clip de audio seleccionado', () => { const it = audioSel(); if (it) { it.fi = ARR.fade; renderArr(); } });
  act('Fade out', 'Fundido de salida', () => { const it = audioSel(); if (it) { it.fo = ARR.fade; renderArr(); } });
  act('Crossfade', 'Fundido cruzado con el clip anterior pegado a este', () => {
    const it = audioSel(); if (!it) return; const prev = song.arr.find(o => o.t === it.t && o !== it && Math.abs(o.s + o.l - it.s) < 1.01);
    if (!prev) { status('No hay un clip de audio pegado antes de este.'); return; } prev.fo = ARR.fade; it.fi = ARR.fade; renderArr(); status('Crossfade aplicado.');
  });
  num('Strip: umbral dB', ARR.silDb, '52px', v => { ARR.silDb = v; }, -90, -5); num('mín. ms', ARR.silMs, '52px', v => { ARR.silMs = v; }, 20, 2000);
  act('Strip silence', 'Corta los silencios del clip de audio seleccionado', stripSilence);
  const nm = el('input'); nm.type = 'text'; nm.placeholder = 'nombre marcador'; nm.style.width = '110px'; bar.appendChild(nm);
  act('+ Marcador', 'Añade un marcador en el cursor', () => { song.markers.push({ s: ARR.cursor, name: nm.value || 'Marcador ' + (song.markers.length + 1) }); song.markers.sort((a, b) => a.s - b.s); renderArr(); });
  const mk = el('select'); const m0 = el('option', '', 'Ir a marcador…'); m0.value = ''; mk.appendChild(m0);
  song.markers.forEach((m, k) => { const o = el('option', '', m.name + ' (' + (Math.floor(m.s / 16) + 1) + ')'); o.value = k; mk.appendChild(o); });
  mk.onchange = () => { if (mk.value === '') return; ARR.cursor = song.markers[+mk.value].s; moveCursor(); const m = $('main'); m.scrollLeft = Math.max(0, HEAD_W + ARR.cursor * ARR.z - 200); mk.value = ''; };
  bar.appendChild(mk);
  const pu = el('button', song.punch.on ? 'on' : '', '● Punch'); pu.title = 'Región de punch in/out para grabar audio'; pu.onclick = () => { song.punch.on = !song.punch.on; renderArr(); }; bar.appendChild(pu);
  act('Punch = selección', 'Usa el clip o rango seleccionado como región de punch', () => {
    const r = ARR.range ? { s: ARR.range.s, e: ARR.range.e } : ARR.sel ? { s: ARR.sel.s, e: ARR.sel.s + ARR.sel.l } : null; if (!r) { status('Selecciona un clip o un rango.'); return; }
    song.punch.start = r.s; song.punch.end = r.e; song.punch.on = true; renderArr();
  });
  sel('Pre-roll', [[0, '0'], [1, '1 compás'], [2, '2 compases']], song.punch.pre, v => { song.punch.pre = +v; });
  const it = ARR.sel, c = it && itemClip(it);
  if (c && c.audio && c.takes && c.takes.length > 1) sel('Toma', c.takes.map((_, k) => [k, 'Toma ' + (k + 1)]), it.take !== undefined ? it.take : c.takes.length - 1, v => { it.take = +v; });
  sel('Automatización', [['read', 'Leer'], ['touch', 'Touch'], ['latch', 'Latch'], ['write', 'Escribir']], ARR.amode, v => { ARR.amode = v; AREC.mode = v; status(v === 'read' ? 'Automatización: solo lectura.' : 'Automatización ' + v + ': mueve controles durante la reproducción del Arrangement para grabarla.', 5000); });
  root.appendChild(bar);
}
function shufflePack(t) {
  const it = song.arr.filter(a => laneOf(a) === t).sort((a, b) => a.s - b.s); if (!it.length) return; let pos = it[0].s;
  it.forEach(o => { o.s = pos; pos += o.l; });
}
function deleteRange(t, s, e) {
  [...song.arr].forEach(o => {
    if (laneOf(o) !== t) return; const oe = o.s + o.l, ll = itemLoopLen(o);
    if (oe <= s || o.s >= e) return;
    if (o.s < s && oe > e) { song.arr.push(Object.assign({}, o, { s: e, l: oe - e, off: ((o.off || 0) + (e - o.s)) % ll })); o.l = s - o.s; }
    else if (o.s < s) o.l = s - o.s;
    else if (oe > e) { o.off = ((o.off || 0) + (e - o.s)) % ll; o.l = oe - e; o.s = e; }
    else song.arr.splice(song.arr.indexOf(o), 1);
  });
}
function stripSilence() {
  const it = ARR.sel, c = it && itemClip(it); if (!c || !c.audio) { status('Selecciona un clip de audio en el Arrangement.'); return; }
  const ca = it.take !== undefined && c.takes && c.takes[it.take] ? c.takes[it.take] : c.audio, buf = audioBuffer(ca, song.bpm); if (!buf) { status('El clip no tiene audio cargado.'); return; }
  const sr = buf.sampleRate, W = Math.round(sr * 0.01), d = buf.getChannelData(0), thr = Math.pow(10, ARR.silDb / 20), minW = Math.max(1, Math.round(ARR.silMs / 10)), sd = stepSec();
  const loud = []; for (let i = 0; i + W <= d.length; i += W) { let s2 = 0; for (let k = 0; k < W; k++) s2 += d[i + k] * d[i + k]; loud.push(Math.sqrt(s2 / W) > thr); }
  const regs = []; let a = -1, quiet = 0;
  loud.forEach((v, k) => { if (v) { if (a < 0) a = k; quiet = 0; } else if (a >= 0 && ++quiet >= minW) { regs.push([a, k - quiet + 1]); a = -1; quiet = 0; } });
  if (a >= 0) regs.push([a, loud.length]);
  const base = (it.off || 0) * sd, end = base + it.l * sd, out = [];
  regs.forEach(([x, y]) => {
    const t0 = Math.max(x * 0.01, base), t1 = Math.min(y * 0.01, end); if (t1 - t0 < sd) return;
    out.push(Object.assign({}, it, { s: it.s + Math.round((t0 - base) / sd), l: Math.max(1, Math.round((t1 - t0) / sd)), off: Math.round(t0 / sd) }));
  });
  if (!out.length) { status('No se encontró audio por encima del umbral.'); return; }
  song.arr.splice(song.arr.indexOf(it), 1, ...out); ARR.sel = out[0]; renderArr(); status('Strip silence: ' + out.length + ' fragmento(s).');
}

// ----------------------------------------------------------------- cadena de plugins desplegable (doble clic en el nombre de la pista)
function laneInserts(i) {
  const out = new Set();
  song.arr.filter(it => laneOf(it) === i).forEach(it => {
    if (it.pat !== undefined) song.tracks.forEach(t => { if (t.clips[it.pat]) out.add(t.insert); }); else if (song.tracks[it.t]) out.add(song.tracks[it.t].insert);
  });
  return out.size ? [...out].sort((a, b) => a - b) : [Math.min(i, NINS - 1)];
}
function chainPanel(i) {
  const row = el('div', 'chainrow'), box = el('div', 'chainbox'); row.appendChild(box);
  const ks = laneInserts(i), used = song.arr.some(it => laneOf(it) === i);
  box.appendChild(el('div', 'chainhead', 'Cadena de plugins — ' + song.lanes[i].name + (used ? '' : ' (sin clips: se muestra el Insert ' + (ks[0] + 1) + ')')));
  ks.forEach(k => {
    const ins = song.inserts[k], sec = el('div', 'chainsec'), t = song.tracks.filter(tt => tt.insert === k).map(tt => tt.name).join(', ');
    sec.appendChild(el('div', 'chainins', 'Insert ' + (k + 1) + ' · ' + ins.name + (t ? '   ← ' + t : '')));
    const slots = el('div', 'chainslots'); buildFxSlots(slots, k, () => { renderArr(); if (winOpen('mixer')) renderMixer(); }); sec.appendChild(slots); box.appendChild(sec);
  });
  return row;
}
