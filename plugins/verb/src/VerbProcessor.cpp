#include "VerbProcessor.h"
#include "VerbLibrary.h"
#include "VerbEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <algorithm>
#include <array>
#include <cmath>

using verb::Convolver;
using verb::ModuleType;

namespace
{
constexpr float mixFloorDb = -60.0f;
constexpr int workerIntervalMs = 25;
constexpr juce::int64 embedLimitBytes = 10 * 1024 * 1024;

// Phase 4 Wave 3 (docs/verb.md, decision 3 A): Shimmer's intervals, in semitones, in the menu's order.
constexpr double shimmerSemitones[] { 12.0, 7.0, 5.0, 19.0, 24.0, -12.0 };

// Phase 4 Wave 3: what Shimmer's loop and Freeze's release need from a built IR: the energy that
// reaches the busier output (stereo: every path the IR has), and the broadband RT60, measured
// from -5 to -35 dB as the Decay module measures. With Modulate the tail is added back first.
std::pair<double, double> loopInfoOf (const verb::BuiltIr& built, double sampleRate)
{
    const auto& audio = built.audio;
    const auto channels = audio.getNumChannels();
    const auto length = audio.getNumSamples();
    const auto withTail = built.modulation.on && built.tailA.getNumChannels() >= channels;

    std::array<double, 4> energy {};
    std::vector<double> envelope ((size_t) length, 0.0);

    for (int ch = 0; ch < std::min (channels, 4); ++ch)
    {
        const auto* samples = audio.getReadPointer (ch);
        const auto* tail = withTail && built.tailA.getNumSamples() >= length ? built.tailA.getReadPointer (ch) : nullptr;

        for (int i = 0; i < length; ++i)
        {
            const auto value = (double) samples[i] + (tail != nullptr ? (double) tail[i] : 0.0);
            energy[(size_t) ch] += value * value;
            envelope[(size_t) i] += value * value;
        }
    }

    // Channels: left to left, right to right, then right to left (RL) and left to right (LR).
    const auto toLeft = energy[0] + energy[2], toRight = energy[1] + energy[3];
    const auto fromLeft = energy[0] + energy[3], fromRight = energy[1] + energy[2];
    const auto loopEnergy = std::max ({ toLeft, toRight, fromLeft, fromRight });

    auto rt60 = verb::schroederRt60 (envelope, sampleRate);

    if (! (rt60 > 0.0))
        rt60 = std::max (0.1, (double) length / sampleRate);

    return { loopEnergy, std::clamp (rt60, 0.05, verb::maxIrSeconds) };
}

fancy::FancyProcessor::Layout createLayout()
{
    using Choice = juce::AudioParameterChoice;
    using Attributes = juce::AudioParameterChoiceAttributes;
    using FloatAttributes = juce::AudioParameterFloatAttributes;

    const auto setting = [] (const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        return std::make_unique<Choice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex, Attributes {}.withAutomatable (false));
    };

    fancy::FancyProcessor::Layout layout;

    layout.add (fancy::floatParameter ("predelay", "Pre-delay", juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.5f), 0.0f,
                                       FloatAttributes {}
                                           .withLabel ("ms")
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " ms"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); })));

    layout.add (fancy::floatParameter ("center", "Center", juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                       FloatAttributes {}
                                           .withStringFromValueFunction ([] (float value, int)
                                           {
                                               const auto amount = juce::roundToInt (std::abs (value));
                                               return amount == 0 ? juce::String ("Center")
                                                                  : juce::String (amount) + (value < 0.0f ? "L" : "R");
                                           })
                                           .withValueFromStringFunction ([] (const juce::String& text)
                                           {
                                               const auto value = fancy::parseNumber (text);
                                               return text.containsIgnoreCase ("L") ? -std::abs (value) : value;
                                           })));

    layout.add (fancy::floatParameter ("width", "Width", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                       FloatAttributes {}
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + "W"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, -12.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    layout.add (setting ("latency", "Latency mode", { "Zero latency", "Low CPU" }, 0));
    layout.add (setting ("maxFft", "Max FFT", { "16384", "65536" }, 1));
    layout.add (setting ("irStorage", "IR storage", { "Embed under 10 MB", "Always embed", "Path only" }, 0));

    // Phase 4 Wave 2, appended (docs/verb.md): Ducking and the Gate on the wet signal.
    const auto msParameter = [] (const char* id, const char* name, float low, float high, float defaultMs)
    {
        return fancy::floatParameter (id, name, juce::NormalisableRange<float> (low, high, 1.0f, 0.5f), defaultMs,
                                      FloatAttributes {}
                                          .withLabel ("ms")
                                          .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " ms"; })
                                          .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
    };

    layout.add (fancy::percentParameter ("duck", "Ducking", 0.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "duckSource", 1 }, "Ducking source", juce::StringArray { "Main", "Sidechain" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "gate", 1 }, "Gate", false));
    layout.add (fancy::floatParameter ("gateThr", "Gate threshold", juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f,
                                       FloatAttributes {}
                                           .withLabel ("dB")
                                           .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
    layout.add (msParameter ("gateHold", "Gate hold", 10.0f, 500.0f, 80.0f));
    layout.add (msParameter ("gateDecay", "Gate decay", 5.0f, 500.0f, 40.0f));

    // Phase 4 Wave 2 (docs/verb.md, item 18), appended: multiplies Width. At 100 % the output is
    // as before, to the bit.
    layout.add (fancy::floatParameter ("extraWidth", "Extra width", juce::NormalisableRange<float> (100.0f, 200.0f, 1.0f), 100.0f,
                                       FloatAttributes {}
                                           .withLabel ("%")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    // Phase 4 Wave 3, appended (docs/verb.md): Freeze and Shimmer. Off and 0 % are the sound as before.
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));
    layout.add (fancy::percentParameter ("shimmer", "Shimmer", 0.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "shimmerPitch", 1 }, "Shimmer interval",
                                          juce::StringArray { "Octave up", "Fifth up", "Fourth up", "Octave and fifth up", "Two octaves up", "Octave down" }, 0));

    return layout;
}

std::vector<fancy::FactoryPreset> factoryPresets()
{
    using Values = std::vector<std::pair<juce::String, float>>;

    std::vector<fancy::FactoryPreset> presets {
        { "Empty", Values {}, "empty" },
        { "Small Room", Values { { "wet", -9.0f } }, "room" },
        { "Plate", Values { { "predelay", 12.0f }, { "wet", -10.0f } }, "plate" },
        { "Hall", Values { { "predelay", 24.0f }, { "wet", -12.0f } }, "hall" },
        { "Long Tail", Values { { "predelay", 40.0f }, { "wet", -14.0f } }, "long" },
        { "Slapback", Values { { "wet", -6.0f } }, "slap" },

        // Phase 3 Q2.8: the library, true stereo and Modulate.
        { "Library Hall", Values { { "predelay", 20.0f }, { "wet", -12.0f } }, "libraryHall" },
        { "Library Plate", Values { { "predelay", 10.0f }, { "wet", -11.0f } }, "libraryPlate" },
        { "True Stereo Room", Values { { "wet", -10.0f } }, "trueStereoRoom" },
        { "Modulated Hall", Values { { "predelay", 20.0f }, { "wet", -12.0f } }, "modulatedHall" },
        { "Endless", Values { { "predelay", 30.0f }, { "wet", -14.0f } }, "endless" },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Small Room", "Rooms" },
        { "Plate", "Plates" },
        { "Hall", "Halls" },
        { "Long Tail", "Halls" },
        { "Slapback", "Echo" },
        { "Library Hall", "Halls" },
        { "Library Plate", "Plates" },
        { "True Stereo Room", "Rooms" },
        { "Modulated Hall", "Halls" },
        { "Endless", "Special" },
    });

    return presets;
}

void addFactoryModule (juce::ValueTree& chain, ModuleType type, std::initializer_list<std::pair<const char*, float>> values)
{
    auto module = verb::makeModule (type, chain.getNumChildren() + 1);

    for (const auto& [id, value] : values)
        module.setProperty (id, value, nullptr);

    chain.appendChild (module, nullptr);
}

// A File module playing a library IR (Phase 3 Q2.8).
void addLibraryModule (juce::ValueTree& chain, const juce::String& libraryKey, bool trueStereo)
{
    auto module = verb::makeModule (ModuleType::file, chain.getNumChildren() + 1);
    module.setProperty (verb::ids::factory, libraryKey, nullptr);
    module.setProperty (verb::ids::mono, false, nullptr);

    if (trueStereo)
        module.setProperty (verb::ids::trueStereo, true, nullptr);

    chain.appendChild (module, nullptr);
}

juce::ValueTree factoryChain (const juce::String& key)
{
    auto chain = verb::makeChain();

    if (key == "libraryHall")
    {
        addLibraryModule (chain, "Halls/Concert Hall", true);
    }
    else if (key == "libraryPlate")
    {
        addLibraryModule (chain, "Plates/Vocal Plate", true);
        addFactoryModule (chain, ModuleType::lpHp, { { "highpass", 150.0f } });
    }
    else if (key == "trueStereoRoom")
    {
        addLibraryModule (chain, "Rooms/Studio Live", true);
    }
    else if (key == "modulatedHall")
    {
        addLibraryModule (chain, "Halls/Concert Hall", true);
        addFactoryModule (chain, ModuleType::modulate, {});
    }
    else if (key == "endless")
    {
        addLibraryModule (chain, "Special/Endless Cloud", true);
        addFactoryModule (chain, ModuleType::modulate, { { "start", 150.0f }, { "rate", 0.2f }, { "depth", 100.0f } });
    }

    if (key == "room")
    {
        addFactoryModule (chain, ModuleType::reverb, { { "length", 0.7f }, { "decay", 0.45f }, { "density", 90.0f }, { "damping", 55.0f } });
        addFactoryModule (chain, ModuleType::lpHp, { { "lowpass", 9000.0f }, { "highpass", 120.0f } });
    }
    else if (key == "plate")
    {
        addFactoryModule (chain, ModuleType::reverb, { { "length", 2.5f }, { "decay", 1.6f }, { "density", 100.0f }, { "damping", 20.0f } });
        addFactoryModule (chain, ModuleType::lpHp, { { "highpass", 180.0f } });
    }
    else if (key == "hall")
    {
        addFactoryModule (chain, ModuleType::reverb, { { "length", 4.0f }, { "decay", 2.8f }, { "density", 75.0f }, { "damping", 45.0f } });
        addFactoryModule (chain, ModuleType::lpHp, { { "lowpass", 12000.0f }, { "highpass", 80.0f } });
    }
    else if (key == "long")
    {
        addFactoryModule (chain, ModuleType::reverb, { { "length", 12.0f }, { "decay", 8.0f }, { "density", 70.0f }, { "damping", 60.0f } });
        addFactoryModule (chain, ModuleType::lpHp, { { "lowpass", 10000.0f }, { "highpass", 60.0f } });
    }
    else if (key == "slap")
    {
        addFactoryModule (chain, ModuleType::echo, { { "start", 90.0f }, { "length", 180.0f }, { "spacing", 90.0f },
                                                     { "startVol", -3.0f }, { "endVol", -14.0f } });
        addFactoryModule (chain, ModuleType::lpHp, { { "lowpass", 6000.0f }, { "highpass", 150.0f } });
    }

    return chain;
}
} // namespace

VerbProcessor::VerbProcessor()
    : FancyProcessor ("Fancy Verb", "FANCY_VERB",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false), // Phase 4 Wave 2
                      createLayout(), factoryPresets()),
      buildThread (*this)
{
    if (const auto xml = juce::XmlDocument::parse (settingsFile()))
    {
        irFolder = juce::File (xml->getStringAttribute ("irFolder"));

        // Phase 4 Wave 1: library favourites.
        favourites = juce::StringArray::fromLines (xml->getStringAttribute ("favourites"));
        favourites.removeEmptyStrings();
    }

    getApvts().state.appendChild (verb::makeChain(), nullptr);
    shimmerAmount = getApvts().getRawParameterValue ("shimmer");
    pendingChain = verb::readChain (getChain());

    dsp.prepare (48000.0, Convolver::latencyOf (readMode()));
    dsp.snapToTargets (readTargets());
    setLatencySamples (Convolver::latencyOf (readMode()));

    getApvts().addParameterListener ("latency", this);

    buildThread.startThread (juce::Thread::Priority::low);
}

VerbProcessor::~VerbProcessor()
{
    getApvts().removeParameterListener ("latency", this);
    buildThread.stopThread (10000);
}

float VerbProcessor::read (const char* parameterId)
{
    return getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

Convolver::Mode VerbProcessor::readMode()
{
    return read ("latency") >= 0.5f ? Convolver::Mode::lowCpu : Convolver::Mode::zeroLatency;
}

int VerbProcessor::readMaxBlock()
{
    return read ("maxFft") >= 0.5f ? 32768 : 8192;
}

VerbDsp::Targets VerbProcessor::readTargets()
{
    VerbDsp::Targets targets;
    targets.predelayMs = read ("predelay");
    targets.center = read ("center") * 0.01;
    // Phase 4 Wave 2: Extra width multiplies Width. Divided by 100 so 100 % is exactly 1.
    targets.width = read ("width") * 0.01 * ((double) read ("extraWidth") / 100.0);
    targets.wet = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dry = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.output = fancy::dbToGain ((double) read ("output"));
    targets.bypass = read ("bypass") >= 0.5f;
    targets.modRate = (double) modulationRate.load (std::memory_order_relaxed);
    targets.modDepth = (double) modulationDepth.load (std::memory_order_relaxed);
    targets.modSpread = (double) modulationSpread.load (std::memory_order_relaxed);

    // Phase 4 Wave 2.
    targets.ducking = read ("duck") * 0.01;
    targets.duckSidechain = read ("duckSource") >= 0.5f;
    targets.gate = read ("gate") >= 0.5f;
    targets.gateThresholdDb = read ("gateThr");
    targets.gateHoldMs = read ("gateHold");
    targets.gateDecayMs = read ("gateDecay");

    // Phase 4 Wave 3.
    targets.freeze = read ("freeze") >= 0.5f;
    targets.shimmer = read ("shimmer") * 0.01;
    targets.shimmerRatio = std::exp2 (shimmerSemitones[juce::jlimit (0, 5, juce::roundToInt (read ("shimmerPitch")))] / 12.0);
    return targets;
}

// The new latency reaches the host at once. The dry path follows when the new convolver lands.
void VerbProcessor::parameterChanged (const juce::String&, float)
{
    setLatencySamples (Convolver::latencyOf (readMode()));
}

bool VerbProcessor::buildPending()
{
    return requestedBuild.load() != completedBuild.load()
        || builtMode.load() != (int) readMode()
        || builtMaxBlock.load() != readMaxBlock();
}

void VerbProcessor::buildNow()
{
    const auto request = requestedBuild.load();

    verb::ChainSpec chain;
    juce::File folder;

    {
        const std::lock_guard lock (buildMutex);
        chain = pendingChain;
        folder = irFolder;
    }

    const auto mode = readMode();
    const auto maxBlock = readMaxBlock();
    const auto notFound = resolver.resolve (chain, folder);

    auto built = verb::buildIr (chain, buildRate.load());

    // A true-stereo IR (Phase 3 Q2.8) has the cross paths as channels 2 (RL) and 3 (LR).
    const auto makeConvolver = [mode, maxBlock] (const juce::AudioBuffer<float>& audio)
    {
        const auto trueStereo = audio.getNumChannels() >= 4;

        return std::make_unique<Convolver> (audio.getReadPointer (0), audio.getReadPointer (1),
                                            trueStereo ? audio.getReadPointer (2) : nullptr,
                                            trueStereo ? audio.getReadPointer (3) : nullptr,
                                            audio.getNumSamples(), mode, maxBlock);
    };

    auto convolver = makeConvolver (built.audio);

    // Phase 4 Wave 3: Shimmer's loop gain and Freeze's release follow the IR.
    const auto [loopEnergy, rt60] = loopInfoOf (built, buildRate.load());
    convolver->setLoopInfo (loopEnergy, rt60);

    // Modulate: the early part here, and the two tails crossfaded by the convolver.
    if (built.modulation.on)
        convolver->attachTails (makeConvolver (built.tailA), makeConvolver (built.tailB), buildRate.load(), built.modulation.seed,
                                built.modulation.rateHz, built.modulation.depth, built.modulation.spread);

    dsp.offer (std::move (convolver));

    irSeconds.store (built.preview.trimmedSeconds);

    auto forExport = std::make_shared<const juce::AudioBuffer<float>> (verb::exportableIr (built));

    {
        const std::lock_guard lock (buildMutex);
        preview = std::move (built.preview);
        missing = notFound;
        exportable = std::move (forExport);
        exportableRate = buildRate.load();
    }

    builtMode.store ((int) mode);
    builtMaxBlock.store (maxBlock);
    completedBuild.store (request);
}

void VerbProcessor::BuildThread::run()
{
    while (! threadShouldExit())
    {
        // Freed here, so the audio thread never pays for it.
        processor.dsp.takeRetired().reset();

        if (processor.buildPending())
            processor.buildNow();
        else
            wait (workerIntervalMs);
    }
}

void VerbProcessor::requestBuild()
{
    {
        const std::lock_guard lock (buildMutex);
        pendingChain = verb::readChain (getChain());
    }

    requestedBuild.fetch_add (1);
    buildThread.notify();
}

bool VerbProcessor::waitForBuild (int timeoutMs)
{
    const auto limit = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (buildPending())
    {
        if (juce::Time::getMillisecondCounter() > limit)
            return false;

        juce::Thread::sleep (1);
    }

    return true;
}

void VerbProcessor::prepareAudio (double sampleRate, int)
{
    const auto rateChanged = sampleRate != buildRate.load();

    dsp.prepare (sampleRate, Convolver::latencyOf (readMode()));
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatency());

    buildRate.store (sampleRate);

    if (rateChanged || ! dsp.hasConvolver())
    {
        requestedBuild.fetch_add (1);
        buildThread.notify();
    }
}

void VerbProcessor::reset()
{
    dsp.reset();
}

bool VerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void VerbProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);

    if (numChannels == 0)
        return;

    const auto offline = isNonRealtime();

    // An offline render waits as long as the build takes: it must never start on an old
    // impulse, and never drop audio to save time.
    if (offline)
        while (buildPending())
            juce::Thread::sleep (1);

    // Phase 4 Wave 2: the main bus and the sidechain, as their own buffers.
    auto mainBuffer = getBusBuffer (buffer, false, 0);
    const auto numSidechain = getBusCount (true) > 1 ? juce::jmin (getBusBuffer (buffer, true, 1).getNumChannels(), 2) : 0;
    auto sidechainBuffer = numSidechain > 0 ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    dsp.setTargets (readTargets());
    dsp.process (mainBuffer.getArrayOfWritePointers(), juce::jmin (numChannels, mainBuffer.getNumChannels()),
                 numSidechain > 0 ? sidechainBuffer.getArrayOfReadPointers() : nullptr, numSidechain, buffer.getNumSamples(), offline);
}

double VerbProcessor::getTailLengthSeconds() const
{
    // Phase 4 Wave 3: Shimmer lengthens the tail; at 100 % it takes about half as long again to die
    // away (the model), so three times the IR covers it.
    const auto shimmer = shimmerAmount != nullptr && shimmerAmount->load (std::memory_order_relaxed) > 0.0f;
    return irSeconds.load() * (shimmer ? 3.0 : 1.0) + 0.5;
}

juce::ValueTree VerbProcessor::getChain()
{
    return getApvts().state.getChildWithName (verb::ids::chain);
}

void VerbProcessor::stateApplied()
{
    auto chain = getChain();

    if (! chain.isValid())
    {
        chain = verb::makeChain();
        getApvts().state.appendChild (chain, nullptr);
    }

    verb::sanitiseChain (chain);
    chainEdited (true);
}

void VerbProcessor::factoryPresetLoaded (const juce::String& data)
{
    auto& state = getApvts().state;

    state.removeChild (getChain(), nullptr);
    state.appendChild (factoryChain (data), nullptr);
    stateApplied();
}

void VerbProcessor::updateModulation()
{
    for (const auto module : getChain())
    {
        if (module[verb::ids::type].toString() == verb::storedName (ModuleType::modulate))
        {
            modulationRate.store ((float) module.getProperty ("rate", 0.3f));
            modulationDepth.store ((float) module.getProperty ("depth", 50.0f) * 0.01f);
            modulationSpread.store ((float) module.getProperty ("spread", 100.0f) * 0.01f);
            return;
        }
    }
}

void VerbProcessor::chainEdited (bool structural)
{
    noteEdited();
    updateModulation();

    if (structural)
        chainRevision.fetch_add (1);

    requestBuild();
}

juce::ValueTree VerbProcessor::findModule (int uid)
{
    return getChain().getChildWithProperty (verb::ids::uid, uid);
}

int VerbProcessor::nextUid()
{
    auto highest = 0;

    for (const auto module : getChain())
        highest = juce::jmax (highest, (int) module[verb::ids::uid]);

    return highest + 1;
}

int VerbProcessor::addModule (ModuleType type, int position)
{
    auto chain = getChain();

    if (chain.getNumChildren() >= verb::maxModules)
        return 0;

    // One Modulate per chain (Phase 3 Q2.8).
    if (type == ModuleType::modulate && chain.getChildWithProperty (verb::ids::type, verb::storedName (ModuleType::modulate)).isValid())
        return 0;

    const auto uid = nextUid();
    chain.addChild (verb::makeModule (type, uid), juce::jlimit (0, chain.getNumChildren(), position), nullptr);
    chainEdited (true);
    return uid;
}

void VerbProcessor::removeModule (int uid)
{
    getChain().removeChild (findModule (uid), nullptr);
    chainEdited (true);
}

void VerbProcessor::moveModule (int uid, int position)
{
    auto chain = getChain();
    const auto from = chain.indexOf (findModule (uid));

    if (from < 0 || from == position)
        return;

    chain.moveChild (from, juce::jlimit (0, chain.getNumChildren() - 1, position), nullptr);
    chainEdited (true);
}

void VerbProcessor::setModuleOn (int uid, bool on)
{
    findModule (uid).setProperty (verb::ids::on, on, nullptr);
    chainEdited (true);
}

void VerbProcessor::setModuleValue (int uid, const juce::Identifier& id, const juce::var& value)
{
    auto module = findModule (uid);

    if (! module.isValid() || module[id] == value)
        return;

    module.setProperty (id, value, nullptr);

    // Modulate's Rate, Depth and Spread act live on the convolver, without a rebuild (Phase 3 Q2.8).
    if (module[verb::ids::type].toString() == verb::storedName (ModuleType::modulate) && id != juce::Identifier ("start"))
    {
        noteEdited();
        updateModulation();
        return;
    }

    chainEdited (false);
}

bool VerbProcessor::loadFile (int uid, const juce::File& file)
{
    auto module = findModule (uid);
    const auto audio = verb::readAudioFile (file);

    if (! module.isValid() || audio == nullptr)
        return false;

    const auto storage = juce::roundToInt (read ("irStorage"));
    const auto embed = storage == 1 || (storage == 0 && file.getSize() < embedLimitBytes);

    module.setProperty (verb::ids::path, file.getFullPathName(), nullptr);
    module.setProperty (verb::ids::hash, verb::hashOfFile (file), nullptr);
    module.setProperty (verb::ids::flac, embed ? verb::encodeFlac (*audio) : juce::String(), nullptr);
    module.setProperty (verb::ids::rate, embed ? audio->sampleRate : 0.0, nullptr);

    module.removeProperty (verb::ids::factory, nullptr);

    // Phase 3 Q2.8: a new file ends any pair. A four-channel file is true stereo, so it's not
    // mixed to mono; the property is only written when it's on, so other modules are unchanged.
    for (const auto& id : { verb::ids::pairPath, verb::ids::pairHash, verb::ids::pairFlac, verb::ids::pairRate })
        module.removeProperty (id, nullptr);

    if (audio->audio.getNumChannels() >= 4)
    {
        module.setProperty (verb::ids::trueStereo, true, nullptr);
        module.setProperty (verb::ids::mono, false, nullptr);
    }
    else
    {
        module.removeProperty (verb::ids::trueStereo, nullptr);
    }

    chainEdited (true);
    return true;
}

bool VerbProcessor::loadPair (int uid, const juce::File& file)
{
    auto module = findModule (uid);
    const auto audio = verb::readAudioFile (file);

    if (! module.isValid() || audio == nullptr || module[verb::ids::path].toString().isEmpty())
        return false;

    const auto storage = juce::roundToInt (read ("irStorage"));
    const auto embed = storage == 1 || (storage == 0 && file.getSize() < embedLimitBytes);

    module.setProperty (verb::ids::pairPath, file.getFullPathName(), nullptr);
    module.setProperty (verb::ids::pairHash, verb::hashOfFile (file), nullptr);
    module.setProperty (verb::ids::pairFlac, embed ? verb::encodeFlac (*audio) : juce::String(), nullptr);
    module.setProperty (verb::ids::pairRate, embed ? audio->sampleRate : 0.0, nullptr);
    module.setProperty (verb::ids::trueStereo, true, nullptr);
    module.setProperty (verb::ids::mono, false, nullptr);

    chainEdited (true);
    return true;
}

bool VerbProcessor::loadLibrary (int uid, const juce::String& key)
{
    auto module = findModule (uid);
    const auto* entry = verb::Library::builtIn().find (key);

    if (! module.isValid() || entry == nullptr)
        return false;

    for (const auto& id : { verb::ids::path, verb::ids::hash, verb::ids::flac, verb::ids::rate,
                            verb::ids::pairPath, verb::ids::pairHash, verb::ids::pairFlac, verb::ids::pairRate })
        module.removeProperty (id, nullptr);

    module.setProperty (verb::ids::factory, key, nullptr);
    module.setProperty (verb::ids::mono, false, nullptr);

    if (entry->isTrueStereo())
        module.setProperty (verb::ids::trueStereo, true, nullptr);
    else
        module.removeProperty (verb::ids::trueStereo, nullptr);

    chainEdited (true);
    return true;
}

void VerbProcessor::clearPair (int uid)
{
    auto module = findModule (uid);

    if (! module.isValid())
        return;

    for (const auto& id : { verb::ids::pairPath, verb::ids::pairHash, verb::ids::pairFlac, verb::ids::pairRate, verb::ids::trueStereo })
        module.removeProperty (id, nullptr);

    chainEdited (true);
}

void VerbProcessor::setTrim (double startSeconds, double endSeconds)
{
    auto chain = getChain();
    chain.setProperty (verb::ids::trimStart, startSeconds, nullptr);
    chain.setProperty (verb::ids::trimEnd, endSeconds, nullptr);
    chainEdited (false);
}

verb::IrPreview VerbProcessor::getPreview()
{
    const std::lock_guard lock (buildMutex);
    return preview;
}

bool VerbProcessor::isMissing (int uid)
{
    const std::lock_guard lock (buildMutex);
    return std::find (missing.begin(), missing.end(), uid) != missing.end();
}

juce::File VerbProcessor::settingsFile() const
{
    return getPresets().getUserDirectory().getParentDirectory().getChildFile ("settings.xml");
}

juce::File VerbProcessor::getIrFolder()
{
    const std::lock_guard lock (buildMutex);
    return irFolder;
}

void VerbProcessor::setIrFolder (const juce::File& folder)
{
    {
        const std::lock_guard lock (buildMutex);
        irFolder = folder;
    }

    saveSettings();
    requestBuild();
}

void VerbProcessor::saveSettings()
{
    juce::XmlElement xml ("FANCY_VERB_SETTINGS");

    {
        const std::lock_guard lock (buildMutex);
        xml.setAttribute ("irFolder", irFolder.getFullPathName());
    }

    if (! favourites.isEmpty())
        xml.setAttribute ("favourites", favourites.joinIntoString ("\n"));

    const auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

// Phase 4 Wave 1 ------------------------------------------------------------------------------

bool VerbProcessor::getEqualLoudness()
{
    return getChain().getProperty (verb::ids::equalLoudness, false);
}

void VerbProcessor::setEqualLoudness (bool on)
{
    auto chain = getChain();

    if (getEqualLoudness() == on)
        return;

    // Off removes the property, so the state is what it was before the option existed.
    if (on)
        chain.setProperty (verb::ids::equalLoudness, true, nullptr);
    else
        chain.removeProperty (verb::ids::equalLoudness, nullptr);

    chainEdited (false);
}

bool VerbProcessor::exportIr (const juce::File& file)
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double rate = 48000.0;

    {
        const std::lock_guard lock (buildMutex);
        audio = exportable;
        rate = exportableRate;
    }

    if (audio == nullptr || audio->getNumSamples() == 0)
        return false;

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    if (! static_cast<juce::FileOutputStream&> (*stream).openedOk())
        return false;

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (rate)
                             .withNumChannels (audio->getNumChannels())
                             .withBitsPerSample (24);

    auto writer = wav.createWriterFor (stream, options);

    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (*audio, 0, audio->getNumSamples());
}

bool VerbProcessor::dropFile (const juce::File& file, int uid)
{
    if (verb::readAudioFile (file) == nullptr)
        return false;

    if (uid > 0 && findModule (uid)[verb::ids::type].toString() == verb::storedName (ModuleType::file))
        return loadFile (uid, file);

    const auto added = addModule (ModuleType::file, getChain().getNumChildren());

    if (added == 0)
        return false;

    if (! loadFile (added, file))
    {
        removeModule (added);
        return false;
    }

    return true;
}

juce::StringArray VerbProcessor::getFavourites()
{
    return favourites;
}

bool VerbProcessor::isFavourite (const juce::String& key)
{
    return favourites.contains (key);
}

void VerbProcessor::setFavourite (const juce::String& key, bool favourite)
{
    if (favourite == favourites.contains (key))
        return;

    if (favourite)
        favourites.add (key);
    else
        favourites.removeString (key);

    saveSettings();
}

juce::AudioProcessorEditor* VerbProcessor::createEditor()
{
    return new VerbEditor (*this);
}
