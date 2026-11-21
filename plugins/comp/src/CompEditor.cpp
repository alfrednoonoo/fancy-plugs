#include "CompEditor.h"

using namespace fancy::ui;

namespace
{
// Figma Reacomp frame without its host title bar, widened by the GR panel.
constexpr int baseWidth = 934;
constexpr int baseHeight = 527;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr int gridLeft = 212;
constexpr int gridWidth = 524;
constexpr int rowTop = 50;
constexpr int rowHeight = 374;

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, 14.0f));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
}
} // namespace

void CompEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgSection);

    g.setColour (colours::bgBody);
    g.fillRect (0, rowTop, 108, rowHeight);
    g.fillRect (112, rowTop, 96, rowHeight);
    g.fillRect (gridLeft, rowTop, gridWidth, 199);
    g.fillRect (gridLeft, 253, 260, 171);
    g.fillRect (476, 253, 260, 171);
}

CompEditor::CompEditor (CompProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      detectorDropdown (parameter ("detector")),
      editLink (colours::uiTeal),
      thresholdMeter (parameter ("threshold")),
      ratioKnob (parameter ("ratio"), "Ratio", colours::uiTeal, Knob::mediumStyle),
      kneeKnob (parameter ("knee"), "Knee", colours::uiTeal, Knob::mediumStyle),
      attackKnob (parameter ("attack"), "Attack", colours::uiTeal, Knob::mediumStyle),
      releaseKnob (parameter ("release"), "Release", colours::uiTeal, Knob::mediumStyle),
      weirdCheckbox (parameter ("weird"), "Weird", colours::uiTealBright),
      classicCheckbox (parameter ("classic"), "Classic", colours::uiTealBright),
      autoCheckbox (parameter ("auto"), "Auto", colours::uiTealBright),
      preCompKnob (parameter ("precomp"), "Pre-comp", colours::accentGreen, Knob::mediumStyle),
      rmsKnob (parameter ("rms"), "RMS size", colours::accentGreen, Knob::mediumStyle),
      lowpassKnob (parameter ("lowpass"), "Lowpass", colours::uiBlue, Knob::mediumStyle),
      highpassKnob (parameter ("highpass"), "Highpass", colours::uiBlue, Knob::mediumStyle),
      linkCheckbox (parameter ("link"), "Link channels"),
      makeupCheckbox (parameter ("makeup"), "Auto make-up"),
      previewCheckbox (getFancyProcessor(), "Preview filter"),
      removedCheckbox (getFancyProcessor(), "Listen to removed", colours::uiTeal, 1),
      limitCheckbox (parameter ("limit"), "Limit output"),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      oversamplingDropdown (parameter ("oversampling")),
      phaseDropdown (parameter ("osPhase")),
      releaseCurveDropdown (parameter ("releaseCurve")),
      feedbackCheckbox (parameter ("feedback"), "Feedback detection"),
      attack90Checkbox (parameter ("attack90"), "Attack = 90 % rise"),
      linkAmountKnob (parameter ("linkAmount"), "Link amount", colours::uiTeal, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      detectorCurve (makeParameters()),
      scLowFreqKnob (parameter ("scLowFreq"), "Freq", colours::uiBlue, Knob::tightStyle),
      scLowGainKnob (parameter ("scLowGain"), "Gain", colours::uiBlue, Knob::tightStyle, true),
      scMidFreqKnob (parameter ("scMidFreq"), "Freq", colours::uiBlue, Knob::tightStyle),
      scMidGainKnob (parameter ("scMidGain"), "Gain", colours::uiBlue, Knob::tightStyle, true),
      scMidQKnob (parameter ("scMidQ"), "Q", colours::uiBlue, Knob::tightStyle),
      scHighFreqKnob (parameter ("scHighFreq"), "Freq", colours::uiBlue, Knob::tightStyle),
      scHighGainKnob (parameter ("scHighGain"), "Gain", colours::uiBlue, Knob::tightStyle, true),
      autoThresholdCheckbox (parameter ("autoThr"), "Auto threshold"),
      typeSegments (parameter ("charType"), colours::uiTeal, colours::bgWell),
      positionSegments (parameter ("charPos"), colours::uiTeal, colours::bgWell),
      driveKnob (parameter ("charDrive"), "Drive", colours::uiTeal, Knob::mediumStyle),
      characterCurve (makeParameters()),
      upwardCheckbox (parameter ("up"), "Upward", colours::accentGreen),
      upThresholdKnob (parameter ("upThr"), "Up thresh", colours::accentGreen, Knob::tightStyle),
      upRatioKnob (parameter ("upRatio"), "Up ratio", colours::accentGreen, Knob::tightStyle),
      maxLiftKnob (parameter ("upLift"), "Max lift", colours::accentGreen, Knob::tightStyle),
      floorKnob (parameter ("upFloor"), "Floor", colours::accentGreen, Knob::tightStyle),
      upwardCurve (makeParameters()),
      holdKnob (parameter ("hold"), "Hold", colours::uiTeal, Knob::smallStyle),
      rangeKnob (parameter ("range"), "Range", colours::uiTeal, Knob::smallStyle),
      zoomSwitch (juce::StringArray { "1.5 s", "3 s", "6 s", "12 s" }),
      gridCheckbox ("Grid"),
      historyBuffer ((size_t) fancy::dsp::LevelHistoryRecorder::capacity)
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    styleLabel (detectorLabel, "Detector input", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    detectorLabel.setBounds (24, 13, 100, 24);
    detectorDropdown.setBounds (129, 13, 131, 24);
    editLink.setBounds (656, 13, 64, 24);

    thresholdMeter.setBounds (0, 74, 108, 326);
    gainReductionMeter.setBounds (112, 74, 96, 326);

    ratioKnob.setBounds (258, 75, 90, 123);
    kneeKnob.setBounds (372, 75, 90, 123);
    attackKnob.setBounds (486, 75, 90, 123);
    releaseKnob.setBounds (600, 75, 90, 123);
    weirdCheckbox.setBounds (388, 205, 70, 20);
    classicCheckbox.setBounds (497, 205, 76, 20);
    autoCheckbox.setBounds (619, 205, 60, 20);

    attackKnob.setShowDefaultMark (true);
    releaseKnob.setShowDefaultMark (true);
    rmsKnob.setShowDefaultMark (true);

    preCompKnob.setBounds (244, 277, 90, 123);
    rmsKnob.setBounds (350, 277, 90, 123);
    lowpassKnob.setBounds (508, 277, 90, 123);
    highpassKnob.setBounds (614, 277, 90, 123);

    // Phase 4 Wave 1: five, spaced evenly by their measured widths (mockup, section 1).
    linkCheckbox.setBounds (24, 440, 125, 20);
    makeupCheckbox.setBounds (167, 440, 128, 20);
    previewCheckbox.setBounds (314, 440, 121, 20);
    previewCheckbox.setTooltip ("Listen to what the detector hears, through Lowpass and Highpass. Turns off when you close this window, and is never saved.");
    removedCheckbox.setBounds (453, 440, 153, 20);
    removedCheckbox.setTooltip ("Listen to what the compression takes off, before make-up. Turns off when you close this window, and is never saved.");
    limitCheckbox.setBounds (624, 440, 113, 20);

    outMeter.setBounds (750, 24, 70, 426);
    wetLabel.setBounds (820, 24, 44, 17);
    dryLabel.setBounds (866, 24, 44, 17);
    wetFader.setBounds (820, 49, 44, 401);
    dryFader.setBounds (866, 49, 44, 401);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &detectorLabel, &detectorDropdown, &editLink, &thresholdMeter, &gainReductionMeter,
             &ratioKnob, &kneeKnob, &attackKnob, &releaseKnob, &weirdCheckbox, &classicCheckbox, &autoCheckbox,
             &preCompKnob, &rmsKnob, &lowpassKnob, &highpassKnob,
             &linkCheckbox, &makeupCheckbox, &previewCheckbox, &removedCheckbox, &limitCheckbox,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    styleLabel (oversamplingLabel, "Oversampling", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (phaseLabel, "Oversampling filter", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (releaseCurveLabel, "Release curve", colours::textPrimary, juce::Justification::centredLeft);

    drawer.setBounds (gridLeft, rowTop, gridWidth, rowHeight);

    oversamplingLabel.setBounds (24, 60, 130, 24);
    oversamplingDropdown.setBounds (160, 60, 110, 24);
    phaseLabel.setBounds (24, 96, 130, 24);
    phaseDropdown.setBounds (160, 96, 110, 24);
    releaseCurveLabel.setBounds (24, 132, 130, 24);
    releaseCurveDropdown.setBounds (160, 132, 110, 24);
    feedbackCheckbox.setBounds (24, 180, 200, 20);
    attack90Checkbox.setBounds (24, 212, 200, 20);
    linkAmountKnob.setBounds (306, 60, 90, 99);
    drawerOutputKnob.setBounds (412, 60, 90, 99);

    // Phase 3 Q2.5: the view switch in the drawer's title row, after "EDIT".
    drawerTabs.setBounds (82, 12, 350, 24);
    drawerTabs.onChange = [this] (int tab) { showTab (tab); };
    drawer.addAndMakeVisible (drawerTabs);

    // Detector tab.
    detectorCurve.setBounds (24, 52, 476, 104);

    const auto groupLabel = [] (juce::Label& label, const juce::String& text)
    {
        styleLabel (label, text.toUpperCase(), colours::textSecondary, juce::Justification::centredLeft);
        label.setFont (font (Weight::semiBold, 11.0f).withExtraKerningFactor (0.06f));
    };

    groupLabel (lowShelfLabel, "Low shelf");
    groupLabel (bellLabel, "Bell");
    groupLabel (highShelfLabel, "High shelf");
    lowShelfLabel.setBounds (30, 162, 120, 14);
    bellLabel.setBounds (178, 162, 120, 14);
    highShelfLabel.setBounds (388, 162, 120, 14);

    {
        auto x = 18;

        for (auto* knob : { &scLowFreqKnob, &scLowGainKnob, &scMidFreqKnob, &scMidGainKnob, &scMidQKnob, &scHighFreqKnob, &scHighGainKnob })
        {
            knob->setBounds (x, 178, 66, 99);
            x += 66;

            if (knob == &scLowGainKnob || knob == &scMidQKnob)
                x += 14;
        }
    }

    autoThresholdCheckbox.setBounds (24, 322, 150, 20);
    styleLabel (autoOffsetLabel, {}, colours::textSecondary, juce::Justification::centredLeft);
    autoOffsetLabel.setFont (font (Weight::regular, 12.0f));
    autoOffsetLabel.setBounds (180, 322, 320, 20);

    tabContents[DrawerTabs::detector] = { &detectorCurve, &lowShelfLabel, &bellLabel, &highShelfLabel,
                                          &scLowFreqKnob, &scLowGainKnob, &scMidFreqKnob, &scMidGainKnob, &scMidQKnob, &scHighFreqKnob, &scHighGainKnob,
                                          &autoThresholdCheckbox, &autoOffsetLabel };

    // Character tab.
    styleLabel (typeLabel, "Type", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (positionLabel, "Position", colours::textPrimary, juce::Justification::centredLeft);
    typeLabel.setBounds (24, 64, 80, 24);
    positionLabel.setBounds (24, 104, 80, 24);
    typeSegments.setBounds (110, 64, 150, 24);
    positionSegments.setBounds (110, 104, 100, 24);
    typeSegments.setTextHeight (12.0f);
    positionSegments.setTextHeight (12.0f);
    driveKnob.setBounds (24, 146, 90, 123);

    styleLabel (characterNote, "Warm: even harmonics, lows saturate more.\nBright: odd harmonics, highs saturate more.\nQuiet material passes at unity.",
                colours::textSecondary, juce::Justification::topLeft);
    characterNote.setFont (font (Weight::regular, 11.0f));
    characterNote.setBounds (24, 284, 240, 60);
    characterCurve.setBounds (276, 60, 224, 224);

    tabContents[DrawerTabs::character] = { &typeLabel, &positionLabel, &typeSegments, &positionSegments, &driveKnob, &characterNote, &characterCurve };

    // Upward tab.
    upwardCheckbox.setBounds (24, 62, 120, 20);
    upThresholdKnob.setBounds (14, 96, 66, 99);
    upRatioKnob.setBounds (80, 96, 66, 99);
    maxLiftKnob.setBounds (14, 200, 66, 99);
    floorKnob.setBounds (80, 200, 66, 99);
    upwardCurve.setBounds (176, 60, 324, 290);

    tabContents[DrawerTabs::upward] = { &upwardCheckbox, &upThresholdKnob, &upRatioKnob, &maxLiftKnob, &floorKnob, &upwardCurve };

    // Phase 4 Wave 1: Hold and Range under Link amount and Output.
    holdKnob.setBounds (306, 184, 90, 99);
    rangeKnob.setBounds (412, 184, 90, 99);
    holdKnob.setTooltip ("The release waits this long after the last peak, so fast settings don't ripple on low notes.");
    rangeKnob.setTooltip ("The most gain reduction Comp will apply. Off at the top.");

    tabContents[DrawerTabs::options] = { &oversamplingLabel, &oversamplingDropdown, &phaseLabel, &phaseDropdown, &releaseCurveLabel, &releaseCurveDropdown,
                                         &feedbackCheckbox, &attack90Checkbox, &linkAmountKnob, &drawerOutputKnob, &holdKnob, &rangeKnob };

    // History page.
    zoomSwitch.setBounds (24, 56, 200, 24);
    zoomSwitch.setSelected (1);
    zoomSwitch.onChange = [this] (int zoom) { levelHistory.setZoom (zoom); updateHistoryCaption(); };

    gridCheckbox.setBounds (238, 58, 70, 20);
    gridCheckbox.setOn (true);
    gridCheckbox.onChange = [this] (bool on) { levelHistory.setGrid (on); };

    historyCaption.setBounds (300, 56, 200, 24);
    historyCaption.setTooltip ("The largest gain reduction in view. Click to clear it.");
    historyCaption.onClick = [this]
    {
        if (levelHistory.isPaused())
            levelHistory.setPaused (false);
        else
            levelHistory.clearPeak();

        updateHistoryCaption();
    };

    levelHistory.setBounds (24, 90, 476, 260);
    levelHistory.setTooltip ("Input in grey, output in teal before make-up, gain reduction in yellow. Click to pause.");
    levelHistory.onPausedChange = [this] { updateHistoryCaption(); };

    tabContents[DrawerTabs::history] = { &zoomSwitch, &gridCheckbox, &historyCaption, &levelHistory };
    updateHistoryCaption();

    for (auto& contents : tabContents)
        for (auto* item : contents)
            drawer.addChildComponent (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &oversamplingLabel, &oversamplingDropdown, &phaseLabel, &phaseDropdown, &releaseCurveLabel, &releaseCurveDropdown,
             &feedbackCheckbox, &attack90Checkbox, &linkAmountKnob, &drawerOutputKnob })
        drawer.addChildComponent (item);

    showTab (DrawerTabs::detector);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
    addLatencyNote (oversamplingDropdown);

    for (auto* knob : { &ratioKnob, &kneeKnob, &attackKnob, &releaseKnob })
        knob->setValueColour (colours::uiTealBright);

    detectorDropdown.addSection (0, "Main input");
    detectorDropdown.addSection (3, "Sidechain");
    detectorDropdown.addSection (6, "Mid/side");

    attackKnob.setTooltip ("Time constant: the time to reach about 63 % of the target. Change this in EDIT.");
    detectorCurve.setTooltip ("Drag a handle to set its band's Freq and Gain. The wheel sets the bell's Q. The bands shape only what the detector hears.");
    autoThresholdCheckbox.setTooltip ("Follows the input's long-term level, so the gain reduction stays steady. Threshold is set as if the input sat at -18 dBFS RMS.");

    updateDimming();
}

CompParameters CompEditor::makeParameters()
{
    return { [this] (const juce::String& id) -> juce::RangedAudioParameter& { return parameter (id); } };
}

void CompEditor::showTab (int tab)
{
    drawerTabs.setSelected (tab);

    for (int t = 0; t < DrawerTabs::numTabs; ++t)
        for (auto* item : tabContents[t])
            item->setVisible (t == tab);
}

void CompEditor::updateHistoryCaption()
{
    shownPeak = levelHistory.getPeakReduction();
    historyCaption.setText (levelHistory.isPaused() ? juce::String ("paused - click to go on")
                                                    : "peak " + juce::String (shownPeak, 1) + " dB",
                            levelHistory.isPaused());
}

void CompEditor::updateDimming()
{
    // A stage's settings dim while it's off, as Comp's other dependent controls do.
    const auto characterOn = parameter ("charType").getValue() > 0.01f;
    const auto upwardOn = parameter ("up").getValue() >= 0.5f;

    for (auto* item : std::initializer_list<juce::Component*> { &positionLabel, &positionSegments, &driveKnob })
        item->setAlpha (characterOn ? 1.0f : 0.4f);

    for (auto* item : std::initializer_list<juce::Component*> { &upThresholdKnob, &upRatioKnob, &maxLiftKnob, &floorKnob })
        item->setAlpha (upwardOn ? 1.0f : 0.4f);
}

void CompEditor::onTick (float seconds)
{
    auto& processor = dynamic_cast<CompProcessor&> (getFancyProcessor());

    const auto left = processor.takeGainReduction (0);
    const auto right = processor.takeGainReduction (1);

    thresholdMeter.update (processor.inputMeter, juce::jmax (left, right) > 0.5f, seconds);

    // Phase 3 Q2.5: the threshold in use while Auto threshold is on, and the drawer's graphs.
    const auto autoOn = parameter ("autoThr").getValue() >= 0.5f;
    const auto offset = processor.getAutoThresholdOffset();
    const auto thresholdDb = parameter ("threshold").convertFrom0to1 (parameter ("threshold").getValue());

    thresholdMeter.setEffectiveThreshold (autoOn ? thresholdDb + offset : -200.0f);
    autoOffsetLabel.setText (autoOn ? "offset " + juce::String (offset >= 0.0f ? "+" : "") + juce::String (offset, 1) + " dB  -  reference -18 dBFS RMS"
                                    : "reference -18 dBFS RMS",
                             juce::dontSendNotification);

    updateDimming();

    if (drawer.isVisible())
    {
        juce::String signature;

        for (const auto* id : { "highpass", "lowpass", "scLowFreq", "scLowGain", "scMidFreq", "scMidGain", "scMidQ", "scHighFreq", "scHighGain",
                                "charType", "charDrive", "threshold", "ratio", "knee", "weird", "up", "upThr", "upRatio", "upLift", "upFloor", "autoThr" })
            signature << juce::String (parameter (id).getValue(), 5) << ";";

        if (signature != graphSignature)
        {
            graphSignature = signature;
            detectorCurve.repaint();
            characterCurve.repaint();
            upwardCurve.repaint();
        }

        upwardCurve.setCurrent (processor.getDisplayLevel(), processor.getDisplayLift(), offset);
    }
    gainReductionMeter.update (left, right, seconds);
    outMeter.update (processor.outputMeter, seconds);

    // Phase 4 Wave 1: the history keeps reading while hidden, so it's current when shown.
    const auto count = processor.popHistory (historyBuffer.data(), (int) historyBuffer.size());
    levelHistory.push (historyBuffer.data(), count);

    if (drawer.isVisible() && drawerTabs.getSelected() == DrawerTabs::history)
        updateHistoryCaption();
}

void CompEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("threshold"), -4.4f);
    setValue (parameter ("highpass"), 93.0f);
    setValue (parameter ("wet"), -11.7f);
    setValue (parameter ("dry"), -4.2f);

    for (const auto* id : { "weird", "classic", "auto", "makeup", "limit" })
        setValue (parameter (id), 1.0f);

    setValue (parameter ("link"), 0.0f);

    thresholdMeter.show (-9.3f, -10.7f, true);
    gainReductionMeter.show (2.8f, 2.6f);
    outMeter.show (-4.2f, -5.8f);

    drawer.setVisible (drawerOpen);
    showTab (DrawerTabs::detector);
}

void CompEditor::showDrawerPreview (int tab)
{
    showPreview (true);

    // The values drawn in reference/phase3/23__comp_q25_mockup.png.
    setValue (parameter ("scMidFreq"), 3000.0f);
    setValue (parameter ("scMidGain"), 12.0f);
    setValue (parameter ("autoThr"), 1.0f);
    setValue (parameter ("charType"), 1.0f);
    setValue (parameter ("up"), 1.0f);
    setValue (parameter ("threshold"), -18.0f);

    thresholdMeter.setEffectiveThreshold (-12.0f);
    autoOffsetLabel.setText ("offset +6.0 dB  -  reference -18 dBFS RMS", juce::dontSendNotification);
    upwardCurve.setCurrent (-52.0f, 6.0f, 0.0f);
    updateDimming();

    // Phase 4 Wave 1: the mockup's Hold and Range, and a history of a beat at 120 BPM into
    // 3:1 at -12 dB, linked, at the 3 s zoom (reference/phase4/29__comp_w1_mockup.png).
    setValue (parameter ("hold"), 20.0f);
    setValue (parameter ("range"), 12.0f);

    {
        constexpr int columns = fancy::dsp::LevelHistoryRecorder::columnsPerSpan * 2;
        const auto secondsPerColumn = 3.0 / (double) columns;
        std::vector<fancy::dsp::LevelColumn> preview ((size_t) columns);
        auto reduction = 0.0;
        auto previousBeat = -1;

        for (int c = 0; c < columns; ++c)
        {
            const auto t = (double) c * secondsPerColumn;
            const auto beatPosition = (t - 0.11) / 0.5 + 10.0;
            const auto phase = beatPosition - std::floor (beatPosition);
            auto amplitude = phase < 0.03 ? 0.95 : 0.95 * std::exp (-phase * 3.2);
            amplitude = amplitude * (0.8 + 0.2 * std::abs (std::sin ((double) c * 0.85))) + 0.03;

            const auto levelDb = 20.0 * std::log10 (amplitude);
            const auto target = std::max (0.0, levelDb + 12.0) * (1.0 - 1.0 / 3.0);
            reduction += (target > reduction ? 0.7 : 0.03) * (target - reduction);

            auto& column = preview[(size_t) c];
            column.inputPeak = (float) amplitude;
            column.outputPeak = (float) (amplitude * std::pow (10.0, -reduction / 20.0));
            column.reductionDb[0] = column.reductionDb[1] = (float) reduction;
            column.thresholdDb = -12.0f;

            const auto beat = (int) std::floor (beatPosition);

            if (previousBeat >= 0 && beat != previousBeat)
                column.mark = beat % 4 == 0 ? fancy::dsp::LevelColumn::bar : fancy::dsp::LevelColumn::beat;

            previousBeat = beat;
        }

        zoomSwitch.setSelected (1);
        levelHistory.setZoom (1);
        levelHistory.show (preview);
        updateHistoryCaption();
    }

    showTab (tab);
}

std::vector<ShellEditor::PopupOpener> CompEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-detector", [this] { detectorDropdown.click(); } });
    openers.push_back ({ "dropdown-oversampling", [this] { drawer.setVisible (true); showTab (DrawerTabs::options); oversamplingDropdown.click(); } });
    openers.push_back ({ "tooltip-attack", [this] { showTip (attackKnob, attackKnob.getTooltip()); } });
    openers.push_back ({ "value-threshold", [this] { thresholdMeter.openValueEditor(); } });
    openers.push_back ({ "sidechain-notice", [this] { forceSidechainNotice (true); } });
    return openers;
}
