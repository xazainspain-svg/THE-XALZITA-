'use strict';
// Mixer estilo FL Studio: inserts verticales (medidor estéreo en dB + fader), master, 10 ranuras de efectos por insert,
// enrutado de pistas a inserts, panorama, mute y solo.

let MIXG = false;               // true = panel de grupos / VCA
let mixSel = 0;                 // insert seleccionado (-1 = master)
let mixMeters = [];             // [{cv, an:[L,R], lv:[0,0]}]

const dbOf = v => v <= 0.0001 ? -Infinity : 20 * Math.log10(v);
const dbTxt = v => { const d = dbOf(v); return d === -Infinity ? '-inf' : (d > 0 ? '+' : '') + d.toFixed(1) + ' dB'; };

function renderMixer() {
  const root = $('#mixer'); root.innerHTML = ''; mixMeters = [];
  const top = el('div', 'mixtop'), strips = el('div', 'strips'), panel = el('div', 'fxpanel'), dv = el('div', 'devview');
  top.append(strips, panel); root.append(top, dv); panel.hidden = !MIXG; renderDeviceView(dv);

  const mkStrip = (idx, name, color, get, set, isMaster) => {
    const s = el('div', 'strip' + (mixSel === idx ? ' sel' : '') + (isMaster ? ' master' : '')); s.style.setProperty('--c', color);
    const sel = () => { mixSel = idx; MIXG = false; renderMixer(); };
    const top = el('div', 'stop2'), num = el('span', 'snum', isMaster ? 'M' : String(idx + 1)); num.onclick = sel;
    if (!isMaster) {
      const ins = song.inserts[idx], led = el('button', 'led' + (ins.mute ? '' : ' on')); led.title = 'Activar / silenciar insert';
      led.onclick = () => { ins.mute = !ins.mute; led.classList.toggle('on', !ins.mute); if (eng) eng.applyMix(); };
      const so = el('button', 'solo' + (ins.solo ? ' on' : ''), 'S'); so.title = 'Solo'; so.onclick = () => { ins.solo = !ins.solo; so.classList.toggle('on', ins.solo); if (eng) eng.applyMix(); };
      top.append(led, num, so);
      const pk = makeKnob('PAN', -1, 1, ins.pan, 0, v => { ins.pan = v; if (eng) eng.applyMix(); autoRec('i' + idx + '.pan', v); }, 30, 'i' + idx + '.pan'); s.append(top, pk.el);
    } else { top.append(num); s.appendChild(top); }
    const fw = el('div', 'sfader'), cv = el('canvas'); cv.width = 22; cv.height = 128;
    const f = el('input'); f.type = 'range'; f.min = 0; f.max = 1.25; f.step = 0.005; f.value = get(); f.className = 'fader'; f.dataset.auto = isMaster ? 'm.vol' : 'i' + idx + '.vol';
    const db = el('div', 'sdb', dbTxt(get()));
    f.oninput = () => { set(+f.value); db.textContent = dbTxt(+f.value); if (eng) eng.applyMix(); autoRec(isMaster ? 'm.vol' : 'i' + idx + '.vol', +f.value); };
    f.ondblclick = () => { f.value = 0.8; f.oninput(); };
    fw.append(cv, f); s.append(fw, db);
    mixMeters.push({ cv, lv: [0, 0], idx: isMaster ? -1 : idx });
    if (!isMaster) {
      const ins = song.inserts[idx], to = el('select'); to.title = 'Salida del insert'; const o0 = el('option', '', '→ M'); o0.value = -1; to.appendChild(o0);
      for (let k = idx + 1; k < song.inserts.length; k++) { const o = el('option', '', '→ ' + (k + 1)); o.value = k; to.appendChild(o); }
      to.value = ins.to !== undefined && ins.to > idx ? ins.to : -1; to.onchange = () => { ins.to = +to.value; if (eng) eng.applyMix(); }; s.appendChild(to);
    }
    const nm = el('div', 'sname', name); nm.onclick = sel;
    if (!isMaster) { nm.ondblclick = () => { const n = prompt('Nombre del insert', name); if (n) { song.inserts[idx].name = n; renderMixer(); } };
      const used = song.inserts[idx].fx.filter(Boolean).length; nm.title = (used ? used + ' efecto(s). ' : '') + 'Recibe: ' + (song.tracks.filter(t => t.insert === idx).map(t => t.name).join(', ') || '—'); }
    s.appendChild(nm);
    return s;
  };
  strips.appendChild(mkStrip(-1, 'MASTER', '#ffb400', () => song.masterVol, v => song.masterVol = v, true));
  song.inserts.forEach((ins, i) => strips.appendChild(mkStrip(i, ins.name, '#6d8cff', () => ins.vol, v => ins.vol = v, false)));
  song.returns.forEach((r, k) => {
    const idx = -2 - k, s = el('div', 'strip ret' + (mixSel === idx ? ' sel' : '')); s.style.setProperty('--c', '#35c9c9');
    const nm = el('div', 'sname', k ? 'Retorno B' : 'Retorno A'); nm.onclick = () => { mixSel = idx; renderMixer(); };
    s.append(nm, el('div', 'sdots', FX_DEFS[r.fx.type].name));
    const fw = el('div', 'sfader'), cv = el('canvas'); cv.width = 22; cv.height = 128;
    const f = el('input'); f.type = 'range'; f.min = 0; f.max = 1.25; f.step = 0.005; f.value = r.vol; f.className = 'fader';
    const db = el('div', 'sdb', dbTxt(r.vol)); f.oninput = () => { r.vol = +f.value; db.textContent = dbTxt(r.vol); if (eng) eng.applyMix(); autoRec('r' + k + '.vol', r.vol); };
    fw.append(cv, f); s.append(fw, db); mixMeters.push({ cv, lv: [0, 0], idx: idx }); strips.appendChild(s);
  });

  if (MIXG) renderGroups(panel);
}

function buildFxSlots(parent, k0, rerender) {
  const ins = song.inserts[k0];
  for (let k = 0; k < NSLOTS; k++) {
    const f = ins.fx[k], slot = el('div', 'slot' + (f ? ' used' : '')), head = el('div', 'shead');
    const sel = el('select'); const o0 = el('option', '', '— ranura ' + (k + 1) + ' —'); o0.value = ''; sel.appendChild(o0);
    Object.keys(FX_DEFS).forEach(key => { const o = el('option', '', FX_DEFS[key].name); o.value = key; sel.appendChild(o); });
    sel.value = f ? f.type : '';
    sel.onchange = () => { ins.fx[k] = sel.value ? fxNew(sel.value) : null; if (eng) eng.rebuildInsert(k0); rerender(); };
    head.appendChild(sel);
    if (f && FX_PRESETS[f.type]) {
      const ps = el('select'); const p0 = el('option', '', 'preset'); p0.value = ''; ps.appendChild(p0);
      Object.keys(FX_PRESETS[f.type]).forEach(n => { const o = el('option', '', n); o.value = n; ps.appendChild(o); });
      ps.onchange = () => { if (!ps.value) return; Object.assign(f.p, FX_PRESETS[f.type][ps.value]); if (eng) eng.rebuildInsert(k0); rerender(); }; head.appendChild(ps);
    }
    if (f) {
      const pw = el('button', f.on !== false ? 'on' : '', '⏻'); pw.title = 'Activar / desactivar';
      pw.onclick = () => { f.on = f.on === false; if (eng) eng.rebuildInsert(k0); rerender(); }; head.appendChild(pw);
      const rm = el('button', '', '✕'); rm.title = 'Quitar el efecto'; rm.onclick = () => { ins.fx[k] = null; if (eng) eng.rebuildInsert(k0); rerender(); }; head.appendChild(rm);
    }
    slot.appendChild(head);
    if (f) FX_DEFS[f.type].params.forEach(d => slider(slot, d.n, d.min, d.max, f.p[d.k], d.step, v => { f.p[d.k] = v; const nodes = eng && eng.ins[k0].fx[k]; if (nodes) nodes.update(); autoRec('i' + k0 + '.f' + k + '.' + d.k, v); }, d.log, 'i' + k0 + '.f' + k + '.' + d.k));
    parent.appendChild(slot);
  }
}

function meterPaint(m, an) {
  const cv = m.cv, g = cv.getContext('2d'), buf = new Float32Array(an[0].fftSize);
  g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height);
  for (let ch = 0; ch < 2; ch++) {
    an[ch].getFloatTimeDomainData(buf); let pk = 0; for (let i = 0; i < buf.length; i++) pk = Math.max(pk, Math.abs(buf[i]));
    m.lv[ch] = Math.max(pk, m.lv[ch] * 0.88);
    const d = dbOf(m.lv[ch]), h = d === -Infinity ? 0 : Math.max(0, Math.min(1, (d + 60) / 66)) * cv.height;
    const gr = g.createLinearGradient(0, cv.height, 0, 0); gr.addColorStop(0, '#2f9e5e'); gr.addColorStop(0.72, '#d9d94a'); gr.addColorStop(0.9, '#ff5050');
    g.fillStyle = gr; g.fillRect(2 + ch * 10, cv.height - h, 8, h);
  }
  g.fillStyle = '#444'; for (let d = 0; d >= -60; d -= 12) { const y = cv.height - (d + 60) / 66 * cv.height; g.fillRect(0, y, 2, 1); g.fillRect(cv.width - 2, y, 2, 1); }
}
function mixerTick() {
  if (!eng) return;
  mixMeters.forEach(m => { const o = m.idx === -1 ? eng.masterM : m.idx <= -2 ? eng.ret[-2 - m.idx] : eng.ins[m.idx]; if (o) meterPaint(m, m.idx === -1 ? [o.anL, o.anR] : [o.anL, o.anR]); });
}

function renderGroups(panel) {
  panel.appendChild(el('h3', '', 'Grupos y VCA'));
  panel.appendChild(el('div', 'hint', 'Un grupo agrupa pistas: el VCA sube o baja el volumen de todas a la vez sin tocar sus faders, y M/S del grupo silencian o aíslan todas.'));
  song.groups.forEach((g, k) => {
    const box = el('div', 'slot used'), head = el('div', 'shead'), nm = el('input'); nm.type = 'text'; nm.value = g.name; nm.onchange = () => { g.name = nm.value; renderMixer(); };
    const bm = el('button', g.mute ? 'on' : '', 'M'), bs = el('button', g.solo ? 'on' : '', 'S'), dl = el('button', '', '✕');
    bm.onclick = () => { g.mute = !g.mute; if (eng) eng.applyMix(); renderMixer(); }; bs.onclick = () => { g.solo = !g.solo; if (eng) eng.applyMix(); renderMixer(); };
    dl.onclick = () => { song.groups.splice(k, 1); song.tracks.forEach(t => { if (t.group === k) t.group = -1; else if (t.group > k) t.group--; }); if (eng) eng.applyMix(); renderMixer(); };
    head.append(nm, bm, bs, dl); box.appendChild(head);
    slider(box, 'VCA', 0, 1.25, g.vca, 0.01, v => { g.vca = v; if (eng) eng.applyMix(); autoRec('g' + k + '.vca', v); }, false, 'g' + k + '.vca');
    const mem = el('div', 'members');
    song.tracks.forEach((t, i) => { const l = el('label', ''), c = el('input'); c.type = 'checkbox'; c.checked = t.group === k; c.onchange = () => { t.group = c.checked ? k : (t.group === k ? -1 : t.group); if (eng) eng.applyMix(); renderMixer(); }; l.append(c, ' ' + t.name); mem.appendChild(l); });
    box.appendChild(mem); panel.appendChild(box);
  });
  const add = el('button', '', '+ Nuevo grupo'); add.onclick = () => { song.groups.push({ name: 'Grupo ' + (song.groups.length + 1), vca: 1, mute: false, solo: false }); renderMixer(); }; panel.appendChild(add);
}

// ----------------------------------------------------------------- vista de dispositivos (estilo Ableton Live)
// Cadena horizontal de módulos: barra de título con encendido y plegado, perillas con su valor, reordenar arrastrando.
const FX_COLORS = { filter: '#7a8a99', delay: '#8a7aa6', reverb: '#6a8f8a', dist: '#a8705a', chorus: '#7a96b3', comp: '#8d9a6a', eq: '#a69a6a', peq: '#a69a6a', duck: '#9a6a7a' };
function fmtParam(d, v) {
  if (d.k === 'type') return v >= 0.5 ? 'HP' : 'LP';
  if (d.k === 'src') return 'Insert ' + Math.round(v);
  const n = d.n;
  if (/Hz/.test(n)) return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' kHz' : Math.round(v) + ' Hz';
  if (/dB/.test(n)) return (v > 0 ? '+' : '') + v.toFixed(1) + ' dB';
  if (/ms/.test(n)) return Math.round(v) + ' ms';
  if (d.k === 'time') return Math.round(v * 1000) + ' ms';
  if (d.k === 'depth' && d.max < 0.1) return (v * 1000).toFixed(1) + ' ms';
  if (d.max <= 1.01 && d.min >= -0.01) return Math.round(v * 100) + ' %';
  return v.toFixed(Math.abs(v) < 10 ? 2 : 1);
}
function deviceEl(f, o) {                                    // o: {k, s, update, rebuild, rerender, movable, removable, powerable}
  const col = FX_COLORS[f.type] || '#7a8a99', dev = el('div', 'dev2' + (f.fold ? ' fold' : '') + (f.on === false ? ' off' : '')); dev.style.setProperty('--dc', col);
  const head = el('div', 'dh'), pw = el('button', 'dpw' + (f.on !== false ? ' on' : ''), ''); pw.title = 'Activar / desactivar el dispositivo';
  const name = el('span', 'dn', FX_DEFS[f.type].name), fd = el('button', 'dfold', f.fold ? '▸' : '▾'); fd.title = 'Plegar / desplegar';
  const rm = el('button', 'dx', '✕'); rm.title = 'Quitar el dispositivo';
  if (o.powerable !== false) head.appendChild(pw); head.appendChild(name); head.appendChild(fd); if (o.removable !== false) head.appendChild(rm);
  pw.onclick = () => { f.on = f.on === false; o.rebuild(); o.rerender(); }; fd.onclick = () => { f.fold = !f.fold; o.rerender(); }; rm.onclick = () => o.remove();
  dev.appendChild(head);
  if (o.movable) {                                           // arrastrar para reordenar
    head.draggable = true; head.ondragstart = e => { e.dataTransfer.setData('xaza/dev', String(o.s)); dev.classList.add('drag'); }; head.ondragend = () => dev.classList.remove('drag');
    dev.ondragover = e => { if (e.dataTransfer.types.includes('xaza/dev')) { e.preventDefault(); dev.classList.add('over'); } }; dev.ondragleave = () => dev.classList.remove('over');
    dev.ondrop = e => { e.preventDefault(); const from = +e.dataTransfer.getData('xaza/dev'); if (isNaN(from) || from === o.s) return; o.move(from, o.s); };
  }
  if (f.fold) { dev.title = FX_DEFS[f.type].name; return dev; }
  const body = el('div', 'dbody'), P = FX_PRESETS[f.type];
  if (P) { const ps = el('select', 'dpre'); const p0 = el('option', '', 'Preset'); p0.value = ''; ps.appendChild(p0); Object.keys(P).forEach(n => { const q = el('option', '', n); q.value = n; ps.appendChild(q); });
    ps.onchange = () => { if (!ps.value) return; Object.assign(f.p, P[ps.value]); o.rebuild(); o.rerender(); }; body.appendChild(ps); }
  const knobs = el('div', 'dknobs');
  FX_DEFS[f.type].params.forEach(d => {
    const toVal = n => { let v = d.log ? d.min * Math.pow(d.max / d.min, n) : d.min + (d.max - d.min) * n; if (d.step === 1) v = Math.round(v); return v; };
    const toNorm = v => Math.max(0, Math.min(1, d.log ? Math.log(v / d.min) / Math.log(d.max / d.min) : (v - d.min) / (d.max - d.min)));
    const cell = el('div', 'dk'), val = el('div', 'dval', fmtParam(d, f.p[d.k]));
    const kb = makeKnob(d.n, 0, 1, toNorm(f.p[d.k]), toNorm(d.v), n => { const v = toVal(n); f.p[d.k] = v; val.textContent = fmtParam(d, v); o.update(); autoRec(o.target(d.k), v); }, 38, o.target(d.k));
    cell.append(kb.el, val, el('div', 'dlab', d.n)); knobs.appendChild(cell);
  });
  body.appendChild(knobs); dev.appendChild(body); return dev;
}
function renderDeviceView(dv) {
  dv.innerHTML = ''; const rr = () => renderMixer();
  const title = el('div', 'dvtitle');
  if (mixSel === -1) { title.innerHTML = '<b>MASTER</b><span>Limitador fijo −3 dB</span>'; dv.append(title); dv.appendChild(el('div', 'dvnote', 'El master lleva un limitador fijo para evitar saturación digital. Inserta efectos en cada insert o retorno.')); }
  else if (mixSel <= -2) {
    const k = -2 - mixSel, r = song.returns[k]; title.innerHTML = '<b>' + r.name + '</b><span>Retorno · salida 100% efecto</span>'; dv.appendChild(title);
    dv.appendChild(deviceEl(r.fx, { powerable: false, removable: false, movable: false, target: () => null, update: () => { if (eng && eng.ret[k]) eng.ret[k].fx.update(); }, rebuild: () => {}, rerender: rr, remove: () => {} }));
  } else {
    const k = mixSel, ins = song.inserts[k], routed = song.tracks.filter(t => t.insert === k).map(t => t.name);
    title.innerHTML = '<b>' + ins.name + '</b><span>' + (routed.length ? 'Recibe: ' + routed.join(', ') : 'Sin canales') + '</span>'; dv.appendChild(title);
    ins.fx.forEach((f, s) => {
      if (!f) return;
      dv.appendChild(deviceEl(f, { k, s, movable: true, target: pk => 'i' + k + '.f' + s + '.' + pk, update: () => { const n = eng && eng.ins[k].fx[s]; if (n) n.update(); }, rebuild: () => { if (eng) eng.rebuildInsert(k); }, rerender: rr,
        remove: () => { ins.fx[s] = null; if (eng) eng.rebuildInsert(k); rr(); },
        move: (from, to) => { const t = ins.fx[from]; ins.fx[from] = ins.fx[to]; ins.fx[to] = t; if (eng) eng.rebuildInsert(k); rr(); } }));
    });
    const free = ins.fx.findIndex(f => !f), add = el('div', 'dadd'), ab = el('button', '', free < 0 ? 'Cadena llena' : '＋ Dispositivo'); ab.disabled = free < 0; add.appendChild(ab);
    ab.onclick = e => { e.stopPropagation(); closeFxPop(); const m = el('div', 'ctxmenu'); Object.keys(FX_DEFS).forEach(key => { const b = el('button', 'mitem', FX_DEFS[key].name); b.onclick = () => { m.remove(); ins.fx[free] = fxNew(key); if (eng) eng.rebuildInsert(k); rr(); }; m.appendChild(b); }); placeNear(m, ab); onOutside(m); };
    dv.appendChild(add);
  }
  const gb = el('button', 'dgrp' + (MIXG ? ' on' : ''), 'Grupos / VCA'); gb.onclick = () => { MIXG = !MIXG; renderMixer(); }; dv.appendChild(gb);
}
