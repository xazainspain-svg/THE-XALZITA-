#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"

/**
    The XaLZa — the full 12-module vocal chain from the web mockup:

      Preamp -> Gate -> De-esser -> Glue Comp -> Opto -> EQ 550 ->
      Resonance -> Saturator -> Doubler -> Reverb -> Delay -> Limiter

    with Master In/Out gain and Stereo Width around the outside. Every
    macro-linked parameter is resolved each block via MacroTouchTracker
    so "last touched wins" behaves identically to the web mockup and the
    Max for Live device.
*/
class XaLZaProcessor : public juce::AudioProcessor
{
public:
    XaLZaProcessor();
    ~XaLZaProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "The XaLZa"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    MacroTouchTracker macroTracker;

private:
    double sr = 44100.0;

    // ---- 1) Preamp: HPF, clean gain, tanh "character" blended dry/wet ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> preHpf;

    // ---- 2) Gate: envelope-follower expander/gate with hold ----
    float gateEnv = 0.0f;
    float gateGain = 1.0f;   // smoothed linear gain currently applied
    int   gateHoldCounter = 0;

    // ---- 3) De-esser: dynamic peak filter driven by a sibilance-band ----
    juce::dsp::IIR::Filter<float> essDetectL, essDetectR;   // band detector (not applied to main signal)
    float essEnv = 0.0f;
    float essGainDb = 0.0f;  // smoothed current attenuation (negative dB)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> essDynEq;

    // ---- 4) Glue Comp ----
    juce::dsp::Compressor<float> compressor;

    // ---- 5) Opto (slow, program-dependent 2nd compressor stage) ----
    juce::dsp::Compressor<float> optoComp;

    // ---- 6) EQ 550 — 3-band (low shelf / mid peak / high shelf) ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> eqLowShelf, eqMidPeak, eqHighShelf;

    // ---- 7) Resonance — static de-resonator notch (tames a harsh peak) ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> resNotch;

    // ---- 8) Saturator — tanh drive + tone tilt + soft ceiling, dry/wet mix ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> satTone;

    // ---- 9) Doubler — two modulated delay voices (chorus-style detune) ----
    juce::dsp::DelayLine<float> dblDelayL, dblDelayR;
    float dblPhase1 = 0.0f, dblPhase2 = 0.0f;

    // ---- 10) Reverb, with its own pre-delay line + duck envelope ----
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> revPreDelayL, revPreDelayR;
    float revDuckEnv = 0.0f;

    // ---- 11) Delay — ping-pong with spread, duck, and an auto-pan LFO ----
    juce::dsp::DelayLine<float> delayL, delayR;
    float dlyDuckEnv = 0.0f;
    float dlyPanPhase = 0.0f;

    // ---- 12) Limiter — input trim, ceiling, release, extra clip stage ----
    juce::dsp::Limiter<float> limiter;

    // Pre-allocated scratch buffers, sized in prepareToPlay so processBlock()
    // never allocates on the audio thread.
    juce::AudioBuffer<float> dryBuffer;               // reused per dry/wet stage
    juce::AudioBuffer<float> revBuffer, dlyBuffer, dblBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaProcessor)
};
