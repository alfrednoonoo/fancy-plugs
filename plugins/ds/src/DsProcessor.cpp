#include "DsProcessor.h"
#include "DsEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
constexpr int designIntervalMs = 30;

std::unique_ptr<juce::AudioParameterFloat> dbParameter (const char* id, const char* name, float low, float high, float defaultDb, int decimals = 1)
{
    return fancy::floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([decimals] (float value, int) { return fancy::formatFixed (value, decimals) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }));
}

std::unique_ptr<juce::AudioParameterFloat> msParameter (const char* id, const char* name, float low, float high, float defaultMs)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 0.1f), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return (value < 10.0f ? fancy::formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
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

    layout.add (dbParameter ("threshold", "Threshold", -60.0f, 0.0f, -28.0f));

    layout.add (fancy::floatParameter (
        "freq", "Freq", fancy::logRange ((float) DsDsp::minHz, (float) DsDsp::maxHz, 1.0f), 6200.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value / 1000.0f, 2) + " kHz"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    layout.add (fancy::floatParameter (
        "width", "Width", juce::NormalisableRange<float> (0.3f, 3.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("oct")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " oct"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "shape", 1 }, "Detection", juce::StringArray { "Band", "Shelf" }, 0));
    layout.add (dbParameter ("range", "Range", -24.0f, 0.0f, -12.0f, 0));
    layout.add (msParameter ("attack", "Attack", 0.1f, 20.0f, 0.5f));
    layout.add (msParameter ("release", "Release", 5.0f, 500.0f, 60.0f));

    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "detector", 1 }, "Detector",
        juce::StringArray { "Main input L+R", "Main Mid", "Main Side", "Main L", "Main R", "Sidechain L+R", "Sidechain L", "Sidechain R" }, 0));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Split", "Wide" }, 0));
    layout.add (toggle ("relative", "Relative threshold", true));
    layout.add (toggle ("link", "Link channels", true));

    // These change the latency, so they're deliberate settings, not something automation flips.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "lookahead", 1 }, "Lookahead",
                                          juce::StringArray { "Off", "1 ms", "2 ms", "5 ms" }, 0, ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "splitPhase", 1 }, "Split filter",
                                          juce::StringArray { "Minimum phase", "Linear phase" }, 0, ChoiceAttributes {}.withAutomatable (false)));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float shelf = 1.0f, wide = 1.0f, lookahead2 = 2.0f;

    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Gentle vocal", { { "threshold", -24.0f }, { "range", -6.0f } } },
        { "Strong vocal", { { "threshold", -32.0f }, { "range", -15.0f }, { "release", 40.0f } } },
        { "Bright female vocal", { { "freq", 7500.0f }, { "width", 1.2f } } },
        { "Deep male vocal", { { "freq", 5000.0f }, { "width", 1.0f } } },
        { "Podcast voice", { { "threshold", -26.0f }, { "range", -9.0f }, { "lookahead", lookahead2 } } },
        { "Harsh cymbals", { { "shape", shelf }, { "freq", 8000.0f }, { "range", -6.0f }, { "release", 120.0f } } },
        { "Bus taming", { { "mode", wide }, { "shape", shelf }, { "freq", 6000.0f }, { "threshold", -30.0f }, { "range", -4.0f } } },
    };

    fancy::setCategories (presets, {
        { "Gentle vocal", "Vocals" },
        { "Strong vocal", "Vocals" },
        { "Bright female vocal", "Vocals" },
        { "Deep male vocal", "Vocals" },
        { "Podcast voice", "Vocals" },
        { "Harsh cymbals", "Mix" },
        { "Bus taming", "Mix" },
    });

    return presets;
}
} // namespace

DsProcessor::DsProcessor()
    : FancyProcessor ("Fancy DS", "FancyDS",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout(), createPresets()),
      designThread (*this)
{
}

DsProcessor::~DsProcessor()
{
    designThread.stopThread (2000);
}

float DsProcessor::read (const char* parameterId) const
{
    return const_cast<DsProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool DsProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

DsDsp::Targets DsProcessor::readTargets() const
{
    DsDsp::Targets targets;

    targets.thresholdDb = (double) read ("threshold");
    targets.freqHz = (double) read ("freq");
    targets.widthOctaves = (double) read ("width");
    targets.shelf = isOn ("shape");
    targets.rangeDb = (double) read ("range");
    targets.attackMs = (double) read ("attack");
    targets.releaseMs = (double) read ("release");
    targets.detector = juce::roundToInt (read ("detector"));
    targets.wide = isOn ("mode");
    targets.relative = isOn ("relative");
    targets.listen = isListening();
    targets.link = isOn ("link");
    targets.lookahead = juce::roundToInt (read ("lookahead"));
    targets.linearPhase = isOn ("splitPhase");
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.bypass = isOn ("bypass");

    return targets;
}

void DsProcessor::DesignThread::run()
{
    while (! threadShouldExit())
    {
        if (! processor.isNonRealtime())
            processor.dsp.updateLinearDesign (processor.readTargets());

        wait (designIntervalMs);
    }
}

void DsProcessor::prepareAudio (double sampleRate, int)
{
    designThread.stopThread (2000);

    dsp.setTargets (readTargets());
    dsp.setSynchronousDesign (isNonRealtime());
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.getLatencySamples());

    // Always running, as a host can switch between offline and real time without preparing
    // again. It skips its work while offline, where each block designs in-line.
    designThread.startThread (juce::Thread::Priority::low);
}

void DsProcessor::releaseResources()
{
    designThread.stopThread (2000);
}

bool DsProcessor::waitForBackgroundWork (int timeoutMs)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (dsp.isDesignPending (readTargets()))
    {
        if (juce::Time::getMillisecondCounterHiRes() > limit)
            return false;

        designThread.notify();
        juce::Thread::sleep (1);
    }

    return true;
}

void DsProcessor::reset()
{
    dsp.reset();
}

bool DsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);

    return sidechain.isDisabled() || sidechain == juce::AudioChannelSet::mono() || sidechain == juce::AudioChannelSet::stereo();
}

void DsProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    const float* sidechain[2] { numSidechain > 0 ? sidechainBuffer.getReadPointer (0) : nullptr,
                                numSidechain > 1 ? sidechainBuffer.getReadPointer (1) : nullptr };

    inputMeter.push (main, numChannels, numSamples);
    sidechainWatch.update (main, numChannels, sidechain, numSidechain, numSamples, getSampleRate(), read ("detector") >= (float) DsDsp::sideLR - 0.5f);

    dsp.setSynchronousDesign (isNonRealtime());
    dsp.setTargets (readTargets());
    dsp.process (main, numChannels, sidechain, numSidechain, numSamples);

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

void DsProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

juce::AudioProcessorEditor* DsProcessor::createEditor()
{
    return new DsEditor (*this);
}
