#include "ModuleRack.h"
#include "VerbLibrary.h"

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
    if (juce::String (setting.unit) == "Hz" && setting.decimals == 0)
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
        case ModuleType::modulate:  return 247;
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

        // Phase 3 Q2.8: the Reverb generator's Early dropdown and True stereo checkbox.
        if (setting.kind != verb::Setting::Kind::knob)
        {
            const auto current = (float) tree.getProperty (setting.id, setting.defaultValue);

            if (setting.kind == verb::Setting::Kind::choice)
                control.parameter = std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { setting.id, 1 }, setting.label,
                                                                                  verb::earlyTypeNames(), juce::roundToInt (current));
            else
                control.parameter = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { setting.id, 1 }, setting.label, current >= 0.5f);

            control.link = std::make_unique<Link> (*control.parameter, [this, id = juce::Identifier (setting.id)] (float value)
            {
                processor.setModuleValue (uid, id, value);
                updateDimming();
            });

            if (setting.kind == verb::Setting::Kind::choice)
                control.component = std::make_unique<fancy::ui::ParamDropdown> (*control.parameter);
            else
                control.component = std::make_unique<ParamCheckbox> (*control.parameter, setting.label, accent());

            addAndMakeVisible (*control.component);
            controls.push_back (std::move (control));
            continue;
        }

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
        const auto& style = type == ModuleType::reverb || type == ModuleType::modulate ? Knob::tightStyle
                          : controls.size() >= 3       ? Knob::smallStyle
                                                       : Knob::mediumStyle;
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
    else if (type == ModuleType::reverb)
    {
        // Phase 3 Q2.8 (reference/phase3/24__verb_q28_mockup.png): two rows of five small knobs,
        // then the Early dropdown and the True stereo checkbox.
        // Nine knobs: Length to Stereo spread, then Low RT, High RT, Size and Build-up.
        for (size_t i = 0; i < controls.size() && i < 9; ++i)
            place (i, { 12 + 66 * (int) (i % 5), i < 5 ? 56 : 158, 66, 99 });

        place (9, { 80, 268, 110, 28 });   // Early
        place (10, { 206, 272, 140, 20 }); // True stereo
        updateDimming();
    }
    else if (type == ModuleType::modulate)
    {
        // Phase 3 Q2.8: one row of four small knobs (reference/phase3/24__verb_q28_mockup.png).
        for (size_t i = 0; i < controls.size(); ++i)
            place (i, { 1 + 61 * (int) i, 110, 62, 99 });
    }
    else if (type == ModuleType::trim)
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
    const auto factory = tree[verb::ids::factory].toString();

    if (factory.isNotEmpty())
        return "/ Library / " + factory.upToFirstOccurrenceOf ("/", false, false);

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
    const auto factory = tree[verb::ids::factory].toString();
    const auto name = factory.isNotEmpty() ? factory.fromFirstOccurrenceOf ("/", false, false)
                                           : path.fromLastOccurrenceOf ("\\", false, false).fromLastOccurrenceOf ("/", false, false);

    // A pair shows both files, left input first (Phase 3 Q2.8).
    const auto pairPath = tree[verb::ids::pairPath].toString();
    const auto pairName = pairPath.fromLastOccurrenceOf ("\\", false, false).fromLastOccurrenceOf ("/", false, false);

    if (processor.isMissing (uid))
        fileDropdown.setText (juce::String::fromUTF8 ("IR missing \xe2\x80\x93 Relink..."), colours::uiRed);
    else if (name.isNotEmpty() && pairName.isNotEmpty() && isTrueStereo())
        fileDropdown.setText (name + " + " + pairName, colours::textPrimary);
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

void ModuleCard::browsePair()
{
    const auto start = currentFile();

    chooser = std::make_unique<juce::FileChooser> ("Choose the right input's impulse response",
                                                   start.existsAsFile() ? start : processor.getIrFolder(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& result)
                          {
                              if (result.getResult().existsAsFile())
                                  processor.loadPair (uid, result.getResult());
                          });
}

void ModuleCard::stepFile (int direction)
{
    // A library IR steps through its category.
    if (const auto factory = tree[verb::ids::factory].toString(); factory.isNotEmpty())
    {
        const auto category = factory.upToFirstOccurrenceOf ("/", false, false);
        juce::StringArray keys;

        for (const auto& entry : verb::Library::builtIn().getEntries())
            if (entry.category == category)
                keys.add (entry.key());

        if (const auto index = keys.indexOf (factory); index >= 0)
            processor.loadLibrary (uid, keys[(index + direction + keys.size()) % keys.size()]);

        return;
    }

    const auto current = currentFile();

    if (! current.existsAsFile())
        return;

    const auto files = verb::audioFilesBeside (current);
    const auto index = files.indexOf (current);

    if (index < 0 || files.isEmpty())
        return;

    processor.loadFile (uid, files[(index + direction + files.size()) % files.size()]);
}

void ModuleCard::updateDimming()
{
    // Size only matters to Room and Hall's early reflections.
    if (type == ModuleType::reverb && controls.size() > 9)
    {
        const auto early = juce::roundToInt ((float) tree.getProperty ("early", 0.0f));
        controls[7].component->setAlpha (early == 1 || early == 2 ? 1.0f : 0.4f);
    }
}

bool ModuleCard::isTrueStereo() const
{
    return type == ModuleType::file && (bool) tree.getProperty (verb::ids::trueStereo, false);
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
    const auto factory = tree[verb::ids::factory].toString();

    if (processor.isMissing (uid) && factory.isEmpty())
    {
        browse();
        return;
    }

    // Phase 3 Q2.8: the library first, by category, then the IR folder.
    constexpr int libraryId = 200000;
    const auto& library = verb::Library::builtIn().getEntries();

    fancy::ui::Menu menu;

    if (! library.empty())
    {
        menu.addSection ("Library");

        for (const auto& category : verb::Library::categories())
        {
            fancy::ui::Menu sub;
            auto holdsCurrent = false;

            for (size_t i = 0; i < library.size(); ++i)
            {
                if (library[i].category != category)
                    continue;

                const auto ticked = library[i].key() == factory;
                holdsCurrent = holdsCurrent || ticked;
                sub.addItem (libraryId + (int) i, library[i].name, true, ticked, library[i].isTrueStereo() ? "TS" : juce::String());
            }

            if (! sub.isEmpty())
                menu.addSubMenu (category, std::move (sub), true, holdsCurrent);
        }
    }

    const auto files = current.existsAsFile() ? verb::audioFilesBeside (current)
                     : processor.getIrFolder().isDirectory() ? verb::audioFilesBeside (processor.getIrFolder().getChildFile ("_"))
                                                             : juce::Array<juce::File>();

    if (! files.isEmpty())
    {
        menu.addSection ("IR folder");

        for (int i = 0; i < files.size(); ++i)
            menu.addItem (i + 1, files[i].getFileName(), true, files[i] == current);
    }

    if (menu.isEmpty())
    {
        browse();
        return;
    }

    // Phase 3 Q2.8: true stereo from two stereo files, one per input.
    constexpr int pairId = 100000, removePairId = 100001;
    const auto paired = tree[verb::ids::pairPath].toString().isNotEmpty() || tree[verb::ids::pairFlac].toString().isNotEmpty();

    if (current.existsAsFile())
    {
        menu.addSeparator();
        menu.addItem (pairId, paired ? "Change right input..." : "Pair right input...");

        if (paired)
            menu.addItem (removePairId, "Remove pair");
    }

    menu.show (fileDropdown, [safeThis = juce::Component::SafePointer<ModuleCard> (this), files] (int chosen)
    {
        if (safeThis == nullptr || chosen <= 0)
            return;

        if (chosen >= libraryId)
        {
            const auto& entries = verb::Library::builtIn().getEntries();

            if ((size_t) (chosen - libraryId) < entries.size())
                safeThis->processor.loadLibrary (safeThis->uid, entries[(size_t) (chosen - libraryId)].key());
        }
        else if (chosen == pairId)
            safeThis->browsePair();
        else if (chosen == removePairId)
            safeThis->processor.clearPair (safeThis->uid);
        else if (chosen <= files.size())
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

    if (isTrueStereo())
    {
        // The TS mark after the title (reference/phase3/24__verb_q28_mockup.png).
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (font (Weight::medium, 16.0f), verb::titleOf (type));
        const juce::Rectangle<float> mark (42.0f + titleWidth + 8.0f, 18.0f, 24.0f, 16.0f);

        g.setColour (accent().withMultipliedAlpha (dim));
        g.fillRoundedRectangle (mark, 3.0f);
        g.setColour (colours::bgWell);
        g.setFont (font (Weight::semiBold, 10.0f));
        g.drawText ("TS", mark, juce::Justification::centred, false);
    }

    if (type == ModuleType::file)
    {
        g.setColour (juce::Colour (0xff7e7e7e));
        g.setFont (font (Weight::medium, 12.0f));
        g.drawText (breadcrumb(), 24, 71, 248, 14, juce::Justification::centredLeft, true);
    }
    else if (type == ModuleType::modulate)
    {
        g.setColour (colours::textSecondary.withMultipliedAlpha (dim));
        g.setFont (font (Weight::regular, 11.0f));
        g.drawFittedText ("Moves the tail after Start between two versions with the same decay. The early part and the pitch don't change.",
                          24, 236, getWidth() - 48, 60, juce::Justification::topLeft, 4);
    }
    else if (type == ModuleType::reverb)
    {
        g.setColour (colours::textPrimary.withMultipliedAlpha (dim));
        g.setFont (font (Weight::regular, 14.0f));
        g.drawText ("Early", 24, 268, 50, 28, juce::Justification::centredLeft, false);
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
}

// ModuleRack::AddButton -------------------------------------------------------

ModuleRack::AddButton::AddButton (const juce::String& text, std::function<void()> clicked, float textHeight, bool outlined)
    : buttonText (text), onClick (std::move (clicked)), height (textHeight), outline (outlined)
{
}

int ModuleRack::AddButton::idealWidth() const
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font (Weight::medium, height), buttonText, 0.0f, 0.0f);
    return juce::roundToInt (glyphs.getBoundingBox (0, -1, true).getWidth()) + juce::roundToInt (height * 1.25f);
}

void ModuleRack::AddButton::paint (juce::Graphics& g)
{
    if (outline)
    {
        g.setColour (colours::grey6);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
    }
    else
    {
        g.setColour (colours::grey5);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    }

    g.setColour (colours::textPrimary);
    g.setFont (font (Weight::medium, height));
    g.drawText (buttonText, getLocalBounds(), juce::Justification::centred, false);
}

// ModuleRack::AddBox ----------------------------------------------------------

void ModuleRack::AddBox::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    juce::Path outline;
    outline.addRoundedRectangle (bounds, 6.0f);

    juce::Path dashed;
    const float dashes[] { 4.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (colours::grey6);
    g.fillPath (dashed);

    const auto centre = bounds.getCentre().translated (0.0f, -14.0f);
    const auto circle = juce::Rectangle<float> (40.0f, 40.0f).withCentre (centre);

    g.setColour (colours::bgBody);
    g.fillEllipse (circle);
    g.setColour (colours::textPrimary);
    g.strokePath (icons::plus (circle.withSizeKeepingCentre (16.0f, 16.0f)), juce::PathStrokeType (2.0f));

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 14.0f));
    g.drawText ("Add module", juce::Rectangle<float> (bounds.getX(), centre.y + 26.0f, bounds.getWidth(), 20.0f), juce::Justification::centred, false);
}

void ModuleRack::AddBox::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

void ModuleRack::AddButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// ModuleRack ------------------------------------------------------------------

ModuleRack::ModuleRack (VerbProcessor& processorToUse)
    : processor (processorToUse),
      addBox ([this] { showAddPanel ((int) cards.size()); })
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, false, true, true);

    // A clicked viewport would take the keys, and the host's space bar with them (H.2).
    viewport.setWantsKeyboardFocus (false);
    addAndMakeVisible (viewport);

    content.addChildComponent (addPanel);
    content.addChildComponent (addBox);
    content.addMouseListener (this, false);

    // Phase 3 Q2.8: eight types at 34 px spacing, then Cancel, inside the 352 tall panel.
    int y = 16;

    for (const auto type : verb::allModuleTypes)
    {
        const auto add = [this, type]
        {
            const auto position = isEmpty() ? 0 : addPosition;
            hideAddPanel();
            processor.addModule (type, position);
        };

        auto panelButton = std::make_unique<AddButton> (verb::menuNameOf (type), add, 14.0f);
        panelButton->setBounds (16, y, panelButton->idealWidth(), 28);
        addPanel.addAndMakeVisible (*panelButton);
        addButtons.push_back (std::move (panelButton));
        y += 34;

        // An empty rack has nothing to modulate.
        if (type == verb::ModuleType::modulate)
            continue;

        auto emptyButton = std::make_unique<AddButton> (verb::menuNameOf (type), add);
        addChildComponent (*emptyButton);
        emptyButtons.push_back (std::move (emptyButton));
    }

    cancelButton = std::make_unique<AddButton> ("Cancel", [this] { hideAddPanel(); }, 14.0f, true);
    cancelButton->setBounds (16, 304, addPanelWidth - 32, 32);
    addPanel.addAndMakeVisible (*cancelButton);

    rebuild();
}

void ModuleRack::mouseDown (const juce::MouseEvent& event)
{
    if (event.eventComponent == &content && isAddPanelOpen())
        hideAddPanel();
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

    // One Modulate per chain: its button is greyed while the chain has one.
    const auto hasModulate = processor.getChain().getChildWithProperty (verb::ids::type, verb::storedName (verb::ModuleType::modulate)).isValid();

    for (size_t i = 0; i < addButtons.size() && i < verb::allModuleTypes.size(); ++i)
    {
        const auto disabled = verb::allModuleTypes[i] == verb::ModuleType::modulate && hasModulate;
        addButtons[i]->setEnabled (! disabled);
        addButtons[i]->setAlpha (disabled ? 0.4f : 1.0f);
    }

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

    // The Add module box follows the last card while the panel is closed (Phase 3 Q2.8).
    const auto showBox = addPosition < 0 && ! cards.empty();
    addBox.setVisible (showBox);

    if (showBox)
    {
        addBox.setBounds (x, 0, addPanelWidth, ModuleCard::height);
        x += addPanelWidth + cardGap;
    }

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
