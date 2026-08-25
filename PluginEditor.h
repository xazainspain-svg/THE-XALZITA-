#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

// Exact palette from the original web mockup artifact (its CSS custom
// properties, oklch() converted to sRGB) so the plugin looks like the
// same product: --panel, --panel-2, --panel-3, --border, --border-soft,
// --text-hi, --text, --text-muted, --accent, --accent2.
namespace XaLZaColour
{
    static const juce::Colour panelBg      { 0xff272829 };  // --panel
    static const juce::Colour panelRaised  { 0xff313233 };  // --panel-2
    static const juce::Colour panelControl { 0xff3c3d3e };  // --panel-3
    static const juce::Colour border       { 0xff474849 };  // --border
    static const juce::Colour borderSoft   { 0xff3c3d3f };  // --border-soft
    static const juce::Colour textHi       { 0xffe7e8e8 };  // --text-hi
    static const juce::Colour textLabel    { 0xffb0b1b2 };  // --text
    static const juce::Colour textMuted    { 0xff747476 };  // --text-muted
    static const juce::Colour accent       { 0xffe78a45 };  // --accent (warm orange)
    static const juce::Colour accent2      { 0xff33a3b4 };  // --accent2 (teal)
    static const juce::Colour danger       { 0xffc74b47 };  // --danger
}

/** Rotary knob: thin track, accent- or gray-coloured value arc, a
    pointer line, dark cap — matches the mockup's SVG knob look
    (knob-svg circle.trk / .fil / .face). Buttons/toggles/combo box are
    also flattened here (sharp 2px corners, 1px hairline border, no
    gradient/glossy sheen) to match the mockup's .seg-btn / .icon-btn-box
    / .toggle-track / .preset flat CSS look instead of stock
    LookAndFeel_V4's rounded, shaded default. */
class XaLZaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XaLZaLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;

    // Slider readouts and the preset combo box show live numbers/names, so
    // (matching the mockup's --font-mono usage on .knob-value/.preset-name)
    // route them to IBM Plex Mono instead of the Space Grotesk every other
    // Label/Button gets by default.
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    // Constructs a Font tagged so getTypefaceForFont() above routes it to
    // the embedded IBM Plex Mono instead of the default Space Grotesk —
    // use for numeric/technical readouts (knob values, preset name,
    // footer CPU/status, chain chips), matching the mockup's split
    // between --font-sans (labels) and IBM Plex Mono (everything numeric).
    static juce::Font monoFont(float size, bool bold = false);

private:
    juce::Typeface::Ptr sansRegular, sansBold, monoRegular, monoBold;
};

/** Stereo LED-segment level meter — matches the mockup's .led-meter /
    .led stacks. Fed a dB value per channel from the editor's Timer;
    does no audio-thread work itself. */
class LedMeter : public juce::Component
{
public:
    void setDb(float dbL, float dbR)
    {
        if (std::abs(dbL - lastDbL) > 0.05f || std::abs(dbR - lastDbR) > 0.05f)
        {
            lastDbL = dbL; lastDbR = dbR;
            repaint();
        }
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        float gap = 3.0f;
        float colW = (full.getWidth() - gap) * 0.5f;
        auto colL = full.removeFromLeft(colW);
        full.removeFromLeft(gap);
        drawColumn(g, colL, lastDbL);
        drawColumn(g, full, lastDbR);
    }

    static void drawColumn(juce::Graphics& g, juce::Rectangle<float> col, float db)
    {
        constexpr int numSeg = 12;
        constexpr float minDb = -50.0f, maxDb = 0.0f;
        float t = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        int lit = (int) std::round(t * (float) numSeg);
        float segH = col.getHeight() / (float) numSeg;

        for (int i = 0; i < numSeg; ++i)
        {
            auto seg = col.removeFromBottom(segH).reduced(0.5f, 0.7f);
            bool on = i < lit;
            juce::Colour c = XaLZaColour::panelControl;
            if (on)
            {
                if (i >= numSeg - 2)      c = XaLZaColour::danger;
                else if (i >= numSeg - 4) c = XaLZaColour::accent;
                else                      c = XaLZaColour::accent2;
            }
            g.setColour(c);
            g.fillRect(seg);
        }
    }

    float lastDbL = -100.0f, lastDbR = -100.0f;
};

/** A row of flat TextButtons standing in for one continuous parameter —
    matches the mockup's .seg-group/.seg-btn (e.g. Comp's Ratio: 2:1 / 4:1 /
    8:1 / 20:1 / Limit instead of a bare knob). No new parameter is added:
    clicking a button just snaps the existing float parameter to that
    button's value, so old presets/automation keep working unchanged, and
    refresh() (called every timer tick) highlights whichever button is
    closest to the parameter's current value — so automation or a preset
    loading a value that isn't one of the fixed options still shows the
    nearest option lit rather than none at all. */
class SegButtonGroup : public juce::Component
{
public:
    struct Option { juce::String label; float value; };

    SegButtonGroup(juce::AudioProcessorValueTreeState& state, juce::String paramID, std::vector<Option> opts)
        : apvts(state), paramIdToWatch(std::move(paramID)), options(std::move(opts))
    {
        for (auto& o : options)
        {
            auto* b = buttons.add(new juce::TextButton(o.label));
            b->setClickingTogglesState(false);   // refresh() drives toggle state, not the click itself
            b->setColour(juce::TextButton::buttonColourId, XaLZaColour::panelControl);
            b->setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
            b->setColour(juce::TextButton::textColourOffId, XaLZaColour::textMuted);
            b->setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
            auto v = o.value;
            b->onClick = [this, v] { setValue(v); };
            addAndMakeVisible(b);
        }
        refresh();
    }

    void refresh()
    {
        if (auto* raw = apvts.getRawParameterValue(paramIdToWatch))
        {
            float current = raw->load();
            int bestIdx = 0;
            float bestDist = std::numeric_limits<float>::max();
            for (int i = 0; i < (int) options.size(); ++i)
            {
                float d = std::abs(options[(size_t) i].value - current);
                if (d < bestDist) { bestDist = d; bestIdx = i; }
            }
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState(i == bestIdx, juce::dontSendNotification);
        }
    }

private:
    void setValue(float v)
    {
        if (auto* p = apvts.getParameter(paramIdToWatch))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            p->endChangeGesture();
        }
        refresh();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int n = buttons.size();
        if (n == 0)
            return;
        int w = area.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto b = (i == n - 1) ? area : area.removeFromLeft(w);
            buttons[i]->setBounds(b.reduced(1, 0));
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramIdToWatch;
    std::vector<Option> options;
    juce::OwnedArray<juce::TextButton> buttons;
};

/** Stereo-field scope (X/Y goniometer), matching the mockup's
    "Goniometer" viz card. Reads points handed to it each frame by the
    editor's Timer — no audio-thread access here. */
class Goniometer : public juce::Component
{
public:
    void setPoints(const std::vector<std::pair<float, float>>& pts)
    {
        points = pts;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);

        auto cx = b.getCentreX(), cy = b.getCentreY();
        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(b.getX(), cy, b.getRight(), cy, 1.0f);
        g.drawLine(cx, b.getY(), cx, b.getBottom(), 1.0f);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        auto scale = juce::jmin(b.getWidth(), b.getHeight()) * 0.48f;
        g.setColour(XaLZaColour::accent2.withAlpha(0.8f));
        for (auto& p : points)
        {
            float side = (p.first - p.second) * 0.7071f;   // L-R
            float mid  = (p.first + p.second) * 0.7071f;   // L+R
            float x = cx + side * scale;
            float y = cy - mid * scale;
            g.fillEllipse(x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }

    std::vector<std::pair<float, float>> points;
};

/** Analog-style VU gauge (semicircular arc, ticks, needle) — matches the
    mockup's Preamp "Input Level — VU" card. Ballistics live entirely in
    here: pushDb() is fed a fast peak-ish dB reading, and the needle
    integrates it with a ~300ms time constant like a real VU instrument,
    not a peak meter. */
class VUMeter : public juce::Component
{
public:
    void pushDb(float db)
    {
        float target = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        constexpr float tau = 0.13f;   // seconds — snappier than a classic 300ms VU
        constexpr float dt  = 1.0f / 30.0f;
        float coef = std::exp(-dt / tau);
        smoothed = coef * smoothed + (1.0f - coef) * target;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        float radius = juce::jmin(b.getWidth() * 0.48f, b.getHeight() * 0.82f);
        juce::Point<float> pivot(b.getCentreX(), b.getBottom() - 10.0f);

        constexpr float startAngle = -2.05f, endAngle = 2.05f;

        juce::Path arc;
        arc.addCentredArc(pivot.x, pivot.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(XaLZaColour::border);
        g.strokePath(arc, juce::PathStrokeType(2.0f));

        juce::Path redArc;
        float redStart = startAngle + 0.86f * (endAngle - startAngle);
        redArc.addCentredArc(pivot.x, pivot.y, radius, radius, 0.0f, redStart, endAngle, true);
        g.setColour(XaLZaColour::danger.withAlpha(0.75f));
        g.strokePath(redArc, juce::PathStrokeType(2.6f));

        for (int i = 0; i <= 8; ++i)
        {
            float t = (float) i / 8.0f;
            float a = startAngle + t * (endAngle - startAngle);
            juce::Point<float> p1(pivot.x + std::sin(a) * radius * 0.92f, pivot.y - std::cos(a) * radius * 0.92f);
            juce::Point<float> p2(pivot.x + std::sin(a) * radius * 1.02f, pivot.y - std::cos(a) * radius * 1.02f);
            g.setColour(t >= 0.86f ? XaLZaColour::danger : XaLZaColour::textMuted);
            g.drawLine({ p1, p2 }, 1.2f);
        }

        float angle = startAngle + smoothed * (endAngle - startAngle);
        juce::Path needle;
        needle.addRectangle(-1.1f, -radius * 0.9f, 2.2f, radius * 0.9f);
        needle.applyTransform(juce::AffineTransform::rotation(angle).translated(pivot));
        g.setColour(XaLZaColour::accent);
        g.fillPath(needle);
        g.setColour(XaLZaColour::panelControl);
        g.fillEllipse(pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);
        g.setColour(XaLZaColour::accent);
        g.drawEllipse(pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f, 1.4f);
    }

    static constexpr float minDb = -40.0f, maxDb = 3.0f;
    float smoothed = 0.0f;
};

/** Real-time spectrum analyser (log-frequency bar display), matching the
    mockup's EQ "Response Curve + Spectrum" card. Runs an actual FFT on
    a window of raw samples handed to it each frame by the editor's
    Timer — this is genuine frequency analysis, not a fake animation. */
class SpectrumAnalyzer : public juce::Component
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048

    SpectrumAnalyzer() : fft(fftOrder), window((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        std::fill(std::begin(fftData), std::end(fftData), 0.0f);
        std::fill(std::begin(bars), std::end(bars), 0.0f);
    }

    void setSampleRate(double sr) { sampleRateHint = (float) juce::jmax(1000.0, sr); }

    // samples: fftSize raw values, oldest to newest.
    void update(const float* samples)
    {
        std::copy(samples, samples + fftSize, fftData);
        window.multiplyWithWindowingTable(fftData, (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData);

        for (int i = 0; i < numBars; ++i)
        {
            float f0 = 40.0f * std::pow(18000.0f / 40.0f, (float) i / (float) numBars);
            float f1 = 40.0f * std::pow(18000.0f / 40.0f, (float) (i + 1) / (float) numBars);
            int bin0 = juce::jlimit(1, fftSize / 2 - 1, (int) (f0 * (float) fftSize / sampleRateHint));
            int bin1 = juce::jlimit(bin0 + 1, fftSize / 2, (int) (f1 * (float) fftSize / sampleRateHint));
            float peak = 0.0f;
            for (int bBin = bin0; bBin < bin1; ++bBin)
                peak = juce::jmax(peak, fftData[bBin]);
            float db = juce::Decibels::gainToDecibels(peak, -100.0f);
            float norm = juce::jlimit(0.0f, 1.0f, (db + 84.0f) / 84.0f);
            // Instant attack (jumps straight to a new peak) but a much
            // gentler decay than before — 0.42/frame was dropping a bar to
            // ~1% of its height in five ~33ms ticks, reading as a nervous
            // strobe rather than a spectrum display. 0.90 gives roughly a
            // quarter-second fall, the usual feel for this kind of meter.
            bars[i] = juce::jmax(norm, bars[i] * 0.90f);
        }
        repaint();
    }

    // Analytic magnitude response of the actual 3-band EQ (same formulas
    // processBlock uses), overlaid on the live spectrum so you can see
    // exactly what the curve you dialled in is doing to the signal you're
    // looking at — not just the raw spectrum on its own.
    void setEqCurve(float lowDb, float lowHz, float midDb, float midHz,
                     float highDb, float highHz, double sampleRate)
    {
        auto low  = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, juce::jmax(1.0f, lowHz), 0.707f, juce::Decibels::decibelsToGain(lowDb));
        auto mid  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, juce::jmax(1.0f, midHz), 0.9f, juce::Decibels::decibelsToGain(midDb));
        auto high = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, juce::jmax(1.0f, highHz), 0.707f, juce::Decibels::decibelsToGain(highDb));

        for (int i = 0; i <= numBars; ++i)
        {
            double f = 40.0 * std::pow(18000.0 / 40.0, (double) i / (double) numBars);
            double mag = low->getMagnitudeForFrequency(f, sampleRate)
                       * mid->getMagnitudeForFrequency(f, sampleRate)
                       * high->getMagnitudeForFrequency(f, sampleRate);
            curveDb[i] = juce::Decibels::gainToDecibels((float) mag, -24.0f);
        }
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::borderSoft);
        for (int i = 1; i < 4; ++i)
        {
            float y = b.getY() + b.getHeight() * (float) i / 4.0f;
            g.drawLine(b.getX(), y, b.getRight(), y, 0.5f);
        }
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        float barW = b.getWidth() / (float) numBars;
        for (int i = 0; i < numBars; ++i)
        {
            float h = bars[i] * b.getHeight();
            juce::Rectangle<float> barRect(b.getX() + (float) i * barW, b.getBottom() - h,
                                            barW * 0.78f, h);
            g.setColour(i >= (int) (numBars * 0.82f) ? XaLZaColour::danger : XaLZaColour::accent2);
            g.fillRect(barRect);
        }

        // EQ curve overlay: +-curveRangeDb maps to the full panel height,
        // centred on 0dB.
        constexpr float curveRangeDb = 15.0f;
        juce::Path curve;
        for (int i = 0; i <= numBars; ++i)
        {
            float x = b.getX() + b.getWidth() * (float) i / (float) numBars;
            float t = juce::jlimit(0.0f, 1.0f, (curveDb[i] + curveRangeDb) / (2.0f * curveRangeDb));
            float y = b.getBottom() - t * b.getHeight();
            if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
        }
        g.setColour(XaLZaColour::accent);
        g.strokePath(curve, juce::PathStrokeType(2.0f));
    }

    static constexpr int numBars = 40;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize];
    float bars[numBars] = {};
    float curveDb[numBars + 1] = {};
    float sampleRateHint = 44100.0f;
};

/** Oscilloscope-style raw waveform trace. Fed a fixed window of raw,
    genuinely POST-process samples each frame by the editor's Timer — used
    for modules whose visualiser is "show me the actual waveform this
    stage produced" (Opto) rather than a level history. A second trace can
    be overlaid (e.g. Saturator's processed-vs-dry) — the primary trace
    (a) is drawn last/brighter so it reads as "the result" over the
    secondary (b) reference. */
class WaveformScope : public juce::Component
{
public:
    static constexpr int numPoints = 256;

    void setSamples(const float* trace, const float* trace2 = nullptr)
    {
        std::copy(trace, trace + numPoints, a);
        haveB = (trace2 != nullptr);
        if (haveB)
            std::copy(trace2, trace2 + numPoints, b);
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto bnds = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(bnds);
        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(bnds.getX(), bnds.getCentreY(), bnds.getRight(), bnds.getCentreY(), 0.6f);
        g.setColour(XaLZaColour::border);
        g.drawRect(bnds, 1.0f);

        auto drawTrace = [&] (const float* d, juce::Colour c)
        {
            juce::Path p;
            for (int i = 0; i < numPoints; ++i)
            {
                float x = bnds.getX() + bnds.getWidth() * (float) i / (float) (numPoints - 1);
                float y = bnds.getCentreY() - juce::jlimit(-1.0f, 1.0f, d[i]) * bnds.getHeight() * 0.46f;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            g.setColour(c);
            g.strokePath(p, juce::PathStrokeType(1.4f));
        };

        if (haveB)
            drawTrace(b, XaLZaColour::textMuted.withAlpha(0.65f));
        drawTrace(a, haveB ? XaLZaColour::accent : XaLZaColour::accent2);
    }

    float a[numPoints] = {}, b[numPoints] = {};
    bool haveB = false;
};

/** Scrolling history line chart — one or two traces — for envelope,
    gain-reduction, or suppression-depth readouts that read best as "the
    story over time" rather than a raw waveform. push() takes already
    normalised [0,1] values (callers map their own dB range) so the same
    component serves every module's own scale; pass NaN for valueB on
    single-line modules. Called once per Timer tick (30Hz) so the trace
    scrolls in real time — no extra smoothing beyond whatever ballistics
    already live in the processor readout itself. */
class EnvelopeGraph : public juce::Component
{
public:
    EnvelopeGraph()
    {
        histA.fill(std::numeric_limits<float>::quiet_NaN());
        histB.fill(std::numeric_limits<float>::quiet_NaN());
    }

    void push(float normA, float normB = std::numeric_limits<float>::quiet_NaN())
    {
        histA[(size_t) writePos] = normA;
        histB[(size_t) writePos] = normB;
        if (!std::isnan(normB))
            hasB = true;
        writePos = (writePos + 1) % histLen;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto bnds = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(bnds);
        g.setColour(XaLZaColour::borderSoft);
        for (int i = 1; i < 4; ++i)
        {
            float y = bnds.getY() + bnds.getHeight() * (float) i / 4.0f;
            g.drawLine(bnds.getX(), y, bnds.getRight(), y, 0.5f);
        }
        g.setColour(XaLZaColour::border);
        g.drawRect(bnds, 1.0f);

        auto drawLine = [&] (const std::array<float, (size_t) histLen>& hist, juce::Colour c)
        {
            juce::Path p;
            bool started = false;
            for (int i = 0; i < histLen; ++i)
            {
                int idx = (writePos + i) % histLen;
                float v = hist[(size_t) idx];
                if (std::isnan(v)) continue;
                float x = bnds.getX() + bnds.getWidth() * (float) i / (float) (histLen - 1);
                float y = bnds.getBottom() - juce::jlimit(0.0f, 1.0f, v) * bnds.getHeight();
                if (!started) { p.startNewSubPath(x, y); started = true; } else p.lineTo(x, y);
            }
            g.setColour(c);
            g.strokePath(p, juce::PathStrokeType(1.6f));
        };

        drawLine(histA, XaLZaColour::accent2);
        if (hasB)
            drawLine(histB, XaLZaColour::accent);
    }

    static constexpr int histLen = 150;   // 5 seconds of history at 30Hz
    std::array<float, (size_t) histLen> histA, histB;
    int writePos = 0;
    bool hasB = false;
};

/** Composite Limiter-page view: a real "brickwall" oscilloscope of the
    post-limiter waveform (the ceiling reads as a visibly flattened top),
    plus a real (simplified ITU-R BS.1770) K-weighted LUFS numeric readout
    with a scrolling history line underneath — matches the mockup's
    "Brickwall Output" + "Loudness - LUFS" pairing for this page. */
class LimiterView : public juce::Component
{
public:
    LimiterView()
    {
        addAndMakeVisible(scope);
        addAndMakeVisible(lufsGraph);
        addAndMakeVisible(lufsLabel);
        addAndMakeVisible(truePeakLabel);
        lufsLabel.setJustificationType(juce::Justification::centredLeft);
        lufsLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        lufsLabel.setColour(juce::Label::textColourId, XaLZaColour::textHi);
        truePeakLabel.setJustificationType(juce::Justification::centredRight);
        truePeakLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        truePeakLabel.setColour(juce::Label::textColourId, XaLZaColour::textHi);
    }

    void update(const float* waveform, float lufsDb, float truePeakDb)
    {
        scope.setSamples(waveform);
        float norm = juce::jlimit(0.0f, 1.0f, (lufsDb + 36.0f) / 36.0f);   // -36..0 LUFS window
        lufsGraph.push(norm);
        lufsLabel.setText(lufsDb <= -69.5f ? juce::String("LUFS  -inf")
                                            : ("LUFS  " + juce::String(lufsDb, 1)),
                           juce::dontSendNotification);
        truePeakLabel.setText("TP  " + juce::String(truePeakDb, 1) + " dBTP", juce::dontSendNotification);
        truePeakLabel.setColour(juce::Label::textColourId,
                                 truePeakDb > -1.0f ? XaLZaColour::danger : XaLZaColour::textHi);
    }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto top = b.removeFromTop(b.getHeight() / 2);
        scope.setBounds(top.reduced(0, 2));
        b.removeFromTop(4);
        auto lufsRow = b.removeFromTop(18);
        truePeakLabel.setBounds(lufsRow.removeFromRight(120));
        lufsLabel.setBounds(lufsRow);
        lufsGraph.setBounds(b);
    }

    WaveformScope scope;
    EnvelopeGraph lufsGraph;
    juce::Label truePeakLabel;
    juce::Label lufsLabel;
};

/** Analytic input-vs-output transfer curve for a simple hard-knee
    compressor. This is an honest APPROXIMATION — juce::dsp::Compressor
    has its own internal knee shape we don't have direct read access to —
    but it uses the exact same threshold/ratio/makeup/mix values
    processBlock feeds the real compressor, and the dry/wet mix blend is
    computed in the LINEAR (amplitude) domain exactly like processBlock
    does, so the on-screen shape genuinely matches what turning those
    knobs is doing: where the knee sits, how hard the ratio bites, and how
    much the Mix knob is softening it back toward the unity diagonal. */
class TransferCurveView : public juce::Component
{
public:
    static constexpr int numPts = 96;
    static constexpr float rangeDb = 60.0f;   // axes span -60..0 dB in and out

    void setCurve(float threshDb, float ratio, float makeupDb, float mixAmt)
    {
        float makeupLin = juce::Decibels::decibelsToGain(makeupDb);
        for (int i = 0; i <= numPts; ++i)
        {
            float xDb = -rangeDb + rangeDb * (float) i / (float) numPts;
            float yDbWet = xDb <= threshDb ? xDb : threshDb + (xDb - threshDb) / juce::jmax(1.0f, ratio);
            float dryLin = juce::Decibels::decibelsToGain(xDb);
            float wetLin = juce::Decibels::decibelsToGain(yDbWet) * makeupLin;
            float outLin = dryLin + (wetLin - dryLin) * mixAmt;
            curveDb[i] = juce::Decibels::gainToDecibels(outLin, -100.0f);
        }
        threshNorm = juce::jlimit(0.0f, 1.0f, (threshDb + rangeDb) / rangeDb);
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::borderSoft);
        for (int i = 1; i < 4; ++i)
        {
            float x = b.getX() + b.getWidth() * (float) i / 4.0f;
            float y = b.getY() + b.getHeight() * (float) i / 4.0f;
            g.drawLine(x, b.getY(), x, b.getBottom(), 0.5f);
            g.drawLine(b.getX(), y, b.getRight(), y, 0.5f);
        }
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        auto mapPt = [&] (float xDb, float yDb)
        {
            float tx = juce::jlimit(0.0f, 1.0f, (xDb + rangeDb) / rangeDb);
            float ty = juce::jlimit(0.0f, 1.0f, (yDb + rangeDb) / rangeDb);
            return juce::Point<float>(b.getX() + tx * b.getWidth(), b.getBottom() - ty * b.getHeight());
        };

        g.setColour(XaLZaColour::textMuted.withAlpha(0.5f));
        auto p0 = mapPt(-rangeDb, -rangeDb), p1 = mapPt(0.0f, 0.0f);
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 1.0f);

        float tx = b.getX() + threshNorm * b.getWidth();
        g.setColour(XaLZaColour::danger.withAlpha(0.35f));
        g.drawLine(tx, b.getY(), tx, b.getBottom(), 1.0f);

        juce::Path curve;
        for (int i = 0; i <= numPts; ++i)
        {
            float xDb = -rangeDb + rangeDb * (float) i / (float) numPts;
            auto pt = mapPt(xDb, curveDb[i]);
            if (i == 0) curve.startNewSubPath(pt); else curve.lineTo(pt);
        }
        g.setColour(XaLZaColour::accent);
        g.strokePath(curve, juce::PathStrokeType(2.0f));

        g.setColour(XaLZaColour::textMuted);
        g.setFont(juce::Font(juce::FontOptions(8.5f)));
        g.drawText("IN dB / OUT dB", b.reduced(3.0f), juce::Justification::topLeft);
    }

    float curveDb[numPts + 1] = {};
    float threshNorm = 1.0f;
};

/** Small standalone frequency-response curve (no bars) — the analytic
    magnitude of a single filter across the audible band, log-frequency
    x-axis. Used where a full FFT spectrum would be overkill for what's
    really just "here's the shape of the one filter this stage applies"
    (Preamp's HPF). */
class FreqResponseView : public juce::Component
{
public:
    void setHighPass(float hz, double sampleRate)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, juce::jmax(1.0f, hz));
        for (int i = 0; i <= numPts; ++i)
        {
            double f = 20.0 * std::pow(20000.0 / 20.0, (double) i / (double) numPts);
            double mag = c->getMagnitudeForFrequency(f, sampleRate);
            curveDb[i] = juce::Decibels::gainToDecibels((float) mag, -36.0f);
        }
        cutoffHz = hz;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::borderSoft);
        for (int i = 1; i < 4; ++i)
        {
            float x = b.getX() + b.getWidth() * (float) i / 4.0f;
            g.drawLine(x, b.getY(), x, b.getBottom(), 0.5f);
        }
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        constexpr float rangeDb = 30.0f;
        juce::Path curve;
        for (int i = 0; i <= numPts; ++i)
        {
            float x = b.getX() + b.getWidth() * (float) i / (float) numPts;
            float t = juce::jlimit(0.0f, 1.0f, (curveDb[i] + rangeDb) / rangeDb);
            float y = b.getBottom() - t * b.getHeight();
            if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
        }
        g.setColour(XaLZaColour::accent);
        g.strokePath(curve, juce::PathStrokeType(2.0f));

        g.setColour(XaLZaColour::textMuted);
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(juce::String((int) cutoffHz) + " Hz HPF", b.reduced(4.0f), juce::Justification::topLeft);
    }

    static constexpr int numPts = 60;
    float curveDb[numPts + 1] = {};
    float cutoffHz = 20.0f;
};

/** Composite Glue-Comp page view: the existing gain-reduction/output
    history graph plus the analytic transfer curve, side by side — the
    moment-to-moment reduction AND the shape of the curve producing it. */
class CompressorView : public juce::Component
{
public:
    CompressorView() { addAndMakeVisible(grGraph); addAndMakeVisible(curve); }

    void push(float grNorm, float outNorm) { grGraph.push(grNorm, outNorm); }
    void setCurve(float threshDb, float ratio, float makeupDb, float mixAmt)
    {
        curve.setCurve(threshDb, ratio, makeupDb, mixAmt);
    }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        grGraph.setBounds(left);
        curve.setBounds(b);
    }

    EnvelopeGraph grGraph;
    TransferCurveView curve;
};

/** Composite Opto page view: the existing post-Opto oscilloscope plus the
    analytic transfer curve (Opto's "Reduction" knob maps to an internal
    threshold at a fixed 4:1 ratio — see processBlock's OPTO block). */
class OptoView : public juce::Component
{
public:
    OptoView() { addAndMakeVisible(scope); addAndMakeVisible(curve); }

    void setSamples(const float* trace) { scope.setSamples(trace); }
    void setCurve(float threshDb, float ratio, float makeupDb, float mixAmt)
    {
        curve.setCurve(threshDb, ratio, makeupDb, mixAmt);
    }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        scope.setBounds(left);
        curve.setBounds(b);
    }

    WaveformScope scope;
    TransferCurveView curve;
};

/** Composite Preamp page view: the input-level VU gauge, a raw post-Gain/
    Character output-waveform trace, a real FFT "harmonic color" bar view
    fed genuinely POST the Character waveshaper (so it shows the actual
    harmonics that stage adds, not a fake animation), and the HPF's
    analytic frequency-response curve — four equal cards, matching the
    original web mockup's Preamp row (Input Level / Output Waveform /
    Harmonic Color / Frequency Response). */
class PreampView : public juce::Component
{
public:
    PreampView() { addAndMakeVisible(vu); addAndMakeVisible(outWave); addAndMakeVisible(harmColor); addAndMakeVisible(freqResp); }

    void pushVu(float db) { vu.pushDb(db); }
    void setOutputWaveform(const float* samples) { outWave.setSamples(samples); }
    void updateHarmonic(const float* samples) { harmColor.update(samples); }
    void setHarmonicSampleRate(double sr) { harmColor.setSampleRate(sr); }
    void setHpf(float hz, double sr) { freqResp.setHighPass(hz, sr); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        constexpr int gap = 6;
        int colW = (b.getWidth() - gap * 3) / 4;
        auto take = [&] { auto c = b.removeFromLeft(colW); b.removeFromLeft(gap); return c; };
        vu.setBounds(take());
        outWave.setBounds(take());
        harmColor.setBounds(take());
        freqResp.setBounds(b);
    }

    VUMeter vu;
    WaveformScope outWave;
    SpectrumAnalyzer harmColor;
    FreqResponseView freqResp;
};

/** Composite Gate page view: a raw post-gate output waveform trace plus
    the existing gate-reduction depth history, side by side — matches the
    original web mockup's two-card Gate row ("Post-Gate Output — Waveform"
    + "Gate Envelope") instead of the reduction history on its own. */
class GateView : public juce::Component
{
public:
    GateView() { addAndMakeVisible(scope); addAndMakeVisible(env); }

    void setWaveform(const float* samples) { scope.setSamples(samples); }
    void push(float normReduction) { env.push(normReduction); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        scope.setBounds(left);
        env.setBounds(b);
    }

    WaveformScope scope;
    EnvelopeGraph env;
};

/** Composite Saturator page view: the existing in-vs-out waveform scope
    plus a real FFT "harmonic content" bar view fed genuinely POST the
    saturator (RawSatOut) — shows the actual harmonics the drive/tone/
    ceiling stage is adding, not a fake animation. */
class SaturatorView : public juce::Component
{
public:
    SaturatorView() { addAndMakeVisible(scope); addAndMakeVisible(harmonics); }

    void setSamples(const float* out, const float* in) { scope.setSamples(out, in); }
    void updateHarmonics(const float* samples) { harmonics.update(samples); }
    void setHarmonicSampleRate(double sr) { harmonics.setSampleRate(sr); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        scope.setBounds(left);
        harmonics.setBounds(b);
    }

    WaveformScope scope;
    SpectrumAnalyzer harmonics;
};

/** Chain-order popup content: 12 rows (current processing order, top =
    first), each with Up/Down buttons that call XaLZaProcessor::moveModule
    directly — genuinely reorders the DSP chain, not just a display. Meant
    to be launched via juce::CallOutBox from a title-bar button, so it
    never has to fight the fixed-pixel module-page layouts for space. */
class ChainOrderPanel : public juce::Component
{
public:
    explicit ChainOrderPanel(XaLZaProcessor& p) : proc(p)
    {
        for (int i = 0; i < XaLZaProcessor::kNumSlots; ++i)
        {
            auto* row = rows.add(new Row());
            addAndMakeVisible(row->label);
            addAndMakeVisible(row->up);
            addAndMakeVisible(row->down);
            row->label.setFont(XaLZaLookAndFeel::monoFont(12.5f));
            row->up.setButtonText(juce::CharPointer_UTF8("\xE2\x96\xB2"));
            row->down.setButtonText(juce::CharPointer_UTF8("\xE2\x96\xBC"));
        }
        refresh();
        setSize(200, XaLZaProcessor::kNumSlots * rowH + 8);
    }

    void refresh()
    {
        for (int pos = 0; pos < XaLZaProcessor::kNumSlots; ++pos)
        {
            int slotId = proc.getChainSlotAt(pos);
            auto* row = rows[pos];
            row->label.setText(juce::String(pos + 1) + ". " + XaLZaProcessor::slotName(slotId),
                                juce::dontSendNotification);
            row->up.onClick = [this, slotId] { proc.moveModule(slotId, -1); refresh(); };
            row->down.onClick = [this, slotId] { proc.moveModule(slotId, 1); refresh(); };
            row->up.setEnabled(pos > 0);
            row->down.setEnabled(pos < XaLZaProcessor::kNumSlots - 1);
        }
    }

private:
    void resized() override
    {
        for (int i = 0; i < rows.size(); ++i)
        {
            auto area = juce::Rectangle<int>(4, 4 + i * rowH, getWidth() - 8, rowH - 2);
            rows[i]->down.setBounds(area.removeFromRight(22));
            rows[i]->up.setBounds(area.removeFromRight(22));
            rows[i]->label.setBounds(area);
        }
    }

    struct Row { juce::Label label; juce::TextButton up, down; };
    static constexpr int rowH = 22;
    XaLZaProcessor& proc;
    juce::OwnedArray<Row> rows;
};

/**
    Editor layout mirrors the web mockup: a narrow vertical tab rail on
    the left (MACROS + the 12 modules, in the mockup's own tab order),
    and a content area on the right that shows either the Macros
    overview (12 macro knobs + a Master mini-panel) or the selected
    module's fine-tune knobs — one page visible at a time, same as the
    mockup's single-panel-per-tab layout.
*/
class XaLZaEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit XaLZaEditor(XaLZaProcessor&);
    ~XaLZaEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    struct KnobUI
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String paramID, macroID;   // macroID empty = not macro-linked (no override indicator)
        bool lastMacroWinning = false;   // last-known state, so we only repaint on an actual change
    };

    // A module's fine-tune page gets its own IN/OUT LED meters (tapping the
    // real signal either side of that stage) and, for the three dynamics
    // modules, a live gain-reduction readout — mirrors the mockup's
    // per-module meters and "Gain Reduction" numbers.
    struct ModuleMeterUI
    {
        LedMeter meterIn, meterOut;
        juce::Label capIn, capOut, dbIn, dbOut, grLabel;
        juce::TextButton bypassBtn { "BYP" }, soloBtn { "SOLO" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        juce::String bypassParamID;
        int tapIn = 0, tapOut = 0;
        // Derived from tapOut (which module this page is) — used to look
        // up the LIVE predecessor tap each frame via
        // proc.getPredecessorTap(), so the IN meter stays correct after
        // the chain has been reordered instead of always reading the
        // original fixed-order tapIn above.
        int slotId = -1;
        int grIndex = -1;   // -1 = no GR readout for this module
        void setVisible(bool v)
        {
            meterIn.setVisible(v); meterOut.setVisible(v);
            capIn.setVisible(v); capOut.setVisible(v);
            dbIn.setVisible(v); dbOut.setVisible(v);
            grLabel.setVisible(v && grIndex >= 0);
            bypassBtn.setVisible(v);
            soloBtn.setVisible(v);
        }
    };

    KnobUI& addKnob(const juce::String& paramID, const juce::String& shortLabel, bool accent);
    ModuleMeterUI& addModuleMeter(const juce::String& tab, int tapIn, int tapOut, int grIndex,
                                   const juce::String& bypassParamID);
    void applyPreset(int presetIndex);
    void toggleSolo(const juce::String& bypassParamID);
    void updateSoloButtonStates();
    void showPage(int index);
    void layoutKnobRow(const std::vector<KnobUI*>& row, juce::Rectangle<int> area,
                        int labelH, int knobW, int knobH, int cellW);
    void layoutModuleMeter(ModuleMeterUI& mm, juce::Rectangle<int> area);
    void timerCallback() override;

    // MIDI Learn: right-click any of the 12 MACROS-page knobs for a small
    // popup menu (Learn / Clear). mouseDown identifies which knob was
    // clicked by comparing against pageKnobs[0] (the macro knobs, in
    // Params.h's xalzaMacroIDs() order) and shows the menu on the matching
    // index.
    void mouseDown(const juce::MouseEvent&) override;
    juce::TooltipWindow tooltipWindow { this, 500 };

    // Resizable/scalable window: all real layout below is computed once
    // against this fixed virtual canvas (matching the original mockup's
    // 900x560 proportions), then uniformly scaled — via contentRoot's
    // transform for controls/visualisers, and via an equal-and-opposite
    // Graphics transform in paint() for the hand-drawn background — to
    // fill whatever real window size the host/user actually resized to.
    // This keeps every existing pixel-based layout/paint calculation
    // below correct unchanged; only the two spots that establish the
    // virtual canvas and apply the transform needed to change.
    static constexpr int baseW = 900, baseH = 560;
    juce::Component contentRoot;

    static constexpr int titleBarH  = 30;
    static constexpr int footerH    = 34;
    static constexpr int railW      = 68;
    static constexpr int marginX    = 14;
    static constexpr int marginY    = 12;
    static constexpr int masterW    = 190;

    static constexpr int macroLabelH = 15;
    static constexpr int macroKnobW  = 58;
    static constexpr int macroKnobH  = 76;
    static constexpr int macroCellW  = 108;

    static constexpr int fineLabelH = 14;
    static constexpr int fineKnobW  = 60;
    static constexpr int fineKnobH  = 82;
    static constexpr int fineCellW  = 96;

    static constexpr int masterLabelH = 13;
    static constexpr int masterKnobW  = 44;
    static constexpr int masterKnobH  = 60;

    static constexpr int moduleMeterH = 92;
    static constexpr int bigVizTitleH = 16;
    static constexpr int bigVizH      = 210;

    XaLZaProcessor& proc;
    XaLZaLookAndFeel laf;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::vector<juce::String> tabNames;
    std::vector<std::vector<KnobUI*>> pageKnobs;   // one entry per tab
    std::vector<KnobUI*> masterKnobs;              // shown on the Macros page only
    std::vector<std::unique_ptr<juce::TextButton>> tabButtons;
    std::vector<std::unique_ptr<ModuleMeterUI>> moduleMeterStorage;
    std::vector<ModuleMeterUI*> moduleMeterByTab;  // one slot per tab, nullptr for MACROS
    int currentTab = 0;

    // Master mini-panel visualisers (shown on the Macros page only)
    LedMeter masterMeterIn, masterMeterOut;
    juce::Label masterCapIn, masterCapOut;
    Goniometer goniometer;
    juce::Label goniometerCap;

    // Momentary LUFS readout on the Master panel (matches the mockup's
    // "LOUDNESS -70.0 LUFS (momentary)" line) — reuses the same real
    // K-weighted LUFS measurement the Limiter page's LimiterView already
    // computes from proc.getLufs(), just displayed here too.
    juce::Label masterLoudnessLabel;

    // Global bypass — real dry passthrough (see XID::MasterBypass), lives in
    // the title bar like the mockup's header BYPASS control.
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Module-specific "big" visualisers, shown only on their own page. Each
    // one reads genuinely POST that module's own processing (never the
    // module's input), wired in natural signal-chain order:
    //   PRE: analog-style VU gauge (input level)
    //   GATE: gate-reduction depth history
    //   ESS: sibilance-band level + reduction depth, dual history
    //   COMP: gain-reduction + output level, dual history
    //   OPTO: post-Opto oscilloscope (real raw waveform)
    //   EQ: real FFT spectrum analyser (post-EQ signal)
    //   RES: dynamic-suppression depth history
    //   SAT: waveform in-vs-out oscilloscope (dual trace)
    PreampView preView;
    juce::Label preVuTitle;
    GateView gateView;
    juce::Label gateEnvTitle;
    EnvelopeGraph essEnvGraph;
    juce::Label essEnvTitle;
    CompressorView compView;
    juce::Label compGrTitle;
    OptoView optoView;
    juce::Label optoScopeTitle;
    SpectrumAnalyzer eqSpectrum;
    juce::Label eqSpectrumTitle;
    EnvelopeGraph resSuppressGraph;
    juce::Label resSuppressTitle;
    SaturatorView satView;
    juce::Label satScopeTitle;
    Goniometer dblGoniometer;
    juce::Label dblGoniometerTitle;
    EnvelopeGraph revDecayGraph;
    juce::Label revDecayTitle;
    WaveformScope dlyScope;
    juce::Label dlyScopeTitle;
    LimiterView limView;
    juce::Label limViewTitle;

    // Resolved from tabNames in the constructor.
    int preTabIndex = 1, gateTabIndex = 10, essTabIndex = 11, compTabIndex = 2,
        optoTabIndex = 3, eqTabIndex = 4, resTabIndex = 9, satTabIndex = 5,
        dblTabIndex = 8, revTabIndex = 6, dlyTabIndex = 7, limTabIndex = 12;

    // "Listen" toggles — Gate and De-esser only, since those are the two
    // modules with a genuinely distinct detector signal worth auditioning.
    juce::TextButton gateListenBtn { "LISTEN" }, essListenBtn { "LISTEN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateListenAttachment, essListenAttachment;

    // External sidechain key toggle — Gate only (the standard use case: key
    // the gate off a different signal, e.g. a click track or a second mic).
    // Genuinely does nothing unless the host is also actually routing audio
    // into the plugin's second input bus — the tooltip says so.
    juce::TextButton gateScBtn { "EXT SC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateScAttachment;

    // Comp's Ratio is a real seg-group of preset ratios (matches the
    // mockup's compRatioSegs) instead of a bare knob — see SegButtonGroup
    // above. Snaps the same existing CompRatio float parameter, so old
    // presets/automation still work unchanged.
    std::unique_ptr<SegButtonGroup> compRatioSeg;

    // EQ's three band-frequency choices are real seg-groups too (matches
    // the mockup's eqLowFreqSegs/eqMidFreqSegs/eqHighFreqSegs), snapping
    // the existing continuous EqLowFreq/EqMidFreq/EqHighFreq parameters —
    // same zero-new-parameter approach as compRatioSeg above.
    std::unique_ptr<SegButtonGroup> eqLowFreqSeg, eqMidFreqSeg, eqHighFreqSeg;

    // Delay's Time is a seg-group too — see addPage("DLY", ...) for why.
    std::unique_ptr<SegButtonGroup> dlyTimeSeg;

    // Factory preset picker (title bar) — drives the 12 macro knobs.
    juce::ComboBox presetBox;

    // Chain-order popup trigger (title bar) — opens a ChainOrderPanel via
    // CallOutBox; the panel talks straight to the processor, so there is
    // nothing to keep in sync here beyond launching it.
    juce::TextButton chainOrderBtn { "CHAIN" };

    // User presets — save/load the FULL plugin state (every real parameter,
    // not just the macros) to a .xalzapreset XML file, so a user's own
    // exact settings can be recalled later or shared with someone else.
    juce::TextButton savePresetBtn { "SAVE" }, loadPresetBtn { "LOAD" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    void savePresetToFile();
    void loadPresetFromFile();

    // Instant A/B compare (footer): switching slots snapshots whatever was
    // loaded into the slot you're leaving and recalls the other slot's last
    // snapshot (or seeds it with the current state, the first time it's
    // visited) — no file save/load round-trip needed for a quick compare.
    juce::TextButton abButtonA { "A" }, abButtonB { "B" };
    juce::ValueTree stateA, stateB;
    bool onSlotA = true;
    void switchAbSlot(bool toA);

    // Solo: reuses the existing per-module bypass params — soloing a
    // module bypasses every OTHER module (remembering their prior states
    // to restore) rather than needing separate solo DSP.
    juce::String activeSoloParamID;
    std::map<juce::String, bool> savedBypassStates;

    // Macros-page summary of which modules are currently bypassed, so
    // there's one place to see the whole chain's on/off state at a glance
    // instead of having to visit every page.
    juce::Label bypassSummaryLabel;

    // Footer brand line, now a real clickable control (was static painted
    // text) — shows the actual build version and opens a small About box
    // (chain list, JUCE version, Windows-VST3-only note).
    juce::TextButton aboutButton { "THE XALZA - Vocal Chain" };
    void showAboutBox();

    // Generic {tab, component, title} list used by showPage()/resized() so
    // every "big viz" page shares one layout path instead of one
    // special-case per module.
    struct BigViz { int tabIndex = -1; juce::Component* comp = nullptr; juce::Label* title = nullptr; };
    std::vector<BigViz> bigViz;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaEditor)
};
