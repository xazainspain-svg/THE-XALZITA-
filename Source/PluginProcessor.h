#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"

/**
    The XaLZa — the full 17-module vocal chain from the web mockup, plus
    the real-time modules added afterward:

      Preamp -> Gate -> Auto-Tune -> De-esser -> Transient Shaper -> Glue Comp ->
      Opto -> EQ 550 -> Resonance -> Saturator -> Exciter -> Doubler -> Reverb ->
      Spring Reverb -> Delay -> Limiter -> Tape Bus

    with Master In/Out gain, Stereo Width and Vintage Drift around the
    outside. Every parameter is a plain, direct APVTS parameter — no macro/
    intensity indirection layer.
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
    // No MIDI-driven parameters anymore (MIDI Learn was macro-only and has
    // been removed) — this is a plain audio effect.
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
        TapIn = 0, TapPre, TapGate, TapTune, TapEss, TapTrs, TapComp, TapOpto, TapEq, TapRes,
        TapSat, TapExc, TapDbl, TapRev, TapSpr, TapDly, TapLim, TapTape, TapOut, kNumMeterTaps
    };

    float getMeterDbL(int tap) const noexcept { return meterDbL[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }
    float getMeterDbR(int tap) const noexcept { return meterDbR[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }

    // Real RMS (mean-square) companion reading for the same tap — a
    // genuinely different measurement from the peak ballistics above, not
    // a derived/smoothed copy of it. See updateMeter().
    float getRmsDbL(int tap) const noexcept { return rmsDbL[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }
    float getRmsDbR(int tap) const noexcept { return rmsDbR[(size_t) juce::jlimit(0, kNumMeterTaps - 1, tap)].load(std::memory_order_relaxed); }

    // ---- Reorderable chain: which of the 17 modules processBlock() runs
    //      first/second/.../last. Identity order (Pre, Gate, ... Tape, same
    //      as MeterTap above) by default — matches every original meter/
    //      raw-tap wiring exactly until the user actually reorders
    //      something. Enum order here is deliberately identical to
    //      MeterTap's Pre..Tape run so tapForSlot() below is just an offset. ----
    enum ModuleSlot
    {
        SlotPre = 0, SlotGate, SlotTune, SlotEss, SlotTrs, SlotComp, SlotOpto, SlotEq, SlotRes,
        SlotSat, SlotExc, SlotDbl, SlotRev, SlotSpr, SlotDly, SlotLim, SlotTape, kNumSlots
    };
    // PSU "sag" emulation state, shared shape used by both Glue Comp
    // (CompSag) and Opto (OptoSag) — a "rail" state (1.0 = full supply
    // voltage) that dips toward a target derived from how much that
    // module's own compressor is currently reducing gain (fast down / much
    // slower up, asymmetric one-pole coefficients), producing its own extra
    // gain reduction on top of the module's normal ratio/release. See
    // sagComputeGain() in the .cpp — offline-verified in Python
    // (transparent at amt=0, bounded, genuine slow-recovery "breathing"
    // signature after a loud hit). Public so the free helper function in
    // PluginProcessor.cpp's anonymous namespace can take it by reference.
    struct SagState
    {
        float envDry = 0.0f, envWet = 0.0f, rail = 1.0f;
        void reset() { envDry = 0.0f; envWet = 0.0f; rail = 1.0f; }
    };

    static int tapForSlot(int slotId) noexcept { return (int) TapPre + juce::jlimit(0, kNumSlots - 1, slotId); }
    static const char* slotName(int slotId) noexcept
    {
        static const char* names[kNumSlots] = { "PREAMP", "GATE", "AUTO-TUNE", "DE-ESSER", "TRANSIENT SHAPER", "GLUE COMP", "OPTO",
                                                  "EQ 550", "RESONANCE", "SATURATOR", "EXCITER", "DOUBLER",
                                                  "REVERB", "SPRING REVERB", "DELAY", "LIMITER", "TAPE BUS" };
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

    // Drag-and-drop reorder: moves slotId directly to an arbitrary target
    // position in one go (a real array move — everything between the old
    // and new position shifts down/up by one), rather than the single-step
    // neighbour swap moveModule() above does. Same UI-driven-only,
    // real-time-safe contract (plain atomic stores) — safe to call on
    // every intermediate position while a drag is in progress, so the
    // live chain genuinely follows the drag instead of only updating on
    // drop.
    void moveModuleTo(int slotId, int targetPos) noexcept
    {
        targetPos = juce::jlimit(0, kNumSlots - 1, targetPos);
        int pos = getChainPosition(slotId);
        if (pos == targetPos)
            return;

        int order[kNumSlots];
        for (int i = 0; i < kNumSlots; ++i)
            order[i] = chainOrder[(size_t) i].load(std::memory_order_relaxed);

        // Remove slotId from its current position, then reinsert it at
        // targetPos, sliding the intervening entries over by one.
        if (pos < targetPos)
        {
            for (int i = pos; i < targetPos; ++i)
                order[i] = order[i + 1];
        }
        else
        {
            for (int i = pos; i > targetPos; --i)
                order[i] = order[i - 1];
        }
        order[targetPos] = slotId;

        for (int i = 0; i < kNumSlots; ++i)
            chainOrder[(size_t) i].store(order[i], std::memory_order_relaxed);
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

    // Same idea, but tapped after Master Out Gain — the true final mix —
    // for the overview page's whole-plugin spectrum analyser.
    float specSampleMaster(int i) const noexcept { return specRingMaster[(size_t) (i & (kSpecSize - 1))].load(std::memory_order_relaxed); }
    int   getSpecWritePosMaster() const noexcept { return specWritePosMaster.load(std::memory_order_relaxed); }

    // Generic raw-sample taps for the remaining per-module visualisers
    // (oscilloscopes / harmonic bars). Each is genuinely POST that module's
    // own processing — never the module's input — so every visualiser shows
    // what that stage actually did to the signal.
    enum RawTap { RawPre = 0, RawGate, RawEss, RawSatIn, RawSatOut, RawOpto, RawDly, RawLim, RawRes, RawTune, RawTape, RawExc, kNumRawTaps };
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

    // Auto-Tune's real detected pitch (0 = unvoiced/no confident pitch this
    // analysis hop) and the real corrected target it's being pulled toward
    // — both genuinely measured/computed in runTune, read by TuneView so
    // its trace shows exactly what the detector and corrector are doing,
    // not a decorative animation.
    float getTuneDetectedHz() const noexcept { return tuneDetectedHzUI.load(std::memory_order_relaxed); }
    float getTuneTargetHz() const noexcept { return tuneTargetHzUI.load(std::memory_order_relaxed); }

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

    // ---- Real telemetry for the analog-character features' own
    // visualisers (Spring Reverb cluster, Tape Bus scope, Iron/Sag
    // meters) — all genuine per-block measurements from the audio thread,
    // same "real data, not decoration" convention as everything above.
    static constexpr int kNumSprCombsUI = 6;
    // Per-comb output level (linear 0..~1, real per-block peak) and LFO
    // phase (radians, real running phase) for the Spring Reverb cluster
    // view — see runSpr.
    float getSprCombLevel(int comb) const noexcept
    {
        return sprCombLevelUI[(size_t) juce::jlimit(0, kNumSprCombsUI - 1, comb)].load(std::memory_order_relaxed);
    }
    float getSprCombPhase(int comb) const noexcept
    {
        return sprCombPhaseUI[(size_t) juce::jlimit(0, kNumSprCombsUI - 1, comb)].load(std::memory_order_relaxed);
    }
    // Tape Bus's real per-block peak wow/flutter deviation (ms) and the
    // real gain reduction (dB) its Iron/drive stage is currently applying
    // — see runTape.
    float getTapeWowDeviationMs() const noexcept { return tapeWowDeviationMsUI.load(std::memory_order_relaxed); }
    float getTapeIronGrDb() const noexcept { return tapeIronGrDbUI.load(std::memory_order_relaxed); }
    // Preamp's Iron stage real gain reduction (dB) — see runPre.
    float getPreIronGrDb() const noexcept { return preIronGrDbUI.load(std::memory_order_relaxed); }
    // PSU Sag's real extra gain reduction (dB, 0..sagDepthMaxDb) for Comp
    // and Opto independently — see sagComputeGain()'s doc comment and
    // runComp/runOpto.
    float getCompSagDb() const noexcept { return compSagDbUI.load(std::memory_order_relaxed); }
    float getOptoSagDb() const noexcept { return optoSagDbUI.load(std::memory_order_relaxed); }

    // Transient Shaper's real detector reading — the actual fast/slow
    // envelope-ratio (dB, ±capDb) driving TrsAttack/TrsSustain, and the
    // real per-block gain (dB) that detector produced — see runTrs.
    float getTrsEnvDiffDb() const noexcept { return trsEnvDiffDbUI.load(std::memory_order_relaxed); }
    float getTrsGainDb() const noexcept { return trsGainDbUI.load(std::memory_order_relaxed); }

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

    juce::String getVersionString() const { return JucePlugin_VersionString; }

private:
    void updateMeter(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);
    void updateGr(int moduleIdx, float preDb, float postDb);
    void pushRaw(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh);
    static void applySmoothedGainDb(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& smoother,
                                     juce::AudioBuffer<float>& buf, float targetDb, int numSamples);

    std::array<std::atomic<float>, kSpecSize> specRing;
    std::atomic<int> specWritePos { 0 };
    std::array<std::atomic<float>, kSpecSize> specRingMaster;
    std::atomic<int> specWritePosMaster { 0 };

    std::array<std::array<std::atomic<float>, kRawSize>, kNumRawTaps> rawRing;
    std::array<std::atomic<int>, kNumRawTaps> rawWritePos;
    std::atomic<float> lastHpfHz { 20.0f };

    std::atomic<float> gateGrDbUI { 0.0f };
    std::atomic<float> essBandDbUI { -100.0f };
    std::atomic<float> essReductionDbUI { 0.0f };
    std::atomic<float> resCutDbUI { 0.0f };
    std::atomic<float> dlyPanPhaseUI { 0.0f };

    std::array<std::atomic<float>, kNumSprCombsUI> sprCombLevelUI, sprCombPhaseUI;
    std::atomic<float> tapeWowDeviationMsUI { 0.0f };
    std::atomic<float> tapeIronGrDbUI { 0.0f };
    std::atomic<float> preIronGrDbUI { 0.0f };
    std::atomic<float> compSagDbUI { 0.0f };
    std::atomic<float> optoSagDbUI { 0.0f };
    std::atomic<float> trsEnvDiffDbUI { 0.0f };
    std::atomic<float> trsGainDbUI { 0.0f };

    std::array<std::atomic<float>, kScopeSize> dblScopeL, dblScopeR;
    std::atomic<int> dblScopeWritePos { 0 };

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
    // "Iron" — transformer-style low-biased saturation (PreIron). One-pole
    // ~300Hz low-band split, one-pole state per channel (see
    // ironSaturateSample() in the .cpp — the SAME helper Tape Bus's Drive
    // reuses below, both offline-verified in Python: normalized by drive
    // itself so tanh(z)/z <= 1 for all z, i.e. this stage can only ever
    // SATURATE/compress the low band, never boost it above its own
    // instantaneous amplitude — a first design that normalized by
    // tanh(drive) instead was found to overshoot input amplitude by up to
    // +0.62 in a sweep test, before any C++ was written).
    float preIronLpL = 0.0f, preIronLpR = 0.0f;

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

    // ---- 2b) Auto-Tune: a real autocorrelation pitch detector (decimated,
    // hop-based — full-rate-every-sample autocorrelation is far too costly
    // for the audio thread) driving a granular pitch shifter (2 overlapping
    // grains reading a live history buffer at a modulated rate — the
    // classic "two-tap variable-speed delay line" pitch-shift technique).
    // Key/Scale pick the target note set; Retune Speed sets how fast the
    // shift ratio slews toward the target (0 = instant/robotic hard-tune);
    // Amount blends the correction ratio between 1.0 (no shift) and full
    // correction, so there is only ever ONE processed signal path — never
    // a dry+shifted blend, which would phase/flange since they aren't
    // phase-aligned. See runTune. Formant correction is NOT implemented
    // (a real simplification, not hidden) — strong shifts will "chipmunk"/
    // "Darth Vader" a little, same trade-off as most simple real-time
    // pitch shifters; this is most audible (and most sought-after, for
    // urban/trap-style hard-tune) at fast Retune + high Amount.
    struct GranularPitchShifter
    {
        static constexpr int kNumGrains = 2;
        std::vector<float> buf;
        int bufLen = 0;
        int writePos = 0;
        float grainLenSamples = 2400.0f;
        std::array<float, kNumGrains> grainDist {};

        void prepare(double sampleRate)
        {
            grainLenSamples = (float) (0.05 * sampleRate);   // 50ms grains
            bufLen = juce::jmax(256, (int) (0.25 * sampleRate));   // ample margin over grainLenSamples at any sample rate
            buf.assign((size_t) bufLen, 0.0f);
            writePos = 0;
            for (int i = 0; i < kNumGrains; ++i)
                grainDist[(size_t) i] = (float) i * grainLenSamples / (float) kNumGrains;
        }
        void reset()
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            writePos = 0;
        }
        // pitchRatio: desired playback speed through recorded history
        // (>1 = pitch up, <1 = pitch down, 1 = transparent passthrough).
        // Each grain's "distance behind the write pointer" changes at rate
        // (1 - pitchRatio) per sample — derived from: read position moves
        // at rate pitchRatio, write position moves at rate 1, so the gap
        // between them changes at (1 - pitchRatio); at pitchRatio==1 the
        // gap never changes, so a grain simply becomes a fixed delay tap
        // (bit-exact silent passthrough character, no windowing loss).
        // Verified offline (Python) against known test tones before this
        // was written in C++: frequency error stayed within a few cents
        // for +-1 octave shifts.
        float processSample(float x, float pitchRatio) noexcept
        {
            if (bufLen == 0)
                return x;
            buf[(size_t) writePos] = x;
            float delta = 1.0f - pitchRatio;
            float acc = 0.0f, sumW = 0.0f;
            for (int i = 0; i < kNumGrains; ++i)
            {
                float readPosF = (float) writePos - grainDist[(size_t) i];
                while (readPosF < 0.0f) readPosF += (float) bufLen;
                int i0 = (int) readPosF;
                float frac = readPosF - (float) i0;
                int i0m = i0 % bufLen;
                int i1 = (i0m + 1) % bufLen;
                float s = buf[(size_t) i0m] + frac * (buf[(size_t) i1] - buf[(size_t) i0m]);
                float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                              * (grainDist[(size_t) i] / grainLenSamples));
                acc += s * w;
                sumW += w;
                grainDist[(size_t) i] += delta;
                if (grainDist[(size_t) i] >= grainLenSamples)      grainDist[(size_t) i] -= grainLenSamples;
                else if (grainDist[(size_t) i] < 0.0f)             grainDist[(size_t) i] += grainLenSamples;
            }
            writePos = (writePos + 1) % bufLen;
            return sumW > 0.0001f ? acc / sumW : x;
        }
    };
    GranularPitchShifter tuneShifterL, tuneShifterR;

    // Pitch detector state — mono-summed (L+R) analysis, autocorrelation
    // run once per hop on a decimated window (cheap enough for the audio
    // thread; see runTune). tuneDetectedHz/tuneSmoothedRatio are the audio-
    // thread's own working state; the *UI atomics below are what TuneView
    // actually reads.
    static constexpr int kTuneWindow   = 4096;   // real-rate analysis window, samples
    static constexpr int kTuneDecimate = 4;
    static constexpr int kTuneHop      = 512;
    std::array<float, kTuneWindow> tuneAnalysisBuf {};
    int   tuneAnalysisWritePos = 0;
    int   tuneHopCounter = 0;
    float tuneDetectedHz = 0.0f;      // 0 = unvoiced / no confident pitch this hop
    float tuneSmoothedRatio = 1.0f;   // one-pole toward the target ratio, rate set by Retune Speed
    std::atomic<float> tuneDetectedHzUI { 0.0f };
    std::atomic<float> tuneTargetHzUI { 0.0f };
    // Decimated autocorrelation + parabolic refinement over the last
    // kTuneWindow samples of tuneAnalysisBuf; returns 0.0f when unvoiced/
    // not confident. Defined in PluginProcessor.cpp, called once per
    // kTuneHop samples from runTune.
    float detectTunePitchHz(double sr) noexcept;

    // ---- Auto-Tune formant preservation (optional, TuneFormant param) ----
    // LPC-based "whiten -> shift residual -> re-colour" path, verified
    // offline (Python) before being written here: extracts a per-hop
    // spectral envelope (Levinson-Durbin LPC, order kOrder) from the same
    // mono analysis window already used for pitch detection, flattens
    // ("whitens") the signal through that envelope's inverse (a stable
    // FIR — the analysis filter A(z)), pitch-shifts the whitened residual
    // with the existing GranularPitchShifter, then re-applies the
    // ORIGINAL (un-shifted) envelope on the way out (the recursive
    // synthesis filter 1/A(z)) — so the harmonic content moves but the
    // resonant "shape of the mouth" stays put, avoiding the chipmunk/
    // Vader character a plain shift has on large corrections. Three
    // details below fixed real instabilities found during offline
    // verification, not just correctness bugs:
    //   - pre-emphasis before LPC estimation (and de-emphasis after
    //     synthesis) — without it a low-pitched, harmonic-rich voice
    //     reliably produces a spurious near-DC resonance in the synthesis
    //     filter (a modeling artifact of the vocal spectral tilt, not a
    //     real formant) that rang up to a large output spike in testing;
    //   - bandwidth expansion (a[k] *= gamma^k) — damps any pole sitting
    //     close to the unit circle so a hop-boundary discontinuity decays
    //     quickly instead of ringing;
    //   - per-sample linear interpolation of the coefficients across each
    //     hop instead of switching once per hop — the whitening (FIR)
    //     side is stable either way, but the recursive re-colouring side
    //     is not: swapping its coefficients abruptly while its own delay
    //     line still holds samples computed under the OLD coefficients is
    //     a state/coefficient mismatch that can excite a resonance hard.
    // A final per-sample soft clip on the synthesised output is a last-
    // resort safety net (an extreme/clipped input could still
    // occasionally produce a large transient even with all of the above
    // in offline testing) — inactive at normal levels. See runTune / the
    // .cpp for the full body.
    struct FormantEnvelope
    {
        static constexpr int kOrder = 24;
        std::array<float, kOrder + 1> aPrev {}, aNew {};
        // How many real (post-reset) samples have been fed into
        // tuneAnalysisBuf so far, capped at kFormantWindow. Below that cap,
        // the analysis window still has left-over zero padding in it (see
        // the priming discussion above), so analyseFormantEnvelope() holds
        // identity coefficients until this reaches kFormantWindow.
        int primeCount = 0;

        void reset() noexcept
        {
            aPrev.fill(0.0f); aPrev[0] = 1.0f;
            aNew.fill(0.0f);  aNew[0]  = 1.0f;
            primeCount = 0;
        }
    };
    struct FormantChannelState
    {
        static constexpr int kOrder = FormantEnvelope::kOrder;
        std::array<float, kOrder> xHist {}, yHist {};
        float preState = 0.0f, deState = 0.0f;
        void reset() noexcept { xHist.fill(0.0f); yHist.fill(0.0f); preState = 0.0f; deState = 0.0f; }
    };
    FormantEnvelope tuneFormantEnv;
    FormantChannelState tuneFormantL, tuneFormantR;
    static constexpr int kFormantWindow = kTuneWindow;   // reuse the pitch-detector's own analysis window
    // Recomputes tuneFormantEnv.aNew from the last kFormantWindow samples
    // of tuneAnalysisBuf (the SAME mono analysis buffer runTune already
    // fills for pitch detection); called once per kTuneHop, right
    // alongside detectTunePitchHz. Defined in PluginProcessor.cpp.
    void analyseFormantEnvelope() noexcept;
    // One sample of the whiten/shift/re-colour chain above, for one
    // channel; frac is this sample's position (0..1) through the current
    // hop, used to linearly interpolate aPrev->aNew. Defined in
    // PluginProcessor.cpp.
    float processFormantPreservedSample(FormantChannelState& st, GranularPitchShifter& shifter,
                                         float x, float frac, float pitchRatio) noexcept;

    // ---- 3) De-esser: dynamic peak filter driven by a sibilance-band ----
    juce::dsp::IIR::Filter<float> essDetectL, essDetectR;   // band detector (not applied to main signal)
    float essEnv = 0.0f;
    float essGainDb = 0.0f;  // smoothed current attenuation (negative dB)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> essDynEq;

    // ---- 3b) Transient Shaper: dual envelope-follower attack/sustain
    // reshaping. A "fast" detector (quick attack AND quick release) hugs
    // the true instantaneous envelope; a "slow" detector (slow attack AND
    // slow release) lags on the way up (so a sharp hit reads fast >> slow
    // = attack) and lags on the way down too (so a decaying tail reads
    // fast << slow = sustain/body) — distinct release times are what
    // makes the sustain side respond at all; equal releases would make
    // the two track identically during any decay. Linked across L/R (one
    // pair of detectors fed by max(|L|,|R|), one gain applied to both
    // channels) so stereo balance isn't disturbed. Verified offline
    // (Python) before being written here — see runTrs.
    struct EnvFollower
    {
        float env = 0.0f;
        float attackCoef = 0.0f, releaseCoef = 0.0f;
        void setTimes(float attackMs, float releaseMs, double sampleRate) noexcept
        {
            attackCoef  = attackMs  <= 0.0f ? 0.0f : std::exp(-1.0f / (float) (sampleRate * attackMs  * 0.001));
            releaseCoef = releaseMs <= 0.0f ? 0.0f : std::exp(-1.0f / (float) (sampleRate * releaseMs * 0.001));
        }
        float process(float absX) noexcept
        {
            float c = absX > env ? attackCoef : releaseCoef;
            env = c * env + (1.0f - c) * absX;
            return env;
        }
        void reset() noexcept { env = 0.0f; }
    };
    EnvFollower trsFast, trsSlow;
    float trsGainSmoothed = 1.0f;   // linear, one-pole smoothed to avoid zipper artifacts on the applied gain

    // ---- 4) Glue Comp ----
    juce::dsp::Compressor<float> compressor;
    SagState compSagState;

    // ---- 5) Opto (slow, program-dependent 2nd compressor stage) ----
    juce::dsp::Compressor<float> optoComp;
    SagState optoSagState;

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

    // ---- 8b) Exciter: harmonic enhancer. Isolates the band above a
    // Tone-controlled crossover (ExcHpf), drives ONLY that band through an
    // asymmetric soft clip (tanh of a signal plus a small squared term, so
    // it generates both even and odd harmonics rather than just odd like a
    // symmetric clipper), and mixes the result back on top of the dry
    // signal — it never replaces the dry path, so Drive=0/Mix=0 is
    // bit-identical passthrough. Deliberately has NO makeup-gain
    // restoration back to unity (unlike the Saturator, which is a
    // full-signal-path effect that must preserve overall level): this is
    // an add-in "spice" signal, and tanh's own natural compression is what
    // keeps its contribution bounded — verified offline (Python) that
    // adding makeup gain here nearly doubled peak output at Drive=100/
    // Mix=100, while leaving it off kept output within ~0.01 of input
    // peak at those same extreme settings. See runExc.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> excHpf;

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

    // Input diffuser — 4 nested Schroeder allpass stages per channel, run
    // on the pre-delayed dry signal just before it enters the algorithmic
    // engine. This is the same "one delay line per stage" allpass structure
    // used ahead of almost every real algorithmic/plate reverb (Dattorro,
    // Griesinger) to break a transient into a dense diffuse cloud in a few
    // milliseconds — without it, a Freeverb-style comb/allpass network on
    // its own tends to sound "boingy"/metallic on percussive input. Always
    // on, no user parameter — pure sound-quality improvement, mirrors
    // exactly between L/R (same stage lengths/gain) so it never smears the
    // stereo image on its own.
    struct AllpassDiffuser
    {
        static constexpr int kStages = 4;
        // Mutually-prime-ish stage lengths (ms) — classic Schroeder/Dattorro
        // diffuser values, short enough to stay inaudible as discrete echoes.
        static constexpr float stageMs[kStages] = { 4.7f, 3.1f, 6.3f, 2.3f };
        static constexpr float g = 0.5f;   // conservative — enough density, no audible ringing

        std::array<juce::dsp::DelayLine<float>, kStages> lines;

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            for (int i = 0; i < kStages; ++i)
            {
                auto& l = lines[(size_t) i];
                l.prepare(spec);
                l.setMaximumDelayInSamples((int) (stageMs[(size_t) i] * 0.001 * spec.sampleRate) + 8);
                l.setDelay(stageMs[(size_t) i] * 0.001f * (float) spec.sampleRate);
            }
        }
        void reset() { for (auto& l : lines) l.reset(); }
        float processSample(float x) noexcept
        {
            for (auto& l : lines)
            {
                float wD = l.popSample(0);
                float w  = x + g * wD;
                x = -g * w + wD;
                l.pushSample(0, w);
            }
            return x;
        }
    };
    AllpassDiffuser revDiffuserL, revDiffuserR;
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

    // ---- 10b) Spring Reverb — a second, physically distinct reverb: a bank
    // of short, damped, independently-LFO-modulated comb filters (mimicking
    // discrete physical spring lengths — the LFO on each comb's delay time
    // gives the characteristic dispersive "chirp"/pitch-sweep spring reverbs
    // are known for), run identically (same tuning) but fully independently
    // per channel. Offline-verified in Python: finite/bounded at every
    // Decay setting including the top of the range (highest feedback = the
    // highest runaway risk), feedback deliberately capped well under 1.0.
    // See runSpr.
    struct DampedModComb
    {
        juce::dsp::DelayLine<float> line;   // default interpolation is Linear, matching the Python model
        float delayMs = 0.0f, lfoHz = 0.0f, lfoDepthMs = 0.0f;
        float lfoPhase = 0.0f, damp = 0.0f;

        void prepare(const juce::dsp::ProcessSpec& spec, float delayMsIn, float lfoHzIn, float lfoDepthMsIn)
        {
            delayMs = delayMsIn; lfoHz = lfoHzIn; lfoDepthMs = lfoDepthMsIn;
            line.prepare(spec);
            line.setMaximumDelayInSamples((int) ((delayMs + lfoDepthMs + 2.0f) * 0.001 * spec.sampleRate) + 8);
            line.reset();
            lfoPhase = 0.0f; damp = 0.0f;
        }
        void reset() { line.reset(); lfoPhase = 0.0f; damp = 0.0f; }
        float processSample(float x, float feedback, float dampCoef, double sampleRate) noexcept
        {
            float lfo = std::sin(lfoPhase);
            lfoPhase += 2.0f * juce::MathConstants<float>::pi * lfoHz / (float) sampleRate;
            if (lfoPhase > juce::MathConstants<float>::pi * 2.0f)
                lfoPhase -= juce::MathConstants<float>::pi * 2.0f;
            float d = juce::jmax(0.0f, (delayMs + lfoDepthMs * lfo) * 0.001f * (float) sampleRate);
            line.setDelay(d);
            float s = line.popSample(0);
            damp = dampCoef * damp + (1.0f - dampCoef) * s;
            float out = damp;
            line.pushSample(0, x + out * feedback);
            return out;
        }
    };
    static constexpr int kNumSprCombs = 6;
    // Short, mutually-prime-ish, non-integer-ratio delay times (ms) —
    // mimics discrete physical spring lengths; each comb's own independent
    // LFO rate gives every spring its own "chirp" speed.
    static constexpr float sprDelayMs[kNumSprCombs] = { 17.3f, 23.7f, 29.1f, 34.9f, 41.3f, 13.7f };
    static constexpr float sprLfoHz[kNumSprCombs]   = { 0.31f, 0.47f, 0.53f, 0.61f, 0.67f, 0.71f };
    std::array<DampedModComb, kNumSprCombs> sprCombL, sprCombR;

    // ---- 11) Delay — ping-pong with spread, duck, and an auto-pan LFO ----
    juce::dsp::DelayLine<float> delayL, delayR;
    // Real tempo-synced pre-delay tap — a separate, smaller delay line ahead
    // of the feedback network (same idea as the Reverb's own pre-delay: it
    // only affects the wet path's timing, not host latency). See runDly.
    juce::dsp::DelayLine<float> dlyPreDelayL, dlyPreDelayR;
    float dlyDuckEnv = 0.0f;
    float dlyPanPhase = 0.0f;
    // Wow phase — only advanced/audible once Drive > 0 (see runDly); a real
    // tape unit's pitch wobble and its saturation are physically linked, so
    // Drive controls both together instead of needing a second knob.
    float dlyWowPhase = 0.0f;
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

    // ---- 13) Tape Bus — the final stage of the chain: transformer-style
    // low-biased saturation (TapeDrive, reusing ironSaturateSample() — see
    // the Preamp Iron comment above) combined with real wow & flutter
    // (TapeWow — a small modulated delay line, the SAME primitive verified
    // for Spring Reverb's LFO-modulated combs above, just with much
    // slower/smaller depth constants: a summed 0.6Hz "wow" + 6.5Hz
    // "flutter" sine pair). Both default to 0 = bit-identical passthrough.
    // See runTape.
    juce::dsp::DelayLine<float> tapeWowL, tapeWowR;
    // Both phases shared across channels (drive the same delay-time target
    // for L and R identically) — both channels physically ride the same
    // tape transport, so wow/flutter affects them together, not
    // independently.
    float tapeWowPhase = 0.0f, tapeFlutterPhase = 0.0f;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> tapeToneFilter;
    float tapeIronLpL = 0.0f, tapeIronLpR = 0.0f;

    // ---- Master "Vintage Drift" — applied at the very end of the signal
    // path: (a) a tiny gain wobble from a bounded, mean-reverting random
    // walk (always hard-clamped, so it is safe by construction regardless
    // of the random sequence) and (b) a very slow/shallow pitch wobble
    // reusing the same bounded modulated-delay primitive as Tape Bus's wow
    // (two slow, mutually-prime-ish sine rates summed, an order of
    // magnitude slower/shallower than Tape Bus's own wow/flutter). 0 =
    // fully transparent. See the master-stage drift block in
    // processBlock().
    juce::dsp::DelayLine<float> driftDelayL, driftDelayR;
    float driftPhase1 = 0.0f, driftPhase2 = 0.0f;   // two slow, mutually-prime-ish rates, summed, for a non-periodic-feeling wander
    float driftGainWalk = 0.0f;   // bounded random walk, hard-clamped every update — see runMasterDrift
    juce::Random driftRng;

    // Pre-allocated scratch buffers, sized in prepareToPlay so processBlock()
    // never allocates on the audio thread.
    juce::AudioBuffer<float> dryBuffer;               // reused per dry/wet stage
    juce::AudioBuffer<float> revBuffer, dlyBuffer, dblBuffer;
    // Exciter's own added-harmonics signal (post-drive, PRE dry/wet mix) —
    // real telemetry for ExciterHarmonicsView's spectrum, not the module's
    // final output. See runExc / RawExc.
    juce::AudioBuffer<float> excHarmBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaProcessor)
};
