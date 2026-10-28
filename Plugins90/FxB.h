#pragma once
// Parte B: distorsion (14) y modulacion (14).
#include "Fx.h"

namespace xfx
{
// LP de un polo con recalculo barato del coeficiente.
struct Tone
{
    OnePole o; int c = 0;
    float run (float x, float f, double sr) { if ((c++ & 15) == 0) o.setFreq (f, sr); return o.lp (x); }
    void reset() { o.reset(); c = 0; }
};
inline float compGain (float g, float k = 0.35f) { return 1.f / std::pow (std::max (g, 1.f), k); }

// -------------------------------------------------------------- Distorsion
struct Saturator    // drive dB, tone Hz, forma (tanh->atan)
{
    double sr = 48000; Tone t;
    void init (double s) { sr = s; t.reset(); }
    float run (float x, const float* p)
    {
        const float g = dbToLin (p[0]);
        const float y = lerp (std::tanh (g * x), 0.6366f * std::atan (1.5f * g * x), p[2]) * compGain (g);
        return t.run (y, p[1], sr);
    }
};
struct TubeSat     // drive dB, bias, tone Hz
{
    double sr = 48000; Tone t; DcBlock dc;
    void init (double s) { sr = s; t.reset(); dc.reset(); }
    float run (float x, const float* p)
    {
        const float g = dbToLin (p[0]);
        const float y = (std::tanh (g * x + p[1]) - std::tanh (p[1])) * compGain (g);
        return t.run (dc.run (y), p[2], sr);
    }
};
struct TapeSat     // drive dB, tone Hz, bump dB
{
    double sr = 48000; Tone t; Flt bump;
    void init (double s) { sr = s; t.reset(); bump.reset(); }
    float run (float x, const float* p)
    {
        bump.upd (Biquad::LS, 90.f, 0.707f, p[2], sr);
        const float g = dbToLin (p[0]), v = bump.run (x) * g;
        return t.run (v / (1.f + std::fabs (v)) * compGain (g, 0.3f) * 1.4f, p[1], sr);
    }
};
struct HardClipper // umbral dB, ganancia dB
{
    void init (double) {}
    float run (float x, const float* p) { const float T = dbToLin (p[0]); return clampf (x * dbToLin (p[1]), -T, T); }
};
struct SoftClipper // drive dB
{
    void init (double) {}
    float run (float x, const float* p)
    {
        const float v = clampf (x * dbToLin (p[0]), -1.5f, 1.5f);
        return v - v * v * v / 6.75f;
    }
};
struct Foldback    // umbral dB, ganancia dB
{
    void init (double) {}
    float run (float x, const float* p)
    {
        const float T = std::max (dbToLin (p[0]), 1e-3f), v = x * dbToLin (p[1]);
        float m = std::fmod (v - T, 4.f * T); if (m < 0) m += 4.f * T;
        return std::fabs (m - 2.f * T) - T;
    }
};
struct BitCrusher  // bits, downsample
{
    float hold = 0; int cnt = 0;
    void init (double) { hold = 0; cnt = 0; }
    float run (float x, const float* p)
    {
        if (cnt <= 0) { const float L = std::pow (2.f, p[0] - 1.f); hold = std::round (clampf (x, -1.f, 1.f) * L) / L; cnt = (int) p[1]; }
        --cnt;
        return hold;
    }
};
struct Decimator   // rate Hz, jitter, suavizado
{
    double sr = 48000; float ph = 0, hold = 0, prev = 0; Rng rng;
    void init (double s) { sr = s; ph = hold = prev = 0; }
    float run (float x, const float* p)
    {
        ph += p[0] / (float) sr;
        if (ph >= 1.f) { hold = x; ph -= 1.f; ph -= p[1] * rng.uni() * 0.9f; if (ph < 0) ph = 0; }
        const float a = p[2] * 0.95f;
        prev = hold * (1 - a) + prev * a;
        return prev;
    }
};
struct Rectifier   // tipo (media->onda completa), cantidad
{
    DcBlock dc;
    void init (double) { dc.reset(); }
    float run (float x, const float* p)
    {
        const float half = x > 0 ? x : 0.f, full = std::fabs (x);
        return dc.run (lerp (x, lerp (half, full, p[0]), p[1]));
    }
};
struct FuzzFace    // fuzz dB, starve, tone Hz
{
    double sr = 48000; Tone t; DcBlock dc;
    void init (double s) { sr = s; t.reset(); dc.reset(); }
    float run (float x, const float* p)
    {
        const float v = x * dbToLin (p[0]);
        float y = v > 0 ? 1.f - std::exp (-v) : -0.8f * (1.f - std::exp (v));
        y *= (1.f - p[1]) + p[1] * std::min (1.f, std::fabs (v) * 5.f);
        return t.run (dc.run (y), p[2], sr) * 0.7f;
    }
};
struct Overdrive   // drive dB, tone Hz, nivel dB
{
    double sr = 48000; Tone t; Flt hp;
    void init (double s) { sr = s; t.reset(); hp.reset(); }
    float run (float x, const float* p)
    {
        hp.upd (Biquad::HP, 700.f, 0.707f, 0.f, sr);
        const float g = dbToLin (p[0]), pre = x + hp.run (x);
        return t.run (std::tanh (g * pre) * compGain (g, 0.4f), p[1], sr) * dbToLin (p[2]);
    }
};
struct SineShaper  // drive, bias
{
    DcBlock dc;
    void init (double) { dc.reset(); }
    float run (float x, const float* p)
    {
        const float k = (1.f + p[0] * 10.f) * 1.5708f;
        return dc.run (std::sin (k * x + p[1]) - std::sin (p[1]));
    }
};
struct AsymDist    // drive dB, asimetria
{
    DcBlock dc;
    void init (double) { dc.reset(); }
    float run (float x, const float* p)
    {
        const float g = dbToLin (p[0]), a = clampf (p[1], -0.95f, 0.95f), v = x * g;
        return dc.run (std::tanh (v > 0 ? v * (1.f + a) : v * (1.f - a)) * compGain (g));
    }
};
struct Exciter     // freq Hz, cantidad, tipo (impar->par)
{
    double sr = 48000; Flt hp, hp2;
    void init (double s) { sr = s; hp.reset(); hp2.reset(); }
    float run (float x, const float* p)
    {
        hp.upd (Biquad::HP, p[0], 0.707f, 0.f, sr); hp2.upd (Biquad::HP, p[0], 0.707f, 0.f, sr);
        const float h = hp.run (x);
        const float har = hp2.run (lerp (std::tanh (4.f * h) * 0.5f, std::fabs (h) * 2.f, p[2]));
        return x + p[1] * har;
    }
};

// -------------------------------------------------------------- Modulacion
inline float shapeLfo (float s, float k) { return lerp (s, std::tanh (8.f * s) / std::tanh (8.f), k); }

struct ChorusFx : Fx   // rate, depth ms, delay ms, ancho
{
    double sr = 48000; Delay dl, dr; Lfo lfo;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.07)); dr.init ((int) (s * 0.07)); dl.clear(); dr.clear(); lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[0], sr); float ph2 = ph + p[3] * 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
        const float wl = dl.read ((p[2] + p[1] * Lfo::sine (ph)) * 0.001f * (float) sr);
        const float wr = dr.read ((p[2] + p[1] * Lfo::sine (ph2)) * 0.001f * (float) sr);
        dl.push (l); dr.push (r); l = wl; r = wr;
    }
};
struct FlangerFx : Fx  // rate, depth, feedback, manual ms
{
    double sr = 48000; Delay dl, dr; Lfo lfo;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.03)); dr.init ((int) (s * 0.03)); dl.clear(); dr.clear(); lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[0], sr); float ph2 = ph + 0.25f; if (ph2 >= 1.f) ph2 -= 1.f;
        const float wl = dl.read (p[3] * (1.f + p[1] * 0.9f * Lfo::sine (ph))  * 0.001f * (float) sr);
        const float wr = dr.read (p[3] * (1.f + p[1] * 0.9f * Lfo::sine (ph2)) * 0.001f * (float) sr);
        dl.push (l + p[2] * wl); dr.push (r + p[2] * wr); l = wl; r = wr;
    }
};
struct PhaserFx : Fx   // rate, depth, feedback, centro Hz
{
    double sr = 48000; AP1 ap[2][6]; float last[2] = { 0, 0 }; Lfo lfo;
    void prepare (double s) override { sr = s; for (auto& c : ap) for (auto& a : c) a.z = 0; last[0] = last[1] = 0; lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float s = Lfo::sine (lfo.tick (p[0], sr));
        const float fc = clampf (p[3] * std::pow (2.f, p[1] * 2.f * s), 20.f, (float) sr * 0.4f);
        const float t = std::tan (kPi * fc / (float) sr), a = (t - 1.f) / (t + 1.f);
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            float y = *io[c] + p[2] * last[c];
            for (auto& st : ap[c]) y = st.run (y, a);
            last[c] = y; *io[c] = y;
        }
    }
};
struct VibratoS    // rate, depth ms
{
    double sr = 48000; Delay d; Lfo lfo;
    void init (double s) { sr = s; d.init ((int) (s * 0.03)); d.clear(); lfo.ph = 0; }
    float run (float x, const float* p)
    {
        const float s = Lfo::sine (lfo.tick (p[0], sr));
        const float y = d.read ((2.f + p[1] * (1.f + s)) * 0.001f * (float) sr);
        d.push (x); return y;
    }
};
struct AutoPanFx : Fx  // rate, depth, forma
{
    double sr = 48000; Lfo lfo;
    void prepare (double s) override { sr = s; lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float pan = shapeLfo (Lfo::sine (lfo.tick (p[0], sr)), p[2]);
        const float th = (1.f + pan * p[1]) * kPi * 0.25f;
        l *= std::cos (th) * 1.4142f; r *= std::sin (th) * 1.4142f;
    }
};
struct RingMod     // freq Hz, forma (seno->cuadrada)
{
    double sr = 48000; Lfo lfo;
    void init (double s) { sr = s; lfo.ph = 0; }
    float run (float x, const float* p)
    {
        const float c = Lfo::sine (lfo.tick (p[0], sr));
        return x * lerp (c, c > 0 ? 1.f : -1.f, p[1]);
    }
};
struct RotaryFx : Fx   // velocidad Hz, profundidad, crossover Hz
{
    double sr = 48000; Flt lp; Delay horn; float hph = 0, dph = 0;
    void prepare (double s) override { sr = s; lp.reset(); horn.init ((int) (s * 0.01)); horn.clear(); hph = dph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        hph += p[0] / (float) sr; if (hph >= 1.f) hph -= 1.f;
        dph += p[0] * 0.67f / (float) sr; if (dph >= 1.f) dph -= 1.f;
        const float m = 0.5f * (l + r);
        lp.upd (Biquad::LP, p[2], 0.707f, 0.f, sr);
        const float low = lp.run (m), high = m - low;
        const float hl = Lfo::sine (hph), hr = Lfo::sine (hph + 0.25f), dl = Lfo::sine (dph), dr = -dl;
        const float sm = 0.001f * (float) sr, k = p[1];
        const float wl = horn.read ((0.6f + 0.4f * k * hl) * sm), wr = horn.read ((0.6f + 0.4f * k * hr) * sm);
        horn.push (high);
        l = low * (1.f - 0.3f * k * (0.5f + 0.5f * dl)) + wl * (1.f - 0.4f * k * (0.5f + 0.5f * hl));
        r = low * (1.f - 0.3f * k * (0.5f + 0.5f * dr)) + wr * (1.f - 0.4f * k * (0.5f + 0.5f * hr));
    }
};
struct EnsembleFx : Fx // rate, depth
{
    double sr = 48000; Delay dl, dr; Lfo lfo;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.03)); dr.init ((int) (s * 0.03)); dl.clear(); dr.clear(); lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[0], sr), sm = 0.001f * (float) sr;
        float wl = 0, wr = 0;
        for (int i = 0; i < 3; ++i)
        {
            wl += dl.read ((12.f + 5.f * p[1] * Lfo::sine (ph + (float) i / 3.f)) * sm);
            wr += dr.read ((12.f + 5.f * p[1] * Lfo::sine (ph + (float) i / 3.f + 1.f / 6.f)) * sm);
        }
        dl.push (l); dr.push (r); l = wl / 3.f; r = wr / 3.f;
    }
};
struct FreqShifter : Fx // shift Hz
{
    double sr = 48000; AP2 ai[2][4], aq[2][4]; float dI[2] = { 0, 0 }; double osc = 0;
    void prepare (double s) override
    {
        sr = s; osc = 0;
        static const float ci[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static const float cq[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int c = 0; c < 2; ++c) { for (int k = 0; k < 4; ++k) { ai[c][k] = AP2(); aq[c][k] = AP2(); ai[c][k].a2 = ci[k] * ci[k]; aq[c][k].a2 = cq[k] * cq[k]; } dI[c] = 0; }
    }
    void tick (float& l, float& r, const float* p) override
    {
        osc += (double) p[0] / sr; osc -= std::floor (osc);
        const float cs = std::cos (kTwoPi * (float) osc), sn = std::sin (kTwoPi * (float) osc);
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            float a = *io[c], b = *io[c];
            for (int k = 0; k < 4; ++k) { a = ai[c][k].run (a); b = aq[c][k].run (b); }
            const float I = dI[c]; dI[c] = a;           // la rama I se retrasa 1 muestra
            *io[c] = I * cs + b * sn;
        }
    }
};
struct WobbleS     // rate, depth, Q, base Hz
{
    double sr = 48000; Flt f; Lfo lfo;
    void init (double s) { sr = s; f.reset(); lfo.ph = 0; }
    float run (float x, const float* p)
    {
        const float s = 0.5f + 0.5f * Lfo::sine (lfo.tick (p[0], sr));
        f.upd (Biquad::LP, p[3] * std::pow (2.f, p[1] * 5.f * s), p[2], 0.f, sr);
        return f.run (x);
    }
};
struct HarmTrem    // rate, depth, crossover Hz
{
    double sr = 48000; OnePole lp; Lfo lfo; int c = 0;
    void init (double s) { sr = s; lp.reset(); lfo.ph = 0; c = 0; }
    float run (float x, const float* p)
    {
        if ((c++ & 15) == 0) lp.setFreq (p[2], sr);
        const float s = Lfo::sine (lfo.tick (p[0], sr)), low = lp.lp (x), high = x - low;
        return low * (1.f - p[1] * (0.5f + 0.5f * s)) + high * (1.f - p[1] * (0.5f - 0.5f * s));
    }
};
struct DimensionFx : Fx // cantidad, rate, profundidad ms
{
    double sr = 48000; Delay dl, dr; Lfo lfo;
    void prepare (double s) override { sr = s; dl.init ((int) (s * 0.03)); dr.init ((int) (s * 0.03)); dl.clear(); dr.clear(); lfo.ph = 0; }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[1], sr), sm = 0.001f * (float) sr, m = 0.5f * (l + r);
        const float wl = dl.read ((8.f + p[2] * Lfo::sine (ph)) * sm);
        const float wr = dr.read ((9.f + p[2] * Lfo::sine (ph + 0.33f)) * sm);
        dl.push (m); dr.push (m);
        l += p[0] * wl; r -= p[0] * wr;
    }
};
struct PitchShiftS // semitonos, ventana ms, cents
{
    double sr = 48000; PitchCore pc;
    void init (double s) { sr = s; pc.init (s); }
    float run (float x, const float* p)
    {
        return pc.run (x, std::pow (2.f, (p[0] + p[2] * 0.01f) / 12.f), p[1] * 0.001f * (float) sr);
    }
};
struct DoublerFx : Fx  // desafinacion cents, ventana ms
{
    double sr = 48000; PitchCore pl, pr;
    void prepare (double s) override { sr = s; pl.init (s); pr.init (s); }
    void tick (float& l, float& r, const float* p) override
    {
        const float win = p[1] * 0.001f * (float) sr, k = p[0] / 1200.f;
        l = pl.run (l, std::pow (2.f, k), win); r = pr.run (r, std::pow (2.f, -k), win);
    }
};
}
