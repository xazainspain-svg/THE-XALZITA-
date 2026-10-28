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
  const n = song.tracks.length, t = trackNew(kind === 'audio' ? 'Audio ' + (n + 1) : 'Synth ' + (n + 1), kind, COLORS[n % COLORS.length], null, Math.min(n, NINS - 1));
  song.tracks.push(t); rebuildEngine(); selTrack = n; renderAll(); status('Pista añadida.');
}
function removeTrack(i) {
  if (song.tracks.length <= 1) { status('Debe quedar al menos una pista.'); return; }
  song.tracks.splice(i, 1); song.arr = song.arr.filter(a => a.t !== i).map(a => (a.t > i ? Object.assign({}, a, { t: a.t - 1 }) : a));
  selTrack = Math.min(selTrack, song.tracks.length - 1); ARR.sel = null; rebuildEngine(); renderAll(); status('Pista quitada.');
}
$('#btnAddM').onclick = () => addTrack('synth'); $('#btnAddA').onclick = () => addTrack('audio');

// ----------------------------------------------------------------- audio: importar, grabar, forma de onda
const newId = () => 'a' + Math.random().toString(36).slice(2, 10);
function audioClip(buf, name) {
  const id = newId(); AUDIO[id] = { buf, name };
  return { bars: Math.min(16, Math.max(1, Math.ceil(buf.duration / (stepSec() * 16)))), notes: [], audio: { id } };
}
let pendingImport = null;
function importAudio(i, s) { pendingImport = [i, s]; $('#audioIn').click(); }
$('#audioIn').onchange = async e => {
  const f = e.target.files[0], tgt = pendingImport; e.target.value = ''; if (!f || !tgt) return;
  try {
    ensureEngine(); const buf = await eng.ctx.decodeAudioData(await f.arrayBuffer());
    song.tracks[tgt[0]].clips[tgt[1]] = audioClip(buf, f.name.replace(/\.[^.]+$/, '')); selTrack = tgt[0]; selSlot = tgt[1]; renderAll();
    status('Audio importado: ' + f.name + ' (' + buf.duration.toFixed(1) + ' s). Suena a su velocidad original.');
  } catch (err) { status('No se pudo leer el audio: ' + err.message, 6000); }
};
let mic = null;
$('#btnMic').onclick = async () => {
  const t = song.tracks[selTrack];
  if (mic) { mic.rec.stop(); return; }
  if (t.kind !== 'audio') { status('Selecciona una pista de audio (o añade una con + Audio).'); return; }
  try {
    ensureEngine(); const stream = await navigator.mediaDevices.getUserMedia({ audio: true }), rec = new MediaRecorder(stream), chunks = [], slot = selSlot, ti = selTrack;
    rec.ondataavailable = ev => chunks.push(ev.data);
    rec.onstop = async () => {
      stream.getTracks().forEach(x => x.stop()); mic = null; $('#btnMic').classList.remove('on'); $('#btnMic').textContent = '● Mic';
      try { const buf = await eng.ctx.decodeAudioData(await new Blob(chunks).arrayBuffer()); song.tracks[ti].clips[slot] = audioClip(buf, 'Grabación'); renderAll(); status('Grabación lista.'); }
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
  const a = AUDIO[c.audio.id];
  pane.appendChild(el('div', 'rollbar', 'Clip de audio: ' + (a ? a.name + ' — ' + a.buf.duration.toFixed(2) + ' s, ' + a.buf.sampleRate + ' Hz, ' + a.buf.numberOfChannels + ' canal(es)' : 'sin audio cargado (los samples no se guardan en el proyecto)')));
  const cv = el('canvas'); cv.width = 1100; cv.height = 150; cv.style.cssText = 'width:100%;max-width:1100px;background:#111;border-radius:3px'; pane.appendChild(cv);
  if (a) drawWave(cv, c.audio.id, 0, a.buf.duration, t.color);
  const row = el('div', 'rollbar'), rp = el('button', '', 'Reemplazar…'); rp.onclick = () => importAudio(selTrack, selSlot);
  const dl = el('button', '', 'Quitar audio'); dl.onclick = () => { t.clips[selSlot] = null; renderAll(); }; row.append(rp, dl); pane.appendChild(row);
}

// ----------------------------------------------------------------- grabación MIDI en vivo + MIDI externo
const REC = { on: false, held: {} };
$('#btnRec').onclick = () => {
  REC.on = !REC.on; $('#btnRec').classList.toggle('on', REC.on);
  if (!REC.on) { status('Grabación detenida.'); return; }
  const t = song.tracks[selTrack]; if (t.kind === 'audio') { REC.on = false; $('#btnRec').classList.remove('on'); status('Para grabar notas selecciona una pista MIDI.'); return; }
  if (view !== 'session') setView('session');
  if (!t.clips[selSlot]) t.clips[selSlot] = clipNew(); ensureEngine(); eng.launch(selTrack, selSlot); updatePlayBtn(); renderAll();
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
  ['Clic derecho', 'Borrar clip o nota'], ['Mayús + arrastrar (regla)', 'Región de loop'], ['Tap', 'Marcar el tempo con clics'], ['+ MIDI / + Audio', 'Añadir pistas (máx. 16)']];
$('#btnHelp').onclick = () => {
  const h = $('#help'); h.hidden = !h.hidden; if (h.hidden) return; h.innerHTML = '';
  const box = el('div', 'helpbox'); box.appendChild(el('h3', '', 'Atajos y funciones'));
  HELP.forEach(([k, d]) => { const r = el('div', 'hrow2'); r.append(el('kbd', '', k), el('span', '', d)); box.appendChild(r); });
  const x = el('button', '', 'Cerrar'); x.onclick = () => { h.hidden = true; }; box.appendChild(x); h.appendChild(box);
};
