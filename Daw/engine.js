'use strict';
// Motor de audio de Xaza Live (Web Audio). Funciona con AudioContext y con OfflineAudioContext (exportar WAV).

const ARR_BARS = 64;
const AUDIO = {};   // id -> {buf: AudioBuffer, name}  (las muestras no se guardan en el JSON del proyecto)
const DRUM_NAMES = ['Bombo', 'Caja', 'Hi-hat', 'Hi-hat ab.', 'Palmas', 'Tom', 'Rim', 'Cencerro'];
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

const FX_DEFS = {
  filter: { name: 'Filtro', params: [
    { k: 'freq', n: 'Corte', min: 40, max: 18000, v: 2000, log: 1 },
    { k: 'q', n: 'Reson.', min: 0.1, max: 18, v: 1 },
    { k: 'type', n: 'LP / HP', min: 0, max: 1, v: 0, step: 1 }] },
  delay: { name: 'Delay', params: [
    { k: 'time', n: 'Tiempo', min: 0.02, max: 1, v: 0.3 },
    { k: 'fb', n: 'Feedback', min: 0, max: 0.9, v: 0.4 },
    { k: 'mix', n: 'Mezcla', min: 0, max: 1, v: 0.3 }] },
  reverb: { name: 'Reverb', params: [
    { k: 'mix', n: 'Mezcla', min: 0, max: 1, v: 0.3 }] },
  dist: { name: 'Distorsión', params: [
    { k: 'drive', n: 'Drive', min: 1, max: 50, v: 8 },
    { k: 'mix', n: 'Mezcla', min: 0, max: 1, v: 0.6 }] },
  chorus: { name: 'Chorus', params: [
    { k: 'rate', n: 'Velocidad', min: 0.1, max: 8, v: 1.2 },
    { k: 'depth', n: 'Profund.', min: 0, max: 0.01, v: 0.004 },
    { k: 'mix', n: 'Mezcla', min: 0, max: 1, v: 0.5 }] },
  comp: { name: 'Compresor', params: [
    { k: 'thr', n: 'Umbral dB', min: -60, max: 0, v: -24 },
    { k: 'ratio', n: 'Ratio', min: 1, max: 20, v: 4 }] },
  eq: { name: 'EQ 3 bandas', params: [
    { k: 'low', n: 'Graves dB', min: -18, max: 18, v: 0 },
    { k: 'mid', n: 'Medios dB', min: -18, max: 18, v: 0 },
    { k: 'high', n: 'Agudos dB', min: -18, max: 18, v: 0 }] },
};

FX_DEFS.peq = { name: 'EQ paramétrico', params: [
  { k: 'f1', n: 'Graves Hz', min: 30, max: 500, v: 100, log: 1 }, { k: 'g1', n: 'Graves dB', min: -18, max: 18, v: 0 },
  { k: 'f2', n: 'Medio 1 Hz', min: 200, max: 3000, v: 500, log: 1 }, { k: 'g2', n: 'Medio 1 dB', min: -18, max: 18, v: 0 }, { k: 'q2', n: 'Medio 1 Q', min: 0.2, max: 10, v: 1 },
  { k: 'f3', n: 'Medio 2 Hz', min: 1000, max: 10000, v: 3000, log: 1 }, { k: 'g3', n: 'Medio 2 dB', min: -18, max: 18, v: 0 }, { k: 'q3', n: 'Medio 2 Q', min: 0.2, max: 10, v: 1 },
  { k: 'f4', n: 'Agudos Hz', min: 3000, max: 16000, v: 8000, log: 1 }, { k: 'g4', n: 'Agudos dB', min: -18, max: 18, v: 0 }] };
FX_DEFS.duck = { name: 'Sidechain (ducking)', params: [
  { k: 'src', n: 'Fuente (insert)', min: 1, max: 16, v: 1, step: 1 }, { k: 'depth', n: 'Profundidad', min: 0, max: 1, v: 0.8 }, { k: 'rel', n: 'Release ms', min: 30, max: 600, v: 150 }] };
const FX_PRESETS = {
  reverb: { 'Sala': { mix: 0.2 }, 'Catedral': { mix: 0.6 } },
  delay: { 'Eco 1/8': { time: 0.25, fb: 0.35, mix: 0.3 }, 'Eco largo': { time: 0.5, fb: 0.55, mix: 0.35 }, 'Slapback': { time: 0.09, fb: 0.1, mix: 0.3 } },
  dist: { 'Suave': { drive: 4, mix: 0.4 }, 'Fuzz': { drive: 35, mix: 0.8 } },
  filter: { 'Paso bajo': { freq: 800, q: 1, type: 0 }, 'Paso alto': { freq: 300, q: 1, type: 1 }, 'Resonante': { freq: 1200, q: 10, type: 0 } },
  comp: { 'Suave': { thr: -18, ratio: 2.5 }, 'Aplastar': { thr: -32, ratio: 12 } },
  eq: { 'Brillo': { low: -2, mid: 0, high: 5 }, 'Calidez': { low: 4, mid: 1, high: -2 } },
  chorus: { 'Ancho': { rate: 0.8, depth: 0.006, mix: 0.5 }, 'Lento': { rate: 0.3, depth: 0.008, mix: 0.4 } },
  peq: { 'Voz clara': { g1: -4, f1: 120, g2: 2, f2: 800, q2: 1, g3: 3, f3: 4000, q3: 1.2, g4: 2, f4: 10000 }, 'Bajo grueso': { g1: 5, f1: 80, g2: -2, f2: 400, q2: 1, g3: 2, f3: 2000, q3: 1, g4: 0, f4: 8000 } },
  duck: { 'Bombo (pump)': { src: 1, depth: 0.85, rel: 180 }, 'Sutil': { src: 1, depth: 0.4, rel: 120 } },
};

const midiFreq = n => 440 * Math.pow(2, (n - 69) / 12);
const noteName = n => NOTE_NAMES[n % 12] + (Math.floor(n / 12) - 1);

function makeNoiseBuf(ctx) {
  const b = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate), d = b.getChannelData(0);
  for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  return b;
}
function makeIR(ctx, sec = 2.2, decay = 2.8) {
  const n = Math.floor(ctx.sampleRate * sec), b = ctx.createBuffer(2, n, ctx.sampleRate);
  for (let c = 0; c < 2; c++) {
    const d = b.getChannelData(c);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / n, decay);
  }
  return b;
}
function distCurve(k) {
  const n = 1024, c = new Float32Array(n);
  for (let i = 0; i < n; i++) { const x = (i / (n - 1)) * 2 - 1; c[i] = Math.tanh(k * x) / Math.tanh(k); }
  return c;
}

// Cada efecto: {in, out, update(), stop()}; p es el objeto de parámetros vivo guardado en la canción.
function makeFx(ctx, type, p, eng) {
  const I = ctx.createGain(), O = ctx.createGain(), extra = [];
  let upd = () => {};
  const mixer = () => { const w = ctx.createGain(), d = ctx.createGain(); d.connect(O); w.connect(O); return { w, d }; };
  switch (type) {
    case 'filter': {
      const f = ctx.createBiquadFilter(); I.connect(f); f.connect(O);
      upd = () => { f.type = p.type >= 0.5 ? 'highpass' : 'lowpass'; f.frequency.value = p.freq; f.Q.value = p.q; };
      break;
    }
    case 'delay': {
      const d = ctx.createDelay(1.5), fb = ctx.createGain(), m = mixer();
      I.connect(m.d); I.connect(d); d.connect(fb); fb.connect(d); d.connect(m.w);
      upd = () => { d.delayTime.value = p.time; fb.gain.value = p.fb; m.w.gain.value = p.mix; m.d.gain.value = p.ret ? 0 : 1; };
      break;
    }
    case 'reverb': {
      const c = ctx.createConvolver(), m = mixer(); c.buffer = eng.ir;
      I.connect(m.d); I.connect(c); c.connect(m.w);
      upd = () => { m.w.gain.value = p.mix * 1.5; m.d.gain.value = p.ret ? 0 : 1 - p.mix * 0.4; };
      break;
    }
    case 'dist': {
      const pre = ctx.createGain(), ws = ctx.createWaveShaper(), post = ctx.createGain(), m = mixer();
      ws.oversample = '2x'; I.connect(m.d); I.connect(pre); pre.connect(ws); ws.connect(post); post.connect(m.w);
      let last = -1;
      upd = () => { if (p.drive !== last) { ws.curve = distCurve(p.drive); last = p.drive; }
        post.gain.value = 0.6 * p.mix; m.d.gain.value = 1 - p.mix; };
      break;
    }
    case 'chorus': {
      const d = ctx.createDelay(0.1), lfo = ctx.createOscillator(), lg = ctx.createGain(), m = mixer();
      d.delayTime.value = 0.02; lfo.connect(lg); lg.connect(d.delayTime); lfo.start(); extra.push(lfo);
      I.connect(m.d); I.connect(d); d.connect(m.w);
      upd = () => { lfo.frequency.value = p.rate; lg.gain.value = p.depth; m.w.gain.value = p.mix; m.d.gain.value = 1 - p.mix * 0.5; };
      break;
    }
    case 'comp': {
      const c = ctx.createDynamicsCompressor(); c.knee.value = 6; I.connect(c); c.connect(O);
      upd = () => { c.threshold.value = p.thr; c.ratio.value = p.ratio; };
      break;
    }
    case 'peq': {
      const mk = (type, fr, q) => { const b = ctx.createBiquadFilter(); b.type = type; b.frequency.value = fr; b.Q.value = q; return b; };
      const b1 = mk('lowshelf', 100, 0.7), b2 = mk('peaking', 500, 1), b3 = mk('peaking', 3000, 1), b4 = mk('highshelf', 8000, 0.7);
      I.connect(b1); b1.connect(b2); b2.connect(b3); b3.connect(b4); b4.connect(O);
      upd = () => { b1.frequency.value = p.f1; b1.gain.value = p.g1; b2.frequency.value = p.f2; b2.gain.value = p.g2; b2.Q.value = p.q2;
        b3.frequency.value = p.f3; b3.gain.value = p.g3; b3.Q.value = p.q3; b4.frequency.value = p.f4; b4.gain.value = p.g4; };
      break;
    }
    case 'duck': {        // sidechain: baja este insert según el nivel de otro (se mide con un analizador cada 16 ms)
      const g = ctx.createGain(), buf = new Float32Array(512); let env = 0; I.connect(g); g.connect(O);
      if (typeof OfflineAudioContext === 'undefined' || !(ctx instanceof OfflineAudioContext)) {
        const timer = setInterval(() => {
          const src = eng.ins && eng.ins[Math.max(0, Math.min(15, Math.round(p.src) - 1))]; if (!src) return;
          src.anL.getFloatTimeDomainData(buf); let pk = 0; for (let i = 0; i < buf.length; i++) pk = Math.max(pk, Math.abs(buf[i]));
          env = Math.max(pk, env * Math.exp(-16 / Math.max(10, p.rel)));
          g.gain.setTargetAtTime(1 - p.depth * Math.min(1, env * 4), ctx.currentTime, 0.008);
        }, 16);
        extra.push({ stop() { clearInterval(timer); } });
      }
      break;
    }
    case 'eq': {
      const lo = ctx.createBiquadFilter(), mi = ctx.createBiquadFilter(), hi = ctx.createBiquadFilter();
      lo.type = 'lowshelf'; lo.frequency.value = 200; mi.type = 'peaking'; mi.frequency.value = 1000; mi.Q.value = 1;
      hi.type = 'highshelf'; hi.frequency.value = 4000;
      I.connect(lo); lo.connect(mi); mi.connect(hi); hi.connect(O);
      upd = () => { lo.gain.value = p.low; mi.gain.value = p.mid; hi.gain.value = p.high; };
      break;
    }
  }
  upd();
  return { in: I, out: O, update: upd, stop() { extra.forEach(o => { try { o.stop(); } catch (e) {} }); } };
}

// ---- audio: buffer derivado (recorte + invertir + estirar al tempo) con caché
function sliceBuf(buf, start, end, rev) {
  const sr = buf.sampleRate, i0 = Math.max(0, Math.floor(start * sr)), i1 = Math.min(buf.length, Math.floor(end * sr)), len = Math.max(1, i1 - i0);
  const out = new AudioBuffer({ length: len, sampleRate: sr, numberOfChannels: buf.numberOfChannels });
  for (let c = 0; c < buf.numberOfChannels; c++) { const src = buf.getChannelData(c), dst = out.getChannelData(c); for (let i = 0; i < len; i++) dst[i] = src[rev ? i1 - 1 - i : i0 + i]; }
  return out;
}
function stretchBuf(buf, ratio) {            // estiramiento granular (mantiene el tono); ratio = duración de salida / entrada
  const sr = buf.sampleRate, G = Math.max(256, Math.round(0.06 * sr)), Hs = G >> 2, Ha = Hs / ratio, outLen = Math.max(1, Math.round(buf.length * ratio));
  const out = new AudioBuffer({ length: outLen, sampleRate: sr, numberOfChannels: buf.numberOfChannels }), win = new Float32Array(G);
  for (let i = 0; i < G; i++) win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / G);
  for (let c = 0; c < buf.numberOfChannels; c++) {
    const src = buf.getChannelData(c), dst = out.getChannelData(c);
    for (let k = 0, o = 0; o < outLen; k++, o = k * Hs) {
      const p = Math.round(k * Ha); if (p >= src.length) break;
      for (let i = 0; i < G && o + i < outLen && p + i < src.length; i++) dst[o + i] += src[p + i] * win[i] / 2;
    }
  }
  return out;
}
function audioBuffer(au, bpm) {
  const a = AUDIO[au.id]; if (!a) return null;
  const start = au.start || 0, end = au.end || a.buf.duration, ratio = au.warp ? (au.ob || 120) / bpm : 1;
  const key = [start.toFixed(3), end.toFixed(3), au.rev ? 1 : 0, ratio.toFixed(3)].join('|'); a.cache = a.cache || {};
  if (!a.cache[key]) {
    const keys = Object.keys(a.cache); if (keys.length > 6) delete a.cache[keys[0]];
    let b = (start > 0 || au.end || au.rev) ? sliceBuf(a.buf, start, end, au.rev) : a.buf;
    a.cache[key] = Math.abs(ratio - 1) > 0.01 ? stretchBuf(b, ratio) : b;
  }
  return a.cache[key];
}

// ---- automatización de cualquier parámetro
function autoInfo(id, S) {
  let m;
  if (id === 'm.vol') return { label: 'Master · volumen', min: 0, max: 1.25, get: () => S.masterVol, set: v => { S.masterVol = v; } };
  if (id === 'bpm') return { label: 'Tempo (BPM)', min: 40, max: 240, js: true, get: () => S.bpm, set: v => { S.bpm = v; } };
  if ((m = /^t(\d+)\.(vol|pan|sa|sb|cutoff|res)$/.exec(id))) {
    const i = +m[1], t = S.tracks[i]; if (!t) return null; const k = m[2], nm = t.name;
    if (k === 'vol') return { label: nm + ' · volumen', min: 0, max: 1.25, get: () => t.vol };
    if (k === 'pan') return { label: nm + ' · panorama', min: -1, max: 1, get: () => t.pan };
    if (k === 'sa' || k === 'sb') return { label: nm + ' · envío ' + (k === 'sa' ? 'A' : 'B'), min: 0, max: 1, get: () => (t.sends || [0, 0])[k === 'sa' ? 0 : 1] };
    if (!t.inst) return null;
    return k === 'cutoff' ? { label: nm + ' · corte del filtro', min: 100, max: 12000, log: 1, js: true, get: () => t.inst.cutoff, set: v => { t.inst.cutoff = v; } }
                          : { label: nm + ' · resonancia', min: 0.1, max: 18, js: true, get: () => t.inst.res, set: v => { t.inst.res = v; } };
  }
  if ((m = /^g(\d+)\.vca$/.exec(id))) { const g = S.groups[+m[1]]; if (!g) return null; return { label: g.name + ' · VCA', min: 0, max: 1.25, js: true, vca: true, get: () => g.vca, set: v => { g.vca = v; } }; }
  if ((m = /^i(\d+)\.(vol|pan)$/.exec(id))) { const x = S.inserts[+m[1]]; if (!x) return null; return m[2] === 'vol' ? { label: x.name + ' · volumen', min: 0, max: 1.25, get: () => x.vol } : { label: x.name + ' · panorama', min: -1, max: 1, get: () => x.pan }; }
  if ((m = /^r(\d+)\.vol$/.exec(id))) { const r = S.returns[+m[1]]; if (!r) return null; return { label: r.name + ' · volumen', min: 0, max: 1.25, get: () => r.vol }; }
  if ((m = /^i(\d+)\.f(\d+)\.(\w+)$/.exec(id))) {
    const x = S.inserts[+m[1]], f = x && x.fx[+m[2]]; if (!f) return null; const d = FX_DEFS[f.type].params.find(q => q.k === m[3]); if (!d) return null;
    return { label: x.name + ' · ' + FX_DEFS[f.type].name + ' · ' + d.n, min: d.min, max: d.max, log: d.log, js: true, fx: [+m[1], +m[2]], get: () => f.p[d.k], set: v => { f.p[d.k] = v; } };
  }
  return null;
}
function autoTargets(S) {                    // lista agrupada para el selector
  const g = [{ group: 'General', items: [['m.vol', 'Master · volumen'], ['bpm', 'Tempo (BPM)']] }];
  S.tracks.forEach((t, i) => {
    const it = [['t' + i + '.vol', 'volumen'], ['t' + i + '.pan', 'panorama'], ['t' + i + '.sa', 'envío A'], ['t' + i + '.sb', 'envío B']];
    if (t.inst && t.inst.cutoff !== undefined) it.push(['t' + i + '.cutoff', 'corte del filtro'], ['t' + i + '.res', 'resonancia']);
    g.push({ group: 'Pista · ' + t.name, items: it });
  });
  S.inserts.forEach((x, k) => {
    const it = [['i' + k + '.vol', 'volumen'], ['i' + k + '.pan', 'panorama']];
    x.fx.forEach((f, sl) => { if (f) FX_DEFS[f.type].params.forEach(d => it.push(['i' + k + '.f' + sl + '.' + d.k, FX_DEFS[f.type].name + ' · ' + d.n])); });
    if (it.length > 2 || x.vol !== 0.8 || S.tracks.some(t => t.insert === k)) g.push({ group: 'Insert · ' + x.name, items: it });
  });
  S.returns.forEach((r, k) => g.push({ group: r.name, items: [['r' + k + '.vol', 'volumen']] }));
  if (S.groups.length) g.push({ group: 'Grupos / VCA', items: S.groups.map((x, k) => ['g' + k + '.vca', x.name + ' · VCA']) });
  return g;
}
const autoMap = (T, v) => T.log ? T.min * Math.pow(T.max / T.min, v) : T.min + (T.max - T.min) * v;
const autoNorm = (T, x) => Math.max(0, Math.min(1, T.log ? Math.log(x / T.min) / Math.log(T.max / T.min) : (x - T.min) / (T.max - T.min)));

// valor de una envolvente [{s, v}] (ordenada por s) en el paso x; interpola linealmente
function envAt(env, x, def) {
  if (!env || !env.length) return def;
  if (x <= env[0].s) return env[0].v;
  for (let i = 1; i < env.length; i++) if (x <= env[i].s) {
    const a = env[i - 1], b = env[i]; return b.s === a.s ? b.v : a.v + (b.v - a.v) * (x - a.s) / (b.s - a.s);
  }
  return env[env.length - 1].v;
}

class Engine {
  constructor(ctx, song) {
    this.ctx = ctx; this.song = song;
    this.noise = makeNoiseBuf(ctx); this.ir = makeIR(ctx);
    this.master = ctx.createGain();
    this.limiter = ctx.createDynamicsCompressor();
    this.limiter.threshold.value = -3; this.limiter.knee.value = 0; this.limiter.ratio.value = 20;
    this.limiter.attack.value = 0.003; this.limiter.release.value = 0.1;
    this.masterM = this.meterTap(this.limiter);
    this.master.connect(this.limiter); this.limiter.connect(ctx.destination);
    this.masterAn = this.masterM.anL;
    this.spec = ctx.createAnalyser(); this.spec.fftSize = 1024; this.spec.smoothingTimeConstant = 0.8; this.limiter.connect(this.spec);
    this.srcs = new Set();
    this.ret = (song.returns || []).map(r => this.buildReturn(r));
    this.ins = song.inserts.map(ins => this.buildInsert(ins));
    this.tr = song.tracks.map(t => this.buildTrack(t));
    this.rt = song.tracks.map(() => ({ playing: -1, queued: undefined, launch: 0 }));
    this.playing = false; this.metro = false; this.log = []; this.mode = 'session';
    this.applyMix();
  }
  dispose() { this.stop(true); try { this.master.disconnect(); } catch (e) {} }

  // medidor estéreo (dos analizadores) colgado de un nodo
  meterTap(node) {
    const c = this.ctx, sp = c.createChannelSplitter(2), anL = c.createAnalyser(), anR = c.createAnalyser();
    anL.fftSize = anR.fftSize = 512; node.connect(sp); sp.connect(anL, 0); sp.connect(anR, 1);
    return { anL, anR, split: sp };
  }
  buildReturn(r) {
    const c = this.ctx, o = { in: c.createGain(), vol: c.createGain() };
    r.fx.p.ret = true; o.fx = makeFx(c, r.fx.type, r.fx.p, this);
    o.in.connect(o.fx.in); o.fx.out.connect(o.vol); o.vol.connect(this.master); Object.assign(o, this.meterTap(o.vol)); return o;
  }
  buildTrack(t) {
    const c = this.ctx, o = { in: c.createGain(), vol: c.createGain(), pan: c.createStereoPanner(), an: c.createAnalyser(), sends: [] };
    o.an.fftSize = 512; o.in.connect(o.vol); o.vol.connect(o.pan); o.pan.connect(o.an);
    this.ret.forEach(r => { const g = c.createGain(); g.gain.value = 0; g.connect(r.in); o.sends.push(g); });
    this.routeTrack(o, t); return o;
  }
  routeTrack(o, t) {
    const idx = Math.max(0, Math.min(this.ins.length - 1, t.insert | 0)), obj = !!(t.obj && t.obj.on), pre = t.preSend || [false, false], key = idx + ':' + obj + ':' + pre.join();
    if (o.routeKey !== key) {
      o.routeKey = key; try { o.pan.disconnect(); } catch (e) {}
      o.pan.connect(o.an); o.sends.forEach((g, k) => { try { o.in.disconnect(g); } catch (e) {} (pre[k] ? o.in : o.pan).connect(g); });
      if (obj) {
        if (!o.pn) {
          o.pn = this.ctx.createPanner(); o.pn.panningModel = 'HRTF'; o.pn.distanceModel = 'inverse'; o.pn.refDistance = 1; o.pn.rolloffFactor = 0.8;
          o.pg = this.ctx.createGain(); o.pg.gain.value = 1.7; o.pn.connect(o.pg);
        }
        try { o.pg.disconnect(); } catch (e) {}
        o.pan.connect(o.pn); o.pg.connect(this.ins[idx].in);
      } else o.pan.connect(this.ins[idx].in);
    }
    if (obj && o.pn) {                     // x: izquierda-derecha, y: atrás-adelante, z: altura (el oyente mira hacia -z)
      const p = o.pn, X = t.obj.x * 2.5, Y = t.obj.z * 2.5, Z = -t.obj.y * 2.5;
      if (p.positionX) { p.positionX.value = X; p.positionY.value = Y; p.positionZ.value = Z; } else p.setPosition(X, Y, Z);
    }
  }
  buildInsert(ins) {
    const c = this.ctx, o = { in: c.createGain(), vol: c.createGain(), pan: c.createStereoPanner(), fx: [] };
    o.vol.connect(o.pan); o.pan.connect(this.master); Object.assign(o, this.meterTap(o.pan)); o.routeKey = 'm';
    this.rebuild(o, ins); return o;
  }
  rebuild(o, ins) {
    o.in.disconnect();
    o.fx.forEach(f => { if (f) { try { f.out.disconnect(); f.stop(); } catch (e) {} } });
    o.fx = ins.fx.map(x => (x && x.on !== false) ? makeFx(this.ctx, x.type, x.p, this) : null);
    let prev = o.in;
    o.fx.forEach(f => { if (f) { prev.connect(f.in); prev = f.out; } });
    prev.connect(o.vol);
  }
  rebuildInsert(k) { this.rebuild(this.ins[k], this.song.inserts[k]); }

  grp(t) { return t.group >= 0 && this.song.groups ? this.song.groups[t.group] : null; }
  anySolo() { return this.song.tracks.some(t => t.solo || (this.grp(t) && this.grp(t).solo)); }
  baseGain(t, anySolo) { const g = this.grp(t); return ((t.mute || (g && g.mute)) || (anySolo && !(t.solo || (g && g.solo)))) ? 0 : t.vol * (g ? g.vca : 1); }
  applyMix() {
    const now = this.ctx.currentTime, S = this.song, anySolo = this.anySolo(), insSolo = S.inserts.some(x => x.solo);
    S.tracks.forEach((t, i) => {
      const o = this.tr[i], g = o.vol.gain;
      g.cancelScheduledValues(now); g.setValueAtTime(this.baseGain(t, anySolo), now);
      o.pan.pan.setValueAtTime(t.pan, now); this.routeTrack(o, t);
      o.sends.forEach((g, k) => g.gain.setValueAtTime((t.sends && t.sends[k]) || 0, now));
    });
    this.ret.forEach((o, k) => o.vol.gain.setValueAtTime(S.returns[k].vol, now));
    S.inserts.forEach((x, k) => {
      const o = this.ins[k], to = (x.to !== undefined && x.to > k && x.to < this.ins.length) ? x.to : -1, key = String(to);
      if (o.routeKey !== key) { o.routeKey = key; try { o.pan.disconnect(); } catch (e) {} o.pan.connect(to < 0 ? this.master : this.ins[to].in); o.pan.connect(o.split); }
      o.vol.gain.setValueAtTime((x.mute || (insSolo && !x.solo)) ? 0 : x.vol, now); o.pan.pan.setValueAtTime(x.pan, now);
    });
    this.master.gain.setValueAtTime(S.masterVol === undefined ? 0.9 : S.masterVol, now);
  }

  // ---- instrumentos
  note(t, i, n, when, dur, vel, ex) {
    let dest = this.tr[i].in;
    if (ex && ex.pan) { const p = this.ctx.createStereoPanner(); p.pan.value = Math.max(-1, Math.min(1, ex.pan)); p.connect(dest); dest = p; }
    const fine = (ex && ex.fine) || 0;
    if (t.kind === 'drums') this.drum(n % 8, when, vel, dest);
    else if (t.kind === 'fm') this.fm(t.inst, n, when, dur, vel, dest, fine);
    else if (t.kind === 'sampler') this.sampler(t.inst, n, when, dur, vel, dest, fine);
    else this.synth(t.inst, n, when, dur, vel, dest, fine);
  }
  synth(inst, n, when, dur, vel, dest, fine = 0) {
    const c = this.ctx, f = midiFreq(n), flt = c.createBiquadFilter(), env = c.createGain();
    flt.type = 'lowpass'; flt.Q.value = inst.res;
    const a = Math.max(0.003, inst.atk), peak = 0.2 * vel, end = when + Math.max(dur, a), stop = end + inst.rel;
    if (inst.fe > 0) {                                  // envolvente de filtro
      const top = Math.min(18000, inst.cutoff * Math.pow(2, inst.fe * 4));
      flt.frequency.setValueAtTime(top, when); flt.frequency.exponentialRampToValueAtTime(Math.max(40, inst.cutoff), when + Math.max(0.02, inst.fd || 0.25));
    } else flt.frequency.value = inst.cutoff;
    env.gain.setValueAtTime(0, when); env.gain.linearRampToValueAtTime(peak, when + a);
    env.gain.setValueAtTime(peak, end); env.gain.linearRampToValueAtTime(0, stop);
    flt.connect(env); env.connect(dest);
    const osc = (type, freq, det) => { const o = c.createOscillator(); o.type = type; o.frequency.value = freq; o.detune.value = det + fine; o.connect(flt); o.start(when); o.stop(stop + 0.05); };
    osc(inst.wave, f, -7); osc(inst.wave, f, 7);
    if (inst.w2 && inst.w2 !== 'none') osc(inst.w2, f, inst.d2 === undefined ? 12 : inst.d2);
    if (inst.sub > 0) { const so = c.createOscillator(), sg = c.createGain(); so.type = 'sine'; so.frequency.value = f / 2; so.detune.value = fine; sg.gain.value = inst.sub; so.connect(sg); sg.connect(env); so.start(when); so.stop(stop + 0.05); }
  }
  fm(inst, n, when, dur, vel, dest, fine = 0) {
    const c = this.ctx, f = midiFreq(n), car = c.createOscillator(), mod = c.createOscillator(), mg = c.createGain(), env = c.createGain();
    const a = Math.max(0.003, inst.atk), peak = 0.25 * vel, end = when + Math.max(dur, a), stop = end + inst.rel;
    car.frequency.value = f; mod.frequency.value = f * inst.ratio; car.detune.value = fine; mod.detune.value = fine;
    mg.gain.setValueAtTime(inst.idx * f, when); mg.gain.exponentialRampToValueAtTime(Math.max(0.01, inst.idx * f * 0.05), when + Math.max(0.03, inst.fdec));
    mod.connect(mg); mg.connect(car.frequency); car.connect(env); env.connect(dest);
    env.gain.setValueAtTime(0, when); env.gain.linearRampToValueAtTime(peak, when + a); env.gain.setValueAtTime(peak, end); env.gain.linearRampToValueAtTime(0, stop);
    car.start(when); mod.start(when); car.stop(stop + 0.05); mod.stop(stop + 0.05);
  }
  sampler(inst, n, when, dur, vel, dest, fine = 0) {
    const a = inst.id && AUDIO[inst.id]; if (!a) return;
    const c = this.ctx, src = c.createBufferSource(), env = c.createGain(), end = when + Math.max(dur, 0.02), stop = end + (inst.rel || 0.15);
    src.buffer = a.buf; src.loop = !!inst.loop; src.playbackRate.value = Math.pow(2, (n - (inst.root || 60) + fine / 100) / 12);
    env.gain.setValueAtTime(0.9 * vel, when); if (inst.loop || dur > 0) { env.gain.setValueAtTime(0.9 * vel, end); env.gain.linearRampToValueAtTime(0, stop); }
    src.connect(env); env.connect(dest); src.start(when); src.stop(stop + 0.05);
  }
  hit(dest, when, peak, decay) {
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(peak, when); g.gain.exponentialRampToValueAtTime(0.001, when + decay);
    g.connect(dest); return g;
  }
  noiseSrc(when, len, dest, type, freq, q) {
    const c = this.ctx, s = c.createBufferSource(), f = c.createBiquadFilter();
    s.buffer = this.noise; f.type = type; f.frequency.value = freq; if (q) f.Q.value = q;
    s.connect(f); f.connect(dest); s.start(when, Math.random() * 0.5); s.stop(when + len);
  }
  drum(d, when, vel, dest) {
    const c = this.ctx, v = vel;
    switch (d) {
      case 0: { // bombo
        const o = c.createOscillator(), g = this.hit(dest, when, 0.9 * v, 0.4);
        o.frequency.setValueAtTime(150, when); o.frequency.exponentialRampToValueAtTime(42, when + 0.12);
        o.connect(g); o.start(when); o.stop(when + 0.45); break; }
      case 1: { // caja
        const g = this.hit(dest, when, 0.5 * v, 0.2); this.noiseSrc(when, 0.25, g, 'highpass', 1500);
        const o = c.createOscillator(), g2 = this.hit(dest, when, 0.4 * v, 0.1);
        o.type = 'triangle'; o.frequency.value = 190; o.connect(g2); o.start(when); o.stop(when + 0.15); break; }
      case 2: { const g = this.hit(dest, when, 0.3 * v, 0.05); this.noiseSrc(when, 0.08, g, 'highpass', 7000); break; }
      case 3: { const g = this.hit(dest, when, 0.3 * v, 0.3); this.noiseSrc(when, 0.35, g, 'highpass', 6500); break; }
      case 4: { // palmas
        [0, 0.012, 0.024].forEach(dt => { const g = this.hit(dest, when + dt, 0.35 * v, 0.03); this.noiseSrc(when + dt, 0.05, g, 'bandpass', 1300, 1.2); });
        const g = this.hit(dest, when + 0.03, 0.4 * v, 0.18); this.noiseSrc(when + 0.03, 0.22, g, 'bandpass', 1200, 1.2); break; }
      case 5: { // tom
        const o = c.createOscillator(), g = this.hit(dest, when, 0.7 * v, 0.3);
        o.frequency.setValueAtTime(220, when); o.frequency.exponentialRampToValueAtTime(90, when + 0.2);
        o.connect(g); o.start(when); o.stop(when + 0.35); break; }
      case 6: { // rim
        const o = c.createOscillator(), g = this.hit(dest, when, 0.3 * v, 0.04);
        o.type = 'square'; o.frequency.value = 820; o.connect(g); o.start(when); o.stop(when + 0.06); break; }
      case 7: { // cencerro
        const f = c.createBiquadFilter(), g = this.hit(dest, when, 0.25 * v, 0.25);
        f.type = 'bandpass'; f.frequency.value = 700; f.Q.value = 1.5; f.connect(g);
        [560, 845].forEach(fr => { const o = c.createOscillator(); o.type = 'square'; o.frequency.value = fr; o.connect(f); o.start(when); o.stop(when + 0.3); }); break; }
    }
  }
  click(when, accent) {
    const o = this.ctx.createOscillator(), g = this.hit(this.master, when, 0.25, 0.04);
    o.frequency.value = accent ? 1500 : 1000; o.connect(g); o.start(when); o.stop(when + 0.05);
  }

  // ---- secuenciador
  stepDur() { return 60 / this.song.bpm / 4; }
  fire(t, i, clip, local, time, sd, isStart, remain, ex) {
    if (clip.audio) {                                   // clip de audio: se dispara al inicio del clip (o del tramo en el Arrangement)
      if (local !== 0 && !isStart) return;
      const au = clip.audio, buf = audioBuffer(au, this.song.bpm); if (!buf) return;
      const off = local * sd; if (off >= buf.duration) return;
      const src = this.ctx.createBufferSource(), g = this.ctx.createGain(), gain = au.gain === undefined ? 1 : au.gain; src.buffer = buf;
      src.connect(g); g.connect(this.tr[i].in);
      const left = Math.min(remain === undefined ? Infinity : remain * sd, buf.duration - off);
      let fi = (au.fi === undefined ? 0.003 : au.fi) + ((ex && ex.fi) || 0) * sd, fo = (au.fo === undefined ? 0.003 : au.fo) + ((ex && ex.fo) || 0) * sd;
      if (fi + fo > left) { const k = left / (fi + fo); fi *= k; fo *= k; }
      g.gain.setValueAtTime(fi > 0 ? 0 : gain, time); if (fi > 0) g.gain.linearRampToValueAtTime(gain, time + fi);
      if (fo > 0 && isFinite(left)) { g.gain.setValueAtTime(gain, time + left - fo); g.gain.linearRampToValueAtTime(0, time + left); }
      src.start(time, off, left); this.srcs.add(src); src.onended = () => this.srcs.delete(src); return;
    }
    const sw = (Math.floor(local + 1e-9) % 2 === 1) ? (this.song.swing || 0) * sd : 0;
    for (const n of clip.notes) {
      if (n.m || Math.floor(n.s + 1e-9) !== local) continue;
      this.note(t, i, n.n, time + sw + (n.s - local) * sd, n.l * sd, n.v === undefined ? 0.8 : n.v, (n.pan || n.fine) ? { pan: n.pan, fine: n.fine } : null);
    }
  }
  scheduleStep(step, time, mode) {
    const s = this.song, sd = this.stepDur();
    if (mode === 'session') {
      if (step % 16 === 0) this.rt.forEach(r => { if (r.queued !== undefined) { r.playing = r.queued; r.queued = undefined; r.launch = step; } });
      s.tracks.forEach((t, i) => {
        const r = this.rt[i]; if (r.playing < 0) return;
        const c = t.clips[r.playing]; if (!c) return;
        const L = c.bars * 16, lc = (step - r.launch) % L; this.fire(t, i, c, lc, time, sd, false, L - lc);
      });
    } else {
      const anySolo = s.tracks.some(t => t.solo);
      (s.autos || []).forEach(au => { if (au.pts && au.pts.length) this.applyAuto(au, step, time, sd); });
      s.arr.forEach(it => {
        if (it.pat !== undefined) {                                   // patrón completo: suenan todos los canales
          if (it.m || step < it.s || step >= it.s + it.l || (s.lanes && s.lanes[it.lane] && s.lanes[it.lane].mute)) return;
          let patLen = 16; s.tracks.forEach(tt => { const cc = tt.clips[it.pat]; if (cc) patLen = Math.max(patLen, cc.bars * 16); });
          const lp = (step - it.s + (it.off || 0)) % patLen;
          s.tracks.forEach((tt, ti) => { const cc = tt.clips[it.pat]; if (!cc || lp >= cc.bars * 16) return; this.fire(tt, ti, cc, lp, time, sd, step === it.s, it.s + it.l - step, null); });
          return;
        }
        if (s.lanes && s.lanes[it.lane !== undefined ? it.lane : it.t] && s.lanes[it.lane !== undefined ? it.lane : it.t].mute) return;
        const t = s.tracks[it.t]; let c = t && t.clips[it.slot];
        if (!c || it.m || step < it.s || step >= it.s + it.l) return;
        if (c.audio && it.take !== undefined && c.takes && c.takes[it.take]) c = Object.assign({}, c, { audio: c.takes[it.take] });
        const len = c.bars * 16, local = (step - it.s + (it.off || 0)) % len;
        this.fire(t, it.t, c, local, time, sd, step === it.s, it.s + it.l - step, c.audio ? { fi: it.fi, fo: it.fo } : null);
      });
    }
    if (this.metro && step % 4 === 0) this.click(time, step % 16 === 0);
  }

  applyAuto(au, step, time, sd) {
    const T = autoInfo(au.target, this.song); if (!T) return;
    const v0 = autoMap(T, envAt(au.pts, step, 0)), v1 = autoMap(T, envAt(au.pts, step + 1, 0)), m = /^(m|t(\d+)|i(\d+)|r(\d+))\.(\w+)$/.exec(au.target);
    if (T.js) {                                          // parámetros que se leen en JS: se restauran al parar
      this.bak = this.bak || new Map(); if (!this.bak.has(au.target)) this.bak.set(au.target, T.get());
      T.set(v0); if (T.fx && this.ins[T.fx[0]].fx[T.fx[1]]) this.ins[T.fx[0]].fx[T.fx[1]].update(); if (T.vca) this.applyMix(); return;
    }
    let ap = null;
    if (au.target === 'm.vol') ap = this.master.gain;
    else if (m && m[2] !== undefined) { const o = this.tr[+m[2]]; ap = m[5] === 'vol' ? o.vol.gain : m[5] === 'pan' ? o.pan.pan : o.sends[m[5] === 'sa' ? 0 : 1].gain; }
    else if (m && m[3] !== undefined) { const o = this.ins[+m[3]]; ap = m[5] === 'vol' ? o.vol.gain : o.pan.pan; }
    else if (m && m[4] !== undefined) ap = this.ret[+m[4]].vol.gain;
    if (ap) { ap.setValueAtTime(v0, time); ap.linearRampToValueAtTime(v1, time + sd); }
  }
  play(mode, startStep = 0) {
    if (this.playing) this.stop();
    this.mode = mode; this.playing = true; this.step = mode === 'arr' ? startStep : 0; this.nextTime = this.ctx.currentTime + 0.08; this.log = [];
    this.timer = setInterval(() => this.tick(), 25);
  }
  tick() {
    while (this.playing && this.nextTime < this.ctx.currentTime + 0.12) {
      this.scheduleStep(this.step, this.nextTime, this.mode);
      this.log.push([this.nextTime, this.step]);
      this.step++; this.nextTime += this.stepDur();
      const lp = this.song.loop;
      if (this.mode === 'arr' && lp && lp.on && this.step >= lp.end) this.step = lp.start;
      if (this.mode === 'arr' && this.step >= ARR_BARS * 16) { setTimeout(() => this.onEnd && this.onEnd(), 600); this.playing = false; clearInterval(this.timer); this.timer = null; break; }
    }
  }
  stop(noMix) {
    this.playing = false; if (this.timer) clearInterval(this.timer); this.timer = null;
    this.rt.forEach(r => { r.playing = -1; r.queued = undefined; });
    this.srcs.forEach(x => { try { x.stop(); } catch (e) {} }); this.srcs.clear();
    if (this.bak) { this.bak.forEach((v, id) => { const T = autoInfo(id, this.song); if (T && T.set) { T.set(v); if (T.vca) this.applyMix(); if (T.fx && this.ins[T.fx[0]].fx[T.fx[1]]) this.ins[T.fx[0]].fx[T.fx[1]].update(); } }); this.bak = null; }
    this.log = []; if (!noMix) this.applyMix();
  }
  currentStep() {
    const now = this.ctx.currentTime;
    while (this.log.length > 1 && this.log[1][0] <= now) this.log.shift();
    return this.log.length && this.log[0][0] <= now ? this.log[0][1] : -1;
  }
  posFloat() {                                 // posición actual en pasos (con decimales)
    const now = this.ctx.currentTime; let e = this.log[0];
    for (const x of this.log) if (x[0] <= now) e = x; else break;
    return e && e[0] <= now ? e[1] + (now - e[0]) / this.stepDur() : -1;
  }
  launch(i, slot) {
    if (!this.playing || this.mode !== 'session') this.play('session');
    this.rt[i].queued = slot;
  }
  launchScene(slot) { this.song.tracks.forEach((t, i) => this.launch(i, t.clips[slot] ? slot : -1)); }
  stopTrack(i) { if (this.playing) this.rt[i].queued = -1; }
}

// ---- exportar a WAV (render offline de la vista Arrangement)
async function renderWav(song) {
  let steps = 0;
  song.arr.forEach(it => { if (it.pat !== undefined ? song.tracks.some(t => t.clips[it.pat]) : (song.tracks[it.t] && song.tracks[it.t].clips[it.slot])) steps = Math.max(steps, it.s + it.l); });
  if (!steps) throw new Error('El Arrangement está vacío.');
  const sr = 44100, sd = 60 / song.bpm / 4, len = Math.ceil((steps * sd + 2.5) * sr);
  const off = new OfflineAudioContext(2, len, sr), eng = new Engine(off, song);
  for (let s = 0; s < steps; s++) eng.scheduleStep(s, s * sd, 'arr');
  return encodeWav(await off.startRendering());
}
function encodeWav(buf) {
  const ch = buf.numberOfChannels, n = buf.length, data = new DataView(new ArrayBuffer(44 + n * ch * 2));
  const w = (o, s) => { for (let i = 0; i < s.length; i++) data.setUint8(o + i, s.charCodeAt(i)); };
  w(0, 'RIFF'); data.setUint32(4, 36 + n * ch * 2, true); w(8, 'WAVEfmt '); data.setUint32(16, 16, true);
  data.setUint16(20, 1, true); data.setUint16(22, ch, true); data.setUint32(24, buf.sampleRate, true);
  data.setUint32(28, buf.sampleRate * ch * 2, true); data.setUint16(32, ch * 2, true); data.setUint16(34, 16, true);
  w(36, 'data'); data.setUint32(40, n * ch * 2, true);
  const chans = []; for (let c = 0; c < ch; c++) chans.push(buf.getChannelData(c));
  let o = 44, peak = 0;
  for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++) {
    const v = Math.max(-1, Math.min(1, chans[c][i])); peak = Math.max(peak, Math.abs(v));
    data.setInt16(o, v < 0 ? v * 0x8000 : v * 0x7fff, true); o += 2;
  }
  return { blob: new Blob([data], { type: 'audio/wav' }), peak };
}
