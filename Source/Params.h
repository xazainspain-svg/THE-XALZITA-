#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <atomic>

// ---------------------------------------------------------------------------
// Parameter IDs — same names/ranges as the Max for Live v1 device and the
// original web mockup, so all three deliverables agree on what "Pre Gain"
// or "Rev Mix" means.
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

    static const juce::String CompMacro  = "CompMacro";
    static const juce::String CompThresh = "CompThresh";
    static const juce::String CompRatio  = "CompRatio";
    static const juce::String CompMakeup = "CompMakeup";

    static const juce::String EqMacro    = "EqMacro";
    static const juce::String EqLowGain  = "EqLowGain";
    static const juce::String EqHighGain = "EqHighGain";

    static const juce::String SatMacro = "SatMacro";
    static const juce::String SatDrive = "SatDrive";
    static const juce::String SatMix   = "SatMix";

    static const juce::String RevMacro = "RevMacro";
    static const juce::String RevSize  = "RevSize";
    static const juce::String RevMix   = "RevMix";
    static const juce::String RevDuck  = "RevDuck";

    static const juce::String DlyMacro    = "DlyMacro";
    static const juce::String DlyTime     = "DlyTime";
    static const juce::String DlyFeedback = "DlyFeedback";
    static const juce::String DlyMix      = "DlyMix";
    static const juce::String DlyDuck     = "DlyDuck";

    static const juce::String LimMacro     = "LimMacro";
    static const juce::String LimCeiling   = "LimCeiling";
    static const juce::String LimInputGain = "LimInputGain";
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
    add(XID::PreGain, "Pre Gain", -24.0f, 24.0f, 0.0f);
    add(XID::PreChar, "Pre Character", 0.0f, 100.0f, 0.0f);
    add(XID::PreHPF, "Pre HPF", 20.0f, 300.0f, 20.0f);

    add(XID::CompMacro, "Comp Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::CompThresh, "Comp Threshold", -60.0f, 0.0f, -18.0f);
    add(XID::CompRatio, "Comp Ratio", 1.0f, 20.0f, 1.0f);
    add(XID::CompMakeup, "Comp Makeup", 0.0f, 24.0f, 0.0f);

    add(XID::EqMacro, "EQ Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::EqLowGain, "EQ Low Gain", -12.0f, 12.0f, 0.0f);
    add(XID::EqHighGain, "EQ High Gain", -12.0f, 12.0f, 0.0f);

    add(XID::SatMacro, "Sat Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::SatDrive, "Sat Drive", 0.0f, 100.0f, 0.0f);
    add(XID::SatMix, "Sat Mix", 0.0f, 100.0f, 0.0f);

    add(XID::RevMacro, "Rev Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::RevSize, "Rev Size", 0.0f, 100.0f, 30.0f);
    add(XID::RevMix, "Rev Mix", 0.0f, 100.0f, 0.0f);
    add(XID::RevDuck, "Rev Duck", 0.0f, 100.0f, 0.0f);

    add(XID::DlyMacro, "Dly Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::DlyTime, "Dly Time", 20.0f, 1000.0f, 250.0f);
    add(XID::DlyFeedback, "Dly Feedback", 0.0f, 90.0f, 15.0f);
    add(XID::DlyMix, "Dly Mix", 0.0f, 100.0f, 0.0f);
    add(XID::DlyDuck, "Dly Duck", 0.0f, 100.0f, 0.0f);

    add(XID::LimMacro, "Lim Intensity", 0.0f, 100.0f, 0.0f);
    add(XID::LimCeiling, "Lim Ceiling", -6.0f, 0.0f, -1.0f);
    add(XID::LimInputGain, "Lim Input Gain", 0.0f, 24.0f, 0.0f);

    return {p.begin(), p.end()};
}

// ---------------------------------------------------------------------------
// Macro routing: each macro morphs a list of target parameters between a
// "neutral" and a "full character" value — same interpolation concept as
// the web mockup's Macros view and the M4L device. Whichever of (macro,
// that specific manual knob) was touched more recently wins, exactly like
// the other two versions ("last moved wins", not additive).
//
// Implementation: a single monotonically increasing touch counter. Every
// relevant parameter's listener stamps "when I last changed" into an
// atomic. processBlock only ever READS these atomics (lock-free, safe to
// call from the audio thread) to decide, per target parameter, whether to
// use the macro-interpolated value or the manual knob's raw value.
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
        { XID::PreMacro, { {XID::PreGain, 0.0f, 12.0f}, {XID::PreChar, 0.0f, 38.0f},
                            {XID::PreHPF, 20.0f, 80.0f} } },
        { XID::CompMacro, { {XID::CompThresh, -6.0f, -18.5f}, {XID::CompRatio, 1.0f, 4.0f},
                             {XID::CompMakeup, 0.0f, 3.2f} } },
        { XID::EqMacro, { {XID::EqLowGain, 0.0f, 4.0f}, {XID::EqHighGain, 0.0f, 6.0f} } },
        { XID::SatMacro, { {XID::SatDrive, 0.0f, 64.0f}, {XID::SatMix, 0.0f, 80.0f} } },
        { XID::RevMacro, { {XID::RevSize, 30.0f, 55.0f}, {XID::RevMix, 0.0f, 28.0f},
                            {XID::RevDuck, 0.0f, 70.0f} } },
        { XID::DlyMacro, { {XID::DlyTime, 250.0f, 280.0f}, {XID::DlyFeedback, 15.0f, 38.0f},
                            {XID::DlyMix, 0.0f, 22.0f}, {XID::DlyDuck, 0.0f, 60.0f} } },
        { XID::LimMacro, { {XID::LimCeiling, -1.0f, -1.0f}, {XID::LimInputGain, 0.0f, 2.4f} } },
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
        (shouldn't happen for the IDs we call this with), just returns the raw
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
