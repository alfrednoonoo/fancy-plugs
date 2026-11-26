#include "RoomProcessor.h"
#include "RoomEditor.h"

#include <fancy/dsp/DelayTime.h>
#include <fancy/plugin/ParamHelpers.h>

#include <cmath>
#include <limits>

namespace
{
constexpr double minBpm = 20.0, maxBpm = 999.0;

// Phase 4 Wave 3 (docs/room.md, decision 1 A): Shimmer's intervals, in semitones, in the menu's
// order, as Verb's.
constexpr double shimmerSemitones[] { 12.0, 7.0, 5.0, 19.0, 24.0, -12.0 };

juce::String msText (float value)
{
    return (value < 10.0f ? fancy::formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms";
}

juce::String secondsText (float value)
{
    return fancy::formatFixed (value, value < 10.0f ? 2 : 1) + " s";
}

float parseSeconds (const juce::String& text)
{
    const auto value = fancy::parseNumber (text);
    return text.containsIgnoreCase ("ms") ? value * 0.001f : value;
}

// Width -1 to +2 with 1.00 in the middle of the knob, as the mockup draws it: -1 to 1 on the
// lower half, 1 to 2 on the upper.
juce::NormalisableRange<float> widthRange()
{
    return { -1.0f, 2.0f,
             [] (float, float, float normalised) { return normalised < 0.5f ? -1.0f + 4.0f * normalised : 1.0f + 2.0f * (normalised - 0.5f); },
             [] (float, float, float value) { return value < 1.0f ? (value + 1.0f) * 0.25f : 0.5f + (value - 1.0f) * 0.5f; },
             [] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value * 100.0f) / 100.0f); } };
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;
    using Attributes = juce::AudioParameterFloatAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    juce::NormalisableRange<float> predelayRange (0.0f, (float) RoomDsp::maxPredelayMs, 0.1f);
    predelayRange.setSkewForCentre (40.0f);

    layout.add (fancy::floatParameter ("predelay", "Pre-delay", predelayRange, 12.0f,
                                       Attributes {}
                                           .withLabel ("ms")
                                           .withStringFromValueFunction ([] (float value, int) { return msText (value); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    juce::StringArray notes;

    for (int i = 0; i < RoomProcessor::numNotes; ++i)
        notes.add (RoomProcessor::noteName (i));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "prenote", 1 }, "Pre-delay note", notes, RoomProcessor::defaultNote));
    layout.add (toggle ("sync", "Sync pre-delay", false));

    layout.add (fancy::percentParameter ("size", "Size", 55.0f));

    layout.add (fancy::floatParameter ("decay", "Decay", fancy::logRange (0.1f, 20.0f, 0.01f), 1.8f,
                                       Attributes {}
                                           .withLabel ("s")
                                           .withStringFromValueFunction ([] (float value, int) { return secondsText (value); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return parseSeconds (text); })));

    layout.add (fancy::percentParameter ("diffusion", "Diffusion", 70.0f));
    layout.add (fancy::percentParameter ("mod", "Mod", 25.0f));

    layout.add (fancy::floatParameter ("width", "Width", widthRange(), 1.0f,
                                       Attributes {}
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::percentParameter ("damping", "Damping", 45.0f));

    const auto frequency = [] (const char* id, const char* name, float low, float high, float defaultHz)
    {
        return fancy::floatParameter (id, name, fancy::logRange (low, high, 1.0f), defaultHz,
                                      juce::AudioParameterFloatAttributes {}
                                          .withLabel ("Hz")
                                          .withStringFromValueFunction ([] (float value, int) { return fancy::formatFilterHz (value); })
                                          .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));
    };

    layout.add (frequency ("lowcut", "Low cut", 20.0f, 1000.0f, 150.0f));
    layout.add (frequency ("highcut", "High cut", 1000.0f, 20000.0f, 9000.0f));

    layout.add (toggle ("freeze", "Freeze", false));
    layout.add (toggle ("wetonly", "Wet only (send)", false));

    layout.add (fancy::percentParameter ("ducking", "Ducking", 0.0f));
    // A Quality change rings the old network out (decision 4), so it's a deliberate setting.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "quality", 1 }, "Quality", juce::StringArray { "Eco", "Normal", "High" }, 1,
                                          ChoiceAttributes {}.withAutomatable (false)));
    layout.add (fancy::levelParameter ("erlevel", "Early reflections", RoomProcessor::mixFloorDb, 6.0f, 0.0f, RoomProcessor::mixFloorDb));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "stereoin", 1 }, "Stereo input", juce::StringArray { "Stereo", "Mono-sum" }, 0));

    layout.add (fancy::levelParameter ("wet", "Wet", RoomProcessor::mixFloorDb, 12.0f, -12.0f, RoomProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", RoomProcessor::mixFloorDb, 12.0f, 0.0f, RoomProcessor::mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    // Phase 4 Wave 1 (docs/room.md), appended. Every default is today's sound.
    layout.add (fancy::floatParameter ("modRate", "Mod rate", fancy::logRange (0.25f, 4.0f, 0.01f), 1.0f,
                                       Attributes {}
                                           .withLabel ("x")
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2) + juce::String (juce::CharPointer_UTF8 ("\xc3\x97")); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (frequency ("eqLowFreq", "EQ low freq", 20.0f, 1000.0f, 200.0f));
    layout.add (fancy::levelParameter ("eqLowGain", "EQ low gain", -12.0f, 12.0f, 0.0f, -100.0f));
    layout.add (frequency ("eqMidFreq", "EQ mid freq", 200.0f, 8000.0f, 1000.0f));
    layout.add (fancy::levelParameter ("eqMidGain", "EQ mid gain", -12.0f, 12.0f, 0.0f, -100.0f));
    layout.add (fancy::floatParameter ("eqMidQ", "EQ mid Q", fancy::logRange (0.3f, 8.0f, 0.01f), 1.0f,
                                       Attributes {}
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 2); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (frequency ("eqHighFreq", "EQ high freq", 1000.0f, 20000.0f, 6000.0f));
    layout.add (fancy::levelParameter ("eqHighGain", "EQ high gain", -12.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("gate", "Gate", false));
    layout.add (fancy::levelParameter ("gateThr", "Gate threshold", -60.0f, 0.0f, -30.0f, -100.0f));

    const auto milliseconds = [] (const char* id, const char* name, float low, float high, float defaultMs)
    {
        return fancy::floatParameter (id, name, fancy::logRange (low, high, 1.0f), defaultMs,
                                      juce::AudioParameterFloatAttributes {}
                                          .withLabel ("ms")
                                          .withStringFromValueFunction ([] (float value, int) { return msText (value); })
                                          .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
    };

    layout.add (milliseconds ("gateHold", "Gate hold", 10.0f, 500.0f, 80.0f));
    layout.add (milliseconds ("gateDecay", "Gate decay", 5.0f, 500.0f, 40.0f));
    layout.add (toggle ("autoGain", "Auto gain", false));

    // Phase 4 Wave 2 (docs/room.md, items 19 to 21), appended. Every default is v1.7.0's sound.
    layout.add (fancy::floatParameter ("lowDecay", "Low decay", fancy::logRange (0.25f, 4.0f, 0.01f), 1.0f,
                                       Attributes {}
                                           .withLabel ("x")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + fancy::formatFixed (value, 2); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    juce::NormalisableRange<float> attackRange (0.0f, (float) RoomDsp::maxAttackMs, 1.0f);
    attackRange.setSkewForCentre (100.0f);
    layout.add (fancy::floatParameter ("attack", "Attack", attackRange, 0.0f,
                                       Attributes {}
                                           .withLabel ("ms")
                                           .withStringFromValueFunction ([] (float value, int) { return msText (value); })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "duckFrom", 1 }, "Duck from", juce::StringArray { "Input", "Sidechain" }, 0));

    // Phase 4 Wave 3, appended (docs/room.md): Shimmer. At 0 % the sound is as before.
    layout.add (fancy::percentParameter ("shimmer", "Shimmer", 0.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "shimmerPitch", 1 }, "Shimmer interval",
                                          juce::StringArray { "Octave up", "Fifth up", "Fourth up", "Octave and fifth up", "Two octaves up", "Octave down" }, 0));
    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    // The last one is what Workflow's A/B test switches to mid-playback, so it keeps Size and
    // Quality where the defaults have them.
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Small room", { { "size", 25.0f }, { "decay", 0.6f }, { "predelay", 5.0f }, { "damping", 50.0f }, { "wet", -14.0f } } },
        { "Drum room", { { "size", 35.0f }, { "decay", 0.9f }, { "predelay", 8.0f }, { "diffusion", 60.0f }, { "damping", 55.0f }, { "erlevel", 2.0f } } },
        { "Vocal plate", { { "size", 45.0f }, { "decay", 1.6f }, { "predelay", 25.0f }, { "diffusion", 85.0f }, { "damping", 35.0f },
                           { "lowcut", 200.0f }, { "highcut", 10000.0f } } },
        { "Big hall", { { "size", 90.0f }, { "decay", 3.5f }, { "predelay", 30.0f }, { "damping", 50.0f }, { "mod", 35.0f } } },
        { "Ambient wash", { { "size", 100.0f }, { "decay", 12.0f }, { "predelay", 60.0f }, { "mod", 50.0f }, { "damping", 30.0f }, { "wet", -10.0f } } },
        { "Send, all wet", { { "wetonly", 1.0f }, { "wet", 0.0f }, { "lowcut", 250.0f } } },
        { "Ducked vocal", { { "ducking", 60.0f }, { "decay", 2.2f }, { "damping", 40.0f }, { "wet", -10.0f } } },
    };

    fancy::setCategories (presets, {
        { "Small room", "Rooms" },
        { "Drum room", "Drums" },
        { "Vocal plate", "Vocals" },
        { "Big hall", "Halls" },
        { "Ambient wash", "Ambient" },
        { "Send, all wet", "Sends" },
        { "Ducked vocal", "Vocals" },
    });

    return presets;
}
} // namespace

double RoomProcessor::noteMs (int noteIndex, double bpm) noexcept
{
    const auto index = juce::jlimit (0, numNotes - 1, noteIndex);
    return delay::noteMs ({ 6 - index / 3, index % 3, 1 }, bpm);
}

juce::String RoomProcessor::noteName (int noteIndex)
{
    const char* const names[] { "1/64", "1/32", "1/16", "1/8" };
    const char* const feels[] { "", " dotted", " triplet" };
    const auto index = juce::jlimit (0, numNotes - 1, noteIndex);
    return juce::String (names[index / 3]) + feels[index % 3];
}

RoomProcessor::RoomProcessor()
    : FancyProcessor ("Fancy Room", "FancyRoom",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false), // Phase 4 Wave 2
                      createLayout(), createPresets())
{
}

float RoomProcessor::read (const char* parameterId) const
{
    return const_cast<RoomProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool RoomProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

double RoomProcessor::unclampedNoteMs() const noexcept
{
    return noteMs (juce::roundToInt (read ("prenote")), getBpm());
}

double RoomProcessor::getPredelayMs() const noexcept
{
    if (isOn ("sync"))
        return std::min (RoomDsp::maxPredelayMs, unclampedNoteMs());

    return (double) read ("predelay");
}

bool RoomProcessor::isPredelayCapped() const noexcept
{
    return isOn ("sync") && unclampedNoteMs() > RoomDsp::maxPredelayMs;
}

double RoomProcessor::getTailLengthSeconds() const
{
    if (isOn ("freeze"))
        return std::numeric_limits<double>::infinity();

    // Phase 4 Wave 3: Shimmer lengthens the tail (to about twice at 100 %), so the host gets three
    // times as long while it's up, as Verb does.
    const auto tail = RoomDsp::tailSeconds (getPredelayMs(), (double) read ("size") / 100.0, (double) read ("decay"));
    return read ("shimmer") > 0.0f ? 3.0 * tail : tail;
}

RoomDsp::Targets RoomProcessor::readTargets() const
{
    RoomDsp::Targets targets;

    targets.predelayMs = getPredelayMs();
    targets.size = (double) read ("size") / 100.0;
    targets.decay = (double) read ("decay");
    targets.diffusion = (double) read ("diffusion") / 100.0;
    targets.mod = (double) read ("mod") / 100.0;
    targets.width = (double) read ("width");
    targets.damping = (double) read ("damping") / 100.0;
    targets.lowCutHz = (double) read ("lowcut");
    targets.highCutHz = (double) read ("highcut");
    targets.freeze = isOn ("freeze");
    targets.ducking = (double) read ("ducking") / 100.0;
    targets.quality = juce::roundToInt (read ("quality"));
    targets.erGain = fancy::levelToGain (read ("erlevel"), mixFloorDb);
    targets.monoSum = juce::roundToInt (read ("stereoin")) == 1;
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    // Decision 1: Wet only treats Dry as -inf, and keeps Dry's own value for when it's off.
    targets.dryGain = isOn ("wetonly") ? 0.0 : fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.bypass = isOn ("bypass");

    // Phase 4 Wave 1.
    targets.modRate = (double) read ("modRate");
    targets.eqLowHz = (double) read ("eqLowFreq");
    targets.eqLowDb = (double) read ("eqLowGain");
    targets.eqMidHz = (double) read ("eqMidFreq");
    targets.eqMidDb = (double) read ("eqMidGain");
    targets.eqMidQ = (double) read ("eqMidQ");
    targets.eqHighHz = (double) read ("eqHighFreq");
    targets.eqHighDb = (double) read ("eqHighGain");
    targets.gate = isOn ("gate");
    targets.gateThresholdDb = (double) read ("gateThr");
    targets.gateHoldMs = (double) read ("gateHold");
    targets.gateDecayMs = (double) read ("gateDecay");
    targets.autoGain = isOn ("autoGain");

    // Phase 4 Wave 2. Listen to ducking is the temporary switch, never a parameter (§0.3).
    targets.lowDecay = (double) read ("lowDecay");
    targets.attackMs = (double) read ("attack");
    targets.duckSidechain = isOn ("duckFrom");
    targets.listenDucking = isListening();

    // Phase 4 Wave 3.
    targets.shimmer = (double) read ("shimmer") / 100.0;
    targets.shimmerRatio = std::exp2 (shimmerSemitones[juce::jlimit (0, 5, juce::roundToInt (read ("shimmerPitch")))] / 12.0);
    return targets;
}

void RoomProcessor::updateTempo()
{
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm())
                tempo.store (juce::jlimit (minBpm, maxBpm, *bpm), std::memory_order_relaxed);
}

void RoomProcessor::prepareAudio (double sampleRate, int)
{
    updateTempo();
    dsp.setTargets (readTargets());
    dsp.prepare (sampleRate);
}

void RoomProcessor::reset()
{
    dsp.reset();
}

bool RoomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    // Phase 4 Wave 2: the optional sidechain, off, mono or stereo.
    if (layouts.inputBuses.size() < 2)
        return true;

    const auto sidechain = layouts.getChannelSet (true, 1);
    return sidechain.isDisabled() || sidechain == juce::AudioChannelSet::mono() || sidechain == juce::AudioChannelSet::stereo();
}

void RoomProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    // Phase 4 Wave 2: the main bus and the optional sidechain, as their own buffers.
    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto channels = mainBuffer.getArrayOfWritePointers();
    auto sidechainBuffer = getBusCount (true) > 1 ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();
    const auto numSidechain = juce::jmin (sidechainBuffer.getNumChannels(), 2);
    const float* sidechain[2] { numSidechain > 0 ? sidechainBuffer.getReadPointer (0) : nullptr,
                                numSidechain > 1 ? sidechainBuffer.getReadPointer (1) : nullptr };

    inputMeter.push (channels, numChannels, numSamples);

    // The notice shows only while Ducking is keyed by a sidechain that's silent.
    sidechainWatch.update (channels, numChannels, sidechain, numSidechain, numSamples, getSampleRate(),
                           isOn ("duckFrom") && read ("ducking") > 0.0f);

    updateTempo();
    dsp.setTargets (readTargets());
    dsp.process (channels, numChannels, numSamples, sidechain, numSidechain);

    outputMeter.push (channels, numChannels, numSamples);
}

juce::AudioProcessorEditor* RoomProcessor::createEditor()
{
    return new RoomEditor (*this);
}
