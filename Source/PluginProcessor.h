#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"

/**
    The XaLZa — vocal chain: Preamp -> Compressor -> EQ -> Saturator ->
    [Reverb + Delay sends, ducked] -> Limiter -> Stereo Width, with
    Master In/Out gain around the outside. Same 8-module design as the
    Max for Live device and the web mockup; every macro-linked parameter
    is resolved each block via MacroTouchTracker so "last touched wins"
    behaves identically across all three deliverables.
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

    // Preamp: high-pass filter, clean gain, and a tanh "character" stage
    // blended in dry/wet (character is not full-on distortion, it's a
    // subtle warmth control, hence the blend rather than 100% wet).
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> preHpf;

    // Compressor (attack/release are fixed — only threshold/ratio/makeup
    // are exposed as controls, matching the M4L device).
    juce::dsp::Compressor<float> compressor;

    // 2-band shelving EQ
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> eqLowShelf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> eqHighShelf;

    // Reverb + ping-pong delay, both run as parallel sends off the main
    // signal and mixed back in, each duck-able by the dry signal's level.
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> delayL, delayR;

    // Limiter (ceiling / brickwall stage, with its own input trim)
    juce::dsp::Limiter<float> limiter;

    // One-pole envelope follower on the dry signal, shared by both sends'
    // duck amounts (each send has its own Duck knob controlling how much
    // that envelope pulls its wet level down).
    float duckEnvShared = 0.0f;

    // Pre-allocated scratch buffers for the reverb/delay sends and the
    // per-sample duck envelope, sized in prepareToPlay so processBlock()
    // doesn't allocate.
    juce::AudioBuffer<float> revBuffer, dlyBuffer;
    juce::AudioBuffer<float> duckEnvBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaProcessor)
};
