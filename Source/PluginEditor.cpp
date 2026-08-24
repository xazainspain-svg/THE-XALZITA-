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
XaLZaEditor::KnobUI& XaLZaEditor::addKnob(const juce::String& paramID, const juce::String& shortLabel,
                                           bool accent)
{
    auto k = std::make_unique<KnobUI>();
    k->slider.getProperties().set("accent", accent);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, accent ? 54 : 46, 15);
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

XaLZaEditor::ModuleMeterUI& XaLZaEditor::addModuleMeter(const juce::String& tab, int tapIn, int tapOut, int grIndex)
{
    auto it = std::find(tabNames.begin(), tabNames.end(), tab);
    jassert(it != tabNames.end());
    auto idx = (size_t) std::distance(tabNames.begin(), it);

    auto mm = std::make_unique<ModuleMeterUI>();
    mm->tapIn = tapIn;
    mm->tapOut = tapOut;
    mm->grIndex = grIndex;

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
    addModuleMeter("PRE",  (int) XaLZaProcessor::TapIn,   (int) XaLZaProcessor::TapPre,  -1);
    addModuleMeter("GATE", (int) XaLZaProcessor::TapPre,  (int) XaLZaProcessor::TapGate, -1);
    addModuleMeter("ESS",  (int) XaLZaProcessor::TapGate, (int) XaLZaProcessor::TapEss,  -1);
    addModuleMeter("COMP", (int) XaLZaProcessor::TapEss,  (int) XaLZaProcessor::TapComp,  0);
    addModuleMeter("OPTO", (int) XaLZaProcessor::TapComp, (int) XaLZaProcessor::TapOpto,  1);
    addModuleMeter("EQ",   (int) XaLZaProcessor::TapOpto, (int) XaLZaProcessor::TapEq,   -1);
    addModuleMeter("RES",  (int) XaLZaProcessor::TapEq,   (int) XaLZaProcessor::TapRes,  -1);
    addModuleMeter("SAT",  (int) XaLZaProcessor::TapRes,  (int) XaLZaProcessor::TapSat,  -1);
    addModuleMeter("DBL",  (int) XaLZaProcessor::TapSat,  (int) XaLZaProcessor::TapDbl,  -1);
    addModuleMeter("REV",  (int) XaLZaProcessor::TapDbl,  (int) XaLZaProcessor::TapRev,  -1);
    addModuleMeter("DLY",  (int) XaLZaProcessor::TapRev,  (int) XaLZaProcessor::TapDly,  -1);
    addModuleMeter("LIM",  (int) XaLZaProcessor::TapDly,  (int) XaLZaProcessor::TapLim,   2);

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

    setSize(900, 560);
    showPage(0);
    startTimerHz(30);
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

    for (auto* mm : moduleMeterByTab)
        if (mm != nullptr)
            mm->setVisible(false);
    if (currentTab >= 0 && currentTab < (int) moduleMeterByTab.size() && moduleMeterByTab[(size_t) currentTab] != nullptr)
        moduleMeterByTab[(size_t) currentTab]->setVisible(true);

    for (size_t i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setToggleState((int) i == currentTab, juce::dontSendNotification);

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
    full.removeFromTop(titleBarH);
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
        layoutKnobRow(pageKnobs[(size_t) currentTab], content, fineLabelH, fineKnobW, fineKnobH, fineCellW);
    }
}
