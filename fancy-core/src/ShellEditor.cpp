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
constexpr float minimumScale = 1.0f, maximumScale = 2.0f;
constexpr int scaleItemBase = 1000;
constexpr int aboutItem = 2000;
constexpr int saveItem = 10001, reloadItem = 10002, renameItem = 10003, deleteItem = 10004, folderItem = 10005;
constexpr int undoItem = 3001, redoItem = 3002, copyItem = 3003, compareItem = 3004, ioItem = 3005;

const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
const juce::String changedMark (juce::CharPointer_UTF8 (" \xe2\x80\xa2"));
const juce::String apostrophe (juce::CharPointer_UTF8 ("\xe2\x80\x99"));
const juce::String rightArrowText (juce::CharPointer_UTF8 (" \xe2\x86\x92 "));
const juce::String approxText (juce::CharPointer_UTF8 ("\xe2\x89\x88 "));
const juce::String slotLetters[] { "A", "B" };

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

void CaptionPanels::setCaption (const juce::String& caption)
{
    if (caption != captionText)
    {
        captionText = caption;
        repaint();
    }
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
    processor.editorOpened();

    addAndMakeVisible (titleStrip);
    addAndMakeVisible (root);
    root.addAndMakeVisible (body);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &presetDropdown, &saveButton, &parameterButton, &ioPill, &outputKnob, &powerPill, &abSwitch })
        root.addAndMakeVisible (item);

    // Compact plugins keep only A | B in the header; the rest is in the Prm menu (W.1).
    if (style == HeaderStyle::full)
    {
        for (juce::Component* item : std::initializer_list<juce::Component*> { &undoButton, &redoButton, &copyButton, &compareButton,
                                                                               &separators[0], &separators[1], &separators[2] })
            root.addAndMakeVisible (item);
    }
    else
    {
        ioPill.setVisible (false);
    }

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    copyButton.onClick = [this] { copyToOtherSlot(); };
    abSwitch.onSelect = [this] (int slot) { selectSlot (slot); };
    compareButton.onClick = [this] { setCompare (! isCompareOn()); };

    // The history starts from the settings the editor opens on, and Compare measures while open.
    processor.getWorkflow().commit();
    processor.compare.setMeasuring (true);

    root.addChildComponent (sidechainNotice);
    root.addChildComponent (tooltips);

    presetDropdown.onOpen = [this] { showPresetMenu(); };
    presetDropdown.onPrevious = [this] { stepPreset (-1); };
    presetDropdown.onNext = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSavePrompt(); };
    parameterButton.onClick = [this] { showParameterMenu(); };

    // Every window resizes from 100 % to 200 % (Phase 3 Q2.1). enableResizing lets a layout
    // reflow instead.
    setResizable (true, false);

    const auto savedScale = (float) (double) processor.getApvts().state.getProperty (uiScaleProperty, 1.0);
    applyScale (savedScale >= minimumScale && savedScale <= maximumScale ? savedScale : 1.0f);

    timerCallback();
    startTimerHz (timerHz);
}

ShellEditor::~ShellEditor()
{
    stopTimer();

    // Compare is never left on with nobody to see it.
    fancyProcessor.compare.setMeasuring (false);
    fancyProcessor.compare.setOn (false);
    fancyProcessor.getWorkflow().commit();

    // So is a listen or preview switch (§0.3).
    fancyProcessor.editorClosed();

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

    if (auto* browser = PresetBrowser::findOpen (root))
        browser->close();

    tooltips.hide();

    if (sidechainNoticeForced)
        forceSidechainNotice (false);
}

bool ShellEditor::keyPressed (const juce::KeyPress& key)
{
    // Only the plugin's own shortcuts, which only arrive while the plugin window has the
    // keyboard focus. Every other key goes back to the host (H.2, W.2).
    constexpr auto ctrl = juce::ModifierKeys::ctrlModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;

    if (key == juce::KeyPress ('s', ctrl, 0))
    {
        showSavePrompt();
        return true;
    }

    if (key == juce::KeyPress ('z', ctrl, 0))
    {
        undo();
        return true;
    }

    if (key == juce::KeyPress ('z', ctrl | shift, 0) || key == juce::KeyPress ('y', ctrl, 0))
    {
        redo();
        return true;
    }

    return false;
}

std::vector<ShellEditor::PopupOpener> ShellEditor::getPopupOpeners()
{
    return {
        { "preset-menu", [this] { browserFilter = {}; showPresetMenu(); } },
        { "preset-browser-search", [this]
          {
              // The second factory preset's first word, cut short, as someone would type it.
              browserFilter = {};
              showPresetMenu();
              const auto names = fancyProcessor.getPresets().getFactoryNames();
              const auto word = names[juce::jmin (1, names.size() - 1)].upToFirstOccurrenceOf (" ", false, false);

              if (auto* browser = getPresetBrowser())
                  browser->setSearch (word.substring (0, juce::jmax (1, word.length() - 1)));
          } },
        { "preset-browser-categories", [this]
          {
              browserFilter = {};
              showPresetMenu();

              if (auto* browser = getPresetBrowser())
                  browser->showCategoryMenu();
          } },
        { "preset-browser-empty", [this]
          {
              browserFilter = {};
              showPresetMenu();

              if (auto* browser = getPresetBrowser())
              {
                  browser->setCategory (PresetManager::Filter::Kind::all);
                  browser->setSearch ("zzz");
              }
          } },
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
    applyScale (uiScale);
}

void ShellEditor::applyTransforms()
{
    const auto transform = juce::AffineTransform::scale (uiScale);
    titleStrip.setTransform (transform);
    root.setTransform (transform);
}

void ShellEditor::applyScale (float scale)
{
    const auto baseWidth = width, baseHeight = height + titleHeight;

    uiScale = juce::jlimit (minimumScale, maximumScale, scale);
    applyTransforms();

    if (auto* constrainer = getConstrainer())
    {
        if (resizable)
        {
            // The layout reflows: any shape between its limits, at this scale.
            constrainer->setFixedAspectRatio (0.0);
            setResizeLimits (juce::roundToInt ((float) minimumWidth * uiScale), juce::roundToInt ((float) (minimumHeight + titleHeight) * uiScale),
                             juce::roundToInt ((float) maximumWidth * uiScale), juce::roundToInt ((float) (maximumHeight + titleHeight) * uiScale));
        }
        else
        {
            // A fixed layout scales as a whole, from 100 % to 200 %, keeping its shape.
            setResizeLimits (juce::roundToInt ((float) baseWidth * minimumScale), juce::roundToInt ((float) baseHeight * minimumScale),
                             juce::roundToInt ((float) baseWidth * maximumScale), juce::roundToInt ((float) baseHeight * maximumScale));
            constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
        }
    }

    setSize (juce::roundToInt ((float) baseWidth * uiScale), juce::roundToInt ((float) baseHeight * uiScale));
}

void ShellEditor::resized()
{
    auto& state = fancyProcessor.getApvts().state;

    if (resizable)
    {
        width = juce::roundToInt ((float) getWidth() / uiScale);
        height = juce::roundToInt ((float) getHeight() / uiScale) - titleHeight;

        state.setProperty (uiWidthProperty, width, nullptr);
        state.setProperty (uiHeightProperty, height, nullptr);
    }
    else if (width > 0)
    {
        // The host or the user dragged the window's edge: the scale follows its width.
        const auto scale = juce::jlimit (minimumScale, maximumScale, (float) getWidth() / (float) width);

        if (std::abs (scale - uiScale) > 1.0e-4f)
        {
            uiScale = scale;
            applyTransforms();
            state.setProperty (uiScaleProperty, std::round (uiScale * 100.0f) / 100.0f, nullptr);
        }
    }

    titleStrip.setBounds (0, 0, width, titleHeight);
    root.setBounds (0, titleHeight, width, height);
    body.setBounds (0, headerHeight, width, height - headerHeight);
    layoutHeader();
    bodyResized();

    if (auto* browser = PresetBrowser::findOpen (root))
        browser->refresh();
}

void ShellEditor::setTitleColour (juce::Colour colour)
{
    titleColour = colour;
    refreshTitle();
}

void ShellEditor::refreshTitle()
{
    titleStrip.setTitle (fancyProcessor.getName(), titleColour.value_or (getAccentColour()));
    titleStrip.setListening (fancyProcessor.isListening());
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

    const auto shortButtons = compact || shortLabels;

    place (presetDropdown, presetWidth > 0 ? presetWidth : (compact ? 110 : 246));
    place (saveButton, shortButtons ? 28 : 63);
    place (parameterButton, shortButtons ? 43 : 86);

    const auto pillWidth = compact ? 48 : 56;

    powerPill.setBounds (width - 16 - pillWidth, top, pillWidth, itemHeight);
    outputKnob.setBounds (powerPill.getX() - 36, 11, 28, 28);
    headerFreeRight = outputKnob.getX() - gap;

    // W.1: in a compact header, A | B takes the I/O pill's place.
    constexpr int abWidth = 46, iconWidth = 24, compareWidth = 84, iconGap = 4, separatorMargin = 6;

    if (compact)
    {
        place (abSwitch, abWidth);
        headerFreeLeft = x;
        return;
    }

    place (ioPill, shortButtons ? 39 : 80);

    // Undo, redo | A B, copy | Compare, with the mockup's separators.
    x -= gap;

    const auto separator = [&] (HeaderSeparator& line)
    {
        x += separatorMargin;
        line.setBounds (x, top, 1, itemHeight);
        x += 1 + separatorMargin;
    };

    const auto button = [&] (juce::Component& item, int itemWidth, int after)
    {
        item.setBounds (x, top, itemWidth, itemHeight);
        x += itemWidth + after;
    };

    separator (separators[0]);
    button (undoButton, iconWidth, iconGap);
    button (redoButton, iconWidth, 0);
    separator (separators[1]);
    button (abSwitch, abWidth, iconGap);
    button (copyButton, iconWidth, 0);
    separator (separators[2]);
    button (compareButton, compareWidth, 0);

    headerFreeLeft = x + gap;
}

void ShellEditor::setShortHeaderLabels (bool shouldBeShort)
{
    shortLabels = shouldBeShort;

    const auto shortButtons = shortLabels || headerStyle == HeaderStyle::compact;
    saveButton.setText (shortButtons ? "+" : "+ New");
    parameterButton.setText (shortButtons ? "Prm" : "Parameter");

    layoutHeader();
    refreshHeader();
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
    // One drag, one click or one menu choice is one undo step: edits are made into a step once
    // no mouse button is down.
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        fancyProcessor.getWorkflow().commit();

    refreshHeader();
    onTick (1.0f / (float) timerHz);
}

void ShellEditor::refreshWorkflow()
{
    auto& workflow = fancyProcessor.getWorkflow();
    const auto active = workflow.getActiveSlot();

    undoButton.setEnabled (workflow.canUndo() || workflow.hasPendingEdits());
    redoButton.setEnabled (workflow.canRedo());
    undoButton.setTooltip (workflow.canUndo() ? "Undo " + workflow.getUndoName() + " (Ctrl+Z)" : juce::String ("Undo (Ctrl+Z)"));
    redoButton.setTooltip (workflow.canRedo() ? "Redo " + workflow.getRedoName() + " (Ctrl+Shift+Z)" : juce::String ("Redo (Ctrl+Shift+Z)"));
    copyButton.setTooltip ("Copy " + slotLetters[active] + " to " + slotLetters[1 - active]);
    abSwitch.setActive (active);
    compareButton.setState (isCompareOn(), fancyProcessor.compare.getGainDb());
}

void ShellEditor::undo()
{
    fancyProcessor.getWorkflow().undo();
    refreshHeader();
}

void ShellEditor::redo()
{
    fancyProcessor.getWorkflow().redo();
    refreshHeader();
}

void ShellEditor::selectSlot (int slot)
{
    fancyProcessor.getWorkflow().selectSlot (slot);
    refreshHeader();
}

void ShellEditor::copyToOtherSlot()
{
    fancyProcessor.getWorkflow().copyToOtherSlot();
    refreshHeader();
}

void ShellEditor::setCompare (bool shouldBeOn)
{
    fancyProcessor.compare.setOn (shouldBeOn);
    refreshHeader();
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

    if (auto* browser = PresetBrowser::findOpen (root))
        browser->refreshFooter();

    refreshTitle();

    const auto inputs = juce::String (getShownInputs());
    const auto outputs = juce::String (fancyProcessor.getMainBusNumOutputChannels());

    ioPill.setText (headerStyle == HeaderStyle::compact || shortLabels ? inputs + "/" + outputs
                                                                       : inputs + " in " + outputs + " out");

    refreshWorkflow();
}

void ShellEditor::showPresetMenu()
{
    closePopups();

    PresetBrowser::Actions actions;
    const auto safe = juce::Component::SafePointer<ShellEditor> (this);

    actions.save = [safe] { if (safe != nullptr) safe->showSavePrompt(); };
    actions.reload = [safe] { if (safe != nullptr) safe->fancyProcessor.getPresets().reload(); };
    actions.rename = [safe] { if (safe != nullptr) safe->showRenamePrompt(); };
    actions.remove = [safe]
    {
        if (safe != nullptr)
            safe->showDeletePrompt (safe->fancyProcessor.getPresets().getCurrentName());
    };
    actions.openFolder = [safe] { if (safe != nullptr) safe->fancyProcessor.getPresets().openUserDirectory(); };
    actions.changed = [safe] { if (safe != nullptr) safe->refreshHeader(); };

    auto browser = std::make_unique<PresetBrowser> (fancyProcessor.getPresets(), browserFilter, presetDropdown, std::move (actions));
    auto* raw = browser.get();
    adoptPopup (std::move (browser));
    raw->refresh();
    refreshHeader();

    if (raw->isShowing())
        raw->getSearchField().grabKeyboardFocus();
}

PresetBrowser* ShellEditor::getPresetBrowser()
{
    return PresetBrowser::findOpen (root);
}

void ShellEditor::stepPreset (int direction)
{
    fancyProcessor.getPresets().step (direction, browserFilter);

    if (auto* browser = PresetBrowser::findOpen (root))
        browser->refresh();

    refreshHeader();
}

namespace
{
// Names may repeat across categories (Phase 3 Q2.1), but not within one. The category is the
// dialog's second field, or a fixed one when renaming.
std::function<juce::String (const juce::StringArray&)> nameCheck (PresetManager& presets, const juce::String& allowed,
                                                                  std::optional<juce::String> fixedCategory)
{
    return [&presets, allowed, fixedCategory] (const juce::StringArray& values)
    {
        const auto name = juce::File::createLegalFileName (values[0]).trim();
        const auto category = fixedCategory.value_or (values[1]).trim();

        if (name.isEmpty())
            return juce::String ("Type a name for the preset.");

        if (name != allowed && presets.userPresetExists (name, category))
            return "There" + apostrophe + "s already a preset called " + quoted (name)
                   + (category.isEmpty() ? juce::String() : " in " + category) + ".";

        return juce::String();
    };
}

// Completes a category as it's typed, from the ones that exist: the rest is selected, so
// typing on replaces it.
void completeCategories (juce::TextEditor& field, juce::StringArray categories)
{
    categories.removeString (PresetManager::userCategory);

    field.onTextChange = [&field, categories, lastLength = field.getText().length()] () mutable
    {
        const auto text = field.getText();
        const auto grew = text.length() > lastLength;
        lastLength = text.length();

        if (! grew || text.isEmpty() || field.getHighlightedRegion().getLength() > 0)
            return;

        for (const auto& category : categories)
        {
            if (category.length() > text.length() && category.startsWithIgnoreCase (text))
            {
                field.setText (text + category.substring (text.length()), false);
                field.setHighlightedRegion ({ text.length(), category.length() });
                lastLength = text.length();
                return;
            }
        }
    };
}
} // namespace

void ShellEditor::showSavePrompt()
{
    if (auto* browser = PresetBrowser::findOpen (root))
        browser->close();

    auto& presets = fancyProcessor.getPresets();

    DialogSpec spec;
    spec.title = "Save as new preset";
    spec.action = "Save";
    spec.fields.push_back ({ "Name", presets.isUserPresetLoaded() ? presets.getCurrentName() : juce::String() });
    spec.fields.push_back ({ "Category (optional)", presets.getCurrentCategory() });
    spec.validate = nameCheck (presets, {}, std::nullopt);
    spec.note = [] (const juce::StringArray& values)
    {
        const auto folder = juce::File::createLegalFileName (values[1].trim()).trim();

        if (folder.isEmpty() || folder.equalsIgnoreCase (PresetManager::userCategory))
            return juce::String ("Saved in your preset folder.");

        return "Saved in Presets\\" + folder + "\\ beside your other presets.";
    };

    showDialog (root, std::move (spec), [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (bool accepted, const juce::StringArray& values)
    {
        if (safeThis != nullptr && accepted)
        {
            safeThis->fancyProcessor.getPresets().saveUser (values[0], values[1]);
            safeThis->refreshHeader();
        }
    });

    if (auto* dialog = DialogLayer::findOpen (root))
        if (auto* field = dialog->getField (1))
            completeCategories (*field, presets.getUserCategories());
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
    spec.validate = nameCheck (presets, current, presets.getCurrentCategory());

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
    Menu menu, parameterList;

    const auto& parameters = fancyProcessor.getParameters();
    const auto hasHost = getHostContext() != nullptr;

    for (int i = 0; i < parameters.size(); ++i)
    {
        const auto* item = parameters[i];

        if (listsParameter (*item))
            parameterList.addItem (i + 1, item->getName (64), hasHost, false, item->getCurrentValueAsText());
    }

    if (headerStyle == HeaderStyle::compact)
    {
        // W.1: the compact header's workflow commands, as panel 2 of the Phase 2.1 mockup has them.
        auto& workflow = fancyProcessor.getWorkflow();
        workflow.commit();

        const auto active = workflow.getActiveSlot();
        const auto inputs = juce::String (getShownInputs());
        const auto outputs = juce::String (fancyProcessor.getMainBusNumOutputChannels());

        menu.addItem (undoItem, "Undo", workflow.canUndo(), false, "Ctrl+Z");
        menu.addItem (redoItem, "Redo", workflow.canRedo(), false, "Ctrl+Shift+Z");
        menu.addItem (copyItem, "Copy " + slotLetters[active] + rightArrowText + slotLetters[1 - active]);
        menu.addItem (compareItem, approxText + "Compare (level-matched)", true, isCompareOn());
        menu.addSeparator();
        menu.addSubMenu ("Parameters" + ellipsis, std::move (parameterList));
        menu.addItem (ioItem, "I/O: " + inputs + " in / " + outputs + " out", false);
    }
    else
    {
        menu = std::move (parameterList);
    }

    menu.addSeparator();

    Menu sizes;

    for (int i = 0; i < (int) std::size (scales); ++i)
        sizes.addItem (scaleItemBase + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true, std::abs (scales[i] - uiScale) < 0.005f);

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
        else if (result == undoItem)
        {
            self.undo();
        }
        else if (result == redoItem)
        {
            self.redo();
        }
        else if (result == copyItem)
        {
            self.copyToOtherSlot();
        }
        else if (result == compareItem)
        {
            self.setCompare (! self.isCompareOn());
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
