#include "ChorusProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusProcessor();
}
