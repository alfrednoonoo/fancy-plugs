#include "TsProcessor.h"
#include "TsEditor.h"

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

std::unique_ptr<juce::AudioParameterFloat> msParameter (const char* id, const char* name, float low, float high, float defaultMs)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 0.1f), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int)
                                          { return (value < 10.0f ? fancy::formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
}

std::unique_ptr<juce::AudioParameterFloat> filterParameter (const char* id, const char* name, float low, float high, bool offAtTop)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 1.0f), offAtTop ? high : low,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([offAtTop, low, high] (float value, int)
                                          { return (offAtTop ? value >= high : value <= low) ? juce::String ("Off") : fancy::formatFilterHz (value); })
            .withValueFromStringFunction ([offAtTop, low, high] (const juce::String& text)
                                          { return text.trim().startsWithIgnoreCase ("off") ? (offAtTop ? high : low) : fancy::parseHz (text); }));
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

    layout.add (dbParameter ("attack", "Attack", -24.0f, 24.0f, 0.0f));
    layout.add (msParameter ("speed", "Speed", 1.0f, 50.0f, 8.0f));
    layout.add (dbParameter ("sustain", "Sustain", -24.0f, 24.0f, 0.0f));
    layout.add (msParameter ("length", "Length", 20.0f, 1000.0f, 180.0f));
    layout.add (fancy::percentParameter ("pump", "Pump", 0.0f));
    layout.add (fancy::percentParameter ("sensitivity", "Sensitivity", 60.0f));
    layout.add (filterParameter ("highpass", "Highpass", TsProcessor::highpassOffHz, TsProcessor::highpassMaxHz, false));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "contour", 1 }, "Contour", juce::StringArray { "Sharp", "Medium", "Smooth" }, 1));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "detector", 1 }, "Detector",
                                          juce::StringArray { "Main input L+R", "Sidechain L+R", "Mid", "Side" }, 0));

    layout.add (toggle ("clip", "Clip output", false));
    layout.add (toggle ("link", "Link channels", true));

    layout.add (fancy::levelParameter ("wet", "Wet", TsProcessor::mixFloorDb, 12.0f, 0.0f, TsProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", TsProcessor::mixFloorDb, 12.0f, TsProcessor::mixFloorDb, TsProcessor::mixFloorDb));

    // These change the latency, so they are deliberate settings, not something automation flips.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "lookahead", 1 }, "Lookahead",
                                          juce::StringArray { "Off", "1 ms", "2 ms", "5 ms" }, 0, ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling",
                                          juce::StringArray { "Off", "2x", "4x" }, 1, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "channels", 1 }, "Channels", juce::StringArray { "Stereo", "Mid only", "Side only" }, 0));
    layout.add (filterParameter ("lowpass", "Lowpass", TsProcessor::lowpassMinHz, TsProcessor::lowpassOffHz, true));

    layout.add (fancy::floatParameter (
        "deadband", "Dead band", juce::NormalisableRange<float> (0.0f, 6.0f, 0.1f), 1.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float sharp = 0.0f, smooth = 2.0f;

    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Snare snap", { { "attack", 8.0f }, { "speed", 5.0f } } },
        { "Kick punch", { { "attack", 6.0f }, { "sustain", -3.0f } } },
        { "Room tame", { { "sustain", -8.0f }, { "length", 300.0f } } },
        { "Tighten loop", { { "attack", 3.0f }, { "sustain", -6.0f }, { "contour", sharp } } },
        { "Guitar pluck", { { "attack", 5.0f }, { "speed", 15.0f } } },
        { "Sustain bloom", { { "sustain", 6.0f }, { "length", 500.0f }, { "contour", smooth } } },
        { "Pump", { { "pump", 60.0f }, { "length", 250.0f } } },
    };

    fancy::setCategories (presets, {
        { "Snare snap", "Drums" },
        { "Kick punch", "Drums" },
        { "Room tame", "Drums" },
        { "Tighten loop", "Mix" },
        { "Pump", "Mix" },
        { "Guitar pluck", "Instruments" },
        { "Sustain bloom", "Instruments" },
    });

    return presets;
}
} // namespace

TsProcessor::TsProcessor()
    : FancyProcessor ("Fancy TS", "FancyTS",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout(), createPresets())
{
}

float TsProcessor::read (const char* parameterId) const
{
    return const_cast<TsProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool TsProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

TsDsp::Targets TsProcessor::readTargets() const
{
    TsDsp::Targets targets;

    const auto highpass = read ("highpass");
    const auto lowpass = read ("lowpass");

    targets.attackDb = (double) read ("attack");
    targets.speedMs = (double) read ("speed");
    targets.sustainDb = (double) read ("sustain");
    targets.lengthMs = (double) read ("length");
    targets.pump = (double) read ("pump") * 0.01;
    targets.sensitivity = (double) read ("sensitivity") * 0.01;
    targets.highpassOn = highpass > highpassOffHz + 0.5f;
    targets.highpassHz = (double) highpass;
    targets.lowpassOn = lowpass < lowpassOffHz - 0.5f;
    targets.lowpassHz = (double) lowpass;
    targets.contour = juce::roundToInt (read ("contour"));
    targets.detector = juce::roundToInt (read ("detector"));
    targets.clip = isOn ("clip");
    targets.link = isOn ("link");
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.lookahead = juce::roundToInt (read ("lookahead"));
    targets.oversampling = juce::roundToInt (read ("oversampling"));
    targets.channels = juce::roundToInt (read ("channels"));
    targets.deadbandDb = (double) read ("deadband");
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.listen = isListening();
    targets.bypass = isOn ("bypass");

    return targets;
}

void TsProcessor::prepareAudio (double sampleRate, int)
{
    dsp.setTargets (readTargets());
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.getLatencySamples());
}

void TsProcessor::reset()
{
    dsp.reset();
}

bool TsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    if (layouts.inputBuses.size() < 2)
        return true;

    const auto sidechain = layouts.getChannelSet (true, 1);
    return sidechain.isDisabled() || sidechain == juce::AudioChannelSet::mono() || sidechain == juce::AudioChannelSet::stereo();
}

void TsProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto numSidechain = getBusCount (true) > 1 ? juce::jmin (getBusBuffer (buffer, true, 1).getNumChannels(), 2) : 0;
    auto sidechainBuffer = numSidechain > 0 ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    const auto channels = mainBuffer.getArrayOfWritePointers();

    inputMeter.push (channels, numChannels, numSamples);

    dsp.setTargets (readTargets());
    dsp.process (channels, numChannels, numSidechain > 0 ? sidechainBuffer.getArrayOfReadPointers() : nullptr, numSidechain, numSamples);

    outputMeter.push (channels, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

juce::AudioProcessorEditor* TsProcessor::createEditor()
{
    return new TsEditor (*this);
}
