#include "DelayProcessor.h"
#include "DelayEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>

namespace
{
constexpr float mixFloorDb = -60.0f;
constexpr double minBpm = 20.0, maxBpm = 999.0;

juce::String lengthText (float ms)
{
    return (ms < 100.0f ? fancy::formatFixed (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms";
}

juce::String panText (float value)
{
    const auto amount = juce::roundToInt (std::abs (value));

    return amount == 0 ? juce::String ("C") : juce::String (amount) + (value < 0.0f ? " L" : " R");
}

float parsePan (const juce::String& text)
{
    const auto value = fancy::parseNumber (text);
    return text.containsIgnoreCase ("L") ? -std::abs (value) : value;
}

// 1/8 or plain 8 both reach the eighth note, and anything between lands on the nearest.
int parseNote (const juce::String& text)
{
    const auto denominator = fancy::parseNumber (text.containsChar ('/') ? text.fromLastOccurrenceOf ("/", false, false) : text);

    return denominator > 0.0f ? juce::jlimit (0, delay::noteCount - 1, juce::roundToInt (std::log2 (denominator))) : 0;
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;
    using FloatAttributes = juce::AudioParameterFloatAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    juce::StringArray multipliers;

    for (int i = 1; i <= delay::maxMultiplier; ++i)
        multipliers.add (juce::String::charToString (0x00d7) + juce::String (i));

    juce::NormalisableRange<float> lengthRange (0.0f, (float) delay::maxDelayMs, 0.1f);
    lengthRange.setSkewForCentre (500.0f);

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto prefix = "Tap " + juce::String (tap + 1) + " ";

        layout.add (toggle (DelayProcessor::id (tap, "used"), prefix + "Used", tap == 0));
        layout.add (toggle (DelayProcessor::id (tap, "on"), prefix + "On", true));
        layout.add (toggle (DelayProcessor::id (tap, "solo"), prefix + "Solo", false));

        layout.add (std::make_unique<Choice> (juce::ParameterID { DelayProcessor::id (tap, "mode"), 1 }, prefix + "Time mode",
                                              juce::StringArray { "ms", "Note" }, 1));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "ms"), prefix + "Length", lengthRange, 375.0f,
                                           FloatAttributes {}
                                               .withLabel ("ms")
                                               .withStringFromValueFunction ([] (float value, int) { return lengthText (value); })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

        layout.add (std::make_unique<Choice> (juce::ParameterID { DelayProcessor::id (tap, "note"), 1 }, prefix + "Note value",
                                              juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 3,
                                              ChoiceAttributes {}.withValueFromStringFunction ([] (const juce::String& text) { return parseNote (text); })));

        layout.add (std::make_unique<Choice> (juce::ParameterID { DelayProcessor::id (tap, "feel"), 1 }, prefix + "Feel",
                                              juce::StringArray { "Straight", "Dotted", "Triplet" }, 1));

        layout.add (std::make_unique<Choice> (juce::ParameterID { DelayProcessor::id (tap, "mult"), 1 }, prefix + "Multiplier", multipliers, 0));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "fb"), prefix + "Feedback",
                                           juce::NormalisableRange<float> (0.0f, 110.0f, 1.0f), 0.0f,
                                           FloatAttributes {}
                                               .withLabel ("%")
                                               .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "lp"), prefix + "Lowpass",
                                           fancy::logRange (200.0f, (float) DelayDsp::lowpassOffHz, 1.0f), (float) DelayDsp::lowpassOffHz,
                                           FloatAttributes {}
                                               .withLabel ("Hz")
                                               .withStringFromValueFunction ([] (float value, int)
                                               {
                                                   return value >= (float) DelayDsp::lowpassOffHz ? juce::String ("Off") : fancy::formatFilterHz (value);
                                               })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "hp"), prefix + "Highpass",
                                           fancy::logRange ((float) DelayDsp::highpassOffHz, 5000.0f, 1.0f), (float) DelayDsp::highpassOffHz,
                                           FloatAttributes {}
                                               .withLabel ("Hz")
                                               .withStringFromValueFunction ([] (float value, int)
                                               {
                                                   return value <= (float) DelayDsp::highpassOffHz ? juce::String ("Off") : fancy::formatFilterHz (value);
                                               })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); })));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "bits"), prefix + "Resolution",
                                           juce::NormalisableRange<float> (4.0f, (float) DelayDsp::fullBits, 1.0f), (float) DelayDsp::fullBits,
                                           FloatAttributes {}
                                               .withLabel ("bit")
                                               .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " bit"; })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "width"), prefix + "Width",
                                           juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
                                           FloatAttributes {}
                                               .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2); })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

        layout.add (fancy::levelParameter (DelayProcessor::id (tap, "vol"), prefix + "Volume", mixFloorDb, 12.0f, 0.0f, mixFloorDb));

        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "pan"), prefix + "Pan",
                                           juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                           FloatAttributes {}
                                               .withStringFromValueFunction ([] (float value, int) { return panText (value); })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return parsePan (text); })));

        layout.add (toggle (DelayProcessor::id (tap, "ping"), prefix + "Ping-pong", false));
        layout.add (toggle (DelayProcessor::id (tap, "inloop"), prefix + "Filters inside feedback loop", true));
        layout.add (toggle (DelayProcessor::id (tap, "sat"), prefix + "Saturate feedback", false));
    }

    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (toggle ("smooth", "Smooth time changes", true));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    // Phase 3 Q2.3, appended so nothing moves (docs/delay.md). Defaults keep the old sound.
    const auto percent = [] (const juce::String& id, const juce::String& name, float defaultValue)
    {
        return fancy::percentParameter (id, name, defaultValue);
    };

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto prefix = "Tap " + juce::String (tap + 1) + " ";

        layout.add (std::make_unique<Choice> (juce::ParameterID { DelayProcessor::id (tap, "char"), 1 }, prefix + "Character",
                                              juce::StringArray { "Clean", "Tape", "BBD" }, 0));
        layout.add (percent (DelayProcessor::id (tap, "charAmt"), prefix + "Amount", 50.0f));
        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "modRate"), prefix + "Mod rate", fancy::logRange (0.1f, 10.0f, 0.01f), 0.8f,
                                           juce::AudioParameterFloatAttributes {}
                                               .withLabel ("Hz")
                                               .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2) + " Hz"; })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
        layout.add (fancy::floatParameter (DelayProcessor::id (tap, "modDepth"), prefix + "Mod depth", juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes {}
                                               .withLabel ("cents")
                                               .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, value < 10.0f ? 1 : 0) + " cents"; })
                                               .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
        layout.add (percent (DelayProcessor::id (tap, "modRand"), prefix + "Random", 30.0f));
        layout.add (percent (DelayProcessor::id (tap, "diff"), prefix + "Diffusion", 0.0f));
    }

    layout.add (fancy::floatParameter ("duckAmt", "Duck amount", juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes {}
                                           .withLabel ("dB")
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (fancy::levelParameter ("duckThr", "Duck threshold", -60.0f, 0.0f, -24.0f, -100.0f));

    juce::NormalisableRange<float> releaseRange (20.0f, 2000.0f, 1.0f);
    releaseRange.setSkewForCentre (250.0f);
    layout.add (fancy::floatParameter ("duckRel", "Duck release", releaseRange, 250.0f,
                                       juce::AudioParameterFloatAttributes {}
                                           .withLabel ("ms")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " ms"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));
    layout.add (fancy::floatParameter ("swing", "Swing", juce::NormalisableRange<float> (50.0f, 75.0f, 1.0f), 50.0f,
                                       juce::AudioParameterFloatAttributes {}
                                           .withLabel ("%")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    // Exact for states saved before Q2.3 (valueForMissingParameter); new instances are Fast.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "satCurve", 1 }, "Saturate curve", juce::StringArray { "Exact", "Fast" }, 1,
                                          juce::AudioParameterChoiceAttributes {}.withAutomatable (false)));

    return layout;
}

// Choice indices, as the parameters' lists have them.
constexpr float msMode = 0.0f;
constexpr float note4 = 2.0f, note8 = 3.0f;
constexpr float straightFeel = 0.0f, dottedFeel = 1.0f, tripletFeel = 2.0f;

std::vector<fancy::FactoryPreset> createPresets()
{
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Slapback", { { "t1mode", msMode }, { "t1ms", 110.0f }, { "t1fb", 10.0f }, { "t1lp", 5000.0f }, { "wet", -6.0f } } },
        { "1/8 dotted", { { "t1note", note8 }, { "t1feel", dottedFeel }, { "t1fb", 35.0f }, { "t1lp", 6000.0f }, { "wet", -6.0f } } },
        { "1/4 ping-pong", { { "t1note", note4 }, { "t1feel", straightFeel }, { "t1fb", 45.0f }, { "t1ping", 1.0f }, { "t1lp", 7000.0f }, { "wet", -6.0f } } },
        { "Triplet echo", { { "t1note", note8 }, { "t1feel", tripletFeel }, { "t1fb", 40.0f }, { "t1lp", 8000.0f }, { "wet", -6.0f } } },
        { "Lo-fi tape", { { "t1note", note4 }, { "t1feel", straightFeel }, { "t1fb", 50.0f }, { "t1bits", 10.0f }, { "t1lp", 3000.0f }, { "t1hp", 150.0f },
                          { "t1sat", 1.0f }, { "smooth", 0.0f }, { "wet", -6.0f } } },
        { "Haas widener", { { "t1mode", msMode }, { "t1ms", 8.0f }, { "t1pan", -100.0f },
                            { "t2used", 1.0f }, { "t2mode", msMode }, { "t2ms", 13.0f }, { "t2pan", 100.0f }, { "wet", -6.0f } } },

        // Phase 3 Q2.3.
        { "Tape Echo", { { "t1note", note4 }, { "t1feel", dottedFeel }, { "t1fb", 45.0f }, { "t1char", 1.0f }, { "t1charAmt", 60.0f },
                         { "t1modDepth", 12.0f }, { "t1modRate", 0.6f }, { "wet", -6.0f } } },
        { "BBD Chorus Echo", { { "t1note", note8 }, { "t1feel", straightFeel }, { "t1fb", 35.0f }, { "t1char", 2.0f }, { "t1charAmt", 50.0f },
                               { "t1modDepth", 20.0f }, { "t1modRate", 1.1f }, { "wet", -6.0f } } },
        { "Diffused Wash", { { "t1note", note4 }, { "t1feel", straightFeel }, { "t1fb", 60.0f }, { "t1diff", 70.0f }, { "t1lp", 7000.0f },
                             { "t2used", 1.0f }, { "t2note", note4 }, { "t2feel", dottedFeel }, { "t2fb", 60.0f }, { "t2diff", 70.0f }, { "t2pan", 60.0f },
                             { "t1pan", -60.0f }, { "wet", -8.0f } } },
        { "Ducked Vocal Echo", { { "t1note", note4 }, { "t1feel", straightFeel }, { "t1fb", 30.0f }, { "t1lp", 6000.0f },
                                 { "duckAmt", 12.0f }, { "duckThr", -30.0f }, { "wet", -6.0f } } },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Slapback", "Short" },
        { "Haas widener", "Short" },
        { "1/8 dotted", "Tempo" },
        { "1/4 ping-pong", "Tempo" },
        { "Triplet echo", "Tempo" },
        { "Lo-fi tape", "Character" },
        { "Tape Echo", "Character" },
        { "BBD Chorus Echo", "Character" },
        { "Diffused Wash", "Ambient" },
        { "Ducked Vocal Echo", "Tempo" },
    });

    return presets;
}
} // namespace

DelayProcessor::DelayProcessor()
    : FancyProcessor ("Fancy Delay", "FancyDelay",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets())
{
    auto& apvts = getApvts();

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto value = [&] (const char* suffix) { return apvts.getRawParameterValue (id (tap, suffix)); };

        tapValues[(size_t) tap] = { value ("used"), value ("on"), value ("solo"), value ("mode"), value ("ms"), value ("note"), value ("feel"),
                                    value ("mult"), value ("fb"), value ("lp"), value ("hp"), value ("bits"), value ("width"), value ("vol"),
                                    value ("pan"), value ("ping"), value ("inloop"), value ("sat"),
                                    value ("char"), value ("charAmt"), value ("modRate"), value ("modDepth"), value ("modRand"), value ("diff") };
    }
}

float DelayProcessor::read (const char* parameterId) const
{
    return const_cast<DelayProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool DelayProcessor::isTapUsed (int tap) const
{
    return tapValues[(size_t) tap].used->load (std::memory_order_relaxed) >= 0.5f;
}

int DelayProcessor::countTaps() const
{
    auto count = 0;

    for (int tap = 0; tap < delay::maxTaps; ++tap)
        count += isTapUsed (tap) ? 1 : 0;

    return count;
}

DelayDsp::TapSpec DelayProcessor::readTap (int tap) const
{
    const auto& values = tapValues[(size_t) tap];

    const auto load = [] (const std::atomic<float>* value) { return value->load (std::memory_order_relaxed); };
    const auto isOn = [&] (const std::atomic<float>* value) { return load (value) >= 0.5f; };

    DelayDsp::TapSpec spec;

    spec.used = isOn (values.used);
    spec.on = isOn (values.on);
    spec.solo = isOn (values.solo);
    spec.noteMode = isOn (values.mode);
    spec.pingPong = isOn (values.ping);
    spec.filtersInLoop = isOn (values.inLoop);
    spec.saturate = isOn (values.saturate);
    spec.lengthMs = (double) load (values.ms);
    spec.noteLength = { juce::roundToInt (load (values.note)), juce::roundToInt (load (values.feel)), juce::roundToInt (load (values.mult)) + 1 };
    spec.feedback = (double) load (values.feedback) * 0.01;
    spec.lowpassHz = (double) load (values.lowpass);
    spec.highpassHz = (double) load (values.highpass);
    spec.bits = (double) load (values.bits);
    spec.width = (double) load (values.width);
    spec.volumeGain = fancy::levelToGain (load (values.volume), mixFloorDb);
    spec.pan = (double) load (values.pan) * 0.01;

    spec.character = juce::jlimit (0, 2, juce::roundToInt (load (values.character)));
    spec.characterAmount = (double) load (values.characterAmount) * 0.01;
    spec.modRateHz = (double) load (values.modRate);
    spec.modDepthCents = (double) load (values.modDepth);
    spec.modRandom = (double) load (values.modRandom) * 0.01;
    spec.diffusion = (double) load (values.diffusion) * 0.01;

    return spec;
}

DelayDsp::Targets DelayProcessor::readTargets() const
{
    DelayDsp::Targets targets;

    for (int tap = 0; tap < delay::maxTaps; ++tap)
        targets.taps[(size_t) tap] = readTap (tap);

    targets.bpm = getBpm();
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.smoothTime = read ("smooth") >= 0.5f;
    targets.bypass = read ("bypass") >= 0.5f;

    targets.duckAmountDb = (double) read ("duckAmt");
    targets.duckThresholdDb = (double) read ("duckThr");
    targets.duckReleaseMs = (double) read ("duckRel");
    targets.swing = (double) read ("swing") * 0.01;
    targets.fastTanh = read ("satCurve") >= 0.5f;

    return targets;
}

void DelayProcessor::updateTempo()
{
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                tempo.store (juce::jlimit (minBpm, maxBpm, *bpm), std::memory_order_relaxed);
                hostTempo.store (true, std::memory_order_relaxed);
            }
        }
    }
}

void DelayProcessor::prepareAudio (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    updateTempo();
    dsp.snapToTargets (readTargets(), getMainBusNumOutputChannels() < 2);
}

void DelayProcessor::reset()
{
    dsp.reset();
}

bool DelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void DelayProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    updateTempo();

    const auto channels = buffer.getArrayOfWritePointers();

    inputMeter.push (channels, numChannels, numSamples);

    dsp.setTargets (readTargets(), numChannels < 2);
    dsp.process (channels, numChannels, numSamples);

    outputMeter.push (channels, numChannels, numSamples);
}

void DelayProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

void DelayProcessor::resetTap (int tap)
{
    static const char* const suffixes[] { "on", "solo", "mode", "ms", "note", "feel", "mult", "fb", "lp", "hp", "bits", "width", "vol", "pan", "ping", "inloop", "sat",
                                          "char", "charAmt", "modRate", "modDepth", "modRand", "diff" };

    for (const auto* suffix : suffixes)
    {
        auto& parameter = *getApvts().getParameter (id (tap, suffix));

        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.getDefaultValue());
        parameter.endChangeGesture();
    }
}

int DelayProcessor::addTap (int after)
{
    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        if (isTapUsed (tap))
            continue;

        // One step later than the tap it follows, so the new handle doesn't land on top of it.
        const auto spec = readTap (after);

        resetTap (tap);
        setParameter (id (tap, "mode"), spec.noteMode ? 1.0f : 0.0f);
        setParameter (id (tap, "ms"), (float) juce::jmin (delay::maxDelayMs, spec.lengthMs * 2.0));
        setParameter (id (tap, "note"), (float) spec.noteLength.note);
        setParameter (id (tap, "feel"), (float) spec.noteLength.feel);
        setParameter (id (tap, "mult"), (float) juce::jmin (delay::maxMultiplier, spec.noteLength.multiplier + 1) - 1.0f);
        setParameter (id (tap, "used"), 1.0f);

        return tap;
    }

    return -1;
}

void DelayProcessor::removeTap (int tap)
{
    if (countTaps() <= 1)
        return;

    setParameter (id (tap, "used"), 0.0f);
    resetTap (tap);
}

juce::AudioProcessorEditor* DelayProcessor::createEditor()
{
    return new DelayEditor (*this);
}
