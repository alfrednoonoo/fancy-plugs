#pragma once

#include "Controls.h"
#include "PresetBrowser.h"
#include "WorkflowControls.h"

#include <fancy/plugin/FancyProcessor.h>

#include <optional>

namespace fancy::ui
{

// The two-tone body shared by Kicker and Exciter, with the plugin name as a caption.
class CaptionPanels final : public juce::Component
{
public:
    CaptionPanels (const juce::String& caption, int leftPanelWidth, int captionY);

    // For a caption that follows the plugin's state, such as Kicker's mode.
    void setCaption (const juce::String& caption);

    void paint (juce::Graphics&) override;

private:
    juce::String captionText;
    int leftWidth, captionTop;
};

// Link with a gear icon that opens a plugin's EDIT drawer.
class EditLink final : public juce::Component
{
public:
    explicit EditLink (juce::Colour accent, const juce::String& text = "EDIT");

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    juce::Colour accentColour;
    juce::String linkText;
};

// Overlay holding settings that aren't part of the main layout.
class EditDrawer final : public juce::Component
{
public:
    explicit EditDrawer (const juce::String& title = "EDIT");

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String titleText;
    PillButton closeButton { "Done" };
};

// Title strip, header bar, scaling and resizing, bypass dimming, the preset browser and
// Parameter menu, and the layer that menus, tooltips and dialogs are drawn on. Subclasses add their controls to getBody() and
// put 30 Hz work in onTick().
class ShellEditor : public juce::AudioProcessorEditor, public PopupHost, private juce::Timer
{
public:
    enum class HeaderStyle { compact, full };

    static constexpr int headerHeight = 53;

    // The strip with the plugin's name above the header (Phase 3 Q2.1). The window is this much
    // taller than the size a plugin gives; the body and header keep their positions in getRoot().
    static constexpr int titleHeight = TitleStrip::height;

    ShellEditor (FancyProcessor& processor, int baseWidth, int baseHeight, HeaderStyle style);
    ~ShellEditor() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // PopupHost
    juce::Component& getPopupParent() override { return root; }
    juce::Colour getAccentColour() override { return colours::uiTeal; }
    void adoptPopup (std::unique_ptr<juce::Component> popup) override;
    void disposePopup (juce::Component* popup) override;

    // Every pop-up this plugin can show, each with a function that opens it, for the
    // screenshot gallery and the tests (Phase 2.1 item S). Subclasses add their own.
    struct PopupOpener
    {
        juce::String name;
        std::function<void()> open;
    };

    virtual std::vector<PopupOpener> getPopupOpeners();

    // Closes any open menu, dialog or tooltip.
    void closePopups();

    // Shows the "No sidechain signal" strip whatever the processor says, for rendered screenshots.
    void forceSidechainNotice (bool shouldShow);
    bool isSidechainNoticeShowing() const noexcept { return sidechainNotice.isVisible(); }

    static constexpr const char* sidechainWarningText = "No sidechain signal: check track routing (REAPER: pins 3/4)";

    Tooltips& getTooltips() noexcept { return tooltips; }

    // Runs an action, asking first when `ask` is true. Only for destructive actions.
    void confirmThen (const juce::String& title, const juce::String& body, const juce::String& action, bool ask,
                      std::function<void()> run);

    // Whether any parameter whose ID is the prefix and then a word, such as "b2freq" for "b2",
    // differs from its default, apart from the ignored words. A baseline, as a plain value,
    // replaces the default for its word.
    bool differsFromDefault (const juce::String& prefix, const juce::StringArray& ignoredSuffixes,
                             const std::vector<std::pair<juce::String, float>>& baselines = {});


    // The header's menus and prompts, public so the gallery and the tests can open them.
    // showPresetMenu opens the preset browser (Phase 3 Q2.1).
    void showPresetMenu();
    PresetBrowser* getPresetBrowser();

    // The preset pill's arrows: the next or previous preset in the browser's list.
    void stepPreset (int direction);
    PresetManager::Filter& getBrowserFilter() noexcept { return browserFilter; }

    // The plugin's own size, 1.0 to 2.0 (the size menu, or dragging the window's edge).
    float getUiScale() const noexcept { return uiScale; }
    void showSavePrompt();
    void showRenamePrompt();
    void showDeletePrompt (const juce::String& presetName);
    void showParameterMenu();
    void showAbout();

    // Undo, redo, A/B and Compare, as the header's buttons and the compact Prm menu do them.
    void undo();
    void redo();
    void selectSlot (int slot);
    void copyToOtherSlot();
    void setCompare (bool shouldBeOn);
    bool isCompareOn() const noexcept { return fancyProcessor.compare.isOn(); }

protected:
    // Lets the layout reflow between these sizes, in 100 % units, as Fancy EQ's does. Call this
    // at the end of the subclass's constructor. The size is saved with the plugin's state.
    // Without it, the window still resizes from 100 % to 200 %, scaling the layout as a whole.
    void enableResizing (int minWidth, int minHeight, int maxWidth, int maxHeight);

    int getBaseWidth() const noexcept { return width; }
    int getBaseHeight() const noexcept { return height; }

    // Shows a tooltip at once, beside a control, without waiting for the mouse to hover.
    void showTip (juce::Component& near, const juce::String& text);

    // The title strip's colour, for a plugin whose accent follows a selection (EQ, MultiComp, FIR).
    void setTitleColour (juce::Colour colour);

    // Narrows the preset dropdown, to make room for controls in the header row.
    void setPresetDropdownWidth (int newWidth);

    // Uses the compact header's labels ("+", "Prm", "4/2") in a full header, to make room for
    // the workflow controls in a narrow window.
    void setShortHeaderLabels (bool shouldBeShort);

    // The free part of the header row, between the workflow controls and the output knob, for
    // a plugin's own items there.
    int getHeaderFreeLeft() const noexcept { return headerFreeLeft; }
    int getHeaderFreeRight() const noexcept { return headerFreeRight; }

    // The number the I/O pill shows. A plugin can count its sidechain here.
    virtual int getShownInputs() { return fancyProcessor.getMainBusNumInputChannels(); }

    // Where the "No sidechain signal" strip goes: by default across the top of the body.
    virtual juce::Rectangle<int> getSidechainNoticeBounds();

    // The window's root, for controls that sit in the header row.
    juce::Component& getRoot() noexcept { return root; }

    virtual void bodyResized() {}

    // Lets a plugin leave parameters out of the Parameter menu.
    virtual bool listsParameter (const juce::AudioProcessorParameter&) { return true; }

    juce::Component& getBody() noexcept { return body; }
    FancyProcessor& getFancyProcessor() noexcept { return fancyProcessor; }
    juce::RangedAudioParameter& parameter (const juce::String& id);

    // Stops the 30 Hz updates, so a rendered screenshot keeps its preview levels. The header is
    // refreshed once more, because the constructor's refresh can't see a plugin's overrides.
    void stopUpdates() { stopTimer(); refreshHeader(); }

    virtual void onTick (float seconds) { juce::ignoreUnused (seconds); }

private:
    class Root final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Shown while the processor's SidechainWatch reports a silent sidechain. Not a control: it
    // appears over the body only while the warning is on.
    class SidechainNotice final : public juce::Component
    {
    public:
        SidechainNotice() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override;
    void refreshHeader();
    void refreshWorkflow();
    void applyScale (float scale);
    void applyTransforms();
    void layoutHeader();
    void refreshTitle();

    juce::SharedResourcePointer<Typefaces> typefaces;
    juce::SharedResourcePointer<FancyLookAndFeel> lookAndFeel;

    FancyProcessor& fancyProcessor;
    int width, height;
    int minimumWidth = 0, minimumHeight = 0, maximumWidth = 0, maximumHeight = 0;
    bool resizable = false;
    const HeaderStyle headerStyle;
    float uiScale = 1.0f;
    int presetWidth = 0;

    TitleStrip titleStrip;
    std::optional<juce::Colour> titleColour;
    PresetManager::Filter browserFilter;
    Root root;
    juce::Component body;
    PresetPill presetDropdown;
    PillButton saveButton, parameterButton, ioPill;
    HeaderIconButton undoButton { HeaderIconButton::Icon::undo }, redoButton { HeaderIconButton::Icon::redo },
        copyButton { HeaderIconButton::Icon::copy };
    AbSwitch abSwitch;
    CompareButton compareButton;
    HeaderSeparator separators[3];
    bool shortLabels = false;
    int headerFreeLeft = 0, headerFreeRight = 0;
    Knob outputKnob;
    PowerPill powerPill;
    SidechainNotice sidechainNotice;
    bool sidechainNoticeForced = false;
    Tooltips tooltips { root };

    // Declared last, so open pop-ups go before the root they sit on.
    std::vector<std::unique_ptr<juce::Component>> popups;
};

} // namespace fancy::ui
