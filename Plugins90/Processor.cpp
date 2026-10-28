// Plugin generico: cada target de CMake compila este archivo con XFX_INDEX = indice del efecto en el registro.
#include <juce_audio_processors/juce_audio_processors.h>
#include "Registry.h"

#ifndef XFX_INDEX
 #define XFX_INDEX 0
#endif

class XfxProcessor : public juce::AudioProcessor
{
public:
    XfxProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          info (xfx::registry().at (XFX_INDEX)),
          apvts (*this, nullptr, "STATE", createLayout (info))
    {
        for (size_t i = 0; i < info.params.size(); ++i) raw[i] = apvts.getRawParameterValue ("p" + juce::String ((int) i));
        rawMix = apvts.getRawParameterValue ("mix");
        rawOut = apvts.getRawParameterValue ("out");
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return juce::String (info.category) == "Time" ? 5.0 : 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr;
        fx = info.make();
        fx->prepare (sr);
        for (size_t i = 0; i < info.params.size(); ++i) { smooth[i].reset (sr, 0.02); smooth[i].setCurrentAndTargetValue (value (i)); }
        smoothMix.reset (sr, 0.02); smoothMix.setCurrentAndTargetValue (*rawMix);
        smoothOut.reset (sr, 0.02); smoothOut.setCurrentAndTargetValue (*rawOut);
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        if (fx == nullptr || buf.getNumChannels() < 2) return;

        const size_t n = info.params.size();
        for (size_t k = 0; k < n; ++k)
        {
            if (info.params[k].kind == 0) smooth[k].setTargetValue (value (k));
            else                          smooth[k].setCurrentAndTargetValue (value (k));
        }
        smoothMix.setTargetValue (*rawMix);
        smoothOut.setTargetValue (*rawOut);

        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        float p[xfx::kMaxParams] = {};

        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            for (size_t k = 0; k < n; ++k) p[k] = smooth[k].getNextValue();
            const float mix = smoothMix.getNextValue(), gain = xfx::dbToLin (smoothOut.getNextValue());
            const float dl = L[i], dr = R[i];
            float wl = dl, wr = dr;
            fx->tick (wl, wr, p);
            if (! std::isfinite (wl) || ! std::isfinite (wr)) { fx->prepare (sampleRate); wl = dl; wr = dr; }
            L[i] = juce::jlimit (-16.f, 16.f, (dl + (wl - dl) * mix) * gain);
            R[i] = juce::jlimit (-16.f, 16.f, (dr + (wr - dr) * mix) * gain);
        }
    }

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const xfx::Info& info)
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (size_t i = 0; i < info.params.size(); ++i)
        {
            const xfx::Param& q = info.params[i];
            const juce::String id = "p" + juce::String ((int) i);
            if (q.kind == 1)
                layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, q.name, q.def > 0.5f));
            else if (q.kind == 2)
                layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, q.name, (int) q.min, (int) q.max, (int) q.def));
            else
            {
                juce::NormalisableRange<float> range (q.min, q.max);
                if (q.skew < 0.f) range.setSkewForCentre (std::sqrt (q.min * q.max));
                layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, q.name, range, q.def));
            }
        }
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.f, 1.f), info.defMix));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out", 1 }, "Output dB", juce::NormalisableRange<float> (-24.f, 12.f), 0.f));
        return layout;
    }

    float value (size_t i) const { return raw[i] != nullptr ? raw[i]->load() : info.params[i].def; }

    const xfx::Info& info;
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<xfx::Fx> fx;
    std::atomic<float>* raw[xfx::kMaxParams] = {};
    std::atomic<float>* rawMix = nullptr;
    std::atomic<float>* rawOut = nullptr;
    juce::SmoothedValue<float> smooth[xfx::kMaxParams], smoothMix, smoothOut;
    double sampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XfxProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XfxProcessor(); }
