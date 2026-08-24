#include "PluginEditor.h"

// =============================================================================
// Look and feel
// =============================================================================
XaLZaLookAndFeel::XaLZaLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, XaLZaColour::labelText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, XaLZaColour::labelText);
}

void XaLZaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    bool accent = (bool) slider.getProperties().getWithDefault("accent", false);
    juce::Colour fillColour = accent ? XaLZaColour::macroAccent : XaLZaColour::fineFill;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(XaLZaColour::panelBorder);
    g.strokePath(track, juce::PathStrokeType(radius * 0.22f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(fillColour);
    g.strokePath(valueArc, juce::PathStrokeType(radius * 0.22f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    juce::Path pointer;
    auto pointerLength = radius * 0.62f;
    auto pointerThickness = 2.2f;
    pointer.addRectangle(-pointerThickness * 0.5f, -radius * 0.86f, pointerThickness, pointerLength);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.fillPath(pointer);

    g.setColour(juce::Colour(0xff1a1a1a));
    auto capR = radius * 0.38f;
    g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
}

// =============================================================================
// Editor
// =============================================================================
XaLZaEditor::KnobUI& XaLZaEditor::addKnob(const juce::String& paramID, const juce::String& shortLabel,
                                           bool accent)
{
    auto k = std::make_unique<KnobUI>();
    k->slider.getProperties().set("accent", accent);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, accent ? 50 : 40, 15);
    addAndMakeVisible(k->slider);

    k->label.setText(shortLabel, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, accent ? XaLZaColour::macroAccent : XaLZaColour::labelText);
    {
        auto fo = juce::FontOptions(accent ? 12.5f : 10.5f);
        if (accent)
            fo = fo.withStyle("Bold");
        k->label.setFont(juce::Font(fo));
    }
    addAndMakeVisible(k->label);

    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramID, k->slider);

    knobs.push_back(std::move(k));
    return *knobs.back();
}

XaLZaEditor::XaLZaEditor(XaLZaProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);

    // ---- Master utility knobs (not macro-linked) ----
    masterKnobs.push_back(&addKnob(XID::MasterInGain, "In Gain", false));
    masterKnobs.push_back(&addKnob(XID::MasterOutGain, "Out Gain", false));
    masterKnobs.push_back(&addKnob(XID::MasterWidth, "Width", false));

    // ---- The 7 macro-bearing modules, left to right = signal order ----
    {
        ModuleColumn m; m.title = "PRE";
        m.macro = &addKnob(XID::PreMacro, "PRE", true);
        m.fine  = { &addKnob(XID::PreGain, "Gain", false),
                    &addKnob(XID::PreChar, "Char", false),
                    &addKnob(XID::PreHPF, "HPF", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "COMP";
        m.macro = &addKnob(XID::CompMacro, "COMP", true);
        m.fine  = { &addKnob(XID::CompThresh, "Thresh", false),
                    &addKnob(XID::CompRatio, "Ratio", false),
                    &addKnob(XID::CompMakeup, "Makeup", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "EQ";
        m.macro = &addKnob(XID::EqMacro, "EQ", true);
        m.fine  = { &addKnob(XID::EqLowGain, "Low", false),
                    &addKnob(XID::EqHighGain, "High", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "SAT";
        m.macro = &addKnob(XID::SatMacro, "SAT", true);
        m.fine  = { &addKnob(XID::SatDrive, "Drive", false),
                    &addKnob(XID::SatMix, "Mix", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "REV";
        m.macro = &addKnob(XID::RevMacro, "REV", true);
        m.fine  = { &addKnob(XID::RevSize, "Size", false),
                    &addKnob(XID::RevMix, "Mix", false),
                    &addKnob(XID::RevDuck, "Duck", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "DLY";
        m.macro = &addKnob(XID::DlyMacro, "DLY", true);
        m.fine  = { &addKnob(XID::DlyTime, "Time", false),
                    &addKnob(XID::DlyFeedback, "Fdbk", false),
                    &addKnob(XID::DlyMix, "Mix", false),
                    &addKnob(XID::DlyDuck, "Duck", false) };
        modules.push_back(std::move(m));
    }
    {
        ModuleColumn m; m.title = "LIM";
        m.macro = &addKnob(XID::LimMacro, "LIM", true);
        m.fine  = { &addKnob(XID::LimCeiling, "Ceil", false),
                    &addKnob(XID::LimInputGain, "In Gn", false) };
        modules.push_back(std::move(m));
    }

    setSize(900, 420);
}

XaLZaEditor::~XaLZaEditor()
{
    setLookAndFeel(nullptr);
}

void XaLZaEditor::paint(juce::Graphics& g)
{
    g.fillAll(XaLZaColour::panelBg);

    auto full = getLocalBounds();
    auto titleArea = full.removeFromTop(titleBarH);

    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(titleArea);
    g.setColour(XaLZaColour::panelBorder);
    g.drawLine(0.0f, (float) titleBarH, (float) getWidth(), (float) titleBarH, 1.0f);

    g.setColour(XaLZaColour::titleText);
    g.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
    g.drawText("THE XALZA", titleArea.reduced(10, 0), juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(XaLZaColour::labelText);
    g.drawText("Vocal Chain", titleArea.reduced(10, 0), juce::Justification::centredRight);

    // Master panel background
    auto body = getLocalBounds();
    body.removeFromTop(titleBarH);
    body.reduce(marginX, marginY);
    auto masterPanel = body.removeFromRight(masterW);
    g.setColour(XaLZaColour::panelRaised);
    g.fillRoundedRectangle(masterPanel.toFloat(), 4.0f);
    g.setColour(XaLZaColour::panelBorder);
    g.drawRoundedRectangle(masterPanel.toFloat(), 4.0f, 1.0f);
    g.setColour(XaLZaColour::labelText);
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.drawText("MASTER", masterPanel.removeFromTop(20), juce::Justification::centred);

    // Faint divider between macro row and fine-tune rows
    body.removeFromRight(8);
    auto dividerY = body.getY() + macroLabelH + macroKnobH + 2;
    g.setColour(XaLZaColour::panelBorder);
    g.drawLine((float) body.getX(), (float) dividerY, (float) body.getRight(), (float) dividerY, 1.0f);
}

void XaLZaEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(titleBarH);
    area.reduce(marginX, marginY);

    auto masterArea = area.removeFromRight(masterW);
    area.removeFromRight(8);

    const int numModules = (int) modules.size();
    const int colW = numModules > 0 ? area.getWidth() / numModules : area.getWidth();

    auto layoutOne = [](KnobUI& k, juce::Rectangle<int> cell, int labelH, int kw, int kh)
    {
        auto lbl = cell.removeFromTop(labelH);
        k.label.setBounds(lbl);
        k.slider.setBounds(cell.withSizeKeepingCentre(kw, kh));
    };

    for (int i = 0; i < numModules; ++i)
    {
        auto& m = modules[(size_t) i];
        auto col = area.removeFromLeft(colW).reduced(4, 0);

        auto macroCell = col.removeFromTop(macroLabelH + macroKnobH);
        layoutOne(*m.macro, macroCell, macroLabelH, macroKnobW, macroKnobH);

        col.removeFromTop(6);

        for (auto* fk : m.fine)
        {
            auto cell = col.removeFromTop(fineLabelH + fineKnobH);
            layoutOne(*fk, cell, fineLabelH, fineKnobW, fineKnobH);
        }
    }

    masterArea.reduce(4, 0);
    masterArea.removeFromTop(20); // room for "MASTER" caption painted in paint()
    for (auto* mk : masterKnobs)
    {
        auto cell = masterArea.removeFromTop(masterLabelH + masterKnobH);
        layoutOne(*mk, cell, masterLabelH, masterKnobW, masterKnobH);
    }
}
