#pragma once
// Los 90 efectos. El orden de esta lista = indice del plugin (ver plugins.txt, que debe coincidir).
#include "FxA.h"
#include "FxB.h"
#include "FxC.h"

namespace xfx
{
inline std::vector<Info> buildRegistry()
{
    std::vector<Info> r;
    auto add = [&] (const char* name, const char* cat, float mix, std::vector<Param> p, std::unique_ptr<Fx> (*make)())
    { r.push_back ({ name, cat, mix, std::move (p), make }); };

    using B_ = Biquad;
    // ---- Filtros (12)
    add ("Low Pass",    "Filters", 1.f, { L ("Cutoff", 20, 20000, 5000), F ("Resonance", 0.5f, 10, 0.707f) }, &mk<PerCh<BQ<B_::LP>>>);
    add ("High Pass",   "Filters", 1.f, { L ("Cutoff", 20, 20000, 200),  F ("Resonance", 0.5f, 10, 0.707f) }, &mk<PerCh<BQ<B_::HP>>>);
    add ("Band Pass",   "Filters", 1.f, { L ("Frequency", 20, 20000, 1000), F ("Q", 0.3f, 20, 2) },            &mk<PerCh<BQ<B_::BP>>>);
    add ("Notch",       "Filters", 1.f, { L ("Frequency", 20, 20000, 1000), F ("Q", 0.3f, 20, 4) },            &mk<PerCh<BQ<B_::Notch>>>);
    add ("Low Shelf",   "Filters", 1.f, { L ("Frequency", 20, 2000, 120),   F ("Gain dB", -18, 18, 6) },        &mk<PerCh<BQG<B_::LS>>>);
    add ("High Shelf",  "Filters", 1.f, { L ("Frequency", 1000, 20000, 8000), F ("Gain dB", -18, 18, 6) },      &mk<PerCh<BQG<B_::HS>>>);
    add ("Peak EQ",     "Filters", 1.f, { L ("Frequency", 20, 20000, 1500), F ("Gain dB", -18, 18, 6), F ("Q", 0.2f, 12, 1) }, &mk<PerCh<PeakEq>>);
    add ("Auto Wah",    "Filters", 1.f, { F ("Sensitivity", 0, 1, 0.6f), L ("Base Freq", 100, 1500, 300), F ("Resonance", 1, 12, 5), L ("Release ms", 10, 500, 120) }, &mk<PerCh<AutoWah>>);
    add ("Vowel",       "Filters", 1.f, { F ("Vowel (A-E-I-O-U)", 0, 4, 0), F ("Resonance", 1, 15, 6), F ("Brightness", 0, 1, 0.5f) }, &mk<PerCh<Vowel>>);
    add ("Comb Filter", "Filters", 1.f, { L ("Frequency", 40, 2000, 200), F ("Feedback", -0.95f, 0.95f, 0.7f) }, &mk<PerCh<CombF>>);
    add ("DJ Filter",   "Filters", 1.f, { F ("LP < 0 > HP", -1, 1, 0), F ("Resonance", 0, 6, 1) },              &mk<PerCh<DjFilter>>);
    add ("Tilt EQ",     "Filters", 1.f, { L ("Pivot", 100, 8000, 1000), F ("Tilt dB", -12, 12, 3) },            &mk<PerCh<TiltEq>>);

    // ---- Dinamica (12)
    add ("Compressor",  "Dynamics", 1.f, { F ("Threshold dB", -60, 0, -18), F ("Ratio", 1, 20, 4), L ("Attack ms", 0.1f, 100, 10), L ("Release ms", 10, 1000, 120), F ("Makeup dB", 0, 24, 0) }, &mk<CompFx>);
    add ("Limiter",     "Dynamics", 1.f, { F ("Ceiling dB", -24, 0, -1), L ("Release ms", 10, 1000, 100) },     &mk<LimiterFx>);
    add ("Gate",        "Dynamics", 1.f, { F ("Threshold dB", -80, 0, -45), F ("Range dB", -80, 0, -60), L ("Attack ms", 0.1f, 50, 1), L ("Release ms", 5, 1000, 100) }, &mk<GateFx>);
    add ("Expander",    "Dynamics", 1.f, { F ("Threshold dB", -80, 0, -40), F ("Ratio", 1, 8, 2), L ("Release ms", 10, 1000, 150) }, &mk<ExpanderFx>);
    add ("De-Esser",    "Dynamics", 1.f, { L ("Frequency", 3000, 12000, 6500), F ("Threshold dB", -50, 0, -25), F ("Amount", 0, 1, 0.7f) }, &mk<DeEsserFx>);
    add ("Transient Shaper", "Dynamics", 1.f, { F ("Attack", -1, 1, 0.5f), F ("Sustain", -1, 1, 0) },            &mk<TransientFx>);
    add ("Upward Comp", "Dynamics", 1.f, { F ("Threshold dB", -60, 0, -35), F ("Amount", 0, 1, 0.5f), F ("Max Boost dB", 0, 24, 12) }, &mk<UpCompFx>);
    add ("Pump",        "Dynamics", 1.f, { L ("Rate Hz", 0.5f, 16, 2), F ("Depth", 0, 1, 0.7f), F ("Curve", 0.5f, 4, 2) }, &mk<PumpFx>);
    add ("Leveler",     "Dynamics", 1.f, { F ("Target dB", -40, -3, -16), L ("Speed ms", 100, 3000, 600), F ("Range dB", 0, 24, 12) }, &mk<LevelerFx>);
    add ("Tremolo",     "Dynamics", 1.f, { L ("Rate Hz", 0.1f, 20, 5), F ("Depth", 0, 1, 0.6f), F ("Shape", 0, 1, 0), F ("Stereo", 0, 1, 0) }, &mk<TremoloFx>);
    add ("Squash",      "Dynamics", 1.f, { F ("Amount", 0, 1, 0.5f) },                                          &mk<SquashFx>);
    add ("Peak Clipper","Dynamics", 1.f, { F ("Ceiling dB", -24, 0, -3), F ("Softness", 0, 1, 0.3f) },          &mk<PerCh<PeakClip>>);

    // ---- Distorsion (14)
    add ("Saturator",   "Distortion", 1.f, { F ("Drive dB", 0, 36, 12), L ("Tone Hz", 500, 20000, 12000), F ("Shape", 0, 1, 0) }, &mk<PerCh<Saturator>>);
    add ("Tube",        "Distortion", 1.f, { F ("Drive dB", 0, 36, 10), F ("Bias", -0.5f, 0.5f, 0.15f), L ("Tone Hz", 500, 20000, 10000) }, &mk<PerCh<TubeSat>>);
    add ("Tape",        "Distortion", 1.f, { F ("Drive dB", 0, 24, 8), L ("Tone Hz", 1000, 20000, 9000), F ("Bump dB", 0, 6, 2) }, &mk<PerCh<TapeSat>>);
    add ("Hard Clip",   "Distortion", 1.f, { F ("Threshold dB", -30, 0, -6), F ("Gain dB", 0, 24, 6) },         &mk<PerCh<HardClipper>>);
    add ("Soft Clip",   "Distortion", 1.f, { F ("Drive dB", 0, 30, 9) },                                        &mk<PerCh<SoftClipper>>);
    add ("Foldback",    "Distortion", 1.f, { F ("Threshold dB", -30, -1, -12), F ("Gain dB", 0, 24, 6) },       &mk<PerCh<Foldback>>);
    add ("Bitcrusher",  "Distortion", 1.f, { F ("Bits", 1, 16, 8), F ("Downsample", 1, 50, 1) },                &mk<PerCh<BitCrusher>>);
    add ("Decimator",   "Distortion", 1.f, { L ("Rate Hz", 200, 48000, 8000), F ("Jitter", 0, 1, 0), F ("Smooth", 0, 1, 0) }, &mk<PerCh<Decimator>>);
    add ("Rectifier",   "Distortion", 1.f, { F ("Half > Full", 0, 1, 1), F ("Amount", 0, 1, 0.5f) },            &mk<PerCh<Rectifier>>);
    add ("Fuzz",        "Distortion", 1.f, { F ("Fuzz dB", 0, 40, 25), F ("Starve", 0, 1, 0.2f), L ("Tone Hz", 500, 12000, 4000) }, &mk<PerCh<FuzzFace>>);
    add ("Overdrive",   "Distortion", 1.f, { F ("Drive dB", 0, 40, 15), L ("Tone Hz", 500, 12000, 3500), F ("Level dB", -12, 12, 0) }, &mk<PerCh<Overdrive>>);
    add ("Sine Shaper", "Distortion", 1.f, { F ("Drive", 0, 1, 0.4f), F ("Bias", -1, 1, 0) },                   &mk<PerCh<SineShaper>>);
    add ("Asym Distortion", "Distortion", 1.f, { F ("Drive dB", 0, 36, 14), F ("Asymmetry", -1, 1, 0.5f) },     &mk<PerCh<AsymDist>>);
    add ("Exciter",     "Distortion", 1.f, { L ("Frequency", 1000, 10000, 3000), F ("Amount", 0, 1, 0.4f), F ("Odd > Even", 0, 1, 0.3f) }, &mk<PerCh<Exciter>>);

    // ---- Modulacion (14)
    add ("Chorus",      "Modulation", 0.5f, { L ("Rate Hz", 0.05f, 5, 0.8f), F ("Depth ms", 0, 10, 3), F ("Delay ms", 5, 30, 15), F ("Width", 0, 1, 0.8f) }, &mk<ChorusFx>);
    add ("Flanger",     "Modulation", 0.5f, { L ("Rate Hz", 0.05f, 5, 0.3f), F ("Depth", 0, 1, 0.7f), F ("Feedback", -0.95f, 0.95f, 0.5f), F ("Manual ms", 0.1f, 10, 2) }, &mk<FlangerFx>);
    add ("Phaser",      "Modulation", 0.5f, { L ("Rate Hz", 0.05f, 5, 0.4f), F ("Depth", 0, 1, 0.7f), F ("Feedback", 0, 0.9f, 0.5f), L ("Center Hz", 200, 3000, 800) }, &mk<PhaserFx>);
    add ("Vibrato",     "Modulation", 1.f,  { L ("Rate Hz", 0.1f, 15, 5), F ("Depth ms", 0, 8, 2) },            &mk<PerCh<VibratoS>>);
    add ("Auto Pan",    "Modulation", 1.f,  { L ("Rate Hz", 0.05f, 20, 1), F ("Depth", 0, 1, 1), F ("Shape", 0, 1, 0) }, &mk<AutoPanFx>);
    add ("Ring Mod",    "Modulation", 0.5f, { L ("Frequency", 20, 5000, 440), F ("Square", 0, 1, 0) },          &mk<PerCh<RingMod>>);
    add ("Rotary",      "Modulation", 1.f,  { F ("Speed Hz", 0.3f, 10, 5), F ("Depth", 0, 1, 0.7f), L ("Crossover Hz", 400, 1500, 800) }, &mk<RotaryFx>);
    add ("Ensemble",    "Modulation", 0.7f, { L ("Rate Hz", 0.1f, 3, 0.6f), F ("Depth", 0, 1, 0.6f) },          &mk<EnsembleFx>);
    add ("Freq Shifter","Modulation", 0.5f, { F ("Shift Hz", -1000, 1000, 50) },                                &mk<FreqShifter>);
    add ("Wobble",      "Modulation", 1.f,  { L ("Rate Hz", 0.1f, 12, 2), F ("Depth", 0, 1, 0.8f), F ("Resonance", 0.5f, 12, 4), L ("Base Hz", 100, 2000, 300) }, &mk<PerCh<WobbleS>>);
    add ("Harmonic Tremolo", "Modulation", 1.f, { L ("Rate Hz", 0.1f, 15, 4), F ("Depth", 0, 1, 0.7f), L ("Crossover Hz", 200, 3000, 800) }, &mk<PerCh<HarmTrem>>);
    add ("Dimension",   "Modulation", 1.f,  { F ("Amount", 0, 1, 0.6f), L ("Rate Hz", 0.05f, 1.5f, 0.25f), F ("Depth ms", 0, 2, 0.5f) }, &mk<DimensionFx>);
    add ("Pitch Shifter","Modulation", 1.f, { F ("Semitones", -12, 12, 5), F ("Window ms", 20, 200, 60), F ("Cents", -100, 100, 0) }, &mk<PerCh<PitchShiftS>>);
    add ("Doubler",     "Modulation", 0.5f, { F ("Detune cents", 0, 50, 12), F ("Window ms", 20, 100, 50) },    &mk<DoublerFx>);

    // ---- Tiempo (12)
    add ("Delay",       "Time", 0.3f, { L ("Time ms", 1, 2000, 350), F ("Feedback", 0, 0.95f, 0.4f), L ("Tone Hz", 500, 20000, 6000) }, &mk<DelayFx>);
    add ("Ping Pong",   "Time", 0.3f, { L ("Time ms", 1, 2000, 300), F ("Feedback", 0, 0.95f, 0.45f), L ("Tone Hz", 500, 20000, 6000) }, &mk<PingPongFx>);
    add ("Slapback",    "Time", 0.3f, { F ("Time ms", 40, 200, 110), F ("Repeats", 0, 0.5f, 0.2f), L ("Tone Hz", 500, 20000, 5000) }, &mk<SlapbackFx>);
    add ("Tape Echo",   "Time", 0.3f, { L ("Time ms", 20, 2000, 400), F ("Feedback", 0, 0.95f, 0.5f), F ("Wow", 0, 1, 0.3f), F ("Saturation", 0, 1, 0.3f) }, &mk<TapeEchoFx>);
    add ("Multi Tap",   "Time", 0.4f, { F ("Spacing ms", 20, 500, 160), I ("Taps", 1, 8, 4), F ("Decay", 0.3f, 1, 0.75f), F ("Feedback", 0, 0.7f, 0.2f) }, &mk<MultiTapFx>);
    add ("Reverb",      "Time", 0.3f, { F ("Size", 0, 1, 0.7f), F ("Damping", 0, 1, 0.4f), F ("Width", 0, 1, 1), F ("Pre-delay ms", 0, 100, 10) }, &mk<ReverbFx<100>>);
    add ("Room",        "Time", 0.3f, { F ("Size", 0, 1, 0.6f), F ("Damping", 0, 1, 0.5f), F ("Width", 0, 1, 1), F ("Pre-delay ms", 0, 100, 0) }, &mk<ReverbFx<35>>);
    add ("Hall",        "Time", 0.3f, { F ("Size", 0, 1, 0.7f), F ("Damping", 0, 1, 0.35f), F ("Width", 0, 1, 1), F ("Pre-delay ms", 0, 100, 25) }, &mk<ReverbFx<170>>);
    add ("Plate",       "Time", 0.3f, { F ("Decay", 0, 1, 0.75f), F ("Damping", 0, 1, 0.2f), F ("Diffusion", 0, 1, 0.8f), F ("Pre-delay ms", 0, 100, 0) }, &mk<PlateFx>);
    add ("Spring",      "Time", 0.4f, { F ("Tension ms", 15, 80, 35), F ("Decay", 0, 0.95f, 0.75f), F ("Drip", 0, 0.85f, 0.6f) }, &mk<SpringFx>);
    add ("Stutter",     "Time", 1.f,  { L ("Interval ms", 100, 2000, 500), L ("Length ms", 5, 250, 60), F ("Hold", 0, 1, 0.5f) }, &mk<StutterFx>);
    add ("Reverse",     "Time", 0.7f, { F ("Length ms", 50, 500, 300), F ("Speed", 0.5f, 2, 1) },               &mk<PerCh<ReverseS>>);

    // ---- Estereo / utilidad (12)
    add ("Stereo Width","Stereo", 1.f, { F ("Width", 0, 2, 1.3f) },                                              &mk<WidthFx>);
    add ("Mid Side",    "Stereo", 1.f, { F ("Mid dB", -24, 12, 0), F ("Side dB", -24, 12, 0) },                  &mk<MidSideFx>);
    add ("Mono Maker",  "Stereo", 1.f, { F ("Amount", 0, 1, 1) },                                                &mk<MonoFx>);
    add ("Pan",         "Stereo", 1.f, { F ("Pan", -1, 1, 0) },                                                  &mk<PanFx>);
    add ("Gain",        "Stereo", 1.f, { F ("Gain dB", -48, 24, 0), B ("Soft Clip", false) },                    &mk<GainFx>);
    add ("Polarity",    "Stereo", 1.f, { B ("Invert L", true), B ("Invert R", true) },                           &mk<PolarityFx>);
    add ("Channel Swap","Stereo", 1.f, { F ("Swap", 0, 1, 1) },                                                  &mk<SwapFx>);
    add ("Haas",        "Stereo", 1.f, { F ("Delay ms", 0, 40, 15), B ("Delay Left", false), F ("Level dB", -12, 0, 0) }, &mk<HaasFx>);
    add ("Crossfeed",   "Stereo", 1.f, { F ("Amount", 0, 1, 0.5f), L ("Frequency", 300, 1500, 700) },            &mk<CrossfeedFx>);
    add ("Stereo Rotate","Stereo", 1.f, { F ("Angle deg", -180, 180, 30) },                                      &mk<RotateFx>);
    add ("Balance",     "Stereo", 1.f, { F ("Balance", -1, 1, 0) },                                              &mk<BalanceFx>);
    add ("Decorrelator","Stereo", 1.f, { F ("Amount", 0, 1, 0.6f) },                                             &mk<DecorrFx>);

    // ---- Creativos (14)
    add ("Tape Stop",   "Creative", 1.f, { B ("Stop", false), F ("Time s", 0.1f, 4, 1) },                        &mk<TapeStopFx>);
    add ("Lo-Fi",       "Creative", 1.f, { F ("Bits", 4, 16, 10), L ("Rate Hz", 2000, 48000, 12000), F ("Noise", 0, 1, 0.1f), F ("Wobble", 0, 1, 0.3f) }, &mk<PerCh<LofiS>>);
    add ("Vinyl",       "Creative", 1.f, { F ("Crackle", 0, 1, 0.4f), F ("Noise", 0, 1, 0.1f), L ("Wear Hz", 1000, 16000, 9000), F ("Wow", 0, 1, 0.2f) }, &mk<PerCh<VinylS>>);
    add ("Radio",       "Creative", 1.f, { L ("Low Cut Hz", 100, 1000, 300), L ("High Cut Hz", 2000, 8000, 3500), F ("Drive dB", 0, 30, 10), F ("Static", 0, 1, 0.05f) }, &mk<PerCh<RadioS>>);
    add ("Telephone",   "Creative", 1.f, { L ("Low Hz", 200, 600, 400), L ("High Hz", 2000, 4500, 3200), F ("Grit", 0, 1, 0.3f) }, &mk<PerCh<PhoneS>>);
    add ("Megaphone",   "Creative", 1.f, { F ("Drive dB", 0, 40, 20), L ("Focus Hz", 500, 3000, 1400), F ("Horn dB", 0, 12, 6) }, &mk<PerCh<MegaphoneS>>);
    add ("Robot",       "Creative", 1.f, { L ("Pitch Hz", 30, 400, 100), F ("Resonance", 0, 0.95f, 0.7f), F ("Ring", 0, 1, 0.5f) }, &mk<PerCh<RobotS>>);
    add ("Underwater",  "Creative", 1.f, { F ("Depth", 0, 1, 0.6f), L ("Rate Hz", 0.1f, 3, 0.5f), L ("Darkness Hz", 200, 4000, 900) }, &mk<PerCh<UnderwaterS>>);
    add ("Gater",       "Creative", 1.f, { L ("Steps/s", 1, 32, 8), I ("Pattern", 0, 7, 0), L ("Smooth ms", 0.5f, 50, 5), F ("Depth", 0, 1, 1) }, &mk<GaterFx>);
    add ("Sample Hold Filter","Creative", 1.f, { L ("Rate Hz", 0.5f, 20, 6), F ("Resonance", 0.5f, 12, 5), F ("Range oct", 0, 6, 4) }, &mk<PerCh<SampleHoldFilt>>);
    add ("Glitch",      "Creative", 1.f, { F ("Chance", 0, 1, 0.4f), L ("Size ms", 20, 500, 120) },              &mk<GlitchFx>);
    add ("Shimmer",     "Creative", 0.4f, { F ("Decay", 0, 1, 0.8f), F ("Shimmer", 0, 1, 0.5f), F ("Damping", 0, 1, 0.4f) }, &mk<ShimmerFx>);
    add ("Resonator",   "Creative", 0.5f, { I ("Root Note", 24, 84, 48), I ("Chord", 0, 3, 0), F ("Decay", 0.5f, 0.99f, 0.93f), L ("Tone Hz", 1000, 16000, 6000) }, &mk<PerCh<ResonatorS>>);
    add ("Freeze",      "Creative", 1.f, { B ("Freeze", false), L ("Size ms", 30, 500, 150) },                   &mk<FreezeFx>);
    // ---- Afinacion (1)
    add ("Pitch Corrector", "Creative", 1.f, { I ("Key (C=0)", 0, 11, 0), I ("Scale 0 Chrom 1 Major 2 Minor", 0, 2, 1), L ("Retune ms", 1, 400, 40), F ("Amount", 0, 1, 1) }, &mk<PitchCorrectFx>);
    return r;
}

inline const std::vector<Info>& registry() { static const std::vector<Info> r = buildRegistry(); return r; }
}
