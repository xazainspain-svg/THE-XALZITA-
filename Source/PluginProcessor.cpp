#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    compressor.prepare(spec);
    eqLowShelf.prepare(spec);
    eqHighShelf.prepare(spec);
    reverb.prepare(spec);
    limiter.prepare(spec);

    compressor.setAttack(10.0f);
    compressor.setRelease(120.0f);
    limiter.setRelease(100.0f);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    delayL.prepare(monoSpec);
    delayR.prepare(monoSpec);
    delayL.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayR.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayL.reset();
    delayR.reset();

    revBuffer.setSize(2, samplesPerBlock, false, false, true);
    dlyBuffer.setSize(2, samplesPerBlock, false, false, true);
    duckEnvBuffer.setSize(1, samplesPerBlock, false, false, true);

    duckEnvShared = 0.0f;
}

void XaLZaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Any input channels beyond what we declared get cleared, standard JUCE practice.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    if (numCh <= 0 || numSamples <= 0)
        return;

    // ---------------------------------------------------------------
    // Master In Gain
    // ---------------------------------------------------------------
    float inGainDb = *apvts.getRawParameterValue(XID::MasterInGain);
    buffer.applyGain(juce::Decibels::decibelsToGain(inGainDb));

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    // ---------------------------------------------------------------
    // 1) Preamp — HPF, clean gain, tanh "character" blended dry/wet
    // ---------------------------------------------------------------
    {
        float hpfHz = macroTracker.effectiveByID(XID::PreMacro, XID::PreHPF);
        *preHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sr, juce::jlimit(20.0f, 500.0f, hpfHz));
        preHpf.process(ctx);

        float preGainDb = macroTracker.effectiveByID(XID::PreMacro, XID::PreGain);
        buffer.applyGain(juce::Decibels::decibelsToGain(preGainDb));

        float charAmt = macroTracker.effectiveByID(XID::PreMacro, XID::PreChar) / 100.0f;
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
    // 2) Compressor — threshold/ratio via juce::dsp, makeup applied after
    // ---------------------------------------------------------------
    {
        float thresh = macroTracker.effectiveByID(XID::CompMacro, XID::CompThresh);
        float ratio  = juce::jmax(1.0f, macroTracker.effectiveByID(XID::CompMacro, XID::CompRatio));
        float makeup = macroTracker.effectiveByID(XID::CompMacro, XID::CompMakeup);

        compressor.setThreshold(thresh);
        compressor.setRatio(ratio);
        compressor.process(ctx);

        buffer.applyGain(juce::Decibels::decibelsToGain(makeup));
    }

    // ---------------------------------------------------------------
    // 3) EQ — low shelf @150Hz, high shelf @6kHz
    // ---------------------------------------------------------------
    {
        float lowDb  = macroTracker.effectiveByID(XID::EqMacro, XID::EqLowGain);
        float highDb = macroTracker.effectiveByID(XID::EqMacro, XID::EqHighGain);

        *eqLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sr, 150.0f, 0.707f, juce::Decibels::decibelsToGain(lowDb));
        *eqHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain(highDb));

        eqLowShelf.process(ctx);
        eqHighShelf.process(ctx);
    }

    // ---------------------------------------------------------------
    // 4) Saturator — heavier tanh drive, dry/wet mix
    // ---------------------------------------------------------------
    {
        float drive = macroTracker.effectiveByID(XID::SatMacro, XID::SatDrive) / 100.0f;
        float mix   = macroTracker.effectiveByID(XID::SatMacro, XID::SatMix) / 100.0f;

        if (mix > 0.0005f)
        {
            float driveAmt = juce::jmap(drive, 0.0f, 1.0f, 1.0f, 10.0f);
            float norm = std::tanh(driveAmt);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    float dry = d[n];
                    float wet = std::tanh(dry * driveAmt) / norm;
                    d[n] = dry + (wet - dry) * mix;
                }
            }
        }
    }

    // ---------------------------------------------------------------
    // Shared duck envelope, tracked from the dry (post-saturator) signal.
    // Both the reverb and delay sends read this same envelope, each
    // scaled by its own Duck knob.
    // ---------------------------------------------------------------
    duckEnvBuffer.setSize(1, numSamples, false, false, true);
    {
        float* env = duckEnvBuffer.getWritePointer(0);
        const float attCoef = std::exp(-1.0f / (0.005f * (float) sr));
        const float relCoef = std::exp(-1.0f / (0.300f * (float) sr));
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; ++n)
        {
            float rect = std::abs(0.5f * (l[n] + r[n]));
            float coef = rect > duckEnvShared ? attCoef : relCoef;
            duckEnvShared = coef * duckEnvShared + (1.0f - coef) * rect;
            env[n] = juce::jlimit(0.0f, 1.0f, duckEnvShared * 4.0f);
        }
    }

    // ---------------------------------------------------------------
    // 5) Reverb send (parallel, ducked, mixed back in)
    // ---------------------------------------------------------------
    revBuffer.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        revBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    {
        float sizePct = macroTracker.effectiveByID(XID::RevMacro, XID::RevSize) / 100.0f;
        float mixPct  = macroTracker.effectiveByID(XID::RevMacro, XID::RevMix) / 100.0f;
        float duckPct = macroTracker.effectiveByID(XID::RevMacro, XID::RevDuck) / 100.0f;

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = juce::jlimit(0.0f, 1.0f, sizePct);
        rp.damping    = 0.5f;
        rp.wetLevel   = 1.0f;   // we do the dry/wet mix ourselves below
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);

        juce::dsp::AudioBlock<float> revBlock(revBuffer);
        juce::dsp::ProcessContextReplacing<float> revCtx(revBlock);
        reverb.process(revCtx);

        if (mixPct > 0.0005f)
        {
            const float* env = duckEnvBuffer.getReadPointer(0);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                auto* wet = revBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    dst[n] += wet[n] * (mixPct * (1.0f - duckPct * env[n]));
            }
        }
    }

    // ---------------------------------------------------------------
    // 6) Delay send — manual ping-pong stereo delay w/ cross-feedback
    // ---------------------------------------------------------------
    dlyBuffer.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dlyBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    {
        float timeMs  = macroTracker.effectiveByID(XID::DlyMacro, XID::DlyTime);
        float fbPct   = macroTracker.effectiveByID(XID::DlyMacro, XID::DlyFeedback) / 100.0f;
        float mixPct  = macroTracker.effectiveByID(XID::DlyMacro, XID::DlyMix) / 100.0f;
        float duckPct = macroTracker.effectiveByID(XID::DlyMacro, XID::DlyDuck) / 100.0f;

        float maxDelay = (float) delayL.getMaximumDelayInSamples() - 1.0f;
        float delaySamples = juce::jlimit(1.0f, maxDelay, (timeMs * 0.001f) * (float) sr);
        delayL.setDelay(delaySamples);
        delayR.setDelay(delaySamples);

        auto* inL = dlyBuffer.getReadPointer(0);
        auto* inR = numCh > 1 ? dlyBuffer.getReadPointer(1) : inL;
        auto* outL = dlyBuffer.getWritePointer(0);
        auto* outR = numCh > 1 ? dlyBuffer.getWritePointer(1) : outL;

        for (int n = 0; n < numSamples; ++n)
        {
            float dL = delayL.popSample(0);
            float dR = delayR.popSample(0);
            delayL.pushSample(0, inL[n] + dR * fbPct);
            delayR.pushSample(0, inR[n] + dL * fbPct);
            outL[n] = dL;
            outR[n] = dR;
        }

        if (mixPct > 0.0005f)
        {
            const float* env = duckEnvBuffer.getReadPointer(0);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                auto* wet = dlyBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    dst[n] += wet[n] * (mixPct * (1.0f - duckPct * env[n]));
            }
        }
    }

    // ---------------------------------------------------------------
    // 7) Limiter — input trim + brickwall ceiling
    // ---------------------------------------------------------------
    {
        float ceilingDb = macroTracker.effectiveByID(XID::LimMacro, XID::LimCeiling);
        float inGain    = macroTracker.effectiveByID(XID::LimMacro, XID::LimInputGain);

        buffer.applyGain(juce::Decibels::decibelsToGain(inGain));
        limiter.setThreshold(ceilingDb);
        limiter.process(ctx);
    }

    // ---------------------------------------------------------------
    // 8) Stereo Width — mid/side, not macro-linked (Master utility control)
    // ---------------------------------------------------------------
    if (numCh > 1)
    {
        float widthPct = *apvts.getRawParameterValue(XID::MasterWidth) / 100.0f;
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
    float outGainDb = *apvts.getRawParameterValue(XID::MasterOutGain);
    buffer.applyGain(juce::Decibels::decibelsToGain(outGainDb));
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
