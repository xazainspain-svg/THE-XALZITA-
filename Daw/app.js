'use strict';
// Interfaz de Xaza Live: Session, Arrangement, piano roll, dispositivos, guardar/abrir, exportar.

const COLORS = ['#ff6b6b', '#ffb400', '#f4e04d', '#4fd37a', '#35c9c9', '#6d8cff', '#b784ff', '#ff7ac8'];
const SCENES = 8, STEP_W = 22, KEY_W = 46;
const $ = (s, r = document) => r.querySelector(s);
const el = (tag, cls, txt) => { const e = document.createElement(tag); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; };

// ------------------------------------------------------------------ modelo
const clipNew = (bars = 1) => ({ bars, notes: [] });
const defInst = (o = {}) => Object.assign({ wave: 'sawtooth', cutoff: 3000, res: 1, atk: 0.01, rel: 0.2 }, o);
function trackNew(name, kind, color, inst) {
  return { name, kind, color, inst: kind === 'synth' ? defInst(inst) : null, vol: 0.8, pan: 0, mute: false, solo: false,
    fx: [], clips: Array(SCENES).fill(null), auto: Array(ARR_BARS).fill(1) };
}
function fxNew(type) {
  const p = {}; FX_DEFS[type].params.forEach(d => p[d.k] = d.v); return { type, p };
}
function demoSong() {
  const T = [
    trackNew('Batería', 'drums', COLORS[0]), trackNew('Bajo', 'synth', COLORS[1], { cutoff: 700, res: 4, rel: 0.1 }),
    trackNew('Lead', 'synth', COLORS[2], { wave: 'square', cutoff: 4500, rel: 0.25 }),
    trackNew('Pad', 'synth', COLORS[3], { wave: 'triangle', atk: 0.4, rel: 0.8, cutoff: 2500 }),
    trackNew('Teclas', 'synth', COLORS[4], { wave: 'sine' }), trackNew('Percusión', 'drums', COLORS[5]),
    trackNew('Synth 2', 'synth', COLORS[6]), trackNew('Synth 3', 'synth', COLORS[7])];
  const n = (s, l, nn, v) => ({ s, l, n: nn, v: v || 0.8 });
  const d1 = clipNew(), d2 = clipNew();
  [0, 8, 10].forEach(s => d1.notes.push(n(s, 1, 0))); [4, 12].forEach(s => d1.notes.push(n(s, 1, 1)));
  for (let s = 0; s < 16; s += 2) d1.notes.push(n(s, 1, 2, s % 4 ? 0.5 : 0.8));
  [0, 6, 8, 11].forEach(s => d2.notes.push(n(s, 1, 0))); [4, 12].forEach(s => d2.notes.push(n(s, 1, 1)));
  [4, 12].forEach(s => d2.notes.push(n(s, 1, 4))); for (let s = 0; s < 16; s += 2) d2.notes.push(n(s, 1, s === 14 ? 3 : 2, 0.6));
  T[0].clips[0] = d1; T[0].clips[1] = d2;
  const b1 = clipNew(), b2 = clipNew();
  [[0, 2, 36], [3, 1, 36], [6, 2, 39], [8, 2, 34], [11, 1, 34], [12, 3, 31]].forEach(x => b1.notes.push(n(x[0], x[1], x[2])));
  [[0, 1, 36], [2, 1, 36], [4, 2, 43], [8, 1, 34], [10, 1, 34], [12, 2, 41]].forEach(x => b2.notes.push(n(x[0], x[1], x[2])));
  T[1].clips[0] = b1; T[1].clips[1] = b2;
  const l1 = clipNew(2);
  [72, 75, 79, 75, 72, 75, 79, 82, 79, 75, 72, 70, 72, 75, 79, 75].forEach((nn, i) => l1.notes.push(n(i * 2, 2, nn, 0.7)));
  [84, 82, 79, 75].forEach((nn, i) => l1.notes.push(n(16 + i * 4, 3, nn, 0.7)));
  T[2].clips[0] = l1;
  const p1 = clipNew(2);
  [[0, 60, 63, 67], [8, 58, 62, 65], [16, 56, 60, 63], [24, 58, 62, 65]].forEach(c => c.slice(1).forEach(nn => p1.notes.push(n(c[0], 8, nn, 0.6))));
  T[3].clips[0] = p1;
  T[2].fx.push(fxNew('delay')); T[3].fx.push(fxNew('reverb')); T[3].fx.push(fxNew('chorus'));
  T[0].pan = 0; T[2].pan = 0.15; T[3].vol = 0.6;
  const arr = [];
  const put = (t, from, to, slot) => { for (let b = from; b < to; b += T[t].clips[slot].bars) arr.push({ t, bar: b, slot }); };
  put(0, 0, 8, 0); put(0, 8, 16, 1); put(1, 2, 16, 0); put(3, 0, 16, 0); put(2, 4, 16, 0);
  return { bpm: 118, tracks: T, arr };
}

// ------------------------------------------------------------------ estado
let song = demoSong(), eng = null, view = 'session', selTrack = 0, selSlot = 0, bottomTab = 'clip', octave = 4, lastLen = 1, autoTrack = 0;
let cells = [], meters = [], rollCv = null, phead = null;

function ensureEngine() {
  if (!eng) {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    eng = new Engine(ctx, song);
    eng.onEnd = () => { if (eng && !eng.playing) updatePlayBtn(); };
  }
  if (eng.ctx.state === 'suspended') eng.ctx.resume();
  return eng;
}
function setSong(s) {
  const wasMetro = eng && eng.metro;
  if (eng) { eng.dispose(); const ctx = eng.ctx; song = s; eng = new Engine(ctx, song); eng.metro = wasMetro; eng.onEnd = () => updatePlayBtn(); }
  else song = s;
  selTrack = 0; selSlot = 0; $('#bpm').value = song.bpm; renderAll();
}
const status = (m, ms = 3000) => { $('#status').textContent = m; if (ms) setTimeout(() => { if ($('#status').textContent === m) $('#status').textContent = ''; }, ms); };
const curClip = () => { const t = song.tracks[selTrack]; return t && t.clips[selSlot]; };

// ------------------------------------------------------------------ transporte
function updatePlayBtn() { $('#btnPlay').classList.toggle('on', !!(eng && eng.playing)); $('#btnPlay').textContent = eng && eng.playing ? '❚❚' : '▶'; }
function togglePlay() {
  ensureEngine();
  if (eng.playing) { eng.stop(); } else if (view === 'arr') eng.play('arr'); else eng.play('session');
  updatePlayBtn();
}
$('#btnPlay').onclick = togglePlay;
$('#btnStop').onclick = () => { if (eng) eng.stop(); updatePlayBtn(); };
$('#bpm').onchange = e => { song.bpm = Math.max(40, Math.min(240, +e.target.value || 120)); e.target.value = song.bpm; };
$('#btnMetro').onclick = e => { ensureEngine(); eng.metro = !eng.metro; e.target.classList.toggle('on', eng.metro); };
function setView(v) {
  if (eng && eng.playing) { eng.stop(); updatePlayBtn(); }
  view = v; $('#session').hidden = v !== 'session'; $('#arr').hidden = v !== 'arr';
  $('#tabSession').classList.toggle('on', v === 'session'); $('#tabArr').classList.toggle('on', v === 'arr');
  renderAll();
}
$('#tabSession').onclick = () => setView('session'); $('#tabArr').onclick = () => setView('arr');
$('#tabClip').onclick = () => { bottomTab = 'clip'; renderBottom(); };
$('#tabDev').onclick = () => { bottomTab = 'dev'; renderBottom(); };

// ------------------------------------------------------------------ vista Session
function renderSession() {
  const root = $('#session'); root.innerHTML = ''; cells = []; meters = [];
  song.tracks.forEach((t, i) => {
    const h = el('div', 'th' + (i === selTrack ? ' sel' : ''), t.name); h.style.setProperty('--c', t.color);
    h.onclick = () => { selTrack = i; renderAll(); }; h.ondblclick = () => { const n = prompt('Nombre de la pista', t.name); if (n) { t.name = n; renderAll(); } };
    root.appendChild(h);
  });
  root.appendChild(el('div', 'th', 'Escenas'));
  for (let s = 0; s < SCENES; s++) {
    song.tracks.forEach((t, i) => {
      const c = t.clips[s], d = el('div', 'cell ' + (c ? 'has' : 'empty') + (i === selTrack && s === selSlot ? ' sel' : ''));
      d.style.setProperty('--c', t.color);
      d.appendChild(el('div', 'pl', c ? '▶' : '+')); if (c) d.appendChild(el('span', 'nm', c.bars + (c.bars > 1 ? ' compases' : ' compás')));
      d.onclick = () => {
        selTrack = i; selSlot = s;
        if (!t.clips[s]) { t.clips[s] = clipNew(); bottomTab = 'clip'; } else { ensureEngine(); eng.launch(i, s); updatePlayBtn(); }
        renderAll();
      };
      d.oncontextmenu = e => { e.preventDefault(); if (t.clips[s] && confirm('¿Borrar este clip?')) { t.clips[s] = null; song.arr = song.arr.filter(a => !(a.t === i && a.slot === s)); renderAll(); } };
      root.appendChild(d); cells[i * SCENES + s] = d;
    });
    const sc = el('div', 'scene'), b = el('button', '', '▶ ' + (s + 1));
    b.onclick = () => { ensureEngine(); eng.launchScene(s); selSlot = s; updatePlayBtn(); renderAll(); };
    sc.appendChild(b); root.appendChild(sc);
  }
  song.tracks.forEach((t, i) => { const b = el('button', '', '■ parar'); b.onclick = () => { if (eng) eng.stopTrack(i); }; root.appendChild(b); });
  root.appendChild(el('div'));
  song.tracks.forEach((t, i) => {
    const m = el('div', 'mix'); m.style.setProperty('--c', t.color);
    const row = el('div', 'row'), bm = el('button', 'm' + (t.mute ? ' on' : ''), 'M'), bs = el('button', 's' + (t.solo ? ' on' : ''), 'S');
    bm.onclick = () => { t.mute = !t.mute; bm.classList.toggle('on', t.mute); if (eng) eng.applyMix(); };
    bs.onclick = () => { t.solo = !t.solo; renderSession(); if (eng) eng.applyMix(); };
    row.append(bm, bs); m.appendChild(row);
    const vl = el('label', '', 'Volumen'), vol = el('input'); vol.type = 'range'; vol.min = 0; vol.max = 1.2; vol.step = 0.01; vol.value = t.vol;
    vol.oninput = () => { t.vol = +vol.value; if (eng) eng.applyMix(); };
    const pl = el('label', '', 'Panorama'), pan = el('input'); pan.type = 'range'; pan.min = -1; pan.max = 1; pan.step = 0.01; pan.value = t.pan;
    pan.oninput = () => { t.pan = +pan.value; if (eng) eng.applyMix(); };
    const cv = el('canvas'); cv.width = 100; cv.height = 10; meters[i] = cv;
    m.append(vl, vol, pl, pan, cv); root.appendChild(m);
  });
  root.appendChild(el('div'));
}

// ------------------------------------------------------------------ vista Arrangement
function renderArr() {
  const root = $('#arr'); root.innerHTML = '';
  const g = el('div', 'agrid'); root.appendChild(g);
  g.appendChild(el('div', 'ahead'));
  for (let b = 0; b < ARR_BARS; b++) g.appendChild(el('div', 'ahead', b + 1));
  song.tracks.forEach((t, i) => {
    const lab = el('div', 'alab', t.name); lab.style.setProperty('--c', t.color); g.appendChild(lab);
    for (let b = 0; b < ARR_BARS; b++) {
      const it = song.arr.find(a => a.t === i && a.bar === b), cover = song.arr.find(a => a.t === i && b > a.bar && t.clips[a.slot] && b < a.bar + t.clips[a.slot].bars);
      const c = el('div', 'acell' + (b % 4 === 0 ? ' b4' : '') + (it || cover ? ' has' : ''), it ? String(it.slot + 1) : (cover ? '·' : ''));
      c.style.setProperty('--c', t.color);
      c.onclick = () => {
        const used = t.clips.map((cl, k) => cl ? k : -1).filter(k => k >= 0), idx = song.arr.findIndex(a => a.t === i && a.bar === b);
        if (idx < 0) { if (used.length) song.arr.push({ t: i, bar: b, slot: used[0] }); }
        else { const cur = used.indexOf(song.arr[idx].slot); if (cur + 1 < used.length) song.arr[idx].slot = used[cur + 1]; else song.arr.splice(idx, 1); }
        selTrack = i; renderArr();
      };
      g.appendChild(c);
    }
  });
  phead = el('div'); phead.id = 'phead'; g.appendChild(phead);
  const ar = el('div'); ar.id = 'autoRow';
  const sel = el('select'); song.tracks.forEach((t, i) => { const o = el('option', '', t.name); o.value = i; sel.appendChild(o); }); sel.value = autoTrack;
  sel.onchange = () => { autoTrack = +sel.value; drawAuto(); };
  ar.append(el('span', '', 'Automatización de volumen — pista: '), sel, el('span', '', '  (clic y arrastra para dibujar; 1 = volumen de la pista)'));
  const cv = el('canvas'); cv.id = 'autoCv'; cv.width = ARR_BARS * 35 + 90; cv.height = 70; ar.appendChild(cv); root.appendChild(ar);
  let drag = false;
  const set = e => {
    const r = cv.getBoundingClientRect(), x = (e.clientX - r.left) * (cv.width / r.width) - 90, y = (e.clientY - r.top) * (cv.height / r.height);
    const b = Math.floor(x / 35); if (b < 0 || b >= ARR_BARS) return;
    song.tracks[autoTrack].auto[b] = Math.max(0, Math.min(1.25, 1.25 * (1 - y / cv.height))); drawAuto();
  };
  cv.onmousedown = e => { drag = true; set(e); }; cv.onmousemove = e => { if (drag) set(e); };
  window.addEventListener('mouseup', () => drag = false);
  drawAuto();
}
function drawAuto() {
  const cv = $('#autoCv'); if (!cv) return; const g = cv.getContext('2d'), a = song.tracks[autoTrack].auto, h = cv.height;
  g.fillStyle = '#111'; g.fillRect(0, 0, cv.width, h);
  g.strokeStyle = '#333'; g.beginPath(); g.moveTo(90, h * (1 - 1 / 1.25)); g.lineTo(cv.width, h * (1 - 1 / 1.25)); g.stroke();
  g.fillStyle = song.tracks[autoTrack].color;
  a.forEach((v, b) => { const y = h * (1 - v / 1.25); g.fillRect(90 + b * 35 + 1, y, 33, h - y); });
}

// ------------------------------------------------------------------ piano roll
function rollRows(t) { return t.kind === 'drums' ? { hi: 7, lo: 0, h: 22 } : { hi: 84, lo: 36, h: 13 }; }
function rowLabel(t, n) { return t.kind === 'drums' ? DRUM_NAMES[n] : (n % 12 === 0 ? noteName(n) : ''); }
const isBlack = n => [1, 3, 6, 8, 10].includes(n % 12);

function renderClipPane() {
  const pane = $('#clipPane'); pane.innerHTML = '';
  const t = song.tracks[selTrack], c = curClip();
  if (!c) { pane.appendChild(el('div', '', 'Selecciona un clip en la vista Session (o haz clic en una celda vacía para crear uno). Clic derecho en un clip: borrar.')); rollCv = null; return; }
  const bar = el('div', 'rollbar'); bar.appendChild(el('span', '', 'Longitud:'));
  [1, 2, 4].forEach(b => {
    const x = el('button', c.bars === b ? 'on' : '', b + (b > 1 ? ' compases' : ' compás'));
    x.onclick = () => { c.bars = b; c.notes = c.notes.filter(n => n.s < b * 16); c.notes.forEach(n => { n.l = Math.min(n.l, b * 16 - n.s); }); renderAll(); };
    bar.appendChild(x);
  });
  const clr = el('button', '', 'Vaciar'); clr.onclick = () => { c.notes = []; drawRoll(); }; bar.appendChild(clr);
  bar.appendChild(el('span', '', 'Clic: añadir / arrastrar el borde: alargar / clic en nota: borrar. Teclado: A-K tocan (Z/X octava).'));
  pane.appendChild(bar);
  const wrap = el('div', 'rollwrap'), cv = el('canvas'); cv.id = 'rollCv'; rollCv = cv;
  const R = rollRows(t); cv.width = KEY_W + c.bars * 16 * STEP_W; cv.height = (R.hi - R.lo + 1) * R.h;
  wrap.appendChild(cv); pane.appendChild(wrap);
  wrap.scrollTop = t.kind === 'drums' ? 0 : (84 - 72) * R.h;
  let dragNote = null;
  const pos = e => { const r = cv.getBoundingClientRect(); const x = e.clientX - r.left, y = e.clientY - r.top;
    return { x, y, step: Math.floor((x - KEY_W) / STEP_W), n: R.hi - Math.floor(y / R.h) }; };
  cv.onmousedown = e => {
    const p = pos(e); ensureEngine();
    if (p.n < R.lo || p.n > R.hi) return;
    if (p.x < KEY_W) { eng.note(t, selTrack, p.n, eng.ctx.currentTime, 0.3, 0.8); return; }
    if (p.step < 0 || p.step >= c.bars * 16) return;
    const hit = c.notes.find(n => n.n === p.n && p.step >= n.s && p.step < n.s + n.l);
    if (hit) {
      const right = KEY_W + (hit.s + hit.l) * STEP_W;
      if (right - p.x < 7 && hit.l >= 1 && t.kind !== 'drums') dragNote = hit; else c.notes.splice(c.notes.indexOf(hit), 1);
    } else {
      dragNote = { s: p.step, l: t.kind === 'drums' ? 1 : lastLen, n: p.n, v: 0.8 }; c.notes.push(dragNote);
      eng.note(t, selTrack, p.n, eng.ctx.currentTime, 0.3, 0.8);
      if (t.kind === 'drums') dragNote = null;
    }
    drawRoll();
  };
  cv.onmousemove = e => {
    if (!dragNote) return; const p = pos(e);
    dragNote.l = Math.max(1, Math.min(c.bars * 16 - dragNote.s, p.step - dragNote.s + 1)); lastLen = dragNote.l; drawRoll();
  };
  window.onmouseup = () => { dragNote = null; };
  drawRoll();
}
function drawRoll(playStep) {
  const cv = rollCv; if (!cv) return; const t = song.tracks[selTrack], c = curClip(); if (!c) return;
  const g = cv.getContext('2d'), R = rollRows(t), W = cv.width, H = cv.height;
  g.fillStyle = '#161618'; g.fillRect(0, 0, W, H);
  for (let n = R.hi; n >= R.lo; n--) {
    const y = (R.hi - n) * R.h;
    g.fillStyle = t.kind === 'drums' ? ((n % 2) ? '#1c1c1f' : '#202024') : (isBlack(n) ? '#1d1d20' : '#232327'); g.fillRect(KEY_W, y, W - KEY_W, R.h);
    g.fillStyle = t.kind !== 'drums' && isBlack(n) ? '#111' : '#d8d8dc'; g.fillRect(0, y, KEY_W - 2, R.h - 1);
    g.fillStyle = t.kind !== 'drums' && isBlack(n) ? '#aaa' : '#222'; g.font = '10px sans-serif'; g.fillText(rowLabel(t, n), 4, y + R.h - 3);
  }
  for (let s = 0; s <= c.bars * 16; s++) {
    g.strokeStyle = s % 16 === 0 ? '#666' : (s % 4 === 0 ? '#3d3d44' : '#2a2a2f'); g.beginPath();
    g.moveTo(KEY_W + s * STEP_W + 0.5, 0); g.lineTo(KEY_W + s * STEP_W + 0.5, H); g.stroke();
  }
  g.fillStyle = t.color;
  c.notes.forEach(n => { const y = (R.hi - n.n) * R.h; g.fillRect(KEY_W + n.s * STEP_W + 1, y + 1, n.l * STEP_W - 2, R.h - 2); });
  if (playStep !== undefined && playStep >= 0) { g.fillStyle = '#4fd37a'; g.fillRect(KEY_W + playStep * STEP_W, 0, 2, H); }
}

// ------------------------------------------------------------------ dispositivos
function slider(parent, label, min, max, val, step, onInput, log) {
  const row = el('div', 'prm'), lab = el('span', '', label), r = el('input'); r.type = 'range';
  if (log) { r.min = Math.log(min); r.max = Math.log(max); r.step = 0.001; r.value = Math.log(val); r.oninput = () => onInput(Math.exp(+r.value)); }
  else { r.min = min; r.max = max; r.step = step || (max - min) / 200; r.value = val; r.oninput = () => onInput(+r.value); }
  row.append(lab, r); parent.appendChild(row);
}
function renderDevPane() {
  const pane = $('#devPane'); pane.innerHTML = ''; const t = song.tracks[selTrack], wrap = el('div', 'dev'); pane.appendChild(wrap);
  const inst = el('div', 'card'); inst.appendChild(el('h4', '', t.kind === 'drums' ? 'Caja de ritmos' : 'Sintetizador'));
  if (t.kind === 'drums') inst.appendChild(el('div', '', 'Sonidos: ' + DRUM_NAMES.join(', ') + '.'));
  else {
    const row = el('div', 'prm'), ws = el('select'); ['sawtooth', 'square', 'triangle', 'sine'].forEach(w => { const o = el('option', '', w); o.value = w; ws.appendChild(o); });
    ws.value = t.inst.wave; ws.onchange = () => t.inst.wave = ws.value; row.append(el('span', '', 'Onda'), ws); inst.appendChild(row);
    slider(inst, 'Corte', 100, 12000, t.inst.cutoff, 0, v => t.inst.cutoff = v, true);
    slider(inst, 'Resonancia', 0.1, 18, t.inst.res, 0.1, v => t.inst.res = v);
    slider(inst, 'Ataque', 0.003, 1.5, t.inst.atk, 0.005, v => t.inst.atk = v);
    slider(inst, 'Release', 0.02, 3, t.inst.rel, 0.01, v => t.inst.rel = v);
  }
  wrap.appendChild(inst);
  t.fx.forEach((f, k) => {
    const def = FX_DEFS[f.type], card = el('div', 'card'), h = el('h4', '', def.name), rm = el('button', '', '✕');
    rm.onclick = () => { t.fx.splice(k, 1); if (eng) eng.rebuildTrack(selTrack); renderDevPane(); };
    h.appendChild(rm); card.appendChild(h);
    def.params.forEach(d => slider(card, d.n, d.min, d.max, f.p[d.k], d.step, v => { f.p[d.k] = v; if (eng && eng.tr[selTrack].fx[k]) eng.tr[selTrack].fx[k].update(); }, d.log));
    wrap.appendChild(card);
  });
  const add = el('div', 'card'); add.appendChild(el('h4', '', 'Añadir efecto'));
  const sel = el('select'); Object.keys(FX_DEFS).forEach(k => { const o = el('option', '', FX_DEFS[k].name); o.value = k; sel.appendChild(o); });
  const btn = el('button', '', '+ Añadir'); btn.onclick = () => { t.fx.push(fxNew(sel.value)); if (eng) eng.rebuildTrack(selTrack); renderDevPane(); };
  add.append(sel, btn); wrap.appendChild(add);
}
function renderBottom() {
  $('#tabClip').classList.toggle('on', bottomTab === 'clip'); $('#tabDev').classList.toggle('on', bottomTab === 'dev');
  $('#clipPane').hidden = bottomTab !== 'clip'; $('#devPane').hidden = bottomTab !== 'dev';
  const t = song.tracks[selTrack]; $('#selInfo').textContent = t.name + (curClip() ? ' — escena ' + (selSlot + 1) : '');
  if (bottomTab === 'clip') renderClipPane(); else renderDevPane();
}
function renderAll() { if (view === 'session') renderSession(); else renderArr(); renderBottom(); }

// ------------------------------------------------------------------ guardar / abrir / exportar
const download = (blob, name) => { const a = el('a'); a.href = URL.createObjectURL(blob); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 2000); };
$('#btnNew').onclick = () => { if (confirm('¿Empezar un proyecto vacío? Se pierde lo no guardado.')) { const s = demoSong(); s.tracks.forEach(t => { t.clips = Array(SCENES).fill(null); t.fx = []; }); s.arr = []; setSong(s); } };
$('#btnSave').onclick = () => {
  const json = JSON.stringify(song);
  try { localStorage.setItem('xazalive_song', json); } catch (e) {}
  download(new Blob([json], { type: 'application/json' }), 'proyecto.xazalive.json'); status('Proyecto guardado.');
};
$('#btnLoad').onclick = () => $('#fileIn').click();
$('#fileIn').onchange = e => {
  const f = e.target.files[0]; if (!f) return;
  f.text().then(txt => {
    const s = JSON.parse(txt);
    if (!s || !Array.isArray(s.tracks) || !s.tracks.length || !Array.isArray(s.arr)) throw new Error('Archivo no válido');
    s.tracks.forEach(t => { t.fx = t.fx || []; t.auto = t.auto || Array(ARR_BARS).fill(1); t.clips = (t.clips || []).concat(Array(SCENES).fill(null)).slice(0, SCENES); });
    setSong(s); status('Proyecto abierto.');
  }).catch(err => status('No se pudo abrir: ' + err.message, 5000));
  e.target.value = '';
};
$('#btnExport').onclick = async () => {
  try { status('Renderizando…', 0); const r = await renderWav(song); download(r.blob, 'xaza-live.wav'); status('WAV exportado (pico ' + r.peak.toFixed(2) + ').', 5000); }
  catch (e) { status(e.message, 5000); }
};

// ------------------------------------------------------------------ teclado
const KEYMAP = 'awsedftgyhujkolp';
document.addEventListener('keydown', e => {
  if (e.target.matches('input,select,textarea') && e.target.type !== 'range') return;
  if (e.code === 'Space') { e.preventDefault(); togglePlay(); return; }
  if (e.repeat || e.ctrlKey || e.metaKey) return;
  const k = e.key.toLowerCase();
  if (k === 'z') { octave = Math.max(1, octave - 1); status('Octava ' + octave); return; }
  if (k === 'x') { octave = Math.min(7, octave + 1); status('Octava ' + octave); return; }
  const i = KEYMAP.indexOf(k); if (i < 0) return;
  ensureEngine(); const t = song.tracks[selTrack];
  const n = t.kind === 'drums' ? 'asdfghjk'.indexOf(k) : (octave + 1) * 12 + i;
  if (n < 0) return; eng.note(t, selTrack, n, eng.ctx.currentTime, 0.25, 0.8);
});

// ------------------------------------------------------------------ animación (playhead, medidores, posición)
function meterDraw(cv, an) {
  const g = cv.getContext('2d'), buf = new Float32Array(an.fftSize); an.getFloatTimeDomainData(buf);
  let s = 0; for (let i = 0; i < buf.length; i++) s += buf[i] * buf[i];
  const lvl = Math.min(1, Math.sqrt(s / buf.length) * 3.2);
  g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height); g.fillStyle = lvl > 0.9 ? '#ff5050' : '#4fd37a'; g.fillRect(0, 0, cv.width * lvl, cv.height);
}
function frame() {
  if (eng) {
    const st = eng.currentStep();
    if (st >= 0 && eng.playing) $('#pos').textContent = (Math.floor(st / 16) + 1) + '.' + (Math.floor(st % 16 / 4) + 1) + '.' + (st % 4 + 1);
    else if (!eng.playing) $('#pos').textContent = '1.1.1';
    if (view === 'session') {
      song.tracks.forEach((t, i) => {
        if (meters[i]) meterDraw(meters[i], eng.tr[i].an);
        const r = eng.rt[i];
        for (let s = 0; s < SCENES; s++) { const c = cells[i * SCENES + s]; if (!c) continue;
          c.classList.toggle('playing', eng.playing && eng.mode === 'session' && r.playing === s);
          c.classList.toggle('queued', eng.playing && r.queued === s); }
      });
    } else if (phead) {
      phead.style.display = eng.playing && st >= 0 ? 'block' : 'none';
      if (st >= 0) phead.style.left = (90 + 1 + (st / 16) * 35) + 'px';
    }
    meterDraw($('#masterMeter'), eng.masterAn);
    if (bottomTab === 'clip' && rollCv) {
      const t = song.tracks[selTrack], c = curClip(), r = eng.rt[selTrack];
      if (c && eng.playing && eng.mode === 'session' && r.playing === selSlot && st >= 0) drawRoll(((st - r.launch) % (c.bars * 16) + c.bars * 16) % (c.bars * 16));
      else if (c && eng.playing && eng.mode === 'arr' && st >= 0) {
        const it = song.arr.find(a => a.t === selTrack && a.slot === selSlot && st >= a.bar * 16 && st < (a.bar + c.bars) * 16);
        drawRoll(it ? st - it.bar * 16 : undefined);
      } else drawRoll();
    }
    if (!eng.playing) updatePlayBtn();
  }
  requestAnimationFrame(frame);
}

try { const saved = localStorage.getItem('xazalive_song'); if (saved) { const s = JSON.parse(saved); if (s && s.tracks && s.arr) song = s; } } catch (e) {}
$('#bpm').value = song.bpm;
renderAll();
requestAnimationFrame(frame);
window.__daw = { get song() { return song; }, get eng() { return eng; }, renderWav, ensureEngine };
