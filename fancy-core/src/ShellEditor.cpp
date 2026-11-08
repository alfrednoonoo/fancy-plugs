#include <fancy/ui/ShellEditor.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fancy::ui
{

namespace
{
const juce::Identifier uiScaleProperty ("uiScale");
const juce::Identifier uiWidthProperty ("uiWidth");
const juce::Identifier uiHeightProperty ("uiHeight");
constexpr int timerHz = 30;
constexpr float scales[] { 1.0f, 1.25f, 1.5f, 2.0f };
constexpr int scaleItemBase = 1000;
constexpr int aboutItem = 2000;
constexpr int saveItem = 10001, reloadItem = 10002, renameItem = 10003, deleteItem = 10004, folderItem = 10005;

const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
const juce::String changedMark (juce::CharPointer_UTF8 (" \xe2\x80\xa2"));
const juce::String apostrophe (juce::CharPointer_UTF8 ("\xe2\x80\x99"));

juce::String quoted (const juce::String& text)
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"));
}

const char* const attribution =
    "UI design based on 'ReaPlugs (vst GUI)' by Eugene Reznik, CC BY 4.0 - "
    "https://www.figma.com/community/file/967866927752067729. Modified from the original.";
} // namespace

// CaptionPanels ---------------------------------------------------------------

CaptionPanels::CaptionPanels (const juce::String& caption, int leftPanelWidth, int captionY)
    : captionText (caption), leftWidth (leftPanelWidth), captionTop (captionY)
{
}

void CaptionPanels::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (colours::bgBody);
    g.fillRect (bounds.removeFromLeft (leftWidth));
    g.setColour (colours::bgSection);
    g.fillRect (bounds);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::medium, 12.0f));
    g.drawText (captionText, leftWidth - 160, captionTop, 136, 18, juce::Justification::centredRight, false);
}

// EditLink --------------------------------------------------------------------

EditLink::EditLink (juce::Colour accent, const juce::String& text)
    : accentColour (accent), linkText (text)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EditLink::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (accentColour);
    g.fillPath (icons::gear (juce::Rectangle<float> (bounds.getX() + 8.0f, bounds.getCentreY() - 6.0f, 12.0f, 12.0f)));

    g.setFont (font (Weight::regular, 14.0f));
    g.drawText (linkText, bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
}

void EditLink::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// EditDrawer ------------------------------------------------------------------

EditDrawer::EditDrawer (const juce::String& title) : titleText (title)
{
    closeButton.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeButton);
}

void EditDrawer::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgSection);

    g.setFont (font (Weight::semiBold, 14.0f).withExtraKerningFactor (0.06f));
    g.setColour (colours::textPrimary);
    g.drawText (titleText, getLocalBounds().withHeight (48).withTrimmedLeft (24), juce::Justification::centredLeft, false);
}

void EditDrawer::resized()
{
    closeButton.setBounds (getWidth() - 78, 12, 62, 24);
}

// ShellEditor -----------------------------------------------------------------

void ShellEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgHeader);

    g.setColour (juce::Colours::black);
    g.fillRect (0.0f, (float) headerHeight - 0.5f, (float) getWidth(), 0.5f);
}

ShellEditor::ShellEditor (FancyProcessor& processor, int baseWidth, int baseHeight, HeaderStyle style)
    : AudioProcessorEditor (processor),
      fancyProcessor (processor),
      width (baseWidth),
      height (baseHeight),
      headerStyle (style),
      saveButton (style == HeaderStyle::compact ? "+" : "+ New"),
      parameterButton (style == HeaderStyle::compact ? "Prm" : "Parameter"),
      ioPill (juce::String {}),
      outputKnob (*processor.getApvts().getParameter ("output"), juce::String {}, colours::uiTeal, Knob::headerStyle),
      powerPill (*processor.getApvts().getParameter ("bypass"))
{
    setLookAndFeel (&lookAndFeel.get());

    addAndMakeVisible (root);
    root.addAndMakeVisible (body);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &presetDropdown, &saveButton, &parameterButton, &ioPill, &outputKnob, &powerPill })
        root.addAndMakeVisible (item);

    root.addChildComponent (sidechainNotice);
    root.addChildComponent (tooltips);

    presetDropdown.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { showSavePrompt(); };
    parameterButton.onClick = [this] { showParameterMenu(); };

    const auto savedScale = (float) (double) processor.getApvts().state.getProperty (uiScaleProperty, 1.0);
    applyScale (std::find (std::begin (scales), std::end (scales), savedScale) != std::end (scales) ? savedScale : 1.0f);

    timerCallback();
    startTimerHz (timerHz);
}

ShellEditor::~ShellEditor()
{
    stopTimer();
    popups.clear();
    setLookAndFeel (nullptr);
}

void ShellEditor::adoptPopup (std::unique_ptr<juce::Component> popup)
{
    root.addAndMakeVisible (*popup);
    popup->toFront (false);
    popups.push_back (std::move (popup));
}

void ShellEditor::disposePopup (juce::Component* popup)
{
    if (popup == nullptr)
        return;

    popup->setVisible (false);

    // Later, because a pop-up asks for this from inside its own callbacks.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ShellEditor> (this), popup]
    {
        if (safeThis == nullptr)
            return;

        auto& list = safeThis->popups;
        list.erase (std::remove_if (list.begin(), list.end(), [popup] (const auto& item) { return item.get() == popup; }), list.end());
    });
}

void ShellEditor::SidechainNotice::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::bgSection.withAlpha (0.92f));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (colours::accentRed);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
    g.setFont (font (Weight::medium, 12.0f));
    g.drawText (sidechainWarningText, bounds.reduced (8.0f, 0.0f), juce::Justification::centred, true);
}

juce::Rectangle<int> ShellEditor::getSidechainNoticeBounds()
{
    constexpr int noticeWidth = 380, noticeHeight = 22;
    return { (width - noticeWidth) / 2, headerHeight + 6, noticeWidth, noticeHeight };
}

void ShellEditor::forceSidechainNotice (bool shouldShow)
{
    sidechainNoticeForced = shouldShow;
    refreshHeader();
}

void ShellEditor::closePopups()
{
    if (auto* menu = MenuLayer::findOpen (root))
        menu->finish (0);

    if (auto* dialog = DialogLayer::findOpen (root))
        dialog->cancel();

    tooltips.hide();

    if (sidechainNoticeForced)
        forceSidechainNotice (false);
}

bool ShellEditor::keyPressed (const juce::KeyPress& key)
{
    // Only the plugin's own shortcuts. Every other key goes back to the host (H.2).
    if (key == juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier, 0))
    {
        showSavePrompt();
        return true;
    }

    return false;
}

std::vector<ShellEditor::PopupOpener> ShellEditor::getPopupOpeners()
{
    return {
        { "preset-menu", [this] { showPresetMenu(); } },
        { "preset-save", [this] { showSavePrompt(); } },
        { "preset-delete", [this] { showDeletePrompt ("Church choir mic"); } },
        { "parameter-menu", [this]
          {
              showParameterMenu();

              // With the UI size submenu open, to show a submenu.
              if (auto* menu = MenuLayer::findOpen (root))
              {
                  auto& panel = menu->getPanel (0);
                  const auto& items = panel.getMenu().getItems();

                  for (int row = 0; row < (int) items.size(); ++row)
                  {
                      if (items[(size_t) row].subMenu != nullptr)
                      {
                          panel.setHighlightedRow (row);
                          menu->openSubMenu (panel, row);
                      }
                  }
              }
          } },
        { "about", [this] { showAbout(); } },
    };
}

juce::RangedAudioParameter& ShellEditor::parameter (const juce::String& id)
{
    return *fancyProcessor.getApvts().getParameter (id);
}

void ShellEditor::enableResizing (int minWidth, int minHeight, int maxWidth, int maxHeight)
{
    minimumWidth = minWidth;
    minimumHeight = minHeight;
    maximumWidth = maxWidth;
    maximumHeight = maxHeight;

    const auto& state = fancyProcessor.getApvts().state;

    width = juce::jlimit (minWidth, maxWidth, (int) state.getProperty (uiWidthProperty, width));
    height = juce::jlimit (minHeight, maxHeight, (int) state.getProperty (uiHeightProperty, height));

    resizable = true;
    setResizable (true, false);
    applyScale (uiScale);
}

void ShellEditor::applyScale (float scale)
{
    const auto baseWidth = width, baseHeight = height;

    uiScale = scale;
    root.setTransform (juce::AffineTransform::scale (scale));

    if (resizable)
        setResizeLimits (juce::roundToInt ((float) minimumWidth * scale), juce::roundToInt ((float) minimumHeight * scale),
                         juce::roundToInt ((float) maximumWidth * scale), juce::roundToInt ((float) maximumHeight * scale));

    setSize (juce::roundToInt ((float) baseWidth * scale), juce::roundToInt ((float) baseHeight * scale));
}

void ShellEditor::resized()
{
    if (resizable)
    {
        width = juce::roundToInt ((float) getWidth() / uiScale);
        height = juce::roundToInt ((float) getHeight() / uiScale);

        auto& state = fancyProcessor.getApvts().state;
        state.setProperty (uiWidthProperty, width, nullptr);
        state.setProperty (uiHeightProperty, height, nullptr);
    }

    root.setBounds (0, 0, width, height);
    body.setBounds (0, headerHeight, width, height - headerHeight);
    layoutHeader();
    bodyResized();
}

void ShellEditor::layoutHeader()
{
    // Positions from the Figma Header/Small and Header/Big components.
    const auto compact = headerStyle == HeaderStyle::compact;
    constexpr int top = 13, itemHeight = 24, gap = 8;

    int x = 16;

    const auto place = [&] (juce::Component& item, int itemWidth)
    {
        item.setBounds (x, top, itemWidth, itemHeight);
        x += itemWidth + gap;
    };

    place (presetDropdown, presetWidth > 0 ? presetWidth : (compact ? 110 : 246));
    place (saveButton, compact ? 28 : 63);
    place (parameterButton, compact ? 43 : 86);
    place (ioPill, compact ? 39 : 80);

    const auto pillWidth = compact ? 48 : 56;

    powerPill.setBounds (width - 16 - pillWidth, top, pillWidth, itemHeight);
    outputKnob.setBounds (powerPill.getX() - 36, 11, 28, 28);
}

void ShellEditor::showTip (juce::Component& near, const juce::String& text)
{
    tooltips.showNow (near, text);
}

void ShellEditor::confirmThen (const juce::String& title, const juce::String& body, const juce::String& action, bool ask,
                               std::function<void()> run)
{
    if (! ask)
    {
        run();
        return;
    }

    DialogSpec spec;
    spec.title = title;
    spec.body = body;
    spec.action = action;
    spec.destructive = true;

    showDialog (root, std::move (spec), [run = std::move (run)] (bool accepted, const juce::StringArray&)
    {
        if (accepted)
            run();
    });
}

bool ShellEditor::differsFromDefault (const juce::String& prefix, const juce::StringArray& ignoredSuffixes,
                                      const std::vector<std::pair<juce::String, float>>& baselines)
{
    for (auto* item : fancyProcessor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (item);

        if (ranged == nullptr)
            continue;

        const auto id = ranged->getParameterID();

        if (! id.startsWith (prefix) || ! juce::CharacterFunctions::isLetter (id[prefix.length()]))
            continue;

        const auto suffix = id.substring (prefix.length());

        if (ignoredSuffixes.contains (suffix))
            continue;

        auto expected = ranged->getDefaultValue();

        for (const auto& [name, plain] : baselines)
            if (name == suffix)
                expected = ranged->convertTo0to1 (plain);

        if (std::abs (ranged->getValue() - expected) > 1.0e-5f)
            return true;
    }

    return false;
}

void ShellEditor::setPresetDropdownWidth (int newWidth)
{
    presetWidth = newWidth;
    layoutHeader();
}

void ShellEditor::timerCallback()
{
    refreshHeader();
    onTick (1.0f / (float) timerHz);
}

void ShellEditor::refreshHeader()
{
    const auto bypassed = fancyProcessor.getBypassParameter()->getValue() >= 0.5f;
    body.setAlpha (bypassed ? 0.5f : 1.0f);

    const auto noticeWanted = sidechainNoticeForced || fancyProcessor.sidechainWatch.isMissing();

    if (noticeWanted != sidechainNotice.isVisible())
    {
        sidechainNotice.setBounds (getSidechainNoticeBounds());
        sidechainNotice.setVisible (noticeWanted);
        sidechainNotice.toFront (false);
    }

    const auto& presets = fancyProcessor.getPresets();
    presetDropdown.setText (presets.getCurrentName() + (presets.isModified() ? changedMark : juce::String()));

    const auto inputs = juce::String (getShownInputs());
    const auto outputs = juce::String (fancyProcessor.getMainBusNumOutputChannels());

    ioPill.setText (headerStyle == HeaderStyle::compact ? inputs + "/" + outputs
                                                        : inputs + " in " + outputs + " out");
}

void ShellEditor::showPresetMenu()
{
    auto& presets = fancyProcessor.getPresets();
    const auto factoryNames = presets.getFactoryNames();
    const auto userNames = presets.getUserNames();
    const auto current = presets.getCurrentName();
    const auto userLoaded = presets.isUserPresetLoaded();

    Menu menu;
    menu.addSection ("Factory");

    for (int i = 0; i < factoryNames.size(); ++i)
        menu.addItem (1 + i, factoryNames[i], true, ! userLoaded && factoryNames[i] == current);

    if (! userNames.isEmpty())
    {
        menu.addSection ("User");

        for (int i = 0; i < userNames.size(); ++i)
            menu.addItem (1 + factoryNames.size() + i, userNames[i], true, userLoaded && userNames[i] == current);
    }

    menu.addSeparator();
    menu.addItem (saveItem, "Save as new preset" + ellipsis, true, false, "Ctrl+S");

    if (presets.isModified())
        menu.addItem (reloadItem, "Reload " + quoted (current), true, false, "discard changes");

    menu.addItem (renameItem, "Rename" + ellipsis, userLoaded);
    menu.addItem (deleteItem, "Delete" + ellipsis, userLoaded);
    menu.addItem (folderItem, "Open preset folder");

    menu.show (presetDropdown, [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        auto& self = *safeThis;
        auto& manager = self.fancyProcessor.getPresets();

        switch (result)
        {
            case saveItem: self.showSavePrompt(); break;
            case reloadItem: manager.reload(); break;
            case renameItem: self.showRenamePrompt(); break;
            case deleteItem: self.showDeletePrompt (manager.getCurrentName()); break;
            case folderItem: manager.openUserDirectory(); break;
            default: manager.load (result - 1); break;
        }

        self.refreshHeader();
    });
}

namespace
{
std::function<juce::String (const juce::StringArray&)> nameCheck (PresetManager& presets, const juce::String& allowed)
{
    return [&presets, allowed] (const juce::StringArray& values)
    {
        const auto name = juce::File::createLegalFileName (values[0]).trim();

        if (name.isEmpty())
            return juce::String ("Type a name for the preset.");

        if (name != allowed && presets.nameExists (name))
            return "There" + apostrophe + "s already a preset called " + quoted (name) + ".";

        return juce::String();
    };
}
} // namespace

void ShellEditor::showSavePrompt()
{
    auto& presets = fancyProcessor.getPresets();

    DialogSpec spec;
    spec.title = "Save as new preset";
    spec.action = "Save";
    spec.fields.push_back ({ "Name", presets.isUserPresetLoaded() ? presets.getCurrentName() : juce::String() });
    spec.validate = nameCheck (presets, {});

    showDialog (root, std::move (spec), [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (bool accepted, const juce::StringArray& values)
    {
        if (safeThis != nullptr && accepted)
        {
            safeThis->fancyProcessor.getPresets().saveUser (values[0]);
            safeThis->refreshHeader();
        }
    });
}

void ShellEditor::showRenamePrompt()
{
    auto& presets = fancyProcessor.getPresets();

    if (! presets.isUserPresetLoaded())
        return;

    const auto current = presets.getCurrentName();

    DialogSpec spec;
    spec.title = "Rename preset";
    spec.action = "Rename";
    spec.fields.push_back ({ "Name", current });
    spec.validate = nameCheck (presets, current);

    showDialog (root, std::move (spec), [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (bool accepted, const juce::StringArray& values)
    {
        if (safeThis != nullptr && accepted)
        {
            safeThis->fancyProcessor.getPresets().renameUser (values[0]);
            safeThis->refreshHeader();
        }
    });
}

void ShellEditor::showDeletePrompt (const juce::String& presetName)
{
    DialogSpec spec;
    spec.title = "Delete user preset?";
    spec.body = quoted (presetName) + " will be removed from your preset folder. This can" + apostrophe + "t be undone.";
    spec.action = "Delete";
    spec.destructive = true;

    showDialog (root, std::move (spec), [safeThis = juce::Component::SafePointer<ShellEditor> (this), presetName] (bool accepted, const juce::StringArray&)
    {
        if (safeThis == nullptr || ! accepted)
            return;

        auto& presets = safeThis->fancyProcessor.getPresets();

        if (presets.isUserPresetLoaded() && presets.getCurrentName() == presetName)
            presets.deleteUser();

        safeThis->refreshHeader();
    });
}

void ShellEditor::showParameterMenu()
{
    Menu menu;

    const auto& parameters = fancyProcessor.getParameters();
    const auto hasHost = getHostContext() != nullptr;

    for (int i = 0; i < parameters.size(); ++i)
    {
        const auto* item = parameters[i];

        if (listsParameter (*item))
            menu.addItem (i + 1, item->getName (64), hasHost, false, item->getCurrentValueAsText());
    }

    menu.addSeparator();

    Menu sizes;

    for (int i = 0; i < (int) std::size (scales); ++i)
        sizes.addItem (scaleItemBase + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true, scales[i] == uiScale);

    menu.addSubMenu ("UI size", std::move (sizes));
    menu.addItem (aboutItem, "About");

    menu.show (parameterButton, [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& self = *safeThis;

        if (result == aboutItem)
        {
            self.showAbout();
        }
        else if (result >= scaleItemBase)
        {
            const auto scale = scales[result - scaleItemBase];
            self.fancyProcessor.getApvts().state.setProperty (uiScaleProperty, scale, nullptr);
            self.applyScale (scale);
        }
        else if (auto* context = self.getHostContext())
        {
            // The host's own menu for the parameter: automation, MIDI learn and so on.
            if (const auto hostMenu = context->getContextMenuForParameter (self.fancyProcessor.getParameters()[result - 1]))
                hostMenu->showNativeMenu (self.getMouseXYRelative());
        }
    });
}

void ShellEditor::showAbout()
{
    DialogSpec spec;
    spec.title = fancyProcessor.getName();
    spec.body = attribution;
    spec.action = "Close";
    spec.showCancel = false;

    showDialog (root, std::move (spec));
}

} // namespace fancy::ui
