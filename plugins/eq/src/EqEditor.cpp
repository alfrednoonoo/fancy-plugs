#include "EqEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// Figma ReaEQ frames without the host title bar.
constexpr int baseWidth = 802;
constexpr int baseHeight = 625;
// 724 wide at the least: 696 until Phase 3 Q2.4, whose MATCH link needs 28 px of the header.
constexpr int minWidth = 724, minHeight = 464, maxWidth = 1800, maxHeight = 1300;

constexpr int gainColumnWidth = 68;
constexpr int detailsHeight = 178;
constexpr int optionsHeight = 50;
constexpr int cardWidth = 437, cardHeight = 122, cardBottomGap = 24;
constexpr int dynamicCardWidth = 720;

constexpr int tabWidth = 42, tabHeight = 23;

const juce::Identifier selectedBandProperty ("selectedBand");

// Match's Smoothing choices, in octaves.
constexpr double smoothingOctaves[] { 1.0 / 12.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 3.0, 1.0 / 2.0, 1.0 };

int smoothingIndex (double octaves)
{
    auto closest = 0;

    for (int i = 1; i < (int) std::size (smoothingOctaves); ++i)
        if (std::abs (smoothingOctaves[i] - octaves) < std::abs (smoothingOctaves[closest] - octaves))
            closest = i;

    return closest;
}

juce::String ellipsis()
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
}

juce::String secondsText (double seconds)
{
    return juce::String (seconds, 1) + " s";
}

void setPlain (juce::RangedAudioParameter& parameter, float value)
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

EqEditor::BandEditor::ChannelChip::ChannelChip (juce::RangedAudioParameter& channel, juce::Colour accentColour)
    : ParamClickControl (channel), accent (accentColour)
{
}

void EqEditor::BandEditor::ChannelChip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto index = juce::jlimit (0, 4, juce::roundToInt (param.convertFrom0to1 (param.getValue())));

    g.setColour (isMenuOpen() ? colours::menuHover : juce::Colour (0xff282828));
    g.fillRoundedRectangle (bounds, 4.0f);

    static const char* const names[] { "ST", "L", "R", "M", "S" };

    g.setColour (index == 0 ? juce::Colour (0xffededed) : accent);
    g.setFont (font (Weight::semiBold, 12.0f));
    g.drawText (names[index], bounds.withTrimmedLeft (4.0f).withTrimmedRight (14.0f), juce::Justification::centred, false);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.strokePath (icons::chevrons (juce::Rectangle<float> (5.0f, 8.0f).withCentre ({ bounds.getRight() - 9.0f, bounds.getCentreY() })),
                  juce::PathStrokeType (1.0f));
}

void EqEditor::BandEditor::ChannelChip::clicked()
{
    const auto names = param.getAllValueStrings();
    const auto current = juce::roundToInt (param.convertFrom0to1 (param.getValue()));

    fancy::ui::Menu menu;

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (1 + i, names[i], true, i == current);

    menu.show (*this, [safeThis = juce::Component::SafePointer<ChannelChip> (this)] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        auto& parameter = safeThis->param;
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.convertTo0to1 ((float) (result - 1)));
        parameter.endChangeGesture();
    });
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

EqEditor::BandEditor::BandEditor (EqProcessor& processor, int bandIndex, EqEditor& ownerToUse)
    : eqProcessor (processor),
      owner (ownerToUse),
      band (bandIndex),
      typeDropdown (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "type")),
                    *processor.getApvts().getParameter (EqProcessor::id (bandIndex, "slope"))),
      channelChip (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "chan")), bandColour (bandIndex)),
      frequencyKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "freq")), "Freq", bandColour (bandIndex), Knob::mediumStyle),
      gainKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "gain")), "Gain", bandColour (bandIndex), Knob::mediumStyle, true),
      qKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "q")), "Q", bandColour (bandIndex), Knob::mediumStyle),
      powerIcon (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "on")), bandColour (bandIndex)),
      plusButton (IconButton::Glyph::plus),
      closeButton (IconButton::Glyph::cross),
      dynamicChip (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "dyn")), "DYN", bandColour (bandIndex)),
      thresholdKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "thresh")), "Threshold", bandColour (bandIndex), Knob::tightStyle),
      rangeKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "range")), "Range", bandColour (bandIndex), Knob::tightStyle, true),
      attackKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "attack")), "Attack", bandColour (bandIndex), Knob::tightStyle),
      releaseKnob (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "release")), "Release", bandColour (bandIndex), Knob::tightStyle),
      modeSwitch (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "dynMode")), juce::Colour (0xff434343), juce::Colours::white),
      sidechainCheckbox (*processor.getApvts().getParameter (EqProcessor::id (bandIndex, "dynSc")), "Sidechain", bandColour (bandIndex))
{
    for (juce::Component* item : std::initializer_list<juce::Component*> { &typeDropdown, &channelChip, &frequencyKnob, &gainKnob, &qKnob, &powerIcon, &plusButton, &closeButton })
        addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &dynamicChip, &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob,
                                                                           &modeSwitch, &sidechainCheckbox })
        addChildComponent (item);

    dynamicChip.setTooltip ("Dynamic: the band's gain follows the level in its own region, up to Range.");
    channelChip.setTooltip ("Channel: Stereo, Left, Right, Mid or Side. Mid and Side work on the sum and the difference.");
    modeSwitch.setTextHeight (12.0f);
    sidechainCheckbox.setLabelHeight (12.0f);
    sidechainCheckbox.setTooltip ("The band listens to the sidechain input instead of the main input.");

    plusButton.onClick = [this] { owner.addBand(); };
    closeButton.onClick = [this] { owner.removeSelectedBand(); };
}

void EqEditor::BandEditor::layoutFor (bool compact)
{
    isCompact = compact;

    // The dynamic layout uses the compact card's short type names in both modes.
    typeDropdown.compact = compact || dynamicShown;

    const auto& style = compact ? Knob::compactStyle : Knob::mediumStyle;

    for (auto* knob : { &frequencyKnob, &gainKnob, &qKnob })
        knob->setStyle (dynamicShown ? Knob::tightStyle : style);

    resized();
    repaint();
}

// The dynamic half, at the Compact-mode card's positions moved by (dx, dy): panel 3 of the
// Phase 2.1 mockup. The normal Details panel uses the same positions, below the band tabs.
void EqEditor::BandEditor::layoutDynamic (int dx, int dy, int buttonsX, int buttonsDy)
{
    typeDropdown.setBounds (dx + 12, dy + 24, 94, 32);
    channelChip.setBounds (dx + 12, dy + 64, 40, 24);
    dynamicChip.setBounds (dx + 56, dy + 64, 50, 24);
    frequencyKnob.setBounds (dx + 112, dy + 12, 62, 99);
    gainKnob.setBounds (dx + 174, dy + 12, 90, 99);
    qKnob.setBounds (dx + 264, dy + 12, 56, 99);

    separatorX = dx + 326;
    separatorTop = dy + 20;

    thresholdKnob.setBounds (dx + 334, dy + 12, 62, 99);
    rangeKnob.setBounds (dx + 396, dy + 12, 62, 99);
    attackKnob.setBounds (dx + 458, dy + 12, 56, 99);
    releaseKnob.setBounds (dx + 514, dy + 12, 56, 99);
    modeSwitch.setBounds (dx + 578, dy + 32, 88, 22);
    sidechainCheckbox.setBounds (dx + 578, dy + 64, 96, 20);

    powerIcon.setBounds (buttonsX, buttonsDy + 17, isCompact ? 14 : 18, isCompact ? 15 : 19);
    plusButton.setBounds (buttonsX, buttonsDy + 56, isCompact ? 14 : 18, isCompact ? 14 : 18);
    closeButton.setBounds (buttonsX + 1, buttonsDy + 94, isCompact ? 12 : 14, isCompact ? 12 : 14);
}

void EqEditor::BandEditor::paint (juce::Graphics& g)
{
    if (isCompact)
    {
        g.setColour (juce::Colour (0xff2e2e2e).withAlpha (0.9f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    }

    if (dynamicShown && separatorX >= 0)
    {
        g.setColour (juce::Colour (0xff434343));
        g.fillRect ((float) separatorX, (float) separatorTop, 1.0f, 82.0f);
    }
}

void EqEditor::BandEditor::resized()
{
    for (juce::Component* item : std::initializer_list<juce::Component*> { &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob, &modeSwitch, &sidechainCheckbox })
        item->setVisible (dynamicShown);

    dynamicChip.setVisible (chipShown);
    separatorX = -1;

    if (dynamicShown)
    {
        if (isCompact)
            layoutDynamic (0, 0, getWidth() - 31, 0);
        else
            layoutDynamic (10 + (getWidth() - 734) / 2, 42, getWidth() - 43, 5);

        return;
    }

    if (isCompact)
    {
        // The Channel chip is always there, so the type dropdown always sits above its row.
        typeDropdown.setBounds (16, 28, 94, 32);
        channelChip.setBounds (16, 68, 40, 24);
        dynamicChip.setBounds (60, 68, 50, 24);

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
    channelChip.setBounds (26, 120, 44, 24);
    dynamicChip.setBounds (74, 120, 94, 24);
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

    // Phase 2.1 D: the chip shows for the types that can be dynamic, and the dynamic half
    // while it's on. In Linear phase both stay visible but dimmed: dynamic bands run static.
    const auto supports = eq::supportsDynamics (spec.type);
    const auto dynamicOn = eqProcessor.readDynamics (band).on;
    const auto linear = eqProcessor.getApvts().getParameter ("phase")->getValue() >= 0.5f;
    const auto wantChip = supports;
    const auto wantDynamic = supports && dynamicOn;

    if (wantChip != chipShown || wantDynamic != dynamicShown)
    {
        chipShown = wantChip;
        dynamicShown = wantDynamic;
        owner.layoutEditors();
    }

    if (linear != linearPhase)
    {
        linearPhase = linear;
        dynamicChip.setEnabled (! linear);
        dynamicChip.setTooltip (linear ? linearTooltip : "Dynamic: the band's gain follows the level in its own region, up to Range.");

        for (juce::Component* item : std::initializer_list<juce::Component*> { &dynamicChip, &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob,
                                                                               &modeSwitch, &sidechainCheckbox })
            item->setAlpha (linear ? 0.4f : 1.0f);
    }

    // A mono bus has one channel, so every band acts on it.
    const auto stereoBus = eqProcessor.getMainBusNumInputChannels() >= 2;
    channelChip.setEnabled (stereoBus);
    channelChip.setAlpha (stereoBus ? 1.0f : 0.4f);
    channelChip.setTooltip (stereoBus ? "Channel: Stereo, Left, Right, Mid or Side. Mid and Side work on the sum and the difference."
                                      : "Channel needs a stereo bus");
    channelChip.repaint();

    gainKnob.setValueSuffix ("(" + noteName (spec.frequencyHz) + ")");
    gainKnob.setAlpha (usesGain ? 1.0f : 0.4f);
    gainKnob.setInterceptsMouseClicks (usesGain, usesGain);
    typeDropdown.repaint();
}

// MatchLink -------------------------------------------------------------------

EqEditor::MatchLink::MatchLink()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EqEditor::MatchLink::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = juce::Point<float> (bounds.getX() + 14.0f, bounds.getCentreY());

    g.setColour (lit ? juce::Colours::white : colours::uiTeal);

    // A target: a ring with a dot in it.
    g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (centre), 1.3f);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));

    // The icon alone, where there's no room for the word.
    if (getWidth() >= 60)
    {
        g.setFont (font (Weight::regular, 14.0f));
        g.drawText ("MATCH", bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
    }
}

void EqEditor::MatchLink::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// MatchCard -------------------------------------------------------------------

EqEditor::MatchCard::Link::Link (juce::RangedAudioParameter& parameterToUse, std::function<void (float)> changed)
    : parameter (parameterToUse), onChange (std::move (changed))
{
    parameter.addListener (this);
}

EqEditor::MatchCard::Link::~Link()
{
    parameter.removeListener (this);
}

void EqEditor::MatchCard::Link::parameterValueChanged (int, float)
{
    onChange (parameter.convertFrom0to1 (parameter.getValue()));
}

EqEditor::MatchCard::MatchCard (EqProcessor& processor)
    : eqProcessor (processor),
      engine (processor.match),
      sourceParameter (juce::ParameterID { "matchSource", 1 }, "Source", juce::StringArray { "Sidechain", "Captured" }, 0),
      smoothingParameter (juce::ParameterID { "matchSmoothing", 1 }, "Smoothing",
                          juce::StringArray { "1/12 oct", "1/6 oct", "1/4 oct", "1/3 oct", "1/2 oct", "1 oct" }, 3),
      amountParameter (fancy::percentParameter ("matchAmount", "Amount", 100.0f)),
      bandsParameter (fancy::floatParameter (
          "matchBands", "Bands", juce::NormalisableRange<float> (1.0f, (float) eq::maxBands, 1.0f), 8.0f,
          juce::AudioParameterFloatAttributes {}
              .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)); })
              .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }))),
      lowParameter (fancy::floatParameter (
          "matchLow", "Low", fancy::logRange (20.0f, 1000.0f, 1.0f), 30.0f,
          juce::AudioParameterFloatAttributes {}
              .withLabel ("Hz")
              .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
              .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }))),
      highParameter (fancy::floatParameter (
          "matchHigh", "High", fancy::logRange (1000.0f, 20000.0f, 10.0f), 18000.0f,
          juce::AudioParameterFloatAttributes {}
              .withLabel ("Hz")
              .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
              .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }))),
      sourceSwitch (sourceParameter, juce::Colour (0xff434343), juce::Colours::white),
      smoothingKnob (smoothingParameter, "Smoothing", colours::uiTeal, Knob::tightStyle),
      amountKnob (*amountParameter, "Amount", colours::uiTeal, Knob::tightStyle),
      bandsKnob (*bandsParameter, "Bands", colours::uiTeal, Knob::tightStyle),
      lowKnob (*lowParameter, "Low", colours::uiTeal, Knob::tightStyle),
      highKnob (*highParameter, "High", colours::uiTeal, Knob::tightStyle),
      learnButton ("Learn"),
      learnCurrentButton ("Learn current"),
      clearButton ("Clear"),
      matchButton ("Match"),
      closeButton (IconButton::Glyph::cross)
{
    using Engine = eq::match::Engine;

    for (juce::Component* item : std::initializer_list<juce::Component*> { &sourceSwitch, &smoothingKnob, &amountKnob, &bandsKnob, &lowKnob, &highKnob,
                                                                           &learnButton, &clearButton, &matchButton, &closeButton })
        addAndMakeVisible (item);

    addChildComponent (learnCurrentButton);

    sourceSwitch.setTextHeight (12.0f);
    sourceSwitch.setTooltip ("Sidechain: the reference plays into the sidechain while the song plays. Captured: learn the reference, then the song.");
    smoothingKnob.setTooltip ("How much detail of the difference is kept.");
    amountKnob.setTooltip ("How far towards the reference the bands go.");
    bandsKnob.setTooltip ("The most bands Match may use, of the free ones.");
    lowKnob.setTooltip ("Nothing is matched below this.");
    highKnob.setTooltip ("Nothing is matched above this.");

    // Each control writes to the engine, which keeps the settings with the project.
    links.push_back (std::make_unique<Link> (sourceParameter, [this] (float value)
    {
        const auto source = value >= 0.5f ? Engine::Source::captured : Engine::Source::sidechain;

        if (source != engine.source)
        {
            engine.source = source;
            engine.setLearning (Engine::Learning::none);
        }
    }));

    links.push_back (std::make_unique<Link> (smoothingParameter, [this] (float value)
    {
        engine.settings.smoothingOctaves = smoothingOctaves[juce::jlimit (0, (int) std::size (smoothingOctaves) - 1, juce::roundToInt (value))];
    }));

    links.push_back (std::make_unique<Link> (*amountParameter, [this] (float value) { engine.settings.amount = (double) value * 0.01; }));
    links.push_back (std::make_unique<Link> (*bandsParameter, [this] (float value) { engine.settings.maxBands = juce::roundToInt (value); }));
    links.push_back (std::make_unique<Link> (*lowParameter, [this] (float value) { engine.settings.lowHz = (double) value; }));
    links.push_back (std::make_unique<Link> (*highParameter, [this] (float value) { engine.settings.highHz = (double) value; }));

    learnButton.onClick = [this]
    {
        toggle (engine.source == Engine::Source::captured ? Engine::Learning::reference : Engine::Learning::both);
    };

    learnCurrentButton.onClick = [this] { toggle (Engine::Learning::current); };

    clearButton.onClick = [this]
    {
        engine.setLearning (Engine::Learning::none);
        engine.clear();
    };

    matchButton.onClick = [this] { startFit(); };

    closeButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };

    readSettings();
}

EqEditor::MatchCard::~MatchCard()
{
    links.clear();
}

void EqEditor::MatchCard::readSettings()
{
    const auto settings = engine.settings;
    const auto source = engine.source;

    setPlain (sourceParameter, source == eq::match::Engine::Source::captured ? 1.0f : 0.0f);
    setPlain (smoothingParameter, (float) smoothingIndex (settings.smoothingOctaves));
    setPlain (*amountParameter, (float) (settings.amount * 100.0));
    setPlain (*bandsParameter, (float) settings.maxBands);
    setPlain (*lowParameter, (float) settings.lowHz);
    setPlain (*highParameter, (float) settings.highHz);
}

void EqEditor::MatchCard::toggle (eq::match::Engine::Learning mode)
{
    engine.setLearning (engine.getLearning() == mode ? eq::match::Engine::Learning::none : mode);
}

void EqEditor::MatchCard::startFit()
{
    if (! engine.isReady() || engine.isFitting())
        return;

    // The spectra hold still while the fit runs.
    engine.setLearning (eq::match::Engine::Learning::none);
    engine.requestFit (eqProcessor.getMatchTarget(), eqProcessor.getMatchFitSettings(), eqProcessor.getMatchRate());
}

void EqEditor::MatchCard::update()
{
    using Engine = eq::match::Engine;

    // A fit that has finished becomes bands, as one undo step.
    std::vector<eq::BandSpec> fitted;

    if (engine.takeFit (fitted))
        eqProcessor.applyMatch (fitted);

    const auto learning = engine.getLearning();
    const auto captured = engine.source == Engine::Source::captured;
    const auto busy = "Learning" + ellipsis();

    learnButton.setText (captured ? (learning == Engine::Learning::reference ? busy : juce::String ("Learn reference"))
                                  : (learning == Engine::Learning::both ? busy : juce::String ("Learn")));
    learnCurrentButton.setText (learning == Engine::Learning::current ? busy : juce::String ("Learn current"));

    if (captured != learnCurrentButton.isVisible())
    {
        learnCurrentButton.setVisible (captured);
        resized();
    }

    const auto outline = learning == Engine::Learning::none ? juce::Rectangle<int>()
                       : (learning == Engine::Learning::current ? learnCurrentButton.getBounds() : learnButton.getBounds());

    const auto ready = engine.isReady();
    const auto fitting = engine.isFitting();

    matchButton.setAlpha (ready && ! fitting ? 1.0f : 0.4f);

    const auto status = "ref " + secondsText (engine.getReferenceSeconds()) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 current "))
                      + secondsText (engine.getCurrentSeconds());

    const auto free = eqProcessor.getNumFreeForMatch();
    const auto caption = fitting ? "Fitting" + ellipsis()
                       : (! ready ? juce::String ("Learn 3 s of each")
                                  : juce::String (free) + (free == 1 ? " band free" : " bands free"));

    if (status != statusText || caption != captionText || outline != learningOutline)
    {
        statusText = status;
        captionText = caption;
        learningOutline = outline;
        repaint();
    }
}

void EqEditor::MatchCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::bgSection.withAlpha (0.96f));
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.setColour (colours::menuHint);
    g.drawText ("MATCH", 14, 12, 52, 22, juce::Justification::centredLeft, false);

    g.setFont (font (Weight::regular, 12.0f));
    g.drawText (statusText, 224, 12, getWidth() - 224 - 36, 22, juce::Justification::centredRight, false);
    g.drawText (captionText, matchButton.getX() - 14, matchButton.getBottom() + 6, matchButton.getWidth() + 28, 14, juce::Justification::centred, false);
}

void EqEditor::MatchCard::paintOverChildren (juce::Graphics& g)
{
    // The button that's learning has the reference's purple round it.
    if (learningOutline.isEmpty())
        return;

    const auto area = learningOutline.toFloat().reduced (0.5f);

    g.setColour (colours::accentPurple);
    g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.2f);
}

void EqEditor::MatchCard::resized()
{
    sourceSwitch.setBounds (68, 12, 150, 22);
    closeButton.setBounds (getWidth() - 26, 17, 12, 12);

    // Sidechain has one Learn button; Captured has one for each spectrum.
    const auto captured = learnCurrentButton.isVisible();

    learnButton.setBounds (12, captured ? 46 : 60, 112, 24);
    learnCurrentButton.setBounds (12, 76, 112, 24);
    clearButton.setBounds (12, captured ? 106 : 92, 112, 24);

    auto x = 132;

    for (auto* knob : { &smoothingKnob, &amountKnob, &bandsKnob, &lowKnob, &highKnob })
    {
        knob->setBounds (x, 42, 62, 99);
        x += 62;
    }

    matchButton.setBounds (452, 66, 84, 24);
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
      matchCard (processor),
      phaseDropdown (parameter ("phase")),
      firDropdown (parameter ("firLength")),
      oversamplingDropdown (parameter ("oversampling")),
      rangeDropdown (parameter ("range")),
      analyzerDropdown (parameter ("analyzer")),
      fftDropdown (parameter ("anaFft")),
      windowDropdown (parameter ("anaWindow")),
      overlapDropdown (parameter ("anaOverlap")),
      analyzerChannelDropdown (parameter ("anaChan")),
      tiltCheckbox (parameter ("anaTilt"), "Pink tilt"),
      peakCheckbox (parameter ("anaPeak"), "Peak hold"),
      smoothingKnob (parameter ("anaSmooth"), "Smoothing", colours::uiTeal, Knob::smallStyle)
{
    // The title strip keeps one colour while the accent follows the selection.
    setTitleColour (bandColour (0));

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
    editLink.onClick = [this]
    {
        // The drawer and the Match card share the space over the graph.
        if (! drawer.isVisible())
            setMatchOpen (false);

        drawer.setVisible (! drawer.isVisible());
    };

    // Phase 3 Q2.4: MATCH, left of EDIT, opens the Match card over the graph.
    getRoot().addAndMakeVisible (matchLink);
    matchLink.setTooltip ("Match: learn a reference's tone and fit bands to it.");
    matchLink.onClick = [this] { setMatchOpen (! matchOpen); };

    body.addChildComponent (matchCard);
    matchCard.onClose = [this] { setMatchOpen (false); };

    for (auto* dropdown : { &phaseDropdown, &firDropdown, &oversamplingDropdown })
        addLatencyNote (*dropdown);

    const std::pair<juce::Label*, const char*> labels[] {
        { &phaseLabel, "Phase" }, { &firLabel, "FIR length" }, { &oversamplingLabel, "Oversampling" }, { &slopeLabel, "Slope" },
        { &rangeLabel, "Graph range" }, { &analyzerLabel, "Analyzer" }, { &fftLabel, "Size" }, { &windowLabel, "Window" }, { &overlapLabel, "Overlap" },
        { &analyzerChannelLabel, "Channel" }
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
    analyzerChannelLabel.setBounds (labelX2, row (4), 66, rowHeight);
    analyzerChannelDropdown.setBounds (controlX2, row (4), 140, rowHeight);

    rangeLabel.setBounds (labelX3, row (0), 86, rowHeight);
    rangeDropdown.setBounds (controlX3, row (0), 110, rowHeight);
    tiltCheckbox.setBounds (labelX3, row (1) + 2, 90, 20);
    peakCheckbox.setBounds (labelX3, row (2) + 2, 90, 20);
    smoothingKnob.setBounds (610, row (1), 90, 99);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &phaseDropdown, &firDropdown, &oversamplingDropdown, &analyzerDropdown, &fftDropdown, &windowDropdown, &overlapDropdown,
             &analyzerChannelDropdown,
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

EqEditor::~EqEditor()
{
    // The Match engine's worker only runs while the card is open.
    eqProcessor().match.setPanelOpen (false);
}

void EqEditor::setMatchOpen (bool shouldBeOpen)
{
    if (shouldBeOpen == matchOpen)
        return;

    matchOpen = shouldBeOpen;
    eqProcessor().match.setPanelOpen (matchOpen);

    if (matchOpen)
    {
        drawer.setVisible (false);
        matchCard.readSettings();
        matchCard.update();
    }

    matchCard.setVisible (matchOpen);
    matchLink.lit = matchOpen;
    matchLink.repaint();

    layoutEditors();
    updateSelection();
    updateMatchOverlay();
}

void EqEditor::updateMatchOverlay()
{
    EqGraph::MatchOverlay overlay;
    overlay.visible = matchOpen;

    if (matchOpen)
    {
        auto& processor = eqProcessor();
        auto& engine = processor.match;
        const auto smoothing = engine.settings.smoothingOctaves;

        overlay.hasReference = engine.getReferenceSeconds() > 0.0;
        overlay.hasCurrent = engine.getCurrentSeconds() > 0.0;
        overlay.hasTarget = overlay.hasReference && overlay.hasCurrent;

        // Drawn as Match sees them, at its Smoothing.
        if (overlay.hasReference)
            overlay.reference = eq::match::smooth (engine.getReferenceDb(), smoothing);

        if (overlay.hasCurrent)
            overlay.current = eq::match::smooth (engine.getCurrentDb(), smoothing);

        if (overlay.hasTarget)
            overlay.target = processor.getMatchTarget();
    }

    graph.setMatchOverlay (overlay);
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
        // In Compact mode the Match card takes the band card's place.
        bandEditors[(size_t) band]->setVisible (anyBand && band == selectedBand && ! (compactMode && matchOpen));
        slopeDropdowns[(size_t) band]->setVisible (band == selectedBand);
    }
}

void EqEditor::bodyResized()
{
    layoutEditors();

    // The handles and curves follow the new size at once, not on the next tick.
    if (! bandEditors.empty())
        graph.update();
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
        // The card widens while the band shows its dynamic half (Phase 2.1 D).
        const auto editorWidth = editor->showsDynamics() ? juce::jmin (dynamicCardWidth, contentWidth - 8) : cardWidth;

        if (compactMode)
            editor->setBounds ((contentWidth - editorWidth) / 2, optionsTop - cardBottomGap - cardHeight, editorWidth, cardHeight);
        else
            editor->setBounds (0, detailsTop, contentWidth, detailsHeight);

        editor->layoutFor (compactMode);
        editor->toFront (false);
    }

    // The Match card floats over the bottom of the graph, as the Compact-mode card does.
    matchCard.setBounds ((contentWidth - MatchCard::cardWidth) / 2, juce::jmax (4, graphHeight - cardBottomGap - MatchCard::cardHeight),
                         MatchCard::cardWidth, MatchCard::cardHeight);
    matchCard.toFront (false);

    drawer.setBounds (0, height - detailsHeight - optionsHeight, contentWidth, detailsHeight + optionsHeight);
    drawer.toFront (false);

    const auto knobRight = width - 16 - 56 - 36;
    const auto linkLeft = knobRight - 72;
    editLink.setBounds (linkLeft, 13, 64, 24);

    // In a narrow window the preset pill gives up room, so Compare clears the links (Phase 3
    // Q2.1: every size renders without overlaps). MATCH sits against EDIT, whose own left
    // margin is the gap, and is just its icon when the word doesn't fit.
    constexpr int presetWidest = 170, presetNarrowest = 110;
    constexpr int matchWide = 74, matchNarrow = 24;

    setPresetDropdownWidth (presetWidest);

    auto matchWidth = matchWide;

    if (getHeaderFreeLeft() - (linkLeft - matchWidth) > presetWidest - presetNarrowest)
        matchWidth = matchNarrow;

    const auto matchLeft = linkLeft - matchWidth;
    matchLink.setBounds (matchLeft, 13, matchWidth, 24);

    if (const auto overlap = getHeaderFreeLeft() - matchLeft; overlap > 0)
        setPresetDropdownWidth (juce::jmax (presetNarrowest, presetWidest - overlap));

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

    if (matchOpen)
    {
        matchCard.update();

        // The curves follow the learning at 10 Hz.
        if (++matchTick % 3 == 0)
            updateMatchOverlay();
    }

    // A mono bus has nothing but the one channel to show.
    const auto stereoBus = eqProcessor().getMainBusNumInputChannels() >= 2;
    analyzerChannelDropdown.setEnabled (stereoBus);
    analyzerChannelDropdown.setAlpha (stereoBus ? 1.0f : 0.4f);

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

void EqEditor::showDynamicPreview (bool compact)
{
    applyPreviewValues();

    auto& processor = eqProcessor();
    processor.setParameter ("b2freq", 3200.0f);
    processor.setParameter ("b2gain", 0.0f);
    processor.setParameter ("b2q", 2.0f);
    processor.setParameter ("b2dyn", 1.0f);
    processor.setParameter ("b2thresh", -24.0f);
    processor.setParameter ("b2range", -6.0f);

    graph.showPreviewChange (1, -3.6f);

    parameter ("compact").setValueNotifyingHost (compact ? 1.0f : 0.0f);
    compactMode = compact;
    drawer.setVisible (false);
    selectBand (1);
    bandEditors[(size_t) selectedBand]->update();
    layoutEditors();
    graph.update();
}

void EqEditor::showChannelPreview()
{
    applyPreviewValues();

    setMatchOpen (false);

    auto& processor = eqProcessor();
    processor.setParameter ("b2dyn", 0.0f);
    graph.showPreviewChange (-1, 0.0f);
    processor.setParameter ("b3chan", 3.0f);
    processor.setParameter ("b3gain", -4.0f);
    processor.setParameter ("b3freq", 3200.0f);
    processor.setParameter ("b3q", 1.2f);

    const auto added = processor.addBand (6000.0f, 5.0f);

    if (added >= 0)
        processor.setParameter (EqProcessor::id (added, "chan"), 4.0f);

    parameter ("compact").setValueNotifyingHost (0.0f);
    compactMode = false;
    drawer.setVisible (false);
    selectBand (2);
    layoutEditors();
    bandEditors[(size_t) selectedBand]->update();
    graph.update();
}

void EqEditor::showMatchPreview()
{
    applyPreviewValues();

    auto& processor = eqProcessor();
    processor.setParameter ("b2dyn", 0.0f);
    graph.showPreviewChange (-1, 0.0f);

    // The mockup's spectra: a falling line, and a reference with more at 100 Hz and 10 kHz and
    // less at 2.8 kHz.
    eq::match::Curve reference {}, current {};

    for (int i = 0; i < eq::match::numPoints; ++i)
    {
        const auto decade = std::log10 (eq::match::hzAt (i));
        const auto base = -52.0 - 9.0 * (decade - 2.0) - (decade < 1.7 ? (1.7 - decade) * 25.0 : 0.0);
        const auto difference = 3.0 * std::exp (-(decade - 2.0) * (decade - 2.0) / 0.05)
                              - 2.5 * std::exp (-(decade - 3.45) * (decade - 3.45) / 0.03)
                              + 2.0 * std::exp (-(decade - 4.0) * (decade - 4.0) / 0.04);

        current[(size_t) i] = base + 0.8 * std::sin (0.9 * i) * std::cos (0.37 * i);
        reference[(size_t) i] = base + difference + 0.8 * std::sin (1.3 * i) * std::cos (0.23 * i);
    }

    processor.match.restoreSpectra (reference, current, 24.6);

    parameter ("compact").setValueNotifyingHost (0.0f);
    compactMode = false;
    drawer.setVisible (false);
    selectBand (1);
    setMatchOpen (true);
    layoutEditors();
    bandEditors[(size_t) selectedBand]->update();
    matchCard.update();
    updateMatchOverlay();
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
