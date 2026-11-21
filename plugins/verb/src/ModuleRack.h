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

    // Phase 3 Q2.8: a true-stereo IR, from a four-channel file or a pair.
    bool isTrueStereo() const;

    // Dims the Reverb generator's Size while Early is Off or Plate.
    void updateDimming();

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
    void paintEqCurve (juce::Graphics&, float dim) const; // Phase 4 Wave 1

public:
    void showFileMenu();
    void showGainMenu();

    // Whether the module's settings differ from a new one's, so deleting it asks first.
    bool isChanged() const;

private:
    void browse();
    void stepFile (int direction);
    void browsePair(); // Phase 3 Q2.8

    juce::File currentFile() const;
    juce::String breadcrumb() const;
    juce::Colour accent() const;

    juce::Rectangle<int> powerArea() const;
    juce::Rectangle<int> closeArea() const;
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
class ModuleRack final : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit ModuleRack (VerbProcessor& processor);
    ~ModuleRack() override;

    // Phase 4 Wave 1 (docs/verb.md section 3): asks where to save, then writes the finished IR.
    void exportIr();

    // Phase 4 Wave 1: an audio file dropped on the rack adds a File module at the end, or
    // replaces the file of the File card it lands on. Anything else is refused with a message.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void paintOverChildren (juce::Graphics&) override;

    // The caption the drop shows, for tests and the screenshots.
    const juce::String& getDropCaption() const noexcept { return dropCaption; }
    void showDropPreview (const juce::String& fileName);

    void rebuild();
    void refresh();

    void resized() override;
    void paint (juce::Graphics&) override;

    // A click on the rack's empty space closes the add panel (Phase 3 Q2.8).
    void mouseDown (const juce::MouseEvent&) override;

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

    // A short message for the user, shown by the editor as a tip (Phase 4 Wave 1).
    std::function<void (const juce::String&)> onMessage;

private:
    class AddButton final : public juce::Component
    {
    public:
        AddButton (const juce::String& text, std::function<void()> clicked, float textHeight = 16.0f, bool outlined = false);

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        int idealWidth() const;

    private:
        juce::String buttonText;
        std::function<void()> onClick;
        float height;
        bool outline;
    };

    // The dashed box after the last card that opens the add panel there (Phase 3 Q2.8,
    // reference/phase3/25__verb_add_module_mockup.png).
    class AddBox final : public juce::Component
    {
    public:
        explicit AddBox (std::function<void()> clicked) : onClick (std::move (clicked)) {}

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        std::function<void()> onClick;
    };

    void layoutContent();
    int indexOf (const ModuleCard& card) const;
    ModuleCard* fileCardAt (int x, int y) const;
    void timerCallback() override;

    std::unique_ptr<juce::FileChooser> exportChooser;
    juce::String dropCaption;
    juce::Point<int> dropPoint;
    bool dragging = false;

    VerbProcessor& processor;
    juce::Viewport viewport;
    juce::Component content;
    juce::Component addPanel;
    AddBox addBox;
    std::unique_ptr<AddButton> cancelButton;

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
