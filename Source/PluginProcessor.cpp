#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float onePoleCoef(float timeMs, double sr)
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.01f, timeMs) * (float) sr));
    }

    // Auto-Tune: nearest MIDI note (as a float, but always an exact integer
    // semitone) to midiNote that belongs to the given Key/Scale. Chromatic
    // (scaleIdx==2) accepts every semitone. Brute-force scan of a +-14
    // semitone window — cheap (<=29*7 comparisons), and simple enough to be
    // obviously correct rather than clever, since this only runs once per
    // sample when a pitch is actually detected.
    inline float nearestScaleNote(float midiNote, int keyIdx, int scaleIdx) noexcept
    {
        int roundedNote = (int) std::round(midiNote);
        if (scaleIdx == 2)
            return (float) roundedNote;

        static const int majorIv[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minorIv[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int* iv = scaleIdx == 0 ? majorIv : minorIv;

        int bestNote = roundedNote;
        float bestDist = 1.0e9f;
        for (int note = roundedNote - 14; note <= roundedNote + 14; ++note)
        {
            int pc = ((note - keyIdx) % 12 + 12) % 12;
            bool inScale = false;
            for (int k = 0; k < 7; ++k)
                if (iv[k] == pc) { inScale = true; break; }
            if (!inScale)
                continue;
            float dist = std::abs((float) note - midiNote);
            if (dist < bestDist) { bestDist = dist; bestNote = note; }
        }
        return (float) bestNote;
    }

    // Levinson-Durbin recursion: from an (order+1)-length autocorrelation
    // sequence r[0..order], returns LPC coefficients aOut[0..order]
    // (aOut[0]=1) and residual prediction error eOut. Reflection
    // coefficients are guaranteed |k|<1 for any real signal with positive
    // energy (r[0]>0), so the resulting synthesis filter 1/A(z) is
    // guaranteed minimum-phase/stable — offline-verified (Python) before
    // this was written in C++; see FormantEnvelope's doc comment in
    // PluginProcessor.h for the three real instabilities that verification
    // caught (none of them in this recursion itself — all downstream, in
    // how the resulting coefficients are analysed/applied).
    inline void levinsonDurbin(const float* r, int order, float* aOut, float& eOut) noexcept
    {
        constexpr int kMaxOrder = 32;   // headroom over XaLZaProcessor::FormantEnvelope::kOrder (24)
        std::array<float, kMaxOrder + 1> a {}, newA {};
        a[0] = 1.0f;
        float e = r[0];
        if (e <= 1.0e-12f)
        {
            aOut[0] = 1.0f;
            for (int i = 1; i <= order; ++i) aOut[i] = 0.0f;
            eOut = 1.0e-6f;
            return;
        }
        for (int i = 1; i <= order; ++i)
        {
            float acc = r[i];
            for (int j = 1; j < i; ++j)
                acc += a[(size_t) j] * r[i - j];
            float k = -acc / e;
            newA = a;
            for (int j = 1; j < i; ++j)
                newA[(size_t) j] = a[(size_t) j] + k * a[(size_t) (i - j)];
            newA[(size_t) i] = k;
            a = newA;
            e *= (1.0f - k * k);
            if (e <= 1.0e-9f) e = 1.0e-9f;
        }
        for (int i = 0; i <= order; ++i)
            aOut[i] = a[(size_t) i];
        eOut = e;
    }
}

float XaLZaProcessor::detectTunePitchHz(double sr) noexcept
{
    constexpr int decWindow = kTuneWindow / kTuneDecimate;   // 1024
    std::array<float, (size_t) decWindow> dec {};
    for (int i = 0; i < decWindow; ++i)
    {
        float sum = 0.0f;
        int base = (tuneAnalysisWritePos + i * kTuneDecimate) % kTuneWindow;
        for (int k = 0; k < kTuneDecimate; ++k)
            sum += tuneAnalysisBuf[(size_t) ((base + k) % kTuneWindow)];
        dec[(size_t) i] = sum / (float) kTuneDecimate;
    }
    // Hann window on the decimated frame — reduces edge artefacts in the
    // autocorrelation (verified offline against known test tones: max
    // error stayed within ~7 cents across the 82Hz-523Hz vocal-ish range
    // tested, well under a semitone).
    for (int i = 0; i < decWindow; ++i)
        dec[(size_t) i] *= 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                              * (float) i / (float) (decWindow - 1));

    double srDec = sr / (double) kTuneDecimate;
    int minLag = juce::jmax(1, (int) (srDec / 1000.0));            // 1000Hz upper bound
    int maxLag = juce::jmin(decWindow - 2, (int) (srDec / 70.0));  // 70Hz lower bound

    auto scoreAtLag = [&] (int lag) -> float
    {
        float num = 0.0f, ea = 0.0f, eb = 0.0f;
        for (int i = 0; i < decWindow - lag; ++i)
        {
            float a = dec[(size_t) i];
            float b = dec[(size_t) (i + lag)];
            num += a * b; ea += a * a; eb += b * b;
        }
        return num / (std::sqrt(ea * eb) + 1.0e-9f);
    };

    float bestScore = -1.0e9f;
    int bestLag = -1;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        float score = scoreAtLag(lag);
        if (score > bestScore) { bestScore = score; bestLag = lag; }
    }

    if (bestLag < 0 || bestScore < 0.5f)   // unvoiced / not a confident periodic signal
        return 0.0f;

    // Parabolic interpolation around bestLag for sub-lag precision.
    float refinedLag = (float) bestLag;
    if (bestLag - 1 >= minLag && bestLag + 1 <= maxLag)
    {
        float sM1 = scoreAtLag(bestLag - 1);
        float sP1 = scoreAtLag(bestLag + 1);
        float denom = sM1 - 2.0f * bestScore + sP1;
        if (std::abs(denom) > 1.0e-9f)
        {
            float d = 0.5f * (sM1 - sP1) / denom;
            refinedLag += juce::jlimit(-1.0f, 1.0f, d);
        }
    }
    return (float) (srDec / (double) refinedLag);
}

void XaLZaProcessor::analyseFormantEnvelope() noexcept
{
    constexpr int order = FormantEnvelope::kOrder;
    tuneFormantEnv.aPrev = tuneFormantEnv.aNew;

    // Not enough real, contiguous audio yet (still within the first
    // kFormantWindow samples after prepare/reset) — a left-zero-padded
    // analysis window has an edge discontinuity that corrupts the LPC
    // estimate (offline-verified: this alone produced an unstable-gain
    // synthesis filter for a warm-up frame). Hold identity (bypass)
    // coefficients until the buffer is fully real.
    if (tuneFormantEnv.primeCount < kFormantWindow)
    {
        tuneFormantEnv.aNew.fill(0.0f);
        tuneFormantEnv.aNew[0] = 1.0f;
        return;
    }

    // Pre-emphasis (flattens the natural -6dB/octave vocal spectral tilt
    // before LPC estimation — offline-verified: without it, a low-pitched
    // harmonic-rich voice reliably produced a spurious +36dB near-DC
    // resonance, an LPC modeling artefact of the tilt rather than a real
    // formant) + Hann window + an RMS gate for frames too quiet to trust.
    constexpr float kPreEmph = 0.97f;
    float prev = 0.0f;
    float energy = 0.0f;
    std::array<float, (size_t) kFormantWindow> pre {};
    for (int i = 0; i < kFormantWindow; ++i)
    {
        float x = tuneAnalysisBuf[(size_t) ((tuneAnalysisWritePos + i) % kTuneWindow)];
        float y = x - kPreEmph * prev;
        prev = x;
        pre[(size_t) i] = y;
        energy += y * y;
    }
    if (std::sqrt(energy / (float) kFormantWindow) < 1.0e-3f)
    {
        tuneFormantEnv.aNew.fill(0.0f);
        tuneFormantEnv.aNew[0] = 1.0f;
        return;
    }

    for (int i = 0; i < kFormantWindow; ++i)
        pre[(size_t) i] *= 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                * (float) i / (float) (kFormantWindow - 1));

    std::array<float, (size_t) order + 1> r {};
    for (int lag = 0; lag <= order; ++lag)
    {
        float sum = 0.0f;
        for (int i = 0; i < kFormantWindow - lag; ++i)
            sum += pre[(size_t) i] * pre[(size_t) (i + lag)];
        r[(size_t) lag] = sum;
    }
    r[0] *= 1.0001f;   // tiny white-noise correction for numerical stability

    std::array<float, (size_t) order + 1> a {};
    float e = 0.0f;
    levinsonDurbin(r.data(), order, a.data(), e);

    // Bandwidth expansion: a'[k] = a[k]*gamma^k pulls every pole slightly
    // toward the origin — cheap insurance against a near-unit-circle pole
    // ringing dangerously (offline-verified: converts "some hops ring
    // into a growing spike" into "always damps out"), imperceptible on
    // the formant peaks themselves.
    constexpr float gamma = 0.994f;
    float gk = 1.0f;
    for (int k = 0; k <= order; ++k)
    {
        tuneFormantEnv.aNew[(size_t) k] = a[(size_t) k] * gk;
        gk *= gamma;
    }
}

float XaLZaProcessor::processFormantPreservedSample(FormantChannelState& st, GranularPitchShifter& shifter,
                                                      float x, float frac, float pitchRatio) noexcept
{
    constexpr int order = FormantEnvelope::kOrder;
    // Per-sample linear interpolation of the coefficients across the
    // current hop, instead of switching once per hop — offline-verified:
    // the whitening (FIR) side below is stable no matter what, but the
    // recursive re-colouring side isn't, and swapping its coefficients
    // abruptly while its own delay line still holds samples computed
    // under the OLD coefficients is a state/coefficient mismatch that can
    // ring a resonance up hard for a few ms before it decays.
    std::array<float, (size_t) order + 1> aC {};
    for (int k = 0; k <= order; ++k)
        aC[(size_t) k] = tuneFormantEnv.aPrev[(size_t) k] * (1.0f - frac) + tuneFormantEnv.aNew[(size_t) k] * frac;

    constexpr float kPreEmph = 0.97f;
    float xPre = x - kPreEmph * st.preState;
    st.preState = x;

    // Whitening (analysis) filter A(z): resid = A(z) * x. Plain FIR —
    // always stable regardless of the coefficient interpolation above.
    float resid = xPre;
    for (int k = 1; k <= order; ++k)
        resid += aC[(size_t) k] * st.xHist[(size_t) (k - 1)];
    for (int k = order - 1; k > 0; --k)
        st.xHist[(size_t) k] = st.xHist[(size_t) (k - 1)];
    st.xHist[0] = xPre;

    float shifted = shifter.processSample(resid, pitchRatio);

    // Re-colouring (synthesis) filter 1/A(z): recursive, using the SAME
    // (original, un-shifted) envelope — this is what puts the formants
    // back after the residual's pitch has moved.
    float yn = shifted;
    for (int k = 1; k <= order; ++k)
        yn -= aC[(size_t) k] * st.yHist[(size_t) (k - 1)];
    for (int k = order - 1; k > 0; --k)
        st.yHist[(size_t) k] = st.yHist[(size_t) (k - 1)];
    st.yHist[0] = yn;   // feedback holds the filter's own true (unclipped) history

    // De-emphasis: restores the natural spectral tilt pre-emphasis removed.
    st.deState = yn + kPreEmph * st.deState;
    float out = st.deState;

    // Last-resort safety soft-clip on the way out only (never fed back
    // into yHist above) — offline-verified that an extreme/already-
    // clipped input could still occasionally produce a large transient
    // even with every fix above; inactive at normal signal levels.
    constexpr float ceiling = 4.0f;
    return ceiling * std::tanh(out / ceiling);
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
      osPreChar(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osSat(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osLimClip(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR),
      osTruePeak(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR)   // factor 2^2 = 4x
{
    for (auto& a : meterDbL) a.store(-100.0f);
    for (auto& a : meterDbR) a.store(-100.0f);
    for (auto& a : rmsDbL) a.store(-100.0f);
    for (auto& a : rmsDbR) a.store(-100.0f);
    for (auto& a : grDb) a.store(0.0f);
    for (auto& a : scopePointsL) a.store(0.0f);
    for (auto& a : scopePointsR) a.store(0.0f);
    for (auto& a : specRing) a.store(0.0f);
    for (auto& a : specRingMaster) a.store(0.0f);
    for (auto& ring : rawRing) for (auto& a : ring) a.store(0.0f);
    for (auto& a : rawWritePos) a.store(0);
    for (auto& a : dblScopeL) a.store(0.0f);
    for (auto& a : dblScopeR) a.store(0.0f);
    for (int i = 0; i < kNumSlots; ++i) chainOrder[(size_t) i].store(i);
    irFormatManager.registerBasicFormats();
}

void XaLZaProcessor::updateMeter(int tap, const juce::AudioBuffer<float>& buf, int numSamples, int numCh)
{
    float peakL = 0.0f, peakR = 0.0f;
    double sumSqL = 0.0, sumSqR = 0.0;
    auto* l = buf.getReadPointer(0);
    for (int n = 0; n < numSamples; ++n)
    {
        peakL = juce::jmax(peakL, std::abs(l[n]));
        sumSqL += (double) l[n] * (double) l[n];
    }
    if (numCh > 1)
    {
        auto* r = buf.getReadPointer(1);
        for (int n = 0; n < numSamples; ++n)
        {
            peakR = juce::jmax(peakR, std::abs(r[n]));
            sumSqR += (double) r[n] * (double) r[n];
        }
    }
    else
    {
        peakR = peakL;
        sumSqR = sumSqL;
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

    // Real RMS: genuine mean-square over this block, not a derived copy
    // of the peak reading above — converted to dB and integrated
    // symmetrically (see rmsCoef, set in prepareToPlay) like a real VU
    // instrument, so it reads "how loud does this actually sound" rather
    // than chasing the same fast transients the peak ballistics do.
    float rmsGainL = std::sqrt((float) (sumSqL / juce::jmax(1, numSamples)));
    float rmsGainR = std::sqrt((float) (sumSqR / juce::jmax(1, numSamples)));
    float rmsTargetL = juce::Decibels::gainToDecibels(rmsGainL, -100.0f);
    float rmsTargetR = juce::Decibels::gainToDecibels(rmsGainR, -100.0f);
    auto smoothRms = [this] (std::atomic<float>& state, float target)
    {
        float prev = state.load(std::memory_order_relaxed);
        state.store(rmsCoef * prev + (1.0f - rmsCoef) * target, std::memory_order_relaxed);
    };
    smoothRms(rmsDbL[(size_t) tap], rmsTargetL);
    smoothRms(rmsDbR[(size_t) tap], rmsTargetR);
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

bool XaLZaProcessor::loadImpulseResponseFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(irFormatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;   // unsupported/corrupt file — previous IR (if any) stays active

    // 30s hard sanity cap — generous for any real reverb/hall/plate IR,
    // just a ceiling against accidentally selecting a huge music file.
    auto numSamplesIr = (int) juce::jmin((juce::int64) reader->lengthInSamples,
                                          (juce::int64) (reader->sampleRate * 30.0));
    if (numSamplesIr <= 0)
        return false;

    juce::AudioBuffer<float> irBuf((int) juce::jlimit((juce::int64) 1, (juce::int64) 2, (juce::int64) reader->numChannels),
                                    numSamplesIr);
    reader->read(&irBuf, 0, numSamplesIr, 0, true, true);

    currentIrFile = file;
    irLoaded = true;
    irTransfer.set({ std::move(irBuf), reader->sampleRate });
    return true;
}

void XaLZaProcessor::clearImpulseResponse()
{
    currentIrFile = juce::File();
    irLoaded = false;
    // A silent 2-sample buffer effectively mutes the convolution path —
    // RevHybrid still works, it just blends toward silence until a new
    // IR is loaded.
    juce::AudioBuffer<float> silence(2, 2);
    silence.clear();
    irTransfer.set({ std::move(silence), sr > 0.0 ? sr : 44100.0 });
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
    for (auto& n : resNotch) n.prepare(spec);
    satTone.prepare(spec);
    reverb.prepare(spec);
    revWetHpf.prepare(spec);
    revWetLpf.prepare(spec);
    revConvolution.prepare(spec);

    essDetectL.prepare(spec);
    essDetectR.prepare(spec);
    for (int b = 0; b < kMaxResBands; ++b)
    {
        resDetectL[b].prepare(spec);
        resDetectR[b].prepare(spec);
        resEnv[b] = 0.0f;
        resCutSmoothed[b] = 0.0f;
        resCutDbPerBandUI[b].store(0.0f, std::memory_order_relaxed);
    }

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

    gateLaSamples = juce::jmax(1, (int) std::round(0.005 * sampleRate));
    gateLaRingSize = (int) juce::nextPowerOfTwo(gateLaSamples + samplesPerBlock + 64);
    gateLaRingMask = gateLaRingSize - 1;
    gateLaRing.setSize(2, gateLaRingSize, false, true, true);
    gateLaWritePos = 0;
    gateLaWasEnabled = false;

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
    for (auto& v : dblVoiceDelay)
    {
        v.prepare(monoSpec);
        v.setMaximumDelayInSamples((int) (sampleRate * 0.5));
        v.reset();
    }

    revPreDelayL.prepare(monoSpec);
    revPreDelayR.prepare(monoSpec);
    revPreDelayL.setMaximumDelayInSamples((int) (sampleRate * 0.2));
    revPreDelayR.setMaximumDelayInSamples((int) (sampleRate * 0.2));
    revPreDelayL.reset();
    revPreDelayR.reset();

    revDiffuserL.prepare(monoSpec);
    revDiffuserR.prepare(monoSpec);
    revDiffuserL.reset();
    revDiffuserR.reset();

    tuneShifterL.prepare(sampleRate);
    tuneShifterR.prepare(sampleRate);
    tuneShifterL.reset();
    tuneShifterR.reset();
    tuneAnalysisBuf.fill(0.0f);
    tuneAnalysisWritePos = 0;
    tuneHopCounter = 0;
    tuneDetectedHz = 0.0f;
    tuneSmoothedRatio = 1.0f;
    tuneFormantEnv.reset();
    tuneFormantL.reset();
    tuneFormantR.reset();

    trsFast.reset();
    trsSlow.reset();
    trsGainSmoothed = 1.0f;

    excHpf.prepare(spec);
    excHpf.reset();

    delayL.prepare(monoSpec);
    delayR.prepare(monoSpec);
    delayL.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayR.setMaximumDelayInSamples((int) (sampleRate * 2.0));
    delayL.reset();
    delayR.reset();

    dlyPreDelayL.prepare(monoSpec);
    dlyPreDelayR.prepare(monoSpec);
    dlyPreDelayL.setMaximumDelayInSamples((int) (sampleRate * 1.0));
    dlyPreDelayR.setMaximumDelayInSamples((int) (sampleRate * 1.0));
    dlyPreDelayL.reset();
    dlyPreDelayR.reset();

    dlyFbHpfL.prepare(monoSpec); dlyFbHpfR.prepare(monoSpec);
    dlyFbLpfL.prepare(monoSpec); dlyFbLpfR.prepare(monoSpec);
    dlyFbHpfL.reset(); dlyFbHpfR.reset(); dlyFbLpfL.reset(); dlyFbLpfR.reset();

    dryBuffer.setSize(2, samplesPerBlock, false, false, true);
    revBuffer.setSize(2, samplesPerBlock, false, false, true);
    revConvBuffer.setSize(2, samplesPerBlock, false, false, true);
    dlyBuffer.setSize(2, samplesPerBlock, false, false, true);
    dblBuffer.setSize(2, samplesPerBlock, false, false, true);

    gateEnv = 0.0f;
    gateGain = 1.0f;
    gateHoldCounter = 0;
    essEnv = 0.0f;
    essGainDb = 0.0f;
    revDuckEnv = 0.0f;
    dlyDuckEnv = 0.0f;
    dblVoicePhase.fill(0.0f);
    dlyPanPhase = 0.0f;

    meterAttCoef = onePoleCoef(1.0f, sampleRate);
    meterRelCoef = onePoleCoef(130.0f, sampleRate);   // fast, real-time feel — was 400ms
    // RMS companion reading: symmetric ~300ms integration both directions
    // (a real VU-style average, not fast-attack/slow-release like the
    // peak ballistics above), so it settles to "how loud does this
    // actually sound" rather than chasing the same transients Peak does.
    rmsCoef = onePoleCoef(300.0f, sampleRate);
}

void XaLZaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi);   // no MIDI-driven parameters (see acceptsMidi())

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    if (numCh <= 0 || numSamples <= 0)
        return;

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
        float hpfHz = juce::jlimit(20.0f, 500.0f, apvts.getRawParameterValue(XID::PreHPF)->load());
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

        applySmoothedGainDb(preGainSmoothed, buffer, apvts.getRawParameterValue(XID::PreGain)->load(), numSamples);

        float charAmt = apvts.getRawParameterValue(XID::PreChar)->load() / 100.0f;
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
        float threshDb = apvts.getRawParameterValue(XID::GateThresh)->load();
        float rangeDb   = apvts.getRawParameterValue(XID::GateRange)->load();
        float attackMs  = apvts.getRawParameterValue(XID::GateAttack)->load();
        float holdMs    = apvts.getRawParameterValue(XID::GateHold)->load();
        float releaseMs = apvts.getRawParameterValue(XID::GateRelease)->load();

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

        bool gateLookahead = apvts.getRawParameterValue(XID::GateLookahead)->load() > 0.5f;
        setLatencySamples((int) std::round(osPreChar.getLatencyInSamples()
                                            + osSat.getLatencyInSamples()
                                            + osLimClip.getLatencyInSamples())
                           + limLookaheadSamples + (gateLookahead ? gateLaSamples : 0));
        if (gateLookahead != gateLaWasEnabled)
        {
            // Toggling mid-stream would otherwise briefly play back stale
            // ring content — clear it so lookahead always starts clean.
            gateLaRing.clear();
            gateLaWritePos = 0;
            gateLaWasEnabled = gateLookahead;
        }

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

            if (gateLookahead)
            {
                float rawL = l[n], rawR = numCh > 1 ? r[n] : rawL;
                int wp = gateLaWritePos & gateLaRingMask;
                gateLaRing.setSample(0, wp, rawL);
                gateLaRing.setSample(1, wp, rawR);
                int rp = (gateLaWritePos - gateLaSamples) & gateLaRingMask;
                float dl = gateLaRing.getSample(0, rp);
                float dr = gateLaRing.getSample(1, rp);
                ++gateLaWritePos;

                l[n] = dl * applied;
                if (numCh > 1) r[n] = dr * applied;
            }
            else
            {
                l[n] *= applied;
                if (numCh > 1) r[n] *= applied;
            }
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
    // 2b) AUTO-TUNE — real autocorrelation pitch detection (decimated,
    // once per kTuneHop samples — see detectTunePitchHz) driving a
    // granular pitch shifter (GranularPitchShifter, PluginProcessor.h)
    // that pulls the detected pitch toward the nearest Key/Scale note.
    // Only ONE processed signal path — Amount blends the correction
    // RATIO toward 1.0, never a dry+shifted audio mix (which would
    // phase/flange, since the two copies aren't phase-aligned).
    // ---------------------------------------------------------------
    auto runTune = [&]()
    {
    bool tuneBypassed = apvts.getRawParameterValue(XID::TuneBypass)->load() > 0.5f;
    if (!tuneBypassed)
    {
        int keyIdx    = juce::jlimit(0, 11, (int) std::round(apvts.getRawParameterValue(XID::TuneKey)->load()));
        int scaleIdx  = juce::jlimit(0, 2, (int) std::round(apvts.getRawParameterValue(XID::TuneScale)->load()));
        float retuneMs  = apvts.getRawParameterValue(XID::TuneRetune)->load();
        float amountPct = apvts.getRawParameterValue(XID::TuneAmount)->load() / 100.0f;

        // 0ms retune -> snap almost instantly (the classic hard-tune/
        // robotic urban-vocal sound); higher -> an audibly natural glide.
        float retuneCoef = onePoleCoef(juce::jmax(1.0f, retuneMs), sr);
        bool formantOn = apvts.getRawParameterValue(XID::TuneFormant)->load() > 0.5f;

        auto* l = buffer.getWritePointer(0);
        auto* r = numCh > 1 ? buffer.getWritePointer(1) : l;

        for (int n = 0; n < numSamples; ++n)
        {
            float mono = numCh > 1 ? 0.5f * (l[n] + r[n]) : l[n];

            tuneAnalysisBuf[(size_t) tuneAnalysisWritePos] = mono;
            tuneAnalysisWritePos = (tuneAnalysisWritePos + 1) % kTuneWindow;
            if (tuneFormantEnv.primeCount < kFormantWindow)
                ++tuneFormantEnv.primeCount;
            if (++tuneHopCounter >= kTuneHop)
            {
                tuneHopCounter = 0;
                tuneDetectedHz = detectTunePitchHz(sr);
                tuneDetectedHzUI.store(tuneDetectedHz, std::memory_order_relaxed);
                if (formantOn)
                    analyseFormantEnvelope();
            }

            float targetRatio = 1.0f;
            float targetHz = 0.0f;
            if (tuneDetectedHz > 0.0f)
            {
                float midiNote = 69.0f + 12.0f * std::log2(tuneDetectedHz / 440.0f);
                float nearestMidi = nearestScaleNote(midiNote, keyIdx, scaleIdx);
                float fullRatio = std::pow(2.0f, (nearestMidi - midiNote) / 12.0f);
                targetRatio = 1.0f + (fullRatio - 1.0f) * amountPct;
                targetHz = tuneDetectedHz * fullRatio;
            }
            tuneSmoothedRatio = retuneCoef * tuneSmoothedRatio + (1.0f - retuneCoef) * targetRatio;
            if ((n & 63) == 0)
                tuneTargetHzUI.store(targetHz, std::memory_order_relaxed);

            if (formantOn)
            {
                float frac = (float) tuneHopCounter / (float) kTuneHop;
                l[n] = processFormantPreservedSample(tuneFormantL, tuneShifterL, l[n], frac, tuneSmoothedRatio);
                if (numCh > 1) r[n] = processFormantPreservedSample(tuneFormantR, tuneShifterR, r[n], frac, tuneSmoothedRatio);
            }
            else
            {
                l[n] = tuneShifterL.processSample(l[n], tuneSmoothedRatio);
                if (numCh > 1) r[n] = tuneShifterR.processSample(r[n], tuneSmoothedRatio);
            }
        }
    }
    else
    {
        tuneDetectedHzUI.store(0.0f, std::memory_order_relaxed);
        tuneTargetHzUI.store(0.0f, std::memory_order_relaxed);
    }
    updateMeter((int) TapTune, buffer, numSamples, numCh);
    pushRaw((int) RawTune, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 3) DE-ESSER — dynamic peak filter driven by a sibilance-band envelope
    // ---------------------------------------------------------------
    auto runEss = [&]()
    {
    bool essBypassed = apvts.getRawParameterValue(XID::EssBypass)->load() > 0.5f;
    if (!essBypassed)
    {
        float threshDb  = apvts.getRawParameterValue(XID::EssThresh)->load();
        float rangeDb   = apvts.getRawParameterValue(XID::EssRange)->load();   // negative, e.g. -8dB
        float freqHz    = apvts.getRawParameterValue(XID::EssFreq)->load();

        // Band (mockup's essBandSegs S/T/CH): a real detection-character
        // change, not a relabel — S keeps a narrow, high-Q band right on
        // the set frequency (sharp sibilance); T biases lower and widens
        // slightly (dental transients); CH biases lower still and widens
        // the most (broad "ch/sh" energy).
        int bandMode = (int) std::round(apvts.getRawParameterValue(XID::EssBand)->load());
        float bandQMult = bandMode == 0 ? 1.5f : (bandMode == 2 ? 0.55f : 1.0f);
        float bandFreqMult = bandMode == 0 ? 1.0f : (bandMode == 2 ? 0.7f : 0.85f);
        float bandFreq = juce::jlimit(1000.0f, 16000.0f, freqHz * bandFreqMult);

        auto detCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, bandFreq, 3.0f * bandQMult);
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
                sr, bandFreq, 2.5f * bandQMult,
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
    pushRaw((int) RawEss, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 3b) TRANSIENT SHAPER — linked dual envelope-follower attack/sustain
    // reshaping (see the EnvFollower/trsFast/trsSlow doc comment in
    // PluginProcessor.h for the detector design). Offline-verified
    // (Python) against a synthetic percussive hit before being written
    // here: +100% attack raised the transient peak ~4x (+12dB cap),
    // -100% attack cut it to ~1/4, +-100% sustain raised/lowered the tail
    // RMS in the same direction, and both knobs at 0 measured as exactly
    // unity gain (bit-identical passthrough) across the whole test signal.
    // ---------------------------------------------------------------
    auto runTrs = [&]()
    {
    bool trsBypassed = apvts.getRawParameterValue(XID::TrsBypass)->load() > 0.5f;
    if (!trsBypassed)
    {
        float attackPct  = apvts.getRawParameterValue(XID::TrsAttack)->load();
        float sustainPct = apvts.getRawParameterValue(XID::TrsSustain)->load();

        if (attackPct != 0.0f || sustainPct != 0.0f)
        {
            trsFast.setTimes(1.0f, 5.0f, sr);
            trsSlow.setTimes(25.0f, 200.0f, sr);
            float gainSmoothCoef = onePoleCoef(0.5f, sr);
            constexpr float capDb = 18.0f;
            float atkGainMaxDb = (attackPct / 100.0f) * 12.0f;
            float susGainMaxDb = (sustainPct / 100.0f) * 12.0f;

            auto* l = buffer.getWritePointer(0);
            auto* r = numCh > 1 ? buffer.getWritePointer(1) : l;
            for (int n = 0; n < numSamples; ++n)
            {
                float linked = numCh > 1 ? juce::jmax(std::abs(l[n]), std::abs(r[n])) : std::abs(l[n]);
                float fast = trsFast.process(linked);
                float slow = trsSlow.process(linked);
                float diffDb = 20.0f * std::log10((fast + 1.0e-6f) / (slow + 1.0e-6f));
                float atkTerm = juce::jlimit(0.0f, capDb, diffDb) / capDb;
                float susTerm = juce::jlimit(0.0f, capDb, -diffDb) / capDb;
                float gainDb = atkGainMaxDb * atkTerm + susGainMaxDb * susTerm;
                float targetGainLin = juce::Decibels::decibelsToGain(gainDb);
                trsGainSmoothed = gainSmoothCoef * trsGainSmoothed + (1.0f - gainSmoothCoef) * targetGainLin;

                l[n] *= trsGainSmoothed;
                if (numCh > 1) r[n] *= trsGainSmoothed;
            }
        }
    }
    updateMeter((int) TapTrs, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 4) GLUE COMP — threshold/ratio via juce::dsp, makeup + dry/wet mix
    // ---------------------------------------------------------------
    auto runComp = [&]()
    {
    bool compBypassed = apvts.getRawParameterValue(XID::CompBypass)->load() > 0.5f;
    if (!compBypassed)
    {
        float thresh = apvts.getRawParameterValue(XID::CompThresh)->load();
        float ratio  = juce::jmax(1.0f, apvts.getRawParameterValue(XID::CompRatio)->load());
        float makeup = apvts.getRawParameterValue(XID::CompMakeup)->load();
        float attackMs = apvts.getRawParameterValue(XID::CompAttack)->load();
        float releaseMs = apvts.getRawParameterValue(XID::CompRelease)->load();
        float mixAmt = apvts.getRawParameterValue(XID::CompMix)->load() / 100.0f;

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
        float reduction = apvts.getRawParameterValue(XID::OptoReduction)->load() / 100.0f;
        float gainDb    = apvts.getRawParameterValue(XID::OptoGain)->load();
        float mixAmt    = apvts.getRawParameterValue(XID::OptoMix)->load() / 100.0f;
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
        float lowDb   = apvts.getRawParameterValue(XID::EqLow)->load();
        float midDb   = apvts.getRawParameterValue(XID::EqMid)->load();
        float highDb  = apvts.getRawParameterValue(XID::EqHigh)->load();
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
        float amount     = apvts.getRawParameterValue(XID::ResAmount)->load() / 100.0f;
        float sharpness  = apvts.getRawParameterValue(XID::ResSharpness)->load() / 100.0f;
        float notchLimit = apvts.getRawParameterValue(XID::ResNotchLimit)->load();
        float reactivity = apvts.getRawParameterValue(XID::ResReactivity)->load() / 100.0f;
        float lowHz      = apvts.getRawParameterValue(XID::ResLow)->load();
        float highHz     = apvts.getRawParameterValue(XID::ResHigh)->load();

        // Style: real Q / detection-bandwidth scaling per band, not a
        // relabel — Delicate narrows both (surgical, less collateral
        // damage to neighbouring harmonics), Wide broadens both (catches
        // more spread-out resonance clusters at the cost of precision).
        int styleMode = (int) std::round(apvts.getRawParameterValue(XID::ResStyle)->load());
        float styleQMult = styleMode == 0 ? 1.6f : (styleMode == 2 ? 0.55f : 1.0f);

        int numBands = juce::jlimit(1, kMaxResBands,
                            (int) std::round(apvts.getRawParameterValue(XID::ResBands)->load()));

        float attMs = juce::jmap(reactivity, 0.0f, 1.0f, 25.0f, 1.5f);
        float relMs = juce::jmap(reactivity, 0.0f, 1.0f, 300.0f, 25.0f);
        float detAtt = onePoleCoef(attMs, sr);
        float detRel = onePoleCoef(relMs, sr);
        float cutSmoothCoef = onePoleCoef(juce::jmax(3.0f, relMs * 0.5f), sr);

        // More bands split the same overall Amount/NotchLimit budget so
        // stacking bands doesn't multiply the total cut applied.
        float perBandBudget = 1.0f / std::sqrt((float) numBands);

        float logLow = std::log(juce::jmax(20.0f, lowHz)), logHigh = std::log(juce::jmax(logLow + 1.0f, highHz));
        float worstCutDb = 0.0f;

        for (int b = 0; b < numBands; ++b)
        {
            float t0 = (float) b / (float) numBands, t1 = (float) (b + 1) / (float) numBands;
            float bandLow = std::exp(logLow + (logHigh - logLow) * t0);
            float bandHigh = std::exp(logLow + (logHigh - logLow) * t1);
            float freq = juce::jlimit(40.0f, 18000.0f, std::sqrt(bandLow * bandHigh));
            float q = juce::jmap(sharpness, 0.0f, 1.0f, 0.5f, 8.0f) * styleQMult;
            float bandQ = juce::jlimit(0.3f, 6.0f, (freq / juce::jmax(20.0f, bandHigh - bandLow)) * styleQMult);

            auto detCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freq, bandQ);
            *resDetectL[b].coefficients = *detCoeffs;
            *resDetectR[b].coefficients = *detCoeffs;

            auto* l = buffer.getReadPointer(0);
            auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
            float cutDb = resCutSmoothed[b];
            for (int n = 0; n < numSamples; ++n)
            {
                float fl = resDetectL[b].processSample(l[n]);
                float fr = numCh > 1 ? resDetectR[b].processSample(r[n]) : fl;
                float rect = std::abs(0.5f * (fl + fr));
                float dCoef = rect > resEnv[b] ? detAtt : detRel;
                resEnv[b] = dCoef * resEnv[b] + (1.0f - dCoef) * rect;

                float envDb = juce::Decibels::gainToDecibels(resEnv[b], -100.0f);
                float depthNorm = juce::jlimit(0.0f, 1.0f, (envDb + 40.0f) / 34.0f);   // -40..-6dB band-energy window
                float targetCut = notchLimit * amount * depthNorm * perBandBudget;
                resCutSmoothed[b] = cutSmoothCoef * resCutSmoothed[b] + (1.0f - cutSmoothCoef) * targetCut;
                cutDb = resCutSmoothed[b];
            }
            worstCutDb = juce::jmin(worstCutDb, cutDb);
            resCutDbPerBandUI[b].store(cutDb, std::memory_order_relaxed);

            *resNotch[b].state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, freq, q, juce::Decibels::decibelsToGain(cutDb));
            resNotch[b].process(ctx);
        }
        resCutDbUI.store(worstCutDb, std::memory_order_relaxed);

        for (int b = numBands; b < kMaxResBands; ++b)
        {
            resEnv[b] = 0.0f;
            resCutSmoothed[b] = 0.0f;
            resCutDbPerBandUI[b].store(0.0f, std::memory_order_relaxed);
        }
    }
    else
    {
        for (int b = 0; b < kMaxResBands; ++b)
        {
            resEnv[b] = 0.0f;
            resCutSmoothed[b] = 0.0f;
            resCutDbPerBandUI[b].store(0.0f, std::memory_order_relaxed);
        }
        resCutDbUI.store(0.0f, std::memory_order_relaxed);
    }
    updateMeter((int) TapRes, buffer, numSamples, numCh);
    pushRaw((int) RawRes, buffer, numSamples, numCh);
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
        float drive   = apvts.getRawParameterValue(XID::SatDrive)->load() / 100.0f;
        float toneDb  = apvts.getRawParameterValue(XID::SatTone)->load();
        float ceilDb  = apvts.getRawParameterValue(XID::SatCeiling)->load();
        float mixAmt  = apvts.getRawParameterValue(XID::SatMix)->load() / 100.0f;

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
    // 8b) EXCITER — harmonic enhancer. Isolates the band above Tone's
    // crossover (excHpf), drives ONLY that band through an asymmetric
    // soft clip (tanh of a signal plus a small squared term, so it
    // generates both even and odd harmonics rather than just odd like a
    // symmetric clipper), and mixes the result back on top of the dry
    // signal. Offline-verified (Python): bit-identical to dry at
    // Drive=0/Mix=0; real 2nd/3rd-harmonic energy appears above the
    // crossover at Drive=100; content well below the crossover is
    // essentially untouched; peak output stayed within ~0.01 of a loud
    // input's own peak even at Drive=100/Mix=100 (no makeup-gain
    // restoration on purpose — see the excHpf doc comment in
    // PluginProcessor.h).
    // ---------------------------------------------------------------
    auto runExc = [&]()
    {
    bool excBypassed = apvts.getRawParameterValue(XID::ExcBypass)->load() > 0.5f;
    if (!excBypassed)
    {
        float drivePct = apvts.getRawParameterValue(XID::ExcDrive)->load();
        float tonePct  = apvts.getRawParameterValue(XID::ExcTone)->load();
        float mixAmt   = apvts.getRawParameterValue(XID::ExcMix)->load() / 100.0f;

        if (mixAmt > 0.0005f)
        {
            for (int ch = 0; ch < numCh; ++ch)
                dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

            float cutoffHz = juce::jmap(tonePct, 0.0f, 100.0f, 1500.0f, 6000.0f);
            *excHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, cutoffHz, 0.707f);
            excHpf.process(ctx);   // buffer now holds the isolated high band

            float driveAmt = juce::jmap(drivePct, 0.0f, 100.0f, 1.0f, 10.0f);
            bool driveOn = drivePct > 0.0005f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* wet = buffer.getWritePointer(ch);
                auto* dry = dryBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                {
                    float hi = wet[n];
                    float harm = hi;
                    if (driveOn)
                    {
                        float driven = driveAmt * hi + 0.15f * driveAmt * hi * hi;
                        harm = std::tanh(driven) / driveAmt;
                    }
                    wet[n] = dry[n] + mixAmt * harm;
                }
            }
        }
    }
    updateMeter((int) TapExc, buffer, numSamples, numCh);
    };

    // ---------------------------------------------------------------
    // 9) DOUBLER — two modulated delay voices layered on top of the dry signal
    // ---------------------------------------------------------------
    auto runDbl = [&]()
    {
    bool dblBypassed = apvts.getRawParameterValue(XID::DblBypass)->load() > 0.5f;
    if (!dblBypassed)
    {
        float detuneAmt = apvts.getRawParameterValue(XID::DblDetune)->load();
        float widthPct  = apvts.getRawParameterValue(XID::DblWidth)->load() / 100.0f;
        float delayMs   = apvts.getRawParameterValue(XID::DblDelay)->load();
        float mixAmt    = apvts.getRawParameterValue(XID::DblMix)->load() / 100.0f;
        int   numVoices = juce::jlimit(2, DblVoiceConfig::kMaxVoices,
                              2 * (int) std::round(apvts.getRawParameterValue(XID::DblVoices)->load() / 2.0f));

        if (mixAmt > 0.0005f)
        {
            float modDepth = juce::jmap(detuneAmt, 0.0f, 40.0f, 0.0f, 6.0f);
            // Loudness stays roughly flat as voices are added — normalise by
            // the same reference the original 2-voice version implicitly used.
            float voiceGain = 1.0f / std::sqrt((float) numVoices * 0.5f);

            float baseSamples[DblVoiceConfig::kMaxVoices];
            float w[DblVoiceConfig::kMaxVoices];
            float gainL[DblVoiceConfig::kMaxVoices], gainR[DblVoiceConfig::kMaxVoices];
            for (int v = 0; v < numVoices; ++v)
            {
                baseSamples[v] = juce::jmax(1.0f, (delayMs + DblVoiceConfig::delayOffsetMs[v]) * 0.001f * (float) sr);
                w[v] = 2.0f * juce::MathConstants<float>::pi * DblVoiceConfig::rateHz[v] / (float) sr;
                float effPan = juce::jlimit(-1.0f, 1.0f, DblVoiceConfig::panPos[v] * widthPct);
                float angle = (effPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f; // 0..pi/2
                gainL[v] = std::cos(angle);
                gainR[v] = std::sin(angle);
            }

            auto* inL = buffer.getReadPointer(0);
            auto* inR = numCh > 1 ? buffer.getReadPointer(1) : inL;
            auto* outL = dblBuffer.getWritePointer(0);
            auto* outR = dblBuffer.getWritePointer(1);

            for (int n = 0; n < numSamples; ++n)
            {
                float monoIn = 0.5f * (inL[n] + inR[n]);
                float accL = 0.0f, accR = 0.0f;

                for (int v = 0; v < numVoices; ++v)
                {
                    dblVoicePhase[(size_t) v] += w[v];
                    if (dblVoicePhase[(size_t) v] > juce::MathConstants<float>::twoPi)
                        dblVoicePhase[(size_t) v] -= juce::MathConstants<float>::twoPi;

                    dblVoiceDelay[(size_t) v].setDelay(juce::jmax(1.0f, baseSamples[v] + modDepth * std::sin(dblVoicePhase[(size_t) v])));
                    float vs = dblVoiceDelay[(size_t) v].popSample(0);
                    dblVoiceDelay[(size_t) v].pushSample(0, monoIn);

                    accL += vs * gainL[v];
                    accR += vs * gainR[v];
                }

                outL[n] = accL * voiceGain;
                outR[n] = accR * voiceGain;
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
    // Drain any pending "Load IR" request (real-time safe, non-blocking —
    // see loadImpulseResponseFile()'s comment) even when Reverb is
    // currently bypassed, so a queued Load IR click never has to wait for
    // the module to be re-enabled to take effect.
    irTransfer.get([this] (IrBufferWithRate& buf)
    {
        revConvolution.loadImpulseResponse(std::move(buf.buffer), buf.sampleRate,
                                            juce::dsp::Convolution::Stereo::yes,
                                            juce::dsp::Convolution::Trim::yes,
                                            juce::dsp::Convolution::Normalise::yes);
    });

    bool revBypassed = apvts.getRawParameterValue(XID::RevBypass)->load() > 0.5f;
    if (!revBypassed)
    {
        float sizePct    = apvts.getRawParameterValue(XID::RevSize)->load() / 100.0f;
        float decaySec   = apvts.getRawParameterValue(XID::RevDecay)->load();
        float preDelayMs = apvts.getRawParameterValue(XID::RevPreDelay)->load();
        float mixPct     = apvts.getRawParameterValue(XID::RevMix)->load() / 100.0f;
        float duckPct    = apvts.getRawParameterValue(XID::RevDuck)->load() / 100.0f;
        float duckRelMs  = apvts.getRawParameterValue(XID::RevDuckRelease)->load();
        // Independent damping trim, centred at 50 = no change from the
        // original Decay-derived formula (so existing sessions/defaults
        // sound identical) — real extra control either side of that.
        float dampingTrimPct = apvts.getRawParameterValue(XID::RevDamping)->load();
        // Hybrid blend: 0 = pure algorithmic (unchanged), 100 = pure
        // loaded-impulse convolution, in between a genuine crossfade of
        // both engines' wet signal.
        float hybridPct = apvts.getRawParameterValue(XID::RevHybrid)->load() / 100.0f;
        float widthPct  = apvts.getRawParameterValue(XID::RevWidth)->load() / 100.0f;
        bool  freezeOn  = apvts.getRawParameterValue(XID::RevFreeze)->load() > 0.5f;

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

        // Both engines process the SAME pre-delayed dry signal, in
        // parallel — the standard hybrid-reverb architecture. Snapshot it
        // into revConvBuffer for the convolution engine BEFORE the
        // algorithmic reverb below turns revBuffer into wet audio in
        // place; skipped entirely (no copy, no convolution CPU cost) when
        // Hybrid is fully off, which is the default/common case.
        bool useConv = hybridPct > 0.0005f;
        if (useConv)
        {
            revConvBuffer.setSize(numCh, numSamples, false, false, true);
            for (int ch = 0; ch < numCh; ++ch)
                revConvBuffer.copyFrom(ch, 0, revBuffer, ch, 0, numSamples);
        }

        // Input diffusion — see AllpassDiffuser's comment. Runs on the
        // pre-delayed dry signal that's about to enter the algorithmic
        // engine only (the convolution snapshot above was already taken
        // from the clean pre-delayed signal, since a loaded IR brings its
        // own real diffusion from the room it was captured in).
        {
            auto* dL = revBuffer.getWritePointer(0);
            auto* dR = numCh > 1 ? revBuffer.getWritePointer(1) : dL;
            for (int n = 0; n < numSamples; ++n)
            {
                dL[n] = revDiffuserL.processSample(dL[n]);
                if (numCh > 1)
                    dR[n] = revDiffuserR.processSample(dR[n]);
            }
        }

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = juce::jlimit(0.0f, 1.0f, sizePct);
        rp.damping    = juce::jlimit(0.05f, 0.95f,
                            juce::jmap(decaySec, 0.3f, 8.0f, 0.9f, 0.1f)
                            + (dampingTrimPct - 50.0f) / 50.0f * 0.3f);
        rp.wetLevel   = 1.0f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        // Real freeze — JUCE's own engine puts itself into a continuous
        // feedback loop at freezeMode>=0.5, giving genuine infinite sustain
        // for pad/ambient use, rather than the previously-hardcoded off.
        rp.freezeMode = freezeOn ? 1.0f : 0.0f;
        reverb.setParameters(rp);

        juce::dsp::AudioBlock<float> revBlock(revBuffer);
        juce::dsp::ProcessContextReplacing<float> revCtx(revBlock);
        reverb.process(revCtx);

        if (useConv)
        {
            juce::dsp::AudioBlock<float> revConvBlock(revConvBuffer);
            juce::dsp::ProcessContextReplacing<float> revConvCtx(revConvBlock);
            revConvolution.process(revConvCtx);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* algo = revBuffer.getWritePointer(ch);
                auto* conv = revConvBuffer.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    algo[n] = algo[n] * (1.0f - hybridPct) + conv[n] * hybridPct;
            }
        }

        // Wet-only tone shaping — a genuine user-facing filter pair on the
        // tail, separate from the reverb's own internal room-size/damping
        // model, so you can clean up boom or tame harshness independently.
        float wetHpfHz = apvts.getRawParameterValue(XID::RevWetHpf)->load();
        float wetLpfHz = apvts.getRawParameterValue(XID::RevWetLpf)->load();
        *revWetHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, juce::jmax(1.0f, wetHpfHz));
        *revWetLpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, juce::jmax(20.0f, wetLpfHz));
        revWetHpf.process(revCtx);
        revWetLpf.process(revCtx);

        // Real M/S wet-tail width — independent of the engine's own fixed
        // internal spread (rp.width stays at its normal 1.0 above), same
        // formula as MasterWidth at the bus level, applied to just the
        // reverb's own wet buffer. 100% = bit-identical to prior behaviour.
        if (numCh > 1 && std::abs(widthPct - 1.0f) > 0.0005f)
        {
            auto* wl = revBuffer.getWritePointer(0);
            auto* wr = revBuffer.getWritePointer(1);
            for (int n = 0; n < numSamples; ++n)
            {
                float mid  = 0.5f * (wl[n] + wr[n]);
                float side = 0.5f * (wl[n] - wr[n]) * widthPct;
                wl[n] = mid + side;
                wr[n] = mid - side;
            }
        }

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
        // Real host tempo/playhead sync: queried live each block, not
        // cached — a genuine tempo change (or a host that only starts
        // reporting BPM once transport is rolling) is reflected immediately.
        // Falls back to 120 BPM whenever the host doesn't report tempo
        // (never crashes, never leaves the delay silently stuck).
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        bpm = juce::jlimit(20.0, 300.0, bpm);
        float wholeNoteMs = (float) (240000.0 / bpm);

        bool dlySync = apvts.getRawParameterValue(XID::DlySync)->load() > 0.5f;
        float timeMs;
        if (dlySync)
        {
            int divIdx = juce::jlimit(0, DlyNoteTable::kNumDivs - 1,
                             (int) std::round(apvts.getRawParameterValue(XID::DlyNoteDiv)->load()));
            timeMs = wholeNoteMs * DlyNoteTable::wholeNoteFraction[divIdx];
        }
        else
        {
            timeMs = apvts.getRawParameterValue(XID::DlyTime)->load();
        }

        int preDivIdx = juce::jlimit(0, 2, (int) std::round(apvts.getRawParameterValue(XID::DlyPreDelay)->load()));
        float preDelayMs = preDivIdx == 1 ? wholeNoteMs / 32.0f : (preDivIdx == 2 ? wholeNoteMs / 16.0f : 0.0f);
        float preDelaySamples = juce::jlimit(0.0f, (float) dlyPreDelayL.getMaximumDelayInSamples() - 1.0f,
                                              preDelayMs * 0.001f * (float) sr);
        dlyPreDelayL.setDelay(preDelaySamples);
        dlyPreDelayR.setDelay(preDelaySamples);

        float fbPct     = apvts.getRawParameterValue(XID::DlyFeedback)->load() / 100.0f;
        float spreadPct = apvts.getRawParameterValue(XID::DlySpread)->load() / 100.0f;
        float mixPct    = apvts.getRawParameterValue(XID::DlyMix)->load() / 100.0f;
        float duckPct   = apvts.getRawParameterValue(XID::DlyDuck)->load() / 100.0f;
        float duckRelMs = apvts.getRawParameterValue(XID::DlyDuckRelease)->load();
        float panRateHz = apvts.getRawParameterValue(XID::DlyPanRate)->load();
        float drivePct  = apvts.getRawParameterValue(XID::DlyDrive)->load() / 100.0f;

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

        // Real tape-echo-style character, only costed when Drive > 0:
        // a soft tanh drive on the recirculating feedback signal (with
        // makeup gain so overall repeat level doesn't visibly drop as
        // Drive increases), plus a small amount of delay-time wow at a
        // fixed, slow, musically-subtle rate — the same physical coupling
        // a real tape unit has between its saturation and its pitch
        // wobble, driven by the one Drive knob instead of a second one.
        bool  driveOn   = drivePct > 0.0005f;
        float drivePre  = 1.0f + drivePct * 2.5f;
        float driveMakeup = driveOn ? 1.0f / std::tanh(drivePre) : 1.0f;
        float wowW = 2.0f * juce::MathConstants<float>::pi * 0.35f / (float) sr;   // fixed, subtle rate
        float wowDepthSamples = 0.0012f * (float) sr;   // up to ~1.2ms peak deviation at full Drive

        for (int n = 0; n < numSamples; ++n)
        {
            if (driveOn)
            {
                dlyWowPhase += wowW;
                if (dlyWowPhase > juce::MathConstants<float>::twoPi) dlyWowPhase -= juce::MathConstants<float>::twoPi;
                float wow = std::sin(dlyWowPhase) * drivePct * wowDepthSamples;
                delayL.setDelay(juce::jlimit(1.0f, maxDelay, delaySamplesL + wow));
                delayR.setDelay(juce::jlimit(1.0f, maxDelay, delaySamplesR - wow));
            }

            float predL = dlyPreDelayL.popSample(0);
            float predR = dlyPreDelayR.popSample(0);
            dlyPreDelayL.pushSample(0, inL[n]);
            dlyPreDelayR.pushSample(0, inR[n]);

            float dL = delayL.popSample(0);
            float dR = delayR.popSample(0);
            float fbL = dlyFbLpfL.processSample(dlyFbHpfL.processSample(dL));
            float fbR = dlyFbLpfR.processSample(dlyFbHpfR.processSample(dR));
            if (driveOn)
            {
                fbL = std::tanh(fbL * drivePre) * driveMakeup;
                fbR = std::tanh(fbR * drivePre) * driveMakeup;
            }
            delayL.pushSample(0, predL + fbR * fbPct);
            delayR.pushSample(0, predR + fbL * fbPct);

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

        dlyPanPhaseUI.store(dlyPanPhase, std::memory_order_relaxed);

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
        float ceilingDb = apvts.getRawParameterValue(XID::LimCeiling)->load();
        float inGain    = apvts.getRawParameterValue(XID::LimInputGain)->load();
        float releaseMs = apvts.getRawParameterValue(XID::LimRelease)->load();
        float clipAmt   = apvts.getRawParameterValue(XID::LimClip)->load() / 100.0f;

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
    // Run the 15 modules above in the user's current chain order (identity
    // order — Pre, Gate, Tune, Ess, Trs, Comp, Opto, Eq, Res, Sat, Exc,
    // Dbl, Rev, Dly, Lim — by default, same as the original fixed
    // sequence, so nothing changes unless the user has actually reordered
    // something via moveModule()). Each lambda reads/writes `buffer` in
    // place and reads whatever the chain has produced so far, exactly
    // like the original fixed-order code did — only WHICH ONE runs at
    // each step is now data-driven.
    // ---------------------------------------------------------------
    {
        const std::array<std::function<void()>, kNumSlots> runners = {
            runPre, runGate, runTune, runEss, runTrs, runComp, runOpto, runEq, runRes, runSat, runExc, runDbl, runRev, runDly, runLim
        };
        for (int pos = 0; pos < kNumSlots; ++pos)
            runners[(size_t) chainOrder[(size_t) pos].load(std::memory_order_relaxed)]();
    }

    // ---------------------------------------------------------------
    // Stereo Width — mid/side, Master utility control
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
    // Master spectrum tap — raw full-rate mono samples of the true final
    // output (post Master Out Gain, so this is exactly what leaves the
    // plugin), for the overview page's whole-mix spectrum analyser.
    // ---------------------------------------------------------------
    {
        auto* l = buffer.getReadPointer(0);
        auto* r = numCh > 1 ? buffer.getReadPointer(1) : l;
        for (int n = 0; n < numSamples; ++n)
        {
            int pos = specWritePosMaster.load(std::memory_order_relaxed);
            specRingMaster[(size_t) (pos & (kSpecSize - 1))].store(0.5f * (l[n] + r[n]), std::memory_order_relaxed);
            specWritePosMaster.store(pos + 1, std::memory_order_relaxed);
        }
    }

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
    juce::String orderStr;
    for (int i = 0; i < kNumSlots; ++i)
        orderStr << chainOrder[(size_t) i].load(std::memory_order_relaxed) << (i + 1 < kNumSlots ? "," : "");
    xml->setAttribute("xalzaChainOrder", orderStr);
    // Reverb's loaded impulse response: the audio itself isn't stored (an
    // arbitrary-length WAV embedded in every preset would bloat it hugely)
    // — just the absolute path, re-decoded on reload if the file's still
    // there. Missing/moved files just leave Hybrid with nothing to blend
    // toward, same as never having loaded one.
    if (currentIrFile.existsAsFile())
        xml->setAttribute("xalzaRevIrPath", currentIrFile.getFullPathName());
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

        // Reverb's loaded impulse response: re-decode from the saved path
        // if it's still there. Real disk I/O, but setStateInformation is
        // already a message-thread, load-time-only call — same cost
        // class as everything else happening in this function.
        if (xml->hasAttribute("xalzaRevIrPath"))
        {
            juce::File irFile(xml->getStringAttribute("xalzaRevIrPath"));
            if (irFile.existsAsFile())
                loadImpulseResponseFile(irFile);
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XaLZaProcessor();
}
