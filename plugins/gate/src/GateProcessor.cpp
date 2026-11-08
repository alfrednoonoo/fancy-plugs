#include "GateProcessor.h"
#include "GateEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>
#include <cstring>

namespace
{
constexpr float mixFloorDb = -60.0f, noiseFloorDb = -90.0f;

juce::NormalisableRange<float> skewedRange (float low, float high, float interval, float centre)
{
    juce::NormalisableRange<float> range (low, high, interval);
    range.setSkewForCentre (centre);
    return range;
}

juce::String timeText (float ms)
{
    return (ms < 100.0f ? fancy::formatFixed (ms, ms < 10.0f ? 1 : 0) : juce::String (juce::roundToInt (ms))) + " ms";
}

std::unique_ptr<juce::AudioParameterFloat> timeParameter (const char* id, const char* name, float low, float high, float centre, float defaultMs,
                                                          bool automatable = true)
{
    return fancy::floatParameter (
        id, name, skewedRange (low, high, 0.1f, centre), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (automatable)
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return timeText (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
}

std::unique_ptr<juce::AudioParameterFloat> dbParameter (const char* id, const char* name, float low, float high, float defaultDb)
{
    return fancy::floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }));
}

std::unique_ptr<juce::AudioParameterFloat> filterParameter (const char* id, const char* name, float low, float high, float defaultHz, bool offAtTop)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 1.0f), defaultHz,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([offAtTop, low, high] (float value, int)
            {
                const auto off = offAtTop ? value >= high : value <= low;
                return off ? juce::String ("Off") : fancy::formatFilterHz (value);
            })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));
}

// A4, C#3, or a plain number.
int parseNote (const juce::String& text)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const auto trimmed = text.trim().toUpperCase();

    if (trimmed.isNotEmpty() && juce::CharacterFunctions::isDigit (trimmed[0]))
        return juce::jlimit (0, 127, trimmed.getIntValue());

    for (int pitch = 11; pitch >= 0; --pitch)
    {
        if (trimmed.startsWith (names[pitch]))
        {
            const auto octave = (int) std::lround (fancy::parseNumber (trimmed.substring ((int) std::strlen (names[pitch]))));
            return juce::jlimit (0, 127, (octave + 1) * 12 + pitch);
        }
    }

    return 69;
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using BoolAttributes = juce::AudioParameterBoolAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    layout.add (dbParameter ("threshold", "Threshold", -90.0f, 0.0f, -40.0f));

    layout.add (fancy::floatParameter (
        "hysteresis", "Hysteresis", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return (value > 0.0f ? "+" : "") + fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    // Changes the latency, so it's a deliberate setting rather than something automation flips.
    layout.add (std::make_unique<Bool> (juce::ParameterID { "lookahead", 1 }, "Lookahead", false, BoolAttributes {}.withAutomatable (false)));

    layout.add (fancy::floatParameter (
        "preopen", "Pre-open", juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f), 5.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return timeText (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    layout.add (timeParameter ("attack", "Attack", 0.0f, 500.0f, 20.0f, 0.5f));
    layout.add (timeParameter ("hold", "Hold", 0.0f, 2000.0f, 150.0f, 20.0f));
    layout.add (timeParameter ("release", "Release", 1.0f, 5000.0f, 300.0f, 100.0f));

    layout.add (fancy::levelParameter ("range", "Range", (float) GateDsp::rangeFloorDb, 0.0f, (float) GateDsp::rangeFloorDb, (float) GateDsp::rangeFloorDb));
    layout.add (timeParameter ("rms", "RMS size", 0.0f, 250.0f, 30.0f, 0.0f));

    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "detector", 1 }, "Detector input",
        juce::StringArray { "Main input L+R", "Main L", "Main R", "Sidechain L+R", "Sidechain L", "Sidechain R" }, 0));

    layout.add (filterParameter ("lowpass", "Lowpass", 20.0f, (float) GateDsp::lowpassOffHz, (float) GateDsp::lowpassOffHz, true));
    layout.add (filterParameter ("highpass", "Highpass", (float) GateDsp::highpassOffHz, 5000.0f, (float) GateDsp::highpassOffHz, false));

    layout.add (toggle ("preview", "Preview filter", false));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Gate", "Duck" }, 0));
    layout.add (toggle ("link", "Link channels", true));

    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, mixFloorDb, mixFloorDb));
    layout.add (fancy::levelParameter ("noise", "Noise", noiseFloorDb, 0.0f, noiseFloorDb, noiseFloorDb));

    layout.add (toggle ("midi", "MIDI on open/close", false));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "midiNote", 1 }, "MIDI note", 0, 127, 69,
        juce::AudioParameterIntAttributes {}
            .withStringFromValueFunction ([] (int value, int) { return GateProcessor::noteName (value); })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseNote (text); })));

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "midiChannel", 1 }, "MIDI channel", 1, 16, 1));
    layout.add (toggle ("velocity", "Velocity from level", false));

    layout.add (fancy::floatParameter (
        "velWindow", "Velocity window", juce::NormalisableRange<float> (1.0f, 5.0f, 0.5f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float duck = 1.0f, sidechainLR = 3.0f;

    return {
        { "Default", {} },
        { "Vocal clean-up", { { "threshold", -45.0f }, { "hysteresis", -6.0f }, { "attack", 2.0f }, { "hold", 60.0f }, { "release", 200.0f },
                              { "range", -18.0f }, { "highpass", 100.0f } } },
        { "Kick tight", { { "threshold", -30.0f }, { "attack", 0.5f }, { "hold", 30.0f }, { "release", 80.0f }, { "lowpass", 150.0f } } },
        { juce::String::fromUTF8 ("Snare (with spill) \xc2\xb7 lookahead"), { { "lookahead", 1.0f }, { "preopen", 2.0f }, { "threshold", -28.0f }, { "hold", 40.0f },
                                              { "release", 120.0f }, { "highpass", 150.0f }, { "lowpass", 6000.0f } } },
        { juce::String::fromUTF8 ("Tom (with spill) \xc2\xb7 lookahead"), { { "lookahead", 1.0f }, { "preopen", 2.0f }, { "threshold", -32.0f }, { "hold", 80.0f },
                                            { "release", 300.0f }, { "lowpass", 400.0f } } },
        { "Duck under kick", { { "mode", duck }, { "detector", sidechainLR }, { "threshold", -30.0f }, { "range", -8.0f }, { "attack", 1.0f },
                               { "hold", 20.0f }, { "release", 150.0f } } },
        { "Drum trigger to MIDI", { { "midi", 1.0f }, { "velocity", 1.0f }, { "threshold", -24.0f }, { "hold", 20.0f }, { "release", 50.0f } } },
        { "Gentle expander", { { "threshold", -50.0f }, { "hysteresis", -6.0f }, { "range", -12.0f }, { "attack", 5.0f }, { "release", 250.0f } } },
    };
}
} // namespace

juce::String GateProcessor::noteName (int note)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

GateProcessor::GateProcessor()
    : FancyProcessor ("Fancy Gate", "FancyGate",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout(), createPresets())
{
}

float GateProcessor::read (const char* parameterId) const
{
    return const_cast<GateProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool GateProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

GateDsp::Targets GateProcessor::readTargets() const
{
    GateDsp::Targets targets;

    targets.thresholdDb = (double) read ("threshold");
    targets.hysteresisDb = (double) read ("hysteresis");
    targets.lookahead = isOn ("lookahead");
    targets.preOpenMs = (double) read ("preopen");
    targets.attackMs = (double) read ("attack");
    targets.holdMs = (double) read ("hold");
    targets.releaseMs = (double) read ("release");
    targets.rangeDb = (double) read ("range");
    targets.rmsMs = (double) read ("rms");
    targets.detector = juce::roundToInt (read ("detector"));
    targets.lowpassHz = (double) read ("lowpass");
    targets.highpassHz = (double) read ("highpass");
    targets.preview = isOn ("preview");
    targets.duck = isOn ("mode");
    targets.link = isOn ("link");
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.noiseGain = fancy::levelToGain (read ("noise"), noiseFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.midi = isOn ("midi");
    targets.note = juce::roundToInt (read ("midiNote"));
    targets.channel = juce::roundToInt (read ("midiChannel"));
    targets.velocityFromLevel = isOn ("velocity");
    targets.velocityWindowMs = (double) read ("velWindow");
    targets.bypass = isOn ("bypass");

    return targets;
}

void GateProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());
}

void GateProcessor::reset()
{
    dsp.reset();
}

bool GateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GateProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;

    midi.clear();

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
    dsp.process (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples, &midi);

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

void GateProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

juce::AudioProcessorEditor* GateProcessor::createEditor()
{
    return new GateEditor (*this);
}
