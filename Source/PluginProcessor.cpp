#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float onePoleCoef(float timeMs, double sr)
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.01f, timeMs) * (float) sr));
    }
}

XaLZaProcessor::XaLZaProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          // Optional external key input for the Gate (XID::GateScEnable) —
          // disabled by default, so a fresh instance behaves exactly as
          // before unless the user both enables it AND the host routes
          // something into it.
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "PARAMS", createXaLZaParameterLayout()),
      macroTracker(apvts),
      osPreChar(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osSat(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osLimClip(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osTruePeak(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR)   // factor 2^2 = 4x
{
    for (auto& a : meterDbL) a.store(-100.0f);
    for (auto& a : meterDbR) a.store(-100.0f);
    for (auto& a : grDb) a.store(0.0f);
    for (auto& a : scopePointsL) a.store(0.0f);
    for (auto& a : scopePointsR) a.store(0.0f);
    for (auto& a : specRing) a.store(0.0f);
    for (auto& ring : rawRing) for (auto& a : ring) a.store(0.0f);
    for (auto& a : rawWritePos) a.store(0);
    for (auto& a : dblScopeL) a.store(0.0f);
    for (auto& a : dblScopeR) a.store(0.0f);
    for (auto& a : macroCcMap) a.store(-1);
    for (int i = 0; i < kNumSlots; ++i) chainOrder[(size_t) i].store(i);
}

void XaLZaProcessor::updateMeter(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh)
{
    float peakL = 0.0f, peakR = 0.0f;
    auto* l = buf.getReadPointer(0);
    for (int n = 0; n < numSamples; ++n)
        peakL = juce::jmax(peakL, std::abs(l[n]));
    if (numCh > 1)
    {
        auto* r = buf.getReadPointer(1);
        for (int n = 0; n < numSamples; ++n)
            peakR = juce::jmax(peakR, std::abs(r[n]));
    }
    else
    {
        peakR = peakL;
    }

    float dbL = juce::Decibels::gainToDecibels(peakL, -100.0f);
    float dbR = juce::Decibels::gainToDecibels(peakR, -100.0f);

    auto smooth = [this] (std::atomic<float>& state, float target)
    {
        float prev = state.load(std::memory_order_relaxed);
        float coef = target > prev ? meterAttCoef : meterRelCoef;
        state.store(coef * prev + (1.0f - coef) * target, std::memory_order_relaxed);
    };
    smooth(meterDbL[(size_t) tap], dbL);
    smooth(meterDbR[(size_t) tap], dbR);
}

void XaLZaProcessor::updateGr(int moduleIdx, float preDb, float postDb)
{
    float target = juce::jlimit(0.0f, 24.0f, preDb - postDb);
    float prev = grDb[(size_t) moduleIdx].load(std::memory_order_relaxed);
    float coef = target > prev ? meterAttCoef : meterRelCoef;
    grDb[(size_t) moduleIdx].store(coef * prev + (1.0f - coef) * target, std::memory_order_relaxed);
}

void XaLZaProcessor::applySmoothedGainDb(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& smoother,
                                          juce::AudioBuffer<float>& buf, float targetDb, int numSamples)
{
    smoother.setTargetValue(juce::Decibels::decibelsToGain(targetDb));
    float g0 = smoother.getCurrentValue();
    float g1 = smoother.skip(numSamples);
    buf.applyGainRamp(0, numSamples, g0, g1);
}

void XaLZaProcessor::pushRaw(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh)
{
    auto t = (size_t) juce::jlimit(0, kNumRawTaps - 1, tap);
    auto* l = buf.getReadPointer(0);
    auto* r = numCh > 1 ? buf.getReadPointer(1) : l;
    for (int n = 0; n < numSamples; ++n)
    {
        int pos = rawWritePos[t].load(std::memory_order_relaxed);
        rawRing[t][(size_t) (pos & (kRawSize - 1))].store(0.5f * (l[n] + r[n]), std::memory_order_relaxed);
        rawWritePos[t].store(pos + 1, std::memory_order_relaxed);
    }
}

bool XaLZaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // The optional Sidechain bus (input bus 1, Gate's external key) must be
    // either fully disabled or stereo — nothing else.
    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.inputBuses[1];
        if (sc != juce::AudioChannelSet::disabled() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void XaLZaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    preHpf.prepare(spec);
    preImpShelf.prepare(spec);
    essDynEq.prepare(spec);
    compressor.prepare(spec);
    optoComp.prepare(spec);
    eqLowShelf.prepare(spec);
    eqMidPeak.prepare(spec);
    eqHighShelf.prepare(spec);
    resNotch.prepare(spec);
    satTone.prepare(spec);
    reverb.prepare(spec);
    revWetHpf.prepare(spec);
    revWetLpf.prepare(spec);

    essDetectL.prepare(spec);
    essDetectR.prepare(spec);
    resDetectL.prepare(spec);
    resDetectR.prepare(spec);
    resEnv = 0.0f;
    resCutSmoothed = 0.0f;

    for (auto* s : { &masterInSmoothed, &masterOutSmoothed, &preGainSmoothed,
                      &compMakeupSmoothed, &optoGainSmoothed, &limInGainSmoothed, &prePadGainSmoothed })
        s->reset(sampleRate, 0.02);   // ~20ms ramp — kills zipper noise, still feels instant

    osPreChar.initProcessing((size_t) samplesPerBlock);
    osSat.initProcessing((size_t) samplesPerBlock);
    osLimClip.initProcessing((size_t) samplesPerBlock);
    osTruePeak.initProcessing((size_t) samplesPerBlock);
    osPreChar.reset();
    osSat.reset();
    osLimClip.reset();
    osTruePeak.reset();
    truePeakScratch.setSize(2, samplesPerBlock, false, false, true);

    // Look-ahead limiter ring: fixed 5ms look-ahead, ring sized generously
    // (look-ahead window + a full block + margin, rounded up to a power of
    // two so index wraparound is a cheap bitmask).
    limLookaheadSamples = juce::jmax(1, (int) std::round(0.005 * sampleRate));
    limRingSize = (int) juce::nextPowerOfTwo(limLookaheadSamples + samplesPerBlock + 64);
    limRingMask = limRingSize - 1;
    limLookaheadRing.setSize(2, limRingSize, false, true, true);
    limRingWritePos = 0;
    limGainSmoothed = 1.0f;

    setLatencySamples((int) std::round(osPreChar.getLatencyInSamples()
                                        + osSat.getLatencyInSamples()
                                        + osLimClip.getLatencyInSamples())
                       + limLookaheadSamples);
    masterInSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));
    masterOutSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));
    preGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));
    compMakeupSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));
    optoGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));
    limInGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(0.0f));

    // Simplified ITU-R BS.1770 K-weighting chain for the Limiter page's real
    // LUFS readout: stage 1 is a +4dB high-shelf @ 1500Hz, stage 2 is a
    // ~38Hz high-pass (the "RLB" curve) — same two stages the spec uses,
    // integrated with a fast one-pole rather than the spec's exact 400ms
    // rectangular gate (this is a real-time meter, not a compliance tool).
    juce::dsp::ProcessSpec monoLufsSpec = spec; monoLufsSpec.numChannels = 1;
    lufsPreL.prepare(monoLufsSpec); lufsPreR.prepare(monoLufsSpec);
    lufsRlbL.prepare(monoLufsSpec); lufsRlbR.prepare(monoLufsSpec);
    *lufsPreL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(4.0f));
    *lufsPreR.coefficients = *lufsPreL.coefficients;
    *lufsRlbL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.0f, 0.5f);
    *lufsRlbR.coefficients = *lufsRlbL.coefficients;
    lufsPreL.reset(); lufsPreR.reset(); lufsRlbL.reset(); lufsRlbR.reset();
    lufsMsL = lufsMsR = 0.0f;

    compressor.setAttack(12.0f);
    compressor.setRelease(250.0f);
    optoComp.setAttack(30.0f);
    optoComp.setRelease(450.0f);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    dblDelayL.prepare(monoSpec);
    dblDelayR.prepare(monoSpec);
    dblDelayL.setMaximumDelayInSamples((int) (sampleRate * 0.5));
    dblDelayR.setMaximumDelayInSamples((int) (sampleRate * 0.5));
    dblDelayL.reset();
    dblDelayR.reset();

    revPreDelayL.prepare(monoSpec);
    revPreDelayR.prepare(monoSpec);
    revPreDelayL.setMaximumDelayInSamples((int) (sampleRate * 0.2));
    revPreDelayR.setMaximumDelayInSamples((int) (sampleRate * 0.2));
    revPreDelayL.reset();
    revPreDelayR.reset();

    delayL.prepare(monoSpec);
    delayR.prepare(monoSpec);
    delayL.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayR.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayL.reset();
    delayR.reset();

    dlyFbHpfL.prepare(monoSpec); dlyFbHpfR.prepare(monoSpec);
    dlyFbLpfL.prepare(monoSpec); dlyFbLpfR.prepare(monoSpec);
    dlyFbHpfL.reset(); dlyFbHpfR.reset(); dlyFbLpfL.reset(); dlyFbLpfR.reset();

    dryBuffer.setSize(2, samplesPerBlock, false, false, true);
    revBuffer.setSize(2, samplesPerBlock, false, false, true);
    dlyBuffer.setSize(2, samplesPerBlock, false, false, true);
    dblBuffer.setSize(2, samplesPerBlock, false, false, true);

    gateEnv = 0.0f;
    gateGain = 1.0f;
    gateHoldCounter = 0;
    essEnv = 0.0f;
    essGainDb = 0.0f;
    revDuckEnv = 0.0f;
    dlyDuckEnv = 0.0f;
    dblPhase1 = dblPhase2 = 0.0f;
    dlyPanPhase = 0.0f;

    meterAttCoef = onePoleCoef(1.0f, sampleRate);
    meterRelCoef = onePoleCoef(130.0f, sampleRate);   // fast, real-time feel — was 400ms
}

void XaLZaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    if (numCh <= 0 || numSamples <= 0)
        return;

    // ---------------------------------------------------------------
    // MIDI Learn / CC control for the 12 macro knobs — a CC message either
    // binds to whichever macro startMidiLearn() last armed (editor-driven,
    // via a right-click menu on the macro knob), or, once bound, drives
    // that macro directly. setValueNotifyingHost from the audio thread is
    // the standard JUCE pattern for MIDI-mapped parameters (APVTS's
    // attachments marshal the UI-side update to the message thread
    // internally), so this is safe here.
    // ---------------------------------------------------------------
    for (const auto metadata : midi)
    {
        auto msg = metadata.getMessage();
        if (!msg.isController())
            continue;
        int cc = msg.getControllerNumber();

        int learnIdx = midiLearnTarget.load(std::memory_order_relaxed);
        if (learnIdx >= 0 && learnIdx < kNumMacros)
        {
            macroCcMap[(size_t) learnIdx].store(cc, std::memory_order_relaxed);
            midiLearnTarget.store(-1, std::memory_order_relaxed);
            continue;
        }

        for (int i = 0; i < kNumMacros; ++i)
        {
            if (macroCcMap[(size_t) i].load(std::memory_order_relaxed) != cc)
                continue;
            if (auto* param = apvts.getParameter(xalzaMacroIDs()[(size_t) i]))
                param->setValueNotifyingHost((float) msg.getControllerValue() / 127.0f);
        }
    }

    // ---------------------------------------------------------------
    // Bypass — real, host-independent dry passthrough. Meters/goniometer
    // still update from the dry signal so the UI doesn't look frozen.
    // ---------------------------------------------------------------
    if (apvts.getRawParameterValue(XID::MasterBypass)->load() > 0.5f)
    {
        for (int tap = 0; tap < (int) kNumMeterTaps; ++tap)
            updateMeter(tap, buffer, numSamples, numCh);

        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; n += 4)
        {
            int pos = scopeWritePos.load(std::memory_order_relaxed);
            scopePointsL[(size_t) (pos & (kScopeSize - 1))].store(l[n], std::memory_order_relaxed);
            scopePointsR[(size_t) (pos & (kScopeSize - 1))].store(r[n], std::memory_order_relaxed);
            scopeWritePos.store(pos + 1, std::memory_order_relaxed);
        }
        return;
    }

    auto& mt = macroTracker;

    // ---------------------------------------------------------------
    // Master In Gain
    // ---------------------------------------------------------------
    applySmoothedGainDb(masterInSmoothed, buffer, apvts.getRawParameterValue(XID::MasterInGain)->load(), numSamples);
    updateMeter((int) TapIn, buffer, numSamples, numCh);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    // ---------------------------------------------------------------
    // 1) PREAMP — HPF, clean gain, tanh "character" blended dry/wet
    // ---------------------------------------------------------------
    auto runPre = [&]()
    {
    bool preBypassed = apvts.getRawParameterValue(XID::PreBypass)->load() > 0.5f;
    if (!preBypassed)
    {
        float hpfHz = juce::jlimit(20.0f, 500.0f, mt.effectiveByID(XID::PreMacro, XID::PreHPF));
        lastHpfHz.store(hpfHz, std::memory_order_relaxed);
        *preHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpfHz);
        preHpf.process(ctx);

        // Impedance: a real, subtle high-shelf tilt (not just cosmetic) —
        // mirrors how a dynamic mic's top end shifts a little with
        // different preamp input-impedance loading. 300ohm reads a touch
        // darker, 2.4kohm a touch brighter, 1.2kohm (the default) is flat.
        float impedanceOhms = apvts.getRawParameterValue(XID::PreImpedance)->load();
        float shelfDb = juce::jmap(impedanceOhms, 300.0f, 2400.0f, -1.5f, 1.5f);
        *preImpShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sr, 8000.0f, 0.707f, juce::Decibels::decibelsToGain(shelfDb));
        preImpShelf.process(ctx);

        // Phase: real polarity flip, ahead of the pad/gain stages (linear,
        // so where it sits relative to them doesn't matter).
        if (apvts.getRawParameterValue(XID::PrePhase)->load() > 0.5f)
            buffer.applyGain(-1.0f);

        // Pad: real -20dB input pad, ahead of the Gain knob (like a
        // physical preamp's pad switch), smoothed so toggling it live
        // doesn't click.
        bool padOn = apvts.getRawParameterValue(XID::PrePad)->load() > 0.5f;
        applySmoothedGainDb(prePadGainSmoothed, buffer, padOn ? -20.0f : 0.0f, numSamples);

        applySmoothedGainDb(preGainSmoothed, buffer, mt.effectiveByID(XID::PreMacro, XID::PreGain), numSamples);

        float charAmt = mt.effectiveByID(XID::PreMacro, XID::PreChar) / 100.0f;
        if (charAmt > 0.0005f)
        {
            float drive = juce::jmap(charAmt, 0.0f, 1.0f, 1.0f, 5.0f);
            float norm = std::tanh(drive);

            // 2x-oversampled tanh — keeps the aliasing this waveshaper would
            // otherwise fold back into the audible band from ever forming.
            auto sub = block.getSubsetChannelBlock(0, (size_t) numCh);
            auto osBlock = osPreChar.processSamplesUp(sub);
            for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
            {
                auto* d = osBlock.getChannelPointer(ch);
                for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
                {
                    float dry = d[n];
                    float wet = std::tanh(dry * drive) / norm;
                    d[n] = dry + (wet - dry) * charAmt;
                }
            }
            osPreChar.processSamplesDown(sub);
        }
    }
    updateMeter((int) TapPre, buffer, numSamples, numCh);
    pushRaw((int) RawPre, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 2) GATE — envelope-follower expander with hold, attack, release
    // ---------------------------------------------------------------
    auto runGate = [&]()
    {
    bool gateBypassed = apvts.getRawParameterValue(XID::GateBypass)->load() > 0.5f;
    if (!gateBypassed)
    {
        float threshDb = mt.effectiveByID(XID::GateMacro, XID::GateThresh);
        float rangeDb   = mt.effectiveByID(XID::GateMacro, XID::GateRange);
        float attackMs  = mt.effectiveByID(XID::GateMacro, XID::GateAttack);
        float holdMs    = mt.effectiveByID(XID::GateMacro, XID::GateHold);
        float releaseMs = mt.effectiveByID(XID::GateMacro, XID::GateRelease);

        int holdSamples = (int) (holdMs * 0.001f * (float) sr);
        float attCoef = onePoleCoef(attackMs, sr);
        float relCoef = onePoleCoef(releaseMs, sr);
        float detAtt = onePoleCoef(0.5f, sr);
        float detRel = onePoleCoef(50.0f, sr);
        float floorLin = juce::Decibels::decibelsToGain(rangeDb);
        bool gateListen = apvts.getRawParameterValue(XID::GateListen)->load() > 0.5f;

        // Optional external sidechain key: detect off a separate signal
        // (routed into the plugin's second input bus) instead of the audio
        // actually being gated. Falls back to normal self-detection whenever
        // the toggle is off or the host hasn't actually connected anything
        // there, so a fresh instance behaves exactly as before.
        bool gateScEnabled = apvts.getRawParameterValue(XID::GateScEnable)->load() > 0.5f;
        auto scBuffer = getBusBuffer(buffer, true, 1);
        bool gateScActive = gateScEnabled && scBuffer.getNumChannels() > 0;

        auto* l = buffer.getWritePointer(0);
        auto* r = numCh > 1 ? buffer.getWritePointer(1) : l;
        auto* detL = gateScActive ? scBuffer.getReadPointer(0) : l;
        auto* detR = gateScActive ? (scBuffer.getNumChannels() > 1 ? scBuffer.getReadPointer(1) : detL) : r;
        for (int n = 0; n < numSamples; ++n)
        {
            float rect = std::abs(0.5f * (detL[n] + detR[n]));
            float dCoef = rect > gateEnv ? detAtt : detRel;
            gateEnv = dCoef * gateEnv + (1.0f - dCoef) * rect;
            float envDb = juce::Decibels::gainToDecibels(gateEnv, -100.0f);

            bool aboveThresh = envDb > threshDb;
            if (aboveThresh)
                gateHoldCounter = holdSamples;
            else if (gateHoldCounter > 0)
                --gateHoldCounter;

            bool open = aboveThresh || gateHoldCounter > 0;
            float targetGain = open ? 1.0f : floorLin;
            float gCoef = targetGain > gateGain ? attCoef : relCoef;
            gateGain = gCoef * gateGain + (1.0f - gCoef) * targetGain;

            // Listen mode: play back only what the gate is cutting out
            // (dry * (1-gain)) instead of the gated signal — lets you hear
            // exactly what would be removed, the standard way to dial in a
            // gate's threshold/range without guessing.
            float applied = gateListen ? (1.0f - gateGain) : gateGain;
            l[n] *= applied;
            if (numCh > 1) r[n] *= applied;
        }
        gateGrDbUI.store(juce::jlimit(0.0f, 60.0f, -juce::Decibels::gainToDecibels(gateGain, -60.0f)),
                          std::memory_order_relaxed);
    }
    else
    {
        gateGain = 1.0f;
        gateGrDbUI.store(0.0f, std::memory_order_relaxed);
    }
    updateMeter((int) TapGate, buffer, numSamples, numCh);
    pushRaw((int) RawGate, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 3) DE-ESSER — dynamic peak filter driven by a sibilance-band envelope
    // ---------------------------------------------------------------
    auto runEss = [&]()
    {
    bool essBypassed = apvts.getRawParameterValue(XID::EssBypass)->load() > 0.5f;
    if (!essBypassed)
    {
        float threshDb  = mt.effectiveByID(XID::EssMacro, XID::EssThresh);
        float rangeDb   = mt.effectiveByID(XID::EssMacro, XID::EssRange);   // negative, e.g. -8dB
        float freqHz    = mt.effectiveByID(XID::EssMacro, XID::EssFreq);

        auto detCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, juce::jlimit(1000.0f, 16000.0f, freqHz), 3.0f);
        *essDetectL.coefficients = *detCoeffs;
        *essDetectR.coefficients = *detCoeffs;

        float detAtt = onePoleCoef(3.0f, sr);
        float detRel = onePoleCoef(60.0f, sr);
        float smAtt  = onePoleCoef(3.0f, sr);
        float smRel  = onePoleCoef(80.0f, sr);
        bool essListen = apvts.getRawParameterValue(XID::EssListen)->load() > 0.5f;

        auto* l = buffer.getWritePointer(0);
        auto* r = numCh > 1 ? buffer.getWritePointer(1) : l;
        for (int n = 0; n < numSamples; ++n)
        {
            float fl = essDetectL.processSample(l[n]);
            float fr = numCh > 1 ? essDetectR.processSample(r[n]) : fl;
            float rect = std::abs(0.5f * (fl + fr));
            float dCoef = rect > essEnv ? detAtt : detRel;
            essEnv = dCoef * essEnv + (1.0f - dCoef) * rect;

            float envDb = juce::Decibels::gainToDecibels(essEnv, -100.0f);
            float targetAtten = envDb > threshDb
                ? juce::jlimit(rangeDb, 0.0f, -(envDb - threshDb) * 1.5f)
                : 0.0f;
            float sCoef = targetAtten < essGainDb ? smAtt : smRel;
            essGainDb = sCoef * essGainDb + (1.0f - sCoef) * targetAtten;

            // Listen mode: play back exactly the band the detector is
            // reacting to, instead of the main signal — bypasses the
            // dynamic EQ stage entirely for this block's samples.
            if (essListen)
            {
                l[n] = fl;
                if (numCh > 1) r[n] = fr;
            }
        }

        if (!essListen)
        {
            *essDynEq.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sr, juce::jlimit(1000.0f, 16000.0f, freqHz), 2.5f,
                juce::Decibels::decibelsToGain(essGainDb));
            essDynEq.process(ctx);
        }

        essBandDbUI.store(juce::Decibels::gainToDecibels(essEnv, -100.0f), std::memory_order_relaxed);
        essReductionDbUI.store(essGainDb, std::memory_order_relaxed);
    }
    else
    {
        essEnv = 0.0f;
        essGainDb = 0.0f;
        essBandDbUI.store(-100.0f, std::memory_order_relaxed);
        essReductionDbUI.store(0.0f, std::memory_order_relaxed);
    }
    updateMeter((int) TapEss, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 4) GLUE COMP — threshold/ratio via juce::dsp, makeup + dry/wet mix
    // ---------------------------------------------------------------
    auto runComp = [&]()
    {
    bool compBypassed = apvts.getRawParameterValue(XID::CompBypass)->load() > 0.5f;
    if (!compBypassed)
    {
        float thresh = mt.effectiveByID(XID::CompMacro, XID::CompThresh);
        float ratio  = juce::jmax(1.0f, apvts.getRawParameterValue(XID::CompRatio)->load());
        float makeup = mt.effectiveByID(XID::CompMacro, XID::CompMakeup);
        float attackMs = mt.effectiveByID(XID::CompMacro, XID::CompAttack);
        float releaseMs = mt.effectiveByID(XID::CompMacro, XID::CompRelease);
        float mixAmt = mt.effectiveByID(XID::CompMacro, XID::CompMix) / 100.0f;

        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        compressor.setThreshold(thresh);
        compressor.setRatio(ratio);
        compressor.setAttack(attackMs);
        compressor.setRelease(releaseMs);
        compressor.process(ctx);

        {
            float inPk = 0.0f, outPk = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* di = dryBuffer.getReadPointer(ch);
                auto* do_ = buffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    inPk = juce::jmax(inPk, std::abs(di[n]));
                    outPk = juce::jmax(outPk, std::abs(do_[n]));
                }
            }
            updateGr(0, juce::Decibels::gainToDecibels(inPk, -100.0f), juce::Decibels::gainToDecibels(outPk, -100.0f));
        }

        applySmoothedGainDb(compMakeupSmoothed, buffer, makeup, numSamples);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            for (int n = 0; n < numSamples; ++n)
                wet[n] = dry[n] + (wet[n] - dry[n]) * mixAmt;
        }
    }
    else
    {
        updateGr(0, 0.0f, 0.0f);
    }
    updateMeter((int) TapComp, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 5) OPTO — slow program-dependent 2nd compression stage, dry/wet mix
    // ---------------------------------------------------------------
    auto runOpto = [&]()
    {
    bool optoBypassed = apvts.getRawParameterValue(XID::OptoBypass)->load() > 0.5f;
    if (!optoBypassed)
    {
        float reduction = mt.effectiveByID(XID::OptoMacro, XID::OptoReduction) / 100.0f;
        float gainDb    = mt.effectiveByID(XID::OptoMacro, XID::OptoGain);
        float mixAmt    = mt.effectiveByID(XID::OptoMacro, XID::OptoMix) / 100.0f;
        float threshDb  = juce::jmap(reduction, 0.0f, 1.0f, 0.0f, -30.0f);

        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        // Mode: real ratio switch — Compress uses the original gentle 4:1,
        // Limit bites much harder at 20:1 (matches the mockup's
        // optoModeSegs: Compress/Limit).
        bool limitMode = apvts.getRawParameterValue(XID::OptoMode)->load() > 0.5f;
        optoComp.setThreshold(threshDb);
        optoComp.setRatio(limitMode ? 20.0f : 4.0f);
        optoComp.process(ctx);

        {
            float inPk = 0.0f, outPk = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* di = dryBuffer.getReadPointer(ch);
                auto* do_ = buffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    inPk = juce::jmax(inPk, std::abs(di[n]));
                    outPk = juce::jmax(outPk, std::abs(do_[n]));
                }
            }
            updateGr(1, juce::Decibels::gainToDecibels(inPk, -100.0f), juce::Decibels::gainToDecibels(outPk, -100.0f));
        }

        applySmoothedGainDb(optoGainSmoothed, buffer, gainDb, numSamples);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            for (int n = 0; n < numSamples; ++n)
                wet[n] = dry[n] + (wet[n] - dry[n]) * mixAmt;
        }
    }
    else
    {
        updateGr(1, 0.0f, 0.0f);
    }
    updateMeter((int) TapOpto, buffer, numSamples, numCh);
    pushRaw((int) RawOpto, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 6) EQ 550 — 3-band (low shelf @150Hz / mid peak @1kHz / high shelf @6kHz)
    // ---------------------------------------------------------------
    auto runEq = [&]()
    {
    if (apvts.getRawParameterValue(XID::EqBypass)->load() <= 0.5f)
    {
        float lowDb   = mt.effectiveByID(XID::EqMacro, XID::EqLow);
        float midDb   = mt.effectiveByID(XID::EqMacro, XID::EqMid);
        float highDb  = mt.effectiveByID(XID::EqMacro, XID::EqHigh);
        float lowHz   = apvts.getRawParameterValue(XID::EqLowFreq)->load();
        float midHz   = apvts.getRawParameterValue(XID::EqMidFreq)->load();
        float highHz  = apvts.getRawParameterValue(XID::EqHighFreq)->load();

        *eqLowShelf.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, lowHz, 0.707f, juce::Decibels::decibelsToGain(lowDb));
        *eqMidPeak.state   = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, midHz, 0.9f, juce::Decibels::decibelsToGain(midDb));
        *eqHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, highHz, 0.707f, juce::Decibels::decibelsToGain(highDb));

        eqLowShelf.process(ctx);
        eqMidPeak.process(ctx);
        eqHighShelf.process(ctx);
    }
    updateMeter((int) TapEq, buffer, numSamples, numCh);

    // ---------------------------------------------------------------
    // Spectrum tap — raw full-rate mono samples for the EQ page's live
    // spectrum analyser (post-EQ, so it shows what the EQ curve actually did).
    // ---------------------------------------------------------------
    {
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; ++n)
        {
            int pos = specWritePos.load(std::memory_order_relaxed);
            specRing[(size_t) (pos & (kSpecSize - 1))].store(0.5f * (l[n] + r[n]), std::memory_order_relaxed);
            specWritePos.store(pos + 1, std::memory_order_relaxed);
        }
    }
    };

    // ---------------------------------------------------------------
    // 7) RESONANCE — dynamically-tracking de-resonator: a bandpass-detector
    //    envelope centred on the ResLow..ResHigh band drives how hard the
    //    notch bites in real time (only engaging when that band is
    //    actually resonating), instead of a fixed always-on cut.
    //    ResReactivity now genuinely controls the follower's speed: 0% is
    //    smooth/near-static, 100% pounces on transient resonant peaks and
    //    releases fast right after.
    // ---------------------------------------------------------------
    auto runRes = [&]()
    {
    bool resBypassed = apvts.getRawParameterValue(XID::ResBypass)->load() > 0.5f;
    if (!resBypassed)
    {
        float amount     = mt.effectiveByID(XID::ResMacro, XID::ResAmount) / 100.0f;
        float sharpness  = mt.effectiveByID(XID::ResMacro, XID::ResSharpness) / 100.0f;
        float notchLimit = mt.effectiveByID(XID::ResMacro, XID::ResNotchLimit);
        float reactivity = mt.effectiveByID(XID::ResMacro, XID::ResReactivity) / 100.0f;
        float lowHz      = mt.effectiveByID(XID::ResMacro, XID::ResLow);
        float highHz     = mt.effectiveByID(XID::ResMacro, XID::ResHigh);

        float freq = juce::jlimit(40.0f, 18000.0f, std::sqrt(juce::jmax(1.0f, lowHz) * juce::jmax(1.0f, highHz)));
        float q = juce::jmap(sharpness, 0.0f, 1.0f, 0.5f, 8.0f);
        float bandQ = juce::jlimit(0.3f, 6.0f, freq / juce::jmax(20.0f, highHz - lowHz));

        auto detCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freq, bandQ);
        *resDetectL.coefficients = *detCoeffs;
        *resDetectR.coefficients = *detCoeffs;

        float attMs = juce::jmap(reactivity, 0.0f, 1.0f, 25.0f, 1.5f);
        float relMs = juce::jmap(reactivity, 0.0f, 1.0f, 300.0f, 25.0f);
        float detAtt = onePoleCoef(attMs, sr);
        float detRel = onePoleCoef(relMs, sr);
        float cutSmoothCoef = onePoleCoef(juce::jmax(3.0f, relMs * 0.5f), sr);

        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        float cutDb = resCutSmoothed;
        for (int n = 0; n < numSamples; ++n)
        {
            float fl = resDetectL.processSample(l[n]);
            float fr = numCh > 1 ? resDetectR.processSample(r[n]) : fl;
            float rect = std::abs(0.5f * (fl + fr));
            float dCoef = rect > resEnv ? detAtt : detRel;
            resEnv = dCoef * resEnv + (1.0f - dCoef) * rect;

            float envDb = juce::Decibels::gainToDecibels(resEnv, -100.0f);
            float depthNorm = juce::jlimit(0.0f, 1.0f, (envDb + 40.0f) / 34.0f);   // -40..-6dB band-energy window
            float targetCut = notchLimit * amount * depthNorm;
            resCutSmoothed = cutSmoothCoef * resCutSmoothed + (1.0f - cutSmoothCoef) * targetCut;
            cutDb = resCutSmoothed;
        }
        resCutDbUI.store(cutDb, std::memory_order_relaxed);

        *resNotch.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, freq, q, juce::Decibels::decibelsToGain(cutDb));
        resNotch.process(ctx);
    }
    else
    {
        resEnv = 0.0f;
        resCutSmoothed = 0.0f;
        resCutDbUI.store(0.0f, std::memory_order_relaxed);
    }
    updateMeter((int) TapRes, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 8) SATURATOR — tanh drive, tone tilt, soft ceiling, dry/wet mix
    // ---------------------------------------------------------------
    auto runSat = [&]()
    {
    // Tapped right here (Sat's own input, whatever currently precedes it
    // in the chain) rather than at the end of a fixed "previous" module,
    // so the SAT page's in-vs-out scope stays correct after a reorder.
    pushRaw((int) RawSatIn, buffer, numSamples, numCh);

    bool satBypassed = apvts.getRawParameterValue(XID::SatBypass)->load() > 0.5f;
    if (!satBypassed)
    {
        float drive   = mt.effectiveByID(XID::SatMacro, XID::SatDrive) / 100.0f;
        float toneDb  = mt.effectiveByID(XID::SatMacro, XID::SatTone);
        float ceilDb  = mt.effectiveByID(XID::SatMacro, XID::SatCeiling);
        float mixAmt  = mt.effectiveByID(XID::SatMacro, XID::SatMix) / 100.0f;

        *satTone.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 3000.0f, 0.707f, juce::Decibels::decibelsToGain(toneDb));

        if (mixAmt > 0.0005f)
        {
            for (int ch = 0; ch < numCh; ++ch)
                dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

            float driveAmt = juce::jmap(drive, 0.0f, 1.0f, 1.0f, 10.0f);
            float norm = std::tanh(driveAmt);
            float ceilLin = juce::Decibels::decibelsToGain(ceilDb);

            // Character (mockup's satCharSegs): four genuinely different
            // waveshapes, not just a label on the same curve —
            //   0 Tube:       the original symmetric tanh (soft, even-order-light)
            //   1 Tape:       tanh with a small DC bias -> asymmetric, adds 2nd harmonic
            //   2 Transistor: cubic soft-clip -> harder knee, more odd harmonics
            //   3 Diode:      asymmetric tanh (different +/- slope) -> classic diode-clipper feel
            int charMode = (int) std::round(apvts.getRawParameterValue(XID::SatChar)->load());
            auto shape = [&] (float x) -> float
            {
                switch (charMode)
                {
                    case 1:
                    {
                        constexpr float bias = 0.06f;
                        return (std::tanh((x + bias) * driveAmt) - std::tanh(bias * driveAmt)) / norm;
                    }
                    case 2:
                    {
                        float y = juce::jlimit(-1.0f, 1.0f, x * driveAmt / 3.0f);
                        return (y - (y * y * y) / 3.0f) / (2.0f / 3.0f);
                    }
                    case 3:
                    {
                        float xd = x * driveAmt;
                        return (xd >= 0.0f ? std::tanh(xd * 1.4f) : std::tanh(xd * 0.7f)) / norm;
                    }
                    default:
                        return std::tanh(x * driveAmt) / norm;
                }
            };

            // 2x-oversampled — this is the hardest-driven waveshaper in the
            // chain, so it's the one that benefits most from anti-aliasing.
            {
                auto sub = block.getSubsetChannelBlock(0, (size_t) numCh);
                auto osBlock = osSat.processSamplesUp(sub);
                for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
                {
                    auto* d = osBlock.getChannelPointer(ch);
                    for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
                    {
                        float wet = shape(d[n]);
                        if (std::abs(wet) > ceilLin)
                            wet = ceilLin * std::tanh(wet / ceilLin); // soft-knee clamp toward ceiling (tanh is odd, sign preserved)
                        d[n] = wet;
                    }
                }
                osSat.processSamplesDown(sub);
            }

            satTone.process(ctx);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* wet = buffer.getWritePointer(ch);
                auto* dry = dryBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    wet[n] = dry[n] + (wet[n] - dry[n]) * mixAmt;
            }
        }
    }
    updateMeter((int) TapSat, buffer, numSamples, numCh);
    pushRaw((int) RawSatOut, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 9) DOUBLER — two modulated delay voices layered on top of the dry signal
    // ---------------------------------------------------------------
    auto runDbl = [&]()
    {
    bool dblBypassed = apvts.getRawParameterValue(XID::DblBypass)->load() > 0.5f;
    if (!dblBypassed)
    {
        float detuneAmt = mt.effectiveByID(XID::DblMacro, XID::DblDetune);
        float widthPct  = mt.effectiveByID(XID::DblMacro, XID::DblWidth) / 100.0f;
        float delayMs   = mt.effectiveByID(XID::DblMacro, XID::DblDelay);
        float mixAmt    = mt.effectiveByID(XID::DblMacro, XID::DblMix) / 100.0f;

        if (mixAmt > 0.0005f)
        {
            float baseSamples1 = juce::jmax(1.0f, delayMs * 0.001f * (float) sr);
            float baseSamples2 = juce::jmax(1.0f, (delayMs + 7.0f) * 0.001f * (float) sr);
            float modDepth = juce::jmap(detuneAmt, 0.0f, 40.0f, 0.0f, 6.0f);
            float w1 = 2.0f * juce::MathConstants<float>::pi * 0.63f / (float) sr;
            float w2 = 2.0f * juce::MathConstants<float>::pi * 0.71f / (float) sr;

            auto* inL = buffer.getReadPointer(0);
            auto* inR = numCh > 1 ? buffer.getReadPointer(1) : inL;
            auto* outL = dblBuffer.getWritePointer(0);
            auto* outR = dblBuffer.getWritePointer(1);

            for (int n = 0; n < numSamples; ++n)
            {
                float monoIn = 0.5f * (inL[n] + inR[n]);
                dblPhase1 += w1; if (dblPhase1 > juce::MathConstants<float>::twoPi) dblPhase1 -= juce::MathConstants<float>::twoPi;
                dblPhase2 += w2; if (dblPhase2 > juce::MathConstants<float>::twoPi) dblPhase2 -= juce::MathConstants<float>::twoPi;

                dblDelayL.setDelay(juce::jmax(1.0f, baseSamples1 + modDepth * std::sin(dblPhase1)));
                dblDelayR.setDelay(juce::jmax(1.0f, baseSamples2 + modDepth * std::sin(dblPhase2)));

                float v1 = dblDelayL.popSample(0);
                float v2 = dblDelayR.popSample(0);
                dblDelayL.pushSample(0, monoIn);
                dblDelayR.pushSample(0, monoIn);

                float centre = 0.5f * (v1 + v2);
                outL[n] = juce::jmap(widthPct, 0.0f, 1.0f, centre, v1);
                outR[n] = juce::jmap(widthPct, 0.0f, 1.0f, centre, v2);
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                auto* wet = dblBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    dst[n] += wet[n] * mixAmt * 0.85f;
            }
        }
    }
    updateMeter((int) TapDbl, buffer, numSamples, numCh);
    {
        // Post-Doubler stereo scope — genuinely the wet stereo-widened
        // signal, decimated the same way as the master goniometer, for the
        // Doubler page's own stereo-field view.
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; n += 4)
        {
            int pos = dblScopeWritePos.load(std::memory_order_relaxed);
            dblScopeL[(size_t) (pos & (kScopeSize - 1))].store(l[n], std::memory_order_relaxed);
            dblScopeR[(size_t) (pos & (kScopeSize - 1))].store(r[n], std::memory_order_relaxed);
            dblScopeWritePos.store(pos + 1, std::memory_order_relaxed);
        }
    }
    };

    // ---------------------------------------------------------------
    // 10) REVERB — pre-delay, size/decay, duck, mixed back in
    // ---------------------------------------------------------------
    auto runRev = [&]()
    {
    bool revBypassed = apvts.getRawParameterValue(XID::RevBypass)->load() > 0.5f;
    if (!revBypassed)
    {
        float sizePct    = mt.effectiveByID(XID::RevMacro, XID::RevSize) / 100.0f;
        float decaySec   = mt.effectiveByID(XID::RevMacro, XID::RevDecay);
        float preDelayMs = mt.effectiveByID(XID::RevMacro, XID::RevPreDelay);
        float mixPct     = mt.effectiveByID(XID::RevMacro, XID::RevMix) / 100.0f;
        float duckPct    = mt.effectiveByID(XID::RevMacro, XID::RevDuck) / 100.0f;
        float duckRelMs  = mt.effectiveByID(XID::RevMacro, XID::RevDuckRelease);

        revBuffer.setSize(numCh, numSamples, false, false, true);

        float preDelaySamples = juce::jmax(0.0f, preDelayMs * 0.001f * (float) sr);
        revPreDelayL.setDelay(preDelaySamples);
        revPreDelayR.setDelay(preDelaySamples);
        {
            auto* inL = buffer.getReadPointer(0);
            auto* inR = numCh > 1 ? buffer.getReadPointer(1) : inL;
            auto* outL = revBuffer.getWritePointer(0);
            auto* outR = numCh > 1 ? revBuffer.getWritePointer(1) : outL;
            for (int n = 0; n < numSamples; ++n)
            {
                revPreDelayL.pushSample(0, inL[n]);
                outL[n] = revPreDelayL.popSample(0);
                if (numCh > 1)
                {
                    revPreDelayR.pushSample(0, inR[n]);
                    outR[n] = revPreDelayR.popSample(0);
                }
            }
        }

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = juce::jlimit(0.0f, 1.0f, sizePct);
        rp.damping    = juce::jlimit(0.05f, 0.95f, juce::jmap(decaySec, 0.3f, 8.0f, 0.9f, 0.1f));
        rp.wetLevel   = 1.0f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);

        juce::dsp::AudioBlock<float> revBlock(revBuffer);
        juce::dsp::ProcessContextReplacing<float> revCtx(revBlock);
        reverb.process(revCtx);

        // Wet-only tone shaping — a genuine user-facing filter pair on the
        // tail, separate from the reverb's own internal room-size/damping
        // model, so you can clean up boom or tame harshness independently.
        float wetHpfHz = apvts.getRawParameterValue(XID::RevWetHpf)->load();
        float wetLpfHz = apvts.getRawParameterValue(XID::RevWetLpf)->load();
        *revWetHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, juce::jmax(1.0f, wetHpfHz));
        *revWetLpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, juce::jmax(20.0f, wetLpfHz));
        revWetHpf.process(revCtx);
        revWetLpf.process(revCtx);

        if (mixPct > 0.0005f)
        {
            float attCoef = onePoleCoef(5.0f, sr);
            float relCoef = onePoleCoef(duckRelMs, sr);
            auto* l = buffer.getReadPointer(0);
            auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                auto* wet = revBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    if (ch == 0)
                    {
                        float rect = std::abs(0.5f * (l[n] + r[n]));
                        float coef = rect > revDuckEnv ? attCoef : relCoef;
                        revDuckEnv = coef * revDuckEnv + (1.0f - coef) * rect;
                    }
                    float env = juce::jlimit(0.0f, 1.0f, revDuckEnv * 4.0f);
                    dst[n] += wet[n] * (mixPct * (1.0f - duckPct * env));
                }
            }
        }
    }
    updateMeter((int) TapRev, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 11) DELAY — ping-pong, spread, duck, auto-pan LFO on the wet signal
    // ---------------------------------------------------------------
    auto runDly = [&]()
    {
    bool dlyBypassed = apvts.getRawParameterValue(XID::DlyBypass)->load() > 0.5f;
    if (!dlyBypassed)
    {
        float timeMs    = apvts.getRawParameterValue(XID::DlyTime)->load();
        float fbPct     = mt.effectiveByID(XID::DlyMacro, XID::DlyFeedback) / 100.0f;
        float spreadPct = mt.effectiveByID(XID::DlyMacro, XID::DlySpread) / 100.0f;
        float mixPct    = mt.effectiveByID(XID::DlyMacro, XID::DlyMix) / 100.0f;
        float duckPct   = mt.effectiveByID(XID::DlyMacro, XID::DlyDuck) / 100.0f;
        float duckRelMs = mt.effectiveByID(XID::DlyMacro, XID::DlyDuckRelease);
        float panRateHz = mt.effectiveByID(XID::DlyMacro, XID::DlyPanRate);

        dlyBuffer.setSize(numCh, numSamples, false, false, true);

        float maxDelay = (float) delayL.getMaximumDelayInSamples() - 1.0f;
        float delaySamplesL = juce::jlimit(1.0f, maxDelay, (timeMs * 0.001f) * (float) sr);
        float delaySamplesR = juce::jlimit(1.0f, maxDelay, delaySamplesL * (1.0f + spreadPct * 0.15f));
        delayL.setDelay(delaySamplesL);
        delayR.setDelay(delaySamplesR);

        // Feedback-path filtering — set once per block, applied per-sample
        // below to whatever gets pushed BACK into the delay line (not the
        // dry-through). Since it's recirculated, this compounds a little
        // more each pass, so later repeats read progressively darker/
        // thinner — the classic analog/tape-echo character.
        float fbHpfHz = apvts.getRawParameterValue(XID::DlyFbHpf)->load();
        float fbLpfHz = apvts.getRawParameterValue(XID::DlyFbLpf)->load();
        auto fbHpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, juce::jmax(1.0f, fbHpfHz));
        auto fbLpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, juce::jmax(20.0f, fbLpfHz));
        dlyFbHpfL.coefficients = fbHpfCoeffs; dlyFbHpfR.coefficients = fbHpfCoeffs;
        dlyFbLpfL.coefficients = fbLpfCoeffs; dlyFbLpfR.coefficients = fbLpfCoeffs;

        auto* inL = buffer.getReadPointer(0);
        auto* inR = numCh > 1 ? buffer.getReadPointer(1) : inL;
        auto* outL = dlyBuffer.getWritePointer(0);
        auto* outR = numCh > 1 ? dlyBuffer.getWritePointer(1) : outL;

        float panW = 2.0f * juce::MathConstants<float>::pi * panRateHz / (float) sr;
        float attCoef = onePoleCoef(5.0f, sr);
        float relCoef = onePoleCoef(duckRelMs, sr);

        for (int n = 0; n < numSamples; ++n)
        {
            float dL = delayL.popSample(0);
            float dR = delayR.popSample(0);
            float fbL = dlyFbLpfL.processSample(dlyFbHpfL.processSample(dL));
            float fbR = dlyFbLpfR.processSample(dlyFbHpfR.processSample(dR));
            delayL.pushSample(0, inL[n] + fbR * fbPct);
            delayR.pushSample(0, inR[n] + fbL * fbPct);

            dlyPanPhase += panW;
            if (dlyPanPhase > juce::MathConstants<float>::twoPi) dlyPanPhase -= juce::MathConstants<float>::twoPi;
            float panL = 0.5f - 0.5f * std::sin(dlyPanPhase) * 0.6f;
            float panR = 0.5f + 0.5f * std::sin(dlyPanPhase) * 0.6f;

            outL[n] = dL * (0.7f + 0.3f * panL) + dR * (0.3f * (1.0f - panL));
            outR[n] = dR * (0.7f + 0.3f * panR) + dL * (0.3f * (1.0f - panR));

            float rect = std::abs(0.5f * (inL[n] + inR[n]));
            float coef = rect > dlyDuckEnv ? attCoef : relCoef;
            dlyDuckEnv = coef * dlyDuckEnv + (1.0f - coef) * rect;
        }

        if (mixPct > 0.0005f)
        {
            float env = juce::jlimit(0.0f, 1.0f, dlyDuckEnv * 4.0f);
            float g = mixPct * (1.0f - duckPct * env);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                auto* wet = dlyBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    dst[n] += wet[n] * g;
            }
        }
    }
    updateMeter((int) TapDly, buffer, numSamples, numCh);
    pushRaw((int) RawDly, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 12) LIMITER — input trim, ceiling, release, extra tanh clip stage
    // ---------------------------------------------------------------
    auto runLim = [&]()
    {
    bool limBypassed = apvts.getRawParameterValue(XID::LimBypass)->load() > 0.5f;
    if (!limBypassed)
    {
        float ceilingDb = mt.effectiveByID(XID::LimMacro, XID::LimCeiling);
        float inGain    = mt.effectiveByID(XID::LimMacro, XID::LimInputGain);
        float releaseMs = mt.effectiveByID(XID::LimMacro, XID::LimRelease);
        float clipAmt   = mt.effectiveByID(XID::LimMacro, XID::LimClip) / 100.0f;

        applySmoothedGainDb(limInGainSmoothed, buffer, inGain, numSamples);

        float limInPk = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getReadPointer(ch);
            for (int n = 0; n < numSamples; ++n)
                limInPk = juce::jmax(limInPk, std::abs(d[n]));
        }

        // Real look-ahead brickwall: write this block into the ring, then
        // for each output sample scan forward through the look-ahead
        // window (all of which has just been written, so it's always
        // available) to find the peak that's about to arrive and apply
        // the needed gain reduction *ahead of* it, not after.
        {
            float ceilLinLim = juce::Decibels::decibelsToGain(ceilingDb);
            float attCoefLim = onePoleCoef(0.3f, sr);         // near-instant — look-ahead already saw it coming
            float relCoefLim = onePoleCoef(releaseMs, sr);

            int w = limRingWritePos;
            auto* inL = buffer.getReadPointer(0);
            auto* inR = numCh > 1 ? buffer.getReadPointer(1) : inL;
            auto* ringL = limLookaheadRing.getWritePointer(0);
            auto* ringR = limLookaheadRing.getWritePointer(1);
            for (int n = 0; n < numSamples; ++n)
            {
                ringL[(w + n) & limRingMask] = inL[n];
                ringR[(w + n) & limRingMask] = numCh > 1 ? inR[n] : inL[n];
            }
            limRingWritePos = w + numSamples;

            auto* outL = buffer.getWritePointer(0);
            auto* outR = numCh > 1 ? buffer.getWritePointer(1) : outL;
            for (int n = 0; n < numSamples; ++n)
            {
                int outPos = w + n - limLookaheadSamples;
                float peakAhead = 0.0f;
                for (int k = 0; k <= limLookaheadSamples; ++k)
                {
                    int idx = (outPos + k) & limRingMask;
                    peakAhead = juce::jmax(peakAhead, std::abs(ringL[idx]), std::abs(ringR[idx]));
                }
                float targetGain = juce::jmin(1.0f, ceilLinLim / juce::jmax(1.0e-6f, peakAhead));
                float coef = targetGain < limGainSmoothed ? attCoefLim : relCoefLim;
                limGainSmoothed = coef * limGainSmoothed + (1.0f - coef) * targetGain;

                int readIdx = outPos & limRingMask;
                outL[n] = ringL[readIdx] * limGainSmoothed;
                if (numCh > 1)
                    outR[n] = ringR[readIdx] * limGainSmoothed;
            }
        }

        {
            float limOutPk = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    limOutPk = juce::jmax(limOutPk, std::abs(d[n]));
            }
            updateGr(2, juce::Decibels::gainToDecibels(limInPk, -100.0f), juce::Decibels::gainToDecibels(limOutPk, -100.0f));
        }

        if (clipAmt > 0.0005f)
        {
            float ceilLin = juce::Decibels::decibelsToGain(ceilingDb);
            float driveAmt = 3.0f;
            float norm = std::tanh(driveAmt);

            auto sub = block.getSubsetChannelBlock(0, (size_t) numCh);
            auto osBlock = osLimClip.processSamplesUp(sub);
            for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
            {
                auto* d = osBlock.getChannelPointer(ch);
                for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
                {
                    float dry = d[n];
                    float wet = ceilLin * std::tanh(dry / juce::jmax(0.0001f, ceilLin) * driveAmt) / norm;
                    d[n] = dry + (wet - dry) * clipAmt;
                }
            }
            osLimClip.processSamplesDown(sub);
        }
    }
    else
    {
        updateGr(2, 0.0f, 0.0f);
    }
    updateMeter((int) TapLim, buffer, numSamples, numCh);
    pushRaw((int) RawLim, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // Run the 12 modules above in the user's current chain order (identity
    // order — Pre, Gate, Ess, Comp, Opto, Eq, Res, Sat, Dbl, Rev, Dly, Lim —
    // by default, same as the original fixed sequence, so nothing changes
    // unless the user has actually reordered something via moveModule()).
    // Each lambda reads/writes `buffer` in place and reads whatever the
    // chain has produced so far, exactly like the original fixed-order
    // code did — only WHICH ONE runs at each step is now data-driven.
    // ---------------------------------------------------------------
    {
        const std::array<std::function<void()>, kNumSlots> runners = {
            runPre, runGate, runEss, runComp, runOpto, runEq, runRes, runSat, runDbl, runRev, runDly, runLim
        };
        for (int pos = 0; pos < kNumSlots; ++pos)
            runners[(size_t) chainOrder[(size_t) pos].load(std::memory_order_relaxed)]();
    }

    // ---------------------------------------------------------------
    // Stereo Width — mid/side, Master utility control (not macro-linked)
    // ---------------------------------------------------------------
    if (numCh > 1)
    {
        float widthPct = apvts.getRawParameterValue(XID::MasterWidth)->load() / 100.0f;
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);
        for (int n = 0; n < numSamples; ++n)
        {
            float mid  = 0.5f * (l[n] + r[n]);
            float side = 0.5f * (l[n] - r[n]) * widthPct;
            l[n] = mid + side;
            r[n] = mid - side;
        }
    }

    // ---------------------------------------------------------------
    // Master Out Gain
    // ---------------------------------------------------------------
    applySmoothedGainDb(masterOutSmoothed, buffer, apvts.getRawParameterValue(XID::MasterOutGain)->load(), numSamples);
    updateMeter((int) TapOut, buffer, numSamples, numCh);

    // ---------------------------------------------------------------
    // Real (simplified) ITU-R BS.1770 K-weighted momentary LUFS of the true
    // final output — measured here, after everything including Master Out
    // Gain, so the Limiter page's loudness readout reflects what actually
    // leaves the plugin.
    // ---------------------------------------------------------------
    {
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        float msCoef = onePoleCoef(400.0f, sr);
        for (int n = 0; n < numSamples; ++n)
        {
            float xl = lufsRlbL.processSample(lufsPreL.processSample(l[n]));
            float xr = lufsRlbR.processSample(lufsPreR.processSample(r[n]));
            lufsMsL = msCoef * lufsMsL + (1.0f - msCoef) * (xl * xl);
            lufsMsR = msCoef * lufsMsR + (1.0f - msCoef) * (xr * xr);
        }
        float sumMs = juce::jmax(1.0e-10f, lufsMsL + lufsMsR);
        float lufs = -0.691f + 10.0f * std::log10(sumMs);
        lufsUI.store(juce::jlimit(-70.0f, 0.0f, lufs), std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------
    // True-peak (4x-oversampled inter-sample peak) reading of the final
    // output — catches peaks a plain sample-peak reading would miss.
    // ---------------------------------------------------------------
    {
        for (int ch = 0; ch < numCh; ++ch)
            truePeakScratch.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        auto sub = juce::dsp::AudioBlock<float>(truePeakScratch).getSubsetChannelBlock(0, (size_t) numCh);
        auto osBlock = osTruePeak.processSamplesUp(sub);
        float peak = 0.0f;
        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* d = osBlock.getChannelPointer(ch);
            for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
                peak = juce::jmax(peak, std::abs(d[n]));
        }
        truePeakDbUI.store(juce::Decibels::gainToDecibels(peak, -100.0f), std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------
    // Goniometer tap — decimated post-chain stereo samples for the UI's
    // stereo-field scope (every 4th sample is plenty for a visual trace).
    // ---------------------------------------------------------------
    {
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; n += 4)
        {
            int pos = scopeWritePos.load(std::memory_order_relaxed);
            scopePointsL[(size_t) (pos & (kScopeSize - 1))].store(l[n], std::memory_order_relaxed);
            scopePointsR[(size_t) (pos & (kScopeSize - 1))].store(r[n], std::memory_order_relaxed);
            scopeWritePos.store(pos + 1, std::memory_order_relaxed);
        }
    }
}

juce::AudioProcessorEditor* XaLZaProcessor::createEditor()
{
    return new XaLZaEditor(*this);
}

void XaLZaProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    // Extra (non-parameter) attributes ride along on the root element —
    // APVTS ignores unknown attributes on reload, so this is safe to add
    // without touching the parameter schema.
    xml->setAttribute("xalzaEditorW", lastEditorWidth);
    xml->setAttribute("xalzaEditorH", lastEditorHeight);
    for (int i = 0; i < kNumMacros; ++i)
        xml->setAttribute("xalzaCc" + juce::String(i), macroCcMap[(size_t) i].load(std::memory_order_relaxed));
    juce::String orderStr;
    for (int i = 0; i < kNumSlots; ++i)
        orderStr << chainOrder[(size_t) i].load(std::memory_order_relaxed) << (i + 1 < kNumSlots ? "," : "");
    xml->setAttribute("xalzaChainOrder", orderStr);
    copyXmlToBinary(*xml, destData);
}

void XaLZaProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        if (xml->hasAttribute("xalzaEditorW") && xml->hasAttribute("xalzaEditorH"))
        {
            // Clamped to XaLZaEditor's own setResizeLimits(baseW, baseH, baseW*2, baseH*2)
            // — keep these in sync if that ever changes.
            lastEditorWidth  = juce::jlimit(900, 1800, xml->getIntAttribute("xalzaEditorW", 900));
            lastEditorHeight = juce::jlimit(560, 1120, xml->getIntAttribute("xalzaEditorH", 560));
        }
        for (int i = 0; i < kNumMacros; ++i)
        {
            auto key = "xalzaCc" + juce::String(i);
            int cc = xml->hasAttribute(key) ? xml->getIntAttribute(key, -1) : -1;
            macroCcMap[(size_t) i].store(juce::jlimit(-1, 127, cc), std::memory_order_relaxed);
        }

        // Chain order: only accept it if it's genuinely a permutation of
        // 0..kNumSlots-1 — anything else (corrupted state, hand-edited
        // file) falls back to identity order rather than risk running the
        // same module twice or dropping one entirely.
        {
            std::array<int, kNumSlots> loaded {};
            for (auto& v : loaded) v = -1;
            bool valid = xml->hasAttribute("xalzaChainOrder");
            if (valid)
            {
                auto tokens = juce::StringArray::fromTokens(xml->getStringAttribute("xalzaChainOrder"), ",", "");
                valid = tokens.size() == kNumSlots;
                if (valid)
                {
                    std::array<bool, kNumSlots> seen {};
                    for (auto& s : seen) s = false;
                    for (int i = 0; i < kNumSlots; ++i)
                    {
                        int v = tokens[i].getIntValue();
                        if (v < 0 || v >= kNumSlots || seen[(size_t) v]) { valid = false; break; }
                        seen[(size_t) v] = true;
                        loaded[(size_t) i] = v;
                    }
                }
            }
            for (int i = 0; i < kNumSlots; ++i)
                chainOrder[(size_t) i].store(valid ? loaded[(size_t) i] : i, std::memory_order_relaxed);
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XaLZaProcessor();
}
