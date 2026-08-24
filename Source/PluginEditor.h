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
    (knob-svg circle.trk / .fil / .face). */
class XaLZaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XaLZaLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
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
            bars[i] = juce::jmax(norm, bars[i] * 0.42f);   // fast attack, fast decay — real-time feel
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
    }

    static constexpr int numBars = 40;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize];
    float bars[numBars] = {};
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
    void resized() override;

private:
    struct KnobUI
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // A module's fine-tune page gets its own IN/OUT LED meters (tapping the
    // real signal either side of that stage) and, for the three dynamics
    // modules, a live gain-reduction readout — mirrors the mockup's
    // per-module meters and "Gain Reduction" numbers.
    struct ModuleMeterUI
    {
        LedMeter meterIn, meterOut;
        juce::Label capIn, capOut, dbIn, dbOut, grLabel;
        int tapIn = 0, tapOut = 0;
        int grIndex = -1;   // -1 = no GR readout for this module
        void setVisible(bool v)
        {
            meterIn.setVisible(v); meterOut.setVisible(v);
            capIn.setVisible(v); capOut.setVisible(v);
            dbIn.setVisible(v); dbOut.setVisible(v);
            grLabel.setVisible(v && grIndex >= 0);
        }
    };

    KnobUI& addKnob(const juce::String& paramID, const juce::String& shortLabel, bool accent);
    ModuleMeterUI& addModuleMeter(const juce::String& tab, int tapIn, int tapOut, int grIndex);
    void showPage(int index);
    void layoutKnobRow(const std::vector<KnobUI*>& row, juce::Rectangle<int> area,
                        int labelH, int knobW, int knobH, int cellW);
    void layoutModuleMeter(ModuleMeterUI& mm, juce::Rectangle<int> area);
    void timerCallback() override;

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
    VUMeter preVu;
    juce::Label preVuTitle;
    EnvelopeGraph gateEnvGraph;
    juce::Label gateEnvTitle;
    EnvelopeGraph essEnvGraph;
    juce::Label essEnvTitle;
    EnvelopeGraph compGrGraph;
    juce::Label compGrTitle;
    WaveformScope optoScope;
    juce::Label optoScopeTitle;
    SpectrumAnalyzer eqSpectrum;
    juce::Label eqSpectrumTitle;
    EnvelopeGraph resSuppressGraph;
    juce::Label resSuppressTitle;
    WaveformScope satScope;
    juce::Label satScopeTitle;

    // Resolved from tabNames in the constructor.
    int preTabIndex = 1, gateTabIndex = 10, essTabIndex = 11, compTabIndex = 2,
        optoTabIndex = 3, eqTabIndex = 4, resTabIndex = 9, satTabIndex = 5;

    // Generic {tab, component, title} list used by showPage()/resized() so
    // every "big viz" page shares one layout path instead of one
    // special-case per module.
    struct BigViz { int tabIndex = -1; juce::Component* comp = nullptr; juce::Label* title = nullptr; };
    std::vector<BigViz> bigViz;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaEditor)
};
