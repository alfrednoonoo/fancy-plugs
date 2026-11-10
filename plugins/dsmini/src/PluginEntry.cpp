#include "DsMiniProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DsMiniProcessor();
}
