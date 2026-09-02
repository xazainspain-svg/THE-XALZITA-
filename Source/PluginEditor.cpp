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
            { XID::PreGain, " dB" }, { XID::PreChar, " %" }, { XID::PreHPF, " Hz" },
            { XID::GateThresh, " dB" }, { XID::GateRange, " dB" }, { XID::GateAttack, " ms" },
            { XID::GateHold, " ms" }, { XID::GateRelease, " ms" },
            { XID::EssThresh, " dB" }, { XID::EssRange, " dB" }, { XID::EssFreq, " Hz" },
            { XID::CompThresh, " dB" }, { XID::CompMakeup, " dB" }, { XID::CompAttack, " ms" },
            { XID::CompRelease, " ms" }, { XID::CompMix, " %" }, { XID::CompRatio, ":1" },
            { XID::OptoReduction, " %" }, { XID::OptoGain, " dB" }, { XID::OptoMix, " %" },
            { XID::EqLow, " dB" }, { XID::EqMid, " dB" }, { XID::EqHigh, " dB" },
            { XID::EqLowFreq, " Hz" }, { XID::EqMidFreq, " Hz" }, { XID::EqHighFreq, " Hz" },
            { XID::ResAmount, " %" }, { XID::ResSharpness, " %" }, { XID::ResReactivity, " %" },
            { XID::ResNotchLimit, " dB" }, { XID::ResLow, " Hz" }, { XID::ResHigh, " Hz" },
            { XID::SatDrive, " %" }, { XID::SatTone, " dB" }, { XID::SatCeiling, " dB" }, { XID::SatMix, " %" },
            { XID::DblDetune, " %" }, { XID::DblWidth, " %" }, { XID::DblDelay, " ms" }, { XID::DblMix, " %" },
            { XID::RevSize, " %" }, { XID::RevDecay, " s" }, { XID::RevPreDelay, " ms" }, { XID::RevMix, " %" },
            { XID::RevDuck, " %" }, { XID::RevDuckRelease, " ms" },
            { XID::RevWetHpf, " Hz" }, { XID::RevWetLpf, " Hz" },
            { XID::DlyTime, " ms" }, { XID::DlyFeedback, " %" }, { XID::DlySpread, " %" }, { XID::DlyMix, " %" },
            { XID::DlyDuck, " %" }, { XID::DlyDuckRelease, " ms" }, { XID::DlyPanRate, " Hz" },
            { XID::DlyFbHpf, " Hz" }, { XID::DlyFbLpf, " Hz" },
            { XID::LimInputGain, " dB" }, { XID::LimCeiling, " dB" }, { XID::LimRelease, " ms" },
            { XID::LimClip, " %" },
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
        //
        // Two real bugs lived here that only showed up at runtime (never
        // caught by compiling, since this is a Windows-only VST3 built via
        // CI with no local way to actually run/see the UI):
        //  1) Slider::getTextFromValue() ALWAYS appends getTextValueSuffix()
        //     AFTER calling textFromValueFunction (see juce_Slider.cpp) — so
        //     a lambda that also appended the suffix produced it twice
        //     ("0.0 dB dB"). Fixed by returning just the number here and
        //     letting the Slider append the suffix itself, exactly once.
        //  2) The attachment's own constructor already calls
        //     sendInitialUpdate() + slider.valueChanged() *before* we get a
        //     chance to install our override below, so the very first
        //     paint of every knob used the attachment's buggy 6-decimal
        //     text — visible until the user actually dragged that specific
        //     knob (that's why some knobs looked "fixed" on screen and
        //     others didn't: only the ones a preset/automation had touched
        //     since load had ever re-rendered). Fixed by forcing one
        //     updateText() call right after installing the new function.
        //  3) juce::String(double, int numberOfDecimalPlaces) only forces
        //     fixed-precision formatting when numberOfDecimalPlaces > 0 (see
        //     StackArrayStream::writeDouble in juce_String.cpp — the
        //     std::ios_base::fixed flag + precision() call are inside an
        //     `if (numDecPlaces > 0)` guard). With decimals == 0 that guard
        //     is skipped entirely and the value falls through to the
        //     std::ostream default ("general") format instead, which prints
        //     up to 6 significant digits and keeps fractional digits for
        //     any non-whole value — e.g. a Fbk HPF value of 1688.16 rendered
        //     as "1688.16" instead of "1688", while a knob that happened to
        //     sit on an exact integer (e.g. 1404.0) looked fine by pure
        //     coincidence. Every 0-decimal knob was affected the instant its
        //     live value wasn't a round number — fixed by rounding to an
        //     int explicitly ourselves instead of relying on the "0
        //     decimals" case of juce::String's double constructor.
        juce::String suffix = k->slider.getTextValueSuffix();
        k->slider.textFromValueFunction = [decimals] (double v)
        {
            return decimals > 0 ? juce::String(v, decimals)
                                 : juce::String(juce::roundToInt(v));
        };
        k->slider.valueFromTextFunction = [suffix] (const juce::String& text)
        {
            return text.upToFirstOccurrenceOf(suffix, false, false).trim().getDoubleValue();
        };
        k->slider.updateText();
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

    addChildComponent(mm->grMeter);

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
    tabNames = { "MASTER", "PRE", "COMP", "OPTO", "EQ", "SAT", "REV", "DLY", "DBL", "RES", "GATE", "ESS", "LIM" };
    pageKnobs.resize(tabNames.size());
    moduleMeterByTab.resize(tabNames.size(), nullptr);

    // ---- Page 0: overview — Master panel + whole-plugin visualisers ----
    // The old page used to also host a 12-knob macro grid (one "Intensity"
    // knob per module); that indirection layer is gone now, so this page
    // is purely the master controls, meters, goniometer, correlation meter
    // and signal-chain-flow overview — every module's real parameters live
    // only on that module's own tab.
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

    // Input Doctor: nudge Pre Gain from the REAL measured input level, not
    // a guess. Reads the same RMS reading the IN meter's teal marker
    // shows, and moves Pre Gain by exactly the dB needed to land around a
    // healthy -18dBFS average.
    autoGainBtn.setTooltip("Measure the real input level and nudge Pre Gain toward a healthy -18dBFS average");
    autoGainBtn.setColour(juce::TextButton::buttonColourId, XaLZaColour::panelBg);
    autoGainBtn.onClick = [this]
    {
        float avgDb = 0.5f * (proc.getRmsDbL((int) XaLZaProcessor::TapIn)
                             + proc.getRmsDbR((int) XaLZaProcessor::TapIn));
        if (avgDb <= -90.0f)
            return;   // no real signal coming in yet - nothing to measure

        constexpr float targetDb = -18.0f;
        float delta = targetDb - avgDb;
        if (auto* param = proc.apvts.getParameter(XID::PreGain))
        {
            float current = proc.apvts.getRawParameterValue(XID::PreGain)->load();
            float suggested = juce::jlimit(0.0f, 70.0f, current + delta);
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(suggested));
            param->endChangeGesture();
        }
    };
    addChildComponent(autoGainBtn);
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
    // 4 main knobs up top; Duck/DuckRelease and Wet HPF/LPF appended right
    // after (same pageKnobs[revTabIndex] vector, indices 4-5 and 6-7 —
    // addPage() appends) so resized() can lay the latter two pairs out
    // inside their own cards below instead of one flat 8-knob row.
    addPage("REV",  { { XID::RevSize, "Size" }, { XID::RevDecay, "Decay" }, { XID::RevPreDelay, "PreDelay" }, { XID::RevMix, "Mix" } });
    addPage("REV",  { { XID::RevDuck, "Duck" }, { XID::RevDuckRelease, "DuckRel" } });
    addPage("REV",  { { XID::RevWetHpf, "Wet HPF" }, { XID::RevWetLpf, "Wet LPF" } });
    // Damping trim + Hybrid IR blend — indices 8/9, placed beside the
    // Load IR button in the custom REV layout below (see currentTab ==
    // revTabIndex in resized()).
    addPage("REV",  { { XID::RevDamping, "Damping" }, { XID::RevHybrid, "Hybrid" } });
    // Time is a real seg-group (was the 9th knob crowding this page badly
    // enough to clip its own neighbours' labels) — fixed ms presets when
    // not synced to host tempo.
    addPage("DLY",  { { XID::DlyFeedback, "Fdbk" }, { XID::DlySpread, "Spread" },
                       { XID::DlyMix, "Mix" }, { XID::DlyDuck, "Duck" }, { XID::DlyDuckRelease, "DuckRel" },
                       { XID::DlyPanRate, "PanRate" }, { XID::DlyFbHpf, "Fbk HPF" }, { XID::DlyFbLpf, "Fbk LPF" } });
    dlyTimeSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::DlyTime,
        std::vector<SegButtonGroup::Option>{ { "100ms", 100.0f }, { "200ms", 200.0f }, { "300ms", 300.0f },
                                               { "500ms", 500.0f }, { "750ms", 750.0f } });
    addChildComponent(*dlyTimeSeg);
    // Real host-tempo sync (see runDly: queries getPlayHead() live, no
    // fabricated animation) — SYNC toggle swaps the fixed-ms Time seg-group
    // for a note-division one computed from the actual host BPM. Pre-Delay
    // is always tempo-synced, matching the mockup's Off/1/32/1/16 options.
    dlySyncBtn.setClickingTogglesState(true);
    dlySyncBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
    dlySyncBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
    dlySyncBtn.setTooltip("Sync Time to the host's tempo instead of a fixed millisecond value");
    addChildComponent(dlySyncBtn);
    dlySyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::DlySync, dlySyncBtn);
    dlyNoteDivSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::DlyNoteDiv,
        std::vector<SegButtonGroup::Option>{ { "1/16", 0.0f }, { "1/8T", 1.0f }, { "1/8", 2.0f },
                                               { "1/4T", 3.0f }, { "1/4", 4.0f }, { "1/2", 5.0f }, { "1/1", 6.0f } });
    addChildComponent(*dlyNoteDivSeg);
    dlyPreDelaySeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::DlyPreDelay,
        std::vector<SegButtonGroup::Option>{ { "Off", 0.0f }, { "1/32", 1.0f }, { "1/16", 2.0f } });
    addChildComponent(*dlyPreDelaySeg);
    addPage("DBL",  { { XID::DblDetune, "Detune" }, { XID::DblWidth, "Width" }, { XID::DblDelay, "Delay" }, { XID::DblMix, "Mix" } });
    // Voices is a real seg-group — each option genuinely changes how many
    // independent modulated delay taps runDbl sums (see DblVoiceConfig in
    // Params.h), not a relabel of an existing knob.
    dblVoicesSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::DblVoices,
        std::vector<SegButtonGroup::Option>{ { "2", 2.0f }, { "4", 4.0f }, { "6", 6.0f }, { "8", 8.0f } });
    addChildComponent(*dblVoicesSeg);
    addPage("RES",  { { XID::ResAmount, "Amount" }, { XID::ResSharpness, "Sharp" }, { XID::ResReactivity, "React" },
                       { XID::ResNotchLimit, "NotchLim" }, { XID::ResLow, "Low" }, { XID::ResHigh, "High" } });
    // Style and Bands are real seg-groups — Style scales each notch's
    // Q/detect-width (Delicate=surgical, Wide=broad), Bands genuinely
    // changes how many parallel adaptive notches run (see runRes).
    resStyleSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::ResStyle,
        std::vector<SegButtonGroup::Option>{ { "Delicate", 0.0f }, { "Vocal", 1.0f }, { "Wide", 2.0f } });
    addChildComponent(*resStyleSeg);
    resBandsSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::ResBands,
        std::vector<SegButtonGroup::Option>{ { "1", 1.0f }, { "2", 2.0f }, { "3", 3.0f }, { "4", 4.0f }, { "5", 5.0f } });
    addChildComponent(*resBandsSeg);
    addPage("GATE", { { XID::GateThresh, "Thresh" }, { XID::GateRange, "Range" }, { XID::GateAttack, "Attack" },
                       { XID::GateHold, "Hold" }, { XID::GateRelease, "Release" } });
    addPage("ESS",  { { XID::EssThresh, "Thresh" }, { XID::EssRange, "Range" }, { XID::EssFreq, "Freq" } });
    // Band is a real seg-group (matches the mockup's essBandSegs) — each
    // option genuinely changes the detector/dynEq Q and frequency bias
    // (see runEss), not a relabel of the existing Freq knob.
    essBandSeg = std::make_unique<SegButtonGroup>(proc.apvts, XID::EssBand,
        std::vector<SegButtonGroup::Option>{ { "S", 0.0f }, { "T", 1.0f }, { "CH", 2.0f } });
    addChildComponent(*essBandSeg);
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
    addChildComponent(correlationMeter);

    // Held-peak numbers under the master bars — same mono-font readout
    // every per-module meter already has, just missing here before.
    auto setupMasterDb = [this] (juce::Label& l)
    {
        l.setText("-inf", juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(XaLZaLookAndFeel::monoFont(9.0f));
        l.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
        addChildComponent(l);
    };
    setupMasterDb(masterDbIn);
    setupMasterDb(masterDbOut);

    masterLoudnessLabel.setJustificationType(juce::Justification::centred);
    masterLoudnessLabel.setFont(XaLZaLookAndFeel::monoFont(11.0f, true));
    masterLoudnessLabel.setColour(juce::Label::textColourId, XaLZaColour::accent2);
    addChildComponent(masterLoudnessLabel);

    bypassSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    bypassSummaryLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    bypassSummaryLabel.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
    bypassSummaryLabel.setText("ALL MODULES ACTIVE", juce::dontSendNotification);
    addChildComponent(bypassSummaryLabel);

    // Signal-chain flow strip: whole-plugin overview, click a node to jump
    // straight to that module's page.
    setupCap(chainFlowCap, "SIGNAL CHAIN (CLICK TO JUMP)");
    addChildComponent(chainFlow);
    addChildComponent(chainFlowCap);
    {
        const int tabIdxBySlot[XaLZaProcessor::kNumSlots] = {
            preTabIndex, gateTabIndex, essTabIndex, compTabIndex, optoTabIndex, eqTabIndex,
            resTabIndex, satTabIndex, dblTabIndex, revTabIndex, dlyTabIndex, limTabIndex
        };
        chainFlow.setTabIndices(tabIdxBySlot);
    }
    chainFlow.onNodeClicked = [this] (int tabIdx) { showPage(tabIdx); };

    // Whole-mix spectrum analyser, in the space the old 12-knob macro grid
    // used to occupy — same real FFT view as the EQ page's, just tapped
    // after Master Out Gain so it shows the true final output.
    setupCap(masterSpectrumCap, "MASTER SPECTRUM");
    addChildComponent(masterSpectrum);
    addChildComponent(masterSpectrumCap);

    // Real-time A<->B morph: SET A/SET B freeze the whole plugin's current
    // parameter values into two independent snapshots (nothing to do with
    // the stateA/stateB full-session compare feature — see the header
    // comment); MORPH then continuously blends every real parameter
    // between them, live, while the plugin keeps processing audio.
    setupCap(morphCap, "MORPH A <-> B");
    morphSlider.setRange(0.0, 100.0, 0.1);
    morphSlider.setValue(0.0, juce::dontSendNotification);
    morphSlider.setColour(juce::Slider::trackColourId, XaLZaColour::accent2);
    morphSlider.setColour(juce::Slider::thumbColourId, XaLZaColour::accent2);
    morphSlider.setTooltip("Capture two full snapshots with SET A / SET B, then drag to morph "
                            "every real parameter between them live.");
    morphSlider.onValueChange = [this]
    {
        if (morphHasA && morphHasB)
            applyMorph((float) morphSlider.getValue() / 100.0f);
    };
    morphSetA.setTooltip("Freeze the CURRENT settings of every module as morph endpoint A");
    morphSetB.setTooltip("Freeze the CURRENT settings of every module as morph endpoint B");
    morphSetA.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    morphSetB.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    morphSlider.setEnabled(false);
    morphSetA.onClick = [this] { captureMorphSnapshot(true); };
    morphSetB.onClick = [this] { captureMorphSnapshot(false); };
    addChildComponent(morphSlider);
    addChildComponent(morphCap);
    addChildComponent(morphSetA);
    addChildComponent(morphSetB);
    masterSpectrum.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);

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
    setupVizLabel(gateEnvTitle,       "POST-GATE WAVEFORM + OPEN/CLOSED ACTIVITY");
    setupVizLabel(essEnvTitle,        "SIBILANCE SPECTRUM + LIVE TARGET BAND");
    setupVizLabel(compGrTitle,        "GAIN REDUCTION METER + TRANSFER CURVE");
    setupVizLabel(optoScopeTitle,     "PHOTOCELL RESPONSE + TRANSFER CURVE");
    setupVizLabel(eqSpectrumTitle,    "RESPONSE SPECTRUM (POST-EQ)");
    setupVizLabel(resSuppressTitle,   "DYNAMIC SUPPRESSION + PER-BAND DEPTH");
    setupVizLabel(satScopeTitle,      "SATURATION CURVE + HARMONIC CONTENT");
    setupVizLabel(dblGoniometerTitle, "STEREO FIELD + PER-VOICE (POST-DOUBLER)");
    setupVizLabel(revDecayTitle,      "SPECTRAL DECAY HEATMAP + IMPULSE RESPONSE");
    setupVizLabel(dlyScopeTitle,      "PING-PONG BOUNCE + TAP TIMELINE (POST-DELAY)");
    setupVizLabel(limViewTitle,       "BRICKWALL OUTPUT + SPECTROGRAM");
    addChildComponent(preView);
    addChildComponent(gateView);
    addChildComponent(essView);
    addChildComponent(compView);
    addChildComponent(optoView);
    addChildComponent(eqSpectrum);
    addChildComponent(resView);
    addChildComponent(satView);
    addChildComponent(dblView);
    addChildComponent(revView);
    setupVizLabel(revDuckCardTitle, "SIDECHAIN DUCKING");
    addChildComponent(revDuckCurve);
    addChildComponent(revDuckFrame);
    revDuckFrame.toBack();

    revLoadIrBtn.setTooltip("Load a real impulse response (WAV/AIFF/FLAC/OGG) for the Hybrid convolution engine");
    revLoadIrBtn.onClick = [this] { loadImpulseResponseFile(); };
    addChildComponent(revLoadIrBtn);
    revIrNameLabel.setText("NO IR LOADED", juce::dontSendNotification);
    revIrNameLabel.setJustificationType(juce::Justification::centredLeft);
    revIrNameLabel.setFont(juce::Font(juce::FontOptions(9.5f)));
    revIrNameLabel.setColour(juce::Label::textColourId, XaLZaColour::textMuted);
    addChildComponent(revIrNameLabel);
    addChildComponent(dlyView);
    addChildComponent(limView);
    eqSpectrum.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
    essView.setSampleRate(proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);

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

    // Real lookahead — a fixed 5ms delay line + latency compensation (see
    // runGate), not a cosmetic toggle: with it on, the gate genuinely opens
    // ahead of a transient reaching the (now delayed) output.
    gateLookaheadBtn.setClickingTogglesState(true);
    gateLookaheadBtn.setColour(juce::TextButton::buttonOnColourId, XaLZaColour::accent);
    gateLookaheadBtn.setColour(juce::TextButton::textColourOnId, XaLZaColour::panelBg);
    gateLookaheadBtn.setTooltip("Adds ~5ms of latency so the gate can react to a transient "
                                 "before it reaches the output, instead of exactly when it arrives");
    addChildComponent(gateLookaheadBtn);
    gateLookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, XID::GateLookahead, gateLookaheadBtn);

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
        { essTabIndex,  &essView,          &essEnvTitle },
        { compTabIndex, &compView,         &compGrTitle },
        { optoTabIndex, &optoView,         &optoScopeTitle },
        { eqTabIndex,   &eqSpectrum,       &eqSpectrumTitle },
        { resTabIndex,  &resView,          &resSuppressTitle },
        { satTabIndex,  &satView,          &satScopeTitle },
        { dblTabIndex,  &dblView,          &dblGoniometerTitle },
        { revTabIndex,  &revView,          &revDecayTitle },
        { dlyTabIndex,  &dlyView,          &dlyScopeTitle },
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

void XaLZaEditor::loadImpulseResponseFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Impulse Response", juce::File(), "*.wav;*.aiff;*.aif;*.flac;*.ogg");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File() || !file.existsAsFile())
            return;

        // The real DSP-side decode/load lives in the processor (message-
        // thread call; the actual convolution engine update happens later
        // on the audio thread — see loadImpulseResponseFile()'s comment
        // in PluginProcessor.h). If that fails (unsupported/corrupt file)
        // don't touch the heatmap preview either, so the two stay honest
        // about what's actually loaded.
        if (!proc.loadImpulseResponseFile(file))
            return;

        // Separate, short editor-side decode of the SAME file purely for
        // the decay-heatmap preview below — capped to 4 real seconds
        // (plenty for any vocal-chain reverb tail) so this stays cheap;
        // the processor's own copy (used for the actual audio) has no
        // such cap beyond the generous 30s sanity ceiling.
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
        if (reader == nullptr)
            return;

        int vizLen = (int) juce::jmin((juce::int64) reader->lengthInSamples,
                                       (juce::int64) (reader->sampleRate * 4.0));
        if (vizLen <= 0)
            return;

        juce::AudioBuffer<float> irBuf((int) juce::jlimit((juce::int64) 1, (juce::int64) 2, (juce::int64) reader->numChannels), vizLen);
        reader->read(&irBuf, 0, vizLen, 0, true, true);

        loadedIrMono.assign((size_t) vizLen, 0.0f);
        int nCh = irBuf.getNumChannels();
        for (int n = 0; n < vizLen; ++n)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
                sum += irBuf.getSample(ch, n);
            loadedIrMono[(size_t) n] = sum / (float) nCh;
        }
        loadedIrSr = reader->sampleRate;
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

void XaLZaEditor::captureMorphSnapshot(bool intoA)
{
    auto& snap = intoA ? morphSnapA : morphSnapB;
    snap.clear();
    for (auto* param : proc.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            snap[ranged->paramID] = ranged->getValue();   // already normalised 0..1
    }
    (intoA ? morphHasA : morphHasB) = true;

    // Visual confirmation: the captured button goes solid accent while the
    // other stays outlined, so it's obvious at a glance which side (if
    // either) still needs capturing.
    morphSetA.setColour(juce::TextButton::buttonColourId,
                         morphHasA ? XaLZaColour::accent2 : juce::Colours::transparentBlack);
    morphSetB.setColour(juce::TextButton::buttonColourId,
                         morphHasB ? XaLZaColour::accent2 : juce::Colours::transparentBlack);
    morphSlider.setEnabled(morphHasA && morphHasB);
}

void XaLZaEditor::applyMorph(float t01)
{
    t01 = juce::jlimit(0.0f, 1.0f, t01);
    for (auto& [paramID, valueA] : morphSnapA)
    {
        auto itB = morphSnapB.find(paramID);
        if (itB == morphSnapB.end())
            continue;   // only blend parameters captured on both sides
        if (auto* param = proc.apvts.getParameter(paramID))
        {
            float blended = juce::jmap(t01, 0.0f, 1.0f, valueA, itB->second);
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, blended));
        }
    }
}

void XaLZaEditor::applyPreset(int presetIndex)
{
    auto& presets = xalzaFactoryPresets();
    if (presetIndex < 0 || presetIndex >= (int) presets.size())
        return;

    // Each preset just sets a fixed list of real parameters directly to
    // concrete values (no macro/intensity indirection anymore).
    for (auto& pv : presets[(size_t) presetIndex].paramValues)
    {
        if (auto* param = proc.apvts.getParameter(pv.first))
        {
            float norm = juce::jlimit(0.0f, 1.0f, param->convertTo0to1(pv.second));
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

    bool onOverview = (currentTab == 0);
    masterMeterIn.setVisible(onOverview);
    masterMeterOut.setVisible(onOverview);
    masterCapIn.setVisible(onOverview);
    masterCapOut.setVisible(onOverview);
    masterDbIn.setVisible(onOverview);
    masterDbOut.setVisible(onOverview);
    goniometer.setVisible(onOverview);
    goniometerCap.setVisible(onOverview);
    correlationMeter.setVisible(onOverview);
    masterLoudnessLabel.setVisible(onOverview);
    bypassSummaryLabel.setVisible(onOverview);
    chainFlow.setVisible(onOverview);
    chainFlowCap.setVisible(onOverview);
    masterSpectrum.setVisible(onOverview);
    masterSpectrumCap.setVisible(onOverview);
    morphSlider.setVisible(onOverview);
    morphCap.setVisible(onOverview);
    morphSetA.setVisible(onOverview);
    morphSetB.setVisible(onOverview);

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
    gateLookaheadBtn.setVisible(currentTab == gateTabIndex);
    essListenBtn.setVisible(currentTab == essTabIndex);
    compRatioSeg->setVisible(currentTab == compTabIndex);
    for (auto* s : { eqLowFreqSeg.get(), eqMidFreqSeg.get(), eqHighFreqSeg.get() })
        s->setVisible(currentTab == eqTabIndex);
    {
        bool onDly = currentTab == dlyTabIndex;
        bool dlySyncOn = proc.apvts.getRawParameterValue(XID::DlySync)->load() > 0.5f;
        dlyTimeSeg->setVisible(onDly && !dlySyncOn);
        dlyNoteDivSeg->setVisible(onDly && dlySyncOn);
        dlySyncBtn.setVisible(onDly);
        dlyPreDelaySeg->setVisible(onDly);
    }
    preImpedanceSeg->setVisible(currentTab == preTabIndex);
    for (auto* b : { &prePadBtn, &prePhaseBtn, &prePhantomBtn })
        b->setVisible(currentTab == preTabIndex);
    autoGainBtn.setVisible(currentTab == preTabIndex);
    optoModeSeg->setVisible(currentTab == optoTabIndex);
    satCharSeg->setVisible(currentTab == satTabIndex);
    dblVoicesSeg->setVisible(currentTab == dblTabIndex);
    for (auto* c : { (juce::Component*) &revDuckCardTitle, (juce::Component*) &revDuckCurve, (juce::Component*) &revDuckFrame,
                      (juce::Component*) &revLoadIrBtn, (juce::Component*) &revIrNameLabel })
        c->setVisible(currentTab == revTabIndex);
    resStyleSeg->setVisible(currentTab == resTabIndex);
    resBandsSeg->setVisible(currentTab == resTabIndex);
    essBandSeg->setVisible(currentTab == essTabIndex);

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
        auto grArea = area.removeFromTop(20).withWidth(totalW).withRight(area.getRight());
        mm.grMeter.setBounds(grArea);
    }
}

void XaLZaEditor::timerCallback()
{
    // fmtHeld reads the METER's held-peak value (see LedMeter::getHeldDbL/R
    // above), not the raw per-block dB — a number that changes every single
    // 30Hz frame is unreadable in practice; holding it for ~1.5s and then
    // letting it fall is what every real peak meter (hardware or plugin,
    // Insight included) actually does, and now the numbers finally agree
    // with what the bar is visually holding instead of jittering on their
    // own separate, faster ballistics.
    auto heldMaxDb = [] (const LedMeter& m) { return juce::jmax(m.getHeldDbL(), m.getHeldDbR()); };
    auto fmtHeld = [] (float v)
    {
        return v <= -99.0f ? juce::String("-inf") : juce::String(v, 1);
    };
    // Same three colour tiers the meter's own top segments already use, so
    // the number and the bar always agree about how hot the signal is
    // instead of the number staying one flat colour regardless of level.
    auto colourForDb = [] (float v)
    {
        if (v >= -0.3f) return XaLZaColour::danger;
        if (v >= -6.0f) return XaLZaColour::accent;
        return XaLZaColour::textMuted;
    };
    auto updateDbLabel = [&] (juce::Label& l, const LedMeter& m)
    {
        float v = heldMaxDb(m);
        l.setText(fmtHeld(v), juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, colourForDb(v));
    };

    masterMeterIn.setDb(proc.getMeterDbL((int) XaLZaProcessor::TapIn), proc.getMeterDbR((int) XaLZaProcessor::TapIn),
                         proc.getRmsDbL((int) XaLZaProcessor::TapIn), proc.getRmsDbR((int) XaLZaProcessor::TapIn));
    masterMeterOut.setDb(proc.getMeterDbL((int) XaLZaProcessor::TapOut), proc.getMeterDbR((int) XaLZaProcessor::TapOut),
                          proc.getRmsDbL((int) XaLZaProcessor::TapOut), proc.getRmsDbR((int) XaLZaProcessor::TapOut));
    updateDbLabel(masterDbIn, masterMeterIn);
    updateDbLabel(masterDbOut, masterMeterOut);

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
    {
        bool dlySyncOn = proc.apvts.getRawParameterValue(XID::DlySync)->load() > 0.5f;
        dlyTimeSeg->setVisible(!dlySyncOn);
        dlyNoteDivSeg->setVisible(dlySyncOn);
        dlyTimeSeg->refresh();
        dlyNoteDivSeg->refresh();
        dlyPreDelaySeg->refresh();
    }
    if (currentTab == preTabIndex)
        preImpedanceSeg->refresh();
    if (currentTab == optoTabIndex)
        optoModeSeg->refresh();
    if (currentTab == satTabIndex)
        satCharSeg->refresh();
    if (currentTab == dblTabIndex)
        dblVoicesSeg->refresh();
    if (currentTab == resTabIndex)
    {
        resStyleSeg->refresh();
        resBandsSeg->refresh();
    }
    if (currentTab == essTabIndex)
        essBandSeg->refresh();

    for (auto& mmPtr : moduleMeterStorage)
    {
        auto& mm = *mmPtr;
        // Live predecessor tap (not the fixed-order mm.tapIn) so this still
        // reads correctly after the chain has been reordered.
        int liveTapIn = mm.slotId >= 0 ? proc.getPredecessorTap(mm.slotId) : mm.tapIn;
        float inL = proc.getMeterDbL(liveTapIn), inR = proc.getMeterDbR(liveTapIn);
        float outL = proc.getMeterDbL(mm.tapOut), outR = proc.getMeterDbR(mm.tapOut);
        mm.meterIn.setDb(inL, inR, proc.getRmsDbL(liveTapIn), proc.getRmsDbR(liveTapIn));
        mm.meterOut.setDb(outL, outR, proc.getRmsDbL(mm.tapOut), proc.getRmsDbR(mm.tapOut));
        updateDbLabel(mm.dbIn, mm.meterIn);
        updateDbLabel(mm.dbOut, mm.meterOut);

        if (mm.grIndex >= 0)
            mm.grMeter.setGrDb(proc.getGrDb(mm.grIndex));
    }

    // Modules-bypassed summary: which modules are bypassed right now, at a glance.
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

        // Signal-chain flow strip: real live chain order plus each node's
        // real bypass state and real post-processing output level (same
        // MeterTap that module's own page IN/OUT bars read).
        {
            int order[XaLZaProcessor::kNumSlots];
            for (int pos = 0; pos < XaLZaProcessor::kNumSlots; ++pos)
                order[pos] = proc.getChainSlotAt(pos);
            chainFlow.setChainOrder(order);

            for (int slot = 0; slot < XaLZaProcessor::kNumSlots; ++slot)
            {
                bool slotBypassed = proc.apvts.getRawParameterValue(bypassIds[(size_t) slot].first)->load() > 0.5f;
                int tap = XaLZaProcessor::tapForSlot(slot);
                float levelDb = juce::jmax(proc.getMeterDbL(tap), proc.getMeterDbR(tap));
                chainFlow.setNodeState(slot, slotBypassed, levelDb);
            }
        }

        // Whole-mix spectrum: same real FFT view as the EQ page's, tapped
        // after Master Out Gain so it shows the true final output.
        double sampleRateMaster = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        masterSpectrum.setSampleRate(sampleRateMaster);
        float masterSpecBuf[SpectrumAnalyzer::fftSize];
        int masterSpecPos = proc.getSpecWritePosMaster();
        for (int i = 0; i < SpectrumAnalyzer::fftSize; ++i)
            masterSpecBuf[i] = proc.specSampleMaster(masterSpecPos - SpectrumAnalyzer::fftSize + i);
        masterSpectrum.update(masterSpecBuf);
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
        // Real sampled open/closed state (see GateActivityView) — not a
        // normalized envelope line like the other dynamics pages.
        gateView.pushState(proc.getGateGrDb());

        float gateWaveBuf[WaveformScope::numPoints];
        int gatePos = proc.getRawWritePos((int) XaLZaProcessor::RawGate);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            gateWaveBuf[i] = proc.rawSample((int) XaLZaProcessor::RawGate, gatePos - WaveformScope::numPoints + i);
        gateView.setWaveform(gateWaveBuf);
    }
    else if (currentTab == essTabIndex)
    {
        double sampleRateEss = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        essView.setSampleRate(sampleRateEss);

        // RawGate is genuinely Ess's own input (Ess is the very next stage
        // after Gate in the chain) — the exact signal runEss's detector
        // analyzes, not an approximation.
        float specBufEss[DeEsserSpectrumView::fftSize];
        int essPos = proc.getRawWritePos((int) XaLZaProcessor::RawGate);
        for (int i = 0; i < DeEsserSpectrumView::fftSize; ++i)
            specBufEss[i] = proc.rawSample((int) XaLZaProcessor::RawGate, essPos - DeEsserSpectrumView::fftSize + i);
        essView.update(specBufEss);

        // Mirrors runEss's own bandFreq computation exactly (see
        // PluginProcessor.cpp) so the live marker sits at the real
        // detection frequency, not a re-derived approximation.
        float essFreqHz = proc.apvts.getRawParameterValue(XID::EssFreq)->load();
        int essBandMode = (int) std::round(proc.apvts.getRawParameterValue(XID::EssBand)->load());
        float essBandFreqMult = essBandMode == 0 ? 1.0f : (essBandMode == 2 ? 0.7f : 0.85f);
        float essTargetHz = juce::jlimit(1000.0f, 16000.0f, essFreqHz * essBandFreqMult);
        essView.setTarget(essTargetHz, proc.getEssReductionDb());
    }
    else if (currentTab == compTabIndex)
    {
        // Real analog-style needle gauge — pushed the raw GR dB straight
        // from getGrDb(0), the exact number driving the audio, not a
        // separately-derived normalized pair like the old dual-line graph.
        compView.push(proc.getGrDb(0));

        compView.setCurve(proc.apvts.getRawParameterValue(XID::CompThresh)->load(),
                           proc.apvts.getRawParameterValue(XID::CompRatio)->load(),
                           proc.apvts.getRawParameterValue(XID::CompMakeup)->load(),
                           proc.apvts.getRawParameterValue(XID::CompMix)->load() / 100.0f);
    }
    else if (currentTab == optoTabIndex)
    {
        // Real photocell glow — pushed the raw GR dB straight from
        // getGrDb(1), the exact value driving the Opto stage's own gain.
        optoView.pushGrDb(proc.getGrDb(1));

        // Opto's "Reduction" knob maps to an internal threshold at a fixed
        // 4:1 ratio — mirrors the exact mapping processBlock's OPTO block uses.
        float reduction = proc.apvts.getRawParameterValue(XID::OptoReduction)->load() / 100.0f;
        float optoThreshDb = juce::jmap(reduction, 0.0f, 1.0f, 0.0f, -30.0f);
        optoView.setCurve(optoThreshDb, 4.0f,
                           proc.apvts.getRawParameterValue(XID::OptoGain)->load(),
                           proc.apvts.getRawParameterValue(XID::OptoMix)->load() / 100.0f);
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

        eqSpectrum.setEqCurve(proc.apvts.getRawParameterValue(XID::EqLow)->load(),
                               proc.apvts.getRawParameterValue(XID::EqLowFreq)->load(),
                               proc.apvts.getRawParameterValue(XID::EqMid)->load(),
                               proc.apvts.getRawParameterValue(XID::EqMidFreq)->load(),
                               proc.apvts.getRawParameterValue(XID::EqHigh)->load(),
                               proc.apvts.getRawParameterValue(XID::EqHighFreq)->load(),
                               sampleRate);
    }
    else if (currentTab == resTabIndex)
    {
        resView.push(juce::jlimit(0.0f, 1.0f, -proc.getResCutDb() / 24.0f));

        int numBands = juce::jlimit(1, 5, (int) std::round(proc.apvts.getRawParameterValue(XID::ResBands)->load()));
        float perBand[5];
        for (int b = 0; b < numBands; ++b)
            perBand[b] = proc.getResBandCutDb(b);
        resView.setBandData(numBands, perBand);
    }
    else if (currentTab == satTabIndex)
    {
        // Real analytic waveshaping curve — the exact same shape()/ceiling/
        // mix math runSat applies (see PluginProcessor.cpp), not a fake
        // curve; this is what actually differs between Tube/Tape/
        // Transistor/Diode.
        int satCharMode = (int) std::round(proc.apvts.getRawParameterValue(XID::SatChar)->load());
        satView.setCurve(satCharMode,
                          proc.apvts.getRawParameterValue(XID::SatDrive)->load() / 100.0f,
                          proc.apvts.getRawParameterValue(XID::SatCeiling)->load(),
                          proc.apvts.getRawParameterValue(XID::SatMix)->load() / 100.0f);

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
        dblView.setPoints(pts);

        int dblVoices = 2 * (int) std::round(proc.apvts.getRawParameterValue(XID::DblVoices)->load() / 2.0f);
        float dblDelayMs = proc.apvts.getRawParameterValue(XID::DblDelay)->load();
        float dblWidthPct = proc.apvts.getRawParameterValue(XID::DblWidth)->load() / 100.0f;
        dblView.setVoiceData(juce::jlimit(2, 8, dblVoices), dblDelayMs, dblWidthPct);
    }
    else if (currentTab == revTabIndex)
    {
        float duckPct = proc.apvts.getRawParameterValue(XID::RevDuck)->load() / 100.0f;
        float duckRelMs = proc.apvts.getRawParameterValue(XID::RevDuckRelease)->load();
        revDuckCurve.setCurve(duckPct, duckRelMs);

        float hybridPct = proc.apvts.getRawParameterValue(XID::RevHybrid)->load() / 100.0f;
        bool showRealIr = proc.isIrLoaded() && hybridPct > 0.0005f && !loadedIrMono.empty();
        revIrNameLabel.setText(proc.isIrLoaded() ? proc.getIrFileName() : juce::String("NO IR LOADED"),
                                juce::dontSendNotification);
        revIrNameLabel.setColour(juce::Label::textColourId,
                                  showRealIr ? XaLZaColour::accent2 : XaLZaColour::textMuted);

        // Recompute the Impulse Response a few times a second (not every
        // frame — it's message-thread work, not audio-thread, but no need
        // to redo it 30x/sec for a control that changes slowly).
        if (++irProbeCounter >= 10)
        {
            irProbeCounter = 0;

            if (showRealIr)
            {
                // Hybrid > 0 and a real IR is loaded: show the ACTUAL
                // loaded impulse (the same file feeding the convolution
                // engine), not the algorithmic probe below — genuinely
                // honest once the user is actually using it.
                int lenSamples = (int) loadedIrMono.size();
                revView.setImpulseResponse(loadedIrMono.data(), lenSamples, loadedIrSr);

                float irDisp[WaveformScope::numPoints];
                int stride = juce::jmax(1, lenSamples / WaveformScope::numPoints);
                for (int i = 0; i < WaveformScope::numPoints; ++i)
                {
                    int start = i * stride;
                    int end = juce::jmin(lenSamples, start + stride);
                    float best = 0.0f;
                    for (int n = start; n < end; ++n)
                        if (std::abs(loadedIrMono[(size_t) n]) > std::abs(best)) best = loadedIrMono[(size_t) n];
                    irDisp[i] = juce::jlimit(-1.0f, 1.0f, best);
                }
                revView.setIrWaveform(irDisp);
            }
            else
            {
                double srIr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
                int lenSamples = (int) (srIr * 1.6);
                if (irProbeBuffer.getNumSamples() != lenSamples)
                    irProbeBuffer.setSize(1, lenSamples, false, false, true);
                irProbeBuffer.clear();
                irProbeBuffer.setSample(0, 0, 1.0f);

                juce::dsp::ProcessSpec probeSpec { srIr, (juce::uint32) lenSamples, 1u };
                irProbeReverb.prepare(probeSpec);
                irProbeReverb.reset();

                float sizePct = proc.apvts.getRawParameterValue(XID::RevSize)->load() / 100.0f;
                float decaySec = proc.apvts.getRawParameterValue(XID::RevDecay)->load();
                float dampingTrimPct = proc.apvts.getRawParameterValue(XID::RevDamping)->load();
                juce::dsp::Reverb::Parameters rp;
                rp.roomSize   = juce::jlimit(0.0f, 1.0f, sizePct);
                rp.damping    = juce::jlimit(0.05f, 0.95f,
                                    juce::jmap(decaySec, 0.3f, 8.0f, 0.9f, 0.1f)
                                    + (dampingTrimPct - 50.0f) / 50.0f * 0.3f);
                rp.wetLevel   = 1.0f;
                rp.dryLevel   = 0.0f;
                rp.width      = 1.0f;
                rp.freezeMode = 0.0f;
                irProbeReverb.setParameters(rp);

                juce::dsp::AudioBlock<float> irBlock(irProbeBuffer);
                juce::dsp::ProcessContextReplacing<float> irCtx(irBlock);
                irProbeReverb.process(irCtx);

                auto* raw = irProbeBuffer.getReadPointer(0);
                // The full-resolution buffer goes straight to the spectral
                // heatmap (it needs real sample-rate content to FFT, not the
                // decimated display trace below).
                revView.setImpulseResponse(raw, lenSamples, srIr);

                float irDisp[WaveformScope::numPoints];
                int stride = juce::jmax(1, lenSamples / WaveformScope::numPoints);
                for (int i = 0; i < WaveformScope::numPoints; ++i)
                {
                    // Peak-hold within each bucket (preserving sign) so the
                    // trace still shows the algorithm's early reflections
                    // rather than aliasing them away with plain decimation.
                    int start = i * stride;
                    int end = juce::jmin(lenSamples, start + stride);
                    float best = 0.0f;
                    for (int n = start; n < end; ++n)
                        if (std::abs(raw[n]) > std::abs(best)) best = raw[n];
                    irDisp[i] = juce::jlimit(-1.0f, 1.0f, best);
                }
                revView.setIrWaveform(irDisp);
            }
        }
    }
    else if (currentTab == dlyTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawDly);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawDly, pos - WaveformScope::numPoints + i);
        dlyView.setSamples(buf);
        dlyView.setPanPhase(proc.getDlyPanPhase());

        // Same real computation runDly does — live host BPM (or the 120
        // fallback), current Time/Sync/Pre-Delay settings — so the timeline
        // always matches what's actually playing.
        double bpm = 120.0;
        if (auto* ph = proc.getPlayHead())
            if (auto posInfo = ph->getPosition())
                if (auto b = posInfo->getBpm())
                    bpm = *b;
        bpm = juce::jlimit(20.0, 300.0, bpm);
        float wholeNoteMs = (float) (240000.0 / bpm);

        bool dlySyncOn = proc.apvts.getRawParameterValue(XID::DlySync)->load() > 0.5f;
        float timeMs;
        if (dlySyncOn)
        {
            int divIdx = juce::jlimit(0, DlyNoteTable::kNumDivs - 1,
                             (int) std::round(proc.apvts.getRawParameterValue(XID::DlyNoteDiv)->load()));
            timeMs = wholeNoteMs * DlyNoteTable::wholeNoteFraction[divIdx];
        }
        else
        {
            timeMs = proc.apvts.getRawParameterValue(XID::DlyTime)->load();
        }

        int preDivIdx = juce::jlimit(0, 2, (int) std::round(proc.apvts.getRawParameterValue(XID::DlyPreDelay)->load()));
        float preDelayMs = preDivIdx == 1 ? wholeNoteMs / 32.0f : (preDivIdx == 2 ? wholeNoteMs / 16.0f : 0.0f);
        float fbPct = proc.apvts.getRawParameterValue(XID::DlyFeedback)->load() / 100.0f;
        dlyView.setTapData(preDelayMs, timeMs, fbPct);
    }
    else if (currentTab == limTabIndex)
    {
        float buf[WaveformScope::numPoints];
        int pos = proc.getRawWritePos((int) XaLZaProcessor::RawLim);
        for (int i = 0; i < WaveformScope::numPoints; ++i)
            buf[i] = proc.rawSample((int) XaLZaProcessor::RawLim, pos - WaveformScope::numPoints + i);
        float limCeilingDb = proc.apvts.getRawParameterValue(XID::LimCeiling)->load();
        limView.update(buf, proc.getLufs(), proc.getTruePeakDb(), limCeilingDb);

        // Spectrogram: a genuine FFT of the same post-limiter tap above,
        // just a wider window (fftSize, not WaveformScope::numPoints) —
        // one real new time-column per frame, not a reused/decimated copy
        // of the waveform trace.
        double sampleRateLim = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        limView.setSampleRate(sampleRateLim);
        float specBufLim[Spectrogram::fftSize];
        for (int i = 0; i < Spectrogram::fftSize; ++i)
            specBufLim[i] = proc.rawSample((int) XaLZaProcessor::RawLim, pos - Spectrogram::fftSize + i);
        limView.pushSpectrogramBlock(specBufLim);
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

        // Real Pearson correlation coefficient over the same window, the
        // standard phase-correlation reading every pro meter pairs with a
        // stereo scatter plot: sum(L*R) / sqrt(sum(L^2) * sum(R^2)).
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
        for (auto& p : pts)
        {
            sumLR += (double) p.first * (double) p.second;
            sumLL += (double) p.first * (double) p.first;
            sumRR += (double) p.second * (double) p.second;
        }
        double denom = std::sqrt(sumLL * sumRR);
        float corr = denom > 1.0e-9 ? (float) (sumLR / denom) : 1.0f;   // silence reads as mono-safe, not undefined
        correlationMeter.setCorrelation(corr);
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

    // Master mini-panel background (only meaningful on the overview page, but
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
            auto meterRow = masterPanel.removeFromTop(11 + 34 + 13);
            int colW = (meterRow.getWidth() - 10) / 2;
            auto inCol = meterRow.removeFromLeft(colW);
            meterRow.removeFromLeft(10);
            auto outCol = meterRow;

            masterCapIn.setBounds(inCol.removeFromTop(11));
            masterMeterIn.setBounds(inCol.removeFromTop(34));
            masterDbIn.setBounds(inCol);
            masterCapOut.setBounds(outCol.removeFromTop(11));
            masterMeterOut.setBounds(outCol.removeFromTop(34));
            masterDbOut.setBounds(outCol);
        }

        masterPanel.removeFromTop(8);
        masterLoudnessLabel.setBounds(masterPanel.removeFromTop(16));

        masterPanel.removeFromTop(10);
        goniometerCap.setBounds(masterPanel.removeFromTop(11));
        auto corrRow = masterPanel.removeFromBottom(16);
        masterPanel.removeFromBottom(3);
        correlationMeter.setBounds(corrRow);
        {
            auto side = juce::jmin(masterPanel.getWidth(), masterPanel.getHeight());
            goniometer.setBounds(masterPanel.withSizeKeepingCentre(side, side).withY(masterPanel.getY()));
        }

        content.removeFromRight(8);
        bypassSummaryLabel.setBounds(content.removeFromBottom(16));
        content.removeFromBottom(4);

        chainFlow.setBounds(content.removeFromBottom(34));
        chainFlowCap.setBounds(content.removeFromBottom(11));
        content.removeFromBottom(6);

        // Real-time A<->B morph strip: two small capture buttons flanking
        // the blend slider, right above the master spectrum.
        {
            auto morphRow = content.removeFromTop(macroLabelH);
            morphCap.setBounds(morphRow);
            auto sliderRow = content.removeFromTop(22);
            morphSetA.setBounds(sliderRow.removeFromLeft(52));
            sliderRow.removeFromLeft(6);
            morphSetB.setBounds(sliderRow.removeFromRight(52));
            sliderRow.removeFromRight(6);
            morphSlider.setBounds(sliderRow);
            content.removeFromTop(6);
        }

        // Whole-mix spectrum analyser fills the space the old 12-knob macro
        // grid used to occupy.
        masterSpectrumCap.setBounds(content.removeFromTop(macroLabelH));
        masterSpectrum.setBounds(content.reduced(0, 2));
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
            if (currentTab == revTabIndex)
            {
                // Only the 4 main knobs (Size/Decay/PreDelay/Mix) go in the
                // top row — Duck/DuckRel and Wet HPF/LPF get their own
                // section below (see the ctrlRow block further down).
                std::vector<KnobUI*> mainKnobs(pageKnobs[(size_t) currentTab].begin(),
                                                pageKnobs[(size_t) currentTab].begin() + 4);
                layoutKnobRow(mainKnobs, knobArea, fineLabelH, fineKnobW, fineKnobH, fineCellW);
            }
            else
            {
                layoutKnobRow(pageKnobs[(size_t) currentTab], knobArea, fineLabelH, fineKnobW, fineKnobH, fineCellW);
            }

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

                // Input Doctor, its own row underneath — real measurement
                // driven, deliberately kept apart from the fixed toggles.
                content.removeFromTop(6);
                auto gainRow = content.removeFromTop(22);
                autoGainBtn.setBounds(gainRow.withSizeKeepingCentre(110, gainRow.getHeight()).withY(gainRow.getY()));
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

            if (currentTab == revTabIndex)
            {
                // Duck/DuckRelease live inside a bordered "Sidechain
                // Ducking" card with their own real curve; Wet HPF/LPF sit
                // beside it as a normal small knob pair; Damping/Hybrid
                // plus the Load IR button+name fill the remaining space on
                // the right — see addPage("REV", ...) above for why
                // pageKnobs[revTabIndex] has these at indices 4/5, 6/7 and
                // 8/9 after the 4 main knobs.
                content.removeFromTop(6);
                constexpr int ctrlRowH = 126, cardW = 380, wetGap = 18, wetAreaW = fineCellW * 2;
                auto ctrlRow = content.removeFromTop(ctrlRowH);

                auto cardArea = ctrlRow.removeFromLeft(cardW);
                revDuckFrame.setBounds(cardArea);
                auto cardInner = cardArea.reduced(10, 6);
                revDuckCardTitle.setBounds(cardInner.removeFromTop(14));
                cardInner.removeFromTop(2);

                revDuckCurve.setBounds(cardInner.removeFromRight(150).reduced(0, 4));
                cardInner.removeFromRight(6);

                auto& knobs = pageKnobs[(size_t) revTabIndex];
                layoutKnobRow({ knobs[4], knobs[5] }, cardInner, fineLabelH, fineKnobW, fineKnobH, fineCellW);

                ctrlRow.removeFromLeft(wetGap);
                auto wetArea = ctrlRow.removeFromLeft(wetAreaW);
                layoutKnobRow({ knobs[6], knobs[7] }, wetArea, fineLabelH, fineKnobW, fineKnobH, fineCellW);

                ctrlRow.removeFromLeft(wetGap);
                auto irArea = ctrlRow;
                auto irHeader = irArea.removeFromTop(18);
                revLoadIrBtn.setBounds(irHeader.removeFromLeft(66));
                irHeader.removeFromLeft(6);
                revIrNameLabel.setBounds(irHeader);
                irArea.removeFromTop(4);
                layoutKnobRow({ knobs[8], knobs[9] }, irArea, fineLabelH, fineKnobW, fineKnobH, fineCellW);
            }

            content.removeFromTop(10);
            auto titleRow = content.removeFromTop(bigVizTitleH);
            if (currentTab == gateTabIndex)
            {
                gateListenBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
                gateScBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
                titleRow.removeFromRight(6);
                gateLookaheadBtn.setBounds(titleRow.removeFromRight(84).reduced(0, 1));
            }
            else if (currentTab == essTabIndex)
            {
                essListenBtn.setBounds(titleRow.removeFromRight(64).reduced(0, 1));
                titleRow.removeFromRight(6);
                essBandSeg->setBounds(titleRow.removeFromRight(150).reduced(0, 1));
            }
            else if (currentTab == compTabIndex)
                compRatioSeg->setBounds(titleRow.removeFromRight(220).reduced(0, 1));
            else if (currentTab == dlyTabIndex)
            {
                // Pre-Delay first (always visible), then SYNC, then whichever
                // of Time (fixed ms) / Note Division (tempo-synced) is
                // currently showing — both get bounds set since only one is
                // visible at a time, harmless for the hidden one.
                dlyPreDelaySeg->setBounds(titleRow.removeFromRight(140).reduced(0, 1));
                titleRow.removeFromRight(6);
                dlySyncBtn.setBounds(titleRow.removeFromRight(50).reduced(0, 1));
                titleRow.removeFromRight(6);
                auto timeArea = titleRow.removeFromRight(260).reduced(0, 1);
                dlyTimeSeg->setBounds(timeArea);
                dlyNoteDivSeg->setBounds(timeArea);
            }
            else if (currentTab == optoTabIndex)
                optoModeSeg->setBounds(titleRow.removeFromRight(160).reduced(0, 1));
            else if (currentTab == satTabIndex)
                satCharSeg->setBounds(titleRow.removeFromRight(260).reduced(0, 1));
            else if (currentTab == dblTabIndex)
                dblVoicesSeg->setBounds(titleRow.removeFromRight(180).reduced(0, 1));
            else if (currentTab == resTabIndex)
            {
                resBandsSeg->setBounds(titleRow.removeFromRight(220).reduced(0, 1));
                titleRow.removeFromRight(6);
                resStyleSeg->setBounds(titleRow.removeFromRight(200).reduced(0, 1));
            }
            // REV's ducking card + wet-tone knob pair eat into the vertical
            // budget other bigViz pages spend entirely on the viz area, so
            // its decay/IR card is shorter than the usual 210px — still
            // plenty to read a scrolling level graph and a short IR trace.
            auto vizArea = content.removeFromTop(currentTab == revTabIndex ? 110 : bigVizH);

            bv->title->setBounds(titleRow);
            bv->comp->setBounds(vizArea);
        }
        else
        {
            layoutKnobRow(pageKnobs[(size_t) currentTab], content, fineLabelH, fineKnobW, fineKnobH, fineCellW);
        }
    }
}
