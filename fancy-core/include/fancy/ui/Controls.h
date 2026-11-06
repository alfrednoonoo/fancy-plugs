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
    struct Style
    {
        float faceDiameter, triangleSide, triangleDistance, arcThickness;
        float labelHeight;
        bool showText;
    };

    static constexpr Style largeStyle { 70.0f, 12.5f, 22.0f, 2.5f, 14.0f, true };
    static constexpr Style mediumStyle { 42.0f, 10.0f, 11.0f, 2.5f, 12.5f, true };
    static constexpr Style smallStyle { 28.0f, 7.0f, 7.5f, 2.0f, 11.5f, true };
    static constexpr Style headerStyle { 16.0f, 5.0f, 4.0f, 1.5f, 0.0f, false };

    Knob (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent, const Style& style, bool bipolar = false);

    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<int> getValueEditorBounds() const override;

    juce::String labelText;
    juce::Colour accentColour;
    Style knobStyle;
    bool isBipolar;
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
