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

/**
    Editor layout mirrors the web mockup: a narrow vertical tab rail on
    the left (MACROS + the 12 modules, in the mockup's own tab order),
    and a content area on the right that shows either the Macros
    overview (12 macro knobs + a Master mini-panel) or the selected
    module's fine-tune knobs — one page visible at a time, same as the
    mockup's single-panel-per-tab layout.
*/
class XaLZaEditor : public juce::AudioProcessorEditor
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

    KnobUI& addKnob(const juce::String& paramID, const juce::String& shortLabel, bool accent);
    void showPage(int index);
    void layoutKnobRow(const std::vector<KnobUI*>& row, juce::Rectangle<int> area,
                        int labelH, int knobW, int knobH, int cellW);

    static constexpr int titleBarH  = 30;
    static constexpr int railW      = 68;
    static constexpr int marginX    = 14;
    static constexpr int marginY    = 12;
    static constexpr int masterW    = 130;

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

    XaLZaProcessor& proc;
    XaLZaLookAndFeel laf;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::vector<juce::String> tabNames;
    std::vector<std::vector<KnobUI*>> pageKnobs;   // one entry per tab
    std::vector<KnobUI*> masterKnobs;              // shown on the Macros page only
    std::vector<std::unique_ptr<juce::TextButton>> tabButtons;
    int currentTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaEditor)
};
