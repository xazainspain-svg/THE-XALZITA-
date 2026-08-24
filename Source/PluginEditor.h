#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// Same dark palette as the Max for Live device (build_amxd.py PANEL_BG /
// PANEL_BG_RAISED / MACRO_ACCENT etc.) so the three deliverables (web
// mockup, M4L device, VST) all look like the same product.
namespace XaLZaColour
{
    static const juce::Colour panelBg     { 0xff252525 };
    static const juce::Colour panelRaised { 0xff2d2d2d };
    static const juce::Colour panelBorder { 0xff0f0f0f };
    static const juce::Colour titleText   { 0xffdbdbdb };
    static const juce::Colour labelText   { 0xffa1a1a1 };
    static const juce::Colour macroAccent { 0xfff09421 };
    static const juce::Colour fineFill    { 0xff7a7a7a };
}

/** Custom rotary knob drawing: a thin track, an accent- or gray-coloured
    value arc, a pointer line, and a dark cap — deliberately not the
    stock JUCE look, so it doesn't read as "generic Max/JUCE patch". */
class XaLZaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XaLZaLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

/**
    Editor layout mirrors the M4L device's presentation panel: a row of
    large "macro" knobs (one per module, amber accent) across the top of
    each module's column, fine-tune knobs stacked below them, and a
    separate raised "Master" utility panel on the right for In/Out gain
    and Stereo Width.
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

    struct ModuleColumn
    {
        juce::String title;
        KnobUI* macro = nullptr;
        std::vector<KnobUI*> fine;
    };

    KnobUI& addKnob(const juce::String& paramID, const juce::String& shortLabel, bool accent);

    static constexpr int titleBarH   = 30;
    static constexpr int marginX     = 12;
    static constexpr int marginY     = 8;
    static constexpr int masterW     = 130;
    static constexpr int macroLabelH = 15;
    static constexpr int macroKnobW  = 56;
    static constexpr int macroKnobH  = 76;
    static constexpr int fineLabelH  = 12;
    static constexpr int fineKnobW   = 34;
    static constexpr int fineKnobH   = 50;
    static constexpr int masterLabelH = 13;
    static constexpr int masterKnobW  = 44;
    static constexpr int masterKnobH  = 60;

    XaLZaProcessor& proc;
    XaLZaLookAndFeel laf;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::vector<ModuleColumn> modules;
    std::vector<KnobUI*> masterKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XaLZaEditor)
};
