#include "ExciterProcessor.h"
#include "ExciterEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>
#include <random>
#include <vector>

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

    // Phase 4 Wave 1 (docs/exciter.md), appended: Off and off are Exciter as it was.
    layout.add (fancy::floatParameter (
        "ceiling", "Ceiling", juce::NormalisableRange<float> ((float) fancy::dsp::OutputCeiling::offDb, 0.0f, 0.1f), (float) fancy::dsp::OutputCeiling::offDb,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("dBTP")
            .withStringFromValueFunction ([] (float value, int)
                                          { return value <= (float) fancy::dsp::OutputCeiling::offDb + 0.01f ? juce::String ("Off") : fancy::formatFixed (value, 1) + " dBTP"; })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          { return text.trim().equalsIgnoreCase ("off") ? (float) fancy::dsp::OutputCeiling::offDb : juce::jlimit (-12.0f, 0.0f, fancy::parseNumber (text)); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "autoGain", 1 }, "Auto gain", false));

    // Phase 4 Wave 2 (docs/exciter.md, items 40 to 43), appended. The defaults are v1.7.0's
    // sound, bit for bit: Harmonics 2 at 0 isn't run, nor Punch at 0, High cut Off, Width 100 %.
    layout.add (fancy::floatParameter (
        "freq2", "Freq 2", fancy::logRange (ExciterProcessor::freq2MinHz, 16000.0f, 1.0f), 400.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFilterHz (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));
    layout.add (fancy::percentParameter ("harmonics2", "Harmonics 2", 0.0f));
    layout.add (fancy::percentParameter ("punch", "Punch", 0.0f));

    // Log-scaled, with Off past the top, as Sature's High.
    layout.add (fancy::floatParameter (
        "highCut", "High cut",
        juce::NormalisableRange<float> (
            ExciterProcessor::highCutMinHz, ExciterProcessor::highCutOffHz,
            [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
            [] (float start, float end, float value) { return std::log (juce::jlimit (start, end, value) / start) / std::log (end / start); },
            [] (float, float, float value)
            {
                return value > ExciterProcessor::highCutMaxHz ? ExciterProcessor::highCutOffHz
                                                              : juce::jlimit (ExciterProcessor::highCutMinHz, ExciterProcessor::highCutMaxHz, std::round (value));
            }),
        ExciterProcessor::highCutOffHz,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int)
                                          { return value > ExciterProcessor::highCutMaxHz ? juce::String ("Off") : fancy::formatFilterHz (value); })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          {
                                              return text.trim().startsWithIgnoreCase ("off")
                                                       ? ExciterProcessor::highCutOffHz
                                                       : juce::jlimit (ExciterProcessor::highCutMinHz, ExciterProcessor::highCutMaxHz, fancy::parseHz (text));
                                          })));

    layout.add (fancy::floatParameter (
        "width", "Width", juce::NormalisableRange<float> (0.0f, 150.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Gentle Air", { { "freq", 6000.0f }, { "clipBoost", 3.0f }, { "harmonics", 30.0f }, { "mixBack", -9.0f } } },
        { "Vocal Presence", { { "freq", 3000.0f }, { "clipBoost", 6.0f }, { "harmonics", 35.0f }, { "mixBack", -9.0f } } },
        { "Drum Snap", { { "freq", 2000.0f }, { "clipBoost", 9.0f }, { "harmonics", 50.0f } } },
        { "Warm Top", { { "freq", 5000.0f }, { "clipBoost", 6.0f }, { "harmonics", 40.0f }, { "character", 0.0f } } },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Gentle Air", "Mix" },
        { "Warm Top", "Mix" },
        { "Vocal Presence", "Vocals" },
        { "Drum Snap", "Drums" },
    });

    return presets;
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
      bypass (*getApvts().getRawParameterValue ("bypass")),
      freq2 (*getApvts().getRawParameterValue ("freq2")),
      harmonics2 (*getApvts().getRawParameterValue ("harmonics2")),
      punch (*getApvts().getRawParameterValue ("punch")),
      highCut (*getApvts().getRawParameterValue ("highCut")),
      width (*getApvts().getRawParameterValue ("width"))
{
    analysisThread.startThread (juce::Thread::Priority::low);
}

ExciterProcessor::~ExciterProcessor()
{
    analysisThread.stopThread (2000);
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
    targets.listen = isListening();

    // Phase 4 Wave 2.
    const auto highCutHz = (double) highCut.load (std::memory_order_relaxed);
    targets.frequency2Hz = (double) freq2.load (std::memory_order_relaxed);
    targets.harmonics2 = (double) harmonics2.load (std::memory_order_relaxed) * 0.01;
    targets.punch = (double) punch.load (std::memory_order_relaxed) * 0.01;
    targets.highCutOn = highCutHz <= (double) highCutMaxHz;
    targets.highCutHz = std::min (highCutHz, (double) highCutMaxHz);
    targets.width = (double) width.load (std::memory_order_relaxed) * 0.01;
    return targets;
}

void ExciterProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());

    ceiling.prepare (sampleRate);
    autoTrim.setTime (200.0, sampleRate);
    autoTrim.snapTo (1.0);
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

    const auto targets = readTargets();
    dsp.setTargets (targets);
    dsp.process (channels, numChannels, numSamples);

    // Phase 4 Wave 1: Auto gain's trim, then the ceiling. Off and settled, nothing is touched.
    const auto autoOn = getApvts().getRawParameterValue ("autoGain")->load (std::memory_order_relaxed) >= 0.5f;

    if (autoOn && isNonRealtime())
    {
        // An offline render measures here, so it doesn't depend on the thread's timing.
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

    const auto ceilingDb = (double) getApvts().getRawParameterValue ("ceiling")->load (std::memory_order_relaxed);
    ceiling.process (channels, numChannels, numSamples, ceilingDb, targets.bypass);

    outputMeter.push (channels, numChannels, numSamples);

    const auto latency = dsp.getLatencySamples() + ceiling.getLatencySamples (ceilingDb);

    if (getLatencySamples() != latency)
        setLatencySamples (latency);
}

juce::AudioProcessorEditor* ExciterProcessor::createEditor()
{
    return new ExciterEditor (*this);
}

// Phase 4 Wave 1: the analysis (docs/exciter.md) ---------------------------------------------

ExciterProcessor::AnalysisKey ExciterProcessor::keyFor (const ExciterDsp::Targets& targets) noexcept
{
    return { targets.frequencyHz, targets.boostGain, targets.harmonics, targets.mixBackGain, targets.character, (double) targets.oversampling,
             targets.frequency2Hz, targets.harmonics2, targets.punch, targets.highCutOn ? targets.highCutHz : -1.0 };
}

ExciterProcessor::Analysis ExciterProcessor::analyse (const ExciterDsp::Targets& settings)
{
    constexpr double rate = 48000.0;
    constexpr double pi = 3.14159265358979323846;

    auto targets = settings;
    targets.outputGain = 1.0;
    targets.bypass = false;
    targets.listen = false;

    const auto render = [&] (std::vector<float>& left, std::vector<float>& right)
    {
        ExciterDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        for (size_t start = 0; start < left.size(); start += 512)
        {
            float* channels[] { left.data() + start, right.data() + start };
            dsp.process (channels, 2, (int) std::min<size_t> (512, left.size() - start));
        }
    };

    Analysis result;

    // THD: a tone an octave above Freq, held below a fifth of the rate so its harmonics count.
    {
        const auto tone = std::min (2.0 * targets.frequencyHz, 0.2 * rate);
        const auto total = (int) (0.3 * rate);
        std::vector<float> left ((size_t) total), right ((size_t) total);

        for (int i = 0; i < total; ++i)
            left[(size_t) i] = right[(size_t) i] = (float) (0.2512 * std::sin (2.0 * pi * tone * i / rate));

        render (left, right);

        const auto count = 8192;
        const auto from = total - count;

        const auto level = [&] (double hz)
        {
            // A Hann-windowed DFT at one frequency.
            double re = 0.0, im = 0.0;

            for (int i = 0; i < count; ++i)
            {
                const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / (count - 1));
                const auto phase = 2.0 * pi * hz * i / rate;
                re += (double) left[(size_t) (from + i)] * window * std::cos (phase);
                im -= (double) left[(size_t) (from + i)] * window * std::sin (phase);
            }

            return std::sqrt (re * re + im * im);
        };

        const auto fundamental = level (tone);
        auto rest = 0.0;

        for (int k = 2; k <= 10 && k * tone < 0.45 * rate; ++k)
            rest += std::pow (level (k * tone), 2.0);

        result.thdPercent = fundamental > 0.0 ? (float) (100.0 * std::sqrt (rest) / fundamental) : 0.0f;
    }

    // Gain on pink noise at -18 dBFS RMS (Paul Kellet's filter on seeded white noise).
    {
        const auto total = (int) (0.6 * rate);
        std::vector<float> left ((size_t) total), right ((size_t) total);
        std::mt19937 random (5);
        std::uniform_real_distribution<double> white (-1.0, 1.0);
        double b0 = 0.0, b1 = 0.0, b2 = 0.0, sum = 0.0;

        for (int i = 0; i < total; ++i)
        {
            const auto w = white (random);
            b0 = 0.99765 * b0 + w * 0.0990460;
            b1 = 0.96300 * b1 + w * 0.2965164;
            b2 = 0.57000 * b2 + w * 1.0526913;
            const auto x = b0 + b1 + b2 + w * 0.1848;
            left[(size_t) i] = (float) x;
            sum += x * x;
        }

        const auto scale = std::pow (10.0, -18.0 / 20.0) / std::sqrt (sum / (double) total);

        for (int i = 0; i < total; ++i)
            left[(size_t) i] = right[(size_t) i] = (float) ((double) left[(size_t) i] * scale);

        const auto input = left;
        render (left, right);

        double in = 0.0, out = 0.0;

        for (int i = total / 2; i < total; ++i)
        {
            in += (double) input[(size_t) i] * (double) input[(size_t) i];
            out += (double) left[(size_t) i] * (double) left[(size_t) i];
        }

        result.gainDb = (float) (10.0 * std::log10 (std::max (out, 1.0e-30) / std::max (in, 1.0e-30)));
    }

    return result;
}

void ExciterProcessor::runAnalysis()
{
    const auto targets = readTargets();
    const auto key = keyFor (targets);
    const auto result = analyse (targets);

    thdPercent.store (result.thdPercent, std::memory_order_relaxed);
    autoGainDb.store (result.gainDb, std::memory_order_relaxed);

    const juce::SpinLock::ScopedLockType lock (analysisLock);
    analysedKey = key;
    analysed = true;
}

void ExciterProcessor::AnalysisThread::run()
{
    while (! threadShouldExit())
    {
        bool stale;
        {
            const juce::SpinLock::ScopedLockType lock (processor.analysisLock);
            stale = ! processor.analysed || processor.analysedKey != keyFor (processor.readTargets());
        }

        if (stale && ! processor.isNonRealtime())
            processor.runAnalysis();

        wait (100);
    }
}
