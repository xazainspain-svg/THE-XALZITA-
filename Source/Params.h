#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <atomic>

// ---------------------------------------------------------------------------
// Parameter IDs — the full 12-module chain from the original web mockup
// (Preamp, Gate, De-esser, Glue Comp, Opto, EQ 550, Resonance, Saturator,
// Doubler, Reverb, Delay, Limiter), same names/ranges/defaults as the
// mockup's PARAM_META table and its "Flat" / "Warm Lead Vocal" presets.
// ---------------------------------------------------------------------------
namespace XID
{
    static const juce::String MasterInGain  = "MasterInGain";
    static const juce::String MasterOutGain = "MasterOutGain";
    static const juce::String MasterWidth   = "MasterWidth";

    static const juce::String PreMacro = "PreMacro";
    static const juce::String PreGain  = "PreGain";
    static const juce::String PreChar  = "PreChar";
    static const juce::String PreHPF   = "PreHPF";

    static const juce::String GateMacro   = "GateMacro";
    static const juce::String GateThresh  = "GateThresh";
    static const juce::String GateRange   = "GateRange";
    static const juce::String GateAttack  = "GateAttack";
    static const juce::String GateHold    = "GateHold";
    static const juce::String GateRelease = "GateRelease";

    static const juce::String EssMacro  = "EssMacro";
    static const juce::String EssThresh = "EssThresh";
    static const juce::String EssRange  = "EssRange";
    static const juce::String EssFreq   = "EssFreq";

    static const juce::String CompMacro   = "CompMacro";
    static const juce::String CompThresh  = "CompThresh";
    static const juce::String CompMakeup  = "CompMakeup";
    static const juce::String CompAttack  = "CompAttack";
    static const juce::String CompRelease = "CompRelease";
    static const juce::String CompMix     = "CompMix";
    static const juce::String CompRatio   = "CompRatio";

    static const juce::String OptoMacro     = "OptoMacro";
    static const juce::String OptoReduction = "OptoReduction";
    static const juce::String OptoGain      = "OptoGain";
    static const juce::String OptoMix       = "OptoMix";

    static const juce::String EqMacro = "EqMacro";
    static const juce::String EqLow   = "EqLow";
    static const juce::String EqMid   = "EqMid";
    static const juce::String EqHigh  = "EqHigh";

    static const juce::String ResMacro      = "ResMacro";
    static const juce::String ResAmount     = "ResAmount";
    static const juce::String ResSharpness  = "ResSharpness";
    static const juce::String ResReactivity = "ResReactivity";
    static const juce::String ResNotchLimit = "ResNotchLimit";
    static const juce::String ResLow        = "ResLow";
    static const juce::String ResHigh       = "ResHigh";

    static const juce::String SatMacro   = "SatMacro";
    static const juce::String SatDrive   = "SatDrive";
    static const juce::String SatTone    = "SatTone";
    static const juce::String SatCeiling = "SatCeiling";
    static const juce::String SatMix     = "SatMix";

    static const juce::String DblMacro  = "DblMacro";
    static const juce::String DblDetune = "DblDetune";
    static const juce::String DblWidth  = "DblWidth";
    static const juce::String DblDelay  = "DblDelay";
    static const juce::String DblMix    = "DblMix";

    static const juce::String RevMacro         = "RevMacro";
    static const juce::String RevSize          = "RevSize";
    static const juce::String RevDecay         = "RevDecay";
    static const juce::String RevPreDelay      = "RevPreDelay";
    static const juce::String RevMix           = "RevMix";
    static const juce::String RevDuck          = "RevDuck";
    static const juce::String RevDuckRelease   = "RevDuckRelease";

    static const juce::String DlyMacro       = "DlyMacro";
    static const juce::String DlyTime        = "DlyTime";
    static const juce::String DlyFeedback    = "DlyFeedback";
    static const juce::String DlySpread      = "DlySpread";
    static const juce::String DlyMix         = "DlyMix";
    static const juce::String DlyDuck        = "DlyDuck";
    static const juce::String DlyDuckRelease = "DlyDuckRelease";
    static const juce::String DlyPanRate     = "DlyPanRate";

    static const juce::String LimMacro     = "LimMacro";
    static const juce::String LimInputGain = "LimInputGain";
    static const juce::String LimCeiling   = "LimCeiling";
    static const juce::String LimRelease   = "LimRelease";
    static const juce::String LimClip      = "LimClip";
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

    add(XID::PreMacro, "Pre Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::PreGain, "Pre Gain", 0.0f, 70.0f, 0.0f);
    add(XID::PreChar, "Pre Character", 0.0f, 100.0f, 0.0f);
    add(XID::PreHPF, "Pre HPF", 20.0f, 400.0f, 20.0f);

    add(XID::GateMacro, "Gate Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::GateThresh, "Gate Threshold", -70.0f, -10.0f, -50.0f);
    add(XID::GateRange, "Gate Range", -80.0f, 0.0f, -60.0f);
    add(XID::GateAttack, "Gate Attack", 0.1f, 50.0f, 2.0f);
    add(XID::GateHold, "Gate Hold", 0.0f, 500.0f, 45.0f);
    add(XID::GateRelease, "Gate Release", 5.0f, 1000.0f, 120.0f);

    add(XID::EssMacro, "De-esser Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::EssThresh, "De-esser Threshold", -40.0f, 0.0f, -20.0f);
    add(XID::EssRange, "De-esser Range", -24.0f, 0.0f, -8.0f);
    add(XID::EssFreq, "De-esser Freq", 2000.0f, 12000.0f, 6300.0f);

    add(XID::CompMacro, "Comp Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::CompThresh, "Comp Threshold", -40.0f, 0.0f, -20.0f);
    add(XID::CompMakeup, "Comp Makeup", -6.0f, 12.0f, 0.0f);
    add(XID::CompAttack, "Comp Attack", 0.1f, 80.0f, 12.0f);
    add(XID::CompRelease, "Comp Release", 20.0f, 1000.0f, 250.0f);
    add(XID::CompMix, "Comp Mix", 0.0f, 100.0f, 0.0f);
    add(XID::CompRatio, "Comp Ratio", 1.0f, 50.0f, 4.0f);

    add(XID::OptoMacro, "Opto Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::OptoReduction, "Opto Reduction", 0.0f, 100.0f, 0.0f);
    add(XID::OptoGain, "Opto Gain", -6.0f, 18.0f, 0.0f);
    add(XID::OptoMix, "Opto Mix", 0.0f, 100.0f, 0.0f);

    add(XID::EqMacro, "EQ Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::EqLow, "EQ Low", -12.0f, 12.0f, 0.0f);
    add(XID::EqMid, "EQ Mid", -12.0f, 12.0f, 0.0f);
    add(XID::EqHigh, "EQ High", -12.0f, 12.0f, 0.0f);

    add(XID::ResMacro, "Res Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::ResAmount, "Res Amount", 0.0f, 100.0f, 0.0f);
    add(XID::ResSharpness, "Res Sharpness", 0.0f, 100.0f, 50.0f);
    add(XID::ResReactivity, "Res Reactivity", 0.0f, 100.0f, 50.0f);
    add(XID::ResNotchLimit, "Res Notch Limit", -24.0f, 0.0f, -12.0f);
    add(XID::ResLow, "Res Low", 20.0f, 400.0f, 120.0f);
    add(XID::ResHigh, "Res High", 2000.0f, 16000.0f, 9400.0f);

    add(XID::SatMacro, "Sat Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::SatDrive, "Sat Drive", 0.0f, 100.0f, 0.0f);
    add(XID::SatTone, "Sat Tone", -12.0f, 12.0f, 0.0f);
    add(XID::SatCeiling, "Sat Ceiling", -6.0f, 0.0f, -0.3f);
    add(XID::SatMix, "Sat Mix", 0.0f, 100.0f, 0.0f);

    add(XID::DblMacro, "Doubler Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::DblDetune, "Doubler Detune", 0.0f, 40.0f, 12.0f);
    add(XID::DblWidth, "Doubler Width", 0.0f, 100.0f, 88.0f);
    add(XID::DblDelay, "Doubler Delay", 0.0f, 40.0f, 14.0f);
    add(XID::DblMix, "Doubler Mix", 0.0f, 100.0f, 0.0f);

    add(XID::RevMacro, "Rev Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::RevSize, "Rev Size", 0.0f, 100.0f, 55.0f);
    add(XID::RevDecay, "Rev Decay", 0.3f, 8.0f, 2.4f);
    add(XID::RevPreDelay, "Rev Pre-Delay", 0.0f, 100.0f, 18.0f);
    add(XID::RevMix, "Rev Mix", 0.0f, 100.0f, 0.0f);
    add(XID::RevDuck, "Rev Duck", 0.0f, 100.0f, 70.0f);
    add(XID::RevDuckRelease, "Rev Duck Release", 20.0f, 800.0f, 220.0f);

    add(XID::DlyMacro, "Dly Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::DlyTime, "Dly Time", 20.0f, 1000.0f, 250.0f);
    add(XID::DlyFeedback, "Dly Feedback", 0.0f, 90.0f, 38.0f);
    add(XID::DlySpread, "Dly Spread", 0.0f, 100.0f, 64.0f);
    add(XID::DlyMix, "Dly Mix", 0.0f, 100.0f, 0.0f);
    add(XID::DlyDuck, "Dly Duck", 0.0f, 100.0f, 70.0f);
    add(XID::DlyDuckRelease, "Dly Duck Release", 20.0f, 800.0f, 220.0f);
    add(XID::DlyPanRate, "Dly Pan Rate", 0.05f, 4.0f, 0.5f);

    add(XID::LimMacro, "Lim Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::LimInputGain, "Lim Input Gain", -12.0f, 12.0f, 0.0f);
    add(XID::LimCeiling, "Lim Ceiling", -6.0f, 0.0f, -0.3f);
    add(XID::LimRelease, "Lim Release", 10.0f, 500.0f, 80.0f);
    add(XID::LimClip, "Lim Clip", 0.0f, 100.0f, 0.0f);

    return {p.begin(), p.end()};
}

// ---------------------------------------------------------------------------
// Macro routing — one knob per module that scales that module's own real
// params between the mockup's "Flat" preset value (0%, neutral) and its
// "Warm Lead Vocal" preset value (100%, full character). Pulled directly
// from the mockup's MACRO_PARAM_KEYS + those two presets, so a macro here
// does exactly what the same macro does in the web version. Whichever of
// (macro, that specific manual knob) was touched more recently wins
// ("last moved wins"), same semantic as the web mockup and the M4L device.
// ---------------------------------------------------------------------------
struct MacroTarget
{
    juce::String paramID;
    float neutral;
    float full;
};

inline const std::map<juce::String, std::vector<MacroTarget>>& xalzaMacroMap()
{
    static const std::map<juce::String, std::vector<MacroTarget>> m = {
        { XID::PreMacro, { {XID::PreGain, 0.0f, 42.0f}, {XID::PreChar, 0.0f, 38.0f},
                            {XID::PreHPF, 20.0f, 80.0f} } },
        { XID::GateMacro, { {XID::GateThresh, -50.0f, -42.0f}, {XID::GateRange, -60.0f, -60.0f},
                             {XID::GateAttack, 2.0f, 2.0f}, {XID::GateHold, 45.0f, 45.0f},
                             {XID::GateRelease, 120.0f, 120.0f} } },
        { XID::EssMacro, { {XID::EssThresh, -20.0f, -16.0f}, {XID::EssRange, -8.0f, -8.0f},
                            {XID::EssFreq, 6300.0f, 6300.0f} } },
        { XID::CompMacro, { {XID::CompThresh, -20.0f, -18.5f}, {XID::CompMakeup, 0.0f, 3.2f},
                             {XID::CompAttack, 12.0f, 12.0f}, {XID::CompRelease, 250.0f, 250.0f},
                             {XID::CompMix, 0.0f, 100.0f} } },
        { XID::OptoMacro, { {XID::OptoReduction, 0.0f, 42.0f}, {XID::OptoGain, 0.0f, 5.0f},
                             {XID::OptoMix, 0.0f, 100.0f} } },
        { XID::EqMacro, { {XID::EqLow, 0.0f, 4.0f}, {XID::EqMid, 0.0f, -2.0f}, {XID::EqHigh, 0.0f, 6.0f} } },
        { XID::ResMacro, { {XID::ResAmount, 0.0f, 75.0f}, {XID::ResSharpness, 50.0f, 50.0f},
                            {XID::ResReactivity, 50.0f, 50.0f}, {XID::ResNotchLimit, -12.0f, -12.0f},
                            {XID::ResLow, 120.0f, 120.0f}, {XID::ResHigh, 9400.0f, 9400.0f} } },
        { XID::SatMacro, { {XID::SatDrive, 0.0f, 64.0f}, {XID::SatTone, 0.0f, 0.0f},
                            {XID::SatCeiling, -0.3f, -0.3f}, {XID::SatMix, 0.0f, 80.0f} } },
        { XID::DblMacro, { {XID::DblDetune, 12.0f, 12.0f}, {XID::DblWidth, 88.0f, 88.0f},
                            {XID::DblDelay, 14.0f, 14.0f}, {XID::DblMix, 0.0f, 50.0f} } },
        { XID::RevMacro, { {XID::RevSize, 55.0f, 55.0f}, {XID::RevDecay, 2.4f, 2.4f},
                            {XID::RevPreDelay, 18.0f, 18.0f}, {XID::RevMix, 0.0f, 28.0f},
                            {XID::RevDuck, 70.0f, 70.0f}, {XID::RevDuckRelease, 220.0f, 220.0f} } },
        { XID::DlyMacro, { {XID::DlyFeedback, 38.0f, 38.0f}, {XID::DlySpread, 64.0f, 64.0f},
                            {XID::DlyMix, 0.0f, 22.0f}, {XID::DlyDuck, 70.0f, 60.0f},
                            {XID::DlyDuckRelease, 220.0f, 200.0f}, {XID::DlyPanRate, 0.5f, 0.5f} } },
        { XID::LimMacro, { {XID::LimInputGain, 0.0f, 2.4f}, {XID::LimCeiling, -0.3f, -1.0f},
                            {XID::LimRelease, 80.0f, 80.0f}, {XID::LimClip, 0.0f, 10.0f} } },
    };
    return m;
}

/** Listens to every macro + every macro-linked manual parameter, and
    answers "what's the effective value of this parameter right now" by
    comparing touch order. Lock-free: safe to query from the audio thread. */
class MacroTouchTracker : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MacroTouchTracker(juce::AudioProcessorValueTreeState& state) : apvts(state)
    {
        for (auto& [macroID, targets] : xalzaMacroMap())
        {
            apvts.addParameterListener(macroID, this);
            touch[macroID] = 0;
            for (auto& t : targets)
            {
                apvts.addParameterListener(t.paramID, this);
                touch[t.paramID] = 0;
            }
        }
    }

    ~MacroTouchTracker() override
    {
        for (auto& [macroID, targets] : xalzaMacroMap())
        {
            apvts.removeParameterListener(macroID, this);
            for (auto& t : targets)
                apvts.removeParameterListener(t.paramID, this);
        }
    }

    void parameterChanged(const juce::String& parameterID, float) override
    {
        touch[parameterID].store(counter.fetch_add(1, std::memory_order_relaxed),
                                  std::memory_order_relaxed);
    }

    /** Effective value for a macro-linked parameter: if its own macro was
        touched more recently than this specific knob, blend toward the
        macro's target; otherwise use the knob's raw current value. */
    float effective(const juce::String& macroID, const MacroTarget& target) const
    {
        float manualRaw = apvts.getRawParameterValue(target.paramID)->load();
        auto macroWins = touch.at(macroID).load(std::memory_order_relaxed)
                        > touch.at(target.paramID).load(std::memory_order_relaxed);
        if (!macroWins)
            return manualRaw;
        auto macroPct = *apvts.getRawParameterValue(macroID) / 100.0f;
        return juce::jmap(juce::jlimit(0.0f, 1.0f, macroPct), 0.0f, 1.0f,
                           target.neutral, target.full);
    }

    /** Convenience overload: looks up the MacroTarget for (macroID, paramID) in
        xalzaMacroMap() and calls effective(). If paramID isn't macro-linked
        (a manual-only knob like CompRatio or DlyTime), just returns the raw
        parameter value. This is what processBlock() calls, one line per knob. */
    float effectiveByID(const juce::String& macroID, const juce::String& paramID) const
    {
        auto it = xalzaMacroMap().find(macroID);
        if (it != xalzaMacroMap().end())
            for (auto& t : it->second)
                if (t.paramID == paramID)
                    return effective(macroID, t);
        return *apvts.getRawParameterValue(paramID);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<uint64_t> counter{1};
    std::map<juce::String, std::atomic<uint64_t>> touch;
};
