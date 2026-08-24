#include "PluginEditor.h"

// =============================================================================
// Look and feel
// =============================================================================
XaLZaLookAndFeel::XaLZaLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, XaLZaColour::textLabel);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, XaLZaColour::textLabel);
    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonOnColourId, XaLZaColour::panelControl);
    setColour(juce::TextButton::textColourOffId, XaLZaColour::textMuted);
    setColour(juce::TextButton::textColourOnId, XaLZaColour::accent);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
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
    juce::Colour fillColour = accent ? XaLZaColour::accent : XaLZaColour::accent2;

    // face (matches .knob-svg circle.face)
    g.setColour(XaLZaColour::panelControl);
    g.fillEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.72f, radius * 1.44f, radius * 1.44f);
    g.setColour(XaLZaColour::border);
    g.drawEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.72f, radius * 1.44f, radius * 1.44f, 1.0f);

    // track (matches circle.trk)
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(XaLZaColour::border);
    g.strokePath(track, juce::PathStrokeType(radius * 0.16f, juce::PathStrokeType::mitered,
                                              juce::PathStrokeType::butt));

    // value arc (matches circle.fil)
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(fillColour);
    g.strokePath(valueArc, juce::PathStrokeType(radius * 0.16f, juce::PathStrokeType::mitered,
                                                 juce::PathStrokeType::butt));

    // pointer (matches line.ptr)
    juce::Path pointer;
    auto pointerLength = radius * 0.6f;
    auto pointerThickness = 2.0f;
    pointer.addRectangle(-pointerThickness * 0.5f, -radius * 0.68f, pointerThickness, pointerLength);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
    g.setColour(XaLZaColour::textHi);
    g.fillPath(pointer);
}

// =============================================================================
// Editor
// =============================================================================
namespace
{
    // Exact-ID unit lookup (not a substring heuristic — "EqLow"/"EqHigh" are
    // dB gains while "ResLow"/"ResHigh" are Hz cutoffs, so guessing from
    // words like "Low"/"High" alone would mislabel one of the two) so every
    // knob's textbox reads like a real control instead of a bare number.
    juce::String inferUnitSuffix(const juce::String& id)
    {
        static const std::map<juce::String, juce::String> units = {
            { XID::MasterInGain, " dB" }, { XID::MasterOutGain, " dB" }, { XID::MasterWidth, " %" },
            { XID::PreGain, " dB" }, { XID::PreChar, " %" }, { XID::PreHPF, " Hz" }, { XID::PreMacro, " %" },
            { XID::GateThresh, " dB" }, { XID::GateRange, " dB" }, { XID::GateAttack, " ms" },
            { XID::GateHold, " ms" }, { XID::GateRelease, " ms" }, { XID::GateMacro, " %" },
            { XID::EssThresh, " dB" }, { XID::EssRange, " dB" }, { XID::EssFreq, " Hz" }, { XID::EssMacro, " %" },
            { XID::CompThresh, " dB" }, { XID::CompMakeup, " dB" }, { XID::CompAttack, " ms" },
            { XID::CompRelease, " ms" }, { XID::CompMix, " %" }, { XID::CompRatio, ":1" }, { XID::CompMacro, " %" },
            { XID::OptoReduction, " %" }, { XID::OptoGain, " dB" }, { XID::OptoMix, " %" }, { XID::OptoMacro, " %" },
            { XID::EqLow, " dB" }, { XID::EqMid, " dB" }, { XID::EqHigh, " dB" }, { XID::EqMacro, " %" },
            { XID::ResAmount, " %" }, { XID::ResSharpness, " %" }, { XID::ResReactivity, " %" },
            { XID::ResNotchLimit, " dB" }, { XID::ResLow, " Hz" }, { XID::ResHigh, " Hz" }, { XID::ResMacro, " %" },
            { XID::SatDrive, " %" }, { XID::SatTone, " dB" }, { XID::SatCeiling, " dB" }, { XID::SatMix, " %" },
            { XID::SatMacro, " %" },
            { XID::DblDetune, " %" }, { XID::DblWidth, " %" }, { XID::DblDelay, " ms" }, { XID::DblMix, " %" },
            { XID::DblMacro, " %" },
            { XID::RevSize, " %" }, { XID::RevDecay, " s" }, { XID::RevPreDelay, " ms" }, { XID::RevMix, " %" },
            { XID::RevDuck, " %" }, { XID::RevDuckRelease, " ms" }, { XID::RevMacro, " %" },
            { XID::DlyTime, " ms" }, { XID::DlyFeedback, " %" }, { XID::DlySpread, " %" }, { XID::DlyMix, " %" },
            { XID::DlyDuck, " %" }, { XID::DlyDuckRelease, " ms" }, { XID::DlyPanRate, " Hz" }, { XID::DlyMacro, " %" },
            { XID::LimInputGain, " dB" }, { XID::LimCeiling, " dB" }, { XID::LimRelease, " ms" },
            { XID::LimClip, " %" }, { XID::LimMacro, " %" },
        };
        auto it = units.find(id);
        return it != units.end() ? it->second : juce::String();
    }
}

XaLZaEditor::KnobUI& XaLZaEditor::addKnob(const juce::String& paramID, const juce::String& shortLabel,
                                           bool accent)
{
    auto k = std::make_unique<KnobUI>();
    k->paramID = paramID;
    k->macroID = accent ? juce::String() : macroForParam(paramID);   // macro knobs themselves have no "override" state
    k->slider.getProperties().set("accent", accent);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, accent ? 54 : 46, 15);
    k->slider.setTextValueSuffix(inferUnitSuffix(paramID));
    // Double-click any knob to snap it back to its default value — real
    // control, not just decoration.
    if (auto* param = proc.apvts.getParameter(paramID))
        k->slider.setDoubleClickReturnValue(true, (double) param->convertFrom0to1(param->getDefaultValue()));
    addChildComponent(k->slider);   // starts hidden — showPage() reveals the current page's knobs

    k->label.setText(shortLabel, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, accent ? XaLZaColour::accent : XaLZaColour::textLabel);
    {
        auto fo = juce::FontOptions(accent ? 12.0f : 10.5f);
        if (accent)
            fo = fo.withStyle("Bold");
        k->label.setFont(juce::Font(fo));
    }
    addChildComponent(k->label);

    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramID, k->slider);

    knobs.push_back(std::move(k));
    return *knobs.back();
}

XaLZaEditor::ModuleMeterUI& XaLZaEditor::addModuleMeter(const juce::String& tab, int tapIn, int tapOut, int grIndex,
                                                          const juce::String& bypassParamID)
{
    auto it = std::find(tabNames.begin(), tabNames.end(), tab);
    jassert(it != tabNames.end());
    auto idx = (size_t) std::distance(tabNames.begin(), it);

    auto mm = std::make_unique<ModuleMeterUI>();
    mm->tapIn = tapIn;
    mm->tapOut = tapOut;
    mm->grIndex = grIndex;

    mm->bypassBtn.setClickingTogglesState(true);
    mm->bypassBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::danger);
    mm->bypassBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::textHi);
    mm->bypassBtn.setColour(juce::TextButton::textColourOffId, XaLZaColour::textMuted);
    mm->bypassBtn.setTooltip("Bypass this module only");
    addChildComponent(mm->bypassBtn);
    mm->bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, bypassParamID, mm->bypassBtn);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text, bool bold)
    {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions(9.0f).withStyle(bold ? "Bold" : "Regular")));
        l.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
        addChildComponent(l);
    };
    setupLabel(mm->capIn, "IN", true);
    setupLabel(mm->capOut, "OUT", true);
    setupLabel(mm->dbIn, "-inf", false);
    setupLabel(mm->dbOut, "-inf", false);

    mm->grLabel.setJustificationType(juce::Justification::centred);
    mm->grLabel.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    mm->grLabel.setColour(juce::Label::textColourId, XaLZaColour::accent2);
    addChildComponent(mm->grLabel);

    addChildComponent(mm->meterIn);
    addChildComponent(mm->meterOut);

    auto* ptr = mm.get();
    moduleMeterStorage.push_back(std::move(mm));
    moduleMeterByTab[idx] = ptr;
    return *ptr;
}

XaLZaEditor::XaLZaEditor(XaLZaProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);

    // ---- Tab order matches the mockup's own tab strip exactly ----
    tabNames = { "MACROS", "PRE", "COMP", "OPTO", "EQ", "SAT", "REV", "DLY", "DBL", "RES", "GATE", "ESS", "LIM" };
    pageKnobs.resize(tabNames.size());
    moduleMeterByTab.resize(tabNames.size(), nullptr);

    // ---- Page 0: MACROS overview — one knob per module + Master panel ----
    struct MacroDef { juce::String id; juce::String label; };
    static const std::vector<MacroDef> macroDefs = {
        { XID::PreMacro,  "PRE" },  { XID::CompMacro, "COMP" }, { XID::OptoMacro, "OPTO" },
        { XID::EqMacro,   "EQ" },   { XID::SatMacro,  "SAT" },  { XID::RevMacro,  "REV" },
        { XID::DlyMacro,  "DLY" },  { XID::DblMacro,  "DBL" },  { XID::ResMacro,  "RES" },
        { XID::GateMacro, "GATE" }, { XID::EssMacro,  "ESS" },  { XID::LimMacro,  "LIM" },
    };
    for (auto& md : macroDefs)
        pageKnobs[0].push_back(&addKnob(md.id, md.label, true));

    masterKnobs.push_back(&addKnob(XID::MasterInGain, "In Gain", false));
    masterKnobs.push_back(&addKnob(XID::MasterOutGain, "Out Gain", false));
    masterKnobs.push_back(&addKnob(XID::MasterWidth, "Width", false));

    // ---- One page per module, fine-tune knobs only (macro lives on page 0) ----
    auto addPage = [this](const juce::String& tab, std::initializer_list<std::pair<juce::String, juce::String>> params)
    {
        auto it = std::find(tabNames.begin(), tabNames.end(), tab);
        jassert(it != tabNames.end());
        auto idx = (size_t) std::distance(tabNames.begin(), it);
        for (auto& pr : params)
            pageKnobs[idx].push_back(&addKnob(pr.first, pr.second, false));
    };

    addPage("PRE",  { { XID::PreGain, "Gain" }, { XID::PreChar, "Char" }, { XID::PreHPF, "HPF" } });
    addPage("COMP", { { XID::CompThresh, "Thresh" }, { XID::CompMakeup, "Makeup" }, { XID::CompAttack, "Attack" },
                       { XID::CompRelease, "Release" }, { XID::CompMix, "Mix" }, { XID::CompRatio, "Ratio" } });
    addPage("OPTO", { { XID::OptoReduction, "Reduction" }, { XID::OptoGain, "Gain" }, { XID::OptoMix, "Mix" } });
    addPage("EQ",   { { XID::EqLow, "Low" }, { XID::EqMid, "Mid" }, { XID::EqHigh, "High" } });
    addPage("SAT",  { { XID::SatDrive, "Drive" }, { XID::SatTone, "Tone" }, { XID::SatCeiling, "Ceiling" }, { XID::SatMix, "Mix" } });
    addPage("REV",  { { XID::RevSize, "Size" }, { XID::RevDecay, "Decay" }, { XID::RevPreDelay, "PreDelay" },
                       { XID::RevMix, "Mix" }, { XID::RevDuck, "Duck" }, { XID::RevDuckRelease, "DuckRel" } });
    addPage("DLY",  { { XID::DlyTime, "Time" }, { XID::DlyFeedback, "Fdbk" }, { XID::DlySpread, "Spread" },
                       { XID::DlyMix, "Mix" }, { XID::DlyDuck, "Duck" }, { XID::DlyDuckRelease, "DuckRel" },
                       { XID::DlyPanRate, "PanRate" } });
    addPage("DBL",  { { XID::DblDetune, "Detune" }, { XID::DblWidth, "Width" }, { XID::DblDelay, "Delay" }, { XID::DblMix, "Mix" } });
    addPage("RES",  { { XID::ResAmount, "Amount" }, { XID::ResSharpness, "Sharp" }, { XID::ResReactivity, "React" },
                       { XID::ResNotchLimit, "NotchLim" }, { XID::ResLow, "Low" }, { XID::ResHigh, "High" } });
    addPage("GATE", { { XID::GateThresh, "Thresh" }, { XID::GateRange, "Range" }, { XID::GateAttack, "Attack" },
                       { XID::GateHold, "Hold" }, { XID::GateRelease, "Release" } });
    addPage("ESS",  { { XID::EssThresh, "Thresh" }, { XID::EssRange, "Range" }, { XID::EssFreq, "Freq" } });
    addPage("LIM",  { { XID::LimInputGain, "InGain" }, { XID::LimCeiling, "Ceiling" }, { XID::LimRelease, "Release" }, { XID::LimClip, "Clip" } });

    // ---- Per-module IN/OUT meters + GR readouts, tapping the real serial chain ----
    addModuleMeter("PRE",  (int) XaLZaProcessor::TapIn,   (int) XaLZaProcessor::TapPre,  -1, XID::PreBypass);
    addModuleMeter("GATE", (int) XaLZaProcessor::TapPre,  (int) XaLZaProcessor::TapGate, -1, XID::GateBypass);
    addModuleMeter("ESS",  (int) XaLZaProcessor::TapGate, (int) XaLZaProcessor::TapEss,  -1, XID::EssBypass);
    addModuleMeter("COMP", (int) XaLZaProcessor::TapEss,  (int) XaLZaProcessor::TapComp,  0, XID::CompBypass);
    addModuleMeter("OPTO", (int) XaLZaProcessor::TapComp, (int) XaLZaProcessor::TapOpto,  1, XID::OptoBypass);
    addModuleMeter("EQ",   (int) XaLZaProcessor::TapOpto, (int) XaLZaProcessor::TapEq,   -1, XID::EqBypass);
    addModuleMeter("RES",  (int) XaLZaProcessor::TapEq,   (int) XaLZaProcessor::TapRes,  -1, XID::ResBypass);
    addModuleMeter("SAT",  (int) XaLZaProcessor::TapRes,  (int) XaLZaProcessor::TapSat,  -1, XID::SatBypass);
    addModuleMeter("DBL",  (int) XaLZaProcessor::TapSat,  (int) XaLZaProcessor::TapDbl,  -1, XID::DblBypass);
    addModuleMeter("REV",  (int) XaLZaProcessor::TapDbl,  (int) XaLZaProcessor::TapRev,  -1, XID::RevBypass);
    addModuleMeter("DLY",  (int) XaLZaProcessor::TapRev,  (int) XaLZaProcessor::TapDly,  -1, XID::DlyBypass);
    addModuleMeter("LIM",  (int) XaLZaProcessor::TapDly,  (int) XaLZaProcessor::TapLim,   2, XID::LimBypass);

    // ---- Master mini-panel visualisers: real In/Out meters + a stereo goniometer ----
    auto setupCap = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
        addChildComponent(l);
    };
    setupCap(masterCapIn, "IN");
    setupCap(masterCapOut, "OUT");
    setupCap(goniometerCap, "GONIOMETER");
    addChildComponent(masterMeterIn);
    addChildComponent(masterMeterOut);
    addChildComponent(goniometer);

    bypassSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    bypassSummaryLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    bypassSummaryLabel.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
    bypassSummaryLabel.setText("ALL MODULES ACTIVE", juce::dontSendNotification);
    addChildComponent(bypassSummaryLabel);

    // ---- Tab rail buttons ----
    for (size_t i = 0; i < tabNames.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton>(tabNames[i]);
        btn->setClickingTogglesState(false);
        auto idx = (int) i;
        btn->onClick = [this, idx] { showPage(idx); };
        addAndMakeVisible(*btn);
        tabButtons.push_back(std::move(btn));
    }

    // ---- Global bypass button (title bar) — real dry passthrough ----
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
    bypassButton.setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::MasterBypass, bypassButton);

    // ---- Per-module "big" post-process visualisers, natural chain order ----
    auto setupVizLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
        addChildComponent(l);
    };
    setupVizLabel(preVuTitle,         "INPUT LEVEL - VU");
    setupVizLabel(gateEnvTitle,       "GATE REDUCTION");
    setupVizLabel(essEnvTitle,        "SIBILANCE BAND + REDUCTION");
    setupVizLabel(compGrTitle,        "GAIN REDUCTION + OUTPUT");
    setupVizLabel(optoScopeTitle,     "POST-OPTO OSCILLOSCOPE");
    setupVizLabel(eqSpectrumTitle,    "RESPONSE SPECTRUM (POST-EQ)");
    setupVizLabel(resSuppressTitle,   "DYNAMIC SUPPRESSION");
    setupVizLabel(satScopeTitle,      "WAVEFORM: IN VS OUT");
    setupVizLabel(dblGoniometerTitle, "STEREO FIELD (POST-DOUBLER)");
    setupVizLabel(revDecayTitle,      "DECAY TAIL (POST-REVERB LEVEL)");
    setupVizLabel(dlyScopeTitle,      "ECHO WAVEFORM (POST-DELAY)");
    setupVizLabel(limViewTitle,       "BRICKWALL OUTPUT + LOUDNESS");
    addChildComponent(preVu);
    addChildComponent(gateEnvGraph);
    addChildComponent(essEnvGraph);
    addChildComponent(compGrGraph);
    addChildComponent(optoScope);
    addChildComponent(eqSpectrum);
    addChildComponent(resSuppressGraph);
    addChildComponent(satScope);
    addChildComponent(dblGoniometer);
    addChildComponent(revDecayGraph);
    addChildComponent(dlyScope);
    addChildComponent(limView);
    eqSpectrum.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);

    auto resolveTab = [this] (const juce::String& tab, int fallback) -> int
    {
        auto it = std::find(tabNames.begin(), tabNames.end(), tab);
        return it != tabNames.end() ? (int) std::distance(tabNames.begin(), it) : fallback;
    };
    preTabIndex  = resolveTab("PRE",  preTabIndex);
    gateTabIndex = resolveTab("GATE", gateTabIndex);
    essTabIndex  = resolveTab("ESS",  essTabIndex);
    compTabIndex = resolveTab("COMP", compTabIndex);
    optoTabIndex = resolveTab("OPTO", optoTabIndex);
    eqTabIndex   = resolveTab("EQ",   eqTabIndex);
    resTabIndex  = resolveTab("RES",  resTabIndex);
    satTabIndex  = resolveTab("SAT",  satTabIndex);
    dblTabIndex  = resolveTab("DBL",  dblTabIndex);
    revTabIndex  = resolveTab("REV",  revTabIndex);
    dlyTabIndex  = resolveTab("DLY",  dlyTabIndex);
    limTabIndex  = resolveTab("LIM",  limTabIndex);

    bigViz = {
        { preTabIndex,  &preVu,            &preVuTitle },
        { gateTabIndex, &gateEnvGraph,     &gateEnvTitle },
        { essTabIndex,  &essEnvGraph,      &essEnvTitle },
        { compTabIndex, &compGrGraph,      &compGrTitle },
        { optoTabIndex, &optoScope,        &optoScopeTitle },
        { eqTabIndex,   &eqSpectrum,       &eqSpectrumTitle },
        { resTabIndex,  &resSuppressGraph, &resSuppressTitle },
        { satTabIndex,  &satScope,         &satScopeTitle },
        { dblTabIndex,  &dblGoniometer,    &dblGoniometerTitle },
        { revTabIndex,  &revDecayGraph,    &revDecayTitle },
        { dlyTabIndex,  &dlyScope,         &dlyScopeTitle },
        { limTabIndex,  &limView,          &limViewTitle },
    };

    // ---- Factory preset picker (title bar) — drives the 12 macro knobs ----
    presetBox.setTextWhenNothingSelected("PRESETS");
    presetBox.setJustificationType(juce::Justification::centred);
    {
        int i = 1;
        for (auto& preset : xalzaFactoryPresets())
            presetBox.addItem(preset.name, i++);
    }
    presetBox.onChange = [this]
    {
        int id = presetBox.getSelectedId();
        if (id > 0)
            applyPreset(id - 1);
    };
    addAndMakeVisible(presetBox);

    savePresetBtn.setTooltip("Save the full current plugin state as a preset file");
    loadPresetBtn.setTooltip("Load a previously saved preset file");
    savePresetBtn.onClick = [this] { savePresetToFile(); };
    loadPresetBtn.onClick = [this] { loadPresetFromFile(); };
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(loadPresetBtn);

    setSize(900, 560);
    showPage(0);
    startTimerHz(30);
}

namespace
{
    juce::File xalzaPresetsFolder()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("XaLZa Presets");
        dir.createDirectory();
        return dir;
    }
}

void XaLZaEditor::savePresetToFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save XaLZa preset", xalzaPresetsFolder(), "*.xalzapreset");
    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    fileChooser->launchAsync(flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension("xalzapreset");

        auto state = proc.apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml != nullptr)
            xml->writeTo(file);
    });
}

void XaLZaEditor::loadPresetFromFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load XaLZa preset", xalzaPresetsFolder(), "*.xalzapreset");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File() || !file.existsAsFile())
            return;

        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml != nullptr && xml->hasTagName(proc.apvts.state.getType()))
            proc.apvts.replaceState(juce::ValueTree::fromXml(*xml));
    });
}

void XaLZaEditor::applyPreset(int presetIndex)
{
    auto& presets = xalzaFactoryPresets();
    if (presetIndex < 0 || presetIndex >= (int) presets.size())
        return;

    // Driving each macro parameter (rather than every individual real
    // parameter underneath it) is enough: MacroTouchTracker always treats
    // "just touched" as the winner, so this reconfigures the whole chain
    // through the exact same signal path a manual macro turn would use.
    for (auto& mp : presets[(size_t) presetIndex].macroPercents)
    {
        if (auto* param = proc.apvts.getParameter(mp.first))
        {
            float norm = juce::jlimit(0.0f, 1.0f, param->convertTo0to1(mp.second));
            param->beginChangeGesture();
            param->setValueNotifyingHost(norm);
            param->endChangeGesture();
        }
    }
}

XaLZaEditor::~XaLZaEditor()
{
    setLookAndFeel(nullptr);
}

void XaLZaEditor::showPage(int index)
{
    currentTab = juce::jlimit(0, (int) tabNames.size() - 1, index);

    for (size_t i = 0; i < pageKnobs.size(); ++i)
    {
        bool visible = (int) i == currentTab;
        for (auto* k : pageKnobs[i])
        {
            k->slider.setVisible(visible);
            k->label.setVisible(visible);
        }
    }
    for (auto* mk : masterKnobs)
    {
        mk->slider.setVisible(currentTab == 0);
        mk->label.setVisible(currentTab == 0);
    }

    bool onMacros = (currentTab == 0);
    masterMeterIn.setVisible(onMacros);
    masterMeterOut.setVisible(onMacros);
    masterCapIn.setVisible(onMacros);
    masterCapOut.setVisible(onMacros);
    goniometer.setVisible(onMacros);
    goniometerCap.setVisible(onMacros);
    bypassSummaryLabel.setVisible(onMacros);

    for (auto* mm : moduleMeterByTab)
        if (mm != nullptr)
            mm->setVisible(false);
    if (currentTab >= 0 && currentTab < (int) moduleMeterByTab.size() && moduleMeterByTab[(size_t) currentTab] != nullptr)
        moduleMeterByTab[(size_t) currentTab]->setVisible(true);

    for (size_t i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setToggleState((int) i == currentTab, juce::dontSendNotification);

    for (auto& bv : bigViz)
    {
        bool on = (currentTab == bv.tabIndex);
        bv.comp->setVisible(on);
        bv.title->setVisible(on);
    }

    resized();
    repaint();
}

void XaLZaEditor::layoutKnobRow(const std::vector<KnobUI*>& row, juce::Rectangle<int> area,
                                 int labelH, int knobW, int knobH, int cellW)
{
    int n = (int) row.size();
    if (n == 0) return;
    int totalW = juce::jmin(area.getWidth(), cellW * n);
    auto rowArea = area.withSizeKeepingCentre(totalW, area.getHeight()).withY(area.getY());

    for (auto* k : row)
    {
        auto cell = rowArea.removeFromLeft(cellW);
        auto lbl = cell.removeFromTop(labelH);
        k->label.setBounds(lbl);
        k->slider.setBounds(cell.withSizeKeepingCentre(knobW, knobH));
    }
}

void XaLZaEditor::layoutModuleMeter(ModuleMeterUI& mm, juce::Rectangle<int> area)
{
    const int blockW = 46, meterH = 40, capH = 11, dbH = 13, blockGap = 18;
    const int totalW = blockW * 2 + blockGap;

    mm.bypassBtn.setBounds(area.getX(), area.getY(), 52, 20);

    auto row = area.removeFromTop(capH + meterH + dbH).withWidth(totalW).withRight(area.getRight());
    auto inBlock  = row.removeFromLeft(blockW);
    row.removeFromLeft(blockGap);
    auto outBlock = row;

    auto layBlock = [&] (juce::Rectangle<int> b, juce::Label& cap, LedMeter& meter, juce::Label& db)
    {
        cap.setBounds(b.removeFromTop(capH));
        meter.setBounds(b.removeFromTop(meterH));
        db.setBounds(b.removeFromTop(dbH));
    };
    layBlock(inBlock, mm.capIn, mm.meterIn, mm.dbIn);
    layBlock(outBlock, mm.capOut, mm.meterOut, mm.dbOut);

    if (mm.grIndex >= 0)
    {
        auto grArea = area.removeFromTop(18).withWidth(totalW).withRight(area.getRight());
        mm.grLabel.setBounds(grArea);
    }
}

void XaLZaEditor::timerCallback()
{
    masterMeterIn.setDb(proc.getMeterDbL((int) XaLZaProcessor::TapIn), proc.getMeterDbR((int) XaLZaProcessor::TapIn));
    masterMeterOut.setDb(proc.getMeterDbL((int) XaLZaProcessor::TapOut), proc.getMeterDbR((int) XaLZaProcessor::TapOut));

    auto fmtDb = [] (float l, float r)
    {
        float v = juce::jmax(l, r);
        return v <= -99.0f ? juce::String("-inf") : juce::String(v, 1);
    };

    for (auto& mmPtr : moduleMeterStorage)
    {
        auto& mm = *mmPtr;
        float inL = proc.getMeterDbL(mm.tapIn), inR = proc.getMeterDbR(mm.tapIn);
        float outL = proc.getMeterDbL(mm.tapOut), outR = proc.getMeterDbR(mm.tapOut);
        mm.meterIn.setDb(inL, inR);
        mm.meterOut.setDb(outL, outR);
        mm.dbIn.setText(fmtDb(inL, inR), juce::dontSendNotification);
        mm.dbOut.setText(fmtDb(outL, outR), juce::dontSendNotification);

        if (mm.grIndex >= 0)
        {
            float gr = proc.getGrDb(mm.grIndex);
            mm.grLabel.setText("GR -" + juce::String(gr, 1) + " dB", juce::dontSendNotification);
        }
    }

    // Macro-vs-manual override indicator: only the current page's own
    // fine-tune knobs need checking. Teal label = the macro is driving
    // this knob right now; default colour = the manual value is winning.
    if (currentTab >= 0 && currentTab < (int) pageKnobs.size())
    {
        for (auto* k : pageKnobs[(size_t) currentTab])
        {
            if (k->macroID.isEmpty())
                continue;
            bool winning = proc.macroTracker.isMacroWinning(k->macroID, k->paramID);
            if (winning != k->lastMacroWinning)
            {
                k->lastMacroWinning = winning;
                k->label.setColour(juce::Label::textColourId,
                                    winning ? XaLZaColour::accent2 : XaLZaColour::textLabel);
            }
        }
    }

    // Macros-page summary: which modules are bypassed right now, at a glance.
    if (currentTab == 0)
    {
        static const std::vector<std::pair<juce::String, juce::String>> bypassIds = {
            { XID::PreBypass, "PRE" }, { XID::GateBypass, "GATE" }, { XID::EssBypass, "ESS" },
            { XID::CompBypass, "COMP" }, { XID::OptoBypass, "OPTO" }, { XID::EqBypass, "EQ" },
            { XID::ResBypass, "RES" }, { XID::SatBypass, "SAT" }, { XID::DblBypass, "DBL" },
            { XID::RevBypass, "REV" }, { XID::DlyBypass, "DLY" }, { XID::LimBypass, "LIM" },
        };
        juce::StringArray bypassed;
        for (auto& bp : bypassIds)
            if (proc.apvts.getRawParameterValue(bp.first)->load() > 0.5f)
                bypassed.add(bp.second);
        bypassSummaryLabel.setText(bypassed.isEmpty() ? juce::String("ALL MODULES ACTIVE")
                                                       : ("BYPASSED: " + bypassed.joinIntoString(", ")),
                                    juce::dontSendNotification);
        bypassSummaryLabel.setColour(juce::Label::textColourId,
                                      bypassed.isEmpty() ? XaLZaColour::textMuted : XaLZaColour::danger);
    }

    // Only do the expensive per-tab visualisers' work while their page is
    // actually showing. Every one of these reads genuinely POST that
    // module's own processing (never the module's input).
    if (currentTab == preTabIndex)
    {
        preVu.pushDb(juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapIn),
                                 proc.getMeterDbR((int) XaLZaProcessor::TapIn)));
    }
    else if (currentTab == gateTabIndex)
    {
        gateEnvGraph.push(juce::jlimit(0.0f, 1.0f, proc.getGateGrDb() / 60.0f));
    }
    else if (currentTab == essTabIndex)
    {
        float bandNorm = juce::jlimit(0.0f, 1.0f, (proc.getEssBandDb() + 60.0f) / 60.0f);
        float redNorm  = juce::jlimit(0.0f, 1.0f, -proc.getEssReductionDb() / 24.0f);
        essEnvGraph.push(bandNorm, redNorm);
    }
    else if (currentTab == compTabIndex)
    {
        float grNorm = juce::jlimit(0.0f, 1.0f, proc.getGrDb(0) / 24.0f);
        float outDbC = juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapComp),
                                   proc.getMeterDbR((int) XaLZaProcessor::TapComp));
        float outNorm = juce::jlimit(0.0f, 1.0f, (outDbC + 50.0f) / 50.0f);
        compGrGraph.push(grNorm, outNorm);
    }
    else if (currentTab == optoTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawOpto);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawOpto, pos - WaveformScope::numPoints + i);
        optoScope.setSamples(buf);
    }
    else if (currentTab == eqTabIndex)
    {
        eqSpectrum.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
        float specBuf[SpectrumAnalyzer::fftSize];
        int pos = proc.getSpecWritePos();
        for (int i = 0; i < SpectrumAnalyzer::fftSize; ++i)
            specBuf[i] = proc.specSample(pos - SpectrumAnalyzer::fftSize + i);
        eqSpectrum.update(specBuf);
    }
    else if (currentTab == resTabIndex)
    {
        resSuppressGraph.push(juce::jlimit(0.0f, 1.0f, -proc.getResCutDb() / 24.0f));
    }
    else if (currentTab == satTabIndex)
    {
        float bufIn[WaveformScope::numPoints], bufOut[WaveformScope::numPoints];
        int posIn  = proc.getRawWritePos((int) XaLZaProcessor::RawSatIn);
        int posOut = proc.getRawWritePos((int) XaLZaProcessor::RawSatOut);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
        {
            bufIn[i]  = proc.rawSample((int) XaLZaProcessor::RawSatIn,  posIn  - WaveformScope::numPoints + i);
            bufOut[i] = proc.rawSample((int) XaLZaProcessor::RawSatOut, posOut - WaveformScope::numPoints + i);
        }
        satScope.setSamples(bufOut, bufIn);   // out = primary/accent, in = muted reference
    }
    else if (currentTab == dblTabIndex)
    {
        constexpr int numPts = 300;
        std::vector<std::pair<float, float>> pts;
        pts.reserve((size_t) numPts);
        int pos = proc.getDblScopeWritePos();
        for (int i = 0; i < numPts; ++i)
        {
            int idx = pos - 1 - i;
            pts.emplace_back(proc.dblScopeSampleL(idx), proc.dblScopeSampleR(idx));
        }
        dblGoniometer.setPoints(pts);
    }
    else if (currentTab == revTabIndex)
    {
        float outDbR = juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapRev),
                                   proc.getMeterDbR((int) XaLZaProcessor::TapRev));
        revDecayGraph.push(juce::jlimit(0.0f, 1.0f, (outDbR + 60.0f) / 60.0f));
    }
    else if (currentTab == dlyTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawDly);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawDly, pos - WaveformScope::numPoints + i);
        dlyScope.setSamples(buf);
    }
    else if (currentTab == limTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawLim);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawLim, pos - WaveformScope::numPoints + i);
        limView.update(buf, proc.getLufs());
    }

    if (currentTab == 0)
    {
        constexpr int numPts = 300;
        std::vector<std::pair<float, float>> pts;
        pts.reserve((size_t) numPts);
        int pos = proc.getScopeWritePos();
        for (int i = 0; i < numPts; ++i)
        {
            int idx = pos - 1 - i;
            pts.emplace_back(proc.scopeSampleL(idx), proc.scopeSampleR(idx));
        }
        goniometer.setPoints(pts);
    }

    repaint(getLocalBounds().removeFromBottom(footerH));
}

void XaLZaEditor::paint(juce::Graphics& g)
{
    g.fillAll(XaLZaColour::panelBg);

    auto full = getLocalBounds();
    auto titleArea = full.removeFromTop(titleBarH);

    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(titleArea);
    g.setColour(XaLZaColour::borderSoft);
    g.drawLine(0.0f, (float) titleBarH, (float) getWidth(), (float) titleBarH, 1.0f);

    // Brand mark: a simple accent "X" badge, echoing the mockup's brand-mark
    auto badge = titleArea.removeFromLeft(titleBarH).reduced(6).toFloat();
    g.setColour(XaLZaColour::panelControl);
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(XaLZaColour::accent);
    juce::Path xMark;
    xMark.addLineSegment({ badge.getX() + 4, badge.getY() + 4, badge.getRight() - 4, badge.getBottom() - 4 }, 1.8f);
    xMark.addLineSegment({ badge.getRight() - 4, badge.getY() + 4, badge.getX() + 4, badge.getBottom() - 4 }, 1.8f);
    g.fillPath(xMark);

    g.setColour(XaLZaColour::textHi);
    g.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
    g.drawText("THE XALZA", titleArea.reduced(8, 0), juce::Justification::centredLeft);
    titleArea.removeFromRight(80);    // leave room for the bypass button
    titleArea.removeFromRight(48);    // leave room for LOAD
    titleArea.removeFromRight(48);    // leave room for SAVE
    titleArea.removeFromRight(140);   // leave room for the preset picker
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(XaLZaColour::textMuted);
    g.drawText("Vocal Chain", titleArea.reduced(10, 0), juce::Justification::centredRight);

    // Tab rail background
    auto body = getLocalBounds();
    body.removeFromTop(titleBarH);
    body.removeFromBottom(footerH);
    auto rail = body.removeFromLeft(railW);
    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(rail);
    g.setColour(XaLZaColour::borderSoft);
    g.drawLine((float) rail.getRight(), (float) rail.getY(), (float) rail.getRight(), (float) rail.getBottom(), 1.0f);

    // Accent bar next to the selected tab
    if (currentTab >= 0 && currentTab < (int) tabButtons.size())
    {
        auto b = tabButtons[(size_t) currentTab]->getBounds();
        g.setColour(XaLZaColour::accent);
        g.fillRect(rail.getX(), b.getY(), 3, b.getHeight());
    }

    // Master mini-panel background (only meaningful on the Macros page, but
    // harmless to paint always since its knobs are hidden on other pages)
    if (currentTab == 0)
    {
        auto content = body.reduced(marginX, marginY);
        auto masterPanel = content.removeFromRight(masterW);
        g.setColour(XaLZaColour::panelRaised);
        g.fillRoundedRectangle(masterPanel.toFloat(), 4.0f);
        g.setColour(XaLZaColour::borderSoft);
        g.drawRoundedRectangle(masterPanel.toFloat(), 4.0f, 1.0f);
        g.setColour(XaLZaColour::textMuted);
        g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
        g.drawText("MASTER", masterPanel.removeFromTop(20), juce::Justification::centred);
    }

    // Footer bar — brand line + a live master-output reading (mockup's .footerbar)
    auto footer = getLocalBounds().removeFromBottom(footerH);
    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(footer);
    g.setColour(XaLZaColour::borderSoft);
    g.drawLine(0.0f, (float) footer.getY(), (float) getWidth(), (float) footer.getY(), 1.0f);

    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.setColour(XaLZaColour::textMuted);
    g.drawText("THE XALZA - Vocal Chain", footer.reduced(14, 0), juce::Justification::centredLeft);

    float outDb = juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapOut), proc.getMeterDbR((int) XaLZaProcessor::TapOut));
    juce::String outText = outDb <= -99.0f ? "OUT  -inf dB" : ("OUT  " + juce::String(outDb, 1) + " dB");
    g.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
    g.setColour(outDb > -1.0f ? XaLZaColour::danger : XaLZaColour::textLabel);
    g.drawText(outText, footer.reduced(14, 0), juce::Justification::centredRight);
}

void XaLZaEditor::resized()
{
    auto full = getLocalBounds();
    auto titleArea = full.removeFromTop(titleBarH);
    bypassButton.setBounds(titleArea.removeFromRight(80).reduced(10, 5));
    loadPresetBtn.setBounds(titleArea.removeFromRight(48).reduced(4, 5));
    savePresetBtn.setBounds(titleArea.removeFromRight(48).reduced(4, 5));
    presetBox.setBounds(titleArea.removeFromRight(140).reduced(6, 6));
    full.removeFromBottom(footerH);

    auto rail = full.removeFromLeft(railW);
    for (size_t i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setBounds(rail.getX(), rail.getY() + (int) i * 34, rail.getWidth(), 34);

    auto content = full.reduced(marginX, marginY);

    if (currentTab == 0)
    {
        auto masterPanel = content.removeFromRight(masterW).reduced(4, 0);
        masterPanel.removeFromTop(20);
        for (auto* mk : masterKnobs)
        {
            auto cell = masterPanel.removeFromTop(masterLabelH + masterKnobH);
            auto lbl = cell.removeFromTop(masterLabelH);
            mk->label.setBounds(lbl);
            mk->slider.setBounds(cell.withSizeKeepingCentre(masterKnobW, masterKnobH));
        }

        masterPanel.removeFromTop(6);
        {
            auto meterRow = masterPanel.removeFromTop(11 + 34);
            int colW = (meterRow.getWidth() - 10) / 2;
            auto inCol = meterRow.removeFromLeft(colW);
            meterRow.removeFromLeft(10);
            auto outCol = meterRow;

            masterCapIn.setBounds(inCol.removeFromTop(11));
            masterMeterIn.setBounds(inCol);
            masterCapOut.setBounds(outCol.removeFromTop(11));
            masterMeterOut.setBounds(outCol);
        }

        masterPanel.removeFromTop(10);
        goniometerCap.setBounds(masterPanel.removeFromTop(11));
        {
            auto side = juce::jmin(masterPanel.getWidth(), masterPanel.getHeight());
            goniometer.setBounds(masterPanel.withSizeKeepingCentre(side, side).withY(masterPanel.getY()));
        }

        content.removeFromRight(8);
        bypassSummaryLabel.setBounds(content.removeFromBottom(16));
        content.removeFromBottom(4);

        // 4 columns x 3 rows of macro knobs
        const int cols = 4;
        int rowH = macroLabelH + macroKnobH + 10;
        for (int i = 0; i < (int) pageKnobs[0].size(); ++i)
        {
            int col = i % cols, row = i / cols;
            auto cell = content.withTrimmedTop(row * rowH).withHeight(rowH)
                                .withTrimmedLeft(col * macroCellW).withWidth(macroCellW);
            auto lbl = cell.removeFromTop(macroLabelH);
            pageKnobs[0][(size_t) i]->label.setBounds(lbl);
            pageKnobs[0][(size_t) i]->slider.setBounds(cell.withSizeKeepingCentre(macroKnobW, macroKnobH));
        }
    }
    else
    {
        auto* mm = (currentTab >= 0 && currentTab < (int) moduleMeterByTab.size())
                       ? moduleMeterByTab[(size_t) currentTab] : nullptr;
        if (mm != nullptr)
        {
            auto meterArea = content.removeFromTop(moduleMeterH).reduced(4, 0);
            layoutModuleMeter(*mm, meterArea);
        }

        auto* bv = (BigViz*) nullptr;
        for (auto& b : bigViz)
            if (b.tabIndex == currentTab) { bv = &b; break; }

        if (bv != nullptr)
        {
            auto knobArea = content.removeFromTop(fineLabelH + fineKnobH + 14);
            layoutKnobRow(pageKnobs[(size_t) currentTab], knobArea, fineLabelH, fineKnobW, fineKnobH, fineCellW);

            content.removeFromTop(10);
            auto titleRow = content.removeFromTop(bigVizTitleH);
            auto vizArea = content.removeFromTop(bigVizH);

            bv->title->setBounds(titleRow);
            // The VU gauge reads best narrower/centred; every other big viz
            // (line charts, oscilloscopes, spectrum) fills the full width.
            if (bv->comp == (juce::Component*) &preVu)
                bv->comp->setBounds(vizArea.withSizeKeepingCentre(juce::jmin(vizArea.getWidth(), 320), vizArea.getHeight()));
            else
                bv->comp->setBounds(vizArea);
        }
        else
        {
            layoutKnobRow(pageKnobs[(size_t) currentTab], content, fineLabelH, fineKnobW, fineKnobH, fineCellW);
        }
    }
}
