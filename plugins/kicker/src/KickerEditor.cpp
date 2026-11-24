#include "KickerEditor.h"

using namespace fancy::ui;

namespace
{
// Native Figma units: the Kicker frame without its 22 px host title bar. Sub mode and the
// Boost | Sub switch follow reference/phase3/21__kicker_sub_mockup.png; Boost's knobs share
// Sub's positions (docs/kicker-sub.md, Decision 3). Phase 4 (mockup 63): the window is 40 px
// wider and the knob card with it, its knobs 20 px right, so the DUCK tag fits beside EDIT; the
// drawer's contents keep their places.
constexpr int baseWidth = 432;
constexpr int baseHeight = 472;
constexpr int leftPanelWidth = 304;
constexpr int knobShift = 20;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr InOutMeter::Geometry meterGeometry { 364.0f, 76.3f, 4.829f, 372.0f };

// The large knob, a little smaller than Figma's 116 px so the mode switch and the octave row
// fit above and below it, as the mockup draws.
constexpr Knob::Style mainStyle { 104.0f, 93.0f, 5.0f, 17.0f, 30.0f, 19.0f, 1.0f, 1.0f, 23.0f, 16.0f, true, true };

const char* const boostCaption = "Boost: lifts its own low end";

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void setUpLabel (juce::Label& label, const juce::String& text, float height, juce::Colour colour)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

KickerEditor::KickerEditor (KickerProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      panels (boostCaption, leftPanelWidth, 381),
      modeSegments (parameter ("mode"), colours::uiRed, colours::bgWell),
      freqKnob (parameter ("freq"), "Freq", colours::uiRed, mainStyle),
      wetKnob (parameter ("wet"), "Wet", colours::uiRed, Knob::mediumStyle),
      cutoffKnob (parameter ("subCutoff"), "Cutoff", colours::uiRed, mainStyle),
      subKnob (parameter ("subLevel"), "Sub", colours::uiRed, Knob::mediumStyle),
      octaveSegments (parameter ("subOctave"), colours::grey5, colours::textPrimary),
      dryKnob (parameter ("dry"), "Dry", colours::uiRed, Knob::mediumStyle),
      editLink (colours::uiRed),
      meter (meterGeometry),
      qKnob (parameter ("q"), "Filter Q", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      sensitivityKnob (parameter ("subSensitivity"), "Sensitivity", colours::uiRed, Knob::smallStyle),
      subLowpassKnob (parameter ("subLowpass"), "Low-pass", colours::uiRed, Knob::smallStyle),
      monoCheckbox (parameter ("monoBelow"), "Mono below Freq"),
      softClipCheckbox (parameter ("softClip"), "Soft clip"),
      subMonoCheckbox (parameter ("subMono"), "Mono sub"),
      subLengthKnob (parameter ("subLen"), "Length", colours::uiRed, Knob::tightStyle),
      roomFreqKnob (parameter ("roomFreq"), "Make-room", colours::uiRed, Knob::tightStyle),
      ceilingKnob (parameter ("ceiling"), "Ceiling", colours::uiTeal, Knob::tightStyle),
      roomCheckbox (parameter ("roomCut"), "Make-room"),
      polarityCheckbox (parameter ("subPolarity"), "Invert sub"),
      autoGainCheckbox (parameter ("autoGain"), "Auto gain"),
      duckKnob (parameter ("duck"), "Duck", colours::uiRed, Knob::smallStyle),
      duckReleaseKnob (parameter ("duckRelease"), "Release", colours::uiRed, Knob::smallStyle),
      duckShapeKnob (parameter ("duckShape"), "Shape", colours::uiRed, Knob::tightStyle),
      duckFromDropdown (parameter ("duckFrom")),
      duckRateDropdown (parameter ("duckRate")),
      clipOsDropdown (parameter ("clipOs"))
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    panels.setCaptionRightInset (12);
    body.addAndMakeVisible (panels);

    modeSegments.setBounds (82 + knobShift, 15, 100, 28);
    modeSegments.setTooltip ("Boost lifts the kick's own low end. Sub creates new bass an octave or two below it.");

    freqKnob.setBounds (66 + knobShift, 52, 132, 148);
    freqKnob.setShowDefaultMark (true);
    cutoffKnob.setBounds (66 + knobShift, 52, 132, 148);
    cutoffKnob.setTooltip ("The highest frequency Sub listens to. Set it just above the kick's body.");

    octaveSegments.setBounds (78 + knobShift, 205, 112, 24);
    octaveSegments.setTooltip (juce::String::fromUTF8 ("How far below the kick the sub plays. \xe2\x88\x92" "2 oct suits toms and pitched-up 808s: "
                                                       "two octaves below a 60 Hz kick is 15 Hz, which is filtered out."));

    wetKnob.setBounds (34 + knobShift, 236, 90, 123);
    subKnob.setBounds (34 + knobShift, 236, 90, 123);
    subKnob.setTooltip ("The level of the sub it creates.");
    dryKnob.setBounds (140 + knobShift, 236, 90, 123);

    editLink.setBounds (16, 377, 64, 24);
    meter.setBounds (leftPanelWidth, 0, baseWidth - leftPanelWidth, bodyHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &modeSegments, &dryKnob, &editLink, &meter })
        body.addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &freqKnob, &wetKnob, &cutoffKnob, &subKnob, &octaveSegments })
        body.addChildComponent (item);

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    qKnob.setBounds (32, 60, 90, 99);
    drawerOutputKnob.setBounds (142, 60, 90, 99);
    monoCheckbox.setBounds (32, 192, 200, 20);
    softClipCheckbox.setBounds (32, 224, 200, 20);

    sensitivityKnob.setBounds (8, 60, 84, 99);
    sensitivityKnob.setTooltip ("Below this level Sub stays silent, so bleed and noise don't make sub.");
    subLowpassKnob.setBounds (90, 60, 84, 99);
    subLowpassKnob.setTooltip ("Smooths the sub: nothing it adds goes much above this.");
    subMonoCheckbox.setBounds (32, 192, 200, 20);
    subMonoCheckbox.setTooltip ("The same sub in both channels, made from L+R. Off: each channel makes its own.");

    drawer.addAndMakeVisible (drawerOutputKnob);
    drawer.addAndMakeVisible (softClipCheckbox);

    // Phase 4 Wave 1: the Sub group under Soft clip (dimmed in Boost), then Auto gain and Ceiling.
    subLengthKnob.setBounds (14, 250, 66, 80);
    subLengthKnob.setTooltip ("Ends the sub this long after each hit, with a short fade, so a long tail doesn't run into the next kick. Off at the top.");
    roomFreqKnob.setBounds (84, 250, 66, 80);
    roomFreqKnob.setTooltip ("Where Make-room cuts the source kick.");
    roomCheckbox.setBounds (156, 262, 110, 20);
    roomCheckbox.setTooltip ("Cuts the source kick's lowest end, so the sub has it to itself.");
    polarityCheckbox.setBounds (156, 290, 110, 20);
    polarityCheckbox.setTooltip ("Flips the sub, for kicks whose own low end cancels it.");
    autoGainCheckbox.setBounds (24, 346, 110, 20);
    autoGainCheckbox.setTooltip ("Turns the output down by as much as Boost or Sub adds, so you hear the shape, not the level.");
    ceilingKnob.setBounds (150, 330, 66, 80);
    ceilingKnob.setTooltip ("Holds the output's true peak under this level. Off at the bottom.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &subLengthKnob, &roomFreqKnob, &roomCheckbox, &polarityCheckbox, &autoGainCheckbox, &ceilingKnob })
        drawer.addAndMakeVisible (item);

    // The sub meter, between In and Out.
    subMeter.setBounds (leftPanelWidth + 70, 49, 6, 315);
    subMeter.setInterceptsMouseClicks (false, false);
    body.addChildComponent (subMeter);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &qKnob, &monoCheckbox, &sensitivityKnob, &subLowpassKnob, &subMonoCheckbox })
        drawer.addChildComponent (item);

    // Phase 4 (mockup 63; drawer coordinates are the mockup's window coordinates less 73, knobs
    // placed as Exciter's Harmonics page places them). The page switch takes the drawer title's
    // place; Main is today's drawer as it was.
    drawerTabs.setBounds (20, 13, 200, 22); // wider than the mockup's 160: each half fits "Duck and clip" (run 45473290714)
    drawerTabs.onChange = [this] (int page) { showDrawerPage (page); };
    drawer.addAndMakeVisible (drawerTabs);

    setUpLabel (duckTitle, "DUCK", 10.0f, colours::textSecondary);
    setUpLabel (duckFromLabel, "Duck from", 13.0f, colours::textSecondary);
    setUpLabel (syncTitle, "HOST SYNC", 10.0f, colours::textSecondary);
    setUpLabel (rateLabel, "Rate", 13.0f, colours::textPrimary);
    setUpLabel (clipTitle, "CLIPPER", 10.0f, colours::textSecondary);
    setUpLabel (clipOsLabel, "Clip oversampling", 13.0f, colours::textSecondary);
    setUpLabel (clipNote, "Latency is reported while it's on.", 11.0f, colours::textSecondary);

    duckTitle.setBounds (24, 53, 200, 12);
    duckKnob.setBounds (19, 63, 90, 99);
    duckReleaseKnob.setBounds (131, 63, 90, 99);
    duckFromLabel.setBounds (24, 171, 100, 24);
    duckFromDropdown.setBounds (134, 171, 120, 24);
    syncTitle.setBounds (24, 209, 200, 12);
    rateLabel.setBounds (24, 227, 36, 24);
    duckRateDropdown.setBounds (62, 227, 84, 24);
    duckShapeKnob.setBounds (173, 205, 66, 80);
    clipTitle.setBounds (24, 311, 200, 12);
    clipOsLabel.setBounds (24, 329, 140, 24);
    clipOsDropdown.setBounds (184, 329, 70, 24);
    clipNote.setBounds (24, 361, 230, 14);

    duckKnob.setTooltip ("Turns the whole output down, by up to this much, while a kick hits on the sidechain (or on the beat, with Host sync), "
                         "so the bass and its added low end make room for the kick. 0 turns it off.");
    duckReleaseKnob.setTooltip ("How fast the output comes back after each kick.");
    duckFromDropdown.setTooltip ("What Duck listens to: a kick on the sidechain (pins 3/4 in REAPER), or a pulse on the host's beat grid, "
                                 "for ducking on the beat without a kick track.");
    duckRateDropdown.setTooltip ("How often Host sync ducks: once per note of this length.");
    duckShapeKnob.setTooltip ("How long each Host sync pulse holds the duck, as a share of the step. Release follows it.");
    clipOsDropdown.setTooltip ("Runs Soft clip at 2x or 4x the sample rate, so a hard-driven kick's click doesn't alias. "
                               "4x is about 35 dB cleaner on a typical kick. Off is Kicker as it was.");
    addLatencyNote (clipOsDropdown);

    mainItems = { &qKnob, &drawerOutputKnob, &sensitivityKnob, &subLowpassKnob, &monoCheckbox, &softClipCheckbox, &subMonoCheckbox,
                  &subLengthKnob, &roomFreqKnob, &roomCheckbox, &polarityCheckbox, &autoGainCheckbox, &ceilingKnob };
    syncItems = { &syncTitle, &rateLabel, &duckRateDropdown, &duckShapeKnob };
    duckItems = { &duckTitle, &duckKnob, &duckReleaseKnob, &duckFromLabel, &duckFromDropdown, &clipTitle, &clipOsLabel, &clipOsDropdown, &clipNote };

    for (auto* item : duckItems)
        drawer.addChildComponent (item);

    for (auto* item : syncItems)
        drawer.addChildComponent (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (true); };

    // The DUCK tag, beside EDIT, while Duck is above 0. Clicking it opens the Duck and clip page.
    duckTag.setBounds (76, 382, 42, 18);
    duckTag.setTooltip ("Duck is on: Kicker turns its output down while a kick hits. Click to see its settings.");
    duckTag.onClick = [this] { drawer.setVisible (true); showDrawerPage (duckPage); };
    body.addChildComponent (duckTag);
    drawer.toFront (false); // the open drawer covers the tag

    updateMode();
    showDrawerPage (mainPage);
}

bool KickerEditor::isHostSyncChosen()
{
    return juce::roundToInt (parameter ("duckFrom").convertFrom0to1 (parameter ("duckFrom").getValue())) == KickerProcessor::duckFromHostSync;
}

void KickerEditor::showDrawerPage (int page)
{
    shownPage = page;
    drawerTabs.setSelected (page);

    // Main's items also follow the mode (updateMode), so they're shown through it.
    for (auto* item : mainItems)
        item->setVisible (false);

    if (page == mainPage)
    {
        for (juce::Component* item : std::initializer_list<juce::Component*> { &drawerOutputKnob, &softClipCheckbox, &subLengthKnob, &roomFreqKnob,
                                                                               &roomCheckbox, &polarityCheckbox, &autoGainCheckbox, &ceilingKnob })
            item->setVisible (true);

        shownMode = -1;
        updateMode();
    }

    for (auto* item : duckItems)
        item->setVisible (page == duckPage);

    for (auto* item : syncItems)
        item->setVisible (page == duckPage && isHostSyncChosen());
}

void KickerEditor::updateMode()
{
    const auto sub = juce::roundToInt (parameter ("mode").convertFrom0to1 (parameter ("mode").getValue())) == 1;
    const auto twoOctaves = juce::roundToInt (parameter ("subOctave").convertFrom0to1 (parameter ("subOctave").getValue())) == 1;

    panels.setCaption (sub ? (twoOctaves ? "Sub: adds two octaves below" : "Sub: adds an octave below") : boostCaption);

    const auto mode = sub ? 1 : 0;

    if (mode == shownMode)
        return;

    shownMode = mode;

    freqKnob.setVisible (! sub);
    wetKnob.setVisible (! sub);
    cutoffKnob.setVisible (sub);
    subKnob.setVisible (sub);
    octaveSegments.setVisible (sub);

    // Phase 4: the drawer's mode items only while its Main page is showing.
    const auto onMain = shownPage == mainPage;
    qKnob.setVisible (onMain && ! sub);
    monoCheckbox.setVisible (onMain && ! sub);
    sensitivityKnob.setVisible (onMain && sub);
    subLowpassKnob.setVisible (onMain && sub);
    subMonoCheckbox.setVisible (onMain && sub);

    // Sub's drawer has three knobs in a row; Boost's has two.
    drawerOutputKnob.setBounds (sub ? juce::Rectangle<int> (172, 60, 84, 99) : juce::Rectangle<int> (142, 60, 90, 99));
    softClipCheckbox.setBounds (32, 224, 200, 20);
}

void KickerEditor::onTick (float seconds)
{
    // Phase 4 Wave 1: the sub meter and the Sub group follow the mode.
    {
        const auto sub = juce::roundToInt (parameter ("mode").convertFrom0to1 (parameter ("mode").getValue())) == 1;
        subMeter.setVisible (sub);
        subMeter.update (dynamic_cast<KickerProcessor&> (getFancyProcessor()).takeSubPeak(), seconds);

        for (auto* item : std::initializer_list<juce::Component*> { &subLengthKnob, &roomFreqKnob, &roomCheckbox, &polarityCheckbox })
            item->setAlpha (sub ? 1.0f : 0.4f);
    }

    // Phase 4: the DUCK tag while Duck is up, and Host sync's row while it's chosen.
    {
        duckTag.setVisible (parameter ("duck").convertFrom0to1 (parameter ("duck").getValue()) > 0.0f);

        const auto hostSync = isHostSyncChosen();

        for (auto* item : syncItems)
            item->setVisible (shownPage == duckPage && hostSync);
    }

    auto& processor = getFancyProcessor();
    updateMode();
    meter.update (processor.inputMeter, processor.outputMeter, seconds);
}

void KickerEditor::showPreview (bool drawerOpen, bool subMode)
{
    stopUpdates();
    setValue (parameter ("mode"), subMode ? 1.0f : 0.0f);
    updateMode();

    if (subMode)
        meter.show (-9.2f, -9.4f, -5.8f, -6.0f);
    else
        meter.show (-5.2f, -6.0f, -5.4f, -6.1f);

    drawer.setVisible (drawerOpen);
    showDrawerPage (mainPage);
    duckTag.setVisible (parameter ("duck").convertFrom0to1 (parameter ("duck").getValue()) > 0.0f);
}

void KickerEditor::showDuckPreview (bool drawerOpen)
{
    setValue (parameter ("duck"), 6.0f);
    setValue (parameter ("duckRelease"), 120.0f);
    setValue (parameter ("duckFrom"), (float) KickerProcessor::duckFromHostSync);
    setValue (parameter ("duckRate"), (float) KickerProcessor::defaultSyncRate);
    setValue (parameter ("duckShape"), 50.0f);
    setValue (parameter ("clipOs"), 2.0f);

    showPreview (drawerOpen);

    if (drawerOpen)
        showDrawerPage (duckPage);
}

juce::Colour KickerEditor::getAccentColour()
{
    return colours::uiRed;
}

std::vector<ShellEditor::PopupOpener> KickerEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "value-freq", [this] { updateMode(); freqKnob.openValueEditor(); } });
    openers.push_back ({ "dropdown-duck-from", [this] { drawer.setVisible (true); showDrawerPage (duckPage); duckFromDropdown.click(); } });
    openers.push_back ({ "dropdown-clip-oversampling", [this] { drawer.setVisible (true); showDrawerPage (duckPage); clipOsDropdown.click(); } });
    return openers;
}

// SubMeter (Phase 4 Wave 1) ---------------------------------------------------------------

void KickerEditor::SubMeter::update (float peak, float seconds)
{
    const auto db = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -100.0f;
    shownDb = db > shownDb ? db : std::max (db, shownDb - 20.0f * seconds);
    repaint();
}

void KickerEditor::SubMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRect (bounds);

    // The In and Out meters' scale: 0 dBFS near the top, -54 at the bottom.
    const auto fraction = juce::jlimit (0.0f, 1.0f, (shownDb + 54.0f) / 60.0f);
    g.setColour (colours::uiRed);
    g.fillRect (bounds.withTop (bounds.getBottom() - bounds.getHeight() * fraction));
}

// DuckTag (Phase 4, mockup 63) ------------------------------------------------------------

KickerEditor::DuckTag::DuckTag()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void KickerEditor::DuckTag::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::uiRed);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    g.setColour (colours::bgWell);
    g.setFont (font (Weight::semiBold, 10.0f).withExtraKerningFactor (0.06f));
    g.drawText ("DUCK", bounds, juce::Justification::centred, false);
}

void KickerEditor::DuckTag::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}
