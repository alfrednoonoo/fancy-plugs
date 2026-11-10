#pragma once

#include "Popups.h"

#include <fancy/plugin/PresetManager.h>

#include <functional>
#include <memory>
#include <vector>

namespace fancy::ui
{

// The preset pill in the header (Phase 3 Q2.1): the preset's name, which opens the browser,
// between previous and next arrows. Drawn as reference/phase3/17c__preset_browser_closeups.png.
class PresetPill final : public juce::Component, public MenuTarget
{
public:
    PresetPill();
    ~PresetPill() override;

    void setText (const juce::String& text);
    const juce::String& getText() const noexcept { return pillText; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onOpen, onPrevious, onNext;

    static constexpr int arrowWidth = 18;

private:
    class Arrow;

    juce::String pillText;
    std::unique_ptr<Arrow> previous, next;
};

// The 20 px strip above the header with the plugin's name (Phase 3 Q2.1, reference/phase3/
// 18__title_strip.png): "FANCY" in grey and the rest in the plugin's colour.
class TitleStrip final : public juce::Component
{
public:
    static constexpr int height = 20;

    TitleStrip() { setInterceptsMouseClicks (false, false); }

    void setTitle (const juce::String& pluginName, juce::Colour colour);

    // A "LISTENING" tag after the name, in the title's colour, while a listen or preview
    // switch is on (§0.3 of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md).
    void setListening (bool shouldShow);
    bool isListeningShown() const noexcept { return listening; }

    void paint (juce::Graphics&) override;

    static constexpr const char* listeningText = "LISTENING";

private:
    juce::String first, rest;
    juce::Colour restColour;
    bool listening = false;
};

// The preset browser: search, category, the list with favourites, and the preset actions,
// as reference/phase3/17a and 17b. It sits on the editor's pop-up layer like a menu; a click
// outside it or Esc closes it.
class PresetBrowser final : public juce::Component
{
public:
    struct Actions
    {
        std::function<void()> save, reload, rename, remove, openFolder;

        // After a preset loads or the list changes, so the header can follow.
        std::function<void()> changed;
    };

    PresetBrowser (PresetManager& presets, PresetManager::Filter& filter, juce::Component& anchor, Actions actions);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

    static PresetBrowser* findOpen (juce::Component& parent);

    void close();

    // Reads the presets again, after a load, a save or a change of filter.
    void refresh();

    // Updates the footer's Reload, Rename and Delete, which follow the loaded preset.
    void refreshFooter();

    // For the tests and the screenshot gallery.
    void setSearch (const juce::String& text);
    void setCategory (PresetManager::Filter::Kind kind, const juce::String& category = {});
    void showCategoryMenu();
    int getNumEntries() const;
    const PresetManager::Entry& getEntry (int index) const;
    void chooseEntry (int index, bool andClose);
    void toggleFavourite (int index);
    void setHighlightedEntry (int index);
    juce::Rectangle<int> getPanelBounds() const;
    juce::TextEditor& getSearchField();
    juce::String getEmptyText() const;
    juce::StringArray getFooterLabels() const;

    juce::Colour getAccent() const noexcept { return accent; }

private:
    class Panel;
    class List;
    class SearchField;
    class FooterButton;

    void layoutPanel();
    void searchChanged();
    juce::String categoryLabel() const;

    PresetManager& presets;
    PresetManager::Filter& filter;
    juce::Component::SafePointer<juce::Component> anchor;
    juce::Component::SafePointer<juce::Component> previousFocus;
    Actions actions;
    juce::Colour accent;
    bool closed = false;

    std::unique_ptr<Panel> panel;
};

} // namespace fancy::ui
