'use strict';
// Modelo de datos de Xaza Live (canción, pistas, inserts del mixer, clips, arrangement).

const COLORS = ['#ff6b6b', '#ffb400', '#f4e04d', '#4fd37a', '#35c9c9', '#6d8cff', '#b784ff', '#ff7ac8'];
const SCENES = 8, NINS = 16, NSLOTS = 10;

const clipNew = (bars = 1) => ({ bars, notes: [] });
const defInst = (o = {}) => Object.assign({ wave: 'sawtooth', cutoff: 3000, res: 1, atk: 0.01, rel: 0.2, w2: 'none', d2: 12, sub: 0, fe: 0, fd: 0.25 }, o);
const defFm = (o = {}) => Object.assign({ ratio: 2, idx: 3, fdec: 0.4, atk: 0.005, rel: 0.3 }, o);
const defSampler = (o = {}) => Object.assign({ id: null, root: 60, rel: 0.15, loop: false }, o);
const instFor = (kind, o) => kind === 'synth' ? defInst(o) : kind === 'fm' ? defFm(o) : kind === 'sampler' ? defSampler(o) : null;
function trackNew(name, kind, color, inst, insert) {
  return { name, kind, color, inst: instFor(kind, inst), insert: insert | 0, sends: [0, 0], preSend: [false, false], group: -1, obj: { on: false, x: 0, y: 1, z: 0 }, vol: 0.8, pan: 0, mute: false, solo: false,
    clips: Array(SCENES).fill(null) };
}
function insertNew(i) {
  return { name: 'Insert ' + (i + 1), vol: 0.8, pan: 0, mute: false, solo: false, fx: Array(NSLOTS).fill(null) };
}
function fxNew(type) {
  const p = {}; FX_DEFS[type].params.forEach(d => p[d.k] = d.v); return { type, p, on: true };
}
function defReturns() {
  const r = fxNew('reverb'), d = fxNew('delay'); r.p.mix = 1; d.p.mix = 1; d.p.fb = 0.45;
  return [{ name: 'Retorno A · Reverb', vol: 0.8, fx: r }, { name: 'Retorno B · Delay', vol: 0.8, fx: d }];
}
const nt = (s, l, n, v) => ({ s, l, n, v: v === undefined ? 0.8 : v });

function demoSong() {
  const T = [
    trackNew('Batería', 'drums', COLORS[0], null, 0), trackNew('Bajo', 'synth', COLORS[1], { cutoff: 700, res: 4, rel: 0.1 }, 1),
    trackNew('Lead', 'synth', COLORS[2], { wave: 'square', cutoff: 4500, rel: 0.25 }, 2),
    trackNew('Pad', 'synth', COLORS[3], { wave: 'triangle', atk: 0.4, rel: 0.8, cutoff: 2500 }, 3),
    trackNew('Teclas', 'synth', COLORS[4], { wave: 'sine' }, 4), trackNew('Percusión', 'drums', COLORS[5], null, 5),
    trackNew('Synth 2', 'synth', COLORS[6], null, 6), trackNew('Synth 3', 'synth', COLORS[7], null, 7)];
  const d1 = clipNew(), d2 = clipNew();
  [0, 8, 10].forEach(s => d1.notes.push(nt(s, 1, 0))); [4, 12].forEach(s => d1.notes.push(nt(s, 1, 1)));
  for (let s = 0; s < 16; s += 2) d1.notes.push(nt(s, 1, 2, s % 4 ? 0.5 : 0.8));
  [0, 6, 8, 11].forEach(s => d2.notes.push(nt(s, 1, 0))); [4, 12].forEach(s => d2.notes.push(nt(s, 1, 1)));
  [4, 12].forEach(s => d2.notes.push(nt(s, 1, 4))); for (let s = 0; s < 16; s += 2) d2.notes.push(nt(s, 1, s === 14 ? 3 : 2, 0.6));
  T[0].clips[0] = d1; T[0].clips[1] = d2;
  const b1 = clipNew(), b2 = clipNew();
  [[0, 2, 36], [3, 1, 36], [6, 2, 39], [8, 2, 34], [11, 1, 34], [12, 3, 31]].forEach(x => b1.notes.push(nt(x[0], x[1], x[2])));
  [[0, 1, 36], [2, 1, 36], [4, 2, 43], [8, 1, 34], [10, 1, 34], [12, 2, 41]].forEach(x => b2.notes.push(nt(x[0], x[1], x[2])));
  T[1].clips[0] = b1; T[1].clips[1] = b2;
  const l1 = clipNew(2);
  [72, 75, 79, 75, 72, 75, 79, 82, 79, 75, 72, 70, 72, 75, 79, 75].forEach((n, i) => l1.notes.push(nt(i * 2, 2, n, 0.7)));
  [84, 82, 79, 75].forEach((n, i) => l1.notes.push(nt(16 + i * 4, 3, n, 0.7)));
  T[2].clips[0] = l1;
  const p1 = clipNew(2);
  [[0, 60, 63, 67], [8, 58, 62, 65], [16, 56, 60, 63], [24, 58, 62, 65]].forEach(c => c.slice(1).forEach(n => p1.notes.push(nt(c[0], 8, n, 0.6))));
  T[3].clips[0] = p1;
  T[2].pan = 0.15; T[3].vol = 0.6;
  const inserts = Array.from({ length: NINS }, (_, i) => insertNew(i));
  T.forEach((t, i) => { inserts[i].name = t.name; });
  inserts[2].fx[0] = fxNew('delay'); inserts[3].fx[0] = fxNew('chorus'); inserts[3].fx[1] = fxNew('reverb');
  inserts[0].fx[0] = fxNew('comp');
  const arr = [], put = (t, from, to, slot) => { arr.push({ t, s: from * 16, l: (to - from) * 16, slot, off: 0 }); };
  put(0, 0, 8, 0); put(0, 8, 16, 1); put(1, 2, 16, 0); put(3, 0, 16, 0); put(2, 4, 16, 0);
  T[3].sends = [0.35, 0]; T[2].sends = [0, 0.25];
  return { groups: [], markers: [{ s: 0, name: 'Inicio' }, { s: 64, name: 'Estribillo' }], punch: { on: false, start: 0, end: 16, pre: 1 }, bpm: 118, swing: 0, masterVol: 0.9, tracks: T, inserts, returns: defReturns(), autos: [{ target: 't3.vol', pts: [{ s: 0, v: 0.16 }, { s: 64, v: 0.8 }, { s: 224, v: 0.8 }, { s: 256, v: 0.24 }] }], arr, loop: { on: false, start: 0, end: 64 } };
}

// Acepta proyectos antiguos (formato por compases / efectos por pista) y completa lo que falte.
function upgradeSong(s) {
  if (!s || !Array.isArray(s.tracks) || !s.tracks.length) throw new Error('Archivo no válido');
  s.bpm = s.bpm || 120; s.masterVol = s.masterVol === undefined ? 0.9 : s.masterVol;
  if (!Array.isArray(s.inserts)) {
    s.inserts = Array.from({ length: NINS }, (_, i) => insertNew(i));
    s.tracks.forEach((t, i) => { if (i < NINS) { s.inserts[i].name = t.name; (t.fx || []).slice(0, NSLOTS).forEach((f, k) => { s.inserts[i].fx[k] = Object.assign({ on: true }, f); }); } });
  }
  while (s.inserts.length < NINS) s.inserts.push(insertNew(s.inserts.length));
  s.inserts.forEach(x => { x.fx = (x.fx || []).concat(Array(NSLOTS).fill(null)).slice(0, NSLOTS); x.vol = x.vol === undefined ? 0.8 : x.vol; x.pan = x.pan || 0; });
  s.tracks.forEach((t, i) => {
    t.insert = t.insert === undefined ? Math.min(i, NINS - 1) : t.insert; delete t.fx;
    t.obj = t.obj || { on: false, x: 0, y: 1, z: 0 };
    if (t.autoVol && t.autoVol.length) { s.autos = s.autos || []; s.autos.push({ target: 't' + i + '.vol', pts: t.autoVol.map(p => ({ s: p.s, v: Math.max(0, Math.min(1, p.v / 1.25)) })) }); }
    if (t.autoPan && t.autoPan.length) { s.autos = s.autos || []; s.autos.push({ target: 't' + i + '.pan', pts: t.autoPan.map(p => ({ s: p.s, v: Math.max(0, Math.min(1, (p.v + 1) / 2)) })) }); }
    delete t.autoVol; delete t.autoPan; delete t.auto;
    if (!t.inst) t.inst = instFor(t.kind);
    t.clips = (t.clips || []).concat(Array(SCENES).fill(null)).slice(0, SCENES);
  });
  s.arr = (s.arr || []).map(it => it.bar !== undefined
    ? { t: it.t, s: it.bar * 16, l: ((s.tracks[it.t] && s.tracks[it.t].clips[it.slot] && s.tracks[it.t].clips[it.slot].bars) || 1) * 16, slot: it.slot, off: 0 }
    : it);
  s.autos = (s.autos || []).filter(a => a && a.target && Array.isArray(a.pts));
  s.loop = s.loop || { on: false, start: 0, end: 64 }; s.swing = s.swing || 0;
  if (!Array.isArray(s.returns) || s.returns.length < 2) s.returns = defReturns();
  s.tracks.forEach(t => { t.sends = t.sends || [0, 0]; t.preSend = t.preSend || [false, false]; t.group = t.group === undefined ? -1 : t.group; });
  s.groups = s.groups || []; s.markers = s.markers || []; s.punch = s.punch || { on: false, start: 0, end: 16, pre: 1 };
  return s;
}
