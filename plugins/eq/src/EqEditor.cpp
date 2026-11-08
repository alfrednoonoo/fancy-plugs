#include "EqEditor.h"

#include <cmath>

using namespace fancy::ui;

namespace
{
// Figma ReaEQ frames without the host title bar.
constexpr int baseWidth = 802;
constexpr int baseHeight = 625;
constexpr int minWidth = 696, minHeight = 464, maxWidth = 1800, maxHeight = 1300;

constexpr int gainColumnWidth = 68;
constexpr int detailsHeight = 178;
constexpr int optionsHeight = 50;
constexpr int cardWidth = 437, cardHeight = 122, cardBottomGap = 24;

constexpr int tabWidth = 42, tabHeight = 23;

const juce::Identifier selectedBandProperty ("selectedBand");

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, 14.0f));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
}

// The nearest note, with middle C as C4. 487.7 Hz reads B4.
juce::String noteName (double hz)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const auto midi = juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0));
    const auto octave = midi >= 0 ? midi / 12 - 1 : -2;

    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (octave);
}

juce::Path typeIcon (eq::BandType type, juce::Rectangle<float> area)
{
    const auto x = area.getX(), w = area.getWidth();
    const auto top = area.getY(), bottom = area.getBottom(), middle = area.getCentreY();

    const auto at = [&] (float position) { return x + w * position; };

    juce::Path path;

    switch (type)
    {
        case eq::BandType::lowShelf:
            path.startNewSubPath (at (0.0f), top);
            path.lineTo (at (0.3f), top);
            path.cubicTo (at (0.5f), top, at (0.5f), middle, at (0.7f), middle);
            path.lineTo (at (1.0f), middle);
            break;

        case eq::BandType::highShelf:
            path.startNewSubPath (at (0.0f), middle);
            path.lineTo (at (0.3f), middle);
            path.cubicTo (at (0.5f), middle, at (0.5f), top, at (0.7f), top);
            path.lineTo (at (1.0f), top);
            break;

        case eq::BandType::tilt:
            path.startNewSubPath (at (0.0f), bottom);
            path.lineTo (at (1.0f), top);
            break;

        case eq::BandType::notch:
            path.startNewSubPath (at (0.0f), top);
            path.lineTo (at (0.35f), top);
            path.cubicTo (at (0.45f), top, at (0.47f), bottom, at (0.5f), bottom);
            path.cubicTo (at (0.53f), bottom, at (0.55f), top, at (0.65f), top);
            path.lineTo (at (1.0f), top);
            break;

        case eq::BandType::lowPass:
            path.startNewSubPath (at (0.0f), middle);
            path.lineTo (at (0.45f), middle);
            path.cubicTo (at (0.65f), middle, at (0.7f), bottom, at (0.9f), bottom);
            break;

        case eq::BandType::highPass:
            path.startNewSubPath (at (0.1f), bottom);
            path.cubicTo (at (0.3f), bottom, at (0.35f), middle, at (0.55f), middle);
            path.lineTo (at (1.0f), middle);
            break;

        case eq::BandType::allPass:
            path.startNewSubPath (at (0.0f), middle);
            path.lineTo (at (1.0f), middle);
            path.addEllipse (at (0.5f) - 3.0f, middle - 3.0f, 6.0f, 6.0f);
            break;

        case eq::BandType::band:
        case eq::BandType::bandPass:
        default:
            path.startNewSubPath (at (0.0f), bottom);
            path.cubicTo (at (0.3f), bottom, at (0.3f), top, at (0.5f), top);
            path.cubicTo (at (0.7f), top, at (0.7f), bottom, at (1.0f), bottom);
            break;
    }

    return path;
}
} // namespace

// Panels ----------------------------------------------------------------------

void EqEditor::Panels::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto contentWidth = getWidth() - gainColumnWidth;

    g.setColour (colours::bgSection);
    g.fillRect (bounds.withLeft (contentWidth));

    g.setColour (colours::bgHeader);
    g.fillRect (0, getHeight() - optionsHeight, contentWidth, optionsHeight);

    if (! compact)
    {
        g.setColour (colours::bgBody);
        g.fillRect (0, getHeight() - optionsHeight - detailsHeight, contentWidth, detailsHeight);
    }
}

// TabStrip --------------------------------------------------------------------

juce::Rectangle<int> EqEditor::TabStrip::tabBounds (int index) const
{
    const auto count = (int) bands.size();
    const auto width = juce::jmin (tabWidth, getWidth() / juce::jmax (1, count) - 1);
    const auto left = (getWidth() - count * (width + 1) + 1) / 2;

    return { left + index * (width + 1), 0, width, tabHeight };
}

void EqEditor::TabStrip::paint (juce::Graphics& g)
{
    g.setFont (font (Weight::regular, 14.0f));

    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const auto area = tabBounds (i);

        g.setColour (bands[(size_t) i] == selected ? bandColour (bands[(size_t) i]) : colours::grey5);
        g.fillRect (area);

        g.setColour (colours::textPrimary);
        g.drawText (juce::String (bands[(size_t) i] + 1), area, juce::Justification::centred, false);
    }
}

void EqEditor::TabStrip::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < (int) bands.size(); ++i)
        if (tabBounds (i).contains (event.getPosition()) && onSelect != nullptr)
            onSelect (bands[(size_t) i]);
}

// BandEditor parts ------------------------------------------------------------

EqEditor::BandEditor::TypeDropdown::TypeDropdown (juce::RangedAudioParameter& type, juce::RangedAudioParameter& slope)
    : ParamClickControl (type), slopeParameter (slope)
{
}

void EqEditor::BandEditor::TypeDropdown::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    if (! compact || isMenuOpen())
    {
        g.setColour (isMenuOpen() ? colours::menuHover : colours::grey5);
        g.fillRoundedRectangle (bounds, compact ? 3.0f : 5.0f);
    }

    const auto type = (eq::BandType) juce::roundToInt (param.convertFrom0to1 (param.getValue()));
    const auto icon = juce::Rectangle<float> (bounds.getX() + 11.0f, bounds.getCentreY() - 4.5f, 18.0f, 9.0f);

    g.setColour (colours::textPrimary);
    g.strokePath (typeIcon (type, icon), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    static const char* const shortNames[] { "LS", "HS", "Bnd", "Nch", "LP", "HP", "BP", "AP", "Tlt" };

    const auto text = compact ? juce::String (shortNames[juce::jlimit (0, 8, (int) type)]) : getValueText();

    g.setFont (compact ? font (Weight::regular, 14.0f) : font (Weight::medium, 16.0f));
    g.drawText (text, bounds.withTrimmedLeft (38.0f).withTrimmedRight (18.0f), juce::Justification::centredLeft, true);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.strokePath (icons::chevrons (juce::Rectangle<float> (7.0f, 10.0f).withCentre ({ bounds.getRight() - 11.5f, bounds.getCentreY() })),
                  juce::PathStrokeType (1.0f));
}

void EqEditor::BandEditor::TypeDropdown::clicked()
{
    // The mockup's order: the gain types, then the filters, with a slope submenu on Low pass
    // and High pass. Ids: 1 + type, then 100 + slope for Low pass and 200 + slope for High pass.
    const auto types = param.getAllValueStrings();
    const auto slopes = slopeParameter.getAllValueStrings();
    const auto current = (int) juce::roundToInt (param.convertFrom0to1 (param.getValue()));
    const auto currentSlope = (int) juce::roundToInt (slopeParameter.convertFrom0to1 (slopeParameter.getValue()));

    const auto icon = [] (eq::BandType type)
    {
        return [type] (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink)
        {
            g.setColour (ink);
            g.strokePath (typeIcon (type, area), juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
    };

    fancy::ui::Menu menu;

    const auto addType = [&] (eq::BandType type)
    {
        const auto index = (int) type;

        // Shelves and Tilt keep their slope; it's in the band's right-click menu and EDIT.
        if (type != eq::BandType::lowPass && type != eq::BandType::highPass)
        {
            menu.addItem (1 + index, types[index], true, index == current).withIcon (icon (type));
            return;
        }

        fancy::ui::Menu slopeMenu;
        const auto base = type == eq::BandType::lowPass ? 100 : 200;

        for (int i = 0; i < slopes.size(); ++i)
            slopeMenu.addItem (base + i, slopes[i], true, index == current && i == currentSlope);

        menu.addSubMenu (types[index], std::move (slopeMenu), true, index == current).withIcon (icon (type));
    };

    for (const auto type : { eq::BandType::lowShelf, eq::BandType::band, eq::BandType::highShelf, eq::BandType::notch })
        addType (type);

    menu.addSeparator();

    for (const auto type : { eq::BandType::lowPass, eq::BandType::highPass, eq::BandType::bandPass, eq::BandType::allPass, eq::BandType::tilt })
        addType (type);

    menu.show (*this, [safeThis = juce::Component::SafePointer<TypeDropdown> (this)] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        auto& self = *safeThis;

        const auto set = [] (juce::RangedAudioParameter& parameter, float plain)
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost (parameter.convertTo0to1 (plain));
            parameter.endChangeGesture();
        };

        if (result >= 100)
        {
            set (self.param, (float) (int) (result >= 200 ? eq::BandType::highPass : eq::BandType::lowPass));
            set (self.slopeParameter, (float) (result % 100));
        }
        else
        {
            set (self.param, (float) (result - 1));
        }
    });
}

// BandEditor ------------------------------------------------------------------

EqEditor::BandEditor::BandEditor (EqProcessor& processor, int bandIndex, EqEditor& owner)
    : eqProcessor (processor),
      band (bandIndex),
      typeDropdown (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "type")),
                    *processor.getApvts().getParameter (EqProcessor::id (bandIndex, "slope"))),
      frequencyKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "freq")), "Freq", bandColour (bandIndex), Knob::mediumStyle),
      gainKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "gain")), "Gain", bandColour (bandIndex), Knob::mediumStyle, true),
      qKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "q")), "Q", bandColour (bandIndex), Knob::mediumStyle),
      powerIcon (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "on")), bandColour (bandIndex)),
      plusButton (IconButton::Glyph::plus),
      closeButton (IconButton::Glyph::cross)
{
    for (juce::Component* item : std::initializer_list<juce::Component*> { &typeDropdown, &frequencyKnob, &gainKnob, &qKnob, &powerIcon, &plusButton, &closeButton })
        addAndMakeVisible (item);

    plusButton.onClick = [&owner] { owner.addBand(); };
    closeButton.onClick = [&owner] { owner.removeSelectedBand(); };
}

void EqEditor::BandEditor::layoutFor (bool compact)
{
    isCompact = compact;
    typeDropdown.compact = compact;

    const auto& style = compact ? Knob::compactStyle : Knob::mediumStyle;

    for (auto* knob : { &frequencyKnob, &gainKnob, &qKnob })
        knob->setStyle (style);

    resized();
    repaint();
}

void EqEditor::BandEditor::paint (juce::Graphics& g)
{
    if (isCompact)
    {
        g.setColour (juce::Colour (0xff2e2e2e).withAlpha (0.9f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    }
}

void EqEditor::BandEditor::resized()
{
    if (isCompact)
    {
        typeDropdown.setBounds (16, 45, 94, 32);
        frequencyKnob.setBounds (138, 25, 62, 79);
        gainKnob.setBounds (198, 25, 100, 79);
        qKnob.setBounds (292, 25, 62, 79);
        powerIcon.setBounds (406, 17, 14, 15);
        plusButton.setBounds (406, 56, 14, 14);
        closeButton.setBounds (407, 94, 12, 12);
        return;
    }

    const auto centreShift = (getWidth() - 734) / 2;
    const auto right = getWidth() - 43;

    typeDropdown.setBounds (26, 80, 142, 32);
    frequencyKnob.setBounds (232 + centreShift, 40, 90, 123);
    gainKnob.setBounds (338 + centreShift, 40, 90, 123);
    qKnob.setBounds (444 + centreShift, 40, 90, 123);
    powerIcon.setBounds (right, 22, 18, 19);
    plusButton.setBounds (right, 82, 18, 18);
    closeButton.setBounds (right + 2, 141, 14, 14);
}

void EqEditor::BandEditor::update()
{
    const auto spec = eqProcessor.readBand (band);
    const auto usesGain = eq::hasGain (spec.type);

    gainKnob.setValueSuffix ("(" + noteName (spec.frequencyHz) + ")");
    gainKnob.setAlpha (usesGain ? 1.0f : 0.4f);
    gainKnob.setInterceptsMouseClicks (usesGain, usesGain);
    typeDropdown.repaint();
}

// EqEditor --------------------------------------------------------------------

EqEditor::EqEditor (EqProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      graph (processor),
      gainFader (parameter ("gain"), colours::uiTeal),
      logCheckbox (parameter ("logFreq"), "Log-scale automated freq."),
      gridCheckbox (parameter ("showGrid"), "Show grid"),
      phaseCheckbox (parameter ("showPhase"), "Show phase"),
      compactCheckbox (parameter ("compact"), "Compact mode"),
      editLink (colours::uiTeal),
      phaseDropdown (parameter ("phase")),
      firDropdown (parameter ("firLength")),
      oversamplingDropdown (parameter ("oversampling")),
      rangeDropdown (parameter ("range")),
      analyzerDropdown (parameter ("analyzer")),
      fftDropdown (parameter ("anaFft")),
      windowDropdown (parameter ("anaWindow")),
      overlapDropdown (parameter ("anaOverlap")),
      tiltCheckbox (parameter ("anaTilt"), "Pink tilt"),
      peakCheckbox (parameter ("anaPeak"), "Peak hold"),
      smoothingKnob (parameter ("anaSmooth"), "Smoothing", colours::uiTeal, Knob::smallStyle)
{
    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (170);

    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    body.addAndMakeVisible (panels);
    body.addAndMakeVisible (graph);

    styleLabel (gainLabel, "Gain", colours::textSecondary, juce::Justification::centred);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &gainLabel, &gainFader, &logCheckbox, &gridCheckbox, &phaseCheckbox, &compactCheckbox, &tabs })
        body.addAndMakeVisible (item);

    for (int band = 0; band < eq::maxBands; ++band)
    {
        bandEditors.push_back (std::make_unique<BandEditor> (processor, band, *this));
        body.addChildComponent (*bandEditors.back());

        slopeDropdowns.push_back (std::make_unique<ParamDropdown> (parameter (EqProcessor::id (band, "slope"))));
    }

    graph.onBandSelected = [this] (int band) { selectBand (band); };
    graph.onDeleteBand = [this] (int band)
    {
        selectBand (band);
        removeSelectedBand();
    };
    graph.onTypeValue = [this] (int band, const juce::String& suffix)
    {
        selectBand (band);
        auto& editor = *bandEditors[(size_t) band];
        (suffix == "gain" ? editor.getGainKnob() : editor.getFrequencyKnob()).openValueEditor();
    };
    tabs.onSelect = [this] (int band) { selectBand (band); };

    getRoot().addAndMakeVisible (editLink);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    for (auto* dropdown : { &phaseDropdown, &firDropdown, &oversamplingDropdown })
        addLatencyNote (*dropdown);

    const std::pair<juce::Label*, const char*> labels[] {
        { &phaseLabel, "Phase" }, { &firLabel, "FIR length" }, { &oversamplingLabel, "Oversampling" }, { &slopeLabel, "Slope" },
        { &rangeLabel, "Graph range" }, { &analyzerLabel, "Analyzer" }, { &fftLabel, "Size" }, { &windowLabel, "Window" }, { &overlapLabel, "Overlap" }
    };

    for (const auto& [label, text] : labels)
    {
        styleLabel (*label, text, colours::textPrimary, juce::Justification::centredLeft);
        drawer.addAndMakeVisible (label);
    }

    constexpr int labelX1 = 24, controlX1 = 124, labelX2 = 262, controlX2 = 332, labelX3 = 500, controlX3 = 590;
    constexpr int rowHeight = 24, firstRow = 52, rowStep = 34;

    const auto row = [] (int index) { return firstRow + index * rowStep; };

    phaseLabel.setBounds (labelX1, row (0), 96, rowHeight);
    phaseDropdown.setBounds (controlX1, row (0), 110, rowHeight);
    firLabel.setBounds (labelX1, row (1), 96, rowHeight);
    firDropdown.setBounds (controlX1, row (1), 110, rowHeight);
    oversamplingLabel.setBounds (labelX1, row (2), 96, rowHeight);
    oversamplingDropdown.setBounds (controlX1, row (2), 110, rowHeight);
    slopeLabel.setBounds (labelX1, row (3), 96, rowHeight);

    analyzerLabel.setBounds (labelX2, row (0), 66, rowHeight);
    analyzerDropdown.setBounds (controlX2, row (0), 140, rowHeight);
    fftLabel.setBounds (labelX2, row (1), 66, rowHeight);
    fftDropdown.setBounds (controlX2, row (1), 140, rowHeight);
    windowLabel.setBounds (labelX2, row (2), 66, rowHeight);
    windowDropdown.setBounds (controlX2, row (2), 140, rowHeight);
    overlapLabel.setBounds (labelX2, row (3), 66, rowHeight);
    overlapDropdown.setBounds (controlX2, row (3), 140, rowHeight);

    rangeLabel.setBounds (labelX3, row (0), 86, rowHeight);
    rangeDropdown.setBounds (controlX3, row (0), 110, rowHeight);
    tiltCheckbox.setBounds (labelX3, row (1) + 2, 90, 20);
    peakCheckbox.setBounds (labelX3, row (2) + 2, 90, 20);
    smoothingKnob.setBounds (610, row (1), 90, 99);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &phaseDropdown, &firDropdown, &oversamplingDropdown, &analyzerDropdown, &fftDropdown, &windowDropdown, &overlapDropdown,
             &rangeDropdown, &tiltCheckbox, &peakCheckbox, &smoothingKnob })
        drawer.addAndMakeVisible (item);

    for (auto& dropdown : slopeDropdowns)
    {
        dropdown->setBounds (controlX1, row (3), 110, rowHeight);
        drawer.addChildComponent (*dropdown);
    }

    body.addChildComponent (drawer);

    selectedBand = juce::jlimit (0, eq::maxBands - 1, (int) processor.getApvts().state.getProperty (selectedBandProperty, 0));
    graph.setSelectedBand (selectedBand);

    compactMode = parameter ("compact").getValue() >= 0.5f;

    enableResizing (minWidth, minHeight, maxWidth, maxHeight);
    updateSelection();
    layoutEditors();
    graph.update();
}

EqProcessor& EqEditor::eqProcessor() noexcept
{
    return dynamic_cast<EqProcessor&> (getFancyProcessor());
}

bool EqEditor::listsParameter (const juce::AudioProcessorParameter& item)
{
    const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (&item);

    if (ranged == nullptr || ! item.isAutomatable())
        return false;

    const auto parameterId = ranged->getParameterID();
    const auto isBandParameter = parameterId[0] == 'b' && juce::CharacterFunctions::isDigit (parameterId[1]);

    if (! isBandParameter)
        return true;

    return ! parameterId.endsWith ("used") && eqProcessor().isBandUsed (parameterId.substring (1).getIntValue() - 1);
}

void EqEditor::selectBand (int band)
{
    selectedBand = band;
    getFancyProcessor().getApvts().state.setProperty (selectedBandProperty, band, nullptr);
    graph.setSelectedBand (band);
    updateSelection();
}

void EqEditor::addBand()
{
    auto& processor = eqProcessor();
    const auto spec = processor.readBand (selectedBand);

    // Half an octave above the selected band, so the new handle doesn't land on top of it.
    const auto added = processor.addBand ((float) juce::jlimit (20.0, 20000.0, spec.frequencyHz * 1.414), 0.0f);

    if (added >= 0)
        selectBand (added);
}

void EqEditor::removeSelectedBand()
{
    // A band added with the + button or a double-click needs no confirm: its frequency and gain
    // are set where it was added, and the rest are as a new band has them.
    const auto prefix = EqProcessor::id (selectedBand, "");
    const auto changed = differsFromDefault (prefix, { "used", "freq", "gain" }, { { "type", 2.0f } });

    confirmThen ("Delete band " + juce::String (selectedBand + 1) + "?",
                 "Its settings will be lost.", "Delete", changed, [this] { removeBandNow(); });
}

void EqEditor::removeBandNow()
{
    auto& processor = eqProcessor();

    processor.removeBand (selectedBand);

    for (int offset = 1; offset < eq::maxBands; ++offset)
    {
        for (const auto candidate : { selectedBand + offset, selectedBand - offset })
        {
            if (candidate >= 0 && candidate < eq::maxBands && processor.isBandUsed (candidate))
            {
                selectBand (candidate);
                return;
            }
        }
    }

    updateSelection();
}

void EqEditor::updateSelection()
{
    auto& processor = eqProcessor();

    std::vector<int> used;

    for (int band = 0; band < eq::maxBands; ++band)
        if (processor.isBandUsed (band))
            used.push_back (band);

    if (! used.empty() && ! processor.isBandUsed (selectedBand))
    {
        // A preset or automation removed it.
        selectedBand = used.front();
        graph.setSelectedBand (selectedBand);
    }

    tabs.bands = std::move (used);
    tabs.selected = selectedBand;
    tabs.repaint();

    const auto anyBand = ! tabs.bands.empty();

    for (int band = 0; band < eq::maxBands; ++band)
    {
        bandEditors[(size_t) band]->setVisible (anyBand && band == selectedBand);
        slopeDropdowns[(size_t) band]->setVisible (band == selectedBand);
    }
}

void EqEditor::bodyResized()
{
    layoutEditors();
}

void EqEditor::layoutEditors()
{
    if (bandEditors.empty())
        return;

    const auto width = getBaseWidth();
    const auto height = getBaseHeight() - headerHeight;
    const auto contentWidth = width - gainColumnWidth;
    const auto optionsTop = height - optionsHeight;
    const auto detailsTop = optionsTop - detailsHeight;
    const auto graphHeight = compactMode ? optionsTop : detailsTop;

    panels.compact = compactMode;
    panels.setBounds (0, 0, width, height);
    panels.repaint();

    graph.setBounds (0, 0, contentWidth, graphHeight);

    // Positions from the Figma frames; the row and the fader follow the window's edges.
    gainLabel.setBounds (contentWidth + 12, 24, 44, 17);
    gainFader.setBounds (contentWidth + 12, 49, 44, height - 73);

    const auto rowLeft = (contentWidth - 609) / 2;
    const auto rowY = optionsTop + 16;

    logCheckbox.setBounds (rowLeft, rowY, 199, 20);
    gridCheckbox.setBounds (rowLeft + 231, rowY, 88, 20);
    phaseCheckbox.setBounds (rowLeft + 351, rowY, 103, 20);
    compactCheckbox.setBounds (rowLeft + 486, rowY, 123, 20);

    tabs.setVisible (! compactMode);
    tabs.setBounds (17, detailsTop, contentWidth, tabHeight);

    for (auto& editor : bandEditors)
    {
        if (compactMode)
            editor->setBounds ((contentWidth - cardWidth) / 2, optionsTop - cardBottomGap - cardHeight, cardWidth, cardHeight);
        else
            editor->setBounds (0, detailsTop, contentWidth, detailsHeight);

        editor->layoutFor (compactMode);
        editor->toFront (false);
    }

    drawer.setBounds (0, height - detailsHeight - optionsHeight, contentWidth, detailsHeight + optionsHeight);
    drawer.toFront (false);

    const auto knobRight = width - 16 - 56 - 36;
    editLink.setBounds (knobRight - 72, 13, 64, 24);

    constexpr float ranges[] { 12.0f, 6.0f, 12.0f, 18.0f, 24.0f };

    const auto rangeIndex = juce::jlimit (0, 4, juce::roundToInt (parameter ("range").convertFrom0to1 (parameter ("range").getValue())));
    graph.setDisplayRange (rangeIndex == 0 ? (compactMode ? 18.0f : 12.0f) : ranges[rangeIndex]);
}

void EqEditor::onTick (float)
{
    const auto compactNow = parameter ("compact").getValue() >= 0.5f;
    const auto rangeNow = parameter ("range").getValue();

    if (compactNow != compactMode || rangeNow != lastRange)
    {
        compactMode = compactNow;
        lastRange = rangeNow;
        layoutEditors();
    }

    updateSelection();

    bandEditors[(size_t) selectedBand]->update();

    const auto usesSlope = eq::hasSlope (eqProcessor().readBand (selectedBand).type);
    slopeDropdowns[(size_t) selectedBand]->setAlpha (usesSlope ? 1.0f : 0.4f);

    oversamplingDropdown.setAlpha (eqProcessor().readTargets().linearPhase ? 0.4f : 1.0f);

    graph.update();
}

void EqEditor::applyPreviewValues()
{
    stopUpdates();

    auto& processor = eqProcessor();

    processor.setParameter ("b1freq", 67.0f);
    processor.setParameter ("b1gain", -7.0f);
    processor.setParameter ("b2freq", 487.7f);
    processor.setParameter ("b2gain", -5.1f);
    processor.setParameter ("b2q", 2.0f);
    processor.setParameter ("b3freq", 1600.0f);
    processor.setParameter ("b3gain", 6.5f);
    processor.setParameter ("b3q", 0.8f);
    processor.setParameter ("b4freq", 17000.0f);
    processor.setParameter ("b4gain", -13.0f);
    processor.setParameter ("gain", -7.3f);

    graph.showPreviewSpectrum();
    selectBand (1);
}

void EqEditor::showPreview (bool drawerOpen)
{
    applyPreviewValues();

    parameter ("compact").setValueNotifyingHost (0.0f);
    compactMode = false;
    layoutEditors();

    drawer.setVisible (drawerOpen);

    bandEditors[(size_t) selectedBand]->update();
    graph.update();
}

void EqEditor::showCompactPreview()
{
    applyPreviewValues();

    parameter ("compact").setValueNotifyingHost (1.0f);
    compactMode = true;
    layoutEditors();

    drawer.setVisible (false);

    bandEditors[(size_t) selectedBand]->update();
    graph.update();
}

std::vector<fancy::ui::ShellEditor::PopupOpener> EqEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();

    openers.push_back ({ "band-type", [this] { bandEditors[(size_t) selectedBand]->getTypeDropdown().click(); } });
    openers.push_back ({ "band-menu", [this]
                         {
                             const auto handle = graph.getHandlePosition (selectedBand);
                             graph.showBandMenu (selectedBand, handle);

                             if (auto* menu = fancy::ui::MenuLayer::findOpen (getRoot()))
                             {
                                 auto& panel = menu->getPanel (0);
                                 const auto& items = panel.getMenu().getItems();

                                 for (int row = 0; row < (int) items.size(); ++row)
                                 {
                                     if (items[(size_t) row].text == "Type")
                                     {
                                         panel.setHighlightedRow (row);
                                         menu->openSubMenu (panel, row);
                                     }
                                 }
                             }
                         } });
    openers.push_back ({ "delete-band", [this] { confirmThen ("Delete band " + juce::String (selectedBand + 1) + "?", "Its settings will be lost.", "Delete", true, [] {}); } });
    openers.push_back ({ "dropdown-phase", [this] { drawer.setVisible (true); phaseDropdown.click(); } });
    openers.push_back ({ "value-freq", [this] { bandEditors[(size_t) selectedBand]->getFrequencyKnob().openValueEditor(); } });

    return openers;
}
