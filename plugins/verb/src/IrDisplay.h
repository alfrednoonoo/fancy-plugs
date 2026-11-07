#pragma once

#include "IrBuilder.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The waveform of the finished impulse, with its time axis, info string and trim handles.
class IrDisplay final : public juce::Component
{
public:
    void setPreview (verb::IrPreview newPreview, double trimStartSeconds, double trimEndSeconds);

    // Called during a handle drag, in seconds. End 0 means the end of the IR.
    std::function<void (double, double)> onTrimChanged;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Handle { none, start, end };

    double windowSeconds() const noexcept;
    float xOf (double seconds) const noexcept;
    double secondsAt (float x) const noexcept;
    double endSeconds() const noexcept;
    Handle handleAt (float x) const noexcept;

    verb::IrPreview preview;
    double trimStart = 0.0, trimEnd = 0.0;
    Handle dragged = Handle::none;
};
