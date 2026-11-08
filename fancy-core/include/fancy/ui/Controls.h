#pragma once

#include "Popups.h"
#include "Style.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

namespace fancy::ui
{

// Parameter-bound control. Provides the §2.3 gestures: vertical drag, shift for fine,
// double-click to reset, mouse wheel, right-click for the host menu. A click on the value,
// or a ctrl-click anywhere, opens a box to type a new value.
class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ParamControl (juce::RangedAudioParameter& parameter);

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // A locked control ignores drags, the wheel, double-clicks and typed values, and shows the
    // hint as its tooltip. A click calls onLockedClick, so the editor can explain why.
    void setLocked (bool shouldLock, const juce::String& hint = {});
    bool isLocked() const noexcept { return locked; }

    std::function<void()> onLockedClick;

    // Sees typed text before the parameter does. Returning true means it was handled.
    std::function<bool (const juce::String&)> onTypedText;

    // Opens the box to type a value, as a click on the value does.
    void openValueEditor() { showValueEditor(); }
    bool isValueEditorOpen() const noexcept { return valueEditor != nullptr && valueEditor->isVisible(); }
    juce::TextEditor* getValueEditor() const noexcept { return valueEditor.get(); }

    // The colour of the typed-value field's border and caret. The plugin's accent by default.
    virtual juce::Colour getAccentColour() const;

protected:
    float getNormalised() const { return param.convertTo0to1 (value); }
    bool isOn() const { return value >= 0.5f; }
    juce::String getValueText() const;
    void setDenormalised (float newValue);

    // Where the value is written. Empty for a control that doesn't show one.
    virtual juce::Rectangle<int> getValueArea() const { return {}; }
    virtual float getDragPixels() const { return 200.0f; }

    juce::RangedAudioParameter& param;

private:
    void showHostMenu();
    void showValueEditor();
    void closeValueEditor (bool applyText);
    void applyTypedText (const juce::String& text);

    float value = 0.0f;
    float lastDragY = 0.0f;
    float dragPosition = 0.0f;
    bool dragging = false;
    bool clickOpensEditor = false;
    bool valueEditorClosing = false;
    bool locked = false;
    juce::String textBeforeEditing, unlockedTooltip;
    juce::ParameterAttachment attachment;
    std::unique_ptr<juce::TextEditor> valueEditor;
};

// A control that acts on a click, with no drag, wheel or double-click gestures.
class ParamClickControl : public ParamControl
{
public:
    using ParamControl::ParamControl;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override {}
    void mouseUp (const juce::MouseEvent&) override {}
    void mouseDoubleClick (const juce::MouseEvent&) override {}
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

    // Acts as a click does: toggles, or opens the dropdown's menu.
    void click() { clicked(); }

protected:
    virtual void clicked();
};

class Knob final : public ParamControl
{
public:
    // Sizes from the Figma Knob and RoundController components.
    struct Style
    {
        float outerDiameter, faceDiameter, arcThickness, triangleSide, triangleDistance;
        float labelHeight, labelGap, valueGap, valueHeight;
        float fontSize;
        bool mediumWeight, showText;
    };

    static constexpr Style largeStyle { 116.0f, 104.0f, 5.0f, 19.0f, 34.3f, 19.0f, 8.0f, 4.0f, 25.0f, 16.0f, true, true };
    static constexpr Style mediumStyle { 72.0f, 62.0f, 4.0f, 15.6f, 17.2f, 17.0f, 8.0f, 3.0f, 23.0f, 14.0f, false, true };
    static constexpr Style smallStyle { 50.0f, 42.0f, 3.0f, 13.5f, 10.0f, 17.0f, 6.0f, 3.0f, 23.0f, 14.0f, false, true };
    static constexpr Style tightStyle { 50.0f, 42.0f, 3.0f, 13.5f, 10.0f, 17.0f, 6.0f, 3.0f, 23.0f, 12.0f, false, true };
    static constexpr Style compactStyle { 50.0f, 42.0f, 3.0f, 13.5f, 10.0f, 0.0f, 0.0f, 3.0f, 23.0f, 12.0f, false, true };
    static constexpr Style headerStyle { 28.0f, 24.0f, 2.0f, 6.1f, 6.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false };

    Knob (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent, const Style& style, bool bipolar = false);

    // Shows the dot marking the default position. The designs use it on some knobs only.
    void setShowDefaultMark (bool shouldShow);

    // Figma colours some value text differently from the arc.
    void setValueColour (juce::Colour colour);

    // Replaces the value text, such as "off" for a control that's locked out.
    void setValueTextOverride (const juce::String& text);

    // Text after the value, such as a note name. White unless a colour is given.
    void setValueSuffix (const juce::String& suffix, juce::Colour colour = juce::Colours::white);

    void setStyle (const Style& style);

    void paint (juce::Graphics&) override;
    juce::Colour getAccentColour() const override { return accentColour; }

private:
    juce::Rectangle<int> getValueArea() const override;

    juce::String labelText;
    juce::Colour accentColour, valueColour;
    juce::String valueSuffix, valueOverride;
    juce::Colour suffixColour { juce::Colours::white };
    Style knobStyle;
    bool isBipolar;
    bool showDefaultMark = false;
};

class Fader final : public ParamControl
{
public:
    Fader (juce::RangedAudioParameter& parameter, juce::Colour accent);

    void paint (juce::Graphics&) override;
    juce::Colour getAccentColour() const override { return accentColour; }

private:
    juce::Rectangle<int> getValueArea() const override;
    float getDragPixels() const override { return (float) getHeight(); }

    juce::Colour accentColour;
};

// One button per choice of a choice parameter. The selected segment is filled.
class ParamSegments : public ParamClickControl
{
public:
    ParamSegments (juce::RangedAudioParameter& parameter, juce::Colour selectedFill, juce::Colour selectedText);

    void setSelectedColours (juce::Colour selectedFill, juce::Colour selectedText);
    void setTextHeight (float height);

    // Widths in proportion to each segment's text, for labels of very different lengths.
    void setFitToText (bool shouldFit);

    void mouseDown (const juce::MouseEvent&) override;
    void paint (juce::Graphics&) override;

protected:
    // Draws one segment's label. By default it's the parameter's text for that choice.
    virtual void paintSegment (juce::Graphics&, int index, juce::Rectangle<float> area, juce::Colour ink);

private:
    int getSelected() const;
    juce::Rectangle<float> segmentBounds (int index) const;

    int numSegments;
    juce::Colour fillColour, textColour;
    float textHeight = 14.0f;
    bool fitToText = false;
};

// A small pill with text on it, filled in the accent colour while the parameter is on.
class ParamTextToggle final : public ParamClickControl
{
public:
    ParamTextToggle (juce::RangedAudioParameter& parameter, const juce::String& text, juce::Colour accent);

    void paint (juce::Graphics&) override;

private:
    juce::String buttonText;
    juce::Colour colour;
};

// A power glyph with no pill, lit in the accent colour while the parameter is on.
class ParamPowerIcon final : public ParamClickControl
{
public:
    ParamPowerIcon (juce::RangedAudioParameter& parameter, juce::Colour accent);

    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
};

// A plus or a cross that fires on mouse up.
class IconButton final : public juce::Component
{
public:
    enum class Glyph { plus, cross };

    explicit IconButton (Glyph glyph);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    Glyph iconGlyph;
};

// Numbered tabs in their own colours, with a + at the end.
class ColourTabs final : public juce::Component
{
public:
    struct Tab
    {
        int id;
        juce::Colour colour;
    };

    void setTabs (std::vector<Tab> newTabs, int selectedId);

    // Where the strip ends, for whatever sits to its right.
    int getEndX() const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onSelect;
    std::function<void()> onAdd;

    bool showAddButton = true;

private:
    juce::Rectangle<int> tabBounds (int index) const;

    std::vector<Tab> tabs;
    int selected = 0;
};

class ParamCheckbox final : public ParamClickControl, private juce::Timer
{
public:
    ParamCheckbox (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent = colours::uiTeal);

    // For a row of long labels. The default is the Figma body size.
    void setLabelHeight (float height);

    // Draws an outline that fades over the given time, to show where the checkbox is.
    void pulse (juce::Colour colour, int milliseconds);
    bool isPulsing() const noexcept { return pulseLeft > 0.0f; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::String labelText;
    juce::Colour accentColour, pulseColour;
    float labelHeight = 14.0f;
    float pulseLeft = 0.0f, pulseLength = 1.0f;
};

// Adds a line to a control's tooltip for settings that change the plugin's latency, because
// some hosts only read the new latency when playback stops and starts (Phase 2.1 H.6).
inline constexpr const char* latencyNote = "Latency changes take effect in some hosts after you stop and start playback.";
void addLatencyNote (juce::SettableTooltipClient& control);

// Draws the shared dropdown look, for a control that isn't a ParamDropdown. Lighter while open.
void paintDropdownBox (juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& text, float textHeight, bool open = false);

// The header's power pill: lit while the plugin is active, grey while bypassed.
class PowerPill final : public ParamClickControl
{
public:
    using ParamClickControl::ParamClickControl;

    void paint (juce::Graphics&) override;
};

class ParamDropdown final : public ParamClickControl, public MenuTarget
{
public:
    using ParamClickControl::ParamClickControl;

    // Puts a section label before the choice at this index, such as "Sidechain".
    void addSection (int choiceIndex, const juce::String& label);

    // For a long choice in a narrow space. The default is the Figma body size.
    void setTextHeight (float height);

    void paint (juce::Graphics&) override;

private:
    void clicked() override;

    float textHeight = 14.0f;
    std::vector<std::pair<int, juce::String>> sections;
};

class PillButton final : public juce::Component
{
public:
    explicit PillButton (const juce::String& text);

    void setText (const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    juce::String buttonText;
};

// Pill with up/down chevrons at the right.
class PillDropdown final : public juce::Component, public MenuTarget
{
public:
    void setText (const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    juce::String dropdownText;
};

} // namespace fancy::ui
