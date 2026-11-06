#pragma once

#include "Style.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace fancy::ui
{

// Parameter-bound control. Provides the §2.3 gestures: vertical drag, shift for fine,
// double-click to reset, ctrl-click to type, mouse wheel, right-click for the host menu.
class ParamControl : public juce::Component
{
public:
    explicit ParamControl (juce::RangedAudioParameter& parameter);

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

protected:
    float getNormalised() const { return param.convertTo0to1 (value); }
    bool isOn() const { return value >= 0.5f; }
    juce::String getValueText() const;
    void setDenormalised (float newValue);

    virtual juce::Rectangle<int> getValueEditorBounds() const { return getLocalBounds(); }
    virtual float getDragPixels() const { return 200.0f; }

    juce::RangedAudioParameter& param;

private:
    void showHostMenu();
    void showValueEditor();
    void hideValueEditor();

    float value = 0.0f;
    float lastDragY = 0.0f;
    bool dragging = false;
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
    static constexpr Style headerStyle { 28.0f, 24.0f, 2.0f, 6.1f, 6.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false };

    Knob (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent, const Style& style, bool bipolar = false);

    // Shows the dot marking the default position. The designs use it on some knobs only.
    void setShowDefaultMark (bool shouldShow);

    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<int> getValueEditorBounds() const override;

    juce::String labelText;
    juce::Colour accentColour;
    Style knobStyle;
    bool isBipolar;
    bool showDefaultMark = false;
};

class Fader final : public ParamControl
{
public:
    Fader (juce::RangedAudioParameter& parameter, juce::Colour accent);

    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<int> getValueEditorBounds() const override;
    float getDragPixels() const override { return (float) getHeight(); }

    juce::Colour accentColour;
};

class ParamCheckbox final : public ParamClickControl
{
public:
    ParamCheckbox (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent = colours::uiTeal);

    void paint (juce::Graphics&) override;

private:
    juce::String labelText;
    juce::Colour accentColour;
};

// The header's power pill: lit while the plugin is active, grey while bypassed.
class PowerPill final : public ParamClickControl
{
public:
    using ParamClickControl::ParamClickControl;

    void paint (juce::Graphics&) override;
};

class ParamDropdown final : public ParamClickControl
{
public:
    using ParamClickControl::ParamClickControl;

    void paint (juce::Graphics&) override;

private:
    void clicked() override;
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
class PillDropdown final : public juce::Component
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
