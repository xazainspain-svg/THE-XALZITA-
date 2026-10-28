#pragma once
#include "Dsp.h"
#include <memory>
#include <string>

namespace xfx
{
constexpr int kMaxParams = 6;

// kind: 0 = float, 1 = bool, 2 = int. skew = -1 -> escala logaritmica (centro geometrico).
struct Param { const char* name; float min, max, def; float skew; int kind; };

inline Param F (const char* n, float mn, float mx, float d) { return { n, mn, mx, d, 1.f, 0 }; }
inline Param L (const char* n, float mn, float mx, float d) { return { n, mn, mx, d, -1.f, 0 }; }
inline Param B (const char* n, bool d)                       { return { n, 0.f, 1.f, d ? 1.f : 0.f, 1.f, 1 }; }
inline Param I (const char* n, int mn, int mx, int d)        { return { n, (float) mn, (float) mx, (float) d, 1.f, 2 }; }

// Un efecto procesa un par estereo muestra a muestra. p[] = parametros ya suavizados.
// tick() debe devolver la senal "wet"; la mezcla dry/wet y el nivel de salida los pone el plugin.
struct Fx
{
    virtual ~Fx() = default;
    virtual void prepare (double sr) = 0;
    virtual void tick (float& l, float& r, const float* p) = 0;
};

// Efecto "por canal": S necesita init(sr) y float run(float x, const float* p).
template <class S>
struct PerCh : Fx
{
    S s[2];
    void prepare (double sr) override { s[0].init (sr); s[1].init (sr); }
    void tick (float& l, float& r, const float* p) override { l = s[0].run (l, p); r = s[1].run (r, p); }
};

struct Info
{
    const char* name;
    const char* category;
    float defMix;
    std::vector<Param> params;
    std::unique_ptr<Fx> (*make)();
};

template <class T> std::unique_ptr<Fx> mk() { return std::make_unique<T>(); }
}
