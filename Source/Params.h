#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <atomic>

// ---------------------------------------------------------------------------
// Parameter IDs — the 12-module chain from the original web mockup
// (Preamp, Gate, De-esser, Glue Comp, Opto, EQ 550, Resonance, Saturator,
// Doubler, Reverb, Delay, Limiter), same names/ranges/defaults as the
// mockup's PARAM_META table and its "Flat" / "Warm Lead Vocal" presets,
// plus three real-time modules added afterward: Auto-Tune (Tune*),
// Transient Shaper (Trs*) and Exciter (Exc*) — 15 modules total.
// ---------------------------------------------------------------------------
namespace XID
{
    static const juce::String MasterInGain  = "MasterInGain";
    static const juce::String MasterOutGain = "MasterOutGain";
    static const juce::String MasterWidth   = "MasterWidth";
    static const juce::String MasterBypass  = "MasterBypass";

    // Per-module bypass — real DSP-level dry passthrough for that one stage
    // only (everything else in the chain keeps processing normally), so any
    // module can be A/B'd in isolation.
    static const juce::String PreBypass  = "PreBypass";
    static const juce::String GateBypass = "GateBypass";
    static const juce::String TuneBypass = "TuneBypass";
    static const juce::String EssBypass  = "EssBypass";
    static const juce::String TrsBypass  = "TrsBypass";
    static const juce::String CompBypass = "CompBypass";
    static const juce::String OptoBypass = "OptoBypass";
    static const juce::String EqBypass   = "EqBypass";
    static const juce::String ResBypass  = "ResBypass";
    static const juce::String SatBypass  = "SatBypass";
    static const juce::String ExcBypass  = "ExcBypass";
    static const juce::String DblBypass  = "DblBypass";
    static const juce::String RevBypass  = "RevBypass";
    static const juce::String DlyBypass  = "DlyBypass";
    static const juce::String LimBypass  = "LimBypass";

    // "Listen" / key-cue modes — audition exactly what a detector is
    // reacting to instead of the plugin's normal output.
    static const juce::String GateListen = "GateListen";   // hears only what the gate is removing
    static const juce::String EssListen  = "EssListen";    // hears only the detected sibilance band

    // Gate's optional external sidechain key (needs the host to actually
    // route audio into the plugin's second input bus — off by default so a
    // fresh instance behaves exactly as before).
    static const juce::String GateScEnable = "GateScEnable";
    static const juce::String GateLookahead = "GateLookahead"; // real fixed-5ms lookahead delay + latency compensation, see runGate

    static const juce::String PreGain  = "PreGain";
    static const juce::String PreChar  = "PreChar";
    static const juce::String PreHPF   = "PreHPF";
    static const juce::String PrePad       = "PrePad";
    static const juce::String PrePhase     = "PrePhase";
    static const juce::String PrePhantom   = "PrePhantom";
    static const juce::String PreImpedance = "PreImpedance";

    static const juce::String GateThresh  = "GateThresh";
    static const juce::String GateRange   = "GateRange";
    static const juce::String GateAttack  = "GateAttack";
    static const juce::String GateHold    = "GateHold";
    static const juce::String GateRelease = "GateRelease";

    static const juce::String EssThresh = "EssThresh";
    static const juce::String EssRange  = "EssRange";
    static const juce::String EssFreq   = "EssFreq";
    static const juce::String EssBand   = "EssBand"; // 0=S 1=T 2=CH — real detect-Q + freq-bias character, see runEss

    // Transient Shaper — linked dual envelope-follower attack/sustain
    // reshaping, see XaLZaProcessor::runTrs. -100..100, 0 = transparent
    // (both default to 0 so a fresh instance / old session is unaffected).
    static const juce::String TrsAttack  = "TrsAttack";
    static const juce::String TrsSustain = "TrsSustain";

    static const juce::String CompThresh  = "CompThresh";
    static const juce::String CompMakeup  = "CompMakeup";
    static const juce::String CompAttack  = "CompAttack";
    static const juce::String CompRelease = "CompRelease";
    static const juce::String CompMix     = "CompMix";
    static const juce::String CompRatio   = "CompRatio";

    static const juce::String OptoReduction = "OptoReduction";
    static const juce::String OptoGain      = "OptoGain";
    static const juce::String OptoMix       = "OptoMix";
    static const juce::String OptoMode      = "OptoMode";

    static const juce::String EqLow   = "EqLow";
    static const juce::String EqMid   = "EqMid";
    static const juce::String EqHigh  = "EqHigh";
    static const juce::String EqLowFreq  = "EqLowFreq";
    static const juce::String EqMidFreq  = "EqMidFreq";
    static const juce::String EqHighFreq = "EqHighFreq";

    static const juce::String ResAmount     = "ResAmount";
    static const juce::String ResSharpness  = "ResSharpness";
    static const juce::String ResReactivity = "ResReactivity";
    static const juce::String ResNotchLimit = "ResNotchLimit";
    static const juce::String ResLow        = "ResLow";
    static const juce::String ResHigh       = "ResHigh";
    static const juce::String ResStyle      = "ResStyle";  // 0=Delicate 1=Vocal 2=Wide — real Q/detect-width scaling, see runRes
    static const juce::String ResBands      = "ResBands";  // 1-5 — real number of parallel adaptive notches, see runRes

    static const juce::String SatDrive   = "SatDrive";
    static const juce::String SatTone    = "SatTone";
    static const juce::String SatCeiling = "SatCeiling";
    static const juce::String SatMix     = "SatMix";
    static const juce::String SatChar    = "SatChar";   // 0=Tube 1=Tape 2=Transistor 3=Diode — real distinct waveshapes, see runSat

    // Exciter — harmonic enhancer. Isolates the band above Tone's crossover
    // and drives ONLY that band through an asymmetric soft clip, mixed back
    // on top of the dry signal — see XaLZaProcessor::runExc.
    static const juce::String ExcDrive = "ExcDrive";
    static const juce::String ExcTone  = "ExcTone";   // crossover freq, 0=1.5kHz .. 100=6kHz
    static const juce::String ExcMix   = "ExcMix";

    static const juce::String DblDetune = "DblDetune";
    static const juce::String DblWidth  = "DblWidth";
    static const juce::String DblDelay  = "DblDelay";
    static const juce::String DblMix    = "DblMix";
    static const juce::String DblVoices = "DblVoices"; // 2/4/6/8 — real voice count, see runDbl

    static const juce::String RevSize          = "RevSize";
    static const juce::String RevDecay         = "RevDecay";
    static const juce::String RevPreDelay      = "RevPreDelay";
    static const juce::String RevMix           = "RevMix";
    static const juce::String RevDuck          = "RevDuck";
    static const juce::String RevDuckRelease   = "RevDuckRelease";
    // Tone-shaping on the WET tail only (post juce::dsp::Reverb) — lets you
    // clean up boom or tame harshness without touching the internal
    // room-size/damping model.
    static const juce::String RevWetHpf        = "RevWetHpf";
    static const juce::String RevWetLpf        = "RevWetLpf";
    // Independent damping trim (offset on top of the Decay-derived value)
    // and the Algorithmic<->Convolution hybrid blend — see runRev.
    static const juce::String RevDamping       = "RevDamping";
    static const juce::String RevHybrid        = "RevHybrid";
    // Real M/S stereo width on the WET tail only (0=mono, 100=normal,
    // 200=exaggerated wide) and a real Freeze (infinite-sustain) toggle —
    // see runRev.
    static const juce::String RevWidth         = "RevWidth";
    static const juce::String RevFreeze        = "RevFreeze";

    // Real-time Auto-Tune — a genuine pitch detector (autocorrelation) +
    // pitch shifter (granular, see XaLZaProcessor::GranularPitchShifter),
    // not a cosmetic knob. Key/Scale pick the target note set; Retune is
    // how fast the detected pitch snaps to it (0 = instant/robotic "hard
    // tune", higher = natural glide); Amount blends the correction ratio
    // between 1.0 (no shift) and the full correction needed — see runTune.
    static const juce::String TuneKey    = "TuneKey";      // 0..11 = C..B
    static const juce::String TuneScale  = "TuneScale";    // 0=Major 1=Minor 2=Chromatic
    static const juce::String TuneRetune = "TuneRetune";   // ms
    static const juce::String TuneAmount = "TuneAmount";   // %
    // Optional LPC-based formant-preserving pitch shift (see
    // XaLZaProcessor::FormantEnvelope / processFormantPreservedSample) —
    // off by default. The plain shift's "chipmunk"/"Vader" character on
    // large corrections is an established, sought-after part of the hard-
    // tune urban-vocal sound (see the GranularPitchShifter doc comment),
    // so this stays an explicit opt-in rather than replacing it outright.
    static const juce::String TuneFormant = "TuneFormant";

    static const juce::String DlyTime        = "DlyTime";
    static const juce::String DlyFeedback    = "DlyFeedback";
    static const juce::String DlySpread      = "DlySpread";
    static const juce::String DlyMix         = "DlyMix";
    static const juce::String DlyDuck        = "DlyDuck";
    static const juce::String DlyDuckRelease = "DlyDuckRelease";
    static const juce::String DlyPanRate     = "DlyPanRate";
    // Filtering in the FEEDBACK path only (not the dry-through) — since it's
    // recirculated, this compounds a little more on each repeat, giving the
    // classic analog/tape-echo "repeats get darker and thinner" character.
    static const juce::String DlyFbHpf       = "DlyFbHpf";
    static const juce::String DlyFbLpf       = "DlyFbLpf";
    // Real tape-echo-style saturation in the feedback path (soft tanh drive
    // with makeup gain) — at higher settings also brings in a small amount
    // of delay-time wow, exactly like a real tape unit's drive and wobble
    // being physically linked — see runDly.
    static const juce::String DlyDrive       = "DlyDrive";
    static const juce::String DlySync        = "DlySync";     // real host-tempo sync toggle, see runDly
    static const juce::String DlyNoteDiv     = "DlyNoteDiv";  // 0..6 index into DlyNoteTable (Params.h) — used when DlySync is on
    static const juce::String DlyPreDelay    = "DlyPreDelay"; // 0=Off 1=1/32 2=1/16 — real tempo-synced pre-delay tap, see runDly

    static const juce::String LimInputGain = "LimInputGain";
    static const juce::String LimCeiling   = "LimCeiling";
    static const juce::String LimRelease   = "LimRelease";
    static const juce::String LimClip      = "LimClip";
}

// ---------------------------------------------------------------------------
// Doubler per-voice layout — shared between PluginProcessor (real DSP: each
// voice is its own modulated delay tap) and PluginEditor (the Per-Voice
// table reads these exact same constants so the numbers it shows are the
// numbers actually driving the sound, not invented display data).
// ---------------------------------------------------------------------------
namespace DblVoiceConfig
{
    constexpr int kMaxVoices = 8;
    // Chorus-rate per voice (Hz) — deliberately non-harmonic spread so voices
    // don't beat together audibly.
    constexpr float rateHz[kMaxVoices]        = { 0.53f, 0.61f, 0.67f, 0.71f, 0.79f, 0.83f, 0.89f, 0.97f };
    // Extra delay stagger per voice on top of the user's Delay knob (ms).
    constexpr float delayOffsetMs[kMaxVoices] = { 0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 28.0f };
    // Pan position per voice at full Width (-1 = hard L, +1 = hard R); index 0
    // is always centre-most so low voice counts stay narrow-but-present.
    constexpr float panPos[kMaxVoices]        = { -1.0f, 1.0f, -0.6f, 0.6f, -0.3f, 0.3f, -0.85f, 0.85f };
}

// ---------------------------------------------------------------------------
// Delay's real tempo-sync note table — fraction of a whole note for each
// DlyNoteDiv index, shared between the DSP (runDly, computes real ms from
// host BPM) and the editor (dlyNoteDivSeg's button order matches exactly).
// ---------------------------------------------------------------------------
namespace DlyNoteTable
{
    constexpr int kNumDivs = 7;
    // 1/16, 1/8T, 1/8, 1/4T, 1/4, 1/2, 1/1 — as a fraction of a whole note.
    constexpr float wholeNoteFraction[kNumDivs] = { 1.0f / 16.0f, 1.0f / 12.0f, 1.0f / 8.0f,
                                                      1.0f / 6.0f,  1.0f / 4.0f, 1.0f / 2.0f, 1.0f };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createXaLZaParameterLayout()
{
    using Param = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    int uid = 1;
    auto add = [&](const juce::String& id, const juce::String& name, float lo, float hi,
                   float init, float step = 0.0f)
    {
        p.push_back(std::make_unique<Param>(juce::ParameterID{id, uid++}, name,
                                             Range(lo, hi, step), init));
    };

    add(XID::MasterInGain, "Master In Gain", -24.0f, 24.0f, 0.0f);
    add(XID::MasterOutGain, "Master Out Gain", -24.0f, 24.0f, 0.0f);
    add(XID::MasterWidth, "Stereo Width", 0.0f, 200.0f, 100.0f);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ XID::MasterBypass, uid++ }, "Bypass", false));

    auto addBypass = [&](const juce::String& id, const juce::String& name)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, uid++ }, name, false));
    };
    addBypass(XID::PreBypass,  "Pre Bypass");
    addBypass(XID::GateBypass, "Gate Bypass");
    addBypass(XID::TuneBypass, "Auto-Tune Bypass");
    addBypass(XID::EssBypass,  "De-esser Bypass");
    addBypass(XID::TrsBypass,  "Transient Shaper Bypass");
    addBypass(XID::CompBypass, "Comp Bypass");
    addBypass(XID::OptoBypass, "Opto Bypass");
    addBypass(XID::EqBypass,   "EQ Bypass");
    addBypass(XID::ResBypass,  "Resonance Bypass");
    addBypass(XID::SatBypass,  "Saturator Bypass");
    addBypass(XID::ExcBypass,  "Exciter Bypass");
    addBypass(XID::DblBypass,  "Doubler Bypass");
    addBypass(XID::RevBypass,  "Reverb Bypass");
    addBypass(XID::DlyBypass,  "Delay Bypass");
    addBypass(XID::LimBypass,  "Limiter Bypass");
    addBypass(XID::GateListen, "Gate Listen");
    addBypass(XID::EssListen,  "De-esser Listen");
    addBypass(XID::GateScEnable, "Gate External Sidechain");
    addBypass(XID::GateLookahead, "Gate Lookahead");
    addBypass(XID::DlySync, "Delay Tempo Sync");
    addBypass(XID::RevFreeze, "Reverb Freeze");
    addBypass(XID::TuneFormant, "Tune Formant Preserve");
    // Mockup toggles/mode switches that snap or gate real DSP:
    addBypass(XID::PrePad, "Pre Pad -20dB");           // real -20dB input pad
    addBypass(XID::PrePhase, "Pre Phase Invert");      // real polarity flip
    addBypass(XID::PrePhantom, "Pre Phantom Power");   // cosmetic only - real phantom power has no audio effect on a plugin
    addBypass(XID::OptoMode, "Opto Mode");             // false=Compress (4:1), true=Limit (20:1)

    add(XID::PreGain, "Pre Gain", 0.0f, 70.0f, 0.0f);
    add(XID::PreChar, "Pre Character", 0.0f, 100.0f, 0.0f);
    add(XID::PreHPF, "Pre HPF", 20.0f, 400.0f, 20.0f);
    // Real seg-group (see PreImpShelf in the processor): tilts a subtle
    // high-shelf, matching how a dynamic mic's top end actually shifts a
    // little with different preamp input-impedance loading.
    add(XID::PreImpedance, "Pre Impedance", 300.0f, 2400.0f, 1200.0f);

    add(XID::GateThresh, "Gate Threshold", -70.0f, -10.0f, -50.0f);
    add(XID::GateRange, "Gate Range", -80.0f, 0.0f, -60.0f);
    add(XID::GateAttack, "Gate Attack", 0.1f, 50.0f, 2.0f);
    add(XID::GateHold, "Gate Hold", 0.0f, 500.0f, 45.0f);
    add(XID::GateRelease, "Gate Release", 5.0f, 1000.0f, 120.0f);

    add(XID::TuneKey, "Tune Key", 0.0f, 11.0f, 0.0f);
    add(XID::TuneScale, "Tune Scale", 0.0f, 2.0f, 0.0f);
    add(XID::TuneRetune, "Tune Retune Speed", 0.0f, 400.0f, 80.0f);
    // Defaults to 0 = fully transparent (ratio stays 1.0, see runTune) —
    // a fresh instance and every old preset/session that never mentions
    // this param sound bit-identical to before Auto-Tune existed.
    add(XID::TuneAmount, "Tune Amount", 0.0f, 100.0f, 0.0f);

    add(XID::EssThresh, "De-esser Threshold", -40.0f, 0.0f, -20.0f);
    add(XID::EssRange, "De-esser Range", -24.0f, 0.0f, -8.0f);
    add(XID::EssFreq, "De-esser Freq", 2000.0f, 12000.0f, 6300.0f);
    add(XID::EssBand, "De-esser Band", 0.0f, 2.0f, 1.0f);

    // Both default to 0 = unity gain everywhere (see runTrs) — a fresh
    // instance and every old session are bit-identical to before this
    // module existed.
    add(XID::TrsAttack, "Trs Attack", -100.0f, 100.0f, 0.0f);
    add(XID::TrsSustain, "Trs Sustain", -100.0f, 100.0f, 0.0f);

    add(XID::CompThresh, "Comp Threshold", -40.0f, 0.0f, -20.0f);
    add(XID::CompMakeup, "Comp Makeup", -6.0f, 12.0f, 0.0f);
    add(XID::CompAttack, "Comp Attack", 0.1f, 80.0f, 12.0f);
    add(XID::CompRelease, "Comp Release", 20.0f, 1000.0f, 250.0f);
    add(XID::CompMix, "Comp Mix", 0.0f, 100.0f, 0.0f);
    add(XID::CompRatio, "Comp Ratio", 1.0f, 50.0f, 4.0f);

    add(XID::OptoReduction, "Opto Reduction", 0.0f, 100.0f, 0.0f);
    add(XID::OptoGain, "Opto Gain", -6.0f, 18.0f, 0.0f);
    add(XID::OptoMix, "Opto Mix", 0.0f, 100.0f, 0.0f);

    add(XID::EqLow, "EQ Low", -12.0f, 12.0f, 0.0f);
    add(XID::EqMid, "EQ Mid", -12.0f, 12.0f, 0.0f);
    add(XID::EqHigh, "EQ High", -12.0f, 12.0f, 0.0f);
    // Selectable band centre/corner frequencies — matches the "550"-style
    // idea of a per-band frequency choice rather than each band being
    // welded to one fixed frequency.
    // Widened to 30Hz so the low-band seg-group can offer the same 30Hz
    // first option the original web mockup's eqLowFreqSegs does.
    add(XID::EqLowFreq, "EQ Low Freq", 30.0f, 500.0f, 150.0f);
    add(XID::EqMidFreq, "EQ Mid Freq", 200.0f, 8000.0f, 1000.0f);
    add(XID::EqHighFreq, "EQ High Freq", 2000.0f, 18000.0f, 6000.0f);

    add(XID::ResAmount, "Res Amount", 0.0f, 100.0f, 0.0f);
    add(XID::ResSharpness, "Res Sharpness", 0.0f, 100.0f, 50.0f);
    add(XID::ResReactivity, "Res Reactivity", 0.0f, 100.0f, 50.0f);
    add(XID::ResNotchLimit, "Res Notch Limit", -24.0f, 0.0f, -12.0f);
    add(XID::ResLow, "Res Low", 20.0f, 400.0f, 120.0f);
    add(XID::ResHigh, "Res High", 2000.0f, 16000.0f, 9400.0f);
    add(XID::ResStyle, "Res Style", 0.0f, 2.0f, 1.0f);
    add(XID::ResBands, "Res Bands", 1.0f, 5.0f, 1.0f);

    add(XID::SatDrive, "Sat Drive", 0.0f, 100.0f, 0.0f);
    add(XID::SatTone, "Sat Tone", -12.0f, 12.0f, 0.0f);
    add(XID::SatCeiling, "Sat Ceiling", -6.0f, 0.0f, -0.3f);
    add(XID::SatMix, "Sat Mix", 0.0f, 100.0f, 0.0f);
    add(XID::SatChar, "Sat Character", 0.0f, 3.0f, 0.0f);

    // Both Drive and Mix default to 0 = bit-identical passthrough (see
    // runExc) until turned up.
    add(XID::ExcDrive, "Exciter Drive", 0.0f, 100.0f, 0.0f);
    add(XID::ExcTone, "Exciter Tone", 0.0f, 100.0f, 50.0f);
    add(XID::ExcMix, "Exciter Mix", 0.0f, 100.0f, 0.0f);

    add(XID::DblDetune, "Doubler Detune", 0.0f, 40.0f, 12.0f);
    add(XID::DblWidth, "Doubler Width", 0.0f, 100.0f, 88.0f);
    add(XID::DblDelay, "Doubler Delay", 0.0f, 40.0f, 14.0f);
    add(XID::DblMix, "Doubler Mix", 0.0f, 100.0f, 0.0f);
    add(XID::DblVoices, "Doubler Voices", 2.0f, 8.0f, 4.0f);

    add(XID::RevSize, "Rev Size", 0.0f, 100.0f, 55.0f);
    add(XID::RevDecay, "Rev Decay", 0.3f, 8.0f, 2.4f);
    add(XID::RevPreDelay, "Rev Pre-Delay", 0.0f, 100.0f, 18.0f);
    add(XID::RevMix, "Rev Mix", 0.0f, 100.0f, 0.0f);
    add(XID::RevDuck, "Rev Duck", 0.0f, 100.0f, 70.0f);
    add(XID::RevDuckRelease, "Rev Duck Release", 20.0f, 800.0f, 220.0f);
    add(XID::RevWetHpf, "Rev Wet HPF", 20.0f, 1000.0f, 150.0f);
    add(XID::RevWetLpf, "Rev Wet LPF", 1000.0f, 18000.0f, 12000.0f);
    // Damping trim: a genuine independent offset on top of the existing
    // Decay-derived damping formula, centred at 50 = no change from the
    // old behaviour (so old sessions/defaults sound identical), giving
    // real extra control either side of that.
    add(XID::RevDamping, "Rev Damping Trim", 0.0f, 100.0f, 50.0f);
    // Hybrid blend: 0 = pure algorithmic (juce::dsp::Reverb) as before,
    // 100 = pure loaded-impulse convolution; in between genuinely mixes
    // both engines' wet signal. Defaults to 0 so nothing changes for
    // anyone until they load an IR and turn this up.
    add(XID::RevHybrid, "Rev Hybrid (IR Blend)", 0.0f, 100.0f, 0.0f);
    // Wet-tail stereo width, independent of the reverb engine's own internal
    // spread — a real M/S scale (0=mono collapse, 100=unchanged, up to
    // 200=exaggerated wide), exactly the same formula MasterWidth already
    // uses at the bus level, applied here to just the reverb's own wet
    // signal. Defaults to 100 = no change from prior behaviour.
    add(XID::RevWidth, "Rev Width", 0.0f, 200.0f, 100.0f);

    add(XID::DlyTime, "Dly Time", 20.0f, 1000.0f, 250.0f);
    add(XID::DlyFeedback, "Dly Feedback", 0.0f, 90.0f, 38.0f);
    add(XID::DlySpread, "Dly Spread", 0.0f, 100.0f, 64.0f);
    add(XID::DlyMix, "Dly Mix", 0.0f, 100.0f, 0.0f);
    add(XID::DlyDuck, "Dly Duck", 0.0f, 100.0f, 70.0f);
    add(XID::DlyDuckRelease, "Dly Duck Release", 20.0f, 800.0f, 220.0f);
    add(XID::DlyPanRate, "Dly Pan Rate", 0.05f, 4.0f, 0.5f);
    add(XID::DlyFbHpf, "Dly Feedback HPF", 20.0f, 2000.0f, 120.0f);
    add(XID::DlyFbLpf, "Dly Feedback LPF", 1000.0f, 18000.0f, 8000.0f);
    // Defaults to 0 = bit-identical to old behaviour until turned up.
    add(XID::DlyDrive, "Dly Drive", 0.0f, 100.0f, 0.0f);
    add(XID::DlyNoteDiv, "Dly Note Division", 0.0f, 6.0f, 4.0f);
    add(XID::DlyPreDelay, "Dly Pre-Delay", 0.0f, 2.0f, 0.0f);

    add(XID::LimInputGain, "Lim Input Gain", -12.0f, 12.0f, 0.0f);
    add(XID::LimCeiling, "Lim Ceiling", -6.0f, 0.0f, -0.3f);
    add(XID::LimRelease, "Lim Release", 10.0f, 500.0f, 80.0f);
    add(XID::LimClip, "Lim Clip", 0.0f, 100.0f, 0.0f);

    return {p.begin(), p.end()};
}

// ---------------------------------------------------------------------------
// Factory presets — each one sets a fixed list of real parameters directly
// to concrete values (no macro/intensity layer involved). The values below
// are the same musical starting points the old macro system used to reach
// via its neutral/full blend at each preset's percentages, just baked down
// to plain numbers now that macros are gone. Applying a preset is simply
// "set these params to these values"; anything not listed here is left
// exactly as the user has it.
// ---------------------------------------------------------------------------
struct XalzaPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> paramValues;   // {paramID, concrete value}
};

inline const std::vector<XalzaPreset>& xalzaFactoryPresets()
{
    static const std::vector<XalzaPreset> presets = {
        { "Init (Flat)", {
            { XID::PreGain, 0.0f },
            { XID::PreChar, 0.0f },
            { XID::PreHPF, 20.0f },
            { XID::GateThresh, -50.0f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 2.0f },
            { XID::GateHold, 45.0f },
            { XID::GateRelease, 120.0f },
            { XID::EssThresh, -20.0f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 6300.0f },
            { XID::CompThresh, -20.0f },
            { XID::CompMakeup, 0.0f },
            { XID::CompAttack, 12.0f },
            { XID::CompRelease, 250.0f },
            { XID::CompMix, 0.0f },
            { XID::OptoReduction, 0.0f },
            { XID::OptoGain, 0.0f },
            { XID::OptoMix, 0.0f },
            { XID::EqLow, 0.0f },
            { XID::EqMid, 0.0f },
            { XID::EqHigh, 0.0f },
            { XID::ResAmount, 0.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 0.0f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 0.0f },
            { XID::DblDetune, 12.0f },
            { XID::DblWidth, 88.0f },
            { XID::DblDelay, 14.0f },
            { XID::DblMix, 0.0f },
            { XID::RevSize, 55.0f },
            { XID::RevDecay, 2.4f },
            { XID::RevPreDelay, 18.0f },
            { XID::RevMix, 0.0f },
            { XID::RevDuck, 70.0f },
            { XID::RevDuckRelease, 220.0f },
            { XID::DlyFeedback, 38.0f },
            { XID::DlySpread, 64.0f },
            { XID::DlyMix, 0.0f },
            { XID::DlyDuck, 70.0f },
            { XID::DlyDuckRelease, 220.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 0.0f },
            { XID::LimCeiling, -0.3f },
            { XID::LimRelease, 80.0f },
            { XID::LimClip, 0.0f },
        } },
        { "Warm Lead Vocal", {
            { XID::PreGain, 42.0f },
            { XID::PreChar, 38.0f },
            { XID::PreHPF, 80.0f },
            { XID::GateThresh, -42.0f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 2.0f },
            { XID::GateHold, 45.0f },
            { XID::GateRelease, 120.0f },
            { XID::EssThresh, -16.0f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 6300.0f },
            { XID::CompThresh, -18.5f },
            { XID::CompMakeup, 3.2f },
            { XID::CompAttack, 12.0f },
            { XID::CompRelease, 250.0f },
            { XID::CompMix, 100.0f },
            { XID::OptoReduction, 42.0f },
            { XID::OptoGain, 5.0f },
            { XID::OptoMix, 100.0f },
            { XID::EqLow, 4.0f },
            { XID::EqMid, -2.0f },
            { XID::EqHigh, 6.0f },
            { XID::ResAmount, 75.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 64.0f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 80.0f },
            { XID::DblDetune, 12.0f },
            { XID::DblWidth, 88.0f },
            { XID::DblDelay, 14.0f },
            { XID::DblMix, 50.0f },
            { XID::RevSize, 55.0f },
            { XID::RevDecay, 2.4f },
            { XID::RevPreDelay, 18.0f },
            { XID::RevMix, 28.0f },
            { XID::RevDuck, 70.0f },
            { XID::RevDuckRelease, 220.0f },
            { XID::DlyFeedback, 38.0f },
            { XID::DlySpread, 64.0f },
            { XID::DlyMix, 22.0f },
            { XID::DlyDuck, 60.0f },
            { XID::DlyDuckRelease, 200.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 2.4f },
            { XID::LimCeiling, -1.0f },
            { XID::LimRelease, 80.0f },
            { XID::LimClip, 10.0f },
        } },
        { "Bright Pop Vocal", {
            { XID::PreGain, 23.1f },
            { XID::PreChar, 20.9f },
            { XID::PreHPF, 53.0f },
            { XID::GateThresh, -44.4f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 2.0f },
            { XID::GateHold, 45.0f },
            { XID::GateRelease, 120.0f },
            { XID::EssThresh, -16.8f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 6300.0f },
            { XID::CompThresh, -18.725f },
            { XID::CompMakeup, 2.72f },
            { XID::CompAttack, 12.0f },
            { XID::CompRelease, 250.0f },
            { XID::CompMix, 85.0f },
            { XID::OptoReduction, 16.8f },
            { XID::OptoGain, 2.0f },
            { XID::OptoMix, 40.0f },
            { XID::EqLow, 3.6f },
            { XID::EqMid, -1.8f },
            { XID::EqHigh, 5.4f },
            { XID::ResAmount, 45.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 22.4f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 28.0f },
            { XID::DblDetune, 12.0f },
            { XID::DblWidth, 88.0f },
            { XID::DblDelay, 14.0f },
            { XID::DblMix, 32.5f },
            { XID::RevSize, 55.0f },
            { XID::RevDecay, 2.4f },
            { XID::RevPreDelay, 18.0f },
            { XID::RevMix, 7.0f },
            { XID::RevDuck, 70.0f },
            { XID::RevDuckRelease, 220.0f },
            { XID::DlyFeedback, 38.0f },
            { XID::DlySpread, 64.0f },
            { XID::DlyMix, 3.3f },
            { XID::DlyDuck, 68.5f },
            { XID::DlyDuckRelease, 217.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 1.8f },
            { XID::LimCeiling, -0.825f },
            { XID::LimRelease, 80.0f },
            { XID::LimClip, 7.5f },
        } },
        { "Broadcast / Podcast", {
            { XID::PreGain, 16.8f },
            { XID::PreChar, 15.2f },
            { XID::PreHPF, 44.0f },
            { XID::GateThresh, -42.8f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 2.0f },
            { XID::GateHold, 45.0f },
            { XID::GateRelease, 120.0f },
            { XID::EssThresh, -17.6f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 6300.0f },
            { XID::CompThresh, -18.5f },
            { XID::CompMakeup, 3.2f },
            { XID::CompAttack, 12.0f },
            { XID::CompRelease, 250.0f },
            { XID::CompMix, 100.0f },
            { XID::OptoReduction, 29.4f },
            { XID::OptoGain, 3.5f },
            { XID::OptoMix, 70.0f },
            { XID::EqLow, 2.0f },
            { XID::EqMid, -1.0f },
            { XID::EqHigh, 3.0f },
            { XID::ResAmount, 52.5f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 9.6f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 12.0f },
            { XID::DblDetune, 12.0f },
            { XID::DblWidth, 88.0f },
            { XID::DblDelay, 14.0f },
            { XID::DblMix, 0.0f },
            { XID::RevSize, 55.0f },
            { XID::RevDecay, 2.4f },
            { XID::RevPreDelay, 18.0f },
            { XID::RevMix, 0.0f },
            { XID::RevDuck, 70.0f },
            { XID::RevDuckRelease, 220.0f },
            { XID::DlyFeedback, 38.0f },
            { XID::DlySpread, 64.0f },
            { XID::DlyMix, 0.0f },
            { XID::DlyDuck, 70.0f },
            { XID::DlyDuckRelease, 220.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 2.16f },
            { XID::LimCeiling, -0.93f },
            { XID::LimRelease, 80.0f },
            { XID::LimClip, 9.0f },
        } },
        { "Intimate ASMR / Podcast", {
            { XID::PreGain, 8.4f },
            { XID::PreChar, 7.6f },
            { XID::PreHPF, 32.0f },
            { XID::GateThresh, -47.6f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 2.0f },
            { XID::GateHold, 45.0f },
            { XID::GateRelease, 120.0f },
            { XID::EssThresh, -18.0f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 6300.0f },
            { XID::CompThresh, -19.325f },
            { XID::CompMakeup, 1.44f },
            { XID::CompAttack, 12.0f },
            { XID::CompRelease, 250.0f },
            { XID::CompMix, 45.0f },
            { XID::OptoReduction, 25.2f },
            { XID::OptoGain, 3.0f },
            { XID::OptoMix, 60.0f },
            { XID::EqLow, 1.4f },
            { XID::EqMid, -0.7f },
            { XID::EqHigh, 2.1f },
            { XID::ResAmount, 30.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 6.4f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 8.0f },
            { XID::DblDetune, 12.0f },
            { XID::DblWidth, 88.0f },
            { XID::DblDelay, 14.0f },
            { XID::DblMix, 0.0f },
            { XID::RevSize, 55.0f },
            { XID::RevDecay, 2.4f },
            { XID::RevPreDelay, 18.0f },
            { XID::RevMix, 2.8f },
            { XID::RevDuck, 70.0f },
            { XID::RevDuckRelease, 220.0f },
            { XID::DlyFeedback, 38.0f },
            { XID::DlySpread, 64.0f },
            { XID::DlyMix, 0.0f },
            { XID::DlyDuck, 70.0f },
            { XID::DlyDuckRelease, 220.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 1.2f },
            { XID::LimCeiling, -0.65f },
            { XID::LimRelease, 80.0f },
            { XID::LimClip, 5.0f },
        } },
        // ---- Genre presets: the first three factory presets to actually
        // use Auto-Tune, the Transient Shaper and the Exciter, each tuned
        // for a distinct urban-vocal-chain character rather than a generic
        // "turn everything up a bit" starting point.
        { "Reggaeton Lead", {
            { XID::PreGain, 38.0f },
            { XID::PreChar, 40.0f },
            { XID::PreHPF, 85.0f },
            { XID::GateThresh, -40.0f },
            { XID::GateRange, -60.0f },
            { XID::GateAttack, 1.5f },
            { XID::GateHold, 30.0f },
            { XID::GateRelease, 90.0f },
            { XID::TuneKey, 0.0f },
            { XID::TuneScale, 0.0f },
            // Fast-ish glide, moderate amount, formant preserved: an
            // audible correction character without the full chipmunk.
            { XID::TuneRetune, 45.0f },
            { XID::TuneAmount, 55.0f },
            { XID::TuneFormant, 1.0f },
            { XID::EssThresh, -16.0f },
            { XID::EssRange, -9.0f },
            { XID::EssFreq, 6800.0f },
            // Punchy transient, tighter tail — cuts through a busy dembow beat.
            { XID::TrsAttack, 25.0f },
            { XID::TrsSustain, -10.0f },
            { XID::CompThresh, -19.0f },
            { XID::CompMakeup, 3.5f },
            { XID::CompAttack, 8.0f },
            { XID::CompRelease, 180.0f },
            { XID::CompMix, 100.0f },
            { XID::OptoReduction, 35.0f },
            { XID::OptoGain, 4.0f },
            { XID::OptoMix, 80.0f },
            { XID::EqLow, 3.0f },
            { XID::EqMid, -1.5f },
            { XID::EqHigh, 5.5f },
            { XID::ResAmount, 55.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 45.0f },
            { XID::SatTone, 1.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 55.0f },
            { XID::ExcDrive, 35.0f },
            { XID::ExcTone, 35.0f },
            { XID::ExcMix, 30.0f },
            { XID::DblDetune, 10.0f },
            { XID::DblWidth, 80.0f },
            { XID::DblDelay, 12.0f },
            { XID::DblMix, 30.0f },
            { XID::RevSize, 45.0f },
            { XID::RevDecay, 1.8f },
            { XID::RevPreDelay, 15.0f },
            { XID::RevMix, 12.0f },
            { XID::RevDuck, 75.0f },
            { XID::RevDuckRelease, 200.0f },
            { XID::DlyFeedback, 30.0f },
            { XID::DlySpread, 60.0f },
            { XID::DlyMix, 16.0f },
            { XID::DlyDuck, 65.0f },
            { XID::DlyDuckRelease, 200.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 2.0f },
            { XID::LimCeiling, -0.5f },
            { XID::LimRelease, 70.0f },
            { XID::LimClip, 12.0f },
        } },
        { "Trap Ad-lib (Hard-Tune)", {
            { XID::PreGain, 44.0f },
            { XID::PreChar, 30.0f },
            { XID::PreHPF, 100.0f },
            { XID::GateThresh, -38.0f },
            { XID::GateRange, -70.0f },
            { XID::GateAttack, 1.0f },
            { XID::GateHold, 20.0f },
            { XID::GateRelease, 60.0f },
            { XID::TuneKey, 0.0f },
            // Chromatic + 0ms retune + 100% amount + formant OFF: the fully
            // robotic hard-tune snap this genre deliberately reaches for —
            // see the GranularPitchShifter doc comment on why that
            // character is a feature here, not a bug to hide.
            { XID::TuneScale, 2.0f },
            { XID::TuneRetune, 0.0f },
            { XID::TuneAmount, 100.0f },
            { XID::TuneFormant, 0.0f },
            { XID::EssThresh, -18.0f },
            { XID::EssRange, -8.0f },
            { XID::EssFreq, 7000.0f },
            { XID::TrsAttack, 40.0f },
            { XID::TrsSustain, -30.0f },
            { XID::CompThresh, -20.0f },
            { XID::CompMakeup, 4.0f },
            { XID::CompAttack, 5.0f },
            { XID::CompRelease, 120.0f },
            { XID::CompMix, 100.0f },
            { XID::OptoReduction, 20.0f },
            { XID::OptoGain, 2.0f },
            { XID::OptoMix, 40.0f },
            { XID::EqLow, 1.0f },
            { XID::EqMid, -2.0f },
            { XID::EqHigh, 6.5f },
            { XID::ResAmount, 40.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 30.0f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 35.0f },
            { XID::ExcDrive, 50.0f },
            { XID::ExcTone, 45.0f },
            { XID::ExcMix, 40.0f },
            { XID::DblDetune, 18.0f },
            { XID::DblWidth, 95.0f },
            { XID::DblDelay, 18.0f },
            { XID::DblMix, 45.0f },
            { XID::RevSize, 30.0f },
            { XID::RevDecay, 1.0f },
            { XID::RevPreDelay, 8.0f },
            { XID::RevMix, 6.0f },
            { XID::RevDuck, 80.0f },
            { XID::RevDuckRelease, 180.0f },
            { XID::DlyFeedback, 42.0f },
            { XID::DlySpread, 70.0f },
            { XID::DlyMix, 24.0f },
            { XID::DlyDuck, 60.0f },
            { XID::DlyDuckRelease, 180.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 3.0f },
            { XID::LimCeiling, -0.3f },
            { XID::LimRelease, 60.0f },
            { XID::LimClip, 18.0f },
        } },
        { "R&B Smooth", {
            { XID::PreGain, 30.0f },
            { XID::PreChar, 25.0f },
            { XID::PreHPF, 70.0f },
            { XID::GateThresh, -46.0f },
            { XID::GateRange, -55.0f },
            { XID::GateAttack, 3.0f },
            { XID::GateHold, 60.0f },
            { XID::GateRelease, 160.0f },
            { XID::TuneKey, 0.0f },
            // Slow, natural glide, gentle amount, formant preserved: pitch
            // support you can barely hear working, not a hard-tune effect.
            { XID::TuneScale, 1.0f },
            { XID::TuneRetune, 120.0f },
            { XID::TuneAmount, 35.0f },
            { XID::TuneFormant, 1.0f },
            { XID::EssThresh, -17.0f },
            { XID::EssRange, -7.0f },
            { XID::EssFreq, 6000.0f },
            // Softened attack, longer sustain — smooths consonants and lets
            // held notes breathe instead of choking the tail off.
            { XID::TrsAttack, -10.0f },
            { XID::TrsSustain, 25.0f },
            { XID::CompThresh, -18.0f },
            { XID::CompMakeup, 2.5f },
            { XID::CompAttack, 15.0f },
            { XID::CompRelease, 300.0f },
            { XID::CompMix, 90.0f },
            { XID::OptoReduction, 45.0f },
            { XID::OptoGain, 4.0f },
            { XID::OptoMix, 85.0f },
            { XID::EqLow, 3.5f },
            { XID::EqMid, -1.0f },
            { XID::EqHigh, 3.0f },
            { XID::ResAmount, 50.0f },
            { XID::ResSharpness, 50.0f },
            { XID::ResReactivity, 50.0f },
            { XID::ResNotchLimit, -12.0f },
            { XID::ResLow, 120.0f },
            { XID::ResHigh, 9400.0f },
            { XID::SatDrive, 25.0f },
            { XID::SatTone, 0.0f },
            { XID::SatCeiling, -0.3f },
            { XID::SatMix, 30.0f },
            { XID::ExcDrive, 15.0f },
            { XID::ExcTone, 55.0f },
            { XID::ExcMix, 15.0f },
            { XID::DblDetune, 8.0f },
            { XID::DblWidth, 70.0f },
            { XID::DblDelay, 10.0f },
            { XID::DblMix, 20.0f },
            { XID::RevSize, 60.0f },
            { XID::RevDecay, 2.8f },
            { XID::RevPreDelay, 22.0f },
            { XID::RevMix, 22.0f },
            { XID::RevDuck, 65.0f },
            { XID::RevDuckRelease, 240.0f },
            { XID::DlyFeedback, 34.0f },
            { XID::DlySpread, 55.0f },
            { XID::DlyMix, 14.0f },
            { XID::DlyDuck, 65.0f },
            { XID::DlyDuckRelease, 240.0f },
            { XID::DlyPanRate, 0.5f },
            { XID::LimInputGain, 1.5f },
            { XID::LimCeiling, -0.6f },
            { XID::LimRelease, 100.0f },
            { XID::LimClip, 5.0f },
        } },
    };
    return presets;
}
