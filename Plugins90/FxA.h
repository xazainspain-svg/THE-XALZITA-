#pragma once
// Parte A: filtros (12) y dinamica (12).
#include "Fx.h"

namespace xfx
{
// ---------------------------------------------------------------- Filtros
template <int T> struct BQ      // p0 freq, p1 Q
{
    double sr = 48000; Flt f;
    void init (double s) { sr = s; f.reset(); }
    float run (float x, const float* p) { f.upd ((Biquad::Type) T, p[0], p[1], 0.f, sr); return f.run (x); }
};
template <int T> struct BQG     // shelf: p0 freq, p1 ganancia dB
{
    double sr = 48000; Flt f;
    void init (double s) { sr = s; f.reset(); }
    float run (float x, const float* p) { f.upd ((Biquad::Type) T, p[0], 0.707f, p[1], sr); return f.run (x); }
};
struct PeakEq   // p0 freq, p1 dB, p2 Q
{
    double sr = 48000; Flt f;
    void init (double s) { sr = s; f.reset(); }
    float run (float x, const float* p) { f.upd (Biquad::Peak, p[0], p[2], p[1], sr); return f.run (x); }
};
struct AutoWah  // p0 sens, p1 freq base, p2 Q, p3 release ms
{
    double sr = 48000; Env e; Flt f;
    void init (double s) { sr = s; e.reset(); f.reset(); }
    float run (float x, const float* p)
    {
        const float en = e.run (x, msToCoef (2.f, sr), msToCoef (p[3], sr));
        const float fc = p[1] * std::pow (2.f, clampf (en * p[0] * 10.f, 0.f, 5.f));
        f.upd (Biquad::BP, fc, p[2], 0.f, sr);
        return f.run (x) * 2.f;
    }
};
struct Vowel    // p0 vocal 0..4 (a e i o u), p1 Q, p2 brillo
{
    double sr = 48000; Flt f1, f2;
    void init (double s) { sr = s; f1.reset(); f2.reset(); }
    float run (float x, const float* p)
    {
        static const float F1[5] = { 800, 400, 350, 450, 325 }, F2[5] = { 1150, 1600, 2300, 800, 700 };
        const float v = clampf (p[0], 0.f, 3.999f); const int i = (int) v; const float t = v - (float) i;
        f1.upd (Biquad::BP, lerp (F1[i], F1[i + 1], t), p[1], 0.f, sr);
        f2.upd (Biquad::BP, lerp (F2[i], F2[i + 1], t), p[1], 0.f, sr);
        return (f1.run (x) + f2.run (x) * (0.4f + p[2])) * 1.8f;
    }
};
struct CombF    // p0 freq Hz, p1 feedback
{
    double sr = 48000; Delay d;
    void init (double s) { sr = s; d.init ((int) (s / 20.0) + 8); d.clear(); }
    float run (float x, const float* p)
    {
        const float o = x + p[1] * d.read ((float) sr / clampf (p[0], 20.f, 4000.f));
        d.push (o);
        return o * (1.f - 0.5f * std::fabs (p[1]));
    }
};
struct DjFilter // p0 posicion -1 (LP) .. +1 (HP), p1 resonancia
{
    double sr = 48000; Flt f;
    void init (double s) { sr = s; f.reset(); }
    float run (float x, const float* p)
    {
        const float pos = p[0];
        if (std::fabs (pos) < 0.02f) return x;
        if (pos < 0) f.upd (Biquad::LP, 20.f * std::pow (1000.f, 1.f + pos), 0.7f + p[1], 0.f, sr);
        else         f.upd (Biquad::HP, 20.f * std::pow (1000.f, pos),       0.7f + p[1], 0.f, sr);
        return f.run (x);
    }
};
struct TiltEq   // p0 freq, p1 tilt dB
{
    double sr = 48000; Flt ls, hs;
    void init (double s) { sr = s; ls.reset(); hs.reset(); }
    float run (float x, const float* p)
    {
        ls.upd (Biquad::LS, p[0], 0.707f,  p[1], sr);
        hs.upd (Biquad::HS, p[0], 0.707f, -p[1], sr);
        return hs.run (ls.run (x));
    }
};

// -------------------------------------------------------------- Dinamica
inline float det2 (float l, float r) { return std::max (std::fabs (l), std::fabs (r)); }

struct CompFx : Fx   // thr, ratio, attack, release, makeup
{
    double sr = 48000; CompCore cc;
    void prepare (double s) override { sr = s; cc.env.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float gr = cc.gr (det2 (l, r), p[0], p[1], msToCoef (p[2], sr), msToCoef (p[3], sr));
        const float g = dbToLin (p[4] - gr); l *= g; r *= g;
    }
};
struct LimiterFx : Fx   // ceiling dB, release ms
{
    double sr = 48000; float g = 1;
    void prepare (double s) override { sr = s; g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        const float c = dbToLin (p[0]), d = det2 (l, r), target = d > c ? c / d : 1.f;
        if (target < g) g = target; else g = target + (g - target) * msToCoef (p[1], sr);
        l *= g; r *= g;
    }
};
struct GateFx : Fx      // thr, range dB, attack, release
{
    double sr = 48000; Env e; float g = 1; bool open = false;
    void prepare (double s) override { sr = s; e.reset(); g = 1; open = false; }
    void tick (float& l, float& r, const float* p) override
    {
        const float lvl = linToDb (e.run (det2 (l, r), msToCoef (0.5f, sr), msToCoef (20.f, sr)));
        if (! open && lvl > p[0]) open = true; else if (open && lvl < p[0] - 4.f) open = false;
        const float target = open ? 1.f : dbToLin (p[1]);
        g = target + (g - target) * msToCoef (target > g ? p[2] : p[3], sr);
        l *= g; r *= g;
    }
};
struct ExpanderFx : Fx  // thr, ratio, release
{
    double sr = 48000; Env e; float g = 1;
    void prepare (double s) override { sr = s; e.reset(); g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        const float lvl = linToDb (e.run (det2 (l, r), msToCoef (1.f, sr), msToCoef (30.f, sr)));
        const float gdb = lvl < p[0] ? std::max (-80.f, (lvl - p[0]) * (p[1] - 1.f)) : 0.f;
        const float target = dbToLin (gdb);
        g = target + (g - target) * msToCoef (target > g ? p[2] : 2.f, sr);
        l *= g; r *= g;
    }
};
struct DeEsserFx : Fx   // freq, thr, amount
{
    double sr = 48000; Flt hl, hr; Env e;
    void prepare (double s) override { sr = s; hl.reset(); hr.reset(); e.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        hl.upd (Biquad::HP, p[0], 0.7f, 0.f, sr); hr.upd (Biquad::HP, p[0], 0.7f, 0.f, sr);
        const float a = hl.run (l), b = hr.run (r);
        const float lvl = linToDb (e.run (det2 (a, b), msToCoef (0.5f, sr), msToCoef (25.f, sr)));
        const float red = std::min (20.f, std::max (0.f, lvl - p[1]) * p[2]);
        const float k = 1.f - dbToLin (-red);
        l -= a * k; r -= b * k;
    }
};
struct TransientFx : Fx // attack -1..1, sustain -1..1
{
    double sr = 48000; Env fast, slow;
    void prepare (double s) override { sr = s; fast.reset(); slow.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float d = det2 (l, r);
        const float f = fast.run (d, msToCoef (0.5f, sr), msToCoef (15.f, sr));
        const float s = slow.run (d, msToCoef (15.f, sr), msToCoef (150.f, sr));
        const float diff = clampf ((f - s) / (s + 1e-3f), -1.f, 1.f);
        const float g = clampf (1.f + p[0] * std::max (diff, 0.f) * 2.f + p[1] * std::max (-diff, 0.f) * 2.f, 0.05f, 4.f);
        l *= g; r *= g;
    }
};
struct UpCompFx : Fx    // thr, amount, max boost dB
{
    double sr = 48000; Env e; float g = 1;
    void prepare (double s) override { sr = s; e.reset(); g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        const float lvl = linToDb (e.run (det2 (l, r), msToCoef (10.f, sr), msToCoef (200.f, sr)));
        const float boost = lvl < p[0] ? std::min (p[2], (p[0] - lvl) * p[1]) : 0.f;
        g += (dbToLin (boost) - g) * 0.002f;
        l *= g; r *= g;
    }
};
struct PumpFx : Fx      // rate Hz, depth, curva
{
    double sr = 48000; Lfo lfo; float g = 1;
    void prepare (double s) override { sr = s; lfo.ph = 0; g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[0], sr);
        const float target = 1.f - p[1] * std::pow (1.f - ph, p[2]);
        g = target + (g - target) * 0.9f;
        l *= g; r *= g;
    }
};
struct LevelerFx : Fx   // target dB, velocidad ms, rango dB
{
    double sr = 48000; Env e; float g = 1;
    void prepare (double s) override { sr = s; e.reset(); g = 1; }
    void tick (float& l, float& r, const float* p) override
    {
        const float lvl = linToDb (e.run (det2 (l, r), msToCoef (30.f, sr), msToCoef (p[1], sr)));
        const float gdb = lvl < -60.f ? 0.f : clampf (p[0] - lvl, -p[2], p[2]);
        g += (dbToLin (gdb) - g) * 0.002f;
        l *= g; r *= g;
    }
};
struct TremoloFx : Fx   // rate, depth, forma (sin->cuadrada), fase estereo
{
    double sr = 48000; Lfo lfo;
    void prepare (double s) override { sr = s; lfo.ph = 0; }
    static float shape (float s, float k) { return lerp (s, std::tanh (8.f * s) / std::tanh (8.f), k); }
    void tick (float& l, float& r, const float* p) override
    {
        const float ph = lfo.tick (p[0], sr);
        float ph2 = ph + p[3] * 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
        l *= 1.f - p[1] * (0.5f + 0.5f * shape (Lfo::sine (ph), p[2]));
        r *= 1.f - p[1] * (0.5f + 0.5f * shape (Lfo::sine (ph2), p[2]));
    }
};
struct SquashFx : Fx    // cantidad
{
    double sr = 48000; CompCore cc;
    void prepare (double s) override { sr = s; cc.env.reset(); }
    void tick (float& l, float& r, const float* p) override
    {
        const float thr = -45.f * p[0];
        const float gr = cc.gr (det2 (l, r), thr, 20.f, msToCoef (0.5f, sr), msToCoef (80.f, sr));
        const float g = dbToLin (-0.6f * thr - gr); l *= g; r *= g;
    }
};
struct PeakClip   // ceiling dB, suavidad
{
    void init (double) {}
    float run (float x, const float* p)
    {
        const float c = dbToLin (p[0]), y = x / c;
        return c * lerp (clampf (y, -1.f, 1.f), std::tanh (y), p[1]);
    }
};
}
