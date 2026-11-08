#pragma once

#include "Style.h"

#include <functional>

namespace fancy::ui
{

// The header's undo, redo and A/B controls (Phase 2.1 W.1), as panel 1 of
// reference/phase2_1/14__phase2_1_controls.png draws them.

// A 24 px square button with a drawn icon, dimmed when disabled.
class HeaderIconButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Icon { undo, redo, copy };

    explicit HeaderIconButton (Icon icon);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void enablementChanged() override { repaint(); }

    std::function<void()> onClick;

private:
    Icon icon;
};

// The two-segment A | B switch.
class AbSwitch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AbSwitch();

    void setActive (int slot);
    int getActive() const noexcept { return active; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Called with the slot clicked, if it isn't the active one.
    std::function<void (int)> onSelect;

private:
    juce::Rectangle<float> segmentBounds (int slot) const;

    int active = 0;
};

// "≈ Compare". While on it's lit, and shows the matching gain instead of its name.
class CompareButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    CompareButton();

    void setState (bool isOn, float gainDb);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

    // The read-out while on, such as "+2.4 dB".
    static juce::String formatGain (float gainDb);

private:
    bool on = false;
    float gain = 0.0f;
};

// The thin line between groups of header controls.
class HeaderSeparator final : public juce::Component
{
public:
    HeaderSeparator() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
};

namespace icons
{
juce::Path undoArrow (juce::Rectangle<float> area);
juce::Path redoArrow (juce::Rectangle<float> area);
juce::Path rightArrow (juce::Rectangle<float> area);
juce::Path approx (juce::Rectangle<float> area);
} // namespace icons

} // namespace fancy::ui
