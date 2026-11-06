#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Skeleton: an empty panel at the Kicker's window size, until the fancy-core components exist.
class KickerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit KickerEditor (juce::AudioProcessor& processor);

    void paint (juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerEditor)
};
