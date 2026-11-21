#include "KickerProcessor.h"
#include "KickerEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>
#include <vector>

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

    // Phase 4 Wave 1 (docs/kicker.md), appended. Off, off, 60 Hz, off, off and Off are Kicker as it was.
    layout.add (fancy::floatParameter (
        "subLen", "Sub length", fancy::logRange (20.0f, 2000.0f, 1.0f), 2000.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return value >= 1999.5f ? juce::String ("Off") : juce::String (juce::roundToInt (value)) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return text.trim().equalsIgnoreCase ("off") ? 2000.0f : juce::jlimit (20.0f, 2000.0f, fancy::parseMs (text)); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "roomCut", 1 }, "Make-room", false));
    layout.add (hzParameter ("roomFreq", "Make-room freq", 20.0f, 200.0f, 60.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "subPolarity", 1 }, "Invert sub", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "autoGain", 1 }, "Auto gain", false));
    layout.add (fancy::floatParameter (
        "ceiling", "Ceiling", juce::NormalisableRange<float> ((float) fancy::dsp::OutputCeiling::offDb, 0.0f, 0.1f), (float) fancy::dsp::OutputCeiling::offDb,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("dBTP")
            .withStringFromValueFunction ([] (float value, int)
                                          { return value <= (float) fancy::dsp::OutputCeiling::offDb + 0.01f ? juce::String ("Off") : fancy::formatFixed (value, 1) + " dBTP"; })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          { return text.trim().equalsIgnoreCase ("off") ? (float) fancy::dsp::OutputCeiling::offDb : juce::jlimit (-12.0f, 0.0f, fancy::parseNumber (text)); })));

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
    analysisThread.startThread (juce::Thread::Priority::low);
}

KickerProcessor::~KickerProcessor()
{
    analysisThread.stopThread (2000);
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

    const auto length = read ("subLen");
    targets.subLengthMs = length >= 1999.5f ? 0.0 : (double) length;
    targets.roomCut = read ("roomCut") >= 0.5f;
    targets.roomHz = (double) read ("roomFreq");
    targets.subPolarity = read ("subPolarity") >= 0.5f;
    return targets;
}

void KickerProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (0);

    ceiling.prepare (sampleRate);
    autoTrim.setTime (200.0, sampleRate);
    autoTrim.snapTo (1.0);
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

    const auto targets = readTargets();
    dsp.setTargets (targets);
    dsp.process (channels, numChannels, numSamples);

    // Phase 4 Wave 1: Auto gain's trim, then the ceiling. Off and settled, nothing is touched.
    const auto read = [this] (const char* id) { return getApvts().getRawParameterValue (id)->load (std::memory_order_relaxed); };
    const auto autoOn = read ("autoGain") >= 0.5f;

    if (autoOn && isNonRealtime())
    {
        bool stale;
        {
            const juce::SpinLock::ScopedLockType lock (analysisLock);
            stale = ! analysed || analysedKey != keyFor (targets);
        }

        if (stale)
            runAnalysis();
    }

    autoTrim.setTarget (autoOn ? std::pow (10.0, -(double) autoGainDb.load (std::memory_order_relaxed) / 20.0) : 1.0);

    if (autoOn || ! autoTrim.isSettled())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto trim = (float) autoTrim.next();

            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= trim;
        }
    }

    const auto ceilingDb = (double) read ("ceiling");
    ceiling.process (channels, numChannels, numSamples, ceilingDb, targets.bypass);

    outputMeter.push (channels, numChannels, numSamples);

    if (getLatencySamples() != ceiling.getLatencySamples (ceilingDb))
        setLatencySamples (ceiling.getLatencySamples (ceilingDb));
}

juce::AudioProcessorEditor* KickerProcessor::createEditor()
{
    return new KickerEditor (*this);
}

// Phase 4 Wave 1: Auto gain's measure (docs/kicker.md) ------------------------------------------

KickerProcessor::AnalysisKey KickerProcessor::keyFor (const KickerDsp::Targets& t) noexcept
{
    return { t.frequencyHz, t.q, t.wetGain, t.dryGain, t.monoWet ? 1.0 : 0.0, t.softClip ? 1.0 : 0.0, t.sub ? 1.0 : 0.0, t.subCutoffHz,
             (double) t.subDivisions, t.subGain, t.subSensitivityGain, t.subLowpassHz, t.subLengthMs, t.roomCut ? t.roomHz : 0.0 };
}

// A reference kick: a body falling from 120 to 50 Hz with a 150 ms decay, every half second,
// peaking at -6 dBFS. The gain is the output's RMS against the input's over the last second.
float KickerProcessor::measureGainDb (const KickerDsp::Targets& settings)
{
    constexpr double rate = 48000.0;
    constexpr double pi = 3.14159265358979323846;

    auto targets = settings;
    targets.outputGain = 1.0;
    targets.bypass = false;

    const auto total = (int) (2.0 * rate);
    std::vector<float> left ((size_t) total), right ((size_t) total);
    auto phase = 0.0;

    for (int i = 0; i < total; ++i)
    {
        const auto t = std::fmod ((double) i / rate, 0.5);
        const auto hz = 50.0 + 70.0 * std::exp (-t / 0.03);

        if (t < 1.0 / rate)
            phase = 0.0;

        phase += 2.0 * pi * hz / rate;
        left[(size_t) i] = right[(size_t) i] = (float) (0.5 * std::exp (-t / 0.15) * std::sin (phase));
    }

    const auto input = left;

    KickerDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    for (int start = 0; start < total; start += 512)
    {
        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, std::min (512, total - start));
    }

    double in = 0.0, out = 0.0;

    for (int i = total / 2; i < total; ++i)
    {
        in += (double) input[(size_t) i] * (double) input[(size_t) i];
        out += (double) left[(size_t) i] * (double) left[(size_t) i];
    }

    return (float) (10.0 * std::log10 (std::max (out, 1.0e-30) / std::max (in, 1.0e-30)));
}

void KickerProcessor::runAnalysis()
{
    const auto targets = readTargets();
    const auto key = keyFor (targets);
    autoGainDb.store (measureGainDb (targets), std::memory_order_relaxed);

    const juce::SpinLock::ScopedLockType lock (analysisLock);
    analysedKey = key;
    analysed = true;
}

void KickerProcessor::AnalysisThread::run()
{
    while (! threadShouldExit())
    {
        const auto autoOn = processor.getApvts().getRawParameterValue ("autoGain")->load (std::memory_order_relaxed) >= 0.5f;
        bool stale;
        {
            const juce::SpinLock::ScopedLockType lock (processor.analysisLock);
            stale = ! processor.analysed || processor.analysedKey != keyFor (processor.readTargets());
        }

        if (autoOn && stale && ! processor.isNonRealtime())
            processor.runAnalysis();

        wait (100);
    }
}
