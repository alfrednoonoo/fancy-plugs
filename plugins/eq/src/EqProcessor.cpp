#include "EqProcessor.h"
#include "EqEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>
#include <numbers>

namespace
{
constexpr float mixFloorDb = -60.0f;
constexpr float minHz = 20.0f;
constexpr float maxHz = 24000.0f;
constexpr int designIntervalMs = 10;

using Flag = std::shared_ptr<std::atomic<bool>>;

juce::String qText (float q)
{
    return fancy::formatFixed (q, q < 10.0f ? (q < 1.0f ? 2 : 1) : 0);
}

std::unique_ptr<juce::AudioParameterFloat> msParameter (const juce::String& id, const juce::String& name, float low, float high, float defaultMs)
{
    return fancy::floatParameter (
        id, name, fancy::logRange (low, high, 0.01f), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, value < 10.0f ? 1 : 0) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
}

fancy::FancyProcessor::Layout createLayout (const Flag& logScale)
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using Attributes = juce::AudioParameterChoiceAttributes;
    using BoolAttributes = juce::AudioParameterBoolAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    const auto option = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue, BoolAttributes {}.withAutomatable (false));
    };

    const auto setting = [] (const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        return std::make_unique<Choice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex, Attributes {}.withAutomatable (false));
    };

    const juce::StringArray types { "Low shelf", "High shelf", "Band", "Notch", "Low pass", "High pass", "Band pass", "All pass", "Tilt" };
    const juce::StringArray slopes { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };

    constexpr int defaultTypes[] { 0, 2, 2, 1 };
    constexpr float defaultHz[] { 80.0f, 500.0f, 2000.0f, 8000.0f };

    // Bands 17-24 (Phase 3 Q2.4) come after everything else, so no ID or index moves.
    constexpr int originalBands = 16;

    const auto addBase = [&] (int band)
    {
        const auto prefix = "Band " + juce::String (band + 1) + " ";
        const auto firstFour = band < 4;

        layout.add (toggle (EqProcessor::id (band, "used"), prefix + "Used", firstFour));
        layout.add (toggle (EqProcessor::id (band, "on"), prefix + "On", true));
        layout.add (std::make_unique<Choice> (juce::ParameterID { EqProcessor::id (band, "type"), 1 }, prefix + "Type", types,
                                              firstFour ? defaultTypes[band] : 2));
        layout.add (std::make_unique<fancy::SwitchableFrequencyParameter> (EqProcessor::id (band, "freq"), prefix + "Freq", logScale, minHz, maxHz,
                                                                            firstFour ? defaultHz[band] : 1000.0f));
        layout.add (fancy::levelParameter (EqProcessor::id (band, "gain"), prefix + "Gain", -24.0f, 24.0f, 0.0f, -100.0f));
        layout.add (fancy::floatParameter (
            EqProcessor::id (band, "q"), prefix + "Q", fancy::logRange (0.1f, 30.0f, 0.001f),
            band == 0 || band == 3 ? 0.707f : 1.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float value, int) { return qText (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));
        layout.add (std::make_unique<Choice> (juce::ParameterID { EqProcessor::id (band, "slope"), 1 }, prefix + "Slope", slopes, 1));
    };

    for (int band = 0; band < originalBands; ++band)
        addBase (band);

    layout.add (fancy::levelParameter ("gain", "Gain", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "phase", 1 }, "Phase", juce::StringArray { "Minimum", "Linear" }, 0));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "firLength", 1 }, "FIR length", juce::StringArray { "2048", "4096", "8192", "16384" }, 2));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling", juce::StringArray { "Off", "2x", "4x" }, 0));

    layout.add (option ("logFreq", "Log-scale automated freq.", true));
    layout.add (option ("showGrid", "Show grid", true));
    layout.add (option ("showPhase", "Show phase", false));
    layout.add (option ("compact", "Compact mode", false));
    const auto plusMinus = juce::String::charToString (0x00b1);

    layout.add (setting ("range", "Graph range",
                         juce::StringArray { "Auto", plusMinus + "6 dB", plusMinus + "12 dB", plusMinus + "18 dB", plusMinus + "24 dB" }, 0));
    layout.add (setting ("analyzer", "Analyzer", { "Off", "Pre", "Post" }, 2));
    layout.add (setting ("anaFft", "Analyzer size", { "4096", "8192", "16384" }, 1));
    layout.add (setting ("anaWindow", "Analyzer window", { "Hann", "Blackman-Harris" }, 1));
    layout.add (setting ("anaOverlap", "Analyzer overlap", { "50 %", "75 %" }, 1));
    layout.add (option ("anaTilt", "Pink tilt", false));
    layout.add (option ("anaPeak", "Peak hold", false));

    layout.add (fancy::floatParameter ("anaSmooth", "Smoothing", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                       juce::AudioParameterFloatAttributes {}
                                           .withAutomatable (false)
                                           .withLabel ("%")
                                           .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
                                           .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    // Phase 2.1 D: dynamic bands, appended after everything above so no ID or index moves.
    // Off by default, which leaves every band as it was.
    const auto addDynamic = [&] (int band)
    {
        const auto prefix = "Band " + juce::String (band + 1) + " ";

        layout.add (toggle (EqProcessor::id (band, "dyn"), prefix + "Dynamic", false));
        layout.add (fancy::levelParameter (EqProcessor::id (band, "thresh"), prefix + "Threshold", -60.0f, 0.0f, -24.0f, -100.0f));
        layout.add (fancy::levelParameter (EqProcessor::id (band, "range"), prefix + "Range", -30.0f, 30.0f, -6.0f, -100.0f));
        layout.add (msParameter (EqProcessor::id (band, "attack"), prefix + "Attack", 0.1f, 200.0f, 5.0f));
        layout.add (msParameter (EqProcessor::id (band, "release"), prefix + "Release", 5.0f, 2000.0f, 80.0f));
        layout.add (std::make_unique<Choice> (juce::ParameterID { EqProcessor::id (band, "dynMode"), 1 }, prefix + "Dynamic mode",
                                              juce::StringArray { "Above", "Below" }, 0));
        layout.add (toggle (EqProcessor::id (band, "dynSc"), prefix + "Sidechain", false));
    };

    for (int band = 0; band < originalBands; ++band)
        addDynamic (band);

    for (int band = originalBands; band < eq::maxBands; ++band)
    {
        addBase (band);
        addDynamic (band);
    }

    // Phase 3 Q2.4: the channel each band filters. Stereo leaves a band as it was.
    for (int band = 0; band < eq::maxBands; ++band)
        layout.add (std::make_unique<Choice> (juce::ParameterID { EqProcessor::id (band, "chan"), 1 },
                                              "Band " + juce::String (band + 1) + " Channel",
                                              juce::StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));

    // What the analyzer shows. Mid is the (L+R)/2 it always showed.
    layout.add (setting ("anaChan", "Analyzer channel", { "Mid", "Side", "Left", "Right" }, 0));

    return layout;
}

// Choice indices, as the parameter's list has them.
constexpr float lowShelf = 0.0f, highShelf = 1.0f, bell = 2.0f, lowPass = 4.0f, highPass = 5.0f;
constexpr float slope24 = 3.0f;

std::vector<fancy::FactoryPreset> createPresets()
{
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Low Cut", { { "b1type", highPass }, { "b1freq", 80.0f }, { "b1slope", slope24 } } },
        { "Vocal Presence", { { "b1type", highPass }, { "b1freq", 90.0f }, { "b2freq", 300.0f }, { "b2gain", -2.0f }, { "b2q", 1.2f },
                              { "b3freq", 3500.0f }, { "b3gain", 2.5f }, { "b3q", 1.0f }, { "b4freq", 10000.0f }, { "b4gain", 2.0f } } },
        { "Smile", { { "b1type", lowShelf }, { "b1freq", 120.0f }, { "b1gain", 3.0f }, { "b2freq", 800.0f }, { "b2gain", -2.0f }, { "b2q", 0.7f },
                     { "b4type", highShelf }, { "b4freq", 7000.0f }, { "b4gain", 3.0f } } },
        { "Air", { { "b4type", highShelf }, { "b4freq", 12000.0f }, { "b4gain", 3.0f } } },
        { "Telephone", { { "b1type", highPass }, { "b1freq", 300.0f }, { "b1slope", slope24 }, { "b3type", bell }, { "b3freq", 1500.0f }, { "b3gain", 3.0f },
                         { "b4type", lowPass }, { "b4freq", 3400.0f }, { "b4slope", slope24 } } },
        { "Linear Master", { { "phase", 1.0f }, { "b1type", highPass }, { "b1freq", 25.0f }, { "b1slope", slope24 }, { "b4type", highShelf }, { "b4freq", 10000.0f }, { "b4gain", 1.0f } } },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Low Cut", "Clean-up" },
        { "Vocal Presence", "Vocals" },
        { "Smile", "Tone" },
        { "Air", "Tone" },
        { "Telephone", "Effects" },
        { "Linear Master", "Mastering" },
    });

    return presets;
}
} // namespace

EqProcessor::EqProcessor()
    : EqProcessor (std::make_shared<std::atomic<bool>> (true))
{
}

EqProcessor::EqProcessor (SharedFlag logScaleFlag)
    : FancyProcessor ("Fancy EQ", "FancyEq",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout (logScaleFlag), createPresets()),
      logScale (std::move (logScaleFlag)),
      designThread (*this)
{
    auto& apvts = getApvts();

    for (int band = 0; band < eq::maxBands; ++band)
        bandValues[(size_t) band] = { apvts.getRawParameterValue (id (band, "used")),
                                      apvts.getRawParameterValue (id (band, "on")),
                                      apvts.getRawParameterValue (id (band, "type")),
                                      apvts.getRawParameterValue (id (band, "freq")),
                                      apvts.getRawParameterValue (id (band, "gain")),
                                      apvts.getRawParameterValue (id (band, "q")),
                                      apvts.getRawParameterValue (id (band, "slope")),
                                      apvts.getRawParameterValue (id (band, "chan")) };

    for (int band = 0; band < eq::maxBands; ++band)
        dynamicValues[(size_t) band] = { apvts.getRawParameterValue (id (band, "dyn")),
                                         apvts.getRawParameterValue (id (band, "thresh")),
                                         apvts.getRawParameterValue (id (band, "range")),
                                         apvts.getRawParameterValue (id (band, "attack")),
                                         apvts.getRawParameterValue (id (band, "release")),
                                         apvts.getRawParameterValue (id (band, "dynMode")),
                                         apvts.getRawParameterValue (id (band, "dynSc")) };

    apvts.addParameterListener ("logFreq", this);
}

EqProcessor::~EqProcessor()
{
    getApvts().removeParameterListener ("logFreq", this);
    designThread.stopThread (2000);
}

void EqProcessor::parameterChanged (const juce::String&, float newValue)
{
    logScale->store (newValue >= 0.5f, std::memory_order_relaxed);

    // Each band keeps its Hz, and the host learns where that now sits in 0-1.
    for (int band = 0; band < eq::maxBands; ++band)
        if (auto* frequency = dynamic_cast<juce::AudioParameterFloat*> (getApvts().getParameter (id (band, "freq"))))
            frequency->setValueNotifyingHost (frequency->convertTo0to1 (frequency->get()));
}

float EqProcessor::read (const char* parameterId)
{
    return getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool EqProcessor::isOn (const char* parameterId)
{
    return read (parameterId) >= 0.5f;
}

bool EqProcessor::isBandUsed (int band)
{
    return bandValues[(size_t) band].used->load (std::memory_order_relaxed) >= 0.5f;
}

eq::BandSpec EqProcessor::readBand (int band)
{
    const auto& values = bandValues[(size_t) band];

    eq::BandSpec spec;
    spec.on = isBandUsed (band) && values.on->load (std::memory_order_relaxed) >= 0.5f;
    spec.type = (eq::BandType) juce::roundToInt (values.type->load (std::memory_order_relaxed));
    spec.frequencyHz = (double) values.frequency->load (std::memory_order_relaxed);
    spec.gainDb = (double) values.gain->load (std::memory_order_relaxed);
    spec.q = (double) values.q->load (std::memory_order_relaxed);
    spec.order = eq::slopeOrders[juce::jlimit (0, 5, juce::roundToInt (values.slope->load (std::memory_order_relaxed)))];
    spec.channel = (eq::Channel) juce::jlimit (0, 4, juce::roundToInt (values.channel->load (std::memory_order_relaxed)));
    return spec;
}

eq::DynamicSpec EqProcessor::readDynamics (int band)
{
    const auto& values = dynamicValues[(size_t) band];
    const auto load = [] (std::atomic<float>* value) { return value->load (std::memory_order_relaxed); };

    eq::DynamicSpec spec;
    spec.on = load (values.on) >= 0.5f;
    spec.thresholdDb = (double) load (values.threshold);
    spec.rangeDb = (double) load (values.range);
    spec.attackMs = (double) load (values.attack);
    spec.releaseMs = (double) load (values.release);
    spec.below = juce::roundToInt (load (values.mode)) == 1;
    spec.sidechain = load (values.sidechain) >= 0.5f;
    return spec;
}

bool EqProcessor::isDynamic (int band)
{
    const auto spec = readBand (band);
    return readDynamics (band).on && spec.on && eq::supportsDynamics (spec.type) && ! isOn ("phase");
}

EqDsp::Targets EqProcessor::readTargets()
{
    EqDsp::Targets targets;

    for (int band = 0; band < eq::maxBands; ++band)
    {
        targets.bands[(size_t) band] = readBand (band);
        targets.dynamics[(size_t) band] = readDynamics (band);
    }

    targets.gain = fancy::levelToGain (read ("gain"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.firLength = juce::roundToInt (read ("firLength"));
    targets.oversampling = juce::roundToInt (read ("oversampling"));
    targets.linearPhase = isOn ("phase");
    targets.bypass = isOn ("bypass");
    return targets;
}

void EqProcessor::DesignThread::run()
{
    while (! threadShouldExit())
    {
        // An offline render designs on its own thread, which keeps it repeatable.
        if (! processor.isNonRealtime())
            processor.dsp.updateLinearDesign (processor.readTargets());

        wait (designIntervalMs);
    }
}

void EqProcessor::prepareAudio (double sampleRate, int)
{
    designThread.stopThread (2000);

    match.setSampleRate (sampleRate);

    dsp.prepare (sampleRate, juce::jlimit (1, 2, getMainBusNumInputChannels()));
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());

    // Always running, as a host can switch between offline and real time without preparing
    // again. It skips its work while offline, where each block designs in-line.
    designThread.startThread (juce::Thread::Priority::low);
}

bool EqProcessor::waitForBackgroundWork (int timeoutMs)
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

void EqProcessor::releaseResources()
{
    designThread.stopThread (2000);
}

void EqProcessor::reset()
{
    dsp.reset();
}

bool EqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    // The optional sidechain for dynamic bands (Phase 2.1 D).
    const auto sidechain = layouts.getChannelSet (true, 1);

    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

void EqProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    auto sidechainBuffer = getBusBuffer (buffer, true, 1);

    const auto numChannels = juce::jmin (getMainBusNumOutputChannels(), mainBuffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto channels = mainBuffer.getArrayOfWritePointers();
    const auto numSidechain = juce::jmin (sidechainBuffer.getNumChannels(), 2);
    const auto* const* sidechain = numSidechain > 0 ? sidechainBuffer.getArrayOfReadPointers() : nullptr;
    const auto analyzerSource = analyzerActive.load (std::memory_order_relaxed) ? juce::roundToInt (read ("analyzer")) : 0;

    const auto analyzerChannel = numChannels >= 2 ? juce::jlimit (0, 3, juce::roundToInt (read ("anaChan"))) : 0;

    // Mid is the plain mono sum; Side, Left and Right are a weighted pair (Phase 3 Q2.4).
    const auto feedAnalyzer = [&]
    {
        constexpr float weights[][2] { { 0.5f, 0.5f }, { 0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } };

        if (analyzerChannel == 0)
            analyzerFifo.push (channels, numChannels, numSamples);
        else
            analyzerFifo.pushWeighted (channels[0], channels[1], weights[analyzerChannel][0], weights[analyzerChannel][1], numSamples);
    };

    if (analyzerSource == 1)
        feedAnalyzer();

    const auto targets = readTargets();
    auto sidechainChosen = false;

    for (int band = 0; band < eq::maxBands; ++band)
        sidechainChosen = sidechainChosen || (targets.dynamics[(size_t) band].sidechain && targets.dynamics[(size_t) band].on
                                              && targets.bands[(size_t) band].on && eq::supportsDynamics (targets.bands[(size_t) band].type)
                                              && ! targets.linearPhase);

    // Match learns from the input before the EQ, and its reference from the sidechain.
    match.pushMain (channels, numChannels, numSamples);
    match.pushSidechain (sidechain, numSidechain, numSamples);
    sidechainChosen = sidechainChosen || match.isLearningFromSidechain();

    sidechainWatch.update (channels, numChannels, sidechain, numSidechain, numSamples, getSampleRate(), sidechainChosen);

    dsp.setSynchronousDesign (isNonRealtime());
    dsp.setTargets (targets);
    dsp.process (channels, numChannels, numSamples, sidechain, numSidechain);

    if (analyzerSource == 2)
        feedAnalyzer();

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

// The band operations set parameters without a gesture, so each is marked as a user edit,
// one undo step (Phase 2.1 W.2).
void EqProcessor::setParameter (const juce::String& parameterId, float value)
{
    const UserEdit edit (*this);
    auto& parameter = *getApvts().getParameter (parameterId);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void EqProcessor::resetBand (int band)
{
    const UserEdit edit (*this);
    for (const auto* suffix : { "on", "type", "freq", "gain", "q", "slope", "chan", "dyn", "thresh", "range", "attack", "release", "dynMode", "dynSc" })
    {
        auto& parameter = *getApvts().getParameter (id (band, suffix));
        parameter.setValueNotifyingHost (parameter.getDefaultValue());
    }
}

int EqProcessor::addBand (float hz, float gainDb)
{
    const UserEdit edit (*this);
    for (int band = 0; band < eq::maxBands; ++band)
    {
        if (isBandUsed (band))
            continue;

        resetBand (band);
        setParameter (id (band, "type"), 2.0f);
        setParameter (id (band, "freq"), hz);
        setParameter (id (band, "gain"), gainDb);
        setParameter (id (band, "used"), 1.0f);
        return band;
    }

    return -1;
}

void EqProcessor::removeBand (int band)
{
    const UserEdit edit (*this);
    setParameter (id (band, "used"), 0.0f);
}

// Match --------------------------------------------------------------------------------------

void EqProcessor::addProjectData (juce::ValueTree& state)
{
    state.removeChild (state.getChildWithName (eq::match::Engine::treeType), nullptr);
    state.appendChild (match.toTree(), nullptr);
}

void EqProcessor::stateApplied()
{
    // Only a project has it. It's kept in the engine, out of the tree that undo and A/B copy.
    auto& state = getApvts().state;

    if (const auto tree = state.getChildWithName (eq::match::Engine::treeType); tree.isValid())
    {
        match.fromTree (tree);
        state.removeChild (tree, nullptr);
    }
}

bool EqProcessor::isMatchBand (int band)
{
    if (! isBandUsed (band) || readDynamics (band).on)
        return false;

    const auto spec = readBand (band);

    if (! spec.on || spec.channel != eq::Channel::stereo)
        return false;

    for (const auto& entry : match.getWritten())
        if (entry.band == band)
            return (int) spec.type == entry.type
                && std::abs (spec.frequencyHz - (double) entry.frequencyHz) <= 0.06 + 1.0e-4 * spec.frequencyHz
                && std::abs (spec.gainDb - (double) entry.gainDb) <= 0.01
                && std::abs (spec.q - (double) entry.q) <= 0.002;

    return false;
}

int EqProcessor::getNumFreeForMatch()
{
    auto count = 0;

    for (int band = 0; band < eq::maxBands; ++band)
        count += (! isBandUsed (band) || isMatchBand (band)) ? 1 : 0;

    return count;
}

eq::match::Curve EqProcessor::getMatchExistingDb()
{
    eq::match::Curve total {};

    const auto rate = getMatchRate();
    const auto linear = isOn ("phase");
    const auto stereoBus = getMainBusNumInputChannels() >= 2;

    for (int band = 0; band < eq::maxBands; ++band)
    {
        const auto spec = readBand (band);
        const auto channel = stereoBus ? spec.channel : eq::Channel::stereo;

        // A Side band does nothing to what both channels share.
        if (! spec.on || isMatchBand (band) || channel == eq::Channel::side)
            continue;

        const auto filter = eq::design (spec, rate);

        for (int i = 0; i < eq::match::numPoints; ++i)
        {
            const auto w = 2.0 * std::numbers::pi * std::min (eq::match::hzAt (i), 0.499 * rate) / rate;
            auto response = filter.responseAt (w);

            if (linear)
                response = std::abs (response);

            // A Left or Right band reaches half of it.
            if (channel == eq::Channel::left || channel == eq::Channel::right)
                response = 0.5 * (response + 1.0);

            total[(size_t) i] += fancy::gainToDb (std::abs (response), -200.0);
        }
    }

    return total;
}

eq::match::Settings EqProcessor::getMatchFitSettings()
{
    auto settings = match.settings;
    settings.maxBands = juce::jmin (settings.maxBands, getNumFreeForMatch());
    return settings;
}

eq::match::Curve EqProcessor::getMatchTarget()
{
    return eq::match::makeTarget (match.getReferenceDb(), match.getCurrentDb(), getMatchExistingDb(), match.settings);
}

int EqProcessor::applyMatch (const std::vector<eq::BandSpec>& fitted)
{
    const UserEdit edit (*this, "Match");

    // The last Match's bands make way, unless they've been changed since: those are the user's.
    for (const auto& entry : match.getWritten())
        if (isMatchBand (entry.band))
            setParameter (id (entry.band, "used"), 0.0f);

    std::vector<eq::match::Engine::Written> written;

    for (const auto& band : fitted)
    {
        auto target = -1;

        for (int candidate = 0; candidate < eq::maxBands && target < 0; ++candidate)
            if (! isBandUsed (candidate))
                target = candidate;

        if (target < 0)
            break;

        resetBand (target);
        setParameter (id (target, "type"), (float) (int) band.type);
        setParameter (id (target, "freq"), (float) band.frequencyHz);
        setParameter (id (target, "gain"), (float) band.gainDb);
        setParameter (id (target, "q"), (float) band.q);
        setParameter (id (target, "used"), 1.0f);

        // As the parameters hold them, so an edit can be told from rounding.
        const auto spec = readBand (target);
        written.push_back ({ target, (int) spec.type, (float) spec.frequencyHz, (float) spec.gainDb, (float) spec.q });
    }

    const auto count = (int) written.size();
    match.setWritten (std::move (written));
    return count;
}

juce::AudioProcessorEditor* EqProcessor::createEditor()
{
    return new EqEditor (*this);
}
