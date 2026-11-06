#include "CompProcessor.h"
#include "CompEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
constexpr float mixFloorDb = -60.0f;

juce::NormalisableRange<float> skewedRange (float high, float interval, float centre)
{
    juce::NormalisableRange<float> range (0.0f, high, interval);
    range.setSkewForCentre (centre);
    return range;
}

std::unique_ptr<juce::AudioParameterFloat> timeParameter (const juce::String& id, const juce::String& name, float high, float centre, float defaultMs)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, skewedRange (high, 0.1f, centre), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); }));
}

juce::String filterText (float hz)
{
    if (hz < 1000.0f)
        return juce::String (juce::roundToInt (hz)) + " Hz";

    const auto kilohertz = hz / 1000.0f;
    return (kilohertz >= 10.0f ? juce::String (juce::roundToInt (kilohertz)) : fancy::formatFixed (kilohertz, 1)) + " kHz";
}

std::unique_ptr<juce::AudioParameterFloat> filterParameter (const juce::String& id, const juce::String& name, float defaultHz)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, fancy::logRange (20.0f, 24000.0f, 1.0f), defaultHz,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return filterText (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));
}

juce::String ratioText (float ratio)
{
    if (ratio >= (float) CompDsp::infiniteRatio)
        return "inf:1";

    return (std::abs (ratio - std::round (ratio)) < 0.05f ? juce::String (juce::roundToInt (ratio)) : fancy::formatFixed (ratio, 1)) + ":1";
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    fancy::FancyProcessor::Layout layout;

    layout.add (fancy::levelParameter ("threshold", "Threshold", -60.0f, 0.0f, 0.0f, -100.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ratio", 1 }, "Ratio", fancy::logRange (1.0f, (float) CompDsp::infiniteRatio, 0.01f), 4.0f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return ratioText (value); })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          {
                                              return text.containsIgnoreCase ("inf") ? (float) CompDsp::infiniteRatio : text.getFloatValue();
                                          })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "knee", 1 }, "Knee", skewedRange (24.0f, 0.01f, 6.0f), 0.5f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); })));

    layout.add (toggle ("weird", "Weird", false));
    layout.add (timeParameter ("attack", "Attack", 500.0f, 30.0f, 3.0f));
    layout.add (toggle ("classic", "Classic", false));
    layout.add (timeParameter ("release", "Release", 5000.0f, 300.0f, 200.0f));
    layout.add (toggle ("auto", "Auto", false));
    layout.add (timeParameter ("precomp", "Pre-comp", 250.0f, 30.0f, 0.0f));
    layout.add (timeParameter ("rms", "RMS size", 250.0f, 30.0f, 5.0f));
    layout.add (filterParameter ("lowpass", "Lowpass", 24000.0f));
    layout.add (filterParameter ("highpass", "Highpass", 20.0f));
    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, mixFloorDb, mixFloorDb));

    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "detector", 1 }, "Detector input",
        juce::StringArray { "Main input L+R", "Main L", "Main R", "Sidechain L+R", "Sidechain L", "Sidechain R", "Mid", "Side" }, 0));

    layout.add (toggle ("link", "Link channels", true));
    layout.add (toggle ("makeup", "Auto make-up", false));
    layout.add (toggle ("preview", "Preview filter", false));
    layout.add (toggle ("limit", "Limit output", false));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling", juce::StringArray { "Off", "2x", "4x" }, 0));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "osPhase", 1 }, "Oversampling filter", juce::StringArray { "Linear", "Minimum" }, 0));
    layout.add (toggle ("feedback", "Feedback detection", false));
    layout.add (fancy::percentParameter ("linkAmount", "Link amount", 100.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "releaseCurve", 1 }, "Release curve", juce::StringArray { "Exponential", "Linear" }, 0));
    layout.add (toggle ("attack90", "Attack = 90 % rise", false));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    return {
        { "Default", {} },
        { "Gentle Glue", { { "threshold", -18.0f }, { "ratio", 2.0f }, { "knee", 6.0f }, { "attack", 30.0f }, { "release", 300.0f }, { "makeup", 1.0f } } },
        { "Vocal Control", { { "threshold", -20.0f }, { "ratio", 4.0f }, { "knee", 3.0f }, { "attack", 5.0f }, { "release", 120.0f }, { "makeup", 1.0f } } },
        { "Drum Punch", { { "threshold", -16.0f }, { "ratio", 4.0f }, { "attack", 25.0f }, { "release", 100.0f }, { "rms", 0.0f }, { "highpass", 80.0f } } },
        { "Parallel Crush", { { "threshold", -36.0f }, { "ratio", 10.0f }, { "attack", 1.0f }, { "release", 80.0f }, { "wet", -6.0f }, { "dry", 0.0f }, { "makeup", 1.0f } } },
        { "Brickwall", { { "threshold", -6.0f }, { "ratio", (float) CompDsp::infiniteRatio }, { "attack", 0.5f }, { "release", 80.0f }, { "precomp", 2.0f }, { "rms", 0.0f }, { "limit", 1.0f } } },
    };
}
} // namespace

CompProcessor::CompProcessor()
    : FancyProcessor ("Fancy Comp", "FancyComp",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout(), createPresets())
{
}

float CompProcessor::read (const char* id)
{
    return getApvts().getRawParameterValue (id)->load (std::memory_order_relaxed);
}

bool CompProcessor::isOn (const char* id)
{
    return read (id) >= 0.5f;
}

CompDsp::Targets CompProcessor::readTargets()
{
    CompDsp::Targets targets;
    targets.thresholdDb = (double) read ("threshold");
    targets.ratio = (double) read ("ratio");
    targets.kneeDb = (double) read ("knee");
    targets.attackMs = (double) read ("attack");
    targets.releaseMs = (double) read ("release");
    targets.preCompMs = (double) read ("precomp");
    targets.rmsMs = (double) read ("rms");
    targets.lowpassHz = (double) read ("lowpass");
    targets.highpassHz = (double) read ("highpass");
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.linkAmount = (double) read ("linkAmount") * 0.01;
    targets.detector = juce::roundToInt (read ("detector"));
    targets.oversampling = juce::roundToInt (read ("oversampling"));
    targets.weirdKnee = isOn ("weird");
    targets.classic = isOn ("classic");
    targets.autoRelease = isOn ("auto");
    targets.linearRelease = isOn ("releaseCurve");
    targets.attack90 = isOn ("attack90");
    targets.minimumPhase = isOn ("osPhase");
    targets.linkChannels = isOn ("link");
    targets.feedback = isOn ("feedback");
    targets.autoMakeup = isOn ("makeup");
    targets.previewFilter = isOn ("preview");
    targets.limitOutput = isOn ("limit");
    targets.bypass = isOn ("bypass");
    return targets;
}

void CompProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());
}

void CompProcessor::reset()
{
    dsp.reset();
}

bool CompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);

    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

void CompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    auto sidechainBuffer = getBusBuffer (buffer, true, 1);

    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = mainBuffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto main = mainBuffer.getArrayOfWritePointers();
    const auto numSidechain = juce::jmin (sidechainBuffer.getNumChannels(), 2);

    inputMeter.push (main, numChannels, numSamples);

    dsp.setTargets (readTargets());
    dsp.process (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples);

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

juce::AudioProcessorEditor* CompProcessor::createEditor()
{
    return new CompEditor (*this);
}
