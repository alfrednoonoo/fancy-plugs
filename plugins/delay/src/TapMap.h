#pragma once

#include "DelayProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

juce::Colour tapColour (int tap);

// The time map at the top of the window: the dry bar, one bar per tap with a numbered handle,
// and faded bars for the feedback repeats. Drag a handle to change a tap's time and volume.
class TapMap final : public juce::Component
{
public:
    explicit TapMap (DelayProcessor& processor);

    void setSelectedTap (int tap);
    void update();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int)> onTapSelected;

    // Exposed for tests: the axis spans this many beats, from four up.
    static int beatsForLongest (double longestMs, double bpm);

private:
    float xForMs (double ms) const;
    double msForX (float x) const;
    float yForVolume (float normalised) const;
    float volumeForY (float y) const;
    float volumeOf (int tap) const;
    double axisMs() const;

    juce::Point<float> handleOf (int tap) const;
    int tapAt (juce::Point<float> position) const;
    void paintRepeats (juce::Graphics&, int tap, const DelayDsp::TapSpec&) const;

    DelayProcessor& delayProcessor;
    DelayDsp::Targets targets;

    int selected = 0;
    int dragged = -1;
    double fixedAxisMs = 0.0;
};
