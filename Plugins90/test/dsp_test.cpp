// Prueba de humo de los 90 efectos: sin NaN/Inf, sin explosion de nivel, en varias frecuencias de muestreo
// y con parametros por defecto, minimos, maximos y aleatorios.
#include "../Registry.h"
#include <cstdio>
#include <cstdlib>

using namespace xfx;

static bool runOne (const Info& info, double sr, int mode, uint32_t seed, double& rmsOutDb, float& peakOut)
{
    auto fx = info.make(); fx->prepare (sr);
    Rng rng; rng.s = seed | 1u;
    float p[kMaxParams] = {};
    const int n = (int) info.params.size();
    for (int i = 0; i < n; ++i)
    {
        const Param& q = info.params[(size_t) i];
        p[i] = mode == 0 ? q.def : mode == 1 ? q.min : mode == 2 ? q.max : lerp (q.min, q.max, rng.uni());
        if (q.kind != 0) p[i] = std::round (p[i]);
    }
    const int N = (int) (sr * 3); double acc = 0; float peak = 0; double phase = 0;
    for (int i = 0; i < N; ++i)
    {
        if (mode == 3 && (i % 4800) == 0)   // movimiento de parametros durante el audio
            for (int k = 0; k < n; ++k) { const Param& q = info.params[(size_t) k]; p[k] = lerp (q.min, q.max, rng.uni()); if (q.kind != 0) p[k] = std::round (p[k]); }
        phase += 220.0 / sr; phase -= std::floor (phase);
        const float env = (i / 24000) % 2 ? 0.05f : 0.5f;                // golpes fuertes y suaves
        float l = env * (0.7f * std::sin (kTwoPi * (float) phase) + 0.3f * rng.bi());
        float r = env * (0.7f * std::sin (kTwoPi * (float) phase * 1.5f) + 0.3f * rng.bi());
        fx->tick (l, r, p);
        if (! std::isfinite (l) || ! std::isfinite (r)) { printf ("  NaN/Inf en '%s' (sr %.0f modo %d) muestra %d\n", info.name, sr, mode, i); return false; }
        peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
        if (i > N / 2) acc += 0.5 * (l * l + r * r);
    }
    rmsOutDb = 10 * std::log10 (acc / (N / 2) + 1e-20); peakOut = peak;
    return true;
}

#include <fstream>
#include <string>

// uso: dsp_test            -> prueba de estabilidad
//      dsp_test --list     -> imprime "Nombre|Categoria" (formato de plugins.txt)
//      dsp_test plugins.txt-> ademas comprueba que plugins.txt coincide con el registro
int main (int argc, char** argv)
{
    const auto& reg = xfx::registry(); int bad = 0;
    if (argc > 1 && std::string (argv[1]) == "--list")
    {
        for (auto& i : reg) printf ("%s|%s\n", i.name, i.category);
        return 0;
    }
    if (argc > 1)
    {
        std::ifstream f (argv[1]); std::string line; size_t k = 0;
        while (std::getline (f, line))
        {
            if (line.empty()) continue;
            if (k >= reg.size() || line != std::string (reg[k].name) + "|" + reg[k].category) { printf ("plugins.txt no coincide en la linea %zu: %s\n", k + 1, line.c_str()); return 1; }
            ++k;
        }
        if (k != reg.size()) { printf ("plugins.txt tiene %zu lineas, el registro %zu\n", k, reg.size()); return 1; }
    }
    printf ("%zu efectos\n", reg.size());
    for (size_t k = 0; k < reg.size(); ++k)
    {
        const Info& info = reg[k];
        if (info.params.size() > (size_t) kMaxParams) { printf ("demasiados parametros: %s\n", info.name); ++bad; }
        double rms = 0; float pk = 0; double worst = -1e9; float worstPk = 0;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int mode = 0; mode < 4; ++mode)
            {
                if (! runOne (info, sr, mode, 777u + (uint32_t) mode, rms, pk)) { ++bad; goto next; }
                worst = std::max (worst, rms); worstPk = std::max (worstPk, pk);
                if (pk > 50.f) { printf ("  nivel excesivo en '%s' (sr %.0f modo %d): pico %.1f\n", info.name, sr, mode, pk); ++bad; goto next; }
            }
        runOne (info, 48000.0, 0, 1u, rms, pk);
        printf ("%2zu %-18s %-11s defaults: RMS %6.1f dB  pico %5.2f | peor caso: RMS %6.1f dB pico %6.2f\n", k + 1, info.name, info.category, rms, pk, worst, worstPk);
        next:;
    }
    printf (bad ? "FALLOS: %d\n" : "OK: todos los efectos son estables\n", bad);
    return bad ? 1 : 0;
}
