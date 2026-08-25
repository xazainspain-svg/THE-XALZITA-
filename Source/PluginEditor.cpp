#include "PluginEditor.h"
#include "BinaryData.h"

// =============================================================================
// Look and feel
// =============================================================================
XaLZaLookAndFeel::XaLZaLookAndFeel()
{
    // Embedded fonts — the exact two typefaces the original web mockup uses
    // (Space Grotesk for UI text, IBM Plex Mono for numeric/technical
    // readouts), compiled into the binary via BinaryData so no system
    // install is required. See getTypefaceForFont() below for how every
    // Font constructed anywhere in the editor resolves to one of these.
    sansRegular = juce::Typeface::createSystemTypefaceFor(BinaryData::SpaceGroteskRegular_ttf, BinaryData::SpaceGroteskRegular_ttfSize);
    sansBold    = juce::Typeface::createSystemTypefaceFor(BinaryData::SpaceGroteskSemiBold_ttf, BinaryData::SpaceGroteskSemiBold_ttfSize);
    monoRegular = juce::Typeface::createSystemTypefaceFor(BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    monoBold    = juce::Typeface::createSystemTypefaceFor(BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);

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

juce::Typeface::Ptr XaLZaLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    // Every Font built anywhere in this editor — whether it explicitly asks
    // for "IBM Plex Mono" (numeric readouts, via monoFont() below) or just
    // uses the default typeface name (every other label/button, unchanged
    // call sites) — is routed through here, so this one place decides which
    // of the four embedded weights actually gets drawn.
    const bool wantsMono = f.getTypefaceName().containsIgnoreCase("Plex Mono");
    if (wantsMono)
        return f.isBold() ? monoBold : monoRegular;
    return f.isBold() ? sansBold : sansRegular;
}

juce::Font XaLZaLookAndFeel::monoFont(float size, bool bold)
{
    return juce::Font(juce::FontOptions(size).withName("IBM Plex Mono").withStyle(bold ? "Bold" : "Regular"));
}

juce::Label* XaLZaLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
    // Fixed compact size instead of inheriting JUCE's default text-box
    // font height — that default ran noticeably larger than intended here,
    // which (together with the too-narrow box widths) was clipping every
    // knob readout to "...". Small enough that even "18000 Hz" now clears
    // the widened box above with room to spare.
    l->setFont(monoFont(10.0f));
    return l;
}

juce::Font XaLZaLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return monoFont(juce::LookAndFeel_V4::getComboBoxFont(box).getHeight());
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

// Flat, sharp-cornered button skin — matches the mockup's .icon-btn-box /
// .seg-btn / .modal-btn (1px hairline border, ~2px corner radius, no
// gradient or drop shadow), replacing LookAndFeel_V4's rounded/glossy
// default which was previously only recoloured, not reshaped.
void XaLZaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float cornerSize = 2.0f;

    juce::Colour fill = backgroundColour;
    if (shouldDrawButtonAsDown)
        fill = fill.getAlpha() > 0 ? fill.darker(0.25f) : XaLZaColour::panelControl.withAlpha(0.6f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.getAlpha() > 0 ? fill.brighter(0.1f) : XaLZaColour::panelControl.withAlpha(0.35f);

    if (fill.getAlpha() > 0)
    {
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, cornerSize);
    }

    // Buttons with a real (non-transparent) base colour always show their
    // hairline border, like the mockup's permanently-boxed .icon-btn-box;
    // plain "clickable text" buttons (transparent base, e.g. tab labels,
    // the About footer link) only pick up the box on hover/press, so they
    // read as text the rest of the time — same as the mockup's plain links.
    if (backgroundColour.getAlpha() > 0 || shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(XaLZaColour::borderSoft);
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }
}

// Flat combo box skin — matches the mockup's .preset box, with a thin
// stroked chevron instead of JUCE's default filled-triangle arrow glyph.
void XaLZaLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/, int buttonW, int /*buttonH*/, juce::ComboBox& /*box*/)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    const float cornerSize = 2.0f;

    g.setColour(XaLZaColour::panelControl);
    g.fillRoundedRectangle(bounds, cornerSize);
    g.setColour(XaLZaColour::borderSoft);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    auto arrowArea = juce::Rectangle<float>((float) (width - buttonW), 0.0f, (float) buttonW, (float) height);
    auto c = arrowArea.getCentre();
    juce::Path arrow;
    arrow.startNewSubPath(c.x - 3.5f, c.y - 2.0f);
    arrow.lineTo(c.x, c.y + 2.0f);
    arrow.lineTo(c.x + 3.5f, c.y - 2.0f);
    g.setColour(XaLZaColour::textMuted);
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
            { XID::EqLowFreq, " Hz" }, { XID::EqMidFreq, " Hz" }, { XID::EqHighFreq, " Hz" },
            { XID::ResAmount, " %" }, { XID::ResSharpness, " %" }, { XID::ResReactivity, " %" },
            { XID::ResNotchLimit, " dB" }, { XID::ResLow, " Hz" }, { XID::ResHigh, " Hz" }, { XID::ResMacro, " %" },
            { XID::SatDrive, " %" }, { XID::SatTone, " dB" }, { XID::SatCeiling, " dB" }, { XID::SatMix, " %" },
            { XID::SatMacro, " %" },
            { XID::DblDetune, " %" }, { XID::DblWidth, " %" }, { XID::DblDelay, " ms" }, { XID::DblMix, " %" },
            { XID::DblMacro, " %" },
            { XID::RevSize, " %" }, { XID::RevDecay, " s" }, { XID::RevPreDelay, " ms" }, { XID::RevMix, " %" },
            { XID::RevDuck, " %" }, { XID::RevDuckRelease, " ms" }, { XID::RevMacro, " %" },
            { XID::RevWetHpf, " Hz" }, { XID::RevWetLpf, " Hz" },
            { XID::DlyTime, " ms" }, { XID::DlyFeedback, " %" }, { XID::DlySpread, " %" }, { XID::DlyMix, " %" },
            { XID::DlyDuck, " %" }, { XID::DlyDuckRelease, " ms" }, { XID::DlyPanRate, " Hz" }, { XID::DlyMacro, " %" },
            { XID::DlyFbHpf, " Hz" }, { XID::DlyFbLpf, " Hz" },
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
    // Wide enough for the longest real readout ("18000 Hz", "-18.2 dB")
    // at the compact mono font createSliderTextBox below sets — the old
    // 46/54px boxes were clipping every single value to "..." before the
    // unit suffix (or even all of the digits) could show.
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, accent ? 78 : 70, 15);
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

    // Without an explicit decimal count JUCE guesses from the parameter's
    // raw float precision, which reads as noise ("0.00000...", "12.847...")
    // instead of a clean readout — pick a sane digit count from how wide
    // the parameter's actual range is (a 0-100% knob doesn't need decimals
    // at all; a 1-50:1 ratio or a small Hz/second range does).
    {
        auto range = k->slider.getRange();
        double span = range.getLength();
        int decimals = span >= 100.0 ? 0 : (span >= 10.0 ? 1 : 2);
        k->slider.setNumDecimalPlacesToDisplay(decimals);

        // setNumDecimalPlacesToDisplay above is a no-op for this slider:
        // the SliderAttachment we just built installs its own
        // textFromValueFunction that calls straight through to the
        // parameter's own getText(), which — for a plain continuous
        // AudioParameterFloat with no explicit step — falls back to a
        // generic ~6-decimal format ("0.000000") no matter what we ask
        // the Slider to do. Overriding textFromValueFunction ourselves,
        // AFTER the attachment sets its own, is what actually wins.
        juce::String suffix = k->slider.getTextValueSuffix();
        k->slider.textFromValueFunction = [decimals, suffix] (double v)
        {
            return juce::String(v, decimals) + suffix;
        };
        k->slider.valueFromTextFunction = [suffix] (const juce::String& text)
        {
            return text.upToFirstOccurrenceOf(suffix, false, false).trim().getDoubleValue();
        };
    }

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
    // tapOut is always TapPre + slotId (see the MeterTap/ModuleSlot comment
    // in PluginProcessor.h — the two enums are deliberately in the same
    // Pre..Lim order), so this derives cleanly without a 13th constructor
    // argument at every one of the 12 call sites below.
    mm->slotId = tapOut - (int) XaLZaProcessor::TapPre;
    mm->grIndex = grIndex;

    mm->bypassParamID = bypassParamID;

    mm->bypassBtn.setClickingTogglesState(true);
    mm->bypassBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::danger);
    mm->bypassBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::textHi);
    mm->bypassBtn.setColour(juce::TextButton::textColourOffId, XaLZaColour::textMuted);
    mm->bypassBtn.setTooltip("Bypass this module only");
    addChildComponent(mm->bypassBtn);
    mm->bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, bypassParamID, mm->bypassBtn);

    mm->soloBtn.setClickingTogglesState(false);   // toggle state is driven manually from activeSoloParamID
    mm->soloBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
    mm->soloBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
    mm->soloBtn.setTooltip("Solo this module — bypasses every other module");
    mm->soloBtn.onClick = [this, bypassParamID] { toggleSolo(bypassParamID); };
    addChildComponent(mm->soloBtn);

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
    // dbIn/dbOut are live numeric dB readouts (mockup's mono-font
    // .knob-value equivalent), unlike the "IN"/"OUT" word captions above.
    mm->dbIn.setFont(XaLZaLookAndFeel::monoFont(9.0f));
    mm->dbOut.setFont(XaLZaLookAndFeel::monoFont(9.0f));

    mm->grLabel.setJustificationType(juce::Justification::centred);
    mm->grLabel.setFont(XaLZaLookAndFeel::monoFont(9.5f, true));
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
    // Natural chain order + full module names (matches the mockup's Macros
    // grid exactly: PREAMP GATE DE-ESSER GLUE COMP OPTO EQ 550 / RESONANCE
    // SATURATOR DOUBLER REVERB DELAY LIMITER) instead of the old 4x3 grid
    // of short tab-style codes in an unrelated order. Must stay in lockstep
    // with Params.h's xalzaMacroIDs() (same order, see its comment).
    struct MacroDef { juce::String id; juce::String label; };
    static const std::vector<MacroDef> macroDefs = {
        { XID::PreMacro,  "PREAMP" },    { XID::GateMacro, "GATE" },     { XID::EssMacro,  "DE-ESSER" },
        { XID::CompMacro, "GLUE COMP" }, { XID::OptoMacro, "OPTO" },     { XID::EqMacro,   "EQ 550" },
        { XID::ResMacro,  "RESONANCE" }, { XID::SatMacro,  "SATURATOR" },{ XID::DblMacro,  "DOUBLER" },
        { XID::RevMacro,  "REVERB" },    { XID::DlyMacro,  "DELAY" },    { XID::LimMacro,  "LIMITER" },
    };
    for (auto& md : macroDefs)
        pageKnobs[0].push_back(&addKnob(md.id, md.label, true));

    // MIDI Learn: pageKnobs[0] now holds exactly the 12 macro knobs, in the
    // same order as Params.h's xalzaMacroIDs() (both built from macroDefs'
    // order) — register this editor as a mouse listener on each slider so
    // mouseDown() can identify which macro a right-click landed on.
    for (auto* k : pageKnobs[0])
        k->slider.addMouseListener(this, false);

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
    // Pad/Phase/Phantom toggles + Impedance seg-group — matches the
    // mockup's Preamp "INPUT" box and IMPEDANCE seg-group.
    for (auto* b : { &prePadBtn, &prePhaseBtn, &prePhantomBtn })
    {
        b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
        b->setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
        addChildComponent(*b);
    }
    prePhantomBtn.setTooltip("Cosmetic only - real 48V phantom power has no effect on a plugin's audio");
    prePadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::PrePad, prePadBtn);
    prePhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::PrePhase, prePhaseBtn);
    prePhantomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::PrePhantom, prePhantomBtn);
    preImpedanceSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::PreImpedance,
        std::vector<SegButtonGroup::Option>{ { "300ohm", 300.0f }, { "1.2k", 1200.0f }, { "2.4k", 2400.0f } });
    addChildComponent(*preImpedanceSeg);
    addPage("COMP", { { XID::CompThresh, "Thresh" }, { XID::CompMakeup, "Makeup" }, { XID::CompAttack, "Attack" },
                       { XID::CompRelease, "Release" }, { XID::CompMix, "Mix" } });
    // Ratio is a seg-group of fixed presets (matches the mockup's
    // compRatioSegs), not a fine-tune knob — laid out in the COMP page's
    // title row instead of the knob row (see resized()).
    compRatioSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::CompRatio,
        std::vector<SegButtonGroup::Option>{ { "2:1", 2.0f }, { "4:1", 4.0f }, { "8:1", 8.0f },
                                               { "20:1", 20.0f }, { "Limit", 50.0f } });
    addChildComponent(*compRatioSeg);
    addPage("OPTO", { { XID::OptoReduction, "Reduction" }, { XID::OptoGain, "Gain" }, { XID::OptoMix, "Mix" } });
    // Mode is a real seg-group (matches the mockup's optoModeSegs) — snaps
    // the boolean OptoMode param, which processBlock reads to pick 4:1 vs
    // 20:1 ratio (see runOpto).
    optoModeSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::OptoMode,
        std::vector<SegButtonGroup::Option>{ { "Compress", 0.0f }, { "Limit", 1.0f } });
    addChildComponent(*optoModeSeg);
    // Low/Mid/High Freq are real seg-groups (matches the mockup's
    // eqLowFreqSegs/eqMidFreqSegs/eqHighFreqSegs), constructed just below —
    // only the three gain knobs stay in the fine-tune knob row.
    addPage("EQ",   { { XID::EqLow, "Low" }, { XID::EqMid, "Mid" }, { XID::EqHigh, "High" } });
    eqLowFreqSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::EqLowFreq,
        std::vector<SegButtonGroup::Option>{ { "30", 30.0f }, { "60", 60.0f }, { "100", 100.0f }, { "200", 200.0f } });
    eqMidFreqSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::EqMidFreq,
        std::vector<SegButtonGroup::Option>{ { "400", 400.0f }, { "800", 800.0f }, { "1.5k", 1500.0f }, { "3k", 3000.0f } });
    eqHighFreqSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::EqHighFreq,
        std::vector<SegButtonGroup::Option>{ { "5k", 5000.0f }, { "7k", 7000.0f }, { "10k", 10000.0f }, { "15k", 15000.0f } });
    for (auto* s : { eqLowFreqSeg.get(), eqMidFreqSeg.get(), eqHighFreqSeg.get() })
        addChildComponent(*s);
    addPage("SAT",  { { XID::SatDrive, "Drive" }, { XID::SatTone, "Tone" }, { XID::SatCeiling, "Ceiling" }, { XID::SatMix, "Mix" } });
    // Character is a real seg-group (matches the mockup's satCharSegs) —
    // four genuinely different waveshapes, not a relabelled knob (see
    // runSat's shape() lambda).
    satCharSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::SatChar,
        std::vector<SegButtonGroup::Option>{ { "Tube", 0.0f }, { "Tape", 1.0f }, { "Transistor", 2.0f }, { "Diode", 3.0f } });
    addChildComponent(*satCharSeg);
    addPage("REV",  { { XID::RevSize, "Size" }, { XID::RevDecay, "Decay" }, { XID::RevPreDelay, "PreDelay" },
                       { XID::RevMix, "Mix" }, { XID::RevDuck, "Duck" }, { XID::RevDuckRelease, "DuckRel" },
                       { XID::RevWetHpf, "Wet HPF" }, { XID::RevWetLpf, "Wet LPF" } });
    // Time is a real seg-group (was the 9th knob crowding this page badly
    // enough to clip its own neighbours' labels) — fixed ms presets for
    // now rather than the mockup's tempo-synced note values, since real
    // sync needs host playhead/tempo access that's a separate, bigger
    // piece of work. Still snaps the existing continuous DlyTime param.
    addPage("DLY",  { { XID::DlyFeedback, "Fdbk" }, { XID::DlySpread, "Spread" },
                       { XID::DlyMix, "Mix" }, { XID::DlyDuck, "Duck" }, { XID::DlyDuckRelease, "DuckRel" },
                       { XID::DlyPanRate, "PanRate" }, { XID::DlyFbHpf, "Fbk HPF" }, { XID::DlyFbLpf, "Fbk LPF" } });
    dlyTimeSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::DlyTime,
        std::vector<SegButtonGroup::Option>{ { "100ms", 100.0f }, { "200ms", 200.0f }, { "300ms", 300.0f },
                                               { "500ms", 500.0f }, { "750ms", 750.0f } });
    addChildComponent(*dlyTimeSeg);
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

    masterLoudnessLabel.setJustificationType(juce::Justification::centred);
    masterLoudnessLabel.setFont(XaLZaLookAndFeel::monoFont(11.0f, true));
    masterLoudnessLabel.setColour(juce::Label::textColourId, XaLZaColour::accent2);
    addChildComponent(masterLoudnessLabel);

    bypassSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    bypassSummaryLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    bypassSummaryLabel.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
    bypassSummaryLabel.setText("ALL MODULES ACTIVE", juce::dontSendNotification);
    addChildComponent(bypassSummaryLabel);

    // Footer brand/About control — styled to read as plain label text
    // (no border/fill) but genuinely clickable, showing the real build
    // version and a short About box.
    aboutButton.setButtonText("THE XALZA - Vocal Chain  v" + proc.getVersionString());
    aboutButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    aboutButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    aboutButton.setColour(juce::TextButton::textColourOffId, XaLZaColour::textMuted);
    aboutButton.setColour(juce::TextButton::textColourOnId, XaLZaColour::textMuted);
    aboutButton.setTooltip("About The XaLZa");
    aboutButton.onClick = [this] { showAboutBox(); };
    addAndMakeVisible(aboutButton);

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
    setupVizLabel(preVuTitle,         "INPUT LEVEL + OUTPUT WAVEFORM + HARMONIC COLOR + HPF RESPONSE");
    setupVizLabel(gateEnvTitle,       "POST-GATE WAVEFORM + REDUCTION");
    setupVizLabel(essEnvTitle,        "SIBILANCE BAND + REDUCTION");
    setupVizLabel(compGrTitle,        "GAIN REDUCTION + OUTPUT + TRANSFER CURVE");
    setupVizLabel(optoScopeTitle,     "POST-OPTO OSCILLOSCOPE + TRANSFER CURVE");
    setupVizLabel(eqSpectrumTitle,    "RESPONSE SPECTRUM (POST-EQ)");
    setupVizLabel(resSuppressTitle,   "DYNAMIC SUPPRESSION");
    setupVizLabel(satScopeTitle,      "WAVEFORM: IN VS OUT + HARMONIC CONTENT");
    setupVizLabel(dblGoniometerTitle, "STEREO FIELD (POST-DOUBLER)");
    setupVizLabel(revDecayTitle,      "DECAY TAIL (POST-REVERB LEVEL)");
    setupVizLabel(dlyScopeTitle,      "ECHO WAVEFORM (POST-DELAY)");
    setupVizLabel(limViewTitle,       "BRICKWALL OUTPUT + LOUDNESS");
    addChildComponent(preView);
    addChildComponent(gateView);
    addChildComponent(essEnvGraph);
    addChildComponent(compView);
    addChildComponent(optoView);
    addChildComponent(eqSpectrum);
    addChildComponent(resSuppressGraph);
    addChildComponent(satView);
    addChildComponent(dblGoniometer);
    addChildComponent(revDecayGraph);
    addChildComponent(dlyScope);
    addChildComponent(limView);
    eqSpectrum.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);

    gateListenBtn.setClickingTogglesState(true);
    essListenBtn.setClickingTogglesState(true);
    for (auto* b : { &gateListenBtn, &essListenBtn })
    {
        b->setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
        b->setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
        b->setTooltip("Listen to exactly what this detector is reacting to");
        addChildComponent(*b);
    }
    gateListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::GateListen, gateListenBtn);
    essListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::EssListen, essListenBtn);

    gateScBtn.setClickingTogglesState(true);
    gateScBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent2);
    gateScBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
    gateScBtn.setTooltip("Key the gate off the plugin's Sidechain input bus instead of "
                          "its own signal - only does anything if your host is actually "
                          "routing audio into that bus");
    addChildComponent(gateScBtn);
    gateScAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::GateScEnable, gateScBtn);

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
        { preTabIndex,  &preView,          &preVuTitle },
        { gateTabIndex, &gateView,         &gateEnvTitle },
        { essTabIndex,  &essEnvGraph,      &essEnvTitle },
        { compTabIndex, &compView,         &compGrTitle },
        { optoTabIndex, &optoView,         &optoScopeTitle },
        { eqTabIndex,   &eqSpectrum,       &eqSpectrumTitle },
        { resTabIndex,  &resSuppressGraph, &resSuppressTitle },
        { satTabIndex,  &satView,          &satScopeTitle },
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

    chainOrderBtn.setTooltip("Reorder the 12-module signal chain");
    chainOrderBtn.onClick = [this]
    {
        auto panel = std::make_unique<ChainOrderPanel>(proc);
        auto& box = juce::CallOutBox::launchAsynchronously(std::move(panel),
            chainOrderBtn.getScreenBounds(), nullptr);
        juce::ignoreUnused(box);
    };
    addAndMakeVisible(chainOrderBtn);

    savePresetBtn.setTooltip("Save the full current plugin state as a preset file");
    loadPresetBtn.setTooltip("Load a previously saved preset file");
    savePresetBtn.onClick = [this] { savePresetToFile(); };
    loadPresetBtn.onClick = [this] { loadPresetFromFile(); };
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(loadPresetBtn);

    abButtonA.setClickingTogglesState(false);
    abButtonB.setClickingTogglesState(false);
    abButtonA.setToggleState(true, juce::dontSendNotification);
    abButtonA.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent2);
    abButtonB.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent2);
    abButtonA.setTooltip("Compare slot A");
    abButtonB.setTooltip("Compare slot B — switching slots snapshots the one you're leaving");
    abButtonA.onClick = [this] { switchAbSlot(true); };
    abButtonB.onClick = [this] { switchAbSlot(false); };
    addAndMakeVisible(abButtonA);
    addAndMakeVisible(abButtonB);

    // Every control/visualiser added above is a direct child of `this` up
    // to this point — sweep them all into contentRoot now, so the fixed
    // pixel-based layout code in resized() can keep addressing them in a
    // constant 900x560 virtual space while contentRoot's own transform
    // (set in resized()) does the actual real-window scaling.
    while (getNumChildComponents() > 0)
        contentRoot.addAndMakeVisible(removeChildComponent(0));
    addAndMakeVisible(contentRoot);

    // Resizable/scalable UI: keeps the mockup's proportions (900x560)
    // locked while dragging, from 1x up to 2x, and restores whatever size
    // was last used (persisted in processor state) instead of always
    // reopening at the hardcoded default.
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double) baseW / (double) baseH);
    setResizeLimits(baseW, baseH, baseW * 2, baseH * 2);
    setSize(proc.lastEditorWidth, proc.lastEditorHeight);
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

void XaLZaEditor::toggleSolo(const juce::String& bypassParamID)
{
    static const std::array<juce::String, 12> allBypass = {
        XID::PreBypass, XID::GateBypass, XID::EssBypass, XID::CompBypass, XID::OptoBypass, XID::EqBypass,
        XID::ResBypass, XID::SatBypass, XID::DblBypass, XID::RevBypass, XID::DlyBypass, XID::LimBypass,
    };

    if (activeSoloParamID == bypassParamID)
    {
        // Turning solo off: restore every module's bypass state to what it
        // was right before this solo started.
        for (auto& id : allBypass)
            if (auto* p = proc.apvts.getParameter(id))
                p->setValueNotifyingHost(savedBypassStates.count(id) && savedBypassStates.at(id) ? 1.0f : 0.0f);
        activeSoloParamID = {};
    }
    else
    {
        // First module being soloed this round: snapshot every module's
        // current bypass state before we start forcing them.
        if (activeSoloParamID.isEmpty())
        {
            savedBypassStates.clear();
            for (auto& id : allBypass)
                savedBypassStates[id] = proc.apvts.getRawParameterValue(id)->load() > 0.5f;
        }
        activeSoloParamID = bypassParamID;
        for (auto& id : allBypass)
            if (auto* p = proc.apvts.getParameter(id))
                p->setValueNotifyingHost(id == bypassParamID ? 0.0f : 1.0f);
    }
    updateSoloButtonStates();
}

void XaLZaEditor::updateSoloButtonStates()
{
    for (auto& mmPtr : moduleMeterStorage)
        mmPtr->soloBtn.setToggleState(activeSoloParamID == mmPtr->bypassParamID, juce::dontSendNotification);
}

void XaLZaEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown())
        return;

    for (int i = 0; i < (int) pageKnobs[0].size(); ++i)
    {
        if (e.eventComponent != &pageKnobs[0][(size_t) i]->slider)
            continue;

        int boundCc = proc.getMacroCc(i);
        juce::PopupMenu menu;
        menu.addItem(1, boundCc >= 0 ? ("MIDI Learn... (currently CC " + juce::String(boundCc) + ")")
                                      : "MIDI Learn...");
        if (boundCc >= 0)
            menu.addItem(2, "Clear MIDI Learn");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, i] (int result)
        {
            if (result == 1)
            {
                proc.startMidiLearn(i);
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "MIDI Learn",
                    "Move a MIDI CC knob or fader now to bind it to this macro.",
                    "OK");
            }
            else if (result == 2)
            {
                proc.clearMidiLearn(i);
            }
        });
        return;
    }
}

void XaLZaEditor::showAboutBox()
{
    juce::String msg;
    msg << "The XaLZa v" << proc.getVersionString() << "\n\n"
        << "12-module vocal chain (Windows VST3):\n"
        << "Preamp - Gate - De-esser - Glue Comp - Opto - EQ 550 -\n"
        << "Resonance - Saturator - Doubler - Reverb - Delay - Limiter\n\n"
        << "Built with JUCE " << juce::String(JUCE_MAJOR_VERSION) << "."
        << juce::String(JUCE_MINOR_VERSION) << "." << juce::String(JUCE_BUILDNUMBER) << ".\n"
        << "macOS/AU is intentionally not built yet.";
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "About The XaLZa", msg, "OK");
}

void XaLZaEditor::switchAbSlot(bool toA)
{
    if (toA == onSlotA)
        return;

    // Save whatever's currently loaded into the slot we're leaving.
    (onSlotA ? stateA : stateB) = proc.apvts.copyState().createCopy();

    onSlotA = toA;
    auto& target = onSlotA ? stateA : stateB;
    if (target.isValid())
        proc.apvts.replaceState(target);
    else
        target = proc.apvts.copyState().createCopy();   // first visit — seed with current state

    abButtonA.setToggleState(onSlotA, juce::dontSendNotification);
    abButtonB.setToggleState(!onSlotA, juce::dontSendNotification);
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
    masterLoudnessLabel.setVisible(onMacros);
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
    gateListenBtn.setVisible(currentTab == gateTabIndex);
    gateScBtn.setVisible(currentTab == gateTabIndex);
    essListenBtn.setVisible(currentTab == essTabIndex);
    compRatioSeg->setVisible(currentTab == compTabIndex);
    for (auto* s : { eqLowFreqSeg.get(), eqMidFreqSeg.get(), eqHighFreqSeg.get() })
        s->setVisible(currentTab == eqTabIndex);
    dlyTimeSeg->setVisible(currentTab == dlyTabIndex);
    preImpedanceSeg->setVisible(currentTab == preTabIndex);
    for (auto* b : { &prePadBtn, &prePhaseBtn, &prePhantomBtn })
        b->setVisible(currentTab == preTabIndex);
    optoModeSeg->setVisible(currentTab == optoTabIndex);
    satCharSeg->setVisible(currentTab == satTabIndex);

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
    mm.soloBtn.setBounds(area.getX(), area.getY() + 24, 52, 20);

    // NB: withRightX (not withRight!) is the one that keeps totalW and just
    // moves the rect so its right edge lands on area.getRight() — withRight
    // instead RECOMPUTES the width from the unchanged left edge, which was
    // silently stretching this whole block back out to the full row width
    // (and colliding with the bypass/solo buttons at area.getX() above).
    auto row = area.removeFromTop(capH + meterH + dbH).withWidth(totalW).withRightX(area.getRight());
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

    if (currentTab == 0)
    {
        float lufs = proc.getLufs();
        masterLoudnessLabel.setText(lufs <= -69.5f ? juce::String("LUFS  -inf")
                                                     : ("LUFS  " + juce::String(lufs, 1)),
                                     juce::dontSendNotification);
    }

    // Keeps the highlighted Ratio button in sync with the underlying
    // CompRatio parameter even when it changes from automation, a preset
    // load, or the Comp macro — not just from clicking a button here.
    if (currentTab == compTabIndex)
        compRatioSeg->refresh();
    if (currentTab == eqTabIndex)
        for (auto* s : { eqLowFreqSeg.get(), eqMidFreqSeg.get(), eqHighFreqSeg.get() })
            s->refresh();
    if (currentTab == dlyTabIndex)
        dlyTimeSeg->refresh();
    if (currentTab == preTabIndex)
        preImpedanceSeg->refresh();
    if (currentTab == optoTabIndex)
        optoModeSeg->refresh();
    if (currentTab == satTabIndex)
        satCharSeg->refresh();

    auto fmtDb = [] (float l, float r)
    {
        float v = juce::jmax(l, r);
        return v <= -99.0f ? juce::String("-inf") : juce::String(v, 1);
    };

    for (auto& mmPtr : moduleMeterStorage)
    {
        auto& mm = *mmPtr;
        // Live predecessor tap (not the fixed-order mm.tapIn) so this still
        // reads correctly after the chain has been reordered.
        int liveTapIn = mm.slotId >= 0 ? proc.getPredecessorTap(mm.slotId) : mm.tapIn;
        float inL = proc.getMeterDbL(liveTapIn), inR = proc.getMeterDbR(liveTapIn);
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

        // MIDI Learn state, reflected as each macro knob's tooltip (right-
        // click shows the actual Learn/Clear menu — this is just the
        // hover-discoverable summary of current binding state).
        for (int i = 0; i < (int) pageKnobs[0].size(); ++i)
        {
            juce::String tip = proc.isMidiLearning(i)
                ? "Waiting for a MIDI CC... (right-click to cancel by re-learning)"
                : (proc.getMacroCc(i) >= 0
                       ? ("Bound to MIDI CC " + juce::String(proc.getMacroCc(i)) + " - right-click to change")
                       : juce::String("Right-click for MIDI Learn"));
            pageKnobs[0][(size_t) i]->slider.setTooltip(tip);
        }
    }

    // Only do the expensive per-tab visualisers' work while their page is
    // actually showing. Every one of these reads genuinely POST that
    // module's own processing (never the module's input).
    if (currentTab == preTabIndex)
    {
        preView.pushVu(juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapIn),
                                   proc.getMeterDbR((int) XaLZaProcessor::TapIn)));

        double sampleRatePre = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        preView.setHarmonicSampleRate(sampleRatePre);
        float preSpecBuf[SpectrumAnalyzer::fftSize];
        int prePos = proc.getRawWritePos((int) XaLZaProcessor::RawPre);
        for (int i = 0; i < SpectrumAnalyzer::fftSize; ++i)
            preSpecBuf[i] = proc.rawSample((int) XaLZaProcessor::RawPre, prePos - SpectrumAnalyzer::fftSize + i);
        preView.updateHarmonic(preSpecBuf);
        preView.setHpf(proc.getCurrentHpfHz(), sampleRatePre);

        float preWaveBuf[WaveformScope::numPoints];
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            preWaveBuf[i] = proc.rawSample((int) XaLZaProcessor::RawPre, prePos - WaveformScope::numPoints + i);
        preView.setOutputWaveform(preWaveBuf);
    }
    else if (currentTab == gateTabIndex)
    {
        gateView.push(juce::jlimit(0.0f, 1.0f, proc.getGateGrDb() / 60.0f));

        float gateWaveBuf[WaveformScope::numPoints];
        int gatePos = proc.getRawWritePos((int) XaLZaProcessor::RawGate);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            gateWaveBuf[i] = proc.rawSample((int) XaLZaProcessor::RawGate, gatePos - WaveformScope::numPoints + i);
        gateView.setWaveform(gateWaveBuf);
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
        compView.push(grNorm, outNorm);

        compView.setCurve(proc.macroTracker.effectiveByID(XID::CompMacro, XID::CompThresh),
                           proc.apvts.getRawParameterValue(XID::CompRatio)->load(),
                           proc.macroTracker.effectiveByID(XID::CompMacro, XID::CompMakeup),
                           proc.macroTracker.effectiveByID(XID::CompMacro, XID::CompMix) / 100.0f);
    }
    else if (currentTab == optoTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawOpto);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawOpto, pos - WaveformScope::numPoints + i);
        optoView.setSamples(buf);

        // Opto's "Reduction" knob maps to an internal threshold at a fixed
        // 4:1 ratio — mirrors the exact mapping processBlock's OPTO block uses.
        float reduction = proc.macroTracker.effectiveByID(XID::OptoMacro, XID::OptoReduction) / 100.0f;
        float optoThreshDb = juce::jmap(reduction, 0.0f, 1.0f, 0.0f, -30.0f);
        optoView.setCurve(optoThreshDb, 4.0f,
                           proc.macroTracker.effectiveByID(XID::OptoMacro, XID::OptoGain),
                           proc.macroTracker.effectiveByID(XID::OptoMacro, XID::OptoMix) / 100.0f);
    }
    else if (currentTab == eqTabIndex)
    {
        double sampleRate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        eqSpectrum.setSampleRate(sampleRate);
        float specBuf[SpectrumAnalyzer::fftSize];
        int pos = proc.getSpecWritePos();
        for (int i = 0; i < SpectrumAnalyzer::fftSize; ++i)
            specBuf[i] = proc.specSample(pos - SpectrumAnalyzer::fftSize + i);
        eqSpectrum.update(specBuf);

        eqSpectrum.setEqCurve(proc.macroTracker.effectiveByID(XID::EqMacro, XID::EqLow),
                               proc.apvts.getRawParameterValue(XID::EqLowFreq)->load(),
                               proc.macroTracker.effectiveByID(XID::EqMacro, XID::EqMid),
                               proc.apvts.getRawParameterValue(XID::EqMidFreq)->load(),
                               proc.macroTracker.effectiveByID(XID::EqMacro, XID::EqHigh),
                               proc.apvts.getRawParameterValue(XID::EqHighFreq)->load(),
                               sampleRate);
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
        satView.setSamples(bufOut, bufIn);   // out = primary/accent, in = muted reference

        double sampleRateSat = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        satView.setHarmonicSampleRate(sampleRateSat);
        float satSpecBuf[SpectrumAnalyzer::fftSize];
        int satPos = proc.getRawWritePos((int) XaLZaProcessor::RawSatOut);
        for (int i = 0; i < SpectrumAnalyzer::fftSize; ++i)
            satSpecBuf[i] = proc.rawSample((int) XaLZaProcessor::RawSatOut, satPos - SpectrumAnalyzer::fftSize + i);
        satView.updateHarmonics(satSpecBuf);
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
        limView.update(buf, proc.getLufs(), proc.getTruePeakDb());
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

    // Unscoped: at a scaled window size the real on-screen footer is
    // larger than the virtual footerH used elsewhere, so a bounds-limited
    // repaint would under-invalidate it — this repaints every tick either
    // way (30Hz), so the cost of a full repaint here is negligible.
    repaint();
}

void XaLZaEditor::paint(juce::Graphics& g)
{
    g.fillAll(XaLZaColour::panelBg);

    // Everything below is hand-drawn against the same fixed 900x560
    // virtual canvas contentRoot's children are laid out in (resized()) —
    // this transform scales it to match contentRoot's own scale exactly,
    // so the background stays pixel-aligned with the (real-window-sized)
    // controls at any window size.
    float scale = getWidth() > 0 ? (float) getWidth() / (float) baseW : 1.0f;
    g.addTransform(juce::AffineTransform::scale(scale));

    auto full = juce::Rectangle<int>(0, 0, baseW, baseH);
    auto titleArea = full.removeFromTop(titleBarH);

    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(titleArea);
    g.setColour(XaLZaColour::borderSoft);
    g.drawLine(0.0f, (float) titleBarH, (float) baseW, (float) titleBarH, 1.0f);

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
    titleArea.removeFromRight(58);    // leave room for CHAIN
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(XaLZaColour::textMuted);
    g.drawText("Vocal Chain", titleArea.reduced(10, 0), juce::Justification::centredRight);

    // Tab rail background
    auto body = juce::Rectangle<int>(0, 0, baseW, baseH);
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
    auto footer = juce::Rectangle<int>(0, 0, baseW, baseH).removeFromBottom(footerH);
    g.setColour(XaLZaColour::panelRaised);
    g.fillRect(footer);
    g.setColour(XaLZaColour::borderSoft);
    g.drawLine(0.0f, (float) footer.getY(), (float) baseW, (float) footer.getY(), 1.0f);

    float outDb = juce::jmax(proc.getMeterDbL((int) XaLZaProcessor::TapOut), proc.getMeterDbR((int) XaLZaProcessor::TapOut));
    juce::String outText = outDb <= -99.0f ? "OUT  -inf dB" : ("OUT  " + juce::String(outDb, 1) + " dB");
    g.setFont(XaLZaLookAndFeel::monoFont(10.5f, true));
    g.setColour(outDb > -1.0f ? XaLZaColour::danger : XaLZaColour::textLabel);
    g.drawText(outText, footer.reduced(14, 0), juce::Justification::centredRight);
}

void XaLZaEditor::paintOverChildren(juce::Graphics& g)
{
    // Left accent stripe — matches the mockup's `.plugin::before` (a fixed
    // 4px accent-coloured bar down the whole left edge of the window).
    // Drawn in paintOverChildren (not paint()) so it sits on top of the
    // tab rail and contentRoot's children instead of being painted over
    // by them, since those are separate child Components composited after
    // the editor's own paint() call returns.
    float scale = getWidth() > 0 ? (float) getWidth() / (float) baseW : 1.0f;
    g.addTransform(juce::AffineTransform::scale(scale));
    g.setColour(XaLZaColour::accent);
    g.fillRect(0, 0, 4, baseH);
}

void XaLZaEditor::resized()
{
    // Remember the current size so a later save/reload (or simply closing
    // and reopening the editor within the same session) restores it,
    // instead of always reopening at the hardcoded default.
    proc.lastEditorWidth  = getWidth();
    proc.lastEditorHeight = getHeight();

    // contentRoot always occupies the fixed 900x560 virtual canvas every
    // layout calculation below is written against; its transform scales
    // that canvas up/down to fill whatever real size the window actually
    // is right now (paint() applies the same scale to the background).
    float scale = getWidth() > 0 ? (float) getWidth() / (float) baseW : 1.0f;
    contentRoot.setTransform(juce::AffineTransform::scale(scale));
    contentRoot.setBounds(0, 0, baseW, baseH);

    auto full = contentRoot.getLocalBounds();
    auto titleArea = full.removeFromTop(titleBarH);
    bypassButton.setBounds(titleArea.removeFromRight(80).reduced(10, 5));
    loadPresetBtn.setBounds(titleArea.removeFromRight(48).reduced(4, 5));
    savePresetBtn.setBounds(titleArea.removeFromRight(48).reduced(4, 5));
    presetBox.setBounds(titleArea.removeFromRight(140).reduced(6, 6));
    chainOrderBtn.setBounds(titleArea.removeFromRight(58).reduced(4, 5));
    auto footerArea = full.removeFromBottom(footerH);
    {
        auto abArea = footerArea.withSizeKeepingCentre(70, footerH).reduced(0, 6);
        abButtonA.setBounds(abArea.removeFromLeft(33));
        abButtonB.setBounds(abArea.removeFromRight(33));
    }
    aboutButton.setBounds(footerArea.removeFromLeft(220).reduced(10, 4));

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

        masterPanel.removeFromTop(8);
        masterLoudnessLabel.setBounds(masterPanel.removeFromTop(16));

        masterPanel.removeFromTop(10);
        goniometerCap.setBounds(masterPanel.removeFromTop(11));
        {
            auto side = juce::jmin(masterPanel.getWidth(), masterPanel.getHeight());
            goniometer.setBounds(masterPanel.withSizeKeepingCentre(side, side).withY(masterPanel.getY()));
        }

        content.removeFromRight(8);
        bypassSummaryLabel.setBounds(content.removeFromBottom(16));
        content.removeFromBottom(4);

        // 6 columns x 2 rows, matching the mockup's Macros grid (was 4x3 of
        // short tab-style codes in an unrelated order — see macroDefs).
        const int cols = 6;
        int cellW = content.getWidth() / cols;
        int rowH = macroLabelH + macroKnobH + 10;
        for (int i = 0; i < (int) pageKnobs[0].size(); ++i)
        {
            int col = i % cols, row = i / cols;
            auto cell = content.withTrimmedTop(row * rowH).withHeight(rowH)
                                .withTrimmedLeft(col * cellW).withWidth(cellW);
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

            if (currentTab == preTabIndex)
            {
                // PAD/PHASE/+48V toggles + Impedance seg-group, centred in
                // their own row under the 3 gain knobs — matches the
                // mockup's Preamp "INPUT" box + IMPEDANCE seg-group.
                content.removeFromTop(4);
                auto ctrlRow = content.removeFromTop(22);
                constexpr int padW = 74, phaseW = 60, phantomW = 60, impW = 170, gap = 8;
                int totalW = padW + phaseW + phantomW + impW + gap * 3;
                auto rowArea = ctrlRow.withSizeKeepingCentre(juce::jmin(ctrlRow.getWidth(), totalW), ctrlRow.getHeight())
                                      .withY(ctrlRow.getY());
                prePadBtn.setBounds(rowArea.removeFromLeft(padW));
                rowArea.removeFromLeft(gap);
                prePhaseBtn.setBounds(rowArea.removeFromLeft(phaseW));
                rowArea.removeFromLeft(gap);
                prePhantomBtn.setBounds(rowArea.removeFromLeft(phantomW));
                rowArea.removeFromLeft(gap);
                preImpedanceSeg->setBounds(rowArea.removeFromLeft(impW));
            }

            if (currentTab == eqTabIndex)
            {
                // One freq seg-group centred under each of the three gain
                // knobs above (Low/Mid/High) — wider than the knob cells
                // themselves (fineCellW=96 was cramming each group's 4
                // buttons into ~22px each, too narrow to read "1.5k"/
                // "10k"-style labels).
                content.removeFromTop(4);
                auto segRow = content.removeFromTop(22);
                constexpr int segCellW = 170;
                int totalW = juce::jmin(segRow.getWidth(), segCellW * 3);
                auto rowArea = segRow.withSizeKeepingCentre(totalW, segRow.getHeight()).withY(segRow.getY());
                eqLowFreqSeg->setBounds(rowArea.removeFromLeft(segCellW).reduced(10, 1));
                eqMidFreqSeg->setBounds(rowArea.removeFromLeft(segCellW).reduced(10, 1));
                eqHighFreqSeg->setBounds(rowArea.removeFromLeft(segCellW).reduced(10, 1));
            }

            content.removeFromTop(10);
            auto titleRow = content.removeFromTop(bigVizTitleH);
            if (currentTab == gateTabIndex)
            {
                gateListenBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
                gateScBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
            }
            else if (currentTab == essTabIndex)
                essListenBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
            else if (currentTab == compTabIndex)
                compRatioSeg->setBounds(titleRow.removeFromRight(220).reduced(0, 1));
            else if (currentTab == dlyTabIndex)
                dlyTimeSeg->setBounds(titleRow.removeFromRight(240).reduced(0, 1));
            else if (currentTab == optoTabIndex)
                optoModeSeg->setBounds(titleRow.removeFromRight(160).reduced(0, 1));
            else if (currentTab == satTabIndex)
                satCharSeg->setBounds(titleRow.removeFromRight(260).reduced(0, 1));
            auto vizArea = content.removeFromTop(bigVizH);

            bv->title->setBounds(titleRow);
            bv->comp->setBounds(vizArea);
        }
        else
        {
            layoutKnobRow(pageKnobs[(size_t) currentTab], content, fineLabelH, fineKnobW, fineKnobH, fineCellW);
        }
    }
}
