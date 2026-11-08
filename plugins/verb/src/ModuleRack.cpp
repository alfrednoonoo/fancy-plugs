#include "ModuleRack.h"

#include <fancy/plugin/ParamHelpers.h>
#include <fancy/ui/ShellEditor.h>

#include <cmath>

using namespace fancy::ui;
using verb::ModuleType;

namespace
{
// Positions from the RevBlocks frame in design/figma-spec.json, relative to each card.
constexpr int cardGap = 4;
constexpr int addPanelWidth = 203;
constexpr int gripWidth = 8, gripHeight = 22;
constexpr int stripLeft = 24, stripWidth = 687;
constexpr int chipTop = 24, chipHeight = 24, chipGap = 2;

juce::Colour accentOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return colours::uiPurple;
        case ModuleType::lpHp:      return colours::uiBlue;
        case ModuleType::normalize: return colours::accentGreen;
        default:                    return colours::uiTeal;
    }
}

juce::String valueText (const verb::Setting& setting, float value)
{
    if (juce::String (setting.unit) == "Hz")
    {
        if (value < 1000.0f)
            return juce::String (juce::roundToInt (value)) + " Hz";

        const auto khz = value / 1000.0f;
        return (std::abs (khz - std::round (khz)) < 0.005f ? juce::String (juce::roundToInt (khz)) : fancy::formatFixed (khz, 2)) + " kHz";
    }

    const auto number = fancy::formatFixed (value, setting.decimals);
    return juce::String (setting.unit).isEmpty() ? number : number + " " + setting.unit;
}

float parseValue (const verb::Setting& setting, const juce::String& text)
{
    return juce::String (setting.unit) == "Hz" ? fancy::parseHz (text) : fancy::parseNumber (text);
}

juce::NormalisableRange<float> rangeOf (const verb::Setting& setting)
{
    if (setting.logarithmic)
        return fancy::logRange (setting.low, setting.high, setting.interval);

    return { setting.low, setting.high, setting.interval, setting.skew };
}

// Figma's positions for two rows of knobs in a 356 wide card.
struct Slot
{
    int x, y, width, height;
};

constexpr Slot gridSlots[] {
    { 27, 67, 90, 123 }, { 133, 67, 90, 123 }, { 239, 67, 90, 123 },
    { 23, 214, 62, 99 }, { 104, 214, 50, 99 }, { 178, 214, 63, 99 }, { 265, 214, 68, 99 },
};

constexpr Slot wideGridSlots[] {
    { 27, 67, 90, 123 }, { 133, 67, 90, 123 }, { 239, 67, 90, 123 },
    { 47, 214, 62, 99 }, { 147, 214, 62, 99 }, { 247, 214, 62, 99 },
};
} // namespace

// ModuleCard::Link ------------------------------------------------------------

ModuleCard::Link::Link (juce::RangedAudioParameter& parameterToUse, std::function<void (float)> changed)
    : parameter (parameterToUse), onChange (std::move (changed))
{
    parameter.addListener (this);
}

ModuleCard::Link::~Link()
{
    parameter.removeListener (this);
}

void ModuleCard::Link::parameterValueChanged (int, float)
{
    onChange (parameter.convertFrom0to1 (parameter.getValue()));
}

// ModuleCard::CardButton ------------------------------------------------------

ModuleCard::CardButton::CardButton (const juce::String& text, bool isDropdown)
    : buttonText (text), textColour (colours::textPrimary), dropdown (isDropdown)
{
}

void ModuleCard::CardButton::setText (const juce::String& text, juce::Colour colour)
{
    buttonText = text;
    textColour = colour;
    repaint();
}

void ModuleCard::CardButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (isMenuOpen() ? colours::menuHover : colours::grey5);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (textColour);
    g.setFont (font (Weight::medium, 16.0f));

    if (dropdown)
    {
        g.strokePath (icons::chevrons (bounds.removeFromRight (28.0f).withSizeKeepingCentre (8.0f, 12.0f)), juce::PathStrokeType (1.0f));
        g.setColour (textColour);
        g.drawFittedText (buttonText, bounds.withTrimmedLeft (10.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }
    else if (buttonText == "<" || buttonText == ">")
    {
        const auto tip = buttonText == "<" ? -4.0f : 4.0f;
        const auto centre = bounds.getCentre();

        juce::Path chevron;
        chevron.startNewSubPath (centre.x - tip, centre.y - 7.0f);
        chevron.lineTo (centre.x + tip, centre.y);
        chevron.lineTo (centre.x - tip, centre.y + 7.0f);

        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.drawText (buttonText, bounds, juce::Justification::centred, false);
    }
}

void ModuleCard::CardButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// ModuleCard ------------------------------------------------------------------

int ModuleCard::widthOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return 296;
        case ModuleType::lpHp:      return 147;
        case ModuleType::normalize: return 163;
        case ModuleType::reverse:   return 163;
        default:                    return 356;
    }
}

ModuleCard::ModuleCard (VerbProcessor& processorToUse, ModuleRack& rack, const juce::ValueTree& module)
    : processor (processorToUse),
      owner (rack),
      tree (module),
      type (verb::typeFromStoredName (module[verb::ids::type].toString()).value_or (ModuleType::reverse)),
      uid (module[verb::ids::uid])
{
    isOn = tree.getProperty (verb::ids::on, true);

    if (type == ModuleType::file)
        addFileControls();
    else
        addSettingKnobs();

    placeKnobs();
    setSize (widthOf (type), height);
    refresh();
}

juce::Colour ModuleCard::accent() const
{
    return accentOf (type);
}

void ModuleCard::addSettingKnobs()
{
    for (const auto& setting : verb::settingsOf (type))
    {
        const auto unit = setting;

        Control control;

        control.parameter = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { setting.id, 1 }, setting.label, rangeOf (setting),
            (float) tree.getProperty (setting.id, setting.defaultValue),
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([unit] (float value, int) { return valueText (unit, value); })
                .withValueFromStringFunction ([unit] (const juce::String& text) { return parseValue (unit, text); }));

        control.link = std::make_unique<Link> (*control.parameter, [this, id = juce::Identifier (setting.id)] (float value)
        {
            processor.setModuleValue (uid, id, value);
        });

        const auto colour = accent();
        const auto& style = controls.size() >= 3 ? Knob::smallStyle : Knob::mediumStyle;
        const auto bipolar = juce::String (setting.id) == "gain";

        auto knob = std::make_unique<Knob> (*control.parameter, setting.label, colour, style, bipolar);
        addAndMakeVisible (*knob);
        control.component = std::move (knob);

        controls.push_back (std::move (control));
    }
}

void ModuleCard::addFileControls()
{
    Control control;

    control.parameter = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "mono", 1 }, "Mix to mono",
                                                                    (bool) tree.getProperty (verb::ids::mono, true));

    control.link = std::make_unique<Link> (*control.parameter, [this] (float value)
    {
        processor.setModuleValue (uid, verb::ids::mono, value >= 0.5f);
    });

    auto checkbox = std::make_unique<ParamCheckbox> (*control.parameter, "Mix to mono", colours::uiPurple);
    checkbox->setBounds (25, 225, 120, 17);
    addAndMakeVisible (*checkbox);
    control.component = std::move (checkbox);
    controls.push_back (std::move (control));

    for (juce::Component* item : std::initializer_list<juce::Component*> { &fileDropdown, &previousButton, &nextButton, &browseButton, &gainDropdown })
        addAndMakeVisible (item);

    fileDropdown.setBounds (24, 104, 248, 32);
    previousButton.setBounds (24, 152, 45, 32);
    nextButton.setBounds (73, 152, 47, 32);
    browseButton.setBounds (152, 152, 120, 32);
    gainDropdown.setBounds (24, 258, 248, 32);

    fileDropdown.onClick = [this] { showFileMenu(); };
    previousButton.onClick = [this] { stepFile (-1); };
    nextButton.onClick = [this] { stepFile (1); };
    browseButton.onClick = [this] { browse(); };
    gainDropdown.onClick = [this] { showGainMenu(); };
}

void ModuleCard::placeKnobs()
{
    const auto place = [this] (size_t index, const Slot& slot)
    {
        if (index < controls.size())
            controls[index].component->setBounds (slot.x, slot.y, slot.width, slot.height);
    };

    if (type == ModuleType::lpHp)
    {
        place (0, { 28, 63, 90, 123 });
        place (1, { 28, 202, 90, 123 });
    }
    else if (type == ModuleType::normalize)
    {
        place (0, { 36, 115, 90, 123 });
    }
    else if (type == ModuleType::echo)
    {
        for (size_t i = 0; i < controls.size(); ++i)
            place (i, gridSlots[i]);
    }
    else if (type == ModuleType::trim || type == ModuleType::reverb)
    {
        for (size_t i = 0; i < controls.size(); ++i)
            place (i, wideGridSlots[i]);
    }
}

juce::File ModuleCard::currentFile() const
{
    const auto path = tree[verb::ids::path].toString();
    return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
}

juce::String ModuleCard::breadcrumb() const
{
    const auto path = tree[verb::ids::path].toString();

    if (path.isEmpty())
        return {};

    auto parts = juce::StringArray::fromTokens (path.replaceCharacter ('\\', '/'), "/", {});
    parts.removeEmptyStrings();
    parts.remove (parts.size() - 1); // the file name is on the dropdown

    if (parts.size() > 4)
        parts = juce::StringArray { parts[0], "...", "...", parts[parts.size() - 2], parts[parts.size() - 1] };

    return "/ " + parts.joinIntoString (" / ");
}

void ModuleCard::refresh()
{
    if (type != ModuleType::file)
        return;

    const auto path = tree[verb::ids::path].toString();
    const auto name = path.fromLastOccurrenceOf ("\\", false, false).fromLastOccurrenceOf ("/", false, false);

    if (processor.isMissing (uid))
        fileDropdown.setText (juce::String::fromUTF8 ("IR missing \xe2\x80\x93 Relink..."), colours::uiRed);
    else
        fileDropdown.setText (name.isEmpty() ? juce::String ("Choose a file...") : name, colours::textPrimary);

    gainDropdown.setText (verb::gainModeName (tree.getProperty (verb::ids::gainMode, 3)), colours::textPrimary);
}

void ModuleCard::browse()
{
    const auto start = currentFile();

    chooser = std::make_unique<juce::FileChooser> ("Choose an impulse response",
                                                   start.existsAsFile() ? start : processor.getIrFolder(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& result)
                          {
                              if (result.getResult().existsAsFile())
                                  processor.loadFile (uid, result.getResult());
                          });
}

void ModuleCard::stepFile (int direction)
{
    const auto current = currentFile();

    if (! current.existsAsFile())
        return;

    const auto files = verb::audioFilesBeside (current);
    const auto index = files.indexOf (current);

    if (index < 0 || files.isEmpty())
        return;

    processor.loadFile (uid, files[(index + direction + files.size()) % files.size()]);
}

bool ModuleCard::isChanged() const
{
    const auto fresh = verb::makeModule (type, uid);

    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const auto name = tree.getPropertyName (i);

        if (name != verb::ids::uid && fresh.getProperty (name) != tree.getProperty (name))
            return true;
    }

    return false;
}

void ModuleCard::showFileMenu()
{
    const auto current = currentFile();

    if (processor.isMissing (uid) || ! current.existsAsFile())
    {
        browse();
        return;
    }

    const auto files = verb::audioFilesBeside (current);

    fancy::ui::Menu menu;
    menu.setScrollingList (true);

    for (int i = 0; i < files.size(); ++i)
        menu.addItem (i + 1, files[i].getFileName(), true, files[i] == current);

    menu.show (fileDropdown, [safeThis = juce::Component::SafePointer<ModuleCard> (this), files] (int chosen)
    {
        if (safeThis != nullptr && chosen > 0)
            safeThis->processor.loadFile (safeThis->uid, files[chosen - 1]);
    });
}

void ModuleCard::showGainMenu()
{
    const auto current = (int) tree.getProperty (verb::ids::gainMode, 3);

    fancy::ui::Menu menu;
    menu.setScrollingList (true);

    for (int mode = 0; mode < verb::numGainModes; ++mode)
        menu.addItem (mode + 1, verb::gainModeName (mode), true, mode == current);

    menu.show (gainDropdown, [safeThis = juce::Component::SafePointer<ModuleCard> (this)] (int chosen)
    {
        if (safeThis != nullptr && chosen > 0)
        {
            safeThis->processor.setModuleValue (safeThis->uid, verb::ids::gainMode, chosen - 1);
            safeThis->refresh();
        }
    });
}

juce::Rectangle<int> ModuleCard::powerArea() const { return { 12, 13, 24, 24 }; }
juce::Rectangle<int> ModuleCard::closeArea() const { return { getWidth() - 26, 3, 24, 24 }; }
juce::Rectangle<int> ModuleCard::plusArea() const { return { getWidth() - 34, getHeight() - 34, 34, 34 }; }
juce::Rectangle<int> ModuleCard::gripArea() const { return { 0, getHeight() / 2 - 20, gripWidth, gripHeight + 18 }; }

void ModuleCard::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    const auto dim = isOn ? 1.0f : 0.4f;

    g.setColour (accent().withMultipliedAlpha (dim));
    g.strokePath (icons::power (powerArea().toFloat().withSizeKeepingCentre (16.0f, 16.0f)), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::textPrimary.withMultipliedAlpha (dim));
    g.setFont (font (Weight::medium, 16.0f));
    g.drawText (verb::titleOf (type), 42, 16, getWidth() - 70, 19, juce::Justification::centredLeft, false);

    if (type == ModuleType::file)
    {
        g.setColour (juce::Colour (0xff7e7e7e));
        g.setFont (font (Weight::medium, 12.0f));
        g.drawText (breadcrumb(), 24, 71, 248, 14, juce::Justification::centredLeft, true);
    }
    else if (type == ModuleType::reverse)
    {
        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 14.0f));
        g.drawText ("Plays the IR backwards", getLocalBounds().withTrimmedLeft (12).withTrimmedRight (12), juce::Justification::centred, true);
    }

    g.setColour (colours::textSecondary);

    const auto close = closeArea().toFloat().withSizeKeepingCentre (11.0f, 11.0f);
    g.drawLine ({ close.getTopLeft(), close.getBottomRight() }, 1.5f);
    g.drawLine ({ close.getBottomLeft(), close.getTopRight() }, 1.5f);

    g.setColour (colours::textPrimary);
    g.strokePath (icons::plus (plusArea().toFloat().withSizeKeepingCentre (14.0f, 14.0f)), juce::PathStrokeType (1.5f));

    g.setColour (colours::grey6);

    const auto gripY = (float) (getHeight() - gripHeight) * 0.5f;

    for (const auto x : { 2.0f, (float) getWidth() - 3.0f })
        g.fillRect (x, gripY, 1.0f, (float) gripHeight);
}

void ModuleCard::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (gripArea().contains (event.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                              : juce::MouseCursor::NormalCursor);
}

void ModuleCard::mouseDown (const juce::MouseEvent& event)
{
    draggingCard = gripArea().contains (event.getPosition());
    dragStartX = getX();

    if (draggingCard)
        toFront (false);
}

void ModuleCard::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingCard)
        owner.cardDragged (*this, event.getDistanceFromDragStartX());
}

void ModuleCard::mouseUp (const juce::MouseEvent& event)
{
    if (draggingCard)
    {
        draggingCard = false;
        owner.cardDropped (*this, event.getDistanceFromDragStartX());
        return;
    }

    const auto position = event.getPosition();

    if (powerArea().contains (position))
    {
        isOn = ! isOn;
        repaint();
        processor.setModuleOn (uid, isOn);
    }
    else if (closeArea().contains (position))
    {
        auto* editor = findParentComponentOfClass<fancy::ui::ShellEditor>();
        auto& verbProcessor = processor;
        const auto moduleUid = uid;
        const auto remove = [&verbProcessor, moduleUid] { verbProcessor.removeModule (moduleUid); };

        if (editor != nullptr)
            editor->confirmThen ("Delete module?", "Its settings will be lost.", "Delete", isChanged(), remove);
        else
            remove();
    }
    else if (plusArea().contains (position))
    {
        const auto& cards = owner.getCards();

        for (size_t i = 0; i < cards.size(); ++i)
            if (cards[i].get() == this)
                owner.showAddPanel ((int) i + 1);
    }
}

// ModuleRack::AddButton -------------------------------------------------------

ModuleRack::AddButton::AddButton (const juce::String& text, std::function<void()> clicked)
    : buttonText (text), onClick (std::move (clicked))
{
}

int ModuleRack::AddButton::idealWidth() const
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font (Weight::medium, 16.0f), buttonText, 0.0f, 0.0f);
    return juce::roundToInt (glyphs.getBoundingBox (0, -1, true).getWidth()) + 20;
}

void ModuleRack::AddButton::paint (juce::Graphics& g)
{
    g.setColour (colours::grey5);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    g.setColour (colours::textPrimary);
    g.setFont (font (Weight::medium, 16.0f));
    g.drawText (buttonText, getLocalBounds(), juce::Justification::centred, false);
}

void ModuleRack::AddButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// ModuleRack ------------------------------------------------------------------

ModuleRack::ModuleRack (VerbProcessor& processorToUse) : processor (processorToUse)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, false, true, true);
    addAndMakeVisible (viewport);

    content.addChildComponent (addPanel);

    int y = 34;

    for (const auto type : verb::allModuleTypes)
    {
        const auto add = [this, type]
        {
            const auto position = isEmpty() ? 0 : addPosition;
            hideAddPanel();
            processor.addModule (type, position);
        };

        auto panelButton = std::make_unique<AddButton> (verb::menuNameOf (type), add);
        panelButton->setBounds (16, y, panelButton->idealWidth(), 32);
        addPanel.addAndMakeVisible (*panelButton);
        addButtons.push_back (std::move (panelButton));
        y += 42;

        auto emptyButton = std::make_unique<AddButton> (verb::menuNameOf (type), add);
        addChildComponent (*emptyButton);
        emptyButtons.push_back (std::move (emptyButton));
    }

    rebuild();
}

int ModuleRack::indexOf (const ModuleCard& card) const
{
    for (size_t i = 0; i < cards.size(); ++i)
        if (cards[i].get() == &card)
            return (int) i;

    return -1;
}

void ModuleRack::rebuild()
{
    cards.clear();

    for (const auto module : processor.getChain())
    {
        auto card = std::make_unique<ModuleCard> (processor, *this, module);
        content.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }

    addPosition = juce::jmin (addPosition, (int) cards.size());

    if (cards.empty())
        addPosition = -1;

    resized();
}

void ModuleRack::refresh()
{
    for (auto& card : cards)
        card->refresh();
}

void ModuleRack::layoutContent()
{
    auto x = 0;

    for (size_t i = 0; i <= cards.size(); ++i)
    {
        if ((int) i == addPosition)
        {
            addPanel.setBounds (x, 0, addPanelWidth, ModuleCard::height);
            x += addPanelWidth + cardGap;
        }

        if (i < cards.size())
        {
            cards[i]->setTopLeftPosition (x, 0);
            x += cards[i]->getWidth() + cardGap;
        }
    }

    addPanel.setVisible (addPosition >= 0);
    content.setSize (juce::jmax (getWidth(), x - cardGap), ModuleCard::height);
}

void ModuleRack::resized()
{
    viewport.setBounds (getLocalBounds());
    viewport.setVisible (! cards.empty());

    layoutContent();

    // Figma's empty state: two centred rows of buttons.
    constexpr int firstRow = 4, secondRow = 3, gap = 15, buttonHeight = 35, rowOneY = 96, rowTwoY = 146;

    const auto placeRow = [this] (int start, int count, int y)
    {
        auto total = gap * (count - 1);

        for (int i = 0; i < count; ++i)
            total += emptyButtons[(size_t) (start + i)]->idealWidth() + 12;

        auto x = (getWidth() - total) / 2;

        for (int i = 0; i < count; ++i)
        {
            auto& button = *emptyButtons[(size_t) (start + i)];
            const auto width = button.idealWidth() + 12;
            button.setBounds (x, y, width, buttonHeight);
            x += width + gap;
        }
    };

    placeRow (0, firstRow, rowOneY);
    placeRow (firstRow, secondRow, rowTwoY);

    for (auto& button : emptyButtons)
        button->setVisible (cards.empty());

    if (onLayoutChanged != nullptr)
        onLayoutChanged();
}

void ModuleRack::paint (juce::Graphics& g)
{
    g.fillAll (cards.empty() ? colours::bgHeader : colours::bgSection);
}

void ModuleRack::showAddPanel (int position)
{
    addPosition = position == addPosition ? -1 : position;
    resized();

    if (addPosition >= 0)
        viewport.setViewPosition (juce::jmax (0, addPanel.getRight() - getWidth()), 0);
}

void ModuleRack::hideAddPanel()
{
    addPosition = -1;
    resized();
}

void ModuleRack::cardDragged (ModuleCard& card, int deltaX)
{
    auto x = 0;

    for (const auto& other : cards)
    {
        if (other.get() == &card)
            break;

        x += other->getWidth() + cardGap;
    }

    card.setTopLeftPosition (x + deltaX, 0);
}

void ModuleRack::cardDropped (ModuleCard& card, int deltaX)
{
    const auto from = indexOf (card);
    auto centre = card.getX() + card.getWidth() / 2;

    juce::ignoreUnused (deltaX);

    auto x = 0;
    auto position = 0;

    for (size_t i = 0; i < cards.size(); ++i)
    {
        if ((int) i == from)
            continue;

        x += cards[i]->getWidth() + cardGap;

        if (centre > x - cards[i]->getWidth() / 2 - cardGap)
            ++position;
    }

    processor.moveModule (card.getUid(), position);
    layoutContent();
}

// ChainStrip ------------------------------------------------------------------

ChainStrip::ChainStrip (VerbProcessor& processorToUse, ModuleRack& rackToUse)
    : processor (processorToUse), rack (rackToUse)
{
}

float ChainStrip::scale() const
{
    return (float) stripWidth / (float) juce::jmax (1, rack.getContentWidth());
}

std::vector<ChainStrip::Chip> ChainStrip::chips() const
{
    std::vector<Chip> list;

    const auto factor = scale();

    for (const auto& card : rack.getCards())
        list.push_back ({ juce::Rectangle<float> ((float) stripLeft + (float) card->getX() * factor, (float) chipTop,
                                                  (float) card->getWidth() * factor - (float) chipGap, (float) chipHeight),
                          card->getUid() });

    return list;
}

void ChainStrip::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgHeader);

    if (rack.isEmpty())
        return;

    const auto chain = processor.getChain();

    for (const auto& chip : chips())
    {
        const auto module = chain.getChildWithProperty (verb::ids::uid, chip.uid);
        const auto type = verb::typeFromStoredName (module[verb::ids::type].toString()).value_or (ModuleType::reverse);
        const auto on = (bool) module.getProperty (verb::ids::on, true);

        g.setColour (colours::bgBody);
        g.fillRoundedRectangle (chip.bounds, 2.0f);

        g.setColour (accentOf (type).withMultipliedAlpha (on ? 1.0f : 0.4f));
        g.strokePath (icons::power (juce::Rectangle<float> (chip.bounds.getX() + 8.0f, chip.bounds.getY() + 5.0f, 14.0f, 14.0f)),
                      juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::medium, 12.0f));
        g.drawText (verb::chipOf (type), chip.bounds.withTrimmedLeft (29.0f), juce::Justification::centredLeft, true);
    }

    const auto factor = scale();
    const juce::Rectangle<float> outline ((float) stripLeft - 8.0f + (float) rack.getScroll() * factor, 16.0f,
                                          (float) rack.getWidth() * factor + 16.0f, 40.0f);

    g.setColour (colours::textSecondary);
    g.drawRoundedRectangle (outline.reduced (0.5f), 5.0f, 1.0f);
}

void ChainStrip::mouseDown (const juce::MouseEvent& event)
{
    for (const auto& chip : chips())
    {
        const juce::Rectangle<float> power (chip.bounds.getX(), chip.bounds.getY(), 26.0f, chip.bounds.getHeight());

        if (power.contains (event.position))
        {
            const auto module = processor.getChain().getChildWithProperty (verb::ids::uid, chip.uid);
            processor.setModuleOn (chip.uid, ! (bool) module.getProperty (verb::ids::on, true));
            repaint();
            return;
        }
    }

    mouseDrag (event);
}

void ChainStrip::mouseDrag (const juce::MouseEvent& event)
{
    const auto centre = (event.position.x - (float) stripLeft) / scale() - (float) rack.getWidth() * 0.5f;
    rack.setScroll (juce::roundToInt (centre));
    repaint();
}
