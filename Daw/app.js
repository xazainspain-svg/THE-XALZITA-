'use strict';
// Núcleo de Xaza Live: estado, transporte, vista Session, panel de dispositivo, guardar/abrir/exportar, teclado, animación.

const $ = (s, r = document) => r.querySelector(s);
const el = (tag, cls, txt) => { const e = document.createElement(tag); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; };

let song = demoSong(), eng = null, view = 'arr', playMode = 'song', selTrack = 0, selSlot = 0, octave = 4;
let cells = [], meters = [];

function ensureEngine() {
  if (!eng) {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    eng = new Engine(ctx, song); eng.onEnd = () => updatePlayBtn();
  }
  if (eng.ctx.state === 'suspended') eng.ctx.resume();
  return eng;
}
function setSong(s, keepSel) {
  const metro = eng && eng.metro, st = selTrack, ss = selSlot;
  if (eng) { eng.dispose(); const ctx = eng.ctx; song = s; eng = new Engine(ctx, song); eng.metro = metro; eng.onEnd = () => updatePlayBtn(); } else song = s;
  selTrack = keepSel ? Math.min(st, song.tracks.length - 1) : 0; selSlot = keepSel ? ss : 0; ARR.sel = null; ROLL.sel.clear();
  $('#bpm').value = song.bpm; $('#swing').value = song.swing || 0; if (typeof refreshKnobs === 'function') refreshKnobs(); renderAll();
}
function rebuildEngine() {
  if (!eng) return; const metro = eng.metro, ctx = eng.ctx; eng.dispose(); eng = new Engine(ctx, song); eng.metro = metro; eng.onEnd = () => updatePlayBtn(); updatePlayBtn();
}
const status = (m, ms = 3500) => { $('#status').textContent = m; if (ms) setTimeout(() => { if ($('#status').textContent === m) $('#status').textContent = ''; }, ms); };
const curClip = () => { const t = song.tracks[selTrack]; return t && t.clips[selSlot]; };

// ------------------------------------------------------------------ transporte
function updatePlayBtn() { const on = !!(eng && eng.playing); $('#btnPlay').classList.toggle('on', on); $('#btnPlay').textContent = on ? '❚❚' : '▶'; }
function togglePlay() {
  ensureEngine();
  if (eng.playing) eng.stop(); else if (playMode === 'song') eng.play('arr', ARR.cursor); else { eng.launchScene(selSlot); }
  updatePlayBtn();
}
$('#btnPlay').onclick = togglePlay;
$('#btnStop').onclick = () => { if (eng) eng.stop(); updatePlayBtn(); };
$('#bpm').onchange = e => { song.bpm = Math.max(40, Math.min(240, +e.target.value || 120)); e.target.value = song.bpm; };
$('#btnMetro').onclick = e => { ensureEngine(); eng.metro = !eng.metro; e.target.classList.toggle('on', eng.metro); };
function setView(v) {                                  // vista principal: 'arr' (Playlist) o 'session'
  if (v === 'mixer' || v === 'imm') { openWin(v); return; }
  if (eng && eng.playing) { eng.stop(); updatePlayBtn(); }
  view = v; $('#session').hidden = v !== 'session'; $('#arr').hidden = v !== 'arr'; updateWinButtons(); renderAll();
}

// ------------------------------------------------------------------ vista Session
function renderSession() {
  const root = $('#session'); root.innerHTML = ''; cells = []; meters = [];
  root.style.gridTemplateColumns = 'repeat(' + song.tracks.length + ',116px) 76px';
  song.tracks.forEach((t, i) => {
    const h = el('div', 'th' + (i === selTrack ? ' sel' : ''), (t.kind === 'audio' ? '♫ ' : '') + t.name); h.style.setProperty('--c', t.color);
    const x = el('span', 'tx', '✕'); x.title = 'Quitar pista'; x.onclick = e => { e.stopPropagation(); removeTrack(i); }; h.appendChild(x);
    h.onclick = () => { selTrack = i; renderAll(); }; h.ondblclick = () => { const n = prompt('Nombre de la pista', t.name); if (n) { t.name = n; renderAll(); } };
    root.appendChild(h);
  });
  root.appendChild(el('div', 'th', 'Escenas'));
  for (let s = 0; s < SCENES; s++) {
    song.tracks.forEach((t, i) => {
      const c = t.clips[s], d = el('div', 'cell ' + (c ? 'has' : 'empty') + (i === selTrack && s === selSlot ? ' sel' : ''));
      d.style.setProperty('--c', t.color);
      d.appendChild(el('div', 'pl', c ? '▶' : (t.kind === 'audio' ? '♫' : '+'))); if (c) d.appendChild(el('span', 'nm', c.name ? c.name : c.audio ? ((AUDIO[c.audio.id] && AUDIO[c.audio.id].name) || 'sin audio') : c.bars + (c.bars > 1 ? ' compases' : ' compás')));
      d.onclick = () => {
        selTrack = i; selSlot = s; ROLL.sel.clear();
        if (!t.clips[s] && t.kind === 'audio') { importAudio(i, s); return; }
        if (!t.clips[s]) { t.clips[s] = clipNew(); openWin('roll'); } else { ensureEngine(); eng.launch(i, s); updatePlayBtn(); }
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
    const vol = el('input'); vol.type = 'range'; vol.min = 0; vol.max = 1.2; vol.step = 0.01; vol.value = t.vol; vol.title = 'Volumen'; vol.dataset.auto = 't' + i + '.vol';
    vol.oninput = () => { t.vol = +vol.value; if (eng) eng.applyMix(); autoRec('t' + i + '.vol', t.vol); };
    const pan = el('input'); pan.type = 'range'; pan.min = -1; pan.max = 1; pan.step = 0.01; pan.value = t.pan; pan.title = 'Panorama'; pan.dataset.auto = 't' + i + '.pan';
    pan.oninput = () => { t.pan = +pan.value; if (eng) eng.applyMix(); autoRec('t' + i + '.pan', t.pan); };
    const ins = el('select'); song.inserts.forEach((x, k) => { const o = el('option', '', 'Insert ' + (k + 1) + (x.name !== 'Insert ' + (k + 1) ? ' · ' + x.name : '')); o.value = k; ins.appendChild(o); });
    ins.value = t.insert; ins.title = 'Insert del mixer al que se envía esta pista'; ins.onchange = () => { t.insert = +ins.value; if (eng) eng.applyMix(); };
    const mkSend = (k, lab) => {
      const box = el('div', 'sendrow'), r = el('input'); r.type = 'range'; r.min = 0; r.max = 1; r.step = 0.01; r.value = (t.sends && t.sends[k]) || 0; r.title = lab; r.dataset.auto = 't' + i + (k ? '.sb' : '.sa');
      r.oninput = () => { t.sends = t.sends || [0, 0]; t.sends[k] = +r.value; if (eng) eng.applyMix(); autoRec('t' + i + (k ? '.sb' : '.sa'), t.sends[k]); };
      const pl = el('label', 'pre'), pc = el('input'); pc.type = 'checkbox'; pc.checked = !!(t.preSend && t.preSend[k]); pc.title = 'Envío pre-fader';
      pc.onchange = () => { t.preSend = t.preSend || [false, false]; t.preSend[k] = pc.checked; if (eng) eng.applyMix(); }; pl.append(pc, 'pre'); box.append(r, pl); return box;
    };
    const cv = el('canvas'); cv.width = 100; cv.height = 10; meters[i] = cv;
    m.append(el('label', '', 'Vol'), vol, el('label', '', 'Pan'), pan, el('label', '', 'Envío A (reverb)'), mkSend(0, 'Envío al retorno A'), el('label', '', 'Envío B (delay)'), mkSend(1, 'Envío al retorno B'), el('label', '', 'Insert'), ins, cv); root.appendChild(m);
  });
  root.appendChild(el('div'));
}

// ------------------------------------------------------------------ dispositivos (instrumento de la pista)
function slider(parent, label, min, max, val, step, onInput, log, target) {
  const row = el('div', 'prm'), lab = el('span', '', label), r = el('input'); r.type = 'range'; if (target) r.dataset.auto = target;
  if (log) { r.min = Math.log(min); r.max = Math.log(max); r.step = 0.001; r.value = Math.log(val); r.oninput = () => onInput(Math.exp(+r.value)); }
  else { r.min = min; r.max = max; r.step = step || (max - min) / 200; r.value = val; r.oninput = () => onInput(+r.value); }
  row.append(lab, r); parent.appendChild(row);
}
function renderDevPane() {
  const pane = $('#devPane'); pane.innerHTML = ''; const t = song.tracks[selTrack], wrap = el('div', 'dev'); pane.appendChild(wrap);
  const names = { audio: 'Pista de audio', drums: 'Caja de ritmos', synth: 'Sintetizador', fm: 'Sintetizador FM', sampler: 'Sampler' };
  const inst = el('div', 'card'); inst.appendChild(el('h4', '', names[t.kind]));
  const sel = (label, opts, val, fn) => { const row = el('div', 'prm'), s = el('select'); opts.forEach(([v, l]) => { const o = el('option', '', l); o.value = v; s.appendChild(o); }); s.value = val; s.onchange = () => fn(s.value); row.append(el('span', '', label), s); inst.appendChild(row); };
  if (t.kind === 'audio') inst.appendChild(el('div', '', 'Clic en una celda vacía para importar audio, o ● Mic para grabar. En la pestaña Clip puedes recortar, invertir, ajustar la ganancia y estirar al tempo.'));
  else if (t.kind === 'drums') inst.appendChild(el('div', '', 'Sonidos: ' + DRUM_NAMES.join(', ') + '.'));
  else if (t.kind === 'synth') {
    const W = ['sawtooth', 'square', 'triangle', 'sine'];
    sel('Onda 1', W.map(w => [w, w]), t.inst.wave, v => t.inst.wave = v);
    sel('Onda 2', [['none', 'ninguna']].concat(W.map(w => [w, w])), t.inst.w2, v => t.inst.w2 = v);
    slider(inst, 'Desafinar 2', -50, 50, t.inst.d2, 1, v => t.inst.d2 = v);
    slider(inst, 'Sub (−1 oct)', 0, 1, t.inst.sub, 0.01, v => t.inst.sub = v);
    slider(inst, 'Corte', 100, 12000, t.inst.cutoff, 0, v => { t.inst.cutoff = v; autoRec('t' + selTrack + '.cutoff', v); }, true, 't' + selTrack + '.cutoff');
    slider(inst, 'Resonancia', 0.1, 18, t.inst.res, 0.1, v => { t.inst.res = v; autoRec('t' + selTrack + '.res', v); }, false, 't' + selTrack + '.res');
    slider(inst, 'Env. filtro', 0, 1, t.inst.fe, 0.01, v => t.inst.fe = v);
    slider(inst, 'Decay filtro', 0.03, 2, t.inst.fd, 0.01, v => t.inst.fd = v);
    slider(inst, 'Ataque', 0.003, 1.5, t.inst.atk, 0.005, v => t.inst.atk = v);
    slider(inst, 'Release', 0.02, 3, t.inst.rel, 0.01, v => t.inst.rel = v);
  } else if (t.kind === 'fm') {
    slider(inst, 'Ratio mod.', 0.25, 8, t.inst.ratio, 0.25, v => t.inst.ratio = v);
    slider(inst, 'Índice', 0, 12, t.inst.idx, 0.1, v => t.inst.idx = v);
    slider(inst, 'Decay índice', 0.03, 2, t.inst.fdec, 0.01, v => t.inst.fdec = v);
    slider(inst, 'Ataque', 0.003, 1.5, t.inst.atk, 0.005, v => t.inst.atk = v);
    slider(inst, 'Release', 0.02, 3, t.inst.rel, 0.01, v => t.inst.rel = v);
  } else {
    const smp = AUDIO[t.inst.id]; inst.appendChild(el('div', '', smp ? 'Sample: ' + smp.name : 'Sin sample. Cárgalo con el botón o desde la pestaña Samples.'));
    const ld = el('button', '', smp ? 'Cambiar sample…' : 'Cargar sample…'); ld.onclick = () => importSamplerFor(selTrack); inst.appendChild(ld);
    slider(inst, 'Nota raíz', 24, 96, t.inst.root, 1, v => t.inst.root = Math.round(v));
    slider(inst, 'Release', 0.02, 3, t.inst.rel, 0.01, v => t.inst.rel = v);
    const lr = el('label', 'prm'), lc = el('input'); lc.type = 'checkbox'; lc.checked = t.inst.loop; lc.onchange = () => { t.inst.loop = lc.checked; }; lr.append(el('span', '', 'Bucle'), lc); inst.appendChild(lr);
  }
  wrap.appendChild(inst);
  const r = el('div', 'card'); r.appendChild(el('h4', '', 'Salida'));
  r.appendChild(el('div', '', 'Esta pista va al Insert ' + (t.insert + 1) + '. Los efectos se editan en la pestaña Mixer.')); wrap.appendChild(r);
}
function renderBottom() {                              // refresca las ventanas abiertas
  const t = song.tracks[selTrack], info = t.name + (curClip() ? ' — patrón ' + (selSlot + 1) : '');
  $('#selInfo').textContent = info; $('#selInfoRack').textContent = '— patrón ' + (selSlot + 1);
  ['rack', 'roll', 'dev', 'mixer', 'imm'].forEach(id => { if (winOpen(id)) renderWin(id); });
  if (typeof updatePatName === 'function') updatePatName();
}
function renderAll() { if (view === 'session') renderSession(); else renderArr(); renderBottom(); if (typeof renderBrowser === 'function') renderBrowser(); }

// ------------------------------------------------------------------ guardar / abrir / exportar
const download = (blob, name) => { const a = el('a'); a.href = URL.createObjectURL(blob); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 2000); };
$('#btnNew').onclick = () => { if (confirm('¿Empezar un proyecto vacío? Se pierde lo no guardado.')) { const s = demoSong(); s.tracks.forEach(t => { t.clips = Array(SCENES).fill(null); }); s.autos = []; s.inserts.forEach(x => { x.fx = Array(NSLOTS).fill(null); }); s.arr = []; setSong(s); } };
$('#btnSave').onclick = () => {
  const json = JSON.stringify(song); try { localStorage.setItem('xazalive_song', json); } catch (e) {}
  download(new Blob([json], { type: 'application/json' }), 'proyecto.xazalive.json'); status('Proyecto guardado.');
};
$('#btnLoad').onclick = () => $('#fileIn').click();
$('#fileIn').onchange = e => {
  const f = e.target.files[0]; if (!f) return;
  f.text().then(txt => { setSong(upgradeSong(JSON.parse(txt))); status('Proyecto abierto.'); }).catch(err => status('No se pudo abrir: ' + err.message, 5000));
  e.target.value = '';
};
$('#btnExport').onclick = async () => {
  try { status('Renderizando…', 0); const r = await renderWav(song); download(r.blob, 'xaza-live.wav'); status('WAV exportado (pico ' + r.peak.toFixed(2) + ').', 5000); }
  catch (e) { status(e.message, 5000); }
};

// ------------------------------------------------------------------ teclado
const KEYMAP = 'awsedftgyhujkolp';
document.addEventListener('keydown', e => {
  if (e.target.matches('input,select,textarea') && e.target.type !== 'range' && e.target.type !== 'checkbox') return;
  if (e.code === 'Space') { e.preventDefault(); togglePlay(); return; }
  if (typeof winKey === 'function' && winKey(e)) { e.preventDefault(); return; }
  if (arrKey(e) || rollKey(e)) { e.preventDefault(); return; }
  if (extrasKey(e)) { e.preventDefault(); return; }
  if (e.repeat || e.ctrlKey || e.metaKey || e.altKey) return;
  const k = e.key.toLowerCase();
  if (k === 'z') { octave = Math.max(1, octave - 1); status('Octava ' + octave); return; }
  if (k === 'x') { octave = Math.min(7, octave + 1); status('Octava ' + octave); return; }
  const i = KEYMAP.indexOf(k); if (i < 0) return;
  const t = song.tracks[selTrack]; if (t.kind === 'audio') return;
  const n = t.kind === 'drums' ? 'asdfghjk'.indexOf(k) : (octave + 1) * 12 + i;
  if (n < 0) return; held[k] = n; liveOn(n, 0.8);
});
const held = {};
document.addEventListener('keyup', e => { const k = e.key.toLowerCase(); if (held[k] !== undefined) { liveOff(held[k]); delete held[k]; } });

// ------------------------------------------------------------------ animación
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
    } else if (view === 'arr') arrTick(st);
    if (winOpen('mixer')) mixerTick();
    meterDraw($('#masterMeter'), eng.masterAn); extrasTick(); recTick(); if (winOpen('rack')) rackTick();
    if (winOpen('roll') && ROLL.cv) {
      const c = curClip(), r = eng.rt[selTrack]; let ps = -1;
      if (c && eng.playing && st >= 0) {
        const L = c.bars * 16;
        if (eng.mode === 'session' && r.playing === selSlot) ps = (((st - r.launch) % L) + L) % L;
        else if (eng.mode === 'arr') { const it = song.arr.find(a => (a.pat === selSlot || (a.t === selTrack && a.slot === selSlot)) && st >= a.s && st < a.s + a.l); if (it) ps = (st - it.s + (it.off || 0)) % L; }
      }
      rollTick(ps);
    }
    if (!eng.playing) { updatePlayBtn(); if (AREC.act.size) autoRecEnd(); }
  }
  requestAnimationFrame(frame);
}

