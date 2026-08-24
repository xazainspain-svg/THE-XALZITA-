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
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createXaLZaParameterLayout()),
      macroTracker(apvts)
{
}

bool XaLZaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void XaLZaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    preHpf.prepare(spec);
    essDynEq.prepare(spec);
    compressor.prepare(spec);
    optoComp.prepare(spec);
    eqLowShelf.prepare(spec);
    eqMidPeak.prepare(spec);
    eqHighShelf.prepare(spec);
    resNotch.prepare(spec);
    satTone.prepare(spec);
    reverb.prepare(spec);
    limiter.prepare(spec);

    essDetectL.prepare(spec);
    essDetectR.prepare(spec);

    compressor.setAttack(12.0f);
    compressor.setRelease(250.0f);
    optoComp.setAttack(30.0f);
    optoComp.setRelease(450.0f);
    limiter.setRelease(80.0f);

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
}

void XaLZaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    if (numCh <= 0 || numSamples <= 0)
        return;

    auto& mt = macroTracker;

    // ---------------------------------------------------------------
    // Master In Gain
    // ---------------------------------------------------------------
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(XID::MasterInGain)->load()));

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    // ---------------------------------------------------------------
    // 1) PREAMP — HPF, clean gain, tanh "character" blended dry/wet
    // ---------------------------------------------------------------
    {
        float hpfHz = mt.effectiveByID(XID::PreMacro, XID::PreHPF);
        *preHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, juce::jlimit(20.0f, 500.0f, hpfHz));
        preHpf.process(ctx);

        buffer.applyGain(juce::Decibels::decibelsToGain(mt.effectiveByID(XID::PreMacro, XID::PreGain)));

        float charAmt = mt.effectiveByID(XID::PreMacro, XID::PreChar) / 100.0f;
        if (charAmt > 0.0005f)
        {
            float drive = juce::jmap(charAmt, 0.0f, 1.0f, 1.0f, 5.0f);
            float norm = std::tanh(drive);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    float dry = d[n];
                    float wet = std::tanh(dry * drive) / norm;
                    d[n] = dry + (wet - dry) * charAmt;
                }
            }
        }
    }

    // ---------------------------------------------------------------
    // 2) GATE — envelope-follower expander with hold, attack, release
    // ---------------------------------------------------------------
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

        auto* l = buffer.getWritePointer(0);
        auto* r = numCh > 1 ? buffer.getWritePointer(1) : l;
        for (int n = 0; n < numSamples; ++n)
        {
            float rect = std::abs(0.5f * (l[n] + r[n]));
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

            l[n] *= gateGain;
            if (numCh > 1) r[n] *= gateGain;
        }
    }

    // ---------------------------------------------------------------
    // 3) DE-ESSER — dynamic peak filter driven by a sibilance-band envelope
    // ---------------------------------------------------------------
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

        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
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
        }

        *essDynEq.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sr, juce::jlimit(1000.0f, 16000.0f, freqHz), 2.5f,
            juce::Decibels::decibelsToGain(essGainDb));
        essDynEq.process(ctx);
    }

    // ---------------------------------------------------------------
    // 4) GLUE COMP — threshold/ratio via juce::dsp, makeup + dry/wet mix
    // ---------------------------------------------------------------
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
        buffer.applyGain(juce::Decibels::decibelsToGain(makeup));

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            for (int n = 0; n < numSamples; ++n)
                wet[n] = dry[n] + (wet[n] - dry[n]) * mixAmt;
        }
    }

    // ---------------------------------------------------------------
    // 5) OPTO — slow program-dependent 2nd compression stage, dry/wet mix
    // ---------------------------------------------------------------
    {
        float reduction = mt.effectiveByID(XID::OptoMacro, XID::OptoReduction) / 100.0f;
        float gainDb    = mt.effectiveByID(XID::OptoMacro, XID::OptoGain);
        float mixAmt    = mt.effectiveByID(XID::OptoMacro, XID::OptoMix) / 100.0f;
        float threshDb  = juce::jmap(reduction, 0.0f, 1.0f, 0.0f, -30.0f);

        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        optoComp.setThreshold(threshDb);
        optoComp.setRatio(4.0f);
        optoComp.process(ctx);
        buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            for (int n = 0; n < numSamples; ++n)
                wet[n] = dry[n] + (wet[n] - dry[n]) * mixAmt;
        }
    }

    // ---------------------------------------------------------------
    // 6) EQ 550 — 3-band (low shelf @150Hz / mid peak @1kHz / high shelf @6kHz)
    // ---------------------------------------------------------------
    {
        float lowDb  = mt.effectiveByID(XID::EqMacro, XID::EqLow);
        float midDb  = mt.effectiveByID(XID::EqMacro, XID::EqMid);
        float highDb = mt.effectiveByID(XID::EqMacro, XID::EqHigh);

        *eqLowShelf.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 150.0f, 0.707f, juce::Decibels::decibelsToGain(lowDb));
        *eqMidPeak.state   = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 1000.0f, 0.9f, juce::Decibels::decibelsToGain(midDb));
        *eqHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain(highDb));

        eqLowShelf.process(ctx);
        eqMidPeak.process(ctx);
        eqHighShelf.process(ctx);
    }

    // ---------------------------------------------------------------
    // 7) RESONANCE — de-resonator: static notch that tames a harsh peak
    //    between ResLow/ResHigh; ResReactivity is reserved for a future
    //    dynamically-tracking version and has no effect yet.
    // ---------------------------------------------------------------
    {
        float amount     = mt.effectiveByID(XID::ResMacro, XID::ResAmount) / 100.0f;
        float sharpness  = mt.effectiveByID(XID::ResMacro, XID::ResSharpness) / 100.0f;
        float notchLimit = mt.effectiveByID(XID::ResMacro, XID::ResNotchLimit);
        float lowHz      = mt.effectiveByID(XID::ResMacro, XID::ResLow);
        float highHz     = mt.effectiveByID(XID::ResMacro, XID::ResHigh);

        float freq = juce::jlimit(40.0f, 18000.0f, std::sqrt(juce::jmax(1.0f, lowHz) * juce::jmax(1.0f, highHz)));
        float q = juce::jmap(sharpness, 0.0f, 1.0f, 0.5f, 8.0f);
        float cutDb = notchLimit * amount;

        *resNotch.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, freq, q, juce::Decibels::decibelsToGain(cutDb));
        resNotch.process(ctx);
    }

    // ---------------------------------------------------------------
    // 8) SATURATOR — tanh drive, tone tilt, soft ceiling, dry/wet mix
    // ---------------------------------------------------------------
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

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    float dry = d[n];
                    float wet = std::tanh(dry * driveAmt) / norm;
                    if (std::abs(wet) > ceilLin)
                        wet = ceilLin * std::tanh(wet / ceilLin); // soft-knee clamp toward ceiling (tanh is odd, sign preserved)
                    d[n] = wet;
                }
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

    // ---------------------------------------------------------------
    // 9) DOUBLER — two modulated delay voices layered on top of the dry signal
    // ---------------------------------------------------------------
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

    // ---------------------------------------------------------------
    // 10) REVERB — pre-delay, size/decay, duck, mixed back in
    // ---------------------------------------------------------------
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

    // ---------------------------------------------------------------
    // 11) DELAY — ping-pong, spread, duck, auto-pan LFO on the wet signal
    // ---------------------------------------------------------------
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
            delayL.pushSample(0, inL[n] + dR * fbPct);
            delayR.pushSample(0, inR[n] + dL * fbPct);

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

    // ---------------------------------------------------------------
    // 12) LIMITER — input trim, ceiling, release, extra tanh clip stage
    // ---------------------------------------------------------------
    {
        float ceilingDb = mt.effectiveByID(XID::LimMacro, XID::LimCeiling);
        float inGain    = mt.effectiveByID(XID::LimMacro, XID::LimInputGain);
        float releaseMs = mt.effectiveByID(XID::LimMacro, XID::LimRelease);
        float clipAmt   = mt.effectiveByID(XID::LimMacro, XID::LimClip) / 100.0f;

        buffer.applyGain(juce::Decibels::decibelsToGain(inGain));
        limiter.setThreshold(ceilingDb);
        limiter.setRelease(releaseMs);
        limiter.process(ctx);

        if (clipAmt > 0.0005f)
        {
            float ceilLin = juce::Decibels::decibelsToGain(ceilingDb);
            float driveAmt = 3.0f;
            float norm = std::tanh(driveAmt);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    float dry = d[n];
                    float wet = ceilLin * std::tanh(dry / juce::jmax(0.0001f, ceilLin) * driveAmt) / norm;
                    d[n] = dry + (wet - dry) * clipAmt;
                }
            }
        }
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
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(XID::MasterOutGain)->load()));
}

juce::AudioProcessorEditor* XaLZaProcessor::createEditor()
{
    return new XaLZaEditor(*this);
}

void XaLZaProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void XaLZaProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XaLZaProcessor();
}
