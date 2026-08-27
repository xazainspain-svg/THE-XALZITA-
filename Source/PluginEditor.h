#pragma once
#include <cstring>
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
    does no audio-thread work itself.
    Real peak-hold ballistics (a bright single-segment marker that snaps
    to a new peak instantly and only falls back down after a hold time,
    like every real hardware/plugin meter — iZotope Insight's Levels
    panel is the reference this was built against) plus a proper clip
    light (the top segment latches red for a few seconds once a channel
    gets near 0 dBFS, instead of only flashing for the one frame it
    actually happened) — neither existed before; the meter used to be
    just the raw fast-attack/slow-release body with nothing held. */
class LedMeter : public juce::Component
{
public:
    // rmsDbL/rmsDbR: the processor's real mean-square (RMS) reading for
    // this same tap — a genuinely different measurement from the peak
    // ballistics above (see XaLZaProcessor::updateMeter), not a derived
    // or smoothed copy of the peak value. Drawn as a thin marker line
    // across the LED column, the same "Peak + RMS together" pairing the
    // iZotope Insight Levels reference this whole meters pass was built
    // from shows.
    void setDb(float dbL, float dbR, float rmsDbL, float rmsDbR)
    {
        updateChannel(dbL, heldL, holdFramesLeftL, clipLatchFramesLeftL);
        updateChannel(dbR, heldR, holdFramesLeftR, clipLatchFramesLeftR);

        if (std::abs(dbL - lastDbL) > 0.05f || std::abs(dbR - lastDbR) > 0.05f
            || std::abs(heldL - lastHeldL) > 0.05f || std::abs(heldR - lastHeldR) > 0.05f
            || std::abs(rmsDbL - lastRmsL) > 0.05f || std::abs(rmsDbR - lastRmsR) > 0.05f)
        {
            lastDbL = dbL; lastDbR = dbR;
            lastHeldL = heldL; lastHeldR = heldR;
            lastRmsL = rmsDbL; lastRmsR = rmsDbR;
            repaint();
        }
    }

    // What the meter is currently holding as each channel's peak — a
    // Label elsewhere can mirror this instead of a raw, jittery number
    // that changes every frame and is unreadable in real use.
    float getHeldDbL() const noexcept { return heldL; }
    float getHeldDbR() const noexcept { return heldR; }

private:
    static void updateChannel(float db, float& held, int& holdFramesLeft, int& clipFramesLeft)
    {
        constexpr float clipThresholdDb = -0.3f;
        constexpr int   holdFrames      = 45;   // ~1.5s at the editor's 30Hz Timer
        constexpr float decayDbPerFrame = 0.7f; // ~21 dB/s fall-off once the hold expires
        constexpr int   clipLatchFrames = 90;   // ~3s — long enough to actually read the clip

        if (db >= held)
        {
            held = db;
            holdFramesLeft = holdFrames;
        }
        else if (holdFramesLeft > 0)
        {
            --holdFramesLeft;
        }
        else
        {
            held = juce::jmax(db, held - decayDbPerFrame);
        }

        if (held >= clipThresholdDb)
            clipFramesLeft = clipLatchFrames;
        else if (clipFramesLeft > 0)
            --clipFramesLeft;
    }

    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        float gap = 3.0f;
        float colW = (full.getWidth() - gap) * 0.5f;
        auto colL = full.removeFromLeft(colW);
        full.removeFromLeft(gap);
        drawColumn(g, colL, lastDbL, lastHeldL, clipLatchFramesLeftL > 0, lastRmsL);
        drawColumn(g, full, lastDbR, lastHeldR, clipLatchFramesLeftR > 0, lastRmsR);
    }

    static void drawColumn(juce::Graphics& g, juce::Rectangle<float> col, float db, float heldDb, bool clipped, float rmsDb)
    {
        constexpr int numSeg = 12;
        constexpr float minDb = -50.0f, maxDb = 0.0f;
        auto colFull = col;   // the loop below consumes `col` bottom-up; keep the original bounds for the RMS line
        float t = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        int lit = (int) std::round(t * (float) numSeg);
        float tHeld = juce::jlimit(0.0f, 1.0f, (heldDb - minDb) / (maxDb - minDb));
        int peakSeg = juce::jlimit(0, numSeg - 1, (int) std::round(tHeld * (float) numSeg) - 1);
        float segH = col.getHeight() / (float) numSeg;

        for (int i = 0; i < numSeg; ++i)
        {
            auto seg = col.removeFromBottom(segH).reduced(0.5f, 0.7f);
            bool on = i < lit;
            bool isPeak = (i == peakSeg);
            bool isClipSeg = clipped && i == numSeg - 1;
            juce::Colour c = XaLZaColour::panelControl;
            if (on || isPeak || isClipSeg)
            {
                if (isClipSeg)             c = XaLZaColour::danger;
                else if (i >= numSeg - 2)  c = XaLZaColour::danger;
                else if (i >= numSeg - 4)  c = XaLZaColour::accent;
                else                       c = XaLZaColour::accent2;
            }
            // The peak-hold marker itself always reads as a bright,
            // distinct highlight (not just "whatever colour that segment
            // would be") so it's legible as a held marker and not
            // mistaken for the continuously-lit body — matches the
            // white peak cap sitting on Insight's grey level bars.
            if (isPeak && !isClipSeg)
                c = XaLZaColour::textHi;
            g.setColour(c);
            g.fillRect(seg);
        }

        // RMS marker: a thin line across the column at the real mean-
        // square level — the "how loud does this actually sound" reading
        // sitting underneath the peak segments' "what's the instantaneous
        // maximum" one, the same Peak+RMS pairing Insight's Levels panel
        // shows. Drawn last so it's never hidden behind a lit segment.
        float tRms = juce::jlimit(0.0f, 1.0f, (rmsDb - minDb) / (maxDb - minDb));
        float rmsY = colFull.getBottom() - tRms * colFull.getHeight();
        // Teal, not the peak-hold marker's white — a thin line reads as a
        // distinct "average level" indicator rather than a second peak cap.
        g.setColour(XaLZaColour::accent2.withAlpha(0.95f));
        g.fillRect(juce::Rectangle<float>(colFull.getX(), rmsY - 0.6f, colFull.getWidth(), 1.2f));
    }

    float lastDbL = -100.0f, lastDbR = -100.0f;
    float heldL = -100.0f, heldR = -100.0f;
    float lastHeldL = -100.0f, lastHeldR = -100.0f;
    float lastRmsL = -100.0f, lastRmsR = -100.0f;
    int holdFramesLeftL = 0, holdFramesLeftR = 0;
    int clipLatchFramesLeftL = 0, clipLatchFramesLeftR = 0;
};

/** Compact gain-reduction meter for the three dynamics modules (Comp,
    Opto, Limiter) — a number over a fill bar, the same "read the number,
    confirm it with a bar" pattern Insight uses for its Levels panel.
    Before this, the only GR feedback anywhere was a bare text label that
    just changed digits with zero sense of how deep the reduction was. */
class GrMeter : public juce::Component
{
public:
    void setGrDb(float db)
    {
        db = juce::jlimit(0.0f, 24.0f, db);
        if (std::abs(db - lastDb) > 0.05f)
        {
            lastDb = db;
            repaint();
        }
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelControl);
        g.fillRect(b);

        constexpr float maxDb = 24.0f;
        float t = juce::jlimit(0.0f, 1.0f, lastDb / maxDb);
        if (t > 0.0f)
        {
            auto fill = b.reduced(1.0f);
            fill = fill.removeFromLeft(fill.getWidth() * t);
            juce::Colour c = lastDb >= 12.0f ? XaLZaColour::danger : XaLZaColour::accent;
            g.setColour(c.withAlpha(0.85f));
            g.fillRect(fill);
        }

        g.setColour(XaLZaColour::borderSoft);
        g.drawRect(b, 1.0f);

        g.setColour(XaLZaColour::textHi);
        g.setFont(XaLZaLookAndFeel::monoFont(9.5f, true));
        g.drawText("GR -" + juce::String(lastDb, 1) + " dB", b, juce::Justification::centred);
    }

    float lastDb = 0.0f;
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

/** Purely decorative bordered-card outline (transparent fill, so it can be
    added on top of already-placed knobs without hiding them) — used to
    visually group a knob cluster the way the mockup groups related
    controls into their own boxes (e.g. Reverb's "Sidechain Ducking"). */
class CardFrame : public juce::Component
{
private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(XaLZaColour::borderSoft);
        g.drawRoundedRectangle(b, 5.0f, 1.0f);
    }
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
        auto b = getLocalBounds();
        int w = juce::jmax(1, b.getWidth()), h = juce::jmax(1, b.getHeight());
        if (trail.isNull() || trail.getWidth() != w || trail.getHeight() != h)
        {
            trail = juce::Image(juce::Image::ARGB, w, h, true);
            trail.clear(trail.getBounds(), XaLZaColour::panelBg);
        }

        auto bf = b.toFloat();
        auto cx = bf.getCentreX(), cy = bf.getCentreY();
        auto scale = juce::jmin(bf.getWidth(), bf.getHeight()) * 0.48f;

        {
            juce::Graphics tg(trail);
            // Fade the persistent trail slightly each frame instead of
            // wiping it clean — new points land bright on top of a
            // slowly-decaying history, so this reads as a dense, living
            // scatter cloud (matching Insight's Polar Sample view) instead
            // of a bare, flickering instant frame.
            tg.setColour(XaLZaColour::panelBg.withAlpha(0.12f));
            tg.fillRect(trail.getBounds());

            tg.setColour(XaLZaColour::accent2.withAlpha(0.55f));
            for (auto& p : points)
            {
                float side = (p.first - p.second) * 0.7071f;   // L-R
                float mid  = (p.first + p.second) * 0.7071f;   // L+R
                float x = cx + side * scale;
                float y = cy - mid * scale;
                tg.fillEllipse(x - 1.1f, y - 1.1f, 2.2f, 2.2f);
            }
        }

        g.drawImageAt(trail, 0, 0);

        // Axes/border stay crisp every frame — they're drawn fresh on top
        // instead of living inside the fading trail image.
        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(bf.getX(), cy, bf.getRight(), cy, 1.0f);
        g.drawLine(cx, bf.getY(), cx, bf.getBottom(), 1.0f);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1);
    }

    std::vector<std::pair<float, float>> points;
    juce::Image trail;
};

/** Real stereo phase-correlation meter, the natural numeric companion to
    the Goniometer's scatter plot — every pro metering suite pairs the
    two (this session's own iZotope Insight reference included, under
    "Sound Field"), but this plugin never had one. Computed live in the
    editor as the standard Pearson correlation coefficient over the exact
    same decimated post-chain L/R samples the Goniometer already reads
    (proc.scopeSampleL/R): +1 = perfectly in-phase/mono-safe, 0 = wide,
    uncorrelated stereo, -1 = out-of-phase (would cancel toward silence
    summed to mono) — a genuine measurement, not a decorative needle. */
class CorrelationMeterView : public juce::Component
{
public:
    void setCorrelation(float c) { corr = juce::jlimit(-1.0f, 1.0f, c); repaint(); }

private:
    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(full);
        g.setColour(XaLZaColour::border);
        g.drawRect(full, 1.0f);

        auto bar = full.reduced(3.0f, 3.0f);
        float midX = bar.getCentreX();

        auto fillC = corr >= 0.0f ? XaLZaColour::accent2 : XaLZaColour::danger;
        float x = midX + corr * bar.getWidth() * 0.5f;
        g.setColour(fillC.withAlpha(0.5f));
        g.fillRect(juce::Rectangle<float>(juce::jmin(midX, x), bar.getY(),
                                           std::abs(x - midX), bar.getHeight()));

        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(midX, bar.getY(), midX, bar.getBottom(), 1.0f);
        g.setColour(fillC);
        g.fillRoundedRectangle(x - 1.4f, bar.getY(), 2.8f, bar.getHeight(), 1.2f);

        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(XaLZaColour::textMuted);
        auto left12 = full.removeFromLeft(12.0f);
        auto right12 = full.removeFromRight(12.0f);
        g.drawText("-1", left12, juce::Justification::centredLeft);
        g.drawText("+1", right12, juce::Justification::centredRight);
        g.drawText("CORR " + juce::String(corr, 2), full, juce::Justification::centred);
    }

    float corr = 0.0f;
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

/** Classic analog-style Gain-Reduction gauge — the needle meter every real
    hardware compressor (1176, dbx, you name it) puts right on the front
    panel, and a shape that was conspicuously missing here: Comp's own
    reduction used to be just a scrolling line graph, the exact same shape
    RES/REV already use for their own depth histories. Rests at 0 dB on
    the right and swings LEFT as real reduction increases (grDb straight
    from getGrDb(0)) — hardware convention, not a mirrored VUMeter. */
class GrNeedleMeter : public juce::Component
{
public:
    void pushGrDb(float grDb)
    {
        float target = juce::jlimit(0.0f, 1.0f, grDb / maxDb);
        constexpr float tau = 0.09f;   // snappier than the input VU — GR needles read fast
        constexpr float dt  = 1.0f / 30.0f;
        float coef = std::exp(-dt / tau);
        smoothed = coef * smoothed + (1.0f - coef) * target;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        auto b = full;
        auto readout = b.removeFromBottom(14.0f);

        float radius = juce::jmin(b.getWidth() * 0.48f, b.getHeight() * 0.82f);
        juce::Point<float> pivot(b.getCentreX(), b.getBottom() - 10.0f);

        constexpr float startAngle = -2.05f, endAngle = 2.05f;

        juce::Path arc;
        arc.addCentredArc(pivot.x, pivot.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(XaLZaColour::border);
        g.strokePath(arc, juce::PathStrokeType(2.0f));

        // Red zone at the HEAVY-reduction end — the needle swings toward
        // it as GR increases, the opposite end from VUMeter's overload
        // zone, since this gauge reads "how much is being pulled down"
        // rather than "how hot is the input".
        juce::Path redArc;
        float redEnd = startAngle + 0.14f * (endAngle - startAngle);
        redArc.addCentredArc(pivot.x, pivot.y, radius, radius, 0.0f, startAngle, redEnd, true);
        g.setColour(XaLZaColour::danger.withAlpha(0.75f));
        g.strokePath(redArc, juce::PathStrokeType(2.6f));

        for (int i = 0; i <= 8; ++i)
        {
            float t = (float) i / 8.0f;
            float a = startAngle + t * (endAngle - startAngle);
            juce::Point<float> p1(pivot.x + std::sin(a) * radius * 0.92f, pivot.y - std::cos(a) * radius * 0.92f);
            juce::Point<float> p2(pivot.x + std::sin(a) * radius * 1.02f, pivot.y - std::cos(a) * radius * 1.02f);
            g.setColour(t <= 0.14f ? XaLZaColour::danger : XaLZaColour::textMuted);
            g.drawLine({ p1, p2 }, 1.2f);
        }

        // Needle rests at endAngle (0dB, right) and swings toward
        // startAngle (max reduction, left) as smoothed increases.
        float angle = endAngle - smoothed * (endAngle - startAngle);
        juce::Path needle;
        needle.addRectangle(-1.1f, -radius * 0.9f, 2.2f, radius * 0.9f);
        needle.applyTransform(juce::AffineTransform::rotation(angle).translated(pivot));
        g.setColour(XaLZaColour::accent);
        g.fillPath(needle);
        g.setColour(XaLZaColour::panelControl);
        g.fillEllipse(pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);
        g.setColour(XaLZaColour::accent);
        g.drawEllipse(pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f, 1.4f);

        g.setColour(XaLZaColour::textMuted);
        g.setFont(XaLZaLookAndFeel::monoFont(9.5f, true));
        g.drawText("GR -" + juce::String(smoothed * maxDb, 1) + " dB", readout, juce::Justification::centred);
    }

    static constexpr float maxDb = 24.0f;
    float smoothed = 0.0f;
};

/** Opto's page-defining visual: a glowing "photocell" orb — the way every
    real optical compressor (LA-2A and kin) is actually built, an
    electric-eye/photocell whose light brightens and dims with the live
    gain reduction it's driving — instead of a rectangular graph like
    almost everything else in this plugin. Brightness AND size both track
    the exact same live GR reading (getGrDb(1)) the meters elsewhere
    already show as a plain number: a different SHAPE for the same real
    data, not a different data source. */
class OptoGlowView : public juce::Component
{
public:
    void pushGrDb(float grDb)
    {
        float target = juce::jlimit(0.0f, 1.0f, grDb / maxDb);
        constexpr float tau = 0.15f;
        constexpr float dt  = 1.0f / 30.0f;
        float coef = std::exp(-dt / tau);
        smoothed = coef * smoothed + (1.0f - coef) * target;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(full);
        g.setColour(XaLZaColour::border);
        g.drawRect(full, 1.0f);

        auto b = full;
        auto readout = b.removeFromBottom(14.0f);
        auto cx = b.getCentreX(), cy = b.getCentreY();
        float maxR = juce::jmin(b.getWidth(), b.getHeight()) * 0.42f;

        // Brighter AND slightly larger as reduction increases — a real
        // photocell glows harder under more light (the sidechain signal
        // driving it), which is exactly what's pulling the gain down.
        float glowT = smoothed;
        float r = maxR * (0.55f + 0.45f * glowT);
        juce::Colour core = XaLZaColour::accent2.interpolatedWith(XaLZaColour::accent, glowT);

        for (int i = 4; i >= 1; --i)
        {
            float ringR = r * (1.0f + 0.22f * (float) i);
            g.setColour(core.withAlpha(0.05f * glowT + 0.02f));
            g.fillEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);
        }
        g.setColour(core.withAlpha(0.85f));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(XaLZaColour::panelBg.withAlpha(0.4f));
        g.fillEllipse(cx - r * 0.35f, cy - r * 0.35f, r * 0.7f, r * 0.7f);

        g.setColour(XaLZaColour::textMuted);
        g.setFont(XaLZaLookAndFeel::monoFont(9.5f, true));
        g.drawText("GR -" + juce::String(smoothed * maxDb, 1) + " dB", readout, juce::Justification::centred);
    }

    static constexpr float maxDb = 24.0f;
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

/** Real-time scrolling spectrogram (frequency x time, colour = level) of
    the plugin's actual final output — the classic "waterfall" every
    serious analyzer ships (this is what iZotope Insight's own Spectrogram
    tab is). Every column is a genuine FFT of live post-limiter audio
    (the same RawLim tap LimiterView's waveform trace already reads, see
    PluginProcessor::RawLim) computed fresh each frame and scrolled into a
    cached Image — nothing here is a fake animation or a canned texture;
    every pixel is a real bin's real magnitude at the instant it arrived. */
class Spectrogram : public juce::Component
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048 — matches SpectrumAnalyzer

    Spectrogram() : fft(fftOrder), window((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        std::fill(std::begin(fftData), std::end(fftData), 0.0f);
    }

    void setSampleRate(double sr) { sampleRateHint = (float) juce::jmax(1000.0, sr); }

    // samples: fftSize raw values, oldest to newest. Computes one new
    // time-column and scrolls it into the waterfall image.
    void pushBlock(const float* samples)
    {
        std::copy(samples, samples + fftSize, fftData);
        window.multiplyWithWindowingTable(fftData, (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData);

        int w = juce::jmax(1, getWidth()), h = juce::jmax(1, getHeight());
        if (image.isNull() || image.getWidth() != w || image.getHeight() != h)
        {
            image = juce::Image(juce::Image::ARGB, w, h, true);
            image.clear(image.getBounds(), XaLZaColour::panelBg);
        }
        if (w < 2 || h < 2)
            return;

        juce::Image::BitmapData dst(image, juce::Image::BitmapData::readWrite);

        // Scroll everything one column to the left (memmove, not memcpy —
        // the source and destination ranges overlap by design), then
        // paint the new column of frequency bins into the freed right
        // edge, one pixel per row.
        for (int y = 0; y < h; ++y)
            std::memmove(dst.getLinePointer(y), dst.getLinePointer(y) + dst.pixelStride,
                         (size_t) (w - 1) * (size_t) dst.pixelStride);

        for (int y = 0; y < h; ++y)
        {
            // Log-frequency mapping with high frequencies at the top —
            // matches how every real spectrogram reads (and the same
            // 40Hz-18kHz log span SpectrumAnalyzer's bars already use).
            float t = 1.0f - (float) y / (float) (h - 1);
            float freqHz = 40.0f * std::pow(18000.0f / 40.0f, t);
            int bin = juce::jlimit(1, fftSize / 2 - 1, (int) (freqHz * (float) fftSize / sampleRateHint));
            float db = juce::Decibels::gainToDecibels(fftData[bin], -100.0f);
            float norm = juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 90.0f);
            dst.setPixelColour(w - 1, y, magnitudeToColour(norm));
        }

        repaint();
    }

private:
    // A compact 5-stop heat ramp (dark panel background through violet and
    // ember into a bright near-white at full level) so louder genuinely
    // reads as visually hotter, the same colour language real spectrogram
    // tools (Insight included) all use instead of one flat hue.
    static juce::Colour magnitudeToColour(float t)
    {
        struct Stop { float pos; juce::Colour c; };
        static const Stop stops[] = {
            { 0.00f, XaLZaColour::panelBg },
            { 0.30f, juce::Colour(0xff3a2360) },
            { 0.58f, juce::Colour(0xffb43a5a) },
            { 0.82f, XaLZaColour::accent },
            { 1.00f, juce::Colour(0xfffff2c8) },
        };
        constexpr int numStops = (int) (sizeof(stops) / sizeof(stops[0]));
        for (int i = 1; i < numStops; ++i)
        {
            if (t <= stops[i].pos)
            {
                float span = juce::jmax(0.0001f, stops[i].pos - stops[i - 1].pos);
                float localT = juce::jlimit(0.0f, 1.0f, (t - stops[i - 1].pos) / span);
                return stops[i - 1].c.interpolatedWith(stops[i].c, localT);
            }
        }
        return stops[numStops - 1].c;
    }

    void paint(juce::Graphics& g) override
    {
        if (!image.isNull())
            g.drawImageAt(image, 0, 0);
        else
            g.fillAll(XaLZaColour::panelBg);
        g.setColour(XaLZaColour::border);
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        // Drop the cached image so pushBlock() reallocates fresh at the
        // new size next frame instead of stretching old content into the
        // wrong aspect ratio.
        image = {};
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize];
    float sampleRateHint = 44100.0f;
    juce::Image image;
};

/** De-esser page's primary visualizer: a real FFT of the exact signal
    the detector itself analyzes (the same post-Gate/pre-Ess tap runEss
    reads — Ess is genuinely the very next stage in the chain, see
    PluginProcessor::RawGate), zoomed to the sibilant range (800Hz-16kHz)
    instead of the full spectrum, with the live target band glowing
    hotter the harder the dynamic EQ is actually pulling it down right
    now. A de-esser is fundamentally a frequency-domain, single-band
    process — this is the first module visualizer in the plugin that
    actually looks like one, instead of reusing the same time-domain
    envelope line several other dynamics pages already show. */
class DeEsserSpectrumView : public juce::Component
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048 — matches SpectrumAnalyzer/Spectrogram

    DeEsserSpectrumView() : fft(fftOrder), window((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        std::fill(std::begin(fftData), std::end(fftData), 0.0f);
        std::fill(std::begin(bars), std::end(bars), 0.0f);
    }

    void setSampleRate(double sr) { sampleRateHint = (float) juce::jmax(1000.0, sr); }

    // centreHz: the real live target frequency runEss is detecting/
    // cutting at this instant (EssFreq scaled by the Band multiplier).
    // reductionDb: the real live dynamic-EQ gain applied there (negative,
    // see essReductionDbUI) — how hot the target marker glows.
    void setTarget(float centreHz, float reductionDb)
    {
        targetHz = centreHz;
        reductionNorm = juce::jlimit(0.0f, 1.0f, -reductionDb / 18.0f);
    }

    // samples: fftSize raw values, oldest to newest.
    void update(const float* samples)
    {
        std::copy(samples, samples + fftSize, fftData);
        window.multiplyWithWindowingTable(fftData, (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData);

        for (int i = 0; i < numBars; ++i)
        {
            float f0 = minHz * std::pow(maxHz / minHz, (float) i / (float) numBars);
            float f1 = minHz * std::pow(maxHz / minHz, (float) (i + 1) / (float) numBars);
            int bin0 = juce::jlimit(1, fftSize / 2 - 1, (int) (f0 * (float) fftSize / sampleRateHint));
            int bin1 = juce::jlimit(bin0 + 1, fftSize / 2, (int) (f1 * (float) fftSize / sampleRateHint));
            float peak = 0.0f;
            for (int b = bin0; b < bin1; ++b)
                peak = juce::jmax(peak, fftData[b]);
            float db = juce::Decibels::gainToDecibels(peak, -100.0f);
            float norm = juce::jlimit(0.0f, 1.0f, (db + 84.0f) / 84.0f);
            bars[i] = juce::jmax(norm, bars[i] * 0.90f);
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
        float targetT = juce::jlimit(0.0f, 1.0f,
            std::log(juce::jmax(minHz, targetHz) / minHz) / std::log(maxHz / minHz));
        int targetBar = juce::jlimit(0, numBars - 1, (int) (targetT * (float) numBars));

        for (int i = 0; i < numBars; ++i)
        {
            float h = bars[i] * b.getHeight();
            juce::Rectangle<float> barRect(b.getX() + (float) i * barW, b.getBottom() - h,
                                            barW * 0.78f, h);
            juce::Colour c = XaLZaColour::accent2;
            if (std::abs(i - targetBar) <= 1)
                c = XaLZaColour::accent2.interpolatedWith(XaLZaColour::danger, reductionNorm);
            g.setColour(c);
            g.fillRect(barRect);
        }

        // A marker at the exact live target Hz (not just "which bucket")
        // so moving Freq or switching Band visibly slides this line.
        float mx = b.getX() + targetT * b.getWidth();
        g.setColour(XaLZaColour::danger.withAlpha(0.5f + 0.5f * reductionNorm));
        g.drawLine(mx, b.getY(), mx, b.getBottom(), 1.6f);
    }

    static constexpr int numBars = 32;
    static constexpr float minHz = 800.0f, maxHz = 16000.0f;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize];
    float bars[numBars] = {};
    float sampleRateHint = 44100.0f;
    float targetHz = 6000.0f;
    float reductionNorm = 0.0f;
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

/** Limiter's brickwall scope. A plain oscilloscope shows the flattened
    top but not WHY it's flat — this does the real per-sample comparison
    runLim's own look-ahead brickwall is doing: any point within ~0.5dB of
    the LIVE ceiling (the exact value the Ceiling knob is set to right
    now) draws in the danger colour instead of the normal trace colour, so
    the moments the limiter is actually clamping read as visibly distinct
    from the moments it's just passing audio through untouched — plus a
    real dashed reference line at the true ceiling level itself. */
class BrickwallScope : public juce::Component
{
public:
    static constexpr int numPoints = 256;

    void setData(const float* trace, float ceilingDbIn)
    {
        std::copy(trace, trace + numPoints, wave);
        ceilingDb = ceilingDbIn;
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

        float ceilLin = juce::Decibels::decibelsToGain(ceilingDb);
        float nearCeil = ceilLin * 0.94f;   // ~ -0.5dB below ceiling: "about to hit the wall"

        float ceilY  = bnds.getCentreY() - juce::jlimit(0.0f, 1.0f, ceilLin) * bnds.getHeight() * 0.46f;
        float floorY = bnds.getCentreY() + juce::jlimit(0.0f, 1.0f, ceilLin) * bnds.getHeight() * 0.46f;
        juce::Path ceilLines;
        ceilLines.startNewSubPath(bnds.getX(), ceilY);  ceilLines.lineTo(bnds.getRight(), ceilY);
        ceilLines.startNewSubPath(bnds.getX(), floorY); ceilLines.lineTo(bnds.getRight(), floorY);
        float dashLens[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, ceilLines, dashLens, 2);
        g.setColour(XaLZaColour::danger.withAlpha(0.4f));
        g.strokePath(dashed, juce::PathStrokeType(1.0f));

        for (int i = 0; i < numPoints - 1; ++i)
        {
            float x0 = bnds.getX() + bnds.getWidth() * (float) i / (float) (numPoints - 1);
            float x1 = bnds.getX() + bnds.getWidth() * (float) (i + 1) / (float) (numPoints - 1);
            float y0 = bnds.getCentreY() - juce::jlimit(-1.0f, 1.0f, wave[i])     * bnds.getHeight() * 0.46f;
            float y1 = bnds.getCentreY() - juce::jlimit(-1.0f, 1.0f, wave[i + 1]) * bnds.getHeight() * 0.46f;
            bool hot = std::abs(wave[i]) >= nearCeil || std::abs(wave[i + 1]) >= nearCeil;
            g.setColour(hot ? XaLZaColour::danger : XaLZaColour::accent2);
            g.drawLine(x0, y0, x1, y1, hot ? 2.0f : 1.4f);
        }
    }

    float wave[numPoints] = {};
    float ceilingDb = 0.0f;
};

/** Limiter's loudness history: a filled-area LUFS trace (not a bare line)
    with the common streaming normalization target zone (roughly -16 to
    -9 LUFS across Spotify/YouTube/Apple Music) shaded in as a real fixed
    reference band, so where the mix sits relative to typical streaming
    targets reads at a glance rather than only as a number. The pushed
    value is the processor's own real (simplified ITU-R BS.1770
    K-weighted) momentary loudness — same figure the numeric readout
    above shows, just with 5 seconds of genuine history behind it. */
class LoudnessHistoryView : public juce::Component
{
public:
    LoudnessHistoryView() { hist.fill(std::numeric_limits<float>::quiet_NaN()); }

    void push(float lufsDb)
    {
        hist[(size_t) writePos] = lufsDb;
        writePos = (writePos + 1) % histLen;
        repaint();
    }

private:
    static float mapY(juce::Rectangle<float> b, float lufsDb)
    {
        float norm = juce::jlimit(0.0f, 1.0f, (lufsDb + 36.0f) / 36.0f);   // -36..0 LUFS window
        return b.getBottom() - norm * b.getHeight();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);

        float zoneTop = mapY(b, -9.0f), zoneBot = mapY(b, -16.0f);
        g.setColour(XaLZaColour::accent.withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(b.getX(), zoneTop, b.getWidth(), zoneBot - zoneTop));
        g.setColour(XaLZaColour::accent.withAlpha(0.28f));
        g.drawLine(b.getX(), zoneTop, b.getRight(), zoneTop, 0.6f);
        g.drawLine(b.getX(), zoneBot, b.getRight(), zoneBot, 0.6f);

        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        juce::Path fill, line;
        bool started = false;
        for (int i = 0; i < histLen; ++i)
        {
            int idx = (writePos + i) % histLen;
            float v = hist[(size_t) idx];
            if (std::isnan(v)) continue;
            float x = b.getX() + b.getWidth() * (float) i / (float) (histLen - 1);
            float y = mapY(b, v);
            if (!started) { fill.startNewSubPath(x, b.getBottom()); fill.lineTo(x, y); line.startNewSubPath(x, y); started = true; }
            else { fill.lineTo(x, y); line.lineTo(x, y); }
        }
        if (started)
        {
            fill.lineTo(b.getRight(), b.getBottom());
            fill.closeSubPath();
            g.setColour(XaLZaColour::accent2.withAlpha(0.18f));
            g.fillPath(fill);
            g.setColour(XaLZaColour::accent2);
            g.strokePath(line, juce::PathStrokeType(1.6f));
        }

        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(XaLZaColour::textMuted);
        g.drawText("TARGET -16..-9 LUFS", b.reduced(3.0f), juce::Justification::topLeft);
    }

    static constexpr int histLen = 150;   // 5 seconds of history at 30Hz
    std::array<float, (size_t) histLen> hist;
    int writePos = 0;
};

/** Composite Limiter-page view: the brickwall scope above, the loudness
    history below — matches the mockup's "Brickwall Output" + "Loudness -
    LUFS" pairing for this page, now with both halves genuinely specific
    to what a limiter/loudness readout actually needs to show. */
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

    void update(const float* waveform, float lufsDb, float truePeakDb, float ceilingDb)
    {
        scope.setData(waveform, ceilingDb);
        lufsGraph.push(lufsDb);
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

    BrickwallScope scope;
    LoudnessHistoryView lufsGraph;
    juce::Label truePeakLabel;
    juce::Label lufsLabel;
};

/** LIM page composite: the existing brickwall-output/loudness view on the
    left, the new real-time Spectrogram waterfall of the actual final
    output on the right — same 50/50 side-by-side pattern every other
    module page's composite "big viz" already uses (DoublerView,
    ReverbView, ResonanceView, DelayView). update() forwards straight to
    the inner LimiterView so every existing call site keeps working
    unchanged; pushSpectrogramBlock()/setSampleRate() are the only new
    calls a caller needs to add. */
class LimiterAnalysisView : public juce::Component
{
public:
    LimiterAnalysisView() { addAndMakeVisible(limView); addAndMakeVisible(spectrogram); }

    void update(const float* waveform, float lufsDb, float truePeakDb, float ceilingDb)
    {
        limView.update(waveform, lufsDb, truePeakDb, ceilingDb);
    }
    void setSampleRate(double sr) { spectrogram.setSampleRate(sr); }
    void pushSpectrogramBlock(const float* fftWindow) { spectrogram.pushBlock(fftWindow); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        limView.setBounds(left);
        spectrogram.setBounds(b);
    }

    LimiterView limView;
    Spectrogram spectrogram;
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

/** Composite Glue-Comp page view: a real analog-style GR needle gauge
    (the meter every hardware compressor actually has) plus the analytic
    transfer curve, side by side — the moment-to-moment reduction AND the
    shape of the curve producing it. Used to be a scrolling gain-
    reduction/output line graph, the same shape RES/REV already use for
    their own history — the needle gauge is a genuinely different, and
    far more "this is a compressor" instrument. */
class CompressorView : public juce::Component
{
public:
    CompressorView() { addAndMakeVisible(grMeter); addAndMakeVisible(curve); }

    void push(float grDb) { grMeter.pushGrDb(grDb); }
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
        grMeter.setBounds(left);
        curve.setBounds(b);
    }

    GrNeedleMeter grMeter;
    TransferCurveView curve;
};

/** Composite Opto page view: the glowing photocell orb (see OptoGlowView
    above) plus the analytic transfer curve (Opto's "Reduction" knob maps
    to an internal threshold at a fixed 4:1 ratio — see processBlock's
    OPTO block). Used to be a plain oscilloscope trace, the same shape
    six other module pages already show — the glow orb is what actually
    makes this page read as "an optical compressor" rather than yet
    another scope. */
class OptoView : public juce::Component
{
public:
    OptoView() { addAndMakeVisible(glow); addAndMakeVisible(curve); }

    void pushGrDb(float grDb) { glow.pushGrDb(grDb); }
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
        glow.setBounds(left);
        curve.setBounds(b);
    }

    OptoGlowView glow;
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

/** Gate page's primary visualizer: a real open/closed ACTIVITY STRIP —
    the natural way to look at what a noise gate actually does (a mostly
    binary decision over time: is it open or is it clamped down), instead
    of reusing the same continuous envelope-line shape several other
    dynamics pages already have. Each column is a genuinely sampled
    historical reading of the gate's own real-time gain reduction
    (getGateGrDb() — the exact value driving the audio right now),
    bucketed into "open" vs "closed"; nothing here is an animated pattern.
    The waveform underneath is the real post-gate signal, kept for the
    usual "see the actual audio" cross-check every other scope provides. */
class GateActivityView : public juce::Component
{
public:
    static constexpr int numCols = 120;

    GateActivityView() { addAndMakeVisible(scope); }

    void setWaveform(const float* samples) { scope.setSamples(samples); }

    void pushState(float grDb)
    {
        // ~0 dB of reduction reads as fully open; any real attenuation
        // (the hold/release tail included) reads as closed — matches the
        // exact threshold+hold+release state machine runGate implements.
        history[(size_t) writePos] = grDb < 1.0f;
        writePos = (writePos + 1) % numCols;
        repaint();
    }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto top = b.removeFromTop(b.getHeight() * 2 / 3);
        scope.setBounds(top.reduced(0, 2));
        b.removeFromTop(4);
        stripArea = b;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = stripArea.toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        auto inner = b.reduced(1.0f);
        if (inner.getWidth() < 2.0f || inner.getHeight() < 2.0f)
            return;
        float colW = inner.getWidth() / (float) numCols;
        for (int i = 0; i < numCols; ++i)
        {
            // Oldest reading on the left, newest on the right, scrolling
            // like every other time-based view in the plugin.
            size_t idx = (size_t) ((writePos + i) % numCols);
            bool open = history[idx];
            juce::Rectangle<float> cell(inner.getX() + (float) i * colW, inner.getY(),
                                         juce::jmax(1.0f, colW * 0.86f), inner.getHeight());
            g.setColour(open ? XaLZaColour::accent2 : XaLZaColour::panelControl);
            g.fillRect(cell);
        }
    }

    WaveformScope scope;
    juce::Rectangle<int> stripArea;
    bool history[numCols] = {};
    int writePos = 0;
};

/** The Saturator's actual waveshaping transfer curve — linear amplitude
    in vs out, -1..+1 — computed with the EXACT SAME shape() formulas
    runSat applies per Character (see PluginProcessor.cpp's runSat lambda:
    Tube/Tape/Transistor/Diode are genuinely different waveshaper math,
    not a relabelled knob) plus the same ceiling soft-clamp and Mix blend.
    An honest analytic replica, the same "real parameters, not
    audio-reactive" approach TransferCurveView/FreqResponseView/
    DuckingCurveView already use elsewhere — except this curve's whole
    SHAPE is different from all of those (a soft-clip S-curve in linear
    amplitude, not a dB-domain knee line), which is what actually makes
    Tube/Tape/Transistor/Diode look as different from each other as they
    sound, and is the first visualizer in the plugin that shows amplitude-
    in-vs-amplitude-out at all. */
class SaturationCurveView : public juce::Component
{
public:
    static constexpr int numPts = 128;

    void setCurve(int charMode, float drivePct, float ceilDb, float mixAmt)
    {
        float driveAmt = juce::jmap(juce::jlimit(0.0f, 1.0f, drivePct), 0.0f, 1.0f, 1.0f, 10.0f);
        float norm = std::tanh(driveAmt);
        float ceilLin = juce::Decibels::decibelsToGain(ceilDb);

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

        float clampedMix = juce::jlimit(0.0f, 1.0f, mixAmt);
        for (int i = 0; i <= numPts; ++i)
        {
            float x = -1.0f + 2.0f * (float) i / (float) numPts;
            float wet = shape(x);
            if (std::abs(wet) > ceilLin)
                wet = ceilLin * std::tanh(wet / ceilLin);
            curve[i] = x + (wet - x) * clampedMix;
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
            float x = b.getX() + b.getWidth() * (float) i / 4.0f;
            float y = b.getY() + b.getHeight() * (float) i / 4.0f;
            g.drawLine(x, b.getY(), x, b.getBottom(), 0.5f);
            g.drawLine(b.getX(), y, b.getRight(), y, 0.5f);
        }
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        // Unity diagonal (what "no saturation at all" looks like) so the
        // real curve's departure from it actually reads as something.
        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);

        juce::Path path;
        for (int i = 0; i <= numPts; ++i)
        {
            float tx = (float) i / (float) numPts;
            float x = b.getX() + tx * b.getWidth();
            float ty = juce::jlimit(-1.0f, 1.0f, curve[i]);
            float y = b.getCentreY() - ty * b.getHeight() * 0.48f;
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(XaLZaColour::accent);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    float curve[numPts + 1] = {};
};

/** Composite Saturator page view: the real analytic waveshaping curve
    above (what used to be a plain in/out oscilloscope trace — a shape
    already reused on six other module pages, so it added nothing
    Saturator-specific) plus the existing real FFT "harmonic content" bar
    view fed genuinely POST the saturator (RawSatOut) — shows the actual
    harmonics the drive/tone/ceiling stage is adding, not a fake
    animation. */
class SaturatorView : public juce::Component
{
public:
    SaturatorView() { addAndMakeVisible(curveView); addAndMakeVisible(harmonics); }

    void setCurve(int charMode, float drivePct, float ceilDb, float mixAmt)
    {
        curveView.setCurve(charMode, drivePct, ceilDb, mixAmt);
    }
    void updateHarmonics(const float* samples) { harmonics.update(samples); }
    void setHarmonicSampleRate(double sr) { harmonics.setSampleRate(sr); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        curveView.setBounds(left);
        harmonics.setBounds(b);
    }

    SaturationCurveView curveView;
    SpectrumAnalyzer harmonics;
};

/** Doubler "Per-Voice" table: one row per active voice, showing the actual
    delay-stagger (ms) and pan position each voice is running with right
    now. Reads the exact same DblVoiceConfig constants the DSP itself uses
    (see runDbl in PluginProcessor.cpp), so every number here is real —
    nothing here is a fake per-voice animation. */
class PerVoiceTable : public juce::Component
{
public:
    void setData(int newNumVoices, float newDelayMs, float newWidthPct)
    {
        if (numVoices == newNumVoices && juce::approximatelyEqual(delayMs, newDelayMs)
            && juce::approximatelyEqual(widthPct, newWidthPct))
            return;
        numVoices = newNumVoices; delayMs = newDelayMs; widthPct = newWidthPct;
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto bnds = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(bnds);
        g.setColour(XaLZaColour::border);
        g.drawRect(bnds, 1.0f);

        auto area = getLocalBounds().reduced(6, 4);
        int rowH = juce::jmax(12, area.getHeight() / juce::jmax(1, numVoices));
        g.setFont(XaLZaLookAndFeel::monoFont(10.5f));

        for (int v = 0; v < numVoices; ++v)
        {
            auto row = area.removeFromTop(rowH);
            float ms = delayMs + DblVoiceConfig::delayOffsetMs[v];
            float pan = juce::jlimit(-1.0f, 1.0f, DblVoiceConfig::panPos[v] * widthPct);
            juce::String panTxt = juce::approximatelyEqual(pan, 0.0f) ? "C"
                : (pan < 0.0f ? (juce::String((int) std::round(-pan * 100)) + "L")
                              : (juce::String((int) std::round(pan * 100)) + "R"));

            g.setColour(v % 2 == 0 ? XaLZaColour::textMuted.withAlpha(0.9f) : XaLZaColour::textMuted.withAlpha(0.65f));
            g.drawText("V" + juce::String(v + 1), row.removeFromLeft(row.getWidth() / 3),
                       juce::Justification::centredLeft);
            g.setColour(XaLZaColour::accent2.withAlpha(0.85f));
            g.drawText(juce::String(ms, 1) + " ms", row.removeFromLeft(row.getWidth() / 2),
                       juce::Justification::centredLeft);
            g.setColour(XaLZaColour::accent.withAlpha(0.85f));
            g.drawText(panTxt, row, juce::Justification::centredLeft);
        }
    }

    int numVoices = 4;
    float delayMs = 14.0f, widthPct = 0.88f;
};

/** Sidechain-ducking curve — an analytic depiction of the exact envelope
    math runRev/runDly actually apply: gain(t) = 1 - duckPct * exp(-t /
    releaseMs) after a transient, using the real Duck/DuckRelease values
    (same shape as TransferCurveView/FreqResponseView above: real
    parameters, not audio-reactive, not invented). */
class DuckingCurveView : public juce::Component
{
public:
    static constexpr int numPts = 96;

    void setCurve(float duckPct, float releaseMs)
    {
        float windowMs = juce::jmax(120.0f, releaseMs * 4.0f);
        for (int i = 0; i <= numPts; ++i)
        {
            float t = windowMs * (float) i / (float) numPts;
            curve[i] = 1.0f - duckPct * std::exp(-t / juce::jmax(1.0f, releaseMs));
        }
        depth = duckPct;
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

        auto mapY = [&] (float g01) { return b.getBottom() - juce::jlimit(0.0f, 1.0f, g01) * b.getHeight(); };

        juce::Path p;
        for (int i = 0; i <= numPts; ++i)
        {
            float x = b.getX() + b.getWidth() * (float) i / (float) numPts;
            float y = mapY(curve[i]);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setColour(XaLZaColour::accent2);
        g.strokePath(p, juce::PathStrokeType(2.0f));

        g.setColour(XaLZaColour::textMuted);
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        // Shortened from a longer sentence that overflowed this card's
        // narrow width and got clipped — confirmed from a screen recording.
        g.drawText("-" + juce::String((int) std::round(depth * 100)) + "% DIP",
                   b.reduced(3.0f), juce::Justification::topLeft);
    }

    float curve[numPts + 1] = {};
    float depth = 0.0f;
};

/** A genuine per-frequency decay HEATMAP of the reverb's own impulse
    response — not the plain "loudness over time" decay-tail line most
    reverb plugins (and this one, until now) show, but a real short-time
    FFT run across the exact IR buffer computed a few lines below (see
    irProbeReverb), revealing what Decay/Damping actually do: highs die
    out faster than lows. This is the one place in the whole plugin you
    can actually SEE that happening frequency-by-frequency, instead of
    reading a single blended loudness curve. Every column is a real FFT
    of the real IR at that point in time — recomputed on the same
    message-thread-only cadence as the plain IR waveform trace beside it,
    a few times a second, never touching the audio thread. */
class ReverbDecayHeatmapView : public juce::Component
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize  = 1 << fftOrder;   // 1024 — the IR is quasi-stationary noise, doesn't need more

    ReverbDecayHeatmapView() : fft(fftOrder), window((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        std::fill(std::begin(fftData), std::end(fftData), 0.0f);
    }

    // samples: the raw, full-resolution impulse-response buffer (NOT the
    // decimated display trace WaveformScope reads) — numSamples long.
    void setImpulseResponse(const float* samples, int numSamples, double sampleRate)
    {
        if (numSamples < fftSize || sampleRate <= 0.0)
            return;
        sampleRateHint = (float) sampleRate;

        int w = juce::jmax(1, getWidth()), h = juce::jmax(1, getHeight());
        if (image.isNull() || image.getWidth() != w || image.getHeight() != h)
            image = juce::Image(juce::Image::ARGB, w, h, true);
        if (w < 2 || h < 2)
            return;

        int hop = juce::jmax(1, (numSamples - fftSize) / juce::jmax(1, w - 1));

        juce::Image::BitmapData dst(image, juce::Image::BitmapData::writeOnly);

        for (int col = 0; col < w; ++col)
        {
            int start = juce::jlimit(0, numSamples - fftSize, col * hop);
            std::copy(samples + start, samples + start + fftSize, fftData);
            window.multiplyWithWindowingTable(fftData, (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData);

            for (int y = 0; y < h; ++y)
            {
                // Log-frequency, high frequencies at the top — same
                // convention as the LIM page's live Spectrogram.
                float t = 1.0f - (float) y / (float) (h - 1);
                float freqHz = 60.0f * std::pow(16000.0f / 60.0f, t);
                int bin = juce::jlimit(1, fftSize / 2 - 1, (int) (freqHz * (float) fftSize / sampleRateHint));
                float db = juce::Decibels::gainToDecibels(fftData[bin], -90.0f);
                float norm = juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 90.0f);
                dst.setPixelColour(col, y, heatColour(norm));
            }
        }

        repaint();
    }

private:
    // A cool-toned heat ramp (panel background through indigo and teal
    // into the plugin's own accent colours at full level) — deliberately
    // a different palette from the LIM Spectrogram's warm inferno ramp,
    // so the two heatmaps read as clearly different views at a glance.
    static juce::Colour heatColour(float t)
    {
        struct Stop { float pos; juce::Colour c; };
        static const Stop stops[] = {
            { 0.00f, XaLZaColour::panelBg },
            { 0.30f, juce::Colour(0xff203050) },
            { 0.60f, XaLZaColour::accent2 },
            { 0.85f, XaLZaColour::accent },
            { 1.00f, juce::Colour(0xfffff2c8) },
        };
        constexpr int numStops = (int) (sizeof(stops) / sizeof(stops[0]));
        for (int i = 1; i < numStops; ++i)
        {
            if (t <= stops[i].pos)
            {
                float span = juce::jmax(0.0001f, stops[i].pos - stops[i - 1].pos);
                float localT = juce::jlimit(0.0f, 1.0f, (t - stops[i - 1].pos) / span);
                return stops[i - 1].c.interpolatedWith(stops[i].c, localT);
            }
        }
        return stops[numStops - 1].c;
    }

    void paint(juce::Graphics& g) override
    {
        if (!image.isNull())
            g.drawImageAt(image, 0, 0);
        else
            g.fillAll(XaLZaColour::panelBg);
        g.setColour(XaLZaColour::border);
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override { image = {}; }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize];
    float sampleRateHint = 44100.0f;
    juce::Image image;
};

/** Composite Reverb page view: the new spectral decay heatmap above
    alongside a genuine Impulse Response waveform trace. The IR is
    computed by running a message-thread-only juce::dsp::Reverb instance
    (mirroring the exact same roomSize/damping the real audio-thread
    reverb is using right now) against a single unit impulse — a real IR
    of the actual algorithm and settings, never touching the audio thread
    and never fabricated. Used to pair a live decay-tail loudness line
    (the same generic envelope shape RES/GATE/COMP already had) with the
    IR trace; the heatmap is what's actually specific to a reverb. */
class ReverbView : public juce::Component
{
public:
    ReverbView() { addAndMakeVisible(heatmap); addAndMakeVisible(ir); }

    void setImpulseResponse(const float* samples, int numSamples, double sampleRate)
    {
        heatmap.setImpulseResponse(samples, numSamples, sampleRate);
    }
    void setIrWaveform(const float* samples) { ir.setSamples(samples); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        heatmap.setBounds(left);
        ir.setBounds(b);
    }

    ReverbDecayHeatmapView heatmap;
    WaveformScope ir;
};

/** Composite Doubler page view: the existing post-Doubler stereo
    goniometer alongside the real Per-Voice table (see PerVoiceTable
    above). */
class DoublerView : public juce::Component
{
public:
    DoublerView() { addAndMakeVisible(gonio); addAndMakeVisible(table); }

    void setPoints(const std::vector<std::pair<float, float>>& pts) { gonio.setPoints(pts); }
    void setVoiceData(int numVoices, float delayMs, float widthPct) { table.setData(numVoices, delayMs, widthPct); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        gonio.setBounds(left);
        table.setBounds(b);
    }

    Goniometer gonio;
    PerVoiceTable table;
};

/** Resonance's per-band suppression bars — one bar per currently-active
    band (real count from ResBands), height/label driven by that band's
    actual live cut depth (see PluginProcessor::getResBandCutDb). Enriches
    the single aggregate suppression line with what each individual
    adaptive notch is actually doing right now. */
class ResBandBars : public juce::Component
{
public:
    void setData(int newNumBands, const float* cutDbPerBand)
    {
        numBands = juce::jlimit(1, kMaxBands, newNumBands);
        for (int i = 0; i < kMaxBands; ++i)
            cutDb[i] = i < numBands ? cutDbPerBand[i] : 0.0f;
        repaint();
    }

private:
    static constexpr int kMaxBands = 5;

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        constexpr float rangeDb = 24.0f;
        auto area = getLocalBounds().reduced(6, 4).toFloat();
        float cellW = area.getWidth() / (float) numBands;
        g.setFont(juce::Font(juce::FontOptions(8.5f)));

        for (int i = 0; i < numBands; ++i)
        {
            auto cell = juce::Rectangle<float>(area.getX() + cellW * (float) i, area.getY(), cellW, area.getHeight());
            auto barCell = cell.reduced(cellW * 0.18f, 0.0f);
            float depthNorm = juce::jlimit(0.0f, 1.0f, -cutDb[i] / rangeDb);
            float labelH = 12.0f;
            auto barArea = barCell.withTrimmedBottom(labelH);
            float barH = barArea.getHeight() * depthNorm;
            auto bar = juce::Rectangle<float>(barArea.getX(), barArea.getBottom() - barH, barArea.getWidth(), barH);

            // A faint outline (not a filled translucent track) so the actual
            // bar height stays readable at a glance instead of looking
            // permanently "mostly full" — confirmed as a real legibility
            // problem from an actual screen recording, not just guessed.
            g.setColour(XaLZaColour::borderSoft);
            g.drawRect(barArea, 1.0f);
            g.setColour(XaLZaColour::accent);
            g.fillRect(bar);

            g.setColour(XaLZaColour::textMuted);
            g.drawText(juce::String(cutDb[i], 1), cell.removeFromBottom(labelH), juce::Justification::centred);
        }
    }

    int numBands = 1;
    float cutDb[kMaxBands] = {};
};

/** Composite Resonance page view: the existing aggregate suppression-depth
    line alongside the real per-band bars above. */
class ResonanceView : public juce::Component
{
public:
    ResonanceView() { addAndMakeVisible(graph); addAndMakeVisible(bars); }

    void push(float normDepth) { graph.push(normDepth); }
    void setBandData(int numBands, const float* cutDbPerBand) { bars.setData(numBands, cutDbPerBand); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        graph.setBounds(left);
        bars.setBounds(b);
    }

    EnvelopeGraph graph;
    ResBandBars bars;
};

/** Delay's "Tap Timeline" — a real depiction of the actual Pre-Delay/Time/
    Feedback values currently in effect: where the pre-delay tap sits, then
    each echo repeat's time position and relative level (feedback^n decay).
    Only buildable honestly now that Pre-Delay and (optionally) Time are
    real tempo-synced values rather than placeholders — see runDly. */
class TapTimelineView : public juce::Component
{
public:
    void setData(float preDelayMs, float timeMs, float feedbackPct)
    {
        preMs = preDelayMs;
        mainMs = juce::jmax(1.0f, timeMs);
        fbPct = juce::jlimit(0.0f, 0.95f, feedbackPct);
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        float windowMs = juce::jlimit(400.0f, 4000.0f, preMs + mainMs * 8.0f);
        auto baseY = b.getBottom() - 14.0f;
        auto mapX = [&] (float ms) { return b.getX() + b.getWidth() * juce::jlimit(0.0f, 1.0f, ms / windowMs); };

        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(b.getX(), baseY, b.getRight(), baseY, 1.0f);

        g.setFont(juce::Font(juce::FontOptions(8.0f)));

        if (preMs > 0.5f)
        {
            float x = mapX(preMs);
            g.setColour(XaLZaColour::accent2);
            g.fillRect(juce::Rectangle<float>(x - 1.5f, b.getY() + 4.0f, 3.0f, baseY - (b.getY() + 4.0f)));
            g.drawText("PRE", juce::Rectangle<float>(x - 16.0f, baseY + 1.0f, 32.0f, 12.0f), juce::Justification::centred);
        }

        float amp = 1.0f;
        for (int i = 1; i <= 8; ++i)
        {
            float tMs = preMs + mainMs * (float) i;
            if (tMs > windowMs) break;
            amp *= (i == 1 ? 1.0f : fbPct);
            float h = juce::jmax(4.0f, (baseY - (b.getY() + 4.0f)) * amp);
            float x = mapX(tMs);
            g.setColour(XaLZaColour::accent.withAlpha(juce::jlimit(0.15f, 1.0f, amp)));
            g.fillRect(juce::Rectangle<float>(x - 1.5f, baseY - h, 3.0f, h));
        }

        g.setColour(XaLZaColour::textMuted);
        g.drawText("TAP TIMELINE (0-" + juce::String((int) windowMs) + "ms)", b.reduced(3.0f), juce::Justification::topLeft);
    }

    float preMs = 0.0f, mainMs = 250.0f, fbPct = 0.3f;
};

/** Delay's ping-pong bounce path. A plain oscilloscope never shows the
    single thing this module's DSP is actually built around — see runDly's
    own comment, "ping-pong with spread, duck, and an auto-pan LFO" — since
    the auto-pan only ever shows up as a stereo balance shift, invisible in
    a mono trace. This draws the real post-delay echo waveform as two
    rails (L on top, R below), each weighted every frame by the EXACT same
    panL/panR law runDly applies per sample (0.5 -/+ 0.5*sin(phase)*0.6),
    so the rail that's momentarily louder visibly swells while the other
    thins out — plus a bouncing ball along the bottom whose X position is
    that same live phase read straight from the processor
    (getDlyPanPhase(), a genuine per-block value), so the ball's swing is
    always in exact sync with what's actually panning the repeats right
    now, not a separately clocked animation guessing at the LFO rate. */
class PingPongBounceView : public juce::Component
{
public:
    static constexpr int numPoints = 256;

    void setWaveform(const float* trace) { std::copy(trace, trace + numPoints, wave); repaint(); }
    void setPhase(float phaseRadians) { phase = phaseRadians; repaint(); }

private:
    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(full);
        g.setColour(XaLZaColour::border);
        g.drawRect(full, 1.0f);

        auto b = full.reduced(1.0f);
        auto ballRow = b.removeFromBottom(14.0f);
        b.removeFromBottom(2.0f);
        auto topRail = b.removeFromTop(b.getHeight() * 0.5f);
        auto botRail = b;

        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(topRail.getX(), topRail.getCentreY(), topRail.getRight(), topRail.getCentreY(), 0.4f);
        g.drawLine(botRail.getX(), botRail.getCentreY(), botRail.getRight(), botRail.getCentreY(), 0.4f);
        g.drawLine(b.getX(), topRail.getBottom(), b.getRight(), topRail.getBottom(), 0.5f);

        // Same per-sample pan law runDly applies, evaluated at the live
        // phase so the rail weighting visibly breathes together with the
        // ball below — a genuine reflection of the current auto-pan state.
        float panL = 0.5f - 0.5f * std::sin(phase) * 0.6f;
        float panR = 0.5f + 0.5f * std::sin(phase) * 0.6f;

        auto drawRail = [&] (juce::Rectangle<float> rail, float weight, juce::Colour c)
        {
            juce::Path p;
            for (int i = 0; i < numPoints; ++i)
            {
                float x = rail.getX() + rail.getWidth() * (float) i / (float) (numPoints - 1);
                float y = rail.getCentreY() - juce::jlimit(-1.0f, 1.0f, wave[i]) * weight * rail.getHeight() * 0.46f;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            g.setColour(c.withAlpha(juce::jlimit(0.28f, 1.0f, 0.3f + weight * 0.7f)));
            g.strokePath(p, juce::PathStrokeType(1.2f + weight * 0.7f));
        };

        drawRail(topRail, panL, XaLZaColour::accent2);
        drawRail(botRail, panR, XaLZaColour::accent);

        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(XaLZaColour::textMuted);
        g.drawText("L", topRail.reduced(3.0f, 1.0f), juce::Justification::topLeft);
        g.drawText("R", botRail.reduced(3.0f, 1.0f), juce::Justification::bottomLeft);

        g.setColour(XaLZaColour::borderSoft);
        g.drawLine(ballRow.getX(), ballRow.getCentreY(), ballRow.getRight(), ballRow.getCentreY(), 1.0f);
        float ballX = ballRow.getX() + ballRow.getWidth() * (0.5f + 0.5f * std::sin(phase) * 0.6f);
        auto ballColour = XaLZaColour::accent2.interpolatedWith(XaLZaColour::accent, 0.5f + 0.5f * std::sin(phase));
        g.setColour(ballColour);
        g.fillEllipse(ballX - 4.0f, ballRow.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(ballColour.withAlpha(0.35f));
        g.drawEllipse(ballX - 6.0f, ballRow.getCentreY() - 6.0f, 12.0f, 12.0f, 1.2f);
    }

    float wave[numPoints] = {};
    float phase = 0.0f;
};

/** Composite Delay page view: the ping-pong bounce path above alongside
    the real Tap Timeline. */
class DelayView : public juce::Component
{
public:
    DelayView() { addAndMakeVisible(bounce); addAndMakeVisible(timeline); }

    void setSamples(const float* s) { bounce.setWaveform(s); }
    void setPanPhase(float phaseRadians) { bounce.setPhase(phaseRadians); }
    void setTapData(float preDelayMs, float timeMs, float feedbackPct) { timeline.setData(preDelayMs, timeMs, feedbackPct); }

private:
    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft(b.getWidth() / 2);
        left.removeFromRight(4);
        bounce.setBounds(left);
        timeline.setBounds(b);
    }

    PingPongBounceView bounce;
    TapTimelineView timeline;
};

/** MACROS-page signal-chain flow strip — a whole-plugin overview that
    didn't exist anywhere before now: the 12 modules drawn as connected
    nodes in their REAL live processing order (XaLZaProcessor::
    getChainSlotAt — the exact order moveModule()/the Chain Order popup
    actually runs processBlock() in, so a reorder shows up here
    immediately, not the fixed original sequence). Each node's glow
    brightness is that module's own real post-processing output level
    (proc.getMeterDbL/R at tapForSlot(slot) — the identical MeterTap value
    that module's own page IN/OUT bars read), and a bypassed module draws
    hollow/dim from its own real bypass parameter — nothing here is
    inferred or animated. Clicking a node jumps straight to that module's
    page, the same "glance at the LED ladder, click the stage you care
    about" workflow a hardware channel strip gives you. */
class SignalChainFlowView : public juce::Component
{
public:
    static constexpr int kNumSlots = 12;

    struct NodeState { bool bypassed = false; float levelDb = -100.0f; };

    std::function<void(int)> onNodeClicked;   // called with the tab index to jump to

    void setTabIndices(const int* tabIdx) { std::copy(tabIdx, tabIdx + kNumSlots, tabIndexBySlot); }

    void setChainOrder(const int* slotOrder) { std::copy(slotOrder, slotOrder + kNumSlots, order); repaint(); }

    void setNodeState(int slotId, bool bypassed, float levelDb)
    {
        auto s = (size_t) juce::jlimit(0, kNumSlots - 1, slotId);
        nodes[s] = { bypassed, levelDb };
        repaint();
    }

private:
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(XaLZaColour::panelBg);
        g.fillRect(b);
        g.setColour(XaLZaColour::border);
        g.drawRect(b, 1.0f);

        auto area = b.reduced(8.0f, 3.0f);
        float nodeW = area.getWidth() / (float) kNumSlots;
        float cy = area.getY() + 11.0f;
        float r = 7.5f;

        for (int i = 0; i < kNumSlots - 1; ++i)
        {
            float x0 = area.getX() + nodeW * (i + 0.5f) + r + 2.0f;
            float x1 = area.getX() + nodeW * (i + 1.5f) - r - 2.0f;
            g.setColour(XaLZaColour::borderSoft);
            g.drawLine(x0, cy, x1, cy, 1.2f);
        }

        for (int i = 0; i < kNumSlots; ++i)
        {
            int slotId = order[i];
            auto& n = nodes[(size_t) juce::jlimit(0, kNumSlots - 1, slotId)];
            float cx = area.getX() + nodeW * (i + 0.5f);
            float norm = juce::jlimit(0.0f, 1.0f, (n.levelDb + 60.0f) / 60.0f);

            if (n.bypassed)
            {
                g.setColour(XaLZaColour::textMuted.withAlpha(0.55f));
                g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.3f);
            }
            else
            {
                auto core = XaLZaColour::accent2.interpolatedWith(XaLZaColour::accent, norm * 0.6f);
                g.setColour(core.withAlpha(0.10f + norm * 0.25f));
                g.fillEllipse(cx - r * 1.9f, cy - r * 1.9f, r * 3.8f, r * 3.8f);
                g.setColour(core);
                g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            }

            g.setFont(juce::Font(juce::FontOptions(7.2f).withStyle("Bold")));
            g.setColour(n.bypassed ? XaLZaColour::textMuted : XaLZaColour::textHi);
            g.drawText(shortCode(slotId), juce::Rectangle<float>(cx - nodeW * 0.5f, cy + r + 3.0f, nodeW, 11.0f),
                        juce::Justification::centred);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (onNodeClicked == nullptr)
            return;
        auto area = getLocalBounds().toFloat().reduced(8.0f, 3.0f);
        float nodeW = area.getWidth() / (float) kNumSlots;
        int i = (int) ((e.position.x - area.getX()) / juce::jmax(1.0f, nodeW));
        i = juce::jlimit(0, kNumSlots - 1, i);
        int slotId = order[i];
        onNodeClicked(tabIndexBySlot[(size_t) juce::jlimit(0, kNumSlots - 1, slotId)]);
    }

    static const char* shortCode(int slotId)
    {
        static const char* codes[kNumSlots] = { "PRE", "GATE", "ESS", "COMP", "OPTO", "EQ",
                                                  "RES", "SAT", "DBL", "REV", "DLY", "LIM" };
        return codes[(size_t) juce::jlimit(0, kNumSlots - 1, slotId)];
    }

    int order[kNumSlots] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int tabIndexBySlot[kNumSlots] = {};
    std::array<NodeState, kNumSlots> nodes;
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
        juce::Label capIn, capOut, dbIn, dbOut;
        GrMeter grMeter;
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
            grMeter.setVisible(v && grIndex >= 0);
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
    // Held-peak dB readouts under the master bars — every per-module meter
    // already paired its bar with a number; the master In/Out pair was the
    // one meter in the whole plugin showing a bar with no number next to
    // it at all.
    juce::Label masterDbIn, masterDbOut;
    Goniometer goniometer;
    juce::Label goniometerCap;
    // Real Pearson-correlation phase meter, computed from the same L/R
    // samples the Goniometer above already reads — see CorrelationMeterView.
    CorrelationMeterView correlationMeter;

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
    // module's input), wired in natural signal-chain order. Deliberately a
    // DIFFERENT SHAPE per module wherever the module's own behaviour calls
    // for one — a gate is a binary open/closed decision, a de-esser is a
    // single targeted frequency band, a saturator is a waveshaping curve —
    // instead of the same oscilloscope/envelope-line pair reused with
    // different data plugged in:
    //   PRE: analog-style VU gauge (input level)
    //   GATE: real open/closed activity strip (see GateActivityView)
    //   ESS: zoomed sibilance-band FFT spectrum with a live target marker
    //        (see DeEsserSpectrumView) — frequency-domain, not a time line
    //   COMP: gain-reduction + output level, dual history
    //   OPTO: post-Opto oscilloscope (real raw waveform)
    //   EQ: real FFT spectrum analyser (post-EQ signal)
    //   RES: dynamic-suppression depth history
    //   SAT: real analytic waveshaping transfer curve (see
    //        SaturationCurveView) — linear amplitude in-vs-out, not a time
    //        trace at all
    PreampView preView;
    juce::Label preVuTitle;
    GateActivityView gateView;
    juce::Label gateEnvTitle;
    DeEsserSpectrumView essView;
    juce::Label essEnvTitle;
    CompressorView compView;
    juce::Label compGrTitle;
    OptoView optoView;
    juce::Label optoScopeTitle;
    SpectrumAnalyzer eqSpectrum;
    juce::Label eqSpectrumTitle;
    ResonanceView resView;
    juce::Label resSuppressTitle;
    SaturatorView satView;
    juce::Label satScopeTitle;
    DoublerView dblView;
    juce::Label dblGoniometerTitle;
    ReverbView revView;
    juce::Label revDecayTitle;
    // Message-thread-only reverb instance for the Impulse Response trace —
    // never touches the audio thread. See ReverbView's comment.
    juce::dsp::Reverb irProbeReverb;
    juce::AudioBuffer<float> irProbeBuffer;
    int irProbeCounter = 0;
    DelayView dlyView;
    juce::Label dlyScopeTitle;
    LimiterAnalysisView limView;
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

    // Gate's real Lookahead toggle — see runGate.
    juce::TextButton gateLookaheadBtn { "LOOKAHEAD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateLookaheadAttachment;

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

    // Real host-tempo sync — see runDly. SYNC toggle swaps dlyTimeSeg (fixed
    // ms) for dlyNoteDivSeg (note divisions computed from live host BPM);
    // dlyPreDelaySeg is a separate, always-synced pre-delay tap.
    juce::TextButton dlySyncBtn { "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dlySyncAttachment;
    std::unique_ptr<SegButtonGroup> dlyNoteDivSeg;
    std::unique_ptr<SegButtonGroup> dlyPreDelaySeg;

    // Preamp's Pad/Phase/Phantom toggles and Impedance seg-group, and
    // Opto's Mode seg-group — all real DSP (Phantom is the one exception,
    // see PrePhantom's comment in Params.h).
    juce::TextButton prePadBtn { "PAD -20dB" }, prePhaseBtn { "PHASE" }, prePhantomBtn { "+48V" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        prePadAttachment, prePhaseAttachment, prePhantomAttachment;
    std::unique_ptr<SegButtonGroup> preImpedanceSeg;
    std::unique_ptr<SegButtonGroup> optoModeSeg;

    // Saturator's Character seg-group (Tube/Tape/Transistor/Diode) — real
    // distinct waveshapes, see runSat.
    std::unique_ptr<SegButtonGroup> satCharSeg;

    // Doubler's Voices seg-group (2/4/6/8) — real voice count, see runDbl —
    // fed into dblView's Per-Voice table (both read the same real data).
    std::unique_ptr<SegButtonGroup> dblVoicesSeg;

    // Reverb's Duck/DuckRelease knobs live in their own bordered "Sidechain
    // Ducking" card together with the real analytic curve above — matches
    // the mockup's dedicated ducking box instead of two knobs lost in the
    // flat 8-knob row (which is also what the Delay page's crowding taught
    // us to avoid).
    juce::Label revDuckCardTitle;
    DuckingCurveView revDuckCurve;
    CardFrame revDuckFrame;

    // Hybrid convolution: load a real impulse response file and blend it
    // with the algorithmic reverb via the Hybrid knob (pageKnobs[revTabIndex][9]).
    // revIrNameLabel shows the loaded file's name (or "NO IR LOADED"); the
    // decoded IR itself lives in the processor (see
    // XaLZaProcessor::loadImpulseResponseFile) — loadedIrMono here is a
    // SEPARATE, editor-owned decode of the same file, kept short (a few
    // seconds) purely so the Reverb page's decay heatmap can show the
    // real loaded impulse instead of the algorithmic-engine probe once
    // Hybrid > 0, without needing to read the DSP engine's internal state
    // back out (juce::dsp::Convolution doesn't expose that).
    juce::TextButton revLoadIrBtn { "LOAD IR" };
    juce::Label revIrNameLabel;
    std::vector<float> loadedIrMono;
    double loadedIrSr = 0.0;
    void loadImpulseResponseFile();

    // Resonance's Style and Bands seg-groups — real filter-topology
    // changes (Q/detect-width scaling and parallel-notch count), see
    // runRes.
    std::unique_ptr<SegButtonGroup> resStyleSeg;
    std::unique_ptr<SegButtonGroup> resBandsSeg;

    // De-esser's Band seg-group (S/T/CH) — real detector Q/freq-bias
    // change, see runEss.
    std::unique_ptr<SegButtonGroup> essBandSeg;

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

    // Macros-page signal-chain flow strip — whole-plugin overview of all
    // 12 modules in their real live order, each glowing with its own
    // real output level; click a node to jump to that page. See
    // SignalChainFlowView's own comment for what's genuinely measured.
    SignalChainFlowView chainFlow;
    juce::Label chainFlowCap;

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
