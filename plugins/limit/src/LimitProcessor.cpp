#include "LimitProcessor.h"
#include "LimitEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
std::unique_ptr<juce::AudioParameterFloat> dbParameter (const char* id, const char* name, float low, float high, float defaultDb)
{
    return fancy::floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }));
}

juce::String timeText (float ms)
{
    return (ms < 10.0f ? fancy::formatFixed (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms";
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

    layout.add (dbParameter ("threshold", "Threshold", -30.0f, 0.0f, 0.0f));
    layout.add (dbParameter ("ceiling", "Ceiling", -12.0f, 0.0f, -1.0f));

    layout.add (fancy::floatParameter (
        "release", "Release", fancy::logRange (1.0f, 1000.0f, 0.1f), 80.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return timeText (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    layout.add (fancy::floatParameter (
        "lookahead", "Lookahead", juce::NormalisableRange<float> (0.0f, 5.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    layout.add (fancy::percentParameter ("link", "Link", 100.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "style", 1 }, "Style", juce::StringArray { "Clean", "Punchy", "Dense" }, 0));

    // Display only: the marker on the LUFS bar.
    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "target", 1 }, "Target",
        juce::StringArray { juce::String::fromUTF8 ("Streaming \xc2\xb7 -14 LUFS"), juce::String::fromUTF8 ("Podcast \xc2\xb7 -16 LUFS"),
                            juce::String::fromUTF8 ("Broadcast \xc2\xb7 -23 LUFS"), juce::String::fromUTF8 ("Custom\xe2\x80\xa6") },
        0, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (fancy::floatParameter (
        "customTarget", "Custom target", juce::NormalisableRange<float> (-36.0f, -5.0f, 0.1f), -14.0f,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("LUFS")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " LUFS"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (toggle ("truePeak", "True peak", true));
    layout.add (toggle ("autoRelease", "Auto release", true));
    layout.add (toggle ("listen", "Listen to removed", false));

    // These change the latency, so they are deliberate settings, not something automation flips.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling",
                                          juce::StringArray { "Off", "2x", "4x", "8x" }, 2, ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "osPhase", 1 }, "Oversampling filter",
                                          juce::StringArray { "Minimum phase", "Linear phase" }, 0, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "dither", 1 }, "Dither",
                                          juce::StringArray { "Off", "16-bit", "16-bit, shaped", "24-bit", "24-bit, shaped" }, 0));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "ballistics", 1 }, "Meter ballistics",
                                          juce::StringArray { "EBU", "RMS" }, 0, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    // Phase 4 Wave 1 (docs/limit.md), appended. Off and Exponential are the old sound.
    layout.add (toggle ("gainLock", "Gain lock", false));
    layout.add (fancy::floatParameter (
        "gainLockRef", "Gain lock reference", juce::NormalisableRange<float> (-30.0f, 0.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (toggle ("dcFilter", "DC filter", false));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "releaseCurve", 1 }, "Release curve",
                                          juce::StringArray { "Exponential", "Linear" }, 0));

    // Phase 4 Wave 2, appended (docs/limit.md): the clipper before the limiter.
    layout.add (toggle ("clip", "Clip before limiting", false));
    layout.add (dbParameter ("clipAmount", "Clip", 0.0f, 12.0f, 3.0f));

    // Phase 4 Wave 3, appended (docs/limit.md, "multiband limiting"): one band is the old sound.
    // Bands and the crossover type change the latency or the phase in a step, so, as Oversampling,
    // they're deliberate settings that automation doesn't flip.
    const auto dot = juce::String::charToString (0x00b7);

    layout.add (std::make_unique<Choice> (juce::ParameterID { "bands", 1 }, "Bands", juce::StringArray { "1", "2", "3", "4" }, 0,
                                          ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "crossoverType", 1 }, "Crossover",
                                          juce::StringArray { "LR 24 dB " + dot + " Min phase", "LR 48 dB " + dot + " Min phase" }, 0,
                                          ChoiceAttributes {}.withAutomatable (false)));

    const float crossoverDefaults[] { 150.0f, 2500.0f, 8000.0f };

    for (int i = 0; i < 3; ++i)
        layout.add (fancy::floatParameter (
            LimitProcessor::crossoverId (i), "Crossover " + juce::String (i + 1), fancy::logRange (20.0f, 20000.0f, 0.1f), crossoverDefaults[i],
            juce::AudioParameterFloatAttributes {}
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    const float releaseDefaults[] { 120.0f, 80.0f, 50.0f, 40.0f };

    for (int i = 0; i < 4; ++i)
        layout.add (fancy::floatParameter (
            LimitProcessor::bandReleaseId (i), "Band " + juce::String (i + 1) + " release", fancy::logRange (1.0f, 1000.0f, 0.1f), releaseDefaults[i],
            juce::AudioParameterFloatAttributes {}
                .withLabel ("ms")
                .withStringFromValueFunction ([] (float value, int) { return timeText (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float punchyStyle = 1.0f, denseStyle = 2.0f, broadcastTarget = 2.0f, shaped16 = 2.0f;

    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Streaming master", { { "threshold", -6.0f }, { "ceiling", -1.0f } } },
        { "Loud and dense", { { "style", denseStyle }, { "threshold", -10.0f }, { "ceiling", -1.0f } } },
        { "Broadcast safe", { { "threshold", -3.0f }, { "ceiling", -2.0f }, { "target", broadcastTarget } } },
        { "Drum bus catch", { { "style", punchyStyle }, { "threshold", -4.0f }, { "ceiling", -0.3f }, { "lookahead", 1.0f } } },
        { "Transparent peak guard", { { "threshold", 0.0f }, { "ceiling", -0.3f } } },
        { "16-bit bounce", { { "ceiling", -0.3f }, { "dither", shaped16 } } },
    };

    fancy::setCategories (presets, {
        { "Streaming master", "Mastering" },
        { "Loud and dense", "Mastering" },
        { "Broadcast safe", "Mastering" },
        { "Drum bus catch", "Mixing" },
        { "Transparent peak guard", "Mixing" },
        { "16-bit bounce", "Utility" },
    });

    return presets;
}
} // namespace

LimitProcessor::LimitProcessor()
    : FancyProcessor ("Fancy Limit", "FancyLimit",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets())
{
}

float LimitProcessor::read (const char* parameterId) const
{
    return const_cast<LimitProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool LimitProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

LimitDsp::Targets LimitProcessor::readTargets() const
{
    LimitDsp::Targets targets;

    targets.thresholdDb = (double) read ("threshold");
    targets.ceilingDb = (double) read ("ceiling");
    targets.releaseMs = (double) read ("release");
    targets.lookaheadMs = (double) read ("lookahead");
    targets.link = (double) read ("link") / 100.0;
    targets.style = juce::roundToInt (read ("style"));
    targets.truePeak = isOn ("truePeak");
    targets.autoRelease = isOn ("autoRelease");
    targets.listen = isOn ("listen");
    targets.oversampling = juce::roundToInt (read ("oversampling"));
    targets.linearPhase = isOn ("osPhase");
    targets.dither = juce::roundToInt (read ("dither"));
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.bypass = isOn ("bypass");

    targets.gainLock = isOn ("gainLock");
    targets.gainLockRefDb = (double) read ("gainLockRef");
    targets.dcFilter = isOn ("dcFilter");
    targets.linearRelease = isOn ("releaseCurve");
    targets.clip = isOn ("clip");                          // Phase 4 Wave 2
    targets.clipAmountDb = (double) read ("clipAmount");

    // Phase 4 Wave 3.
    targets.bands = juce::jlimit (1, LimitDsp::maxBands, juce::roundToInt (read ("bands")) + 1);
    targets.crossoverLr48 = juce::roundToInt (read ("crossoverType")) == 1;

    // Fixed IDs: this runs on the audio thread every block, so no juce::String is built here.
    static constexpr const char* crossoverIds[] { "crossover1", "crossover2", "crossover3" };
    static constexpr const char* releaseIds[] { "bandRelease1", "bandRelease2", "bandRelease3", "bandRelease4" };

    for (int i = 0; i < LimitDsp::maxBands - 1; ++i)
        targets.crossoverHz[(size_t) i] = (double) read (crossoverIds[i]);

    for (int i = 0; i < LimitDsp::maxBands; ++i)
        targets.bandReleaseMs[(size_t) i] = (double) read (releaseIds[i]);

    return targets;
}

float LimitProcessor::getTargetLufs() const
{
    static constexpr float presetLufs[] { -14.0f, -16.0f, -23.0f };

    const auto target = juce::roundToInt (read ("target"));
    return target == custom ? read ("customTarget") : presetLufs[juce::jlimit (0, 2, target)];
}

void LimitProcessor::resetLoudness() noexcept
{
    dsp.loudness.requestReset();
    dsp.truePeakMeter.requestReset();
    dsp.requestOversReset();
}

void LimitProcessor::prepareAudio (double sampleRate, int)
{
    dsp.setTargets (readTargets());
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.getLatencySamples());
}

void LimitProcessor::reset()
{
    dsp.reset();
}

bool LimitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void LimitProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

void LimitProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

juce::AudioProcessorEditor* LimitProcessor::createEditor()
{
    return new LimitEditor (*this);
}
