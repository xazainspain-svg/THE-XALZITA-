'use strict';
// Funciones de DAW completo: deshacer/rehacer, grabación MIDI en vivo, MIDI externo, pistas de audio (importar y grabar
// con micrófono), añadir/quitar pistas, tap tempo, swing, espectro, copiar/pegar clips y ayuda de atajos.

const stepSec = () => 60 / song.bpm / 4;

// ----------------------------------------------------------------- deshacer / rehacer (instantáneas del proyecto)
const UNDO = { stack: [], redo: [], pre: null };
const snap = () => JSON.stringify(song);
document.addEventListener('pointerdown', () => { UNDO.pre = snap(); }, true);
document.addEventListener('keydown', e => { if (!(e.ctrlKey || e.metaKey) || !['z', 'y'].includes(e.key.toLowerCase())) UNDO.pre = snap(); }, true);
const commit = () => setTimeout(() => {
  if (UNDO.pre === null) return; const cur = snap();
  if (cur !== UNDO.pre) { UNDO.stack.push(UNDO.pre); if (UNDO.stack.length > 100) UNDO.stack.shift(); UNDO.redo = []; }
  UNDO.pre = null; updateUndoBtns();
}, 0);
['pointerup', 'keyup', 'change'].forEach(ev => document.addEventListener(ev, commit, true));
function updateUndoBtns() { $('#btnUndo').disabled = !UNDO.stack.length; $('#btnRedo').disabled = !UNDO.redo.length; }
function undo() { if (!UNDO.stack.length) return; UNDO.redo.push(snap()); setSong(upgradeSong(JSON.parse(UNDO.stack.pop())), true); UNDO.pre = null; updateUndoBtns(); status('Deshacer'); }
function redo() { if (!UNDO.redo.length) return; UNDO.stack.push(snap()); setSong(upgradeSong(JSON.parse(UNDO.redo.pop())), true); UNDO.pre = null; updateUndoBtns(); status('Rehacer'); }
$('#btnUndo').onclick = undo; $('#btnRedo').onclick = redo; updateUndoBtns();

// ----------------------------------------------------------------- pistas: añadir / quitar
function addTrack(kind) {
  if (song.tracks.length >= 16) { status('Máximo 16 pistas.'); return; }
  const n = song.tracks.length, t = trackNew(({ audio: 'Audio ', fm: 'FM ', sampler: 'Sampler ', synth: 'Synth ', drums: 'Batería ' }[kind]) + (n + 1), kind, COLORS[n % COLORS.length], null, Math.min(n, NINS - 1));
  song.tracks.push(t); rebuildEngine(); selTrack = n; renderAll(); status('Pista añadida.'); if (kind === 'sampler') importSamplerFor(n);
}
function removeTrack(i) {
  if (song.tracks.length <= 1) { status('Debe quedar al menos una pista.'); return; }
  song.tracks.splice(i, 1); song.arr = song.arr.filter(a => a.t !== i).map(a => (a.t > i ? Object.assign({}, a, { t: a.t - 1 }) : a));
  selTrack = Math.min(selTrack, song.tracks.length - 1); ARR.sel = null; rebuildEngine(); renderAll(); status('Pista quitada.');
}
$('#btnAddM').onclick = () => addTrack('synth'); $('#btnAddA').onclick = () => addTrack('audio'); $('#btnAddF').onclick = () => addTrack('fm'); $('#btnAddS').onclick = () => addTrack('sampler');

// ----------------------------------------------------------------- audio: importar, grabar, forma de onda
const newId = () => 'a' + Math.random().toString(36).slice(2, 10);
function audioClip(buf, name) {
  const id = newId(); AUDIO[id] = { buf, name };
  return { bars: Math.min(16, Math.max(1, Math.ceil(buf.duration / (stepSec() * 16)))), notes: [], audio: { id } };
}
let pendingImport = null, pendingSampler = null;
function importAudio(i, s) { pendingImport = [i, s]; pendingSampler = null; $('#audioIn').click(); }
function importSamplerFor(i) { pendingSampler = i; pendingImport = null; $('#audioIn').click(); }
$('#audioIn').onchange = async e => {
  const f = e.target.files[0], tgt = pendingImport, smp = pendingSampler; e.target.value = ''; if (!f) return;
  try {
    ensureEngine(); const buf = await eng.ctx.decodeAudioData(await f.arrayBuffer());
    if (smp !== null) { const id = newId(); AUDIO[id] = { buf, name: f.name.replace(/\.[^.]+$/, '') }; song.tracks[smp].inst.id = id; renderAll(); status('Sample cargado: ' + AUDIO[id].name); return; }
    if (!tgt) { const id = newId(); AUDIO[id] = { buf, name: f.name.replace(/\.[^.]+$/, '') }; renderBottom(); status('Sample añadido a la biblioteca.'); return; }
    song.tracks[tgt[0]].clips[tgt[1]] = audioClip(buf, f.name.replace(/\.[^.]+$/, '')); selTrack = tgt[0]; selSlot = tgt[1]; renderAll();
    status('Audio importado: ' + f.name + ' (' + buf.duration.toFixed(1) + ' s). Suena a su velocidad original.');
  } catch (err) { status('No se pudo leer el audio: ' + err.message, 6000); }
};
let mic = null;
$('#btnMic').onclick = async () => {
  const t = song.tracks[selTrack];
  if (mic) { mic.rec.stop(); return; }
  if (t.kind !== 'audio') { status('Selecciona una pista de audio (o añade una con + Audio).'); return; }
  if (view === 'arr' && song.punch.on) { try { await recordPunch(); } catch (err) { status('Micrófono no disponible aquí: ' + err.message, 8000); } return; }
  try {
    ensureEngine(); const stream = await navigator.mediaDevices.getUserMedia({ audio: true }), rec = new MediaRecorder(stream), chunks = [], slot = selSlot, ti = selTrack;
    rec.ondataavailable = ev => chunks.push(ev.data);
    rec.onstop = async () => {
      stream.getTracks().forEach(x => x.stop()); mic = null; $('#btnMic').classList.remove('on'); $('#btnMic').textContent = '● Mic';
      try { const buf = await eng.ctx.decodeAudioData(await new Blob(chunks).arrayBuffer()); const had = song.tracks[ti].clips[slot] && song.tracks[ti].clips[slot].audio; addTake(ti, slot, buf, 'Grabación'); renderAll(); status(had ? 'Nueva toma añadida al clip.' : 'Grabación lista.'); }
      catch (err) { status('No se pudo procesar la grabación: ' + err.message, 6000); }
    };
    rec.start(); mic = { rec }; $('#btnMic').classList.add('on'); $('#btnMic').textContent = '■ Parar'; status('Grabando… pulsa ■ Parar para terminar.', 0);
  } catch (err) { status('Micrófono no disponible aquí: ' + err.message + ' (abre Daw/index.html en tu navegador y concede permiso).', 8000); }
};
function drawWave(cv, id, off, dur, color) {
  const a = AUDIO[id], g = cv.getContext('2d'), W = cv.width, H = cv.height; g.clearRect(0, 0, W, H);
  if (!a) { g.fillStyle = color; g.font = '10px sans-serif'; g.fillText('sin audio (no se guarda en el JSON)', 4, H / 2); return; }
  const d = a.buf.getChannelData(0), sr = a.buf.sampleRate; g.fillStyle = color;
  for (let x = 0; x < W; x++) {
    const i0 = Math.floor((off + x / W * dur) * sr), i1 = Math.min(d.length, Math.floor((off + (x + 1) / W * dur) * sr)); if (i0 >= d.length) break;
    let mn = 0, mx = 0; for (let i = i0; i < i1; i += Math.max(1, ((i1 - i0) / 24) | 0)) { mn = Math.min(mn, d[i]); mx = Math.max(mx, d[i]); }
    g.fillRect(x, H / 2 + mn * H / 2, 1, Math.max(1, (mx - mn) * H / 2));
  }
}
function renderAudioPane(pane, c, t) {
  const au = c.audio, a = AUDIO[au.id];
  pane.appendChild(el('div', 'rollbar', 'Clip de audio: ' + (a ? a.name + ' — ' + a.buf.duration.toFixed(2) + ' s, ' + a.buf.sampleRate + ' Hz, ' + a.buf.numberOfChannels + ' canal(es)' : 'sin audio cargado (los samples no se guardan en el proyecto)')));
  if (!a) return;
  const cv = el('canvas'); cv.width = 1100; cv.height = 130; cv.style.cssText = 'width:100%;max-width:1100px;background:#111;border-radius:3px'; pane.appendChild(cv);
  const paint = () => {
    drawWave(cv, au.id, 0, a.buf.duration, t.color); const g = cv.getContext('2d'), s = (au.start || 0) / a.buf.duration, e = (au.end || a.buf.duration) / a.buf.duration;
    g.fillStyle = 'rgba(0,0,0,.6)'; g.fillRect(0, 0, s * cv.width, cv.height); g.fillRect(e * cv.width, 0, cv.width, cv.height);
    g.fillStyle = '#ffb400'; g.fillRect(s * cv.width, 0, 2, cv.height); g.fillRect(e * cv.width - 2, 0, 2, cv.height);
  };
  const refresh = () => { c.bars = Math.min(16, Math.max(1, Math.ceil(audioBuffer(au, song.bpm).duration / (stepSec() * 16)))); paint(); };
  paint();
  const mk = (label, min, max, step, get, set) => { const l = el('label', 'sep', label + ' '), r = el('input'); r.type = 'range'; r.min = min; r.max = max; r.step = step; r.value = get(); r.style.width = '120px'; r.oninput = () => { set(+r.value); refresh(); }; l.appendChild(r); return l; };
  if (c.takes && c.takes.length > 1) { const tk = el('div', 'rollbar'); tk.appendChild(el('span', 'sep', 'Tomas:')); c.takes.forEach((tt, k) => { const b = el('button', c.audio === tt ? 'on' : '', 'Toma ' + (k + 1)); b.onclick = () => { c.audio = tt; renderClipPane(); }; tk.appendChild(b); }); pane.appendChild(tk); }
  const row = el('div', 'rollbar');
  row.append(mk('Inicio s', 0, a.buf.duration, 0.01, () => au.start || 0, v => { au.start = Math.min(v, (au.end || a.buf.duration) - 0.02); }),
    mk('Fin s', 0, a.buf.duration, 0.01, () => au.end || a.buf.duration, v => { au.end = Math.max(v, (au.start || 0) + 0.02); }),
    mk('Ganancia', 0, 2, 0.01, () => au.gain === undefined ? 1 : au.gain, v => { au.gain = v; }),
    mk('Fade in s', 0, 2, 0.01, () => au.fi === undefined ? 0.003 : au.fi, v => { au.fi = v; }), mk('Fade out s', 0, 2, 0.01, () => au.fo === undefined ? 0.003 : au.fo, v => { au.fo = v; }));
  const rv = el('label', 'sep'), rc = el('input'); rc.type = 'checkbox'; rc.checked = !!au.rev; rc.onchange = () => { au.rev = rc.checked; refresh(); }; rv.append(rc, ' Invertir');
  const wp = el('label', 'sep'), wc = el('input'); wc.type = 'checkbox'; wc.checked = !!au.warp; wc.onchange = () => { au.warp = wc.checked; au.ob = au.ob || song.bpm; refresh(); status(au.warp ? 'Estirado al tempo del proyecto (calidad granular básica).' : 'Sin estirar.'); }; wp.append(wc, ' Estirar al tempo · BPM original ');
  const ob = el('input'); ob.type = 'number'; ob.min = 40; ob.max = 240; ob.value = au.ob || song.bpm; ob.style.width = '56px'; ob.onchange = () => { au.ob = +ob.value || song.bpm; refresh(); }; wp.appendChild(ob);
  const nz = el('button', '', 'Normalizar'); nz.onclick = () => { let pk = 0; for (let ch = 0; ch < a.buf.numberOfChannels; ch++) { const d = a.buf.getChannelData(ch); for (let i = 0; i < d.length; i++) pk = Math.max(pk, Math.abs(d[i])); } au.gain = pk > 0 ? Math.min(4, 0.95 / pk) : 1; renderClipPane(); };
  const rp = el('button', '', 'Reemplazar…'); rp.onclick = () => importAudio(selTrack, selSlot);
  const dl = el('button', '', 'Quitar audio'); dl.onclick = () => { t.clips[selSlot] = null; renderAll(); };
  const nm = el('input'); nm.type = 'text'; nm.placeholder = 'nombre'; nm.value = c.name || ''; nm.style.width = '110px'; nm.oninput = () => { c.name = nm.value; };
  row.append(rv, wp, nz, rp, dl, nm); pane.appendChild(row);
}

// ----------------------------------------------------------------- grabación MIDI en vivo + MIDI externo
const REC = { on: false, held: {} };
$('#btnRec').onclick = () => {
  if (AUD.active || song.lanes.some(l => l.arm)) { toggleAudioRec(); return; }
  REC.on = !REC.on; $('#btnRec').classList.toggle('on', REC.on);
  if (!REC.on) { status('Grabación detenida.'); return; }
  const t = song.tracks[selTrack]; if (t.kind === 'audio') { REC.on = false; $('#btnRec').classList.remove('on'); status('Para grabar notas selecciona una pista MIDI.'); return; }
  if (!t.clips[selSlot]) t.clips[selSlot] = clipNew(); ensureEngine(); eng.launch(selTrack, selSlot); updatePlayBtn(); renderAll(); openWin('roll');
  status('Grabando: toca con el teclado (A–K) o un teclado MIDI. Las notas se cuantizan a 1/2 paso.', 6000);
};
function recPos(c) {
  const L = c.bars * 16, p = eng.posFloat(); if (p < 0) return -1; const r = eng.rt[selTrack];
  return (((eng.mode === 'session' && r.playing === selSlot ? p - r.launch : p) % L) + L) % L;
}
function liveOn(n, v) {
  const t = song.tracks[selTrack]; if (!t || t.kind === 'audio') return;
  ensureEngine(); eng.note(t, selTrack, n, eng.ctx.currentTime, 0.25, v);
  const c = curClip(); if (!REC.on || !eng.playing || !c) return;
  const p = recPos(c); if (p < 0) return;
  const note = { s: Math.min(c.bars * 16 - 0.5, Math.round(p * 2) / 2), l: 0.5, n, v: Math.max(0.2, v) }; c.notes.push(note); REC.held[n] = { note, t0: p };
  if (ROLL.cv) drawRoll();
}
function liveOff(n) {
  const h = REC.held[n]; if (!h) return; delete REC.held[n]; const c = curClip(); if (!c) return;
  const p = recPos(c); let len = p - h.t0; if (len < 0) len += c.bars * 16;
  h.note.l = Math.max(0.5, Math.min(c.bars * 16 - h.note.s, Math.round(len * 2) / 2)); if (ROLL.cv) drawRoll();
}
try {
  if (navigator.requestMIDIAccess) navigator.requestMIDIAccess().then(acc => {
    let k = 0; acc.inputs.forEach(inp => { k++; inp.onmidimessage = m => {
      const [st, n, vel] = m.data, cmd = st & 0xf0;
      if (cmd === 0x90 && vel > 0) liveOn(n, vel / 127); else if (cmd === 0x80 || (cmd === 0x90 && vel === 0)) liveOff(n); }; });
    if (k) status('MIDI: ' + k + ' dispositivo(s) conectado(s).');
  }).catch(() => {});
} catch (e) {}

// ----------------------------------------------------------------- tempo: tap y swing
const taps = [];
$('#btnTap').onclick = () => {
  const now = performance.now(); if (taps.length && now - taps[taps.length - 1] > 2000) taps.length = 0; taps.push(now); if (taps.length > 5) taps.shift();
  if (taps.length >= 2) { const iv = (taps[taps.length - 1] - taps[0]) / (taps.length - 1); song.bpm = Math.max(40, Math.min(240, Math.round(60000 / iv))); $('#bpm').value = song.bpm; }
};
$('#swing').oninput = e => { song.swing = +e.target.value; };

// ----------------------------------------------------------------- espectro del master
function extrasTick() {
  if (!eng) return; const cv = $('#spec'), g = cv.getContext('2d'), an = eng.spec, b = new Uint8Array(an.frequencyBinCount); an.getByteFrequencyData(b);
  g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height); const bars = 28;
  for (let k = 0; k < bars; k++) {
    const lo = Math.floor(Math.pow(b.length, k / bars)), hi = Math.max(lo + 1, Math.floor(Math.pow(b.length, (k + 1) / bars))); let m = 0;
    for (let i = lo; i < hi && i < b.length; i++) m = Math.max(m, b[i]);
    const h = m / 255 * cv.height; g.fillStyle = k > 22 ? '#6d8cff' : k > 8 ? '#4fd37a' : '#ffb400'; g.fillRect(k * 4, cv.height - h, 3, h);
  }
}

// ----------------------------------------------------------------- copiar / pegar clips, atajos, ayuda
let CLIPBOARD = null;
function extrasKey(e) {
  const ctrl = e.ctrlKey || e.metaKey, k = e.key.toLowerCase();
  if (ctrl && k === 'z') { e.shiftKey ? redo() : undo(); return true; }
  if (ctrl && k === 'y') { redo(); return true; }
  if (view === 'session' && ctrl && k === 'c') { const c = curClip(); if (c) { CLIPBOARD = { kind: song.tracks[selTrack].kind === 'audio', clip: JSON.parse(JSON.stringify(c)) }; status('Clip copiado.'); } return true; }
  if (view === 'session' && ctrl && k === 'v') {
    const t = song.tracks[selTrack]; if (!CLIPBOARD) return true;
    if ((t.kind === 'audio') !== CLIPBOARD.kind) { status('No se puede pegar entre pista de audio y MIDI.'); return true; }
    t.clips[selSlot] = JSON.parse(JSON.stringify(CLIPBOARD.clip)); renderAll(); status('Clip pegado.'); return true;
  }
  return false;
}
const HELP = [['Espacio', 'Reproducir / parar'], ['A–K, W E T Y U', 'Tocar notas (Z / X cambian de octava)'], ['● Rec', 'Grabar notas en vivo en el clip seleccionado'],
  ['Ctrl+Z / Ctrl+Y', 'Deshacer / rehacer'], ['Ctrl+C / Ctrl+V', 'Copiar / pegar clip (Session)'], ['1–5', 'Herramientas del piano roll'],
  ['Ctrl+A, Supr, Ctrl+D, flechas', 'Seleccionar, borrar, duplicar, mover notas'], ['Ctrl+D / Ctrl+E / Supr', 'Arrangement: duplicar / dividir / borrar clip'],
  ['Clic derecho', 'Borrar clip o nota'], ['Mayús + arrastrar (regla)', 'Región de loop'], ['Tap', 'Marcar el tempo con clics'], ['+ MIDI / Audio / FM / Sampler', 'Añadir pistas (máx. 16)'], ['Channel Rack', 'Step sequencer: clic y arrastra para pintar pasos, Mayús+clic = acento'], ['MIDI ↓ / ↑ , Stems', 'Exportar/importar MIDI y un WAV por pista'], ['Arrangement: Modo y Herramienta', 'Slip / Shuffle / Grid; Grabber / Trim / Selector (rango + Supr)'], ['Fade in / out / Crossfade', 'Fundidos del clip de audio seleccionado'], ['● Punch + ● Mic', 'Grabar audio entre punch in y out con pre-roll; crea tomas para comping'], ['Automatización: Touch / Latch / Escribir', 'Mueve un control durante la reproducción del Arrangement para grabarla']];
$('#btnHelp').onclick = () => {
  const h = $('#help'); h.hidden = !h.hidden; if (h.hidden) return; h.innerHTML = '';
  const box = el('div', 'helpbox'); box.appendChild(el('h3', '', 'Atajos y funciones'));
  HELP.forEach(([k, d]) => { const r = el('div', 'hrow2'); r.append(el('kbd', '', k), el('span', '', d)); box.appendChild(r); });
  const x = el('button', '', 'Cerrar'); x.onclick = () => { h.hidden = true; }; box.appendChild(x); h.appendChild(box);
};

// ----------------------------------------------------------------- grabación de automatización (Touch / Latch / Escribir)
const AREC = { mode: 'read', act: new Map() };
function autoRec(target, value) {
  if (AREC.mode === 'read' || !eng || !eng.playing || eng.mode !== 'arr') return;
  const T = autoInfo(target, song); if (!T) return; const st = Math.round(eng.currentStep()); if (st < 0) return;
  let a = AREC.act.get(target);
  if (!a) {
    let lane = song.autos.find(x => x.target === target); if (!lane) { lane = { target, pts: [] }; song.autos.push(lane); }
    a = { lane, start: st, backup: lane.pts.slice(), pts: [], last: -1, end: st }; AREC.act.set(target, a);
  }
  const v = autoNorm(T, value);
  if (a.last === st && a.pts.length) a.pts[a.pts.length - 1].v = v; else { a.pts.push({ s: st, v }); a.last = st; }
  a.end = st; a.lane.pts = a.backup.filter(p => p.s < a.start).concat(a.pts);
}
function autoRecEnd() {
  AREC.act.forEach(a => {
    const tail = AREC.mode === 'touch' ? a.backup.filter(p => p.s > a.end) : [];     // Touch vuelve a la curva anterior al soltar
    a.lane.pts = a.backup.filter(p => p.s < a.start).concat(a.pts, tail).sort((x, y) => x.s - y.s);
  });
  const n = AREC.act.size; AREC.act.clear(); if (n && view === 'arr') renderArr();
}
document.addEventListener('pointerup', () => { if (AREC.mode === 'touch' && AREC.act.size) autoRecEnd(); }, true);

// ----------------------------------------------------------------- tomas (comping) y punch in/out
function addTake(ti, slot, buf, name) {
  const t = song.tracks[ti], c = t.clips[slot], id = newId(); AUDIO[id] = { buf, name };
  if (c && c.audio) { c.takes = c.takes || [c.audio]; const au = { id }; c.takes.push(au); c.audio = au; c.bars = Math.max(c.bars, Math.min(16, Math.ceil(buf.duration / (stepSec() * 16)))); return { clip: c, take: c.takes.length - 1 }; }
  t.clips[slot] = { bars: Math.min(16, Math.max(1, Math.ceil(buf.duration / (stepSec() * 16)))), notes: [], audio: { id } }; return { clip: t.clips[slot], take: undefined };
}
async function recordPunch() {
  const ti = selTrack, pn = song.punch, t = song.tracks[ti];
  ensureEngine();
  const stream = await navigator.mediaDevices.getUserMedia({ audio: true }), rec = new MediaRecorder(stream), chunks = [];
  rec.ondataavailable = ev => chunks.push(ev.data);
  rec.onstop = async () => {
    stream.getTracks().forEach(x => x.stop()); clearInterval(mic && mic.poll); mic = null; $('#btnMic').classList.remove('on'); $('#btnMic').textContent = '● Mic'; if (eng) { eng.stop(); updatePlayBtn(); }
    try {
      const buf = await eng.ctx.decodeAudioData(await new Blob(chunks).arrayBuffer());
      const it = ARR.sel && ARR.sel.t === ti && itemClip(ARR.sel) && itemClip(ARR.sel).audio ? ARR.sel : null;
      if (it) { const r = addTake(ti, it.slot, buf, 'Toma'); it.take = r.take; status('Toma ' + (r.take + 1) + ' añadida al clip. Elígela en el selector "Toma".'); }
      else {
        let slot = t.clips.findIndex(c => !c); if (slot < 0) slot = SCENES - 1; const r = addTake(ti, slot, buf, 'Grabación');
        song.arr.push({ t: ti, s: pn.start, l: Math.max(1, pn.end - pn.start), slot, off: 0 }); ARR.sel = song.arr[song.arr.length - 1]; status('Grabación colocada en la región de punch.');
      }
      renderAll();
    } catch (err) { status('No se pudo procesar la grabación: ' + err.message, 6000); }
  };
  const start = Math.max(0, pn.start - pn.pre * 16); eng.play('arr', start); updatePlayBtn(); let on = false;
  const poll = setInterval(() => {
    const p = eng.posFloat(); if (p < 0) return;
    if (!on && p >= pn.start) { on = true; rec.start(); }
    if (on && (p >= pn.end || !eng.playing)) { clearInterval(poll); if (rec.state !== 'inactive') rec.stop(); }
  }, 10);
  mic = { rec, poll }; $('#btnMic').classList.add('on'); $('#btnMic').textContent = '■ Parar'; status('Pre-roll… la grabación empieza en el punch in y acaba en el punch out.', 6000);
}

// ----------------------------------------------------------------- grabación de audio estilo Pro Tools
// Pistas armadas (R), monitorización con medidor de entrada, cuenta atrás con metrónomo, pre-roll, punch, QuickPunch
// (REC con el transporte en marcha), grabación en bucle (una toma por vuelta) y compensación de latencia.
const AUD = { stream: null, src: null, an: null, mon: null, rec: null, active: false, poll: null, startPos: 0, lane: 0, busy: false };
async function listInputs() {
  try {
    const devs = (await navigator.mediaDevices.enumerateDevices()).filter(d => d.kind === 'audioinput'), sel = $('#inSel'), cur = sel.value;
    sel.innerHTML = '<option value="">Predeterminada</option>'; devs.forEach(d => { const o = el('option', '', d.label || 'Entrada ' + (sel.length)); o.value = d.deviceId; sel.appendChild(o); }); sel.value = cur;
  } catch (err) {}
}
async function ensureInput() {
  if (AUD.stream) return true;
  try {
    ensureEngine(); const id = $('#inSel').value;
    AUD.stream = await navigator.mediaDevices.getUserMedia({ audio: { deviceId: id ? { exact: id } : undefined, echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    const c = eng.ctx; AUD.src = c.createMediaStreamSource(AUD.stream); AUD.an = c.createAnalyser(); AUD.an.fftSize = 512; AUD.src.connect(AUD.an);
    AUD.mon = c.createGain(); AUD.mon.gain.value = $('#monCb').checked ? 1 : 0; AUD.src.connect(AUD.mon); AUD.mon.connect(c.destination); listInputs(); return true;
  } catch (err) { status('Entrada de audio no disponible aquí: ' + err.message + ' (abre Daw/index.html en tu navegador y concede permiso).', 8000); return false; }
}
function closeInput() { if (!AUD.stream || AUD.active) return; AUD.stream.getTracks().forEach(t => t.stop()); try { AUD.src.disconnect(); AUD.mon.disconnect(); } catch (e) {} AUD.stream = AUD.src = AUD.an = AUD.mon = null; }
async function armChanged() {
  if (song.lanes.some(l => l.arm)) { if (!(await ensureInput())) song.lanes.forEach(l => { l.arm = false; }); } else closeInput();
  if (view === 'arr') renderArr(); $('#btnRec').classList.toggle('armed', song.lanes.some(l => l.arm));
}
$('#monCb').onchange = () => { if (AUD.mon) AUD.mon.gain.value = $('#monCb').checked ? 1 : 0; };
$('#inSel').onchange = async () => { if (AUD.stream && !AUD.active) { closeInput(); await ensureInput(); } };

function toggleAudioRec() { AUD.active ? stopAudioRec() : startAudioRec(); }
async function startAudioRec() {
  if (AUD.busy) return; AUD.busy = true;
  try {
    const lane = song.lanes.findIndex(l => l.arm); if (lane < 0 || !(await ensureInput())) return;
    ensureEngine(); const sd = stepSec(), count = +$('#countSel').value, pn = song.punch, lp = song.loop; AUD.lane = lane; AUD.chunks = [];
    let start, quick = false;
    if (eng.playing && eng.mode === 'arr') { start = Math.round(eng.posFloat()); quick = true; }            // QuickPunch
    else if (lp.on) start = lp.start; else if (pn.on) start = pn.start; else start = ARR.cursor;
    AUD.loopLen = lp.on && !quick ? lp.end - lp.start : 0; AUD.end = pn.on && !quick && !lp.on ? pn.end : Infinity; AUD.startPos = start;
    const rec = new MediaRecorder(AUD.stream); AUD.rec = rec; rec.ondataavailable = ev => AUD.chunks.push(ev.data); rec.onstop = finishAudioRec;
    let metro0 = eng.metro, began = quick;
    if (quick) { rec.start(); AUD.recStart = start; }
    else {
      const pre = Math.max(count, pn.on ? pn.pre : 0) * 16, from = Math.max(0, start - pre); if (count > 0) eng.metro = true;
      eng.play('arr', from); if (count === 0 && from === start) { began = true; rec.start(); AUD.recStart = start; eng.metro = metro0; }
    }
    AUD.active = true; AUD.metro0 = metro0; $('#btnRec').classList.add('on'); updatePlayBtn();
    AUD.poll = setInterval(() => {
      if (!eng.playing && AUD.active && began) { stopAudioRec(); return; }
      const p = eng.posFloat(); if (p < 0) return;
      if (!began && p >= start) { began = true; rec.start(); AUD.recStart = p; eng.metro = AUD.metro0; }
      if (began && p >= AUD.end) stopAudioRec();
    }, 10);
    status(quick ? 'QuickPunch: grabando… pulsa ● para terminar.' : (count ? 'Cuenta atrás…' : 'Grabando…') + ' Pulsa ● o ■ para terminar.', 5000);
  } finally { AUD.busy = false; }
}
function stopAudioRec() {
  if (!AUD.active) return; AUD.active = false; clearInterval(AUD.poll); if (eng) { eng.metro = AUD.metro0; eng.stop(); updatePlayBtn(); }
  $('#btnRec').classList.remove('on'); if (AUD.rec && AUD.rec.state !== 'inactive') AUD.rec.stop();
}
async function finishAudioRec() {
  try {
    const raw = await eng.ctx.decodeAudioData(await new Blob(AUD.chunks).arrayBuffer()), sr = raw.sampleRate, sd = stepSec(), lat = Math.max(0, +$('#latMs').value || 0) / 1000;
    const skip = Math.min(raw.length - 1, Math.round(lat * sr)), full = sliceBuf(raw, skip / sr, raw.duration, false);
    const ln = song.lanes[AUD.lane]; let ti = ln.ch; if (ti === undefined || !song.tracks[ti] || song.tracks[ti].kind !== 'audio') {
      const n = song.tracks.length; if (n >= 16) { status('Máximo 16 pistas.'); return; } song.tracks.push(trackNew(ln.name, 'audio', COLORS[n % COLORS.length], null, Math.min(n, NINS - 1))); ln.ch = n; ti = n; rebuildEngine();
    }
    const t = song.tracks[ti]; let slot = t.clips.findIndex(c => !c); if (slot < 0) slot = SCENES - 1;
    const mkId = b => { const id = newId(); AUDIO[id] = { buf: b, name: ln.name }; return id; }, bars = b => Math.min(16, Math.max(1, Math.ceil(b.duration / (sd * 16))));
    let clip, item, s0 = Math.max(0, Math.round(AUD.recStart || AUD.startPos));
    if (AUD.loopLen > 0) {                                       // grabación en bucle: una toma por vuelta
      const per = AUD.loopLen * sd, n = Math.max(1, Math.floor(full.duration / per + 0.2)), takes = [];
      for (let k = 0; k < n; k++) { const part = sliceBuf(full, k * per, Math.min(full.duration, (k + 1) * per), false); if (part.duration > 0.05) takes.push({ id: mkId(part) }); }
      clip = { bars: Math.min(16, Math.max(1, Math.ceil(AUD.loopLen / 16))), notes: [], audio: takes[takes.length - 1], takes }; item = { lane: AUD.lane, t: ti, slot, s: s0, l: AUD.loopLen, off: 0, take: takes.length - 1 };
      status(takes.length + ' toma(s) grabadas en bucle. Elígelas con el selector "Toma" (Más ▾).', 7000);
    } else {
      const au = { id: mkId(full) }; clip = { bars: bars(full), notes: [], audio: au }; item = { lane: AUD.lane, t: ti, slot, s: s0, l: Math.max(1, Math.ceil(full.duration / sd)), off: 0 };
      status('Grabación colocada en ' + ln.name + ' (' + full.duration.toFixed(1) + ' s).', 5000);
    }
    t.clips[slot] = clip; song.arr.push(item); resolveOverlap(item); ARR.sel = item; renderAll();
  } catch (err) { status('No se pudo procesar la grabación: ' + err.message, 7000); }
  closeInput(); if (song.lanes.some(l => l.arm)) ensureInput();
}
// indicador de grabación (bloque rojo que crece) y medidor de entrada
function recTick() {
  const cv = $('#inMeter'), g = cv.getContext('2d'); g.fillStyle = '#0d1114'; g.fillRect(0, 0, cv.width, cv.height);
  if (AUD.an) { const b = new Float32Array(AUD.an.fftSize); AUD.an.getFloatTimeDomainData(b); let pk = 0; for (let i = 0; i < b.length; i++) pk = Math.max(pk, Math.abs(b[i]));
    const w = Math.min(1, pk * 1.6) * cv.width; g.fillStyle = pk > 0.9 ? '#ff4d4d' : '#4fd37a'; g.fillRect(0, 4, w, cv.height - 8); }
  const lane = ARR.laneEls && ARR.laneEls[AUD.lane]; let blk = document.getElementById('recblock');
  if (AUD.active && lane && eng && AUD.recStart !== undefined) {
    const p = eng.posFloat(); if (!blk || blk.parentNode !== lane) { if (blk) blk.remove(); blk = el('div', 'recblock'); blk.id = 'recblock'; lane.appendChild(blk); }
    blk.style.left = AUD.recStart * ARR.z + 'px'; blk.style.width = Math.max(0, (p - AUD.recStart) * ARR.z) + 'px';
  } else if (blk) blk.remove();
}
