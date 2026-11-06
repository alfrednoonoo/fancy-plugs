#include "KickerProcessor.h"
#include "KickerEditor.h"

#include <fancy/Decibels.h>
#include <fancy/plugin/ParamFormat.h>

namespace
{
constexpr float levelFloorDb = -60.0f;

juce::NormalisableRange<float> logRange (float low, float high, float interval)
{
    return { low, high,
             [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
             [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); },
             [interval] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value / interval) * interval); } };
}

std::unique_ptr<juce::AudioParameterFloat> levelParameter (const juce::String& id, const juce::String& name, float low, float high, float defaultDb, float floorDb)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([floorDb] (float value, int) { return fancy::formatDb (value, floorDb); })
            .withValueFromStringFunction ([floorDb] (const juce::String& text) { return fancy::parseDb (text, floorDb); }));
}

fancy::FancyProcessor::Layout createLayout()
{
    fancy::FancyProcessor::Layout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "freq", 1 }, "Freq", logRange (20.0f, 240.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    layout.add (levelParameter ("wet", "Wet", levelFloorDb, 12.0f, -12.0f, levelFloorDb));
    layout.add (levelParameter ("dry", "Dry", levelFloorDb, 12.0f, 0.0f, levelFloorDb));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "q", 1 }, "Filter Q", logRange (0.5f, 10.0f, 0.01f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2); })));

    layout.add (levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "monoBelow", 1 }, "Mono below Freq", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "softClip", 1 }, "Soft clip", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    return {
        { "Default", {} },
        { "Tight Kick", { { "freq", 60.0f }, { "wet", -9.0f }, { "q", 1.5f } } },
        { "Deep Sub", { { "freq", 42.0f }, { "wet", -6.0f }, { "q", 3.0f }, { "monoBelow", 1.0f } } },
        { "Bass Lift", { { "freq", 90.0f }, { "wet", -12.0f }, { "q", 1.0f } } },
        { "Safe Boom", { { "freq", 50.0f }, { "wet", 0.0f }, { "dry", -3.0f }, { "softClip", 1.0f } } },
    };
}

double levelToGain (float db)
{
    return db <= levelFloorDb ? 0.0 : fancy::dbToGain ((double) db);
}
} // namespace

KickerProcessor::KickerProcessor()
    : FancyProcessor ("Fancy Kicker", "FancyKicker",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets()),
      freq (*getApvts().getRawParameterValue ("freq")),
      wet (*getApvts().getRawParameterValue ("wet")),
      dry (*getApvts().getRawParameterValue ("dry")),
      q (*getApvts().getRawParameterValue ("q")),
      output (*getApvts().getRawParameterValue ("output")),
      monoBelow (*getApvts().getRawParameterValue ("monoBelow")),
      softClip (*getApvts().getRawParameterValue ("softClip")),
      bypass (*getApvts().getRawParameterValue ("bypass"))
{
}

KickerDsp::Targets KickerProcessor::readTargets() const
{
    KickerDsp::Targets targets;
    targets.frequencyHz = (double) freq.load (std::memory_order_relaxed);
    targets.q = (double) q.load (std::memory_order_relaxed);
    targets.wetGain = levelToGain (wet.load (std::memory_order_relaxed));
    targets.dryGain = levelToGain (dry.load (std::memory_order_relaxed));
    targets.outputGain = fancy::dbToGain ((double) output.load (std::memory_order_relaxed));
    targets.monoWet = monoBelow.load (std::memory_order_relaxed) >= 0.5f;
    targets.softClip = softClip.load (std::memory_order_relaxed) >= 0.5f;
    targets.bypass = bypass.load (std::memory_order_relaxed) >= 0.5f;
    return targets;
}

void KickerProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (0);
}

void KickerProcessor::reset()
{
    dsp.reset();
}

bool KickerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void KickerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
}

juce::AudioProcessorEditor* KickerProcessor::createEditor()
{
    return new KickerEditor (*this);
}
