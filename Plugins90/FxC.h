#pragma once
// Parte C: tiempo (12), estereo / utilidad (12) y creativos (14).
#include "FxB.h"

namespace xfx
{
// ------------------------------------------------------------------ Tiempo
struct DelayFx : Fx    // tiempo ms, feedback, tone Hz
{
    double sr = 48000; Delay dl, dr; Tone tl, tr;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 2.1)); dr.init ((int) (s * 2.1)); dl.clear(); dr.clear(); tl.reset(); tr.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float T = p[0] * 0.001f * (float) sr;
        const float wl = dl.read (T), wr = dr.read (T);
        dl.push (l + p[1] * tl.run (wl, p[2], sr)); dr.push (r + p[1] * tr.run (wr, p[2], sr));
        l = wl; r = wr;
    }
};
struct PingPongFx : Fx // tiempo ms, feedback, tone Hz
{
    double sr = 48000; Delay dl, dr; Tone tl, tr;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 2.1)); dr.init ((int) (s * 2.1)); dl.clear(); dr.clear(); tl.reset(); tr.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float T = p[0] * 0.001f * (float) sr;
        const float a = dl.read (T), b = dr.read (T), m = 0.5f * (l + r);
        dl.push (m + p[1] * tl.run (b, p[2], sr)); dr.push (tr.run (a, p[2], sr));
        l = a; r = b;
    }
};
struct SlapbackFx : Fx // tiempo ms, repeticiones, tone Hz
{
    double sr = 48000; Delay dl, dr; Tone tl, tr;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.3)); dr.init ((int) (s * 0.3)); dl.clear(); dr.clear(); tl.reset(); tr.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float T = p[0] * 0.001f * (float) sr;
        const float wl = dl.read (T), wr = dr.read (T * 1.09f);
        dl.push (l + p[1] * tl.run (wl, p[2], sr)); dr.push (r + p[1] * tr.run (wr, p[2], sr));
        l = wl; r = wr;
    }
};
struct TapeEchoFx : Fx // tiempo ms, feedback, wow, saturacion
{
    double sr = 48000; Delay dl, dr; Tone tl, tr; Lfo w1, w2;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 2.1)); dr.init ((int) (s * 2.1)); dl.clear(); dr.clear(); tl.reset(); tr.reset(); w1.ph = w2.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float a = Lfo::sine (w1.tick (0.7f, sr)), b = Lfo::sine (w2.tick (5.3f, sr));
        const float T = p[0] * 0.001f * (float) sr * (1.f + p[2] * (0.004f * a + 0.0015f * b));
        const float k = 1.f + 3.f * p[3], c = 1.f / (1.f + 0.5f * p[3]);
        const float wl = std::tanh (tl.run (dl.read (T), 3500.f, sr) * k) * c;
        const float wr = std::tanh (tr.run (dr.read (T), 3500.f, sr) * k) * c;
        dl.push (l + p[1] * wl); dr.push (r + p[1] * wr);
        l = wl; r = wr;
    }
};
struct MultiTapFx : Fx // espaciado ms, taps, decaimiento, feedback
{
    double sr = 48000; Delay dm;
    void prepare (double s) override { sr = s; dm.init ((int) (s * 4.2)); dm.clear(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float m = 0.5f * (l + r), T = p[0] * 0.001f * (float) sr;
        const int n = std::max (1, (int) std::lround (p[1]));
        float sl = 0, sr_ = 0, g = 1;
        for (int k = 1; k <= n; ++k) { const float v = dm.read (T * (float) k) * g; if (k & 1) sl += v; else sr_ += v; g *= p[2]; }
        const float norm = 1.f / std::sqrt ((float) n);
        dm.push (m + p[3] * 0.5f * (sl + sr_) * norm);
        l = sl * norm; r = sr_ * norm;
    }
};
template <int Scale>   // Scale = % del tamano base (Room 35, Reverb 100, Hall 170)
struct ReverbFx : Fx   // tamano, damping, ancho, pre-delay ms
{
    double sr = 48000; RevCore rc; Delay pl, pr;
    void prepare (double s) override { sr = s; rc.init (s, Scale / 100.f); pl.init ((int) (s * 0.11)); pr.init ((int) (s * 0.11)); pl.clear(); pr.clear(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float pd = std::max (1.f, p[3] * 0.001f * (float) sr);
        const float il = pl.read (pd), ir = pr.read (pd); pl.push (l); pr.push (r);
        rc.run (il, ir, 0.7f + 0.25f * p[0], p[1] * 0.9f, p[2], l, r);
    }
};
struct PlateFx : Fx    // decay, damping, difusion, pre-delay ms
{
    double sr = 48000; RevCore rc; AllpassD ap[4]; Delay pre;
    void prepare (double s) override
    {
        sr = s; rc.init (s, 0.7f); pre.init ((int) (s * 0.11)); pre.clear();
        static const int len[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) { ap[i] = AllpassD(); ap[i].init ((int) (len[i] * s / 44100.0)); }
    }
    void tick (float& l, float& r, const float* p) override
    {
        float y = pre.read (std::max (1.f, p[3] * 0.001f * (float) sr)); pre.push (0.5f * (l + r));
        for (auto& a : ap) y = a.run (y, 0.75f * p[2]);
        rc.run (y, y, 0.7f + 0.25f * p[0], p[1] * 0.9f, 1.f, l, r);
    }
};
struct SpringFx : Fx   // tension ms, decay, goteo
{
    double sr = 48000; Delay d[2]; AP1 ap[2][16]; Tone t[2];
    void prepare (double s) override
    {
        sr = s; for (int c = 0; c < 2; ++c) { d[c].init ((int) (s * 0.1)); d[c].clear(); t[c].reset(); for (auto& a : ap[c]) a.z = 0; }
    }
    void tick (float& l, float& r, const float* p) override
    {
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float x = *io[c];
            float y = d[c].read (p[0] * (c ? 1.07f : 1.f) * 0.001f * (float) sr);
            for (auto& a : ap[c]) y = a.run (y, p[2]);
            y = t[c].run (y, 3500.f, sr);
            d[c].push (x + p[1] * y);
            *io[c] = y;
        }
    }
};
struct StutterFx : Fx  // intervalo ms, largo ms, hold
{
    double sr = 48000; Delay dl, dr; int t = 0, capPos = 0, capLen = 64;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.5)); dr.init ((int) (s * 0.5)); dl.clear(); dr.clear(); t = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const int interval = std::max (64, (int) (p[0] * 0.001f * (float) sr));
        if (t >= interval) t = 0;
        if (t == 0) { capPos = dl.w; capLen = std::max (32, (int) (p[1] * 0.001f * (float) sr)); }
        if ((float) t < p[2] * (float) interval)
        {
            const int idx = capPos - capLen + (t % capLen);
            l = dl.at (idx); r = dr.at (idx);
        }
        else { dl.push (l); dr.push (r); }
        ++t;
    }
};
struct ReverseS        // largo ms, velocidad
{
    double sr = 48000; PitchCore pc;
    void init (double s) { sr = s; pc.init (s); }
    float run (float x, const float* p) { return pc.run (x, -p[1], p[0] * 0.001f * (float) sr); }
};

// ----------------------------------------------------------- Estereo / util
struct WidthFx : Fx    // ancho
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { const float m = 0.5f * (l + r), s = 0.5f * (l - r) * p[0]; l = m + s; r = m - s; }
};
struct MidSideFx : Fx  // mid dB, side dB
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override
    {
        const float m = 0.5f * (l + r) * dbToLin (p[0]), s = 0.5f * (l - r) * dbToLin (p[1]); l = m + s; r = m - s;
    }
};
struct MonoFx : Fx     // cantidad
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { const float m = 0.5f * (l + r); l = lerp (l, m, p[0]); r = lerp (r, m, p[0]); }
};
struct PanFx : Fx      // pan
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { const float th = (p[0] + 1.f) * kPi * 0.25f; l *= std::cos (th) * 1.4142f; r *= std::sin (th) * 1.4142f; }
};
struct GainFx : Fx     // ganancia dB, soft clip
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override
    {
        const float g = dbToLin (p[0]); l *= g; r *= g;
        if (p[1] > 0.5f) { l = std::tanh (l); r = std::tanh (r); }
    }
};
struct PolarityFx : Fx // invertir L, invertir R
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { if (p[0] > 0.5f) l = -l; if (p[1] > 0.5f) r = -r; }
};
struct SwapFx : Fx     // cantidad de intercambio
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { const float a = l, b = r; l = lerp (a, b, p[0]); r = lerp (b, a, p[0]); }
};
struct HaasFx : Fx     // retardo ms, lado, nivel dB
{
    double sr = 48000; Delay dl, dr;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.05)); dr.init ((int) (s * 0.05)); dl.clear(); dr.clear(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float d = std::max (1.f, p[0] * 0.001f * (float) sr), g = dbToLin (p[2]);
        const float wl = dl.read (d), wr = dr.read (d); dl.push (l); dr.push (r);
        if (p[1] > 0.5f) l = wl * g; else r = wr * g;
    }
};
struct CrossfeedFx : Fx // cantidad, freq Hz
{
    double sr = 48000; Tone tl, tr;
    void prepare (double s) override { sr = s; tl.reset(); tr.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float cl = tl.run (r, p[1], sr), cr = tr.run (l, p[1], sr), a = p[0];
        l = l * (1.f - 0.3f * a) + 0.5f * a * cl; r = r * (1.f - 0.3f * a) + 0.5f * a * cr;
    }
};
struct RotateFx : Fx   // angulo grados
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override
    {
        const float a = p[0] * kPi / 180.f, c = std::cos (a), s = std::sin (a), x = l, y = r;
        l = x * c - y * s; r = x * s + y * c;
    }
};
struct BalanceFx : Fx  // balance
{
    void prepare (double) override {}
    void tick (float& l, float& r, const float* p) override { l *= p[0] > 0 ? 1.f - p[0] : 1.f; r *= p[0] < 0 ? 1.f + p[0] : 1.f; }
};
struct DecorrFx : Fx   // cantidad
{
    AllpassD a[2][4];
    void prepare (double sr) override
    {
        static const int L[2][4] = { { 113, 337, 521, 211 }, { 131, 367, 491, 257 } };
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 4; ++i) { a[c][i] = AllpassD(); a[c][i].init ((int) (L[c][i] * sr / 44100.0)); }
    }
    void tick (float& l, float& r, const float* p) override
    {
        const float g = 0.55f * p[0];
        for (auto& s : a[0]) l = s.run (l, g);
        for (auto& s : a[1]) r = s.run (r, g);
    }
};

// ---------------------------------------------------------------- Creativos
struct TapeStopFx : Fx // stop, tiempo s
{
    double sr = 48000; Delay dl, dr; float rate = 1, lag = 1; bool was = false;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 8.0)); dr.init ((int) (s * 8.0)); dl.clear(); dr.clear(); rate = 1; lag = 1; was = false; }
    void tick (float& l, float& r, const float* p) override
    {
        dl.push (l); dr.push (r);
        const bool stop = p[0] > 0.5f;
        if (stop) rate = std::max (0.f, rate - 1.f / (std::max (p[1], 0.05f) * (float) sr));
        else
        {
            if (was) lag = std::min (lag, 0.25f * (float) sr);
            const float target = lag > 2.f ? 2.f : 1.f, step = 1.f / (0.25f * (float) sr);
            rate += clampf (target - rate, -step, step);
        }
        was = stop;
        lag = clampf (lag + 1.f - rate, 1.f, (float) dl.n - 8.f);
        const float g = std::min (1.f, rate * 4.f);
        l = dl.read (lag) * g; r = dr.read (lag) * g;
    }
};
struct LofiS           // bits, rate Hz, ruido, wobble
{
    double sr = 48000; Delay d; Lfo a, b; Tone t; Rng rng; float ph = 0, hold = 0;
    void init (double s) { sr = s; d.init ((int) (s * 0.04)); d.clear(); t.reset(); a.ph = b.ph = ph = hold = 0; }
    float run (float x, const float* p)
    {
        const float w = Lfo::sine (a.tick (0.6f, sr)) + 0.4f * Lfo::sine (b.tick (5.1f, sr));
        d.push (x); float y = d.read ((10.f + 3.f * p[3] * w) * 0.001f * (float) sr);
        y = t.run (y, p[1] * 0.45f, sr);
        ph += p[1] / (float) sr; if (ph >= 1.f) { ph -= 1.f; hold = y; }
        const float L = std::pow (2.f, p[0] - 1.f);
        return std::round (clampf (hold, -1.f, 1.f) * L) / L + rng.bi() * 0.02f * p[2];
    }
};
struct VinylS          // crujido, ruido, desgaste Hz, wow
{
    double sr = 48000; Delay d; Lfo a; Tone t; Rng rng; float pop = 0;
    void init (double s) { sr = s; d.init ((int) (s * 0.03)); d.clear(); t.reset(); a.ph = 0; pop = 0; }
    float run (float x, const float* p)
    {
        d.push (x); float y = d.read ((6.f + 2.f * p[3] * Lfo::sine (a.tick (0.55f, sr))) * 0.001f * (float) sr);
        y = t.run (y, p[2], sr);
        if (rng.uni() < p[0] * 25.f / (float) sr) { const float v = rng.bi(); pop = v * v * v * 0.7f; }
        pop *= 0.85f;
        const float dust = rng.uni() < 0.08f ? rng.bi() * 0.004f * p[0] : 0.f;
        return y + pop + dust + rng.bi() * 0.012f * p[1];
    }
};
struct RadioS          // corte grave Hz, corte agudo Hz, drive dB, ruido
{
    double sr = 48000; Flt hp, lp; Rng rng;
    void init (double s) { sr = s; hp.reset(); lp.reset(); }
    float run (float x, const float* p)
    {
        hp.upd (Biquad::HP, p[0], 0.8f, 0.f, sr); lp.upd (Biquad::LP, p[1], 0.8f, 0.f, sr);
        const float g = dbToLin (p[2]);
        return lp.run (std::tanh (g * hp.run (x)) * compGain (g, 0.3f)) + rng.bi() * 0.02f * p[3];
    }
};
struct PhoneS          // grave Hz, agudo Hz, grit
{
    double sr = 48000; Flt h1, h2, l1, l2; float ph = 0, hold = 0;
    void init (double s) { sr = s; h1.reset(); h2.reset(); l1.reset(); l2.reset(); ph = hold = 0; }
    float run (float x, const float* p)
    {
        h1.upd (Biquad::HP, p[0], 0.707f, 0.f, sr); h2.upd (Biquad::HP, p[0], 0.707f, 0.f, sr);
        l1.upd (Biquad::LP, p[1], 0.707f, 0.f, sr); l2.upd (Biquad::LP, p[1], 0.707f, 0.f, sr);
        float y = l2.run (l1.run (h2.run (h1.run (x))));
        y = std::tanh (y * (1.f + p[2] * 8.f)) / (1.f + p[2] * 2.f);
        ph += lerp ((float) sr, 8000.f, p[2]) / (float) sr; if (ph >= 1.f) { ph -= 1.f; hold = y; }
        return hold;
    }
};
struct MegaphoneS      // drive dB, foco Hz, cuerno dB
{
    double sr = 48000; Flt hp, lp, pk;
    void init (double s) { sr = s; hp.reset(); lp.reset(); pk.reset(); }
    float run (float x, const float* p)
    {
        hp.upd (Biquad::HP, 400.f, 0.707f, 0.f, sr); lp.upd (Biquad::LP, 4500.f, 0.707f, 0.f, sr); pk.upd (Biquad::Peak, p[1], 2.f, p[2], sr);
        const float g = dbToLin (p[0]);
        return pk.run (std::tanh (g * lp.run (hp.run (x))) * compGain (g, 0.3f)) * 0.8f;
    }
};
struct RobotS          // tono Hz, resonancia, anillo
{
    double sr = 48000; Delay d; Lfo lfo;
    void init (double s) { sr = s; d.init ((int) (s / 25.0) + 8); d.clear(); lfo.ph = 0; }
    float run (float x, const float* p)
    {
        const float y = lerp (x, x * Lfo::sine (lfo.tick (p[0], sr)), p[2]);
        const float o = y + p[1] * d.read ((float) sr / clampf (p[0], 25.f, 2000.f));
        d.push (o);
        return o * (1.f - 0.5f * p[1]);
    }
};
struct UnderwaterS     // profundidad, rate, oscuridad Hz
{
    double sr = 48000; Delay d; Lfo a, b; Flt f;
    void init (double s) { sr = s; d.init ((int) (s * 0.03)); d.clear(); f.reset(); a.ph = b.ph = 0; }
    float run (float x, const float* p)
    {
        const float s = Lfo::sine (a.tick (p[1], sr)), c = Lfo::sine (b.tick (p[1] * 1.7f, sr));
        d.push (x);
        f.upd (Biquad::LP, p[2] * (1.f + 0.3f * p[0] * c), 1.2f, 0.f, sr);
        return f.run (d.read ((5.f + 4.f * p[0] * s) * 0.001f * (float) sr));
    }
};
struct GaterFx : Fx    // pasos/s, patron, suavizado ms, profundidad
{
    double sr = 48000; float ph = 0, g = 1;
    void prepare (double s) override { sr = s; ph = 0; g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        static const unsigned pat[8] = { 0xAAAA, 0x8888, 0xEEEE, 0x9249, 0xF0F0, 0xCCCC, 0xA5A5, 0xDADA };
        ph += p[0] / (16.f * (float) sr); if (ph >= 1.f) ph -= 1.f;
        const int step = std::min (15, (int) (ph * 16.f)), idx = std::min (7, std::max (0, (int) std::lround (p[1])));
        const float target = ((pat[idx] >> step) & 1u) ? 1.f : 1.f - p[3];
        g = target + (g - target) * msToCoef (p[2], sr);
        l *= g; r *= g;
    }
};
struct SampleHoldFilt  // rate Hz, Q, rango octavas
{
    double sr = 48000; Flt f; float ph = 0, cur = 400, target = 400; Rng rng;
    void init (double s) { sr = s; f.reset(); ph = 0; cur = target = 400; }
    float run (float x, const float* p)
    {
        ph += p[0] / (float) sr;
        if (ph >= 1.f) { ph -= 1.f; target = 100.f * std::pow (2.f, rng.uni() * p[2]); }
        cur += (target - cur) * (1.f - msToCoef (5.f, sr));
        f.upd (Biquad::LP, cur, p[1], 0.f, sr);
        return f.run (x);
    }
};
struct GlitchFx : Fx   // probabilidad, tamano ms
{
    double sr = 48000; Delay dl, dr; Rng rng; int t = 0, sliceLen = 1, mode = 0, loopLen = 64, capPos = 0;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.6)); dr.init ((int) (s * 0.6)); dl.clear(); dr.clear(); t = 0; sliceLen = 1; mode = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        if (t >= sliceLen)
        {
            t = 0; sliceLen = std::max (256, (int) (p[1] * 0.001f * (float) sr));
            const float u = rng.uni();
            if (u < p[0] * 0.6f) { mode = 1; loopLen = std::max (64, sliceLen >> (1 + (int) (rng.uni() * 3.f))); capPos = dl.w; }
            else if (u < p[0])   mode = 2;
            else                 mode = 0;
        }
        if (mode == 1) { const int idx = capPos - loopLen + (t % loopLen); l = dl.at (idx); r = dr.at (idx); }
        else
        {
            dl.push (l); dr.push (r);
            if (mode == 2) { l = 0; r = 0; }
        }
        ++t;
    }
};
struct ShimmerFx : Fx  // decay, shimmer, damping
{
    double sr = 48000; RevCore rc; PitchCore pl, pr; float fl = 0, fr = 0;
    void prepare (double s) override { sr = s; rc.init (s, 1.2f); pl.init (s); pr.init (s); fl = fr = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        float oL, oR;
        rc.run (l + fl, r + fr, 0.7f + 0.25f * p[0], p[2] * 0.9f, 1.f, oL, oR);
        const float win = 0.08f * (float) sr;
        fl = std::tanh (pl.run (oL, 2.f, win)) * p[1] * 0.2f;
        fr = std::tanh (pr.run (oR, 2.f, win)) * p[1] * 0.2f;
        l = oL; r = oR;
    }
};
struct ResonatorS      // nota MIDI, acorde, decay, tone Hz
{
    double sr = 48000; Delay d[4]; Tone t[4];
    void init (double s) { sr = s; for (int i = 0; i < 4; ++i) { d[i].init ((int) (s / 30.0) + 8); d[i].clear(); t[i].reset(); } }
    float run (float x, const float* p)
    {
        static const int iv[4][4] = { { 0, 4, 7, 12 }, { 0, 3, 7, 12 }, { 0, 7, 12, 19 }, { 0, 5, 7, 12 } };
        const int ch = std::min (3, std::max (0, (int) std::lround (p[1])));
        float sum = 0;
        for (int i = 0; i < 4; ++i)
        {
            const float f = 440.f * std::pow (2.f, (std::round (p[0]) + (float) iv[ch][i] - 69.f) / 12.f);
            const float o = d[i].read ((float) sr / std::min (f, 4000.f));
            d[i].push (x + p[2] * t[i].run (o, p[3], sr));
            sum += o;
        }
        return sum * (1.f - p[2]) * 1.5f;
    }
};
struct FreezeFx : Fx   // freeze, tamano ms
{
    double sr = 48000; Delay dl, dr; bool frozen = false; int fpos = 0, fsize = 1024; float fph = 0, fg = 0;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.6)); dr.init ((int) (s * 0.6)); dl.clear(); dr.clear(); frozen = false; fph = fg = 0; }
    static float rd (const Delay& d, float idx)
    {
        const int i = (int) std::floor (idx); const float f = idx - (float) i;
        return d.at (i) * (1.f - f) + d.at (i + 1) * f;
    }
    void tick (float& l, float& r, const float* p) override
    {
        const bool want = p[0] > 0.5f;
        if (want && ! frozen) { frozen = true; fpos = dl.w; fsize = std::max (64, (int) (p[1] * 0.001f * (float) sr)); fph = 0; }
        if (! want) frozen = false;
        fg += ((frozen ? 1.f : 0.f) - fg) * (1.f - msToCoef (10.f, sr));
        if (! frozen) { dl.push (l); dr.push (r); }
        if (fg < 0.0005f) return;
        float ph2 = fph + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
        const float s1 = std::sin (kPi * fph), s2 = std::sin (kPi * ph2), w1 = s1 * s1, w2 = s2 * s2;
        const float i1 = (float) (fpos - fsize) + fph * (float) fsize, i2 = (float) (fpos - fsize) + ph2 * (float) fsize;
        const float fl = rd (dl, i1) * w1 + rd (dl, i2) * w2, fr = rd (dr, i1) * w1 + rd (dr, i2) * w2;
        fph += 1.f / (float) fsize; if (fph >= 1.f) fph -= 1.f;
        l = lerp (l, fl, fg); r = lerp (r, fr, fg);
    }
};
}
