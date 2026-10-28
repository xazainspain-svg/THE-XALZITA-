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
  dispose() { this.stop(); try { this.master.disconnect(); } catch (e) {} }

  // medidor estéreo (dos analizadores) colgado de un nodo
  meterTap(node) {
    const c = this.ctx, sp = c.createChannelSplitter(2), anL = c.createAnalyser(), anR = c.createAnalyser();
    anL.fftSize = anR.fftSize = 512; node.connect(sp); sp.connect(anL, 0); sp.connect(anR, 1);
    return { anL, anR };
  }
  buildReturn(r) {
    const c = this.ctx, o = { in: c.createGain(), vol: c.createGain() };
    r.fx.p.ret = true; o.fx = makeFx(c, r.fx.type, r.fx.p, this);
    o.in.connect(o.fx.in); o.fx.out.connect(o.vol); o.vol.connect(this.master); Object.assign(o, this.meterTap(o.vol)); return o;
  }
  buildTrack(t) {
    const c = this.ctx, o = { in: c.createGain(), vol: c.createGain(), pan: c.createStereoPanner(), an: c.createAnalyser(), sends: [] };
    o.an.fftSize = 512; o.in.connect(o.vol); o.vol.connect(o.pan); o.pan.connect(o.an);
    this.ret.forEach(r => { const g = c.createGain(); g.gain.value = 0; o.pan.connect(g); g.connect(r.in); o.sends.push(g); });
    this.routeTrack(o, t); return o;
  }
  routeTrack(o, t) {
    const idx = Math.max(0, Math.min(this.ins.length - 1, t.insert | 0)), obj = !!(t.obj && t.obj.on), key = idx + ':' + obj;
    if (o.routeKey !== key) {
      o.routeKey = key; try { o.pan.disconnect(); } catch (e) {}
      o.pan.connect(o.an); o.sends.forEach(g => o.pan.connect(g));
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
    o.vol.connect(o.pan); o.pan.connect(this.master); Object.assign(o, this.meterTap(o.pan));
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

  baseGain(t, anySolo) { return (t.mute || (anySolo && !t.solo)) ? 0 : t.vol; }
  applyMix() {
    const now = this.ctx.currentTime, S = this.song, anySolo = S.tracks.some(t => t.solo), insSolo = S.inserts.some(x => x.solo);
    S.tracks.forEach((t, i) => {
      const o = this.tr[i], g = o.vol.gain;
      g.cancelScheduledValues(now); g.setValueAtTime(this.baseGain(t, anySolo), now);
      o.pan.pan.setValueAtTime(t.pan, now); this.routeTrack(o, t);
      o.sends.forEach((g, k) => g.gain.setValueAtTime((t.sends && t.sends[k]) || 0, now));
    });
    this.ret.forEach((o, k) => o.vol.gain.setValueAtTime(S.returns[k].vol, now));
    S.inserts.forEach((x, k) => {
      const o = this.ins[k];
      o.vol.gain.setValueAtTime((x.mute || (insSolo && !x.solo)) ? 0 : x.vol, now); o.pan.pan.setValueAtTime(x.pan, now);
    });
    this.master.gain.setValueAtTime(S.masterVol === undefined ? 0.9 : S.masterVol, now);
  }

  // ---- instrumentos
  note(t, i, n, when, dur, vel) {
    const dest = this.tr[i].in;
    if (t.kind === 'drums') this.drum(n % 8, when, vel, dest); else this.synth(t.inst, n, when, dur, vel, dest);
  }
  synth(inst, n, when, dur, vel, dest) {
    const c = this.ctx, f = midiFreq(n), flt = c.createBiquadFilter(), env = c.createGain();
    flt.type = 'lowpass'; flt.frequency.value = inst.cutoff; flt.Q.value = inst.res;
    const a = Math.max(0.003, inst.atk), peak = 0.2 * vel, end = when + Math.max(dur, a), stop = end + inst.rel;
    env.gain.setValueAtTime(0, when); env.gain.linearRampToValueAtTime(peak, when + a);
    env.gain.setValueAtTime(peak, end); env.gain.linearRampToValueAtTime(0, stop);
    flt.connect(env); env.connect(dest);
    [-7, 7].forEach(det => {
      const o = c.createOscillator(); o.type = inst.wave; o.frequency.value = f; o.detune.value = det;
      o.connect(flt); o.start(when); o.stop(stop + 0.05);
    });
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
  fire(t, i, clip, local, time, sd, isStart, remain) {
    if (clip.audio) {                                   // clip de audio: se dispara al inicio del clip (o del tramo en el Arrangement)
      if (local !== 0 && !isStart) return;
      const a = AUDIO[clip.audio.id]; if (!a) return;
      const off = local * sd; if (off >= a.buf.duration) return;
      const src = this.ctx.createBufferSource(); src.buffer = a.buf; src.connect(this.tr[i].in);
      const left = Math.min(remain === undefined ? Infinity : remain * sd, a.buf.duration - off);
      src.start(time, off, left); this.srcs.add(src); src.onended = () => this.srcs.delete(src); return;
    }
    const sw = (Math.floor(local + 1e-9) % 2 === 1) ? (this.song.swing || 0) * sd : 0;
    for (const n of clip.notes) {
      if (n.m || Math.floor(n.s + 1e-9) !== local) continue;
      this.note(t, i, n.n, time + sw + (n.s - local) * sd, n.l * sd, n.v === undefined ? 0.8 : n.v);
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
      s.tracks.forEach((t, i) => {            // envolventes de automatización (volumen / panorama)
        const o = this.tr[i];
        if (t.autoVol && t.autoVol.length) {
          const g = o.vol.gain; g.setValueAtTime(this.baseGain(t, anySolo) * envAt(t.autoVol, step, 1), time);
          g.linearRampToValueAtTime(this.baseGain(t, anySolo) * envAt(t.autoVol, step + 1, 1), time + sd);
        }
        if (t.autoPan && t.autoPan.length) {
          const p = o.pan.pan; p.setValueAtTime(envAt(t.autoPan, step, t.pan), time);
          p.linearRampToValueAtTime(envAt(t.autoPan, step + 1, t.pan), time + sd);
        }
      });
      s.arr.forEach(it => {
        const t = s.tracks[it.t], c = t && t.clips[it.slot];
        if (!c || step < it.s || step >= it.s + it.l) return;
        const len = c.bars * 16, local = (step - it.s + (it.off || 0)) % len;
        this.fire(t, it.t, c, local, time, sd, step === it.s, it.s + it.l - step);
      });
    }
    if (this.metro && step % 4 === 0) this.click(time, step % 16 === 0);
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
  stop() {
    this.playing = false; if (this.timer) clearInterval(this.timer); this.timer = null;
    this.rt.forEach(r => { r.playing = -1; r.queued = undefined; });
    this.srcs.forEach(x => { try { x.stop(); } catch (e) {} }); this.srcs.clear();
    this.log = []; this.applyMix();
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
  song.arr.forEach(it => { if (song.tracks[it.t] && song.tracks[it.t].clips[it.slot]) steps = Math.max(steps, it.s + it.l); });
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
