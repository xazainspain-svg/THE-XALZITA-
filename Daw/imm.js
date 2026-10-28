'use strict';
// Mezcla inmersiva propia: objetos de audio 3D (posición x/y/z por pista), monitorización binaural con HRTF
// (usa auriculares) y exportación de la metadata de posiciones. NO usa tecnología, formatos ni licencias de Dolby.

const IMM = { size: 460, drag: null, cv: null, selSide: null };
const BED = [  // distribución de referencia 7.1.4 (solo guía visual): [nombre, x, y, altura]
  ['L', -0.55, 0.85, 0], ['C', 0, 1, 0], ['R', 0.55, 0.85, 0], ['Lss', -1, 0, 0], ['Rss', 1, 0, 0],
  ['Lrs', -0.8, -0.8, 0], ['Rrs', 0.8, -0.8, 0], ['Ltf', -0.5, 0.5, 1], ['Rtf', 0.5, 0.5, 1], ['Ltr', -0.5, -0.5, 1], ['Rtr', 0.5, -0.5, 1]];

function renderImm() {
  const root = $('#imm'); root.innerHTML = '';
  const left = el('div', 'immL'), right = el('div', 'immR'); root.append(left, right);
  left.appendChild(el('div', 'hint', 'Vista superior: tú estás en el centro mirando hacia arriba. Arrastra los objetos para colocarlos. Usa auriculares: la escucha es binaural (HRTF).'));
  const cv = el('canvas'); cv.width = cv.height = IMM.size; IMM.cv = cv; left.appendChild(cv);
  const lg = el('div', 'hint', 'Puntos blancos = distribución de referencia 7.1.4 (guía visual). Aviso: mezcla inmersiva propia; no es Dolby Atmos ni usa su tecnología.'); left.appendChild(lg);

  right.appendChild(el('h3', '', 'Objetos 3D'));
  song.tracks.forEach((t, i) => {
    const row = el('div', 'slot' + (t.obj.on ? ' used' : '')), head = el('div', 'shead');
    const cb = el('input'); cb.type = 'checkbox'; cb.checked = t.obj.on;
    cb.onchange = () => { t.obj.on = cb.checked; if (eng) eng.applyMix(); renderImm(); };
    const nm = el('b', '', t.name); nm.style.color = t.color; nm.style.flex = '1'; nm.onclick = () => { selTrack = i; IMM.selObj = i; renderImm(); };
    head.append(cb, nm); row.appendChild(head);
    if (t.obj.on) {
      const upd = () => { if (eng) eng.applyMix(); drawImm(); };
      slider(row, 'Izq ↔ Der', -1, 1, t.obj.x, 0.01, v => { t.obj.x = v; upd(); });
      slider(row, 'Atrás ↔ Frente', -1, 1, t.obj.y, 0.01, v => { t.obj.y = v; upd(); });
      slider(row, 'Altura', -1, 1, t.obj.z, 0.01, v => { t.obj.z = v; upd(); });
    }
    right.appendChild(row);
  });
  const ex = el('button', '', 'Exportar metadata de objetos (JSON)');
  ex.onclick = () => {
    const meta = { program: 'Xaza Live', note: 'Metadata propia de posiciones de objetos; no es ADM/Dolby.', bpm: song.bpm,
      objects: song.tracks.filter(t => t.obj.on).map(t => ({ name: t.name, x: t.obj.x, y: t.obj.y, z: t.obj.z })) };
    download(new Blob([JSON.stringify(meta, null, 2)], { type: 'application/json' }), 'objetos-3d.json'); status('Metadata exportada.');
  };
  right.appendChild(ex);
  bindImm(); drawImm();
}
const immPos = t => { const c = IMM.size / 2, R = c - 28; return { px: c + t.obj.x * R, py: c - t.obj.y * R }; };
function drawImm() {
  const cv = IMM.cv; if (!cv) return; const g = cv.getContext('2d'), S = IMM.size, c = S / 2, R = c - 28;
  g.fillStyle = '#121215'; g.fillRect(0, 0, S, S);
  g.strokeStyle = '#2d2d34'; [1, 0.66, 0.33].forEach(f => { g.beginPath(); g.arc(c, c, R * f, 0, 6.3); g.stroke(); });
  g.beginPath(); g.moveTo(c, 10); g.lineTo(c, S - 10); g.moveTo(10, c); g.lineTo(S - 10, c); g.stroke();
  g.fillStyle = '#888'; g.font = '10px sans-serif';
  BED.forEach(([n, x, y, h]) => { const px = c + x * R, py = c - y * R; g.globalAlpha = h ? 0.45 : 0.8; g.fillStyle = '#ddd'; g.beginPath(); g.arc(px, py, h ? 3 : 4, 0, 6.3); g.fill(); g.fillText(n, px + 6, py + 3); });
  g.globalAlpha = 1; g.fillStyle = '#ffb400'; g.beginPath(); g.arc(c, c, 6, 0, 6.3); g.fill(); g.fillText('tú', c + 9, c + 4);
  song.tracks.forEach((t, i) => {
    if (!t.obj.on) return; const { px, py } = immPos(t), r = 11 + t.obj.z * 5;
    g.fillStyle = t.color; g.globalAlpha = 0.9; g.beginPath(); g.arc(px, py, Math.max(6, r), 0, 6.3); g.fill(); g.globalAlpha = 1;
    g.strokeStyle = '#fff'; g.lineWidth = i === selTrack ? 2 : 1; g.stroke(); g.lineWidth = 1;
    g.fillStyle = '#111'; g.font = 'bold 10px sans-serif'; g.fillText(String(i + 1), px - 3, py + 3);
    g.fillStyle = '#ddd'; g.font = '10px sans-serif'; g.fillText(t.name + (t.obj.z ? ' ↑' + t.obj.z.toFixed(1) : ''), px + r + 3, py + 3);
  });
}
function bindImm() {
  const cv = IMM.cv, S = IMM.size, c = S / 2, R = c - 28;
  const pt = e => { const r = cv.getBoundingClientRect(); return { x: (e.clientX - r.left) * S / r.width, y: (e.clientY - r.top) * S / r.height }; };
  cv.onmousedown = e => {
    const p = pt(e); let best = null, bd = 18;
    song.tracks.forEach((t, i) => { if (!t.obj.on) return; const q = immPos(t), d = Math.hypot(q.px - p.x, q.py - p.y); if (d < bd) { bd = d; best = i; } });
    if (best !== null) { IMM.drag = best; selTrack = best; drawImm(); }
  };
  window.addEventListener('mousemove', e => {
    if (IMM.drag === null || view !== 'imm') return; const p = pt(e), t = song.tracks[IMM.drag];
    t.obj.x = Math.max(-1, Math.min(1, (p.x - c) / R)); t.obj.y = Math.max(-1, Math.min(1, (c - p.y) / R));
    if (eng) eng.applyMix(); drawImm();
  });
  window.addEventListener('mouseup', () => { if (IMM.drag !== null) { IMM.drag = null; if (view === 'imm') renderImm(); } });
}
function immTick() {}
