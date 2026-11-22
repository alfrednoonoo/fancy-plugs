#include "ChorusProcessor.h"
#include "ChorusEditor.h"

#include <fancy/plugin/ParamHelpers.h>

namespace
{
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

std::unique_ptr<juce::AudioParameterFloat> filterParameter (const char* id, const char* name, float low, float high, float defaultHz, bool offAtTop)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 1.0f), defaultHz,
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
    using Attributes = juce::AudioParameterFloatAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode",
                                          juce::StringArray { "Classic", "Ensemble", "Dimension", "Doubler", "Flange" }, (int) ChorusDsp::ensemble));

    layout.add (fancy::floatParameter ("rate", "Rate", fancy::logRange (0.05f, 10.0f, 0.01f), 0.8f,
                                       Attributes {}
                                           .withLabel ("Hz")
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2) + " Hz"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    juce::StringArray notes;

    for (int i = 0; i < ChorusProcessor::numNotes; ++i)
        notes.add (ChorusProcessor::noteName (i));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "syncRate", 1 }, "Rate (synced)", notes, ChorusProcessor::defaultNote));

    layout.add (fancy::percentParameter ("depth", "Depth", 45.0f));
    layout.add (msParameter ("delay", "Delay", (float) ChorusDsp::minDelayMs, (float) ChorusDsp::maxDelayMs, 7.0f));

    layout.add (fancy::floatParameter ("feedback", "Feedback", juce::NormalisableRange<float> (-90.0f, 90.0f, 1.0f), 0.0f,
                                       Attributes {}
                                           .withLabel ("%")
                                           .withStringFromValueFunction ([] (float value, int)
                                                                         {
                                                                             const auto whole = juce::roundToInt (value);
                                                                             return (whole > 0 ? "+" : "") + juce::String (whole) + " %";
                                                                         })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::floatParameter ("voices", "Voices", juce::NormalisableRange<float> (1.0f, (float) ChorusDsp::maxVoices, 1.0f), 3.0f,
                                       Attributes {}
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::percentParameter ("spread", "Spread", 60.0f));
    layout.add (fancy::percentParameter ("width", "Width", 100.0f));
    layout.add (filterParameter ("lowCut", "Low cut", ChorusProcessor::lowCutOffHz, ChorusProcessor::lowCutMaxHz, 150.0f, false));
    layout.add (filterParameter ("highCut", "High cut", ChorusProcessor::highCutMinHz, ChorusProcessor::highCutOffHz, 9000.0f, true));
    layout.add (fancy::percentParameter ("age", "Age", 25.0f));

    layout.add (toggle ("sync", "Tempo sync", false));
    layout.add (toggle ("bassMono", "Keep bass mono", true));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "lfoShape", 1 }, "LFO shape", juce::StringArray { "Sine", "Triangle", "Smooth random" }, 0));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "input", 1 }, "Input", juce::StringArray { "Stereo", "Mono" }, 0));

    layout.add (fancy::floatParameter ("lfoPhase", "LFO phase", juce::NormalisableRange<float> (0.0f, 180.0f, 1.0f), 90.0f,
                                       Attributes {}
                                           .withLabel (juce::String::fromUTF8 ("\xc2\xb0"))
                                           .withStringFromValueFunction ([] (float value, int)
                                                                         { return juce::String (juce::roundToInt (value)) + juce::String::fromUTF8 ("\xc2\xb0"); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::percentParameter ("ducking", "Ducking", 0.0f));

    layout.add (fancy::levelParameter ("wet", "Wet", ChorusProcessor::mixFloorDb, 12.0f, -3.0f, ChorusProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", ChorusProcessor::mixFloorDb, 12.0f, 0.0f, ChorusProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr auto classic = (float) ChorusDsp::classic, ensemble = (float) ChorusDsp::ensemble, dimension = (float) ChorusDsp::dimension,
                   doubler = (float) ChorusDsp::doubler, flange = (float) ChorusDsp::flange;

    // Classic and Dimension's two voices live here only: changing Mode never writes Voices
    // (docs/chorus.md, "Decisions").
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Classic chorus", { { "mode", classic }, { "voices", 2.0f } } },
        { "Wide pad", { { "mode", ensemble }, { "voices", 5.0f } } },
        { "Dimension width", { { "mode", dimension }, { "voices", 2.0f }, { "depth", 25.0f } } },
        { "Vocal double", { { "mode", doubler } } },
        { "Slow flange", { { "mode", flange }, { "feedback", 60.0f }, { "rate", 0.2f } } },
        { "Vibrato", { { "mode", classic }, { "voices", 1.0f }, { "rate", 5.0f }, { "wet", 0.0f }, { "dry", ChorusProcessor::mixFloorDb } } },
        { "Lo-fi wobble", { { "mode", classic }, { "age", 80.0f }, { "rate", 0.3f } } },
    };

    fancy::setCategories (presets, {
        { "Classic chorus", "Instruments" },
        { "Wide pad", "Instruments" },
        { "Dimension width", "Mix" },
        { "Vocal double", "Vocals" },
        { "Slow flange", "Creative" },
        { "Vibrato", "Creative" },
        { "Lo-fi wobble", "Creative" },
    });

    return presets;
}
} // namespace

juce::String ChorusProcessor::noteName (int noteIndex)
{
    const char* const names[] { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
    const char* const feels[] { "", " dotted", " triplet" };
    const auto index = juce::jlimit (0, numNotes - 1, noteIndex);
    return juce::String (names[index / 3]) + feels[index % 3];
}

double ChorusProcessor::noteBeats (int noteIndex)
{
    const auto index = juce::jlimit (0, numNotes - 1, noteIndex);
    const auto beats = 16.0 / (double) (1 << (index / 3));
    const auto feel = index % 3;
    return beats * (feel == 1 ? 1.5 : feel == 2 ? 2.0 / 3.0 : 1.0);
}

ChorusProcessor::ChorusProcessor()
    : FancyProcessor ("Fancy Chorus", "FancyChorus",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets())
{
}

float ChorusProcessor::read (const char* parameterId) const
{
    return const_cast<ChorusProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool ChorusProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

ChorusDsp::Targets ChorusProcessor::readTargets() const
{
    ChorusDsp::Targets targets;

    const auto lowCut = read ("lowCut");
    const auto highCut = read ("highCut");

    targets.mode = juce::roundToInt (read ("mode"));
    targets.rateHz = (double) read ("rate");
    targets.sync = isOn ("sync");
    targets.syncBeats = noteBeats (juce::roundToInt (read ("syncRate")));
    targets.bpm = tempo.load (std::memory_order_relaxed);
    targets.playing = playing.load (std::memory_order_relaxed);
    targets.ppq = ppq.load (std::memory_order_relaxed);
    targets.depth = (double) read ("depth") * 0.01;
    targets.delayMs = (double) read ("delay");
    targets.feedback = (double) read ("feedback") * 0.01;
    targets.voices = juce::roundToInt (read ("voices"));
    targets.spread = (double) read ("spread") * 0.01;
    targets.width = (double) read ("width") * 0.01;
    targets.lowCutOn = lowCut > lowCutOffHz + 0.5f;
    targets.lowCutHz = (double) lowCut;
    targets.highCutOn = highCut < highCutOffHz - 0.5f;
    targets.highCutHz = (double) highCut;
    targets.age = (double) read ("age") * 0.01;
    targets.bassMono = isOn ("bassMono");
    targets.shape = juce::roundToInt (read ("lfoShape"));
    targets.monoInput = juce::roundToInt (read ("input")) == 1;
    targets.lfoPhaseDeg = (double) read ("lfoPhase");
    targets.ducking = (double) read ("ducking") * 0.01;
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.listen = isListening();
    targets.bypass = isOn ("bypass");

    return targets;
}

void ChorusProcessor::updateTransport()
{
    auto isPlaying = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                tempo.store (juce::jlimit (20.0, 999.0, *bpm), std::memory_order_relaxed);

            if (const auto quarterNotes = position->getPpqPosition())
            {
                ppq.store (*quarterNotes, std::memory_order_relaxed);
                isPlaying = position->getIsPlaying();
            }
        }
    }

    playing.store (isPlaying, std::memory_order_relaxed);
}

void ChorusProcessor::prepareAudio (double sampleRate, int)
{
    updateTransport();
    dsp.setTargets (readTargets());
    dsp.prepare (sampleRate);
    setLatencySamples (0);
}

void ChorusProcessor::reset()
{
    dsp.reset();
}

bool ChorusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void ChorusProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto channels = mainBuffer.getArrayOfWritePointers();

    inputMeter.push (channels, numChannels, numSamples);

    updateTransport();
    dsp.setTargets (readTargets());
    dsp.process (channels, numChannels, numSamples);

    outputMeter.push (channels, numChannels, numSamples);
}

juce::AudioProcessorEditor* ChorusProcessor::createEditor()
{
    return new ChorusEditor (*this);
}
