#include "MultiCompProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiCompProcessor();
}
