#include "MultiCompEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;
using multicomp::maxBands;

namespace
{
// design/phase2-mockup-src/xcomp.html at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 417;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 632;

constexpr int graphX = 13, graphY = 10, graphWidth = 610, graphHeight = 190;
constexpr int tabRowY = 209, tabRowHeight = 22;
constexpr int cardX = 13, cardY = 238, cardWidth = 610, cardHeight = 139;
constexpr int footerY = 386;
constexpr int faderTop = 26, faderHeight = 331;

constexpr float labelHeight = 12.0f;

constexpr OutMeter::Geometry inputGeometry { 9.5f, 17.0f, 108.0f, 20.0f, 1.45f, 0.0f, false, false };
constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };
constexpr GainReductionMeter::Geometry reductionGeometry { 15.0f, 17.0f, 80.0f, true, "GR" };

const juce::Identifier selectedBandProperty ("selectedBand");

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification, float height = 14.0f)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}

juce::RangedAudioParameter& bandParameter (MultiCompProcessor& processor, int band, const char* suffix)
{
    return *processor.getApvts().getParameter (MultiCompProcessor::id (band, suffix));
}
} // namespace

// Panels ----------------------------------------------------------------------

void MultiCompEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) cardX, (float) cardY, (float) cardWidth, (float) cardHeight, 6.0f);
}

// BandEditor ------------------------------------------------------------------

MultiCompEditor::BandEditor::BandEditor (MultiCompProcessor& processor, int bandIndex, MultiCompEditor& owner)
    : multiProcessor (processor),
      band (bandIndex),
      inputMeter (inputGeometry, "In"),
      thresholdFader (bandParameter (processor, bandIndex, "thr"), multiBandColour (bandIndex)),
      reductionMeter (reductionGeometry),
      ratioKnob (bandParameter (processor, bandIndex, "ratio"), "Ratio", multiBandColour (bandIndex), Knob::tightStyle),
      kneeKnob (bandParameter (processor, bandIndex, "knee"), "Knee", multiBandColour (bandIndex), Knob::tightStyle),
      attackKnob (bandParameter (processor, bandIndex, "att"), "Attack", multiBandColour (bandIndex), Knob::tightStyle),
      releaseKnob (bandParameter (processor, bandIndex, "rel"), "Release", multiBandColour (bandIndex), Knob::tightStyle),
      rmsKnob (bandParameter (processor, bandIndex, "rms"), "RMS size", multiBandColour (bandIndex), Knob::tightStyle),
      gainKnob (bandParameter (processor, bandIndex, "gain"), "Gain", multiBandColour (bandIndex), Knob::tightStyle, true),
      makeupCheckbox (bandParameter (processor, bandIndex, "mk"), "Auto make-up"),
      autoReleaseCheckbox (bandParameter (processor, bandIndex, "ar"), "Auto release"),
      feedbackCheckbox (bandParameter (processor, bandIndex, "fb"), "Feedback detect"),
      sidechainCheckbox (bandParameter (processor, bandIndex, "sc"), "Sidechain"),
      soloButton (bandParameter (processor, bandIndex, "solo"), "S", multiBandColour (bandIndex)),
      powerIcon (bandParameter (processor, bandIndex, "on"), multiBandColour (bandIndex)),
      closeButton (IconButton::Glyph::cross)
{
    setInterceptsMouseClicks (false, true);

    styleLabel (thresholdLabel, "Threshold", colours::textSecondary, juce::Justification::centred, labelHeight);

    inputMeter.setBounds (cardX + 3, cardY + 18, 36, 120);
    thresholdLabel.setBounds (cardX + 21, cardY + 5, 56, 15);
    thresholdFader.setBounds (cardX + 27, cardY + 22, 44, 117);
    reductionMeter.setBounds (cardX + 67, cardY + 16, 38, 123);

    constexpr int knobWidth = 70, knobHeight = 99, knobTop = cardY + 8, firstCentre = 162, spacing = 64;

    const auto placeKnob = [&] (Knob& knob, int index)
    {
        knob.setBounds (firstCentre + index * spacing - knobWidth / 2, knobTop, knobWidth, knobHeight);
    };

    placeKnob (ratioKnob, 0);
    placeKnob (kneeKnob, 1);
    placeKnob (attackKnob, 2);
    placeKnob (releaseKnob, 3);
    placeKnob (rmsKnob, 4);
    placeKnob (gainKnob, 5);

    // Wider than the rest, for the make-up amount after its value.
    gainKnob.setBounds (gainKnob.getBounds().withSizeKeepingCentre (108, knobHeight));

    constexpr int checkboxY = cardY + 110;

    makeupCheckbox.setBounds (138, checkboxY, 108, 20);
    autoReleaseCheckbox.setBounds (248, checkboxY, 98, 20);
    feedbackCheckbox.setBounds (348, checkboxY, 122, 20);
    sidechainCheckbox.setBounds (472, checkboxY, 84, 20);

    for (auto* checkbox : { &makeupCheckbox, &autoReleaseCheckbox, &feedbackCheckbox, &sidechainCheckbox })
        checkbox->setLabelHeight (labelHeight);

    soloButton.setBounds (540, tabRowY, 23, tabRowHeight);
    powerIcon.setBounds (574, tabRowY + 5, 12, 13);
    closeButton.setBounds (607, tabRowY + 7, 8, 8);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &inputMeter, &thresholdLabel, &thresholdFader, &reductionMeter,
             &ratioKnob, &kneeKnob, &attackKnob, &releaseKnob, &rmsKnob, &gainKnob,
             &makeupCheckbox, &autoReleaseCheckbox, &feedbackCheckbox, &sidechainCheckbox,
             &soloButton, &powerIcon, &closeButton })
        addAndMakeVisible (item);

    closeButton.onClick = [&owner] { owner.removeSelectedBand(); };

    ratioKnob.setTooltip ("Below 1:1 the level above the threshold is raised: upward expansion.");
    attackKnob.setTooltip ("Time constant: the time to reach about 63 % of the target. Change this in EDIT.");
    gainKnob.setTooltip ("In brackets: the amount Auto make-up adds.");
}

void MultiCompEditor::BandEditor::update (int titleX, float reductionDb, float seconds)
{
    const auto spec = multiProcessor.readBand (band);
    const auto makeup = spec.autoMakeup ? (float) MultiCompDsp::autoMakeupDb (spec) : 0.0f;

    gainKnob.setValueSuffix (spec.autoMakeup ? "(+" + fancy::formatFixed (makeup, 1) + ")" : juce::String {}, multiBandColour (band));

    inputMeter.update (multiProcessor.getBandInput (band), seconds);
    reductionMeter.update (reductionDb, reductionDb, seconds);

    titleLeft = titleX;
    repaint (0, tabRowY, 536, tabRowHeight);
}

void MultiCompEditor::BandEditor::showLevels (float left, float right, float reductionDb)
{
    inputMeter.show (left, right);
    reductionMeter.show (reductionDb, reductionDb);
}

void MultiCompEditor::BandEditor::paint (juce::Graphics& g)
{
    const juce::Rectangle<int> row (titleLeft, tabRowY, 534 - titleLeft, tabRowHeight);

    g.setFont (font (Weight::semiBold, 14.0f).withExtraKerningFactor (0.04f));
    g.setColour (multiBandColour (band));
    g.drawText ("BAND " + juce::String (band + 1), row, juce::Justification::centredLeft, true);

    const auto range = fancy::formatHz ((float) multiProcessor.bandBottomHz (band)) + " " + juce::String::charToString (0x2013) + " "
                     + fancy::formatHz ((float) multiProcessor.bandTopHz (band));

    g.setFont (font (Weight::regular, 13.0f));
    g.setColour (colours::textSecondary);
    g.drawText (range, row.withTrimmedLeft (66), juce::Justification::centredLeft, true);

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (569.0f, (float) tabRowY, 23.0f, (float) tabRowHeight, 3.0f);
    g.fillRoundedRectangle (599.0f, (float) tabRowY, 24.0f, (float) tabRowHeight, 3.0f);
}

// MultiCompEditor -------------------------------------------------------------

MultiCompEditor::MultiCompEditor (MultiCompProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      graph (processor),
      detectorDropdown (parameter ("detector")),
      spectrumCheckbox (parameter ("showSpectrum"), "Show spectrum"),
      linkCheckbox (parameter ("link"), "Link bands"),
      gridCheckbox (parameter ("showGrid"), "Show grid"),
      limitCheckbox (parameter ("limit"), "Limit output"),
      crossoverDropdown (parameter ("crossover")),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      editLink (colours::uiTeal),
      attack90Checkbox (parameter ("attack90"), "Attack = 90 % rise"),
      logCheckbox (parameter ("logFreq"), "Log-scale automated freq."),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle)
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    graph.setBounds (graphX, graphY, graphWidth, graphHeight);
    graph.onBandSelected = [this] (int band) { selectBand (band); };
    body.addAndMakeVisible (graph);

    tabs.setBounds (cardX, tabRowY, 280, tabRowHeight);
    tabs.onSelect = [this] (int band) { selectBand (band); };
    tabs.onAdd = [this] { addBand(); };
    body.addAndMakeVisible (tabs);

    for (int band = 0; band < maxBands; ++band)
    {
        bandEditors.push_back (std::make_unique<BandEditor> (processor, band, *this));
        bandEditors.back()->setBounds (0, 0, rightColumnX, bodyHeight);
        body.addChildComponent (*bandEditors.back());
    }

    tabs.toFront (false);

    // No room for its label beside the workflow controls, so the dropdown says it in its tooltip.
    detectorDropdown.setBounds (getHeaderFreeLeft(), 13, getHeaderFreeRight() - getHeaderFreeLeft(), 24);
    detectorDropdown.setTextHeight (labelHeight);
    detectorDropdown.setTooltip ("Detector input");
    getRoot().addAndMakeVisible (detectorDropdown);

    spectrumCheckbox.setBounds (15, footerY, 116, 20);
    linkCheckbox.setBounds (133, footerY, 86, 20);
    gridCheckbox.setBounds (221, footerY, 82, 20);
    limitCheckbox.setBounds (305, footerY, 90, 20);

    for (auto* checkbox : { &spectrumCheckbox, &linkCheckbox, &gridCheckbox, &limitCheckbox })
        checkbox->setLabelHeight (labelHeight);

    detectorDropdown.addSection (0, "Main input");
    detectorDropdown.addSection (3, "Sidechain");

    linkCheckbox.setTooltip ("Moves all bands together. Hold Alt to edit one.");

    styleLabel (crossoverLabel, "Crossover", colours::textSecondary, juce::Justification::centredRight, labelHeight);
    crossoverLabel.setBounds (398, footerY - 2, 62, 24);
    crossoverDropdown.setBounds (466, footerY - 2, 157, 24);
    crossoverDropdown.setTextHeight (labelHeight);

    outMeter.setBounds (638, 10, 46, 350);

    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    wetLabel.setBounds (680, 10, 33, 17);
    dryLabel.setBounds (713, 10, 33, 17);
    wetFader.setBounds (680, faderTop, 33, faderHeight);
    dryFader.setBounds (713, faderTop, 33, faderHeight);

    editLink.setBounds (664, footerY - 2, 64, 24);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &spectrumCheckbox, &linkCheckbox, &gridCheckbox, &limitCheckbox, &crossoverLabel, &crossoverDropdown,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader, &editLink })
        body.addAndMakeVisible (item);

    drawer.setBounds (0, tabRowY - 2, rightColumnX, bodyHeight - tabRowY + 2);

    attack90Checkbox.setBounds (24, 60, 200, 20);
    logCheckbox.setBounds (24, 92, 220, 20);
    drawerOutputKnob.setBounds (300, 56, 90, 99);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &attack90Checkbox, &logCheckbox, &drawerOutputKnob })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
    addLatencyNote (crossoverDropdown);
    addLatencyNote (limitCheckbox);

    selectedBand = juce::jlimit (0, maxBands - 1, (int) processor.getApvts().state.getProperty (selectedBandProperty, 0));
    graph.setSelectedBand (selectedBand);

    updateSelection();
}

MultiCompProcessor& MultiCompEditor::multiProcessor() noexcept
{
    return dynamic_cast<MultiCompProcessor&> (getFancyProcessor());
}

bool MultiCompEditor::listsParameter (const juce::AudioProcessorParameter& item)
{
    const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (&item);

    if (ranged == nullptr || ! item.isAutomatable())
        return false;

    const auto parameterId = ranged->getParameterID();
    const auto isBandParameter = parameterId[0] == 'b' && juce::CharacterFunctions::isDigit (parameterId[1]);

    return ! isBandParameter || parameterId.substring (1).getIntValue() <= multiProcessor().getNumBands();
}

void MultiCompEditor::selectBand (int band)
{
    selectedBand = band;
    getFancyProcessor().getApvts().state.setProperty (selectedBandProperty, band, nullptr);
    graph.setSelectedBand (band);
    updateSelection();
}

void MultiCompEditor::addBand()
{
    auto& processor = multiProcessor();

    // Halves the selected band, or the widest one if that's too narrow to split.
    const auto middle = [&] (int band) { return std::sqrt (processor.bandBottomHz (band) * processor.bandTopHz (band)); };

    auto added = processor.splitBand (selectedBand, middle (selectedBand));

    for (int band = processor.getNumBands() - 1; band >= 0 && added < 0; --band)
        added = processor.splitBand (band, middle (band));

    if (added >= 0)
        selectBand (added);
}

void MultiCompEditor::removeSelectedBand()
{
    if (multiProcessor().getNumBands() <= 1)
        return;

    // The crossover belongs to the split, not the band's settings.
    const auto changed = differsFromDefault (MultiCompProcessor::id (selectedBand, ""), { "top" });

    confirmThen ("Delete band " + juce::String (selectedBand + 1) + "?", "Its settings will be lost.", "Delete", changed,
                 [this] { removeBandNow(); });
}

void MultiCompEditor::removeBandNow()
{
    auto& processor = multiProcessor();

    if (processor.getNumBands() <= 1)
        return;

    processor.removeBand (selectedBand);
    selectBand (juce::jmin (selectedBand, processor.getNumBands() - 1));
}

void MultiCompEditor::updateSelection()
{
    const auto numBands = multiProcessor().getNumBands();

    if (selectedBand >= numBands)
    {
        // A preset or a state load has fewer bands.
        selectedBand = numBands - 1;
        graph.setSelectedBand (selectedBand);
    }

    std::vector<ColourTabs::Tab> used;

    for (int band = 0; band < numBands; ++band)
        used.push_back ({ band, multiBandColour (band) });

    tabs.showAddButton = numBands < maxBands;
    tabs.setTabs (std::move (used), selectedBand);

    for (int band = 0; band < maxBands; ++band)
        bandEditors[(size_t) band]->setVisible (band == selectedBand);
}

void MultiCompEditor::onTick (float seconds)
{
    constexpr float fallDbPerSecond = 40.0f;

    auto& processor = multiProcessor();

    updateSelection();

    std::array<float, maxBands> reductions {};

    for (int band = 0; band < maxBands; ++band)
        reductions[(size_t) band] = processor.takeGainReduction (band);

    bandEditors[(size_t) selectedBand]->update (tabs.getEndX() + 10, reductions[(size_t) selectedBand], seconds);

    for (size_t band = 0; band < reductions.size(); ++band)
        displayed[band] = juce::jmax (reductions[band], displayed[band] - fallDbPerSecond * seconds);

    graph.update (displayed);
    outMeter.update (processor.outputMeter, seconds);
}

void MultiCompEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    const auto bandValue = [this] (int band, const char* suffix, float value) { setValue (parameter (MultiCompProcessor::id (band, suffix)), value); };

    constexpr float thresholds[] { -24.0f, -18.0f, -28.0f, -32.0f };

    for (int band = 0; band < 4; ++band)
        bandValue (band, "thr", thresholds[band]);

    bandValue (1, "ratio", 3.0f);
    bandValue (1, "gain", 2.0f);
    bandValue (1, "mk", 1.0f);

    setValue (parameter ("wet"), -1.5f);

    selectBand (1);

    displayed = { 2.2f, 4.1f, 1.3f, 0.6f };

    graph.showPreviewSpectrum();
    graph.update (displayed);
    bandEditors[1]->update (tabs.getEndX() + 10, 4.1f, 0.0f);
    bandEditors[1]->showLevels (-18.0f, -17.0f, 4.1f);
    outMeter.show (-6.0f, -4.2f);

    drawer.setVisible (drawerOpen);
}

juce::Colour MultiCompEditor::getAccentColour()
{
    return multiBandColour (selectedBand);
}

std::vector<ShellEditor::PopupOpener> MultiCompEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-detector", [this] { detectorDropdown.click(); } });
    openers.push_back ({ "dropdown-crossover", [this] { crossoverDropdown.click(); } });
    openers.push_back ({ "sidechain-notice", [this] { forceSidechainNotice (true); } });
    openers.push_back ({ "delete-band", [this] { confirmThen ("Delete band " + juce::String (selectedBand + 1) + "?", "Its settings will be lost.", "Delete", true, [] {}); } });
    openers.push_back ({ "tooltip-ratio", [this] { auto& knob = bandEditors[(size_t) selectedBand]->getRatioKnob(); showTip (knob, knob.getTooltip()); } });
    openers.push_back ({ "value-threshold", [this] { bandEditors[(size_t) selectedBand]->getThresholdFader().openValueEditor(); } });
    return openers;
}
