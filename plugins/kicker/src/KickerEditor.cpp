#include "KickerEditor.h"

KickerEditor::KickerEditor (juce::AudioProcessor& processor)
    : AudioProcessorEditor (processor)
{
    setSize (270, 290);
}

void KickerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff303030));
    g.setColour (juce::Colour (0xff909090));
    g.setFont (13.0f);
    g.drawText ("Fancy Kicker", getLocalBounds(), juce::Justification::centred);
}
