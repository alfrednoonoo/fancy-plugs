#include "ExciterProcessor.h"
#include "ExciterEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
constexpr float mixFloorDb = -60.0f;

fancy::FancyProcessor::Layout createLayout()
{
    fancy::FancyProcessor::Layout layout;

    layout.add (fancy::floatParameter (
        "freq", "Freq", fancy::logRange (500.0f, 16000.0f, 1.0f), 2000.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " Hz"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    layout.add (fancy::levelParameter ("clipBoost", "Clip Boost", 0.0f, 24.0f, 0.0f, -100.0f));
    layout.add (fancy::percentParameter ("harmonics", "Harmonics", 0.0f));
    layout.add (fancy::levelParameter ("mixBack", "Mix Back", mixFloorDb, 6.0f, -6.0f, mixFloorDb));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "oversampling", 1 }, "Oversampling", juce::StringArray { "Off", "2x", "4x", "8x" }, 2));

    layout.add (fancy::percentParameter ("character", "Character", 50.0f));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    return {
        { "Default", {} },
        { "Gentle Air", { { "freq", 6000.0f }, { "clipBoost", 3.0f }, { "harmonics", 30.0f }, { "mixBack", -9.0f } } },
        { "Vocal Presence", { { "freq", 3000.0f }, { "clipBoost", 6.0f }, { "harmonics", 35.0f }, { "mixBack", -9.0f } } },
        { "Drum Snap", { { "freq", 2000.0f }, { "clipBoost", 9.0f }, { "harmonics", 50.0f } } },
        { "Warm Top", { { "freq", 5000.0f }, { "clipBoost", 6.0f }, { "harmonics", 40.0f }, { "character", 0.0f } } },
    };
}
} // namespace

ExciterProcessor::ExciterProcessor()
    : FancyProcessor ("Fancy Exciter", "FancyExciter",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets()),
      freq (*getApvts().getRawParameterValue ("freq")),
      clipBoost (*getApvts().getRawParameterValue ("clipBoost")),
      harmonics (*getApvts().getRawParameterValue ("harmonics")),
      mixBack (*getApvts().getRawParameterValue ("mixBack")),
      oversampling (*getApvts().getRawParameterValue ("oversampling")),
      character (*getApvts().getRawParameterValue ("character")),
      output (*getApvts().getRawParameterValue ("output")),
      bypass (*getApvts().getRawParameterValue ("bypass"))
{
}

ExciterDsp::Targets ExciterProcessor::readTargets() const
{
    ExciterDsp::Targets targets;
    targets.frequencyHz = (double) freq.load (std::memory_order_relaxed);
    targets.boostGain = fancy::dbToGain ((double) clipBoost.load (std::memory_order_relaxed));
    targets.harmonics = (double) harmonics.load (std::memory_order_relaxed) * 0.01;
    targets.mixBackGain = fancy::levelToGain (mixBack.load (std::memory_order_relaxed), mixFloorDb);
    targets.character = (double) character.load (std::memory_order_relaxed) * 0.01;
    targets.outputGain = fancy::dbToGain ((double) output.load (std::memory_order_relaxed));
    targets.oversampling = juce::roundToInt (oversampling.load (std::memory_order_relaxed));
    targets.bypass = bypass.load (std::memory_order_relaxed) >= 0.5f;
    return targets;
}

void ExciterProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());
}

void ExciterProcessor::reset()
{
    dsp.reset();
}

bool ExciterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void ExciterProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto channels = buffer.getArrayOfWritePointers();

    inputMeter.push (channels, numChannels, numSamples);

    dsp.setTargets (readTargets());
    dsp.process (channels, numChannels, numSamples);

    outputMeter.push (channels, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

juce::AudioProcessorEditor* ExciterProcessor::createEditor()
{
    return new ExciterEditor (*this);
}
