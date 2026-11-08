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

    workflow = std::make_unique<Workflow> (*this);
}

void FancyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    compare.prepare (sampleRate);
    prepareAudio (sampleRate, samplesPerBlock);
}

void FancyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();
    const auto inputs = juce::jmin (getMainBusNumInputChannels(), buffer.getNumChannels());
    const auto outputs = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels());

    compare.pushInput (buffer.getArrayOfReadPointers(), inputs, numSamples);
    processAudio (buffer, midi);
    compare.process (buffer.getArrayOfWritePointers(), outputs, numSamples, getLatencySamples());
}

juce::AudioProcessorParameter* FancyProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void FancyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    presets.addToState (state);
    addToState (state);

    if (juce::MessageManager::existsAndIsCurrentThread())
        workflow->commit();

    state.appendChild (workflow->createSlotsTree(), nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FancyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto tree = juce::ValueTree::fromXml (*xml);

        if (applyState (tree))
        {
            presets.stateRestored();

            // A host's state is never an undo step. Version 1 has no slots: B starts as A.
            workflow->restoreSlots (tree.getChildWithName (Workflow::slotsType));
            workflow->rebase();
        }
    }
}

bool FancyProcessor::applyState (const juce::ValueTree& tree)
{
    if (! tree.isValid() || tree.getType() != apvts.state.getType())
        return false;

    auto clean = tree.createCopy();

    for (int i = clean.getNumChildren(); --i >= 0;)
    {
        auto child = clean.getChild (i);

        // The A/B slots belong to the workflow, which setStateInformation gives them to.
        if (child.hasType (Workflow::slotsType))
        {
            clean.removeChild (i, nullptr);
            continue;
        }

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

    // Migration (version 3): a parameter the state doesn't mention takes its default, so an
    // older state, or a preset saved before a parameter existed, sounds as it was saved.
    for (auto* parameter : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (ranged == nullptr || parameter == getBypassParameter())
            continue;

        const auto id = ranged->getParameterID();

        if (clean.getChildWithProperty ("id", id).isValid())
            continue;

        juce::ValueTree child ("PARAM");
        child.setProperty ("id", id, nullptr);
        child.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
        clean.appendChild (child, nullptr);
    }

    apvts.replaceState (clean);
    stateApplied();
    return true;
}

} // namespace fancy
