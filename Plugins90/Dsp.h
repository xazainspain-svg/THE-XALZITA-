#pragma once
// Primitivas DSP sin dependencias (ni JUCE): se pueden probar con un g++ normal.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace xfx
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToLin (float d)  { return std::pow (10.f, d * 0.05f); }
inline float linToDb (float x)  { return 20.f * std::log10 (std::max (x, 1e-9f)); }
inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline float msToCoef (float ms, double sr)     { return std::exp (-1.f / (std::max (ms, 0.01f) * 0.001f * (float) sr)); }

struct Rng
{
    uint32_t s;
    Rng() { static uint32_t seed = 12345u; seed = seed * 1664525u + 1013904223u; s = seed | 1u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni()  { return (next() >> 8) * (1.f / 16777216.f); }   // [0,1)
    float bi()   { return uni() * 2.f - 1.f; }                    // [-1,1)
};

struct Biquad
{
    enum Type { LP, HP, BP, Notch, LS, HS, Peak, AP };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void set (Type t, float f, float q, float gdb, double sr)
    {
        f = clampf (f, 10.f, (float) sr * 0.45f);
        q = std::max (q, 0.1f);
        const float w = kTwoPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float A = std::pow (10.f, gdb / 40.f);
        float al = s / (2.f * q), a0 = 1.f;
        switch (t)
        {
            case LP:    b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
            case HP:    b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
            case BP:    b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
            case Notch: b0 = 1; b1 = -2 * c; b2 = 1; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
            case AP:    b0 = 1 - al; b1 = -2 * c; b2 = 1 + al; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
            case Peak:  b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; break;
            case LS:
            {
                const float beta = 2.f * std::sqrt (A) * (s / 1.41421356f);
                b0 = A * ((A + 1) - (A - 1) * c + beta);
                b1 = 2 * A * ((A - 1) - (A + 1) * c);
                b2 = A * ((A + 1) - (A - 1) * c - beta);
                a0 = (A + 1) + (A - 1) * c + beta;
                a1 = -2 * ((A - 1) + (A + 1) * c);
                a2 = (A + 1) + (A - 1) * c - beta;
                break;
            }
            case HS:
            {
                const float beta = 2.f * std::sqrt (A) * (s / 1.41421356f);
                b0 = A * ((A + 1) + (A - 1) * c + beta);
                b1 = -2 * A * ((A - 1) + (A + 1) * c);
                b2 = A * ((A + 1) + (A - 1) * c - beta);
                a0 = (A + 1) - (A - 1) * c + beta;
                a1 = 2 * ((A - 1) - (A + 1) * c);
                a2 = (A + 1) - (A - 1) * c - beta;
                break;
            }
        }
        const float inv = 1.f / a0;
        b0 *= inv; b1 *= inv; b2 *= inv; a1 *= inv; a2 *= inv;
    }
    float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Biquad cuyos coeficientes se recalculan cada 16 muestras (barato aunque el parametro se mueva).
struct Flt
{
    Biquad b; int c = 0;
    void upd (Biquad::Type t, float f, float q, float g, double sr) { if ((c++ & 15) == 0) b.set (t, f, q, g, sr); }
    float run (float x) { return b.run (x); }
    void reset() { b.reset(); c = 0; }
};

struct OnePole
{
    float a = 0, z = 0;
    void setFreq (float f, double sr) { a = std::exp (-kTwoPi * clampf (f, 1.f, (float) sr * 0.45f) / (float) sr); }
    float lp (float x) { z = x * (1 - a) + z * a; return z; }
    void reset() { z = 0; }
};

struct DcBlock
{
    float x1 = 0, y1 = 0;
    float run (float x) { const float y = x - x1 + 0.9995f * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// d = muestras hacia atras (1 = la ultima muestra escrita). Leer ANTES de escribir.
struct Delay
{
    std::vector<float> buf; int w = 0, n = 1;
    void init (int maxSamples) { n = std::max (maxSamples, 8) + 4; buf.assign ((size_t) n, 0.f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; if (++w >= n) w = 0; }
    float read (float d) const
    {
        d = clampf (d, 1.f, (float) (n - 3));
        float r = (float) w - d; if (r < 0) r += (float) n;
        int i = (int) r; const float f = r - (float) i; int j = i + 1; if (j >= n) j = 0;
        if (i >= n) i = 0;
        return buf[(size_t) i] * (1 - f) + buf[(size_t) j] * f;
    }
    float at (int idx) const { idx %= n; if (idx < 0) idx += n; return buf[(size_t) idx]; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
};

struct Lfo
{
    float ph = 0;
    float tick (float hz, double sr) { ph += hz / (float) sr; if (ph >= 1.f) ph -= 1.f; return ph; }
    static float sine (float phase) { return std::sin (kTwoPi * phase); }
};

struct Env
{
    float e = 0;
    float run (float x, float a, float r) { x = std::fabs (x); e = x > e ? a * (e - x) + x : r * (e - x) + x; return e; }
    void reset() { e = 0; }
};

// Calculo de reduccion de ganancia (dB) para compresion hacia abajo.
struct CompCore
{
    Env env;
    float gr (float det, float thr, float ratio, float aC, float rC)
    {
        const float lev = linToDb (env.run (det, aC, rC));
        const float over = lev - thr;
        return over > 0.f ? over * (1.f - 1.f / std::max (ratio, 1.f)) : 0.f;
    }
};

// Allpass de 1er orden (coef a) para phaser / dispersion.
struct AP1
{
    float z = 0;
    float run (float x, float a) { const float y = a * x + z; z = x - a * y; return y; }
};

// Allpass tipo Schroeder con delay fijo.
struct AllpassD
{
    Delay d; float len = 1;
    void init (int n) { d.init (n); len = (float) n; }
    float run (float x, float g) { const float z = d.read (len), w = x + g * z; d.push (w); return z - g * w; }
};

// Pitch shifter de dos granos con ventana sin^2 (tambien sirve para reverse con ratio = -1).
struct PitchCore
{
    Delay d; float ph = 0;
    void init (double sr) { d.init ((int) (sr * 0.6)); ph = 0; }
    float run (float x, float ratio, float winSamples)
    {
        ph += (1.f - ratio) / winSamples;
        while (ph >= 1.f) ph -= 1.f;
        while (ph < 0.f)  ph += 1.f;
        float ph2 = ph + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
        const float s1 = std::sin (kPi * ph), s2 = std::sin (kPi * ph2);
        const float y = d.read (ph * winSamples + 2.f) * s1 * s1 + d.read (ph2 * winSamples + 2.f) * s2 * s2;
        d.push (x);
        return y;
    }
};

// Nucleo de reverb tipo Freeverb (8 combs + 4 allpass por canal).
struct RevCore
{
    static constexpr int kC = 8, kA = 4;
    struct Comb { std::vector<float> b; int i = 0; float store = 0; };
    struct Ap   { std::vector<float> b; int i = 0; };
    Comb c[2][kC]; Ap a[2][kA];

    void init (double sr, float scale)
    {
        static const int cl[kC] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int al[kA] = { 556, 441, 341, 225 };
        const float k = (float) sr / 44100.f * scale;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int j = 0; j < kC; ++j) { c[ch][j].b.assign ((size_t) std::max (8, (int) ((cl[j] + ch * 23) * k)), 0.f); c[ch][j].i = 0; c[ch][j].store = 0; }
            for (int j = 0; j < kA; ++j) { a[ch][j].b.assign ((size_t) std::max (8, (int) ((al[j] + ch * 23) * k)), 0.f); a[ch][j].i = 0; }
        }
    }
    float tank (int ch, float in, float fb, float damp)
    {
        float out = 0;
        for (int j = 0; j < kC; ++j)
        {
            Comb& cb = c[ch][j];
            const float o = cb.b[(size_t) cb.i];
            cb.store = o * (1 - damp) + cb.store * damp;
            cb.b[(size_t) cb.i] = in + cb.store * fb;
            if (++cb.i >= (int) cb.b.size()) cb.i = 0;
            out += o;
        }
        for (int j = 0; j < kA; ++j)
        {
            Ap& ap = a[ch][j];
            const float bo = ap.b[(size_t) ap.i];
            const float o = -out + bo;
            ap.b[(size_t) ap.i] = out + bo * 0.5f;
            if (++ap.i >= (int) ap.b.size()) ap.i = 0;
            out = o;
        }
        return out;
    }
    void run (float inL, float inR, float fb, float damp, float width, float& oL, float& oR)
    {
        const float in = (inL + inR) * 0.015f;
        const float l = tank (0, in, fb, damp), r = tank (1, in, fb, damp);
        const float w1 = width * 0.5f + 0.5f, w2 = (1 - width) * 0.5f;
        oL = (l * w1 + r * w2) * 3.f;
        oR = (r * w1 + l * w2) * 3.f;
    }
};

// Par de allpass de 2o orden (en z^-2) para el transformador de Hilbert.
struct AP2
{
    float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float run (float x) { const float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
};
}
