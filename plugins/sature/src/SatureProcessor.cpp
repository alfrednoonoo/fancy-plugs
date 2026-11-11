#include "SatureProcessor.h"
#include "SatureEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
juce::String signedDb (float value)
{
    const auto text = fancy::formatFixed (value, 1);
    return (value > 0.0f && text != "0.0" ? "+" : "") + text + " dB";
}

std::unique_ptr<juce::AudioParameterFloat> dbParameter (const char* id, const char* name, float low, float high, float defaultDb)
{
    return fancy::floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return signedDb (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }));
}

bool isOffText (const juce::String& text)
{
    return text.trim().startsWithIgnoreCase ("off");
}

// Log-scaled, with an Off position past one end: below 20 Hz for Low, above 20 kHz for High.
juce::NormalisableRange<float> offRange (float start, float end, float onLow, float onHigh, float offValue)
{
    return { start, end,
             [] (float s, float e, float normalised) { return s * std::pow (e / s, normalised); },
             [] (float s, float e, float value) { return std::log (juce::jlimit (s, e, value) / s) / std::log (e / s); },
             [onLow, onHigh, offValue] (float, float, float value)
             {
                 if (value < onLow || value > onHigh)
                     return offValue;

                 return juce::jlimit (onLow, onHigh, std::round (value));
             } };
}

std::unique_ptr<juce::AudioParameterFloat> bandParameter (const char* id, const char* name, float start, float end,
                                                          float onLow, float onHigh, float offValue)
{
    return fancy::floatParameter (
        id, name, offRange (start, end, onLow, onHigh, offValue), offValue,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([onLow, onHigh] (float value, int)
                                          { return value < onLow || value > onHigh ? juce::String ("Off") : fancy::formatFilterHz (value); })
            .withValueFromStringFunction ([onLow, onHigh, offValue] (const juce::String& text)
                                          { return isOffText (text) ? offValue : juce::jlimit (onLow, onHigh, fancy::parseHz (text)); }));
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    layout.add (std::make_unique<Choice> (juce::ParameterID { "type", 1 }, "Type", juce::StringArray { "Tape", "Tube", "Transistor", "Clip" }, 0));
    layout.add (dbParameter ("drive", "Drive", 0.0f, 36.0f, 6.0f));

    layout.add (fancy::floatParameter (
        "character", "Character", juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), -30.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int) { return SatureProcessor::characterText (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return SatureProcessor::parseCharacter (text); })));

    layout.add (dbParameter ("tone", "Tone", -6.0f, 6.0f, 0.0f));

    layout.add (bandParameter ("low", "Low", SatureProcessor::lowOffHz, SatureProcessor::lowMaxHz,
                               SatureProcessor::lowMinHz, SatureProcessor::lowMaxHz, SatureProcessor::lowOffHz));
    layout.add (bandParameter ("high", "High", SatureProcessor::highMinHz, SatureProcessor::highOffHz,
                               SatureProcessor::highMinHz, SatureProcessor::highMaxHz, SatureProcessor::highOffHz));

    layout.add (toggle ("autogain", "Auto gain", true));

    layout.add (fancy::levelParameter ("wet", "Wet", SatureProcessor::mixFloorDb, 12.0f, 0.0f, SatureProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", SatureProcessor::mixFloorDb, 12.0f, SatureProcessor::mixFloorDb, SatureProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));

    // These change the latency, so they are deliberate settings, not something automation flips.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling",
                                          juce::StringArray { "Off", "2x", "4x", "8x" }, 2, ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "osfilter", 1 }, "Oversampling filter",
                                          juce::StringArray { "Minimum phase", "Linear phase" }, 0, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (dbParameter ("trim", "Input trim", -24.0f, 24.0f, 0.0f));
    layout.add (toggle ("dcfilter", "DC filter", true));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float tube = 1.0f, transistor = 2.0f, clip = 3.0f;

    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Tape warmth", { { "drive", 9.0f }, { "character", -20.0f }, { "tone", -1.0f } } },
        { "Tube glow", { { "type", tube }, { "drive", 8.0f }, { "character", -40.0f } } },
        { "Transistor edge", { { "type", transistor }, { "drive", 12.0f }, { "character", 20.0f }, { "tone", 1.0f } } },
        { "Clip it", { { "type", clip }, { "drive", 10.0f }, { "character", 0.0f } } },
        { "Bass drive", { { "type", tube }, { "drive", 14.0f }, { "character", -50.0f }, { "high", 1000.0f } } },
        { "Top sparkle", { { "drive", 12.0f }, { "character", 30.0f }, { "low", 2000.0f }, { "wet", -6.0f }, { "dry", 0.0f } } },
        { "Parallel crush", { { "type", clip }, { "drive", 30.0f }, { "character", 0.0f }, { "wet", -6.0f }, { "dry", 0.0f } } },
    };

    fancy::setCategories (presets, {
        { "Tape warmth", "Mix" },
        { "Tube glow", "Vocals" },
        { "Transistor edge", "Drums" },
        { "Clip it", "Drums" },
        { "Bass drive", "Bass" },
        { "Top sparkle", "Mix" },
        { "Parallel crush", "Drums" },
    });

    return presets;
}
} // namespace

juce::String SatureProcessor::characterText (float value)
{
    const auto percent = juce::roundToInt (value);

    if (percent == 0)
        return "neutral";

    return (percent < 0 ? "even " : "odd ") + juce::String (std::abs (percent)) + " %";
}

float SatureProcessor::parseCharacter (const juce::String& text)
{
    const auto lower = text.trim().toLowerCase();

    if (lower.startsWith ("neutral"))
        return 0.0f;

    const auto magnitude = std::abs (fancy::parseNumber (lower));

    if (lower.contains ("even"))
        return -magnitude;

    if (lower.contains ("odd"))
        return magnitude;

    return fancy::parseNumber (lower);
}

SatureProcessor::SatureProcessor()
    : FancyProcessor ("Fancy Sature", "FancySature",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets())
{
}

float SatureProcessor::read (const char* parameterId) const
{
    return const_cast<SatureProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool SatureProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

SatureDsp::Targets SatureProcessor::readTargets() const
{
    SatureDsp::Targets targets;

    const auto low = read ("low");
    const auto high = read ("high");

    targets.type = juce::roundToInt (read ("type"));
    targets.driveDb = (double) read ("drive");
    targets.character = (double) read ("character") / 100.0;
    targets.toneDb = (double) read ("tone");
    targets.lowOn = low >= lowMinHz;
    targets.lowHz = (double) juce::jmax (low, lowMinHz);
    targets.highOn = high <= highMaxHz;
    targets.highHz = (double) juce::jmin (high, highMaxHz);
    targets.autoGain = isOn ("autogain");
    targets.trimGain = fancy::dbToGain ((double) read ("trim"));
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.oversampling = juce::roundToInt (read ("oversampling"));
    targets.minimumPhase = ! isOn ("osfilter");
    targets.dcFilter = isOn ("dcfilter");
    targets.listen = isListening();
    targets.bypass = isOn ("bypass");

    return targets;
}

void SatureProcessor::prepareAudio (double sampleRate, int)
{
    dsp.setTargets (readTargets());
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.getLatencySamples());
}

void SatureProcessor::reset()
{
    dsp.reset();
}

bool SatureProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void SatureProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (getMainBusNumInputChannels(), buffer.getNumChannels(), 2);
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

juce::AudioProcessorEditor* SatureProcessor::createEditor()
{
    return new SatureEditor (*this);
}
