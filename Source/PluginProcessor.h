#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
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
    // MIDI input is accepted purely for macro-knob MIDI Learn/CC control
    // (see below) — this remains an audio effect (isMidiEffect stays
    // false), it just optionally also listens for CC messages.
    bool acceptsMidi() const override { return true; }
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

    // Last-used editor window size, restored on the next editor open (and
    // persisted through save/reload via getStateInformation) so a resize
    // sticks instead of always reopening at the hardcoded default. Only
    // ever touched on the message thread (editor ctor/resize, state I/O).
    int lastEditorWidth = 900, lastEditorHeight = 560;

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

    // Real RMS (mean-square) companion reading for the same tap — a
    // genuinely different measurement from the peak ballistics above, not
    // a derived/smoothed copy of it. See updateMeter().
    float getRmsDbL(int tap) const noexcept { return rmsDbL[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }
    float getRmsDbR(int tap) const noexcept { return rmsDbR[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }

    // ---- Reorderable chain: which of the 12 modules processBlock() runs
    //      first/second/.../last. Identity order (Pre, Gate, ... Lim, same
    //      as MeterTap above) by default — matches every original meter/
    //      raw-tap wiring exactly until the user actually reorders
    //      something. Enum order here is deliberately identical to
    //      MeterTap's Pre..Lim run so tapForSlot() below is just an offset. ----
    enum ModuleSlot
    {
        SlotPre = 0, SlotGate, SlotEss, SlotComp, SlotOpto, SlotEq, SlotRes,
        SlotSat, SlotDbl, SlotRev, SlotDly, SlotLim, kNumSlots
    };
    static int tapForSlot(int slotId) noexcept { return (int) TapPre + juce::jlimit(0, kNumSlots - 1, slotId); }
    static const char* slotName(int slotId) noexcept
    {
        static const char* names[kNumSlots] = { "PREAMP", "GATE", "DE-ESSER", "GLUE COMP", "OPTO",
                                                  "EQ 550", "RESONANCE", "SATURATOR", "DOUBLER",
                                                  "REVERB", "DELAY", "LIMITER" };
        return names[(size_t) juce::jlimit(0, kNumSlots - 1, slotId)];
    }

    // Which slot processes at chain position 'position' (0 = first).
    int getChainSlotAt(int position) const noexcept { return chainOrder[(size_t) juce::jlimit(0, kNumSlots - 1, position)].load(std::memory_order_relaxed); }
    // Where slotId currently sits (0 = first, kNumSlots-1 = last).
    int getChainPosition(int slotId) const noexcept
    {
        for (int pos = 0; pos < kNumSlots; ++pos)
            if (chainOrder[(size_t) pos].load(std::memory_order_relaxed) == slotId)
                return pos;
        return 0;
    }
    // Swaps slotId with its neighbour in the given direction (-1 = earlier,
    // +1 = later); a no-op at either end. UI-driven only (reordering is a
    // rare, deliberate action, not a per-block operation).
    void moveModule(int slotId, int direction) noexcept
    {
        int pos = getChainPosition(slotId);
        int newPos = juce::jlimit(0, kNumSlots - 1, pos + (direction < 0 ? -1 : 1));
        if (newPos == pos)
            return;
        int other = chainOrder[(size_t) newPos].load(std::memory_order_relaxed);
        chainOrder[(size_t) newPos].store(slotId, std::memory_order_relaxed);
        chainOrder[(size_t) pos].store(other, std::memory_order_relaxed);
    }
    // Live "input" meter tap for slotId: the previous module's own output
    // tap at whatever position slotId CURRENTLY sits (or the master input
    // level if it's first) — recomputed from the live chain order rather
    // than the fixed pairing the original 12-tab layout assumed, so each
    // module page's IN meter stays correct after a reorder.
    int getPredecessorTap(int slotId) const noexcept
    {
        int pos = getChainPosition(slotId);
        return pos == 0 ? (int) TapIn : tapForSlot(getChainSlotAt(pos - 1));
    }

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
    enum RawTap { RawPre = 0, RawGate, RawSatIn, RawSatOut, RawOpto, RawDly, RawLim, kNumRawTaps };
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

    // Real live phase (radians, wraps at 2*pi) of the Delay's ping-pong
    // auto-pan LFO — read directly by the UI so the Delay page's bounce
    // indicator swings in exact sync with what's actually panning the
    // repeats between L/R, not a separately-clocked visual guess.
    float getDlyPanPhase() const noexcept { return dlyPanPhaseUI.load(std::memory_order_relaxed); }
    // Per-band cut depth (real, one entry per currently-active Resonance
    // band) for the Dynamic Suppression bars — see runRes.
    float getResBandCutDb(int band) const noexcept
    {
        return resCutDbPerBandUI[(size_t) juce::jlimit(0, kMaxResBands - 1, band)].load(std::memory_order_relaxed);
    }

    // Post-Doubler stereo scope: a decimated ring of the wet doubler
    // signal's L/R, genuinely post that module, for the Doubler page's
    // stereo-field goniometer.
    float dblScopeSampleL(int i) const noexcept { return dblScopeL[(size_t) (i & (kScopeSize - 1))].load(std::memory_order_relaxed); }
    float dblScopeSampleR(int i) const noexcept { return dblScopeR[(size_t) (i & (kScopeSize - 1))].load(std::memory_order_relaxed); }
    int   getDblScopeWritePos() const noexcept { return dblScopeWritePos.load(std::memory_order_relaxed); }

    // ---- Reverb: user-loadable impulse response for the hybrid
    // algorithmic/convolution engine (see runRev). Decoding an audio file
    // is real disk/CPU work, so this is called from the MESSAGE thread
    // only (the Reverb page's "Load IR" file-chooser callback, or a saved
    // patch's own reload in setStateInformation) — never from
    // processBlock(). The decoded buffer is handed to the audio thread
    // through a lock-free SpinLock-tryLock transfer (irTransfer, drained
    // at the top of runRev every block); the actual
    // juce::dsp::Convolution::loadImpulseResponse() call only ever
    // happens on the audio thread, which is the realtime-safe pattern
    // JUCE's own ConvolutionDemo uses. Returns false if the file couldn't
    // be read as audio (unsupported format, corrupt data) — the
    // previously-loaded IR (if any) is left untouched in that case.
    bool loadImpulseResponseFile(const juce::File& file);
    void clearImpulseResponse();
    juce::String getIrFileName() const { return currentIrFile.existsAsFile() ? currentIrFile.getFileNameWithoutExtension() : juce::String(); }
    juce::File   getIrFile() const { return currentIrFile; }
    bool isIrLoaded() const noexcept { return irLoaded; }

    // Real (simplified, one-pole-integrated) ITU-R BS.1770 K-weighted
    // momentary loudness of the true final output (post-limiter, post-
    // width, post-master-gain) — a real measurement, not a fake animation.
    float getLufs() const noexcept { return lufsUI.load(std::memory_order_relaxed); }

    // Real 4x-oversampled inter-sample true-peak reading of the final
    // output, in dBTP.
    float getTruePeakDb() const noexcept { return truePeakDbUI.load(std::memory_order_relaxed); }

    // MIDI Learn for the 12 macro knobs: macroIdx indexes Params.h's
    // xalzaMacroIDs() (same order the editor's MACROS page shows them in).
    // startMidiLearn arms macroIdx to bind to the NEXT CC message received
    // in processBlock; the binding (or -1 = unbound) is read back with
    // getMacroCc() for the editor's tooltip/menu state, and persists across
    // save/reload via getStateInformation's extra XML attributes.
    static constexpr int kNumMacros = 12;
    int  getMacroCc(int macroIdx) const noexcept { return macroCcMap[(size_t) juce::jlimit(0, kNumMacros - 1, macroIdx)].load(std::memory_order_relaxed); }
    void startMidiLearn(int macroIdx) noexcept { midiLearnTarget.store(macroIdx, std::memory_order_relaxed); }
    void cancelMidiLearn() noexcept { midiLearnTarget.store(-1, std::memory_order_relaxed); }
    void clearMidiLearn(int macroIdx) noexcept { macroCcMap[(size_t) juce::jlimit(0, kNumMacros - 1, macroIdx)].store(-1, std::memory_order_relaxed); }
    bool isMidiLearning(int macroIdx) const noexcept { return midiLearnTarget.load(std::memory_order_relaxed) == macroIdx; }

    juce::String getVersionString() const { return JucePlugin_VersionString; }

private:
    void updateMeter(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);
    void updateGr(int moduleIdx, float preDb, float postDb);
    void pushRaw(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);
    static void applySmoothedGainDb(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& smoother,
                                     juce::AudioBuffer<float>& buf, float targetDb, int numSamples);

    std::array<std::atomic<float>, kSpecSize> specRing;
    std::atomic<int> specWritePos { 0 };

    std::array<std::array<std::atomic<float>, kRawSize>, kNumRawTaps> rawRing;
    std::array<std::atomic<int>, kNumRawTaps> rawWritePos;
    std::atomic<float> lastHpfHz { 20.0f };

    std::atomic<float> gateGrDbUI { 0.0f };
    std::atomic<float> essBandDbUI { -100.0f };
    std::atomic<float> essReductionDbUI { 0.0f };
    std::atomic<float> resCutDbUI { 0.0f };
    std::atomic<float> dlyPanPhaseUI { 0.0f };

    std::array<std::atomic<float>, kScopeSize> dblScopeL, dblScopeR;
    std::atomic<int> dblScopeWritePos { 0 };

    std::array<std::atomic<int>, kNumMacros> macroCcMap;   // -1 = unbound
    std::atomic<int> midiLearnTarget { -1 };                // -1 = not currently learning

    std::array<std::atomic<int>, kNumSlots> chainOrder;    // identity order by default

    juce::dsp::IIR::Filter<float> lufsPreL, lufsPreR, lufsRlbL, lufsRlbR;
    float lufsMsL = 0.0f, lufsMsR = 0.0f;
    std::atomic<float> lufsUI { -70.0f };

    std::array<std::atomic<float>, kNumMeterTaps> meterDbL, meterDbR;
    std::array<std::atomic<float>, 3> grDb;
    float meterAttCoef = 0.3f, meterRelCoef = 0.9995f;

    // Real RMS companion reading for every meter tap above — a genuine
    // mean-square average (not the peak meter's fast-attack/slow-release
    // ballistics), symmetrically integrated over ~300ms like a real VU
    // instrument, so the LedMeter can show Peak and RMS together the way
    // the iZotope Insight Levels reference does.
    std::array<std::atomic<float>, kNumMeterTaps> rmsDbL, rmsDbR;
    float rmsCoef = 0.9f;

    std::array<std::atomic<float>, kScopeSize> scopePointsL, scopePointsR;
    std::atomic<int> scopeWritePos { 0 };

    double sr = 44100.0;

    // Smoothed gain stages — every "instant" dB->gain applied straight to
    // the buffer used to jump on every block when its parameter changed
    // (or was automated), which can click/zipper. Each of these now ramps
    // over ~20ms instead of stepping.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterInSmoothed, masterOutSmoothed,
        preGainSmoothed, compMakeupSmoothed, optoGainSmoothed, limInGainSmoothed, prePadGainSmoothed;

    // ---- 1) Preamp: HPF, clean gain, tanh "character" blended dry/wet ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> preHpf;
    // Real, subtle high-shelf tilt driven by the Impedance seg-group.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> preImpShelf;

    // ---- 2) Gate: envelope-follower expander/gate with hold ----
    float gateEnv = 0.0f;
    float gateGain = 1.0f;   // smoothed linear gain currently applied
    int   gateHoldCounter = 0;

    // Optional real lookahead: when on, a fixed 5ms delay line (same
    // convention as the limiter's own lookahead ring) sits between the
    // envelope detector and the output, so the gate genuinely reacts to a
    // transient before it reaches the delayed output rather than exactly
    // when it arrives. See runGate.
    juce::AudioBuffer<float> gateLaRing;
    int gateLaRingSize = 0, gateLaRingMask = 0, gateLaWritePos = 0, gateLaSamples = 0;
    bool gateLaWasEnabled = false;

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

    // ---- 7) Resonance — up to kMaxResBands parallel dynamically-tracking
    //      de-resonator notches, real count from ResBands: each covers its
    //      own log-spaced slice of the Low-High range, with its own
    //      bandpass-detector envelope (speed set by ResReactivity) driving
    //      how hard that notch bites in real time. ResStyle scales Q/detect
    //      width per band (Delicate=narrow+surgical, Wide=broad) ----
    static constexpr int kMaxResBands = 5;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> resNotch[kMaxResBands];
    juce::dsp::IIR::Filter<float> resDetectL[kMaxResBands], resDetectR[kMaxResBands];
    float resEnv[kMaxResBands] = {}, resCutSmoothed[kMaxResBands] = {};
    std::atomic<float> resCutDbPerBandUI[kMaxResBands];

    // ---- 8) Saturator — tanh drive + tone tilt + soft ceiling, dry/wet mix ----
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> satTone;

    // 2x oversampling around every tanh/waveshaping stage in the chain
    // (Preamp Character, Saturator, Limiter's extra clip stage) — pushes
    // the fold-back aliasing those nonlinearities generate up above
    // Nyquist before it can alias back down, audibly. Low-latency
    // polyphase-IIR halfband filters, so the added group delay is tiny.
    juce::dsp::Oversampling<float> osPreChar, osSat, osLimClip;

    // ---- 9) Doubler — up to 8 independent modulated delay voices, real
    // count driven by DblVoices (see DblVoiceConfig in Params.h) ----
    std::array<juce::dsp::DelayLine<float>, DblVoiceConfig::kMaxVoices> dblVoiceDelay;
    std::array<float, DblVoiceConfig::kMaxVoices> dblVoicePhase {};

    // ---- 10) Reverb, with its own pre-delay line + duck envelope ----
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> revPreDelayL, revPreDelayR;
    float revDuckEnv = 0.0f;
    // Wet-only tone shaping (post juce::dsp::Reverb, pre mix-back) — a
    // genuine user-facing filter pair, separate from the reverb's own
    // internal room-size/damping model.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> revWetHpf, revWetLpf;

    // Hybrid convolution engine — blended with the algorithmic reverb
    // above via RevHybrid (0 = pure algorithmic, 100 = pure loaded IR).
    // See loadImpulseResponseFile()'s comment above for the threading
    // model; runRev drains irTransfer (real-time safe, non-blocking)
    // every block before doing anything else.
    juce::dsp::Convolution revConvolution;
    juce::AudioBuffer<float> revConvBuffer;
    juce::AudioFormatManager irFormatManager;
    juce::File currentIrFile;
    bool irLoaded = false;

    struct IrBufferWithRate
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 0.0;
    };
    class IrBufferTransfer
    {
    public:
        void set(IrBufferWithRate&& p)
        {
            const juce::SpinLock::ScopedLockType lock(mutex);
            pending = std::move(p);
            hasNew = true;
        }
        template <typename Fn>
        void get(Fn&& fn)
        {
            const juce::SpinLock::ScopedTryLockType lock(mutex);
            if (lock.isLocked() && hasNew)
            {
                fn(pending);
                hasNew = false;
            }
        }
    private:
        IrBufferWithRate pending;
        bool hasNew = false;
        juce::SpinLock mutex;
    };
    IrBufferTransfer irTransfer;

    // ---- 11) Delay — ping-pong with spread, duck, and an auto-pan LFO ----
    juce::dsp::DelayLine<float> delayL, delayR;
    // Real tempo-synced pre-delay tap — a separate, smaller delay line ahead
    // of the feedback network (same idea as the Reverb's own pre-delay: it
    // only affects the wet path's timing, not host latency). See runDly.
    juce::dsp::DelayLine<float> dlyPreDelayL, dlyPreDelayR;
    float dlyDuckEnv = 0.0f;
    float dlyPanPhase = 0.0f;
    // Feedback-path-only filtering (mono per channel — this loop is already
    // sample-by-sample, so plain juce::dsp::IIR::Filter<float>::processSample
    // is simpler here than a block ProcessorDuplicator): each repeat that
    // recirculates through the delay line picks up a bit more HPF/LPF,
    // giving the classic analog/tape-echo "repeats get darker" character.
    juce::dsp::IIR::Filter<float> dlyFbHpfL, dlyFbHpfR, dlyFbLpfL, dlyFbLpfR;

    // ---- 12) Limiter — input trim, real look-ahead brickwall, extra clip ----
    // A genuine look-ahead limiter: incoming (post-input-gain) samples are
    // written into a ring buffer; the output is that same audio delayed by
    // limLookaheadSamples, with a gain envelope computed by scanning FORWARD
    // from each output sample through the lookahead window it's about to
    // reach — so gain reduction is already in place *before* a peak arrives,
    // not reacting after the fact like a plain fast limiter. The added
    // delay is reported to the host via setLatencySamples() for correct
    // automatic delay compensation.
    juce::AudioBuffer<float> limLookaheadRing;
    int limRingSize = 0, limRingMask = 0, limRingWritePos = 0, limLookaheadSamples = 0;
    float limGainSmoothed = 1.0f;

    // 4x-oversampled true-peak (inter-sample peak) detector on the final
    // output — a plain sample-peak reading can miss peaks that only appear
    // between samples once converted back to analogue; this catches those.
    juce::dsp::Oversampling<float> osTruePeak;
    juce::AudioBuffer<float> truePeakScratch;
    std::atomic<float> truePeakDbUI { -100.0f };

    // Pre-allocated scratch buffers, sized in prepareToPlay so processBlock()
    // never allocates on the audio thread.
    juce::AudioBuffer<float> dryBuffer;               // reused per dry/wet stage
    juce::AudioBuffer<float> revBuffer, dlyBuffer, dblBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaProcessor)
};
