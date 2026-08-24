#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaEditor)
};
