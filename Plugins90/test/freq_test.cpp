// Verifica que Freq Shifter y Pitch Shifter mueven la frecuencia en la direccion y cantidad correctas.
#include "../Registry.h"
#include <cstdio>
using namespace xfx;
static double goertzel (const std::vector<float>& x, double f, double sr)
{
    double w = 2 * 3.14159265358979 * f / sr, c = 2 * std::cos (w), s1 = 0, s2 = 0;
    for (float v : x) { double s = v + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / x.size();
}
static std::vector<float> run (const char* name, std::vector<float> p, double f, double sr)
{
    for (auto& i : registry()) if (std::string (i.name) == name)
    {
        auto fx = i.make(); fx->prepare (sr); std::vector<float> out;
        for (int n = 0; n < (int) sr; ++n)
        {
            float l = 0.5f * std::sin (kTwoPi * (float) std::fmod (f * n / sr, 1.0)), r = l;
            fx->tick (l, r, p.data()); if (n > sr / 2) out.push_back (l);
        }
        return out;
    }
    return {};
}
int main()
{
    const double sr = 48000; int bad = 0;
    auto a = run ("Freq Shifter", { 200 }, 1000, sr);
    double up = goertzel (a, 1200, sr), dn = goertzel (a, 800, sr), orig = goertzel (a, 1000, sr);
    printf ("FreqShift +200: 1200Hz=%.4f 800Hz=%.4f 1000Hz=%.4f\n", up, dn, orig);
    if (! (up > 10 * dn && up > 10 * orig)) { puts ("FALLO freq shifter"); ++bad; }
    a = run ("Freq Shifter", { -200 }, 1000, sr);
    if (! (goertzel (a, 800, sr) > 10 * goertzel (a, 1200, sr))) { puts ("FALLO freq shifter (negativo)"); ++bad; }
    a = run ("Pitch Shifter", { 12, 60, 0 }, 1000, sr);
    double o2 = goertzel (a, 2000, sr), o1 = goertzel (a, 1000, sr);
    printf ("Pitch +12st: 2000Hz=%.4f 1000Hz=%.4f\n", o2, o1);
    if (! (o2 > 3 * o1)) { puts ("FALLO pitch shifter"); ++bad; }
    // Corrector: 215 Hz (A3 40 cents baja) en cromatica debe acercarse a 220 Hz.
    a = run ("Pitch Corrector", { 0, 0, 20, 1 }, 215, sr);
    double c220 = goertzel (a, 220, sr), c215 = goertzel (a, 215, sr);
    printf ("Pitch Corrector 215 Hz: 220Hz=%.4f 215Hz=%.4f\n", c220, c215);
    if (! (c220 > 1.5 * c215)) { puts ("FALLO pitch corrector"); ++bad; }
    a = run ("Low Pass", { 500, 0.707f }, 5000, sr);
    double lp = goertzel (a, 5000, sr); printf ("LowPass 500Hz sobre 5k: %.5f (esperado ~0.0015)\n", lp);
    if (lp > 0.01) { puts ("FALLO lowpass"); ++bad; }
    return bad;
}
