#include "KickerProcessor.h"
#include "KickerEditor.h"

KickerProcessor::KickerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void KickerProcessor::prepareToPlay (double, int) {}

void KickerProcessor::releaseResources() {}

bool KickerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void KickerProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
}

juce::AudioProcessorEditor* KickerProcessor::createEditor()
{
    return new KickerEditor (*this);
}

void KickerProcessor::getStateInformation (juce::MemoryBlock&) {}

void KickerProcessor::setStateInformation (const void*, int) {}
