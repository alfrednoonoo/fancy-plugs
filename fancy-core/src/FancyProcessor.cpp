#include <fancy/plugin/FancyProcessor.h>

#include <cmath>

namespace fancy
{

FancyProcessor::FancyProcessor (const juce::String& pluginName,
                                const juce::Identifier& stateType,
                                const BusesProperties& buses,
                                Layout layout,
                                std::vector<FactoryPreset> factoryPresets)
    : AudioProcessor (buses),
      name (pluginName),
      apvts (*this, nullptr, stateType, std::move (layout)),
      presets (*this, std::move (factoryPresets))
{
    // The value tree seeds its raw values without snapping them to each parameter's grid, so
    // the first write from a host would shift them slightly and change the audio. Writing
    // them here means a parameter already holds the value any later write would give it.
    for (auto* parameter : getParameters())
        parameter->setValueNotifyingHost (parameter->getValue());
}

juce::AudioProcessorParameter* FancyProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void FancyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FancyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        applyState (juce::ValueTree::fromXml (*xml));
}

bool FancyProcessor::applyState (const juce::ValueTree& tree)
{
    if (! tree.isValid() || tree.getType() != apvts.state.getType())
        return false;

    auto clean = tree.createCopy();

    for (int i = clean.getNumChildren(); --i >= 0;)
    {
        auto child = clean.getChild (i);

        // Anything else belongs to the plugin, which checks it in stateApplied().
        if (! child.hasType ("PARAM"))
            continue;

        const auto id = child.getProperty ("id").toString();

        if (apvts.getParameter (id) == nullptr)
        {
            clean.removeChild (i, nullptr);
            continue;
        }

        const auto value = (double) child.getProperty ("value");

        if (! std::isfinite (value))
        {
            clean.removeChild (i, nullptr);
            continue;
        }

        const auto range = apvts.getParameterRange (id);
        child.setProperty ("value", juce::jlimit ((double) range.start, (double) range.end, value), nullptr);
    }

    apvts.replaceState (clean);
    stateApplied();
    return true;
}

} // namespace fancy
