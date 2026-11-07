#pragma once

#include "Controls.h"

#include <fancy/plugin/FancyProcessor.h>

namespace fancy::ui
{

// The two-tone body shared by Kicker and Exciter, with the plugin name as a caption.
class CaptionPanels final : public juce::Component
{
public:
    CaptionPanels (const juce::String& caption, int leftPanelWidth, int captionY);

    void paint (juce::Graphics&) override;

private:
    juce::String captionText;
    int leftWidth, captionTop;
};

// Link with a gear icon that opens a plugin's EDIT drawer.
class EditLink final : public juce::Component
{
public:
    explicit EditLink (juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    juce::Colour accentColour;
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

// Header bar, scaling, bypass dimming and the Parameter menu. Subclasses add their
// controls to getBody() and put 30 Hz work in onTick().
class ShellEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    enum class HeaderStyle { compact, full };

    static constexpr int headerHeight = 53;

    ShellEditor (FancyProcessor& processor, int baseWidth, int baseHeight, HeaderStyle style);
    ~ShellEditor() override;

    void resized() override;

protected:
    // Lets the host resize the window between these sizes, in 100 % units. Call this at the
    // end of the subclass's constructor. The size is saved with the plugin's state.
    void enableResizing (int minWidth, int minHeight, int maxWidth, int maxHeight);

    int getBaseWidth() const noexcept { return width; }
    int getBaseHeight() const noexcept { return height; }

    // The window's root, for controls that sit in the header row.
    juce::Component& getRoot() noexcept { return root; }

    virtual void bodyResized() {}

    // Lets a plugin leave parameters out of the Parameter menu.
    virtual bool listsParameter (const juce::AudioProcessorParameter&) { return true; }

    juce::Component& getBody() noexcept { return body; }
    FancyProcessor& getFancyProcessor() noexcept { return fancyProcessor; }
    juce::RangedAudioParameter& parameter (const juce::String& id);

    // Stops the 30 Hz updates, so a rendered screenshot keeps its preview levels.
    void stopUpdates() { stopTimer(); }

    virtual void onTick (float seconds) { juce::ignoreUnused (seconds); }

private:
    class Root final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override;
    void applyScale (float scale);
    void showPresetMenu();
    void showSavePrompt();
    void showParameterMenu();
    void showAbout();
    void layoutHeader();

    juce::SharedResourcePointer<Typefaces> typefaces;
    juce::SharedResourcePointer<FancyLookAndFeel> lookAndFeel;

    FancyProcessor& fancyProcessor;
    int width, height;
    int minimumWidth = 0, minimumHeight = 0, maximumWidth = 0, maximumHeight = 0;
    bool resizable = false;
    const HeaderStyle headerStyle;
    float uiScale = 1.0f;

    Root root;
    juce::Component body;
    PillDropdown presetDropdown;
    PillButton saveButton, parameterButton, ioPill;
    Knob outputKnob;
    PowerPill powerPill;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace fancy::ui
