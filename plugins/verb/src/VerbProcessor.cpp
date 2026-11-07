#include "VerbProcessor.h"
#include "VerbEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <algorithm>
#include <cmath>

using verb::Convolver;
using verb::ModuleType;

namespace
{
constexpr float mixFloorDb = -60.0f;
constexpr int workerIntervalMs = 25;
constexpr juce::int64 embedLimitBytes = 10 * 1024 * 1024;

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
                                           .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); })));

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
                                               const auto value = text.getFloatValue();
                                               return text.containsIgnoreCase ("L") ? -value : value;
                                           })));

    layout.add (fancy::floatParameter ("width", "Width", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                       FloatAttributes {}
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + "W"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); })));

    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, -12.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    layout.add (setting ("latency", "Latency mode", { "Zero latency", "Low CPU" }, 0));
    layout.add (setting ("maxFft", "Max FFT", { "16384", "65536" }, 1));
    layout.add (setting ("irStorage", "IR storage", { "Embed under 10 MB", "Always embed", "Path only" }, 0));

    return layout;
}

std::vector<fancy::FactoryPreset> factoryPresets()
{
    using Values = std::vector<std::pair<juce::String, float>>;

    return {
        { "Empty", Values {}, "empty" },
        { "Small Room", Values { { "wet", -9.0f } }, "room" },
        { "Plate", Values { { "predelay", 12.0f }, { "wet", -10.0f } }, "plate" },
        { "Hall", Values { { "predelay", 24.0f }, { "wet", -12.0f } }, "hall" },
        { "Long Tail", Values { { "predelay", 40.0f }, { "wet", -14.0f } }, "long" },
        { "Slapback", Values { { "wet", -6.0f } }, "slap" },
    };
}

void addFactoryModule (juce::ValueTree& chain, ModuleType type, std::initializer_list<std::pair<const char*, float>> values)
{
    auto module = verb::makeModule (type, chain.getNumChildren() + 1);

    for (const auto& [id, value] : values)
        module.setProperty (id, value, nullptr);

    chain.appendChild (module, nullptr);
}

juce::ValueTree factoryChain (const juce::String& key)
{
    auto chain = verb::makeChain();

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
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), factoryPresets()),
      buildThread (*this)
{
    if (const auto xml = juce::XmlDocument::parse (settingsFile()))
        irFolder = juce::File (xml->getStringAttribute ("irFolder"));

    getApvts().state.appendChild (verb::makeChain(), nullptr);
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
    targets.width = read ("width") * 0.01;
    targets.wet = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dry = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.output = fancy::dbToGain ((double) read ("output"));
    targets.bypass = read ("bypass") >= 0.5f;
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

    dsp.offer (std::make_unique<Convolver> (built.audio.getReadPointer (0), built.audio.getReadPointer (1),
                                            built.audio.getNumSamples(), mode, maxBlock));

    irSeconds.store (built.preview.trimmedSeconds);

    {
        const std::lock_guard lock (buildMutex);
        preview = std::move (built.preview);
        missing = notFound;
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

void VerbProcessor::prepareToPlay (double sampleRate, int)
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

    return out == layouts.getMainInputChannelSet();
}

void VerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);

    if (numChannels == 0)
        return;

    const auto offline = isNonRealtime();

    // An offline render can wait, and it must not start on an old impulse.
    if (offline)
        waitForBuild (60000);

    dsp.setTargets (readTargets());
    dsp.process (buffer.getArrayOfWritePointers(), numChannels, buffer.getNumSamples(), offline);
}

double VerbProcessor::getTailLengthSeconds() const
{
    return irSeconds.load() + 0.5;
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

void VerbProcessor::chainEdited (bool structural)
{
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

    chainEdited (true);
    return true;
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

    juce::XmlElement xml ("FANCY_VERB_SETTINGS");
    xml.setAttribute ("irFolder", folder.getFullPathName());

    const auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);

    requestBuild();
}

juce::AudioProcessorEditor* VerbProcessor::createEditor()
{
    return new VerbEditor (*this);
}
