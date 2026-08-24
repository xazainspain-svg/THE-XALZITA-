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

    // ---- Metering / visualisers: written on the audio thread, read (lock
    //      free) by the editor's Timer at ~30Hz. Order matches the real
    //      serial signal chain in processBlock(). ----
    enum MeterTap
    {
        TapIn = 0, TapPre, TapGate, TapEss, TapComp, TapOpto, TapEq, TapRes,
        TapSat, TapDbl, TapRev, TapDly, TapLim, TapOut, kNumMeterTaps
    };

    float getMeterDbL(int tap) const noexcept { return meterDbL[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }
    float getMeterDbR(int tap) const noexcept { return meterDbR[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }

    // Gain-reduction readout (positive dB = amount of reduction): 0=Comp, 1=Opto, 2=Lim
    float getGrDb(int moduleIdx) const noexcept { return grDb[(size_t) juce::jlimit(0, 2, moduleIdx)].load(std::memory_order_relaxed); }

    // Goniometer: a lock-free ring of decimated post-chain stereo samples.
    static constexpr int kScopeSize = 1024; // power of two
    float scopeSampleL(int i) const noexcept { return scopePointsL[(size_t) (i & (kScopeSize - 1))].load(std::memory_order_relaxed); }
    float scopeSampleR(int i) const noexcept { return scopePointsR[(size_t) (i & (kScopeSize - 1))].load(std::memory_order_relaxed); }
    int   getScopeWritePos() const noexcept { return scopeWritePos.load(std::memory_order_relaxed); }

    // Spectrum tap: a lock-free ring of raw (full-rate, mono) samples taken
    // right after EQ 550, for the EQ page's live spectrum analyser.
    static constexpr int kSpecSize = 8192; // power of two, comfortably >= UI's FFT window
    float specSample(int i) const noexcept { return specRing[(size_t) (i & (kSpecSize - 1))].load(std::memory_order_relaxed); }
    int   getSpecWritePos() const noexcept { return specWritePos.load(std::memory_order_relaxed); }

    // Generic raw-sample taps for the remaining per-module visualisers
    // (oscilloscopes / harmonic bars). Each is genuinely POST that module's
    // own processing — never the module's input — so every visualiser shows
    // what that stage actually did to the signal.
    enum RawTap { RawPre = 0, RawGate, RawSatIn, RawSatOut, RawOpto, kNumRawTaps };
    static constexpr int kRawSize = 8192; // power of two
    float rawSample(int tap, int i) const noexcept
    {
        auto t = (size_t) juce::jlimit(0, kNumRawTaps - 1, tap);
        return rawRing[t][(size_t) (i & (kRawSize - 1))].load(std::memory_order_relaxed);
    }
    int getRawWritePos(int tap) const noexcept
    {
        return rawWritePos[(size_t) juce::jlimit(0, kNumRawTaps - 1, tap)].load(std::memory_order_relaxed);
    }

    // Preamp HPF cutoff, read directly by the UI to draw an analytic
    // frequency-response curve (no audio tap needed for that one).
    float getCurrentHpfHz() const noexcept { return lastHpfHz.load(std::memory_order_relaxed); }

    // Per-block readouts for the modules whose visualiser is an envelope /
    // suppression-depth line rather than a raw waveform or GR percentage.
    float getGateGrDb() const noexcept { return gateGrDbUI.load(std::memory_order_relaxed); }
    float getEssBandDb() const noexcept { return essBandDbUI.load(std::memory_order_relaxed); }
    float getEssReductionDb() const noexcept { return essReductionDbUI.load(std::memory_order_relaxed); }
    float getResCutDb() const noexcept { return resCutDbUI.load(std::memory_order_relaxed); }

private:
    void updateMeter(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);
    void updateGr(int moduleIdx, float preDb, float postDb);
    void pushRaw(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);

    std::array<std::atomic<float>, kSpecSize> specRing;
    std::atomic<int> specWritePos { 0 };

    std::array<std::array<std::atomic<float>, kRawSize>, kNumRawTaps> rawRing;
    std::array<std::atomic<int>, kNumRawTaps> rawWritePos;
    std::atomic<float> lastHpfHz { 20.0f };

    std::atomic<float> gateGrDbUI { 0.0f };
    std::atomic<float> essBandDbUI { -100.0f };
    std::atomic<float> essReductionDbUI { 0.0f };
    std::atomic<float> resCutDbUI { 0.0f };

    std::array<std::atomic<float>, kNumMeterTaps> meterDbL, meterDbR;
    std::array<std::atomic<float>, 3> grDb;
    float meterAttCoef = 0.3f, meterRelCoef = 0.9995f;

    std::array<std::atomic<float>, kScopeSize> scopePointsL, scopePointsR;
    std::atomic<int> scopeWritePos { 0 };

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
