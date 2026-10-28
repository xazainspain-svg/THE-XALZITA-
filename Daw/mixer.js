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
  const strips = el('div', 'strips'), panel = el('div', 'fxpanel');
  root.append(strips, panel);

  const mkStrip = (idx, name, color, get, set, isMaster) => {
    const s = el('div', 'strip' + (mixSel === idx ? ' sel' : '') + (isMaster ? ' master' : '')); s.style.setProperty('--c', color);
    const nm = el('div', 'sname', name); nm.onclick = () => { mixSel = idx; renderMixer(); };
    if (!isMaster) nm.ondblclick = () => { const n = prompt('Nombre del insert', name); if (n) { song.inserts[idx].name = n; renderMixer(); } };
    s.appendChild(nm);
    if (!isMaster) {
      const used = song.inserts[idx].fx.filter(Boolean).length, dots = el('div', 'sdots', used ? used + ' fx' : '—'); s.appendChild(dots);
    } else s.appendChild(el('div', 'sdots', 'limitador'));
    const fw = el('div', 'sfader'), cv = el('canvas'); cv.width = 22; cv.height = 170;
    const f = el('input'); f.type = 'range'; f.min = 0; f.max = 1.25; f.step = 0.005; f.value = get(); f.className = 'fader';
    const db = el('div', 'sdb', dbTxt(get()));
    f.oninput = () => { set(+f.value); db.textContent = dbTxt(+f.value); if (eng) eng.applyMix(); autoRec(isMaster ? 'm.vol' : 'i' + idx + '.vol', +f.value); };
    f.ondblclick = () => { f.value = 0.8; f.oninput(); };
    fw.append(cv, f); s.append(fw, db);
    const mm = { cv, lv: [0, 0], idx: isMaster ? -1 : idx }; mixMeters.push(mm);
    if (!isMaster) {
      const ins = song.inserts[idx], pan = el('input'); pan.type = 'range'; pan.min = -1; pan.max = 1; pan.step = 0.01; pan.value = ins.pan; pan.title = 'Panorama';
      pan.oninput = () => { ins.pan = +pan.value; if (eng) eng.applyMix(); autoRec('i' + idx + '.pan', ins.pan); }; pan.ondblclick = () => { pan.value = 0; pan.oninput(); };
      const row = el('div', 'row'), bm = el('button', 'm' + (ins.mute ? ' on' : ''), 'M'), bs = el('button', 's' + (ins.solo ? ' on' : ''), 'S');
      bm.onclick = () => { ins.mute = !ins.mute; bm.classList.toggle('on', ins.mute); if (eng) eng.applyMix(); };
      bs.onclick = () => { ins.solo = !ins.solo; bs.classList.toggle('on', ins.solo); if (eng) eng.applyMix(); };
      const to = el('select'); to.title = 'Salida del insert'; const o0 = el('option', '', '→ Master'); o0.value = -1; to.appendChild(o0);
      for (let k = idx + 1; k < song.inserts.length; k++) { const o = el('option', '', '→ ' + (k + 1)); o.value = k; to.appendChild(o); }
      to.value = ins.to !== undefined && ins.to > idx ? ins.to : -1; to.onchange = () => { ins.to = +to.value; if (eng) eng.applyMix(); };
      row.append(bm, bs); s.append(pan, row, to);
      const routed = song.tracks.filter(t => t.insert === idx).map(t => t.name);
      s.appendChild(el('div', 'sroute', routed.length ? '← ' + routed.join(', ') : ''));
    }
    return s;
  };
  strips.appendChild(mkStrip(-1, 'MASTER', '#ffb400', () => song.masterVol, v => song.masterVol = v, true));
  song.inserts.forEach((ins, i) => strips.appendChild(mkStrip(i, ins.name, '#6d8cff', () => ins.vol, v => ins.vol = v, false)));
  song.returns.forEach((r, k) => {
    const idx = -2 - k, s = el('div', 'strip ret' + (mixSel === idx ? ' sel' : '')); s.style.setProperty('--c', '#35c9c9');
    const nm = el('div', 'sname', k ? 'Retorno B' : 'Retorno A'); nm.onclick = () => { mixSel = idx; renderMixer(); };
    s.append(nm, el('div', 'sdots', FX_DEFS[r.fx.type].name));
    const fw = el('div', 'sfader'), cv = el('canvas'); cv.width = 22; cv.height = 170;
    const f = el('input'); f.type = 'range'; f.min = 0; f.max = 1.25; f.step = 0.005; f.value = r.vol; f.className = 'fader';
    const db = el('div', 'sdb', dbTxt(r.vol)); f.oninput = () => { r.vol = +f.value; db.textContent = dbTxt(r.vol); if (eng) eng.applyMix(); autoRec('r' + k + '.vol', r.vol); };
    fw.append(cv, f); s.append(fw, db); mixMeters.push({ cv, lv: [0, 0], idx: idx }); strips.appendChild(s);
  });

  const tg = el('div', 'shead'); [['Efectos', false], ['Grupos / VCA', true]].forEach(([l, v]) => { const b = el('button', MIXG === v ? 'on' : '', l); b.onclick = () => { MIXG = v; renderMixer(); }; tg.appendChild(b); });
  panel.appendChild(tg); if (MIXG) { renderGroups(panel); return; }
  // ---- panel de ranuras de efectos
  if (mixSel <= -2) {
    const k = -2 - mixSel, r = song.returns[k]; panel.appendChild(el('h3', '', r.name));
    panel.appendChild(el('div', 'hint', 'Las pistas envían señal aquí con los potenciómetros "Envío" de la vista Session. Salida 100% efecto.'));
    const slot = el('div', 'slot used'); FX_DEFS[r.fx.type].params.forEach(d => slider(slot, d.n, d.min, d.max, r.fx.p[d.k], d.step, v => { r.fx.p[d.k] = v; if (eng && eng.ret[k]) eng.ret[k].fx.update(); }, d.log));
    panel.appendChild(slot); return;
  }
  if (mixSel < 0) { panel.appendChild(el('h3', '', 'Master')); panel.appendChild(el('div', 'hint', 'El master lleva un limitador fijo (-3 dB) para evitar saturación digital. Los efectos se insertan en cada insert.')); return; }
  const ins = song.inserts[mixSel];
  panel.appendChild(el('h3', '', ins.name + ' — ranuras de efectos'));
  const routed = song.tracks.map((t, i) => [t, i]).filter(x => x[0].insert === mixSel);
  panel.appendChild(el('div', 'hint', routed.length ? 'Recibe: ' + routed.map(x => x[0].name).join(', ') : 'Ninguna pista llega a este insert. Elige el insert de cada pista en la vista Session (selector "Insert").'));
  for (let k = 0; k < NSLOTS; k++) {
    const f = ins.fx[k], slot = el('div', 'slot' + (f ? ' used' : '')), head = el('div', 'shead');
    const sel = el('select'); const o0 = el('option', '', '— ranura ' + (k + 1) + ' —'); o0.value = ''; sel.appendChild(o0);
    Object.keys(FX_DEFS).forEach(key => { const o = el('option', '', FX_DEFS[key].name); o.value = key; sel.appendChild(o); });
    sel.value = f ? f.type : '';
    sel.onchange = () => { ins.fx[k] = sel.value ? fxNew(sel.value) : null; if (eng) eng.rebuildInsert(mixSel); renderMixer(); };
    head.appendChild(sel);
    if (f && FX_PRESETS[f.type]) {
      const ps = el('select'); const p0 = el('option', '', 'preset'); p0.value = ''; ps.appendChild(p0);
      Object.keys(FX_PRESETS[f.type]).forEach(n => { const o = el('option', '', n); o.value = n; ps.appendChild(o); });
      ps.onchange = () => { if (!ps.value) return; Object.assign(f.p, FX_PRESETS[f.type][ps.value]); if (eng) eng.rebuildInsert(mixSel); renderMixer(); }; head.appendChild(ps);
    }
    if (f) {
      const pw = el('button', f.on !== false ? 'on' : '', '⏻'); pw.title = 'Activar / desactivar';
      pw.onclick = () => { f.on = f.on === false; if (eng) eng.rebuildInsert(mixSel); renderMixer(); }; head.appendChild(pw);
    }
    slot.appendChild(head);
    if (f) FX_DEFS[f.type].params.forEach(d => slider(slot, d.n, d.min, d.max, f.p[d.k], d.step, v => { f.p[d.k] = v; const nodes = eng && eng.ins[mixSel].fx[k]; if (nodes) nodes.update(); autoRec('i' + mixSel + '.f' + k + '.' + d.k, v); }, d.log));
    panel.appendChild(slot);
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
    slider(box, 'VCA', 0, 1.25, g.vca, 0.01, v => { g.vca = v; if (eng) eng.applyMix(); autoRec('g' + k + '.vca', v); });
    const mem = el('div', 'members');
    song.tracks.forEach((t, i) => { const l = el('label', ''), c = el('input'); c.type = 'checkbox'; c.checked = t.group === k; c.onchange = () => { t.group = c.checked ? k : (t.group === k ? -1 : t.group); if (eng) eng.applyMix(); renderMixer(); }; l.append(c, ' ' + t.name); mem.appendChild(l); });
    box.appendChild(mem); panel.appendChild(box);
  });
  const add = el('button', '', '+ Nuevo grupo'); add.onclick = () => { song.groups.push({ name: 'Grupo ' + (song.groups.length + 1), vca: 1, mute: false, solo: false }); renderMixer(); }; panel.appendChild(add);
}
