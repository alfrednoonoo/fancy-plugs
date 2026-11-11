#include "KickerProcessor.h"
#include "KickerEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
constexpr float levelFloorDb = -60.0f;

fancy::FancyProcessor::Layout createLayout()
{
    fancy::FancyProcessor::Layout layout;

    layout.add (fancy::floatParameter (
        "freq", "Freq", fancy::logRange (20.0f, 240.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    layout.add (fancy::levelParameter ("wet", "Wet", levelFloorDb, 12.0f, -12.0f, levelFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", levelFloorDb, 12.0f, 0.0f, levelFloorDb));

    layout.add (fancy::floatParameter (
        "q", "Filter Q", fancy::logRange (0.5f, 10.0f, 0.01f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "monoBelow", 1 }, "Mono below Freq", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "softClip", 1 }, "Soft clip", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Sub mode (docs/kicker-sub.md), appended. A state without them loads in Boost, as before.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Boost", "Sub" }, 0));

    const auto hzParameter = [] (const char* id, const char* name, float low, float high, float defaultHz)
    {
        return fancy::floatParameter (
            id, name, fancy::logRange (low, high, 1.0f), defaultHz,
            juce::AudioParameterFloatAttributes {}
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float value, int) { return fancy::formatFilterHz (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));
    };

    layout.add (hzParameter ("subCutoff", "Cutoff", 40.0f, 200.0f, 100.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "subOctave", 1 }, "Octave",
                                                              juce::StringArray { juce::String::fromUTF8 ("\xe2\x88\x92" "1 oct"), juce::String::fromUTF8 ("\xe2\x88\x92" "2 oct") }, 0));
    layout.add (fancy::levelParameter ("subLevel", "Sub", levelFloorDb, 6.0f, -9.0f, levelFloorDb));

    layout.add (fancy::floatParameter (
        "subSensitivity", "Sensitivity", juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -40.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (hzParameter ("subLowpass", "Sub low-pass", 60.0f, 250.0f, 120.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "subMono", 1 }, "Mono sub", true));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Tight Kick", { { "freq", 60.0f }, { "wet", -9.0f }, { "q", 1.5f } } },
        { "Deep Sub", { { "freq", 42.0f }, { "wet", -6.0f }, { "q", 3.0f }, { "monoBelow", 1.0f } } },
        { "Bass Lift", { { "freq", 90.0f }, { "wet", -12.0f }, { "q", 1.0f } } },
        { "Safe Boom", { { "freq", 50.0f }, { "wet", 0.0f }, { "dry", -3.0f }, { "softClip", 1.0f } } },
        { "Sub: Deep 808", { { "mode", 1.0f }, { "subCutoff", 90.0f }, { "subLevel", 0.0f } } },
        { "Sub: Tight Punch", { { "mode", 1.0f }, { "subLevel", -6.0f }, { "subLowpass", 90.0f }, { "subSensitivity", -30.0f } } },
        { "Sub: Tom Drop", { { "mode", 1.0f }, { "subOctave", 1.0f }, { "subCutoff", 180.0f }, { "subLevel", -6.0f } } },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Tight Kick", "Kick" },
        { "Safe Boom", "Kick" },
        { "Sub: Deep 808", "Kick" },
        { "Sub: Tight Punch", "Kick" },
        { "Sub: Tom Drop", "Kick" },
        { "Deep Sub", "Bass" },
        { "Bass Lift", "Bass" },
    });

    return presets;
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
    targets.wetGain = fancy::levelToGain (wet.load (std::memory_order_relaxed), levelFloorDb);
    targets.dryGain = fancy::levelToGain (dry.load (std::memory_order_relaxed), levelFloorDb);
    targets.outputGain = fancy::dbToGain ((double) output.load (std::memory_order_relaxed));
    targets.monoWet = monoBelow.load (std::memory_order_relaxed) >= 0.5f;
    targets.softClip = softClip.load (std::memory_order_relaxed) >= 0.5f;
    targets.bypass = bypass.load (std::memory_order_relaxed) >= 0.5f;

    const auto read = [this] (const char* id) { return const_cast<KickerProcessor*> (this)->getApvts().getRawParameterValue (id)->load (std::memory_order_relaxed); };

    targets.sub = juce::roundToInt (read ("mode")) == 1;
    targets.subCutoffHz = (double) read ("subCutoff");
    targets.subDivisions = juce::roundToInt (read ("subOctave")) == 1 ? 4 : 2;
    targets.subGain = fancy::levelToGain (read ("subLevel"), levelFloorDb);
    targets.subSensitivityGain = fancy::dbToGain ((double) read ("subSensitivity"));
    targets.subLowpassHz = (double) read ("subLowpass");
    targets.subMono = read ("subMono") >= 0.5f;
    return targets;
}

void KickerProcessor::prepareAudio (double sampleRate, int)
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

void KickerProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
