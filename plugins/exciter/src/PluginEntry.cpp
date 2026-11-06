#include "ExciterProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExciterProcessor();
}
