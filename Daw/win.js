'use strict';
// Estructura de ventanas al estilo de FL Studio: barra de menús, barra de herramientas (PAT/SONG, tempo, patrón, perillas),
// navegador a la izquierda, ventanas flotantes (Channel Rack, Piano roll, Mixer, Instrumento, Inmersivo) y atajos F5–F10.

// ----------------------------------------------------------------- ventanas flotantes
const WINS = { rack: { x: 232, y: 112, w: 700, h: 400 }, roll: { x: 300, y: 190, w: 960, h: 500 }, mixer: { x: 232, y: 130, w: 1020, h: 600 },
  dev: { x: 900, y: 118, w: 330, h: 430 }, auto: { x: 300, y: 300, w: 760, h: 340 }, imm: { x: 330, y: 150, w: 800, h: 540 } };
let zTop = 30;
const winEl = id => $('#win-' + id);
const winOpen = id => { const e = winEl(id); return !!e && !e.hidden; };
function renderWin(id) {
  if (id === 'rack') renderRackPane(); else if (id === 'roll') renderClipPane(); else if (id === 'dev') renderDevPane();
  else if (id === 'mixer') renderMixer(); else if (id === 'imm') renderImm(); else if (id === 'auto') renderAutoEditor();
}
function focusWin(id) { FOCUS = id; winEl(id).style.zIndex = ++zTop; document.querySelectorAll('.win').forEach(w => w.classList.toggle('active', w === winEl(id))); }
function openWin(id) { const e = winEl(id); if (!e) return; e.hidden = false; focusWin(id); updateWinButtons(); renderWin(id); }
function closeWin(id) { winEl(id).hidden = true; if (FOCUS === id) FOCUS = 'arr'; updateWinButtons(); }
function toggleWin(id) { winOpen(id) ? closeWin(id) : openWin(id); }
function updateWinButtons() {
  const on = { wbPlaylist: view === 'arr', wbRack: winOpen('rack'), wbRoll: winOpen('roll'), wbBrowser: !$('#browser').hidden, wbMixer: winOpen('mixer'), wbDev: winOpen('dev') };
  Object.entries(on).forEach(([k, v]) => $('#' + k).classList.toggle('on', v));
}
function initWins() {
  Object.keys(WINS).forEach(id => {
    const w = winEl(id), p = WINS[id], vw = window.innerWidth, vh = window.innerHeight;
    w.style.left = Math.max(0, Math.min(p.x, vw - 200)) + 'px'; w.style.top = Math.max(0, Math.min(p.y, vh - 120)) + 'px';
    w.style.width = Math.min(p.w, vw - 20) + 'px'; w.style.height = Math.min(p.h, vh - 130) + 'px';
    const head = w.querySelector('.wt'); head.addEventListener('mousedown', e => {
      focusWin(id); if (e.target.closest('button')) return; const r = w.getBoundingClientRect(), dx = e.clientX - r.left, dy = e.clientY - r.top;
      const mv = ev => { w.style.left = Math.max(-r.width + 80, Math.min(window.innerWidth - 80, ev.clientX - dx)) + 'px'; w.style.top = Math.max(40, Math.min(window.innerHeight - 30, ev.clientY - dy)) + 'px'; };
      const up = () => { window.removeEventListener('mousemove', mv); window.removeEventListener('mouseup', up); };
      window.addEventListener('mousemove', mv); window.addEventListener('mouseup', up); e.preventDefault();
    });
    head.addEventListener('dblclick', e => { if (!e.target.closest('button')) w.classList.toggle('max'); });
    w.addEventListener('mousedown', () => focusWin(id), true);
    w.querySelector('.wx').onclick = () => closeWin(id);
    let tm = null; if (window.ResizeObserver) new ResizeObserver(() => { if (w.hidden || !['roll', 'mixer'].includes(id)) return; clearTimeout(tm); tm = setTimeout(() => renderWin(id), 120); }).observe(w);
  });
}
function winKey(e) {
  const map = { F5: () => setView('arr'), F6: () => toggleWin('rack'), F7: () => toggleWin('roll'), F8: () => { $('#browser').hidden = !$('#browser').hidden; updateWinButtons(); }, F9: () => toggleWin('mixer'), F10: () => toggleWin('dev') };
  if (map[e.key]) { map[e.key](); return true; }
  if (e.key === 'Escape') { document.querySelectorAll('.menu.open').forEach(m => m.classList.remove('open')); }
  return false;
}

// ----------------------------------------------------------------- perillas
function makeKnob(label, min, max, val, def, onInput, size = 36, target) {
  const w = el('div', 'knobw'), cv = el('canvas'); if (target) w.dataset.auto = target; cv.width = cv.height = size; w.title = label; w.append(cv, el('span', '', label)); let v = val;
  const draw = () => {
    const g = cv.getContext('2d'), c = size / 2, r = c - 4, a0 = Math.PI * 0.75, a1 = Math.PI * 2.25, t = (v - min) / (max - min), a = a0 + (a1 - a0) * t;
    g.clearRect(0, 0, size, size); g.lineWidth = 3; g.lineCap = 'round';
    g.strokeStyle = '#1b1f23'; g.beginPath(); g.arc(c, c, r, a0, a1); g.stroke();
    g.strokeStyle = '#f6921e'; g.beginPath(); g.arc(c, c, r, a0, a); g.stroke();
    g.fillStyle = '#4a545c'; g.beginPath(); g.arc(c, c, r - 4, 0, 6.3); g.fill();
    g.strokeStyle = '#e8edf0'; g.lineWidth = 2; g.beginPath(); g.moveTo(c + Math.cos(a) * (r - 11), c + Math.sin(a) * (r - 11)); g.lineTo(c + Math.cos(a) * (r - 4), c + Math.sin(a) * (r - 4)); g.stroke();
  };
  const set = (nv, fire) => { v = Math.max(min, Math.min(max, nv)); draw(); if (fire) onInput(v); };
  cv.onmousedown = e => {
    e.preventDefault(); const y0 = e.clientY, v0 = v;
    const mv = ev => set(v0 + (y0 - ev.clientY) * (max - min) / 160, true), up = () => { window.removeEventListener('mousemove', mv); window.removeEventListener('mouseup', up); };
    window.addEventListener('mousemove', mv); window.addEventListener('mouseup', up);
  };
  cv.ondblclick = () => set(def, true);
  cv.onwheel = e => { e.preventDefault(); set(v + (e.deltaY < 0 ? 1 : -1) * (max - min) / 50, true); };
  draw(); return { el: w, set: nv => set(nv, false) };
}
let KNOB = {};
function initKnobs() {
  const g = $('#gKnobs'); g.innerHTML = '';
  KNOB.vol = makeKnob('VOL', 0, 1.25, song.masterVol, 0.9, v => { song.masterVol = v; if (eng) eng.applyMix(); autoRec('m.vol', v); }, 36, 'm.vol');
  KNOB.swing = makeKnob('SWING', 0, 0.5, song.swing || 0, 0, v => { song.swing = v; $('#swing').value = v; });
  g.append(KNOB.vol.el, KNOB.swing.el);
}
function refreshKnobs() { if (KNOB.vol) { KNOB.vol.set(song.masterVol); KNOB.swing.set(song.swing || 0); } }

// ----------------------------------------------------------------- menús
function buildMenus() {
  const bar = $('#menubar'); bar.innerHTML = ''; const logo = el('div', 'mlogo'); logo.innerHTML = 'XAZA<b>live</b>'; bar.appendChild(logo);
  const click = id => () => $('#' + id).click();
  const MENUS = [
    ['Archivo', [['Nuevo proyecto', click('btnNew')], ['Abrir…', click('btnLoad')], ['Guardar', click('btnSave')], null, ['Importar MIDI…', click('btnMidiIn')], ['Exportar MIDI', click('btnMidiOut')],
      ['Exportar WAV', click('btnExport')], ['Exportar stems', click('btnStems')]]],
    ['Editar', [['Deshacer  Ctrl+Z', click('btnUndo')], ['Rehacer  Ctrl+Y', click('btnRedo')]]],
    ['Añadir', [['Sintetizador', click('btnAddM')], ['Sintetizador FM', click('btnAddF')], ['Sampler…', click('btnAddS')], ['Pista de audio', click('btnAddA')], ['Batería', () => addTrack('drums')]]],
    ['Patrones', Array.from({ length: SCENES }, (_, i) => [song.patterns[i].name, () => { selSlot = i; renderAll(); }]).concat([null, ['Renderizar patrón a audio', () => renderPatternToAudio(selSlot)]])],
    ['Ver', [['Playlist  F5', () => setView('arr')], ['Channel Rack  F6', () => toggleWin('rack')], ['Piano roll  F7', () => toggleWin('roll')], ['Navegador  F8', () => { $('#browser').hidden = !$('#browser').hidden; updateWinButtons(); }],
      ['Mixer  F9', () => toggleWin('mixer')], ['Instrumento  F10', () => toggleWin('dev')], null, ['Inmersivo 3D', () => toggleWin('imm')], ['Vista Session (clips)', () => setView('session')]]],
    ['Opciones', [['Metrónomo', click('btnMetro')], ['Tap tempo', click('btnTap')], ['Grabar con micrófono', click('btnMic')]]],
    ['Ayuda', [['Atajos y funciones', click('btnHelp')]]],
  ];
  MENUS.forEach(([name, items]) => {
    const m = el('div', 'menu'), b = el('button', 'mtitle', name), list = el('div', 'mlist');
    items.forEach(it => { if (!it) { list.appendChild(el('div', 'msep')); return; } const i = el('button', 'mitem', it[0]); i.onclick = () => { document.querySelectorAll('.menu.open').forEach(x => x.classList.remove('open')); it[1](); }; list.appendChild(i); });
    b.onclick = e => { e.stopPropagation(); const open = m.classList.contains('open'); document.querySelectorAll('.menu.open').forEach(x => x.classList.remove('open')); if (!open) m.classList.add('open'); };
    b.onmouseenter = () => { if (document.querySelector('.menu.open') && !m.classList.contains('open')) { document.querySelectorAll('.menu.open').forEach(x => x.classList.remove('open')); m.classList.add('open'); } };
    m.append(b, list); bar.appendChild(m);
  });
  document.addEventListener('click', () => document.querySelectorAll('.menu.open').forEach(x => x.classList.remove('open')));
}

// ----------------------------------------------------------------- navegador
const BR = { open: { Proyecto: true, Patrones: true, Instrumentos: true, Efectos: false, Samples: true, Historial: false } };
function renderBrowser() {
  const root = $('#browser'); root.innerHTML = '';
  const sec = (name, fill) => {
    const head = el('div', 'bhead' + (BR.open[name] ? ' open' : ''), name), body = el('div', 'bbody'); body.hidden = !BR.open[name];
    head.onclick = () => { BR.open[name] = !BR.open[name]; renderBrowser(); }; root.append(head, body); if (BR.open[name]) fill(body);
  };
  const item = (parent, text, color, on, dbl, sel) => { const d = el('div', 'bitem' + (sel ? ' sel' : ''), text); if (color) d.style.setProperty('--c', color); d.onclick = on; if (dbl) d.ondblclick = dbl; parent.appendChild(d); return d; };
  sec('Proyecto', b => song.tracks.forEach((t, i) => item(b, (t.kind === 'audio' ? '♫ ' : '') + t.name, t.color, () => { selTrack = i; renderAll(); }, () => { selTrack = i; openWin('dev'); }, i === selTrack)));
  sec('Patrones', b => Array.from({ length: SCENES }, (_, s) => { const used = song.tracks.some(t => t.clips[s]); const d = item(b, song.patterns[s].name + (used ? '' : ' (vacío)'), used ? song.patterns[s].color : '#555', () => { selSlot = s; renderAll(); }, () => { selSlot = s; renderAll(); openWin('roll'); }, s === selSlot); d.draggable = true; d.title = 'Arrástralo al Playlist'; d.ondragstart = e => { e.dataTransfer.setData('xaza/pat', String(s)); }; }));
  sec('Instrumentos', b => [['synth', 'Sintetizador'], ['fm', 'Sintetizador FM'], ['sampler', 'Sampler'], ['drums', 'Batería'], ['audio', 'Pista de audio']].forEach(([k, n]) => item(b, '＋ ' + n, '#6d8cff', () => status('Doble clic para añadir: ' + n), () => addTrack(k))));
  sec('Efectos', b => Object.keys(FX_DEFS).forEach(k => item(b, FX_DEFS[k].name, '#35c9c9', () => status('Doble clic para insertarlo en el insert de la pista seleccionada.'), () => {
    const ins = song.inserts[song.tracks[selTrack].insert], slot = ins.fx.findIndex(f => !f); if (slot < 0) { status('El insert no tiene ranuras libres.'); return; }
    ins.fx[slot] = fxNew(k); if (eng) eng.rebuildInsert(song.tracks[selTrack].insert); mixSel = song.tracks[selTrack].insert; MIXG = false; openWin('mixer'); status(FX_DEFS[k].name + ' añadido al ' + ins.name + '.'); })));
  sec('Historial', b => {
    if (!UNDO.stack.length) { b.appendChild(el('div', 'bnote', 'Aún no hay cambios.')); return; }
    UNDO.stack.map((e, i) => [e, i]).slice(-30).reverse().forEach(([e, i]) => { const tm = new Date(e.t), d = item(b, tm.toLocaleTimeString().slice(0, 8) + '  ' + e.label, '#8995a0', () => gotoHistory(i)); d.title = 'Clic: volver al estado anterior a este cambio'; });
    const cur = el('div', 'bnote', 'Clic en un cambio para volver antes de él (puedes rehacer).'); b.appendChild(cur);
  });
  sec('Samples', b => {
    const ids = Object.keys(AUDIO); if (!ids.length) b.appendChild(el('div', 'bnote', 'Sin samples. Importa uno.'));
    ids.forEach(id => { const d0 = item(b, '♪ ' + AUDIO[id].name, '#9ecb6a', () => { ensureEngine(); const s = eng.ctx.createBufferSource(); s.buffer = AUDIO[id].buf; s.connect(eng.master); s.start(); },
      () => { const t = song.tracks[selTrack]; if (t.kind === 'sampler') { t.inst.id = id; status('Sample asignado a ' + t.name); renderAll(); } else if (t.kind === 'audio') { t.clips[selSlot] = audioClipFromId(id); renderAll(); } else status('Selecciona una pista sampler o de audio.'); }); d0.draggable = true; d0.title = 'Clic: escuchar · arrástralo al Playlist'; d0.ondragstart = e => { e.dataTransfer.setData('xaza/sample', id); }; });
    const imp = el('button', 'bbtn', 'Importar sample…'); imp.onclick = () => importLibrary(); b.appendChild(imp);
  });
}

// ----------------------------------------------------------------- barra de herramientas
function updatePatName() { const e = $('#patName'); if (e) { e.textContent = song.patterns[selSlot].name; e.style.color = song.patterns[selSlot].color; } }
function initToolbar() {
  $('#patPrev').onclick = () => { selSlot = (selSlot + SCENES - 1) % SCENES; renderAll(); };
  $('#patNext').onclick = () => { selSlot = (selSlot + 1) % SCENES; renderAll(); };
  const setMode = m => { playMode = m; $('#btnPat').classList.toggle('on', m === 'pat'); $('#btnSong').classList.toggle('on', m === 'song'); if (eng && eng.playing) { eng.stop(); updatePlayBtn(); } };
  $('#btnPat').onclick = () => setMode('pat'); $('#btnSong').onclick = () => setMode('song');
  $('#wbPlaylist').onclick = () => setView('arr'); $('#wbRack').onclick = () => toggleWin('rack'); $('#wbRoll').onclick = () => toggleWin('roll');
  $('#wbMixer').onclick = () => toggleWin('mixer'); $('#wbDev').onclick = () => toggleWin('dev'); $('#wbBrowser').onclick = () => { $('#browser').hidden = !$('#browser').hidden; updateWinButtons(); };
  const bpm = $('#bpm'), lcd = bpm.parentNode; let dragY = null, v0 = 0;
  lcd.onmousedown = e => { if (e.target === bpm && document.activeElement === bpm) return; dragY = e.clientY; v0 = song.bpm; };
  window.addEventListener('mousemove', e => { if (dragY === null) return; const nv = Math.max(40, Math.min(240, Math.round(v0 + (dragY - e.clientY) / 2))); if (nv !== song.bpm) { song.bpm = nv; bpm.value = nv; } });
  window.addEventListener('mouseup', () => { dragY = null; });
}

// ----------------------------------------------------------------- clips de automatización (clic derecho en un control)
function freeLane(s, l) {                                      // pista del Playlist sin nada en ese tramo (de abajo hacia arriba)
  for (let i = song.lanes.length - 1; i >= 0; i--) if (!song.arr.some(it => laneOf(it) === i && it.s < s + l && it.s + it.l > s)) return i;
  return ARR.lane || 0;
}
function createAutoClip(id) {
  const T = autoInfo(id, song); if (!T) { status('Ese parámetro no se puede automatizar.'); return; }
  const s = aSnapF(ARR.cursor), l = 32, v = autoNorm(T, T.get()), it = { lane: freeLane(s, l), auto: id, s, l, off: 0, pts: [{ s: 0, v }, { s: l, v }] };
  song.arr.push(it); ARR.editAuto = it; ARR.sel = it; ARR.sels = new Set([it]); setView('arr'); openWin('auto');
  status('Clip de automatización creado en el Playlist para: ' + T.label + '. Edítalo en su ventana; muévelo y alárgalo como cualquier clip.', 7000);
}
document.addEventListener('contextmenu', e => {
  const t = e.target.closest && e.target.closest('[data-auto]'); document.querySelectorAll('.ctxmenu').forEach(x => x.remove()); if (!t) return;
  e.preventDefault(); const m = el('div', 'ctxmenu'), b = el('button', 'mitem', 'Crear clip de automatización'); b.onclick = () => { m.remove(); createAutoClip(t.dataset.auto); };
  m.appendChild(b); m.style.left = e.clientX + 'px'; m.style.top = e.clientY + 'px'; document.body.appendChild(m);
  const close = () => { m.remove(); document.removeEventListener('mousedown', close, true); }; setTimeout(() => document.addEventListener('mousedown', ev => { if (!m.contains(ev.target)) close(); }, true), 0);
});
