#pragma once

#include "Controls.h"

#include <fancy/plugin/FancyProcessor.h>

namespace fancy::ui
{

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
    EditDrawer();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
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
    const int width, height;
    const HeaderStyle headerStyle;
    float uiScale = 1.0f;

    Root root;
    juce::Component body;
    PillDropdown presetDropdown;
    PillButton saveButton, parameterButton, ioPill;
    Knob outputKnob;
    PowerPill powerPill;
};

} // namespace fancy::ui
