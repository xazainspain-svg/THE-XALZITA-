'use strict';
// Funciones al estilo FL Studio: Channel Rack (step sequencer), herramientas del piano roll (cuantizar, humanizar, arpegiador,
// strum, voltear, pan y tono fino por nota), editor de samples, navegador de samples, importar/exportar MIDI y stems.

// ----------------------------------------------------------------- Channel Rack
const RACK = { note: {} };
function rackClip(ti, create) { const t = song.tracks[ti]; if (!t.clips[selSlot] && create) t.clips[selSlot] = clipNew(); return t.clips[selSlot]; }
function renderRackPane() {
  const pane = $('#rackPane'); pane.innerHTML = '';
  const hb = el('div', 'rollbar rackhead'), pv = el('button', 'mini', '◀'), nx = el('button', 'mini', '▶'), pn = el('span', 'lcd2', 'Patrón ' + (selSlot + 1));
  pv.onclick = () => { selSlot = (selSlot + SCENES - 1) % SCENES; renderAll(); }; nx.onclick = () => { selSlot = (selSlot + 1) % SCENES; renderAll(); };
  hb.append(pv, pn, nx); hb.title = 'Clic: activar/desactivar paso (arrastra para pintar) · Mayús+clic: acento'; pane.appendChild(hb);
  const wrap = el('div', 'rack'); pane.appendChild(wrap); let drag = null;
  const stepCell = (ti, pitchFn, c, s) => {
    const b = el('div', 'rs g' + (Math.floor(s / 4) % 2)); b.dataset.s = s;
    const find = () => c.notes.find(n => n.n === pitchFn() && Math.floor(n.s + 1e-9) === s && n.l <= 1.01);
    const hit = find(); if (hit) { b.classList.add('on'); if (hit.v >= 0.95) b.classList.add('acc'); b.style.setProperty('--c', song.tracks[ti].color); }
    const set = on => {
      const cur = find();
      if (on && !cur) { c.notes.push({ s, l: 1, n: pitchFn(), v: 0.8 }); b.classList.add('on'); b.style.setProperty('--c', song.tracks[ti].color); ensureEngine(); eng.note(song.tracks[ti], ti, pitchFn(), eng.ctx.currentTime, 0.2, 0.8); }
      else if (!on && cur) { c.notes.splice(c.notes.indexOf(cur), 1); b.classList.remove('on', 'acc'); }
    };
    b.onmousedown = e => {
      e.preventDefault(); const cur = find();
      if (e.shiftKey && cur) { cur.v = cur.v >= 0.95 ? 0.6 : 1; b.classList.toggle('acc', cur.v >= 0.95); return; }
      drag = !cur; set(drag);
    };
    b.onmouseenter = () => { if (drag !== null) set(drag); };
    return b;
  };
  window.addEventListener('mouseup', () => { drag = null; });
  song.tracks.forEach((t, ti) => {
    if (t.kind === 'audio') return;
    const c = rackClip(ti, false), box = el('div', 'rackrow'), head = el('div', 'rh'); head.style.setProperty('--c', t.color);
    const led = el('button', 'led' + (t.mute ? '' : ' on')); led.title = 'Activar / silenciar canal'; led.onclick = () => { t.mute = !t.mute; led.classList.toggle('on', !t.mute); if (eng) eng.applyMix(); };
    const kp = makeKnob('PAN', -1, 1, t.pan, 0, v => { t.pan = v; if (eng) eng.applyMix(); autoRec('t' + ti + '.pan', v); }, 26), kv = makeKnob('VOL', 0, 1.2, t.vol, 0.8, v => { t.vol = v; if (eng) eng.applyMix(); autoRec('t' + ti + '.vol', v); }, 26);
    const nb = el('button', 'cname' + (ti === selTrack ? ' sel' : ''), t.name); nb.onclick = () => { selTrack = ti; renderAll(); }; nb.ondblclick = () => openWin('dev');
    head.append(led, kp.el, kv.el, nb);
    if (!c) { const mk = el('button', '', '+ crear patrón'); mk.onclick = () => { rackClip(ti, true); renderRackPane(); }; head.appendChild(mk); box.appendChild(head); wrap.appendChild(box); return; }
    const L = c.bars * 16;
    if (t.kind === 'drums') {
      box.appendChild(head);
      const grid = el('div', 'rgrid');
      DRUM_NAMES.forEach((dn, d) => { const r = el('div', 'rline'); r.appendChild(el('span', 'rn', dn)); for (let s = 0; s < L; s++) r.appendChild(stepCell(ti, () => d, c, s)); grid.appendChild(r); });
      box.appendChild(grid);
    } else {
      const sel = el('select'); for (let n = 96; n >= 24; n--) { const o = el('option', '', noteName(n)); o.value = n; sel.appendChild(o); }
      sel.value = RACK.note[ti] || 60; sel.onchange = () => { RACK.note[ti] = +sel.value; renderRackPane(); }; head.appendChild(sel); box.appendChild(head);
      const r = el('div', 'rline'); r.appendChild(el('span', 'rn', noteName(+sel.value)));
      for (let s = 0; s < L; s++) r.appendChild(stepCell(ti, () => +sel.value, c, s)); box.appendChild(r);
    }
    wrap.appendChild(box);
  });
  const add = el('button', 'bbtn', '＋ Añadir canal'); add.onclick = () => addTrack('synth'); wrap.appendChild(add);
}
function rackTick() {
  if (!eng || !eng.playing) return; const st = eng.currentStep(); if (st < 0) return; let ps = -1;
  for (let i = 0; i < song.tracks.length; i++) { const c = song.tracks[i].clips[selSlot]; if (!c) continue; const L = c.bars * 16, r = eng.rt[i];
    if (eng.mode === 'session' && r.playing === selSlot) { ps = (((st - r.launch) % L) + L) % L; break; } if (eng.mode === 'arr') { ps = st % L; break; } }
  document.querySelectorAll('#rackPane .rs.now').forEach(x => x.classList.remove('now'));
  if (ps >= 0) document.querySelectorAll('#rackPane .rs[data-s="' + ps + '"]').forEach(x => x.classList.add('now'));
}

// ----------------------------------------------------------------- herramientas del piano roll
const TOOLSET = { arp: 'up', rate: 0.5 };
function rollTargets(c) { return ROLL.sel.size ? [...ROLL.sel] : c.notes; }
function groupByStart(notes) { const m = new Map(); notes.forEach(n => { const k = Math.round(n.s * 1000); if (!m.has(k)) m.set(k, []); m.get(k).push(n); }); return [...m.values()]; }
function rollToolsRow(pane, c, t) {
  const row = el('div', 'rollbar'); row.appendChild(el('span', 'sep', 'Herramientas'));
  const act = (label, tip, fn) => { const b = el('button', '', label); b.title = tip; b.onclick = () => { fn(); drawRoll(); }; row.appendChild(b); };
  act('Cuantizar', 'Ajusta inicio y duración al snap', () => { const sn = ROLL.snap > 0 ? ROLL.snap : 1; rollTargets(c).forEach(n => { n.s = Math.max(0, Math.round(n.s / sn) * sn); n.l = Math.max(sn, Math.round(n.l / sn) * sn); n.s = Math.min(n.s, c.bars * 16 - sn); n.l = Math.min(n.l, c.bars * 16 - n.s); }); });
  act('Humanizar', 'Variación aleatoria de tiempo y velocidad', () => rollTargets(c).forEach(n => { n.s = Math.max(0, Math.min(c.bars * 16 - n.l, n.s + (Math.random() - 0.5) * 0.3)); n.v = Math.max(0.2, Math.min(1, (n.v === undefined ? 0.8 : n.v) + (Math.random() - 0.5) * 0.3)); }));
  const pat = el('select'); [['up', 'Arp ↑'], ['down', 'Arp ↓'], ['updown', 'Arp ↑↓'], ['rand', 'Arp azar']].forEach(([v, l]) => { const o = el('option', '', l); o.value = v; pat.appendChild(o); }); pat.value = TOOLSET.arp; pat.onchange = () => { TOOLSET.arp = pat.value; };
  const rt = el('select'); [[1, '1 paso'], [0.5, '1/2 paso'], [0.25, '1/4 paso']].forEach(([v, l]) => { const o = el('option', '', l); o.value = v; rt.appendChild(o); }); rt.value = TOOLSET.rate; rt.onchange = () => { TOOLSET.rate = +rt.value; };
  row.append(pat, rt);
  act('Aplicar arpegio', 'Convierte los acordes seleccionados en arpegios', () => {
    const tg = rollTargets(c), keep = c.notes.filter(n => !tg.includes(n)), out = [];
    groupByStart(tg).forEach(g => {
      if (g.length < 2) { out.push(...g); return; }
      const ps = g.map(n => n.n).sort((a, b) => a - b), len = Math.max(...g.map(n => n.l)), s0 = g[0].s, r = TOOLSET.rate, cnt = Math.max(1, Math.floor(len / r));
      let seq = ps.slice(); if (TOOLSET.arp === 'down') seq.reverse(); else if (TOOLSET.arp === 'updown') seq = ps.concat(ps.slice(1, -1).reverse());
      for (let k = 0; k < cnt; k++) out.push({ s: s0 + k * r, l: r, n: TOOLSET.arp === 'rand' ? ps[Math.floor(Math.random() * ps.length)] : seq[k % seq.length], v: g[0].v === undefined ? 0.8 : g[0].v });
    });
    c.notes = keep.concat(out); ROLL.sel = new Set(out);
  });
  act('Strum', 'Escalona las notas de cada acorde', () => groupByStart(rollTargets(c)).forEach(g => { if (g.length < 2) return; g.sort((a, b) => a.n - b.n).forEach((n, k) => { const end = n.s + n.l; n.s = Math.min(end - 0.25, n.s + k * 0.25); n.l = end - n.s; }); }));
  act('Voltear ↔', 'Invierte el orden en el tiempo', () => { const tg = rollTargets(c); if (!tg.length) return; const a = Math.min(...tg.map(n => n.s)), b = Math.max(...tg.map(n => n.s + n.l)); tg.forEach(n => { n.s = a + b - (n.s + n.l); }); });
  act('Voltear ↕', 'Espeja las alturas', () => { const tg = rollTargets(c); if (!tg.length) return; const a = Math.min(...tg.map(n => n.n)), b = Math.max(...tg.map(n => n.n)); tg.forEach(n => { n.n = a + b - n.n; }); });
  const prop = (label, key, min, max, step) => {
    const l = el('label', 'sep', label + ' '), r = el('input'); r.type = 'range'; r.min = min; r.max = max; r.step = step; r.style.width = '70px';
    r.value = ROLL.sel.size ? ([...ROLL.sel][0][key] || 0) : 0; r.title = 'Se aplica a las notas seleccionadas';
    r.oninput = () => { [...ROLL.sel].forEach(n => { n[key] = +r.value; }); }; l.appendChild(r); row.appendChild(l);
  };
  prop('Pan nota', 'pan', -1, 1, 0.05); prop('Tono fino', 'fine', -100, 100, 1);
  const nm = el('input'); nm.type = 'text'; nm.placeholder = 'nombre del patrón'; nm.value = c.name || ''; nm.style.width = '120px'; nm.oninput = () => { c.name = nm.value; };
  nm.onchange = () => renderAll(); row.append(el('span', 'sep', 'Nombre'), nm);
  pane.appendChild(row);
}

// ----------------------------------------------------------------- navegador de samples
function renderSamplesPane() {
  const pane = $('#smpPane'); pane.innerHTML = '';
  const bar = el('div', 'rollbar'), imp = el('button', '', 'Importar sample…'); imp.onclick = () => importLibrary();
  bar.append(imp, el('span', 'hint', 'Los samples importados aparecen aquí. Asígnalos a una pista sampler o a una celda de audio.')); pane.appendChild(bar);
  const ids = Object.keys(AUDIO); if (!ids.length) { pane.appendChild(el('div', '', 'Todavía no hay samples. Importa uno, o crea una pista de audio y haz clic en una celda vacía.')); return; }
  ids.forEach(id => {
    const a = AUDIO[id], row = el('div', 'smp'); row.appendChild(el('b', '', a.name)); row.appendChild(el('span', '', a.buf.duration.toFixed(2) + ' s'));
    const pv = el('button', '', '▶'); pv.onclick = () => { ensureEngine(); const s = eng.ctx.createBufferSource(); s.buffer = a.buf; s.connect(eng.master); s.start(); };
    const t = song.tracks[selTrack];
    const toS = el('button', '', 'A pista sampler'); toS.onclick = () => { if (t.kind !== 'sampler') { status('Selecciona una pista sampler (+ Sampler).'); return; } t.inst.id = id; status('Sample asignado a ' + t.name + '.'); renderBottom(); };
    const toA = el('button', '', 'A celda de audio'); toA.onclick = () => { if (t.kind !== 'audio') { status('Selecciona una pista de audio.'); return; } t.clips[selSlot] = audioClipFromId(id); renderAll(); };
    row.append(pv, toS, toA); pane.appendChild(row);
  });
}
function audioClipFromId(id) { const a = AUDIO[id]; return { bars: Math.min(16, Math.max(1, Math.ceil(a.buf.duration / (stepSec() * 16)))), notes: [], audio: { id } }; }
function importLibrary() { pendingImport = null; $('#audioIn').click(); }

// ----------------------------------------------------------------- MIDI (SMF) importar / exportar
function vlq(n) { const b = [n & 0x7f]; while ((n >>= 7)) b.unshift((n & 0x7f) | 0x80); return b; }
function exportMidi() {
  const PPQ = 480, TPS = PPQ / 4, tracks = [], u32 = n => [n >>> 24, (n >> 16) & 255, (n >> 8) & 255, n & 255];
  const chunk = (id, data) => [...id].map(c => c.charCodeAt(0)).concat(u32(data.length), data);
  const mpqn = Math.round(60000000 / song.bpm), tempo = [0, 0xff, 0x51, 3, (mpqn >> 16) & 255, (mpqn >> 8) & 255, mpqn & 255, 0, 0xff, 0x2f, 0];
  tracks.push(chunk('MTrk', tempo));
  song.tracks.forEach((t, i) => {
    if (t.kind === 'audio') return;
    const ev = [], items = song.arr.filter(a => a.t === i && t.clips[a.slot]);
    const place = (c, base, len, off) => { const L = c.bars * 16; for (let rep = -1; rep * L < len + L; rep++) c.notes.forEach(n => { const s = rep * L + n.s - off; if (s >= 0 && s < len) ev.push([base + s, Math.min(n.l, len - s), n.n, n.v === undefined ? 0.8 : n.v]); }); };
    if (items.length) items.forEach(it => place(t.clips[it.slot], it.s, it.l, it.off || 0)); else { const c = t.clips[selSlot]; if (c) place(c, 0, c.bars * 16, 0); }
    if (!ev.length) return;
    const ch = t.kind === 'drums' ? 9 : (i % 15 === 9 ? 10 : i % 15), msgs = [];
    ev.forEach(([s, l, n, v]) => { msgs.push([Math.round(s * TPS), 1, n, v]); msgs.push([Math.round((s + l) * TPS), 0, n, v]); });
    msgs.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    const data = [0, 0xff, 0x03, t.name.length].concat([...t.name].map(c => c.charCodeAt(0) & 127)); let last = 0;
    msgs.forEach(([tk, on, n, v]) => { data.push(...vlq(tk - last), (on ? 0x90 : 0x80) | ch, n & 127, on ? Math.max(1, Math.round(v * 127)) : 0); last = tk; });
    data.push(0, 0xff, 0x2f, 0); tracks.push(chunk('MTrk', data));
  });
  const head = chunk('MThd', [0, 1, 0, tracks.length, PPQ >> 8, PPQ & 255]);
  download(new Blob([new Uint8Array(head.concat(...tracks))], { type: 'audio/midi' }), 'xaza-live.mid');
  status('MIDI exportado (' + (tracks.length - 1) + ' pista(s)).');
}
function importMidi(buf) {
  const d = new DataView(buf); let p = 0; const u16 = () => { const v = d.getUint16(p); p += 2; return v; }, u32 = () => { const v = d.getUint32(p); p += 4; return v; }, u8 = () => d.getUint8(p++);
  if (u32() !== 0x4d546864) throw new Error('No es un archivo MIDI'); u32(); u16(); const nt = u16(), div = u16(); if (div & 0x8000) throw new Error('Formato de tiempo SMPTE no soportado');
  const TPS = div / 4, perTrack = []; let tempo = null;
  for (let k = 0; k < nt; k++) {
    const notes = []; perTrack.push(notes);
    if (u32() !== 0x4d54726b) break; const len = u32(), end = p + len; let tick = 0, run = 0; const open = {};
    const rv = () => { let v = 0, b; do { b = u8(); v = (v << 7) | (b & 127); } while (b & 128); return v; };
    while (p < end) {
      tick += rv(); let st = d.getUint8(p); if (st & 0x80) { p++; if (st < 0xf0) run = st; } else st = run;
      if (st === 0xff) { const type = u8(), l = rv(); if (type === 0x51 && l === 3 && tempo === null) tempo = 60000000 / ((d.getUint8(p) << 16) | (d.getUint8(p + 1) << 8) | d.getUint8(p + 2)); p += l; }
      else if (st === 0xf0 || st === 0xf7) { p += rv(); }
      else { const c = st & 0xf0, a = u8(), b = (c === 0xc0 || c === 0xd0) ? 0 : u8();
        if (c === 0x90 && b > 0) open[a] = [tick, b]; else if (c === 0x80 || (c === 0x90 && b === 0)) { const o = open[a]; if (o) { notes.push({ s: o[0] / TPS, l: Math.max(0.25, (tick - o[0]) / TPS), n: a, v: o[1] / 127 }); delete open[a]; } } }
    }
    p = end;
  }
  const notes = perTrack.sort((a, b) => b.length - a.length)[0] || [];
  if (!notes.length) throw new Error('El archivo no tiene notas');
  return { notes, tempo };
}
const GM_DRUM = { 36: 0, 35: 0, 38: 1, 40: 1, 42: 2, 44: 2, 46: 3, 39: 4, 39.5: 4, 45: 5, 47: 5, 48: 5, 37: 6, 56: 7 };
$('#midiIn').onchange = async e => {
  const f = e.target.files[0]; e.target.value = ''; if (!f) return;
  try {
    const { notes, tempo } = importMidi(await f.arrayBuffer()), t = song.tracks[selTrack]; if (t.kind === 'audio') throw new Error('Selecciona una pista MIDI.');
    const end = Math.max(...notes.map(n => n.s + n.l)), bars = [1, 2, 4, 8].find(b => b * 16 >= end) || 8, L = bars * 16, clip = clipNew(bars);
    notes.filter(n => n.s < L).forEach(n => { const nn = t.kind === 'drums' ? (GM_DRUM[n.n] !== undefined ? GM_DRUM[n.n] : n.n % 8) : n.n; clip.notes.push({ s: n.s, l: Math.min(n.l, L - n.s), n: nn, v: n.v }); });
    t.clips[selSlot] = clip; if (tempo) { song.bpm = Math.round(tempo); $('#bpm').value = song.bpm; } renderAll(); status('MIDI importado: ' + clip.notes.length + ' notas en ' + bars + ' compás(es).');
  } catch (err) { status('No se pudo importar el MIDI: ' + err.message, 6000); }
};
$('#btnMidiIn').onclick = () => $('#midiIn').click(); $('#btnMidiOut').onclick = exportMidi;

// ----------------------------------------------------------------- stems
$('#btnStems').onclick = async () => {
  try {
    const used = song.tracks.map((t, i) => i).filter(i => song.arr.some(a => a.t === i)); if (!used.length) throw new Error('El Arrangement está vacío.');
    for (const i of used) {
      status('Renderizando stem: ' + song.tracks[i].name + '…', 0);
      const s = JSON.parse(JSON.stringify(song)); s.tracks.forEach((t, k) => { t.mute = k !== i; t.solo = false; });
      const r = await renderWav(s); download(r.blob, 'stem-' + song.tracks[i].name.replace(/[^\w-]+/g, '_') + '.wav');
    }
    status(used.length + ' stems exportados.', 5000);
  } catch (e) { status(e.message, 5000); }
};
