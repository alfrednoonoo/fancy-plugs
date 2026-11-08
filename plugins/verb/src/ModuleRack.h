#pragma once

#include "VerbProcessor.h"

#include <fancy/ui/Controls.h>

#include <functional>
#include <memory>
#include <vector>

class ModuleRack;

// One module in the rack. Its settings aren't host parameters, so each knob gets a private
// parameter that writes through to the chain.
class ModuleCard final : public juce::Component
{
public:
    static constexpr int height = 352;

    ModuleCard (VerbProcessor& processor, ModuleRack& rack, const juce::ValueTree& module);

    static int widthOf (verb::ModuleType type);

    int getUid() const noexcept { return uid; }
    verb::ModuleType getType() const noexcept { return type; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Keeps the file controls current with the last build.
    void refresh();

private:
    struct Link final : public juce::AudioProcessorParameter::Listener
    {
        Link (juce::RangedAudioParameter& parameterToUse, std::function<void (float)> changed);
        ~Link() override;

        void parameterValueChanged (int, float) override;
        void parameterGestureChanged (int, bool) override {}

        juce::RangedAudioParameter& parameter;
        std::function<void (float)> onChange;
    };

    struct Control
    {
        std::unique_ptr<juce::RangedAudioParameter> parameter;
        std::unique_ptr<Link> link;
        std::unique_ptr<juce::Component> component;
    };

    class CardButton final : public juce::Component, public fancy::ui::MenuTarget
    {
    public:
        explicit CardButton (const juce::String& text, bool isDropdown = false);

        void setText (const juce::String& text, juce::Colour colour);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onClick;

    private:
        juce::String buttonText;
        juce::Colour textColour;
        bool dropdown;
    };

    void addSettingKnobs();
    void addFileControls();
    void placeKnobs();

public:
    void showFileMenu();
    void showGainMenu();

    // Whether the module's settings differ from a new one's, so deleting it asks first.
    bool isChanged() const;

private:
    void browse();
    void stepFile (int direction);

    juce::File currentFile() const;
    juce::String breadcrumb() const;
    juce::Colour accent() const;

    juce::Rectangle<int> powerArea() const;
    juce::Rectangle<int> closeArea() const;
    juce::Rectangle<int> plusArea() const;
    juce::Rectangle<int> gripArea() const;

    VerbProcessor& processor;
    ModuleRack& owner;
    juce::ValueTree tree;
    const verb::ModuleType type;
    const int uid;

    bool isOn = true;
    bool draggingCard = false;
    int dragStartX = 0;

    std::vector<Control> controls;

    CardButton fileDropdown { {}, true }, previousButton { "<" }, nextButton { ">" }, browseButton { "Browse..." }, gainDropdown { {}, true };
    std::unique_ptr<juce::FileChooser> chooser;
};

// The scrolling row of cards, and the panel that adds a module.
class ModuleRack final : public juce::Component
{
public:
    explicit ModuleRack (VerbProcessor& processor);

    void rebuild();
    void refresh();

    void resized() override;
    void paint (juce::Graphics&) override;

    // Opens the add panel so that a new module lands at this position.
    void showAddPanel (int position);
    void hideAddPanel();
    bool isAddPanelOpen() const noexcept { return addPosition >= 0; }

    void cardDragged (ModuleCard& card, int deltaX);
    void cardDropped (ModuleCard& card, int deltaX);

    int getContentWidth() const noexcept { return content.getWidth(); }
    int getScroll() const noexcept { return viewport.getViewPositionX(); }
    void setScroll (int x) { viewport.setViewPosition (x, 0); }
    bool isEmpty() const noexcept { return cards.empty(); }

    const std::vector<std::unique_ptr<ModuleCard>>& getCards() const noexcept { return cards; }

    std::function<void()> onLayoutChanged;

private:
    class AddButton final : public juce::Component
    {
    public:
        AddButton (const juce::String& text, std::function<void()> clicked);

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        int idealWidth() const;

    private:
        juce::String buttonText;
        std::function<void()> onClick;
    };

    void layoutContent();
    int indexOf (const ModuleCard& card) const;

    VerbProcessor& processor;
    juce::Viewport viewport;
    juce::Component content;
    juce::Component addPanel;

    std::vector<std::unique_ptr<ModuleCard>> cards;
    std::vector<std::unique_ptr<AddButton>> addButtons, emptyButtons;
    int addPosition = -1;
};

// The mini-map under the rack: one chip per module, with the visible part outlined.
class ChainStrip final : public juce::Component
{
public:
    explicit ChainStrip (VerbProcessor& processor, ModuleRack& rack);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    struct Chip
    {
        juce::Rectangle<float> bounds;
        int uid;
    };

    std::vector<Chip> chips() const;
    float scale() const;

    VerbProcessor& processor;
    ModuleRack& rack;
};
