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
        juce::StringArray { "Main input L+R", "Main L", "Main R", "Sidechain L+R", "Sidechain L", "Sidechain R", "Mid", "Side",
                            "Host sync" }, 0)); // Phase 4 Wave 2, appended

    layout.add (filterParameter ("lowpass", "Lowpass", 20.0f, (float) GateDsp::lowpassOffHz, (float) GateDsp::lowpassOffHz, true));
    layout.add (filterParameter ("highpass", "Highpass", (float) GateDsp::highpassOffHz, 5000.0f, (float) GateDsp::highpassOffHz, false));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Gate", "Duck",
                                                                                                   "Upward" }, 0)); // Phase 4 Wave 2, appended
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

    // Phase 4 Wave 1 (docs/gate.md), appended. Off, inf:1 and 0 % are the gate as it was.
    layout.add (toggle ("midiIn", "MIDI trigger", false));
    layout.add (fancy::floatParameter (
        "ratio", "Ratio", fancy::logRange (1.1f, (float) GateDsp::infiniteRatio, 0.01f), (float) GateDsp::infiniteRatio,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int)
                                          { return value >= (float) GateDsp::infiniteRatio ? juce::String ("inf:1") : fancy::formatFixed (value, value < 10.0f ? 1 : 0) + ":1"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseRatio (text, (float) GateDsp::infiniteRatio); })));
    layout.add (fancy::percentParameter ("deBleed", "De-bleed", 0.0f));

    // Phase 4 Wave 2, appended (docs/gate.md): Host sync's grid and the pulse's length.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "syncRate", 1 }, "Sync rate", GateProcessor::syncRateNames(), GateProcessor::defaultSyncRate));
    layout.add (fancy::floatParameter ("syncShape", "Sync shape", juce::NormalisableRange<float> (5.0f, 100.0f, 1.0f), 50.0f,
                                       juce::AudioParameterFloatAttributes {}
                                           .withLabel ("%")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    // Phase 4 Wave 2, v1.7.1, appended (docs/gate.md): the Transient detector (item 11) and Upward
    // mode (item 12). Level, and the Gate and Duck modes, are the gate as it was.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "detMode", 1 }, "Detector mode", juce::StringArray { "Level", "Transient" }, 0));
    layout.add (dbParameter ("rise", "Rise", 3.0f, 24.0f, 8.0f));
    layout.add (fancy::floatParameter (
        "boost", "Boost", juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 4.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int) { return (value > 0.0f ? "+" : "") + fancy::formatFixed (value, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (fancy::floatParameter (
        "upRatio", "Up ratio", fancy::logRange (1.1f, 4.0f, 0.01f), 1.5f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + ":1"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseRatio (text, 4.0f); })));
    layout.add (fancy::floatParameter (
        "ceiling", "Ceiling", juce::NormalisableRange<float> ((float) fancy::dsp::OutputCeiling::offDb, 0.0f, 0.1f), (float) fancy::dsp::OutputCeiling::offDb,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("dBTP")
            .withStringFromValueFunction ([] (float value, int)
                                          { return value <= (float) fancy::dsp::OutputCeiling::offDb ? juce::String ("Off") : fancy::formatFixed (value, 1) + " dBTP"; })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          { return text.trim().equalsIgnoreCase ("off") ? (float) fancy::dsp::OutputCeiling::offDb : juce::jlimit (-12.0f, 0.0f, fancy::parseNumber (text)); })));

    // Phase 4 Wave 3, appended (docs/gate.md, "multiband gating", decisions A): one band is the
    // gate as it was. Bands and the crossover type change the phase in a step, so, as Limit's,
    // they're deliberate settings that automation doesn't flip.
    const auto dot = juce::String::charToString (0x00b7);

    layout.add (std::make_unique<Choice> (juce::ParameterID { "bands", 1 }, "Bands", juce::StringArray { "1", "2", "3" }, 0,
                                          juce::AudioParameterChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "crossoverType", 1 }, "Crossover",
                                          juce::StringArray { "LR 24 dB " + dot + " Min phase", "LR 48 dB " + dot + " Min phase" }, 0,
                                          juce::AudioParameterChoiceAttributes {}.withAutomatable (false)));

    const float crossoverDefaults[] { 250.0f, 2500.0f };

    for (int i = 0; i < GateDsp::maxBands - 1; ++i)
        layout.add (fancy::floatParameter (
            GateProcessor::crossoverId (i), "Crossover " + juce::String (i + 1), fancy::logRange (20.0f, 20000.0f, 0.1f), crossoverDefaults[i],
            juce::AudioParameterFloatAttributes {}
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

    const float offsetDefaults[] { 0.0f, 6.0f, 6.0f };

    for (int i = 0; i < GateDsp::maxBands; ++i)
        layout.add (fancy::floatParameter (
            GateProcessor::bandThresholdId (i), "Band " + juce::String (i + 1) + " threshold", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), offsetDefaults[i],
            juce::AudioParameterFloatAttributes {}
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float value, int) { return (value >= 0.0f ? "+" : "") + fancy::formatFixed (value, 1) + " dB"; })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    for (int i = 0; i < GateDsp::maxBands; ++i)
        layout.add (fancy::percentParameter (GateProcessor::bandDepthId (i), "Band " + juce::String (i + 1) + " range", 100.0f));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float duck = 1.0f, sidechainLR = 3.0f;

    std::vector<fancy::FactoryPreset> presets {
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

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Vocal clean-up", "Clean-up" },
        { "Gentle expander", "Clean-up" },
        { "Kick tight", "Drums" },
        // Written as UTF-8 escapes, as the presets' own names are, for the middle dot.
        { "Snare (with spill) \xc2\xb7 lookahead", "Drums" },
        { "Tom (with spill) \xc2\xb7 lookahead", "Drums" },
        { "Duck under kick", "Sidechain" },
        { "Drum trigger to MIDI", "Sidechain" },
    });

    return presets;
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
    targets.preview = isListening();
    targets.duck = juce::roundToInt (read ("mode")) == 1;
    targets.upward = juce::roundToInt (read ("mode")) == 2;
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

    targets.midiIn = isOn ("midiIn");
    targets.ratio = (double) read ("ratio");
    targets.deBleed = (double) read ("deBleed") * 0.01;

    // Phase 4 Wave 2.
    targets.syncStepBeats = syncStepBeats (juce::roundToInt (read ("syncRate")));
    targets.syncShape = juce::jlimit (0.05, 1.0, (double) read ("syncShape") * 0.01);

    // Phase 4 Wave 2, v1.7.1.
    targets.transient = juce::roundToInt (read ("detMode")) == 1;
    targets.riseDb = (double) read ("rise");
    targets.boostDb = (double) read ("boost");
    targets.upRatio = (double) read ("upRatio");

    // Phase 4 Wave 3. Fixed IDs, so nothing builds a String on the audio thread.
    static constexpr const char* crossoverIds[] { "crossover1", "crossover2" };
    static constexpr const char* thresholdIds[] { "bandThreshold1", "bandThreshold2", "bandThreshold3" };
    static constexpr const char* depthIds[] { "bandDepth1", "bandDepth2", "bandDepth3" };

    targets.bands = juce::jlimit (1, GateDsp::maxBands, juce::roundToInt (read ("bands")) + 1);
    targets.crossoverLr48 = juce::roundToInt (read ("crossoverType")) == 1;

    for (int i = 0; i < GateDsp::maxBands - 1; ++i)
        targets.crossoverHz[(size_t) i] = (double) read (crossoverIds[i]);

    for (int i = 0; i < GateDsp::maxBands; ++i)
    {
        targets.bandOffsetDb[(size_t) i] = (double) read (thresholdIds[i]);
        targets.bandDepth[(size_t) i] = (double) read (depthIds[i]) * 0.01;
    }

    return targets;
}

void GateProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (readTargets());
    ceiling.prepare (sampleRate);
    setLatencySamples (totalLatency());
}

int GateProcessor::totalLatency() const noexcept
{
    return dsp.getLatencySamples() + ceiling.getLatencySamples ((double) read ("ceiling"));
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

    // Phase 4 Wave 1: the MIDI trigger's notes, read before the buffer is reused for the
    // gate's own MIDI out. Any note on any channel counts; velocity doesn't (Decision 1 A).
    {
        std::array<GateDsp::IncomingNote, GateDsp::maxIncomingNotes> notes {};
        auto count = 0;

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (count < GateDsp::maxIncomingNotes && (message.isNoteOn() || message.isNoteOff()))
                notes[(size_t) count++] = { metadata.samplePosition, message.isNoteOn() };
        }

        dsp.setIncomingNotes (notes.data(), count);
    }

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
    dsp.setTransport (readPosition()); // Phase 4 Wave 2: Host sync's grid
    dsp.process (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples, &midi);

    // Phase 4 Wave 2, v1.7.1: the output ceiling, out of the path entirely while it's Off.
    ceiling.process (main, numChannels, numSamples, (double) read ("ceiling"), isOn ("bypass"));

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != totalLatency())
        setLatencySamples (totalLatency());
}

void GateProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

// Phase 4 Wave 2: each of 1/1 to 1/16, then dotted, then triplet (Comp's list).
const juce::StringArray& GateProcessor::syncRateNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray list;

        for (const auto* note : { "1/1", "1/2", "1/4", "1/8", "1/16" })
            for (const auto* feel : { "", " dotted", " triplet" })
                list.add (juce::String (note) + feel);

        return list;
    }();

    return names;
}

double GateProcessor::syncStepBeats (int index) noexcept
{
    const auto note = juce::jlimit (0, 14, index) / 3, feel = juce::jlimit (0, 14, index) % 3;
    const auto beats = 4.0 / (double) (1 << note);
    return beats * (feel == 1 ? 1.5 : feel == 2 ? 2.0 / 3.0 : 1.0);
}

// The host's position for Host sync. Without a tempo the trigger runs on at 120 BPM.
fancy::dsp::TempoTrigger::Position GateProcessor::readPosition()
{
    fancy::dsp::TempoTrigger::Position result;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            const auto ppq = position->getPpqPosition();
            const auto bpm = position->getBpm();

            result.hasTempo = bpm.hasValue() && *bpm > 0.0;
            result.bpm = result.hasTempo ? *bpm : fancy::dsp::TempoTrigger::defaultBpm;
            result.playing = position->getIsPlaying() && ppq.hasValue();
            result.ppq = ppq.hasValue() ? *ppq : 0.0;
        }
    }

    return result;
}

juce::AudioProcessorEditor* GateProcessor::createEditor()
{
    return new GateEditor (*this);
}
