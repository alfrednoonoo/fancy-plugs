#include "FirProcessor.h"
#include "FirEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>

using namespace fir;

namespace
{
constexpr float gateFloorDb = -90.0f;
constexpr int buildIntervalMs = 20;

const juce::Identifier curvesType ("Curves"), curveType ("Curve"), profileType ("Profile");
const juce::Identifier pointsProperty ("points"), freehandProperty ("freehand"), indexProperty ("index");
const juce::Identifier dataProperty ("data"), secondsProperty ("seconds");

juce::NormalisableRange<float> skewedRange (float low, float high, float interval, float centre)
{
    juce::NormalisableRange<float> range (low, high, interval);
    range.setSkewForCentre (centre);
    return range;
}

std::unique_ptr<juce::AudioParameterFloat> timeParameter (const juce::String& id, const juce::String& name, float low, float high, float centre, float defaultMs)
{
    return fancy::floatParameter (
        id, name, skewedRange (low, high, 0.1f, centre), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return (value < 100.0f ? fancy::formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
}

std::unique_ptr<juce::AudioParameterFloat> dbParameter (const juce::String& id, const juce::String& name, float low, float high, float defaultDb,
                                                         bool automatable = true, int decimals = 1)
{
    return fancy::floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, decimals == 0 ? 1.0f : 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (automatable)
            .withLabel ("dB")
            .withStringFromValueFunction ([decimals] (float value, int) { return (value > 0.0f ? "+" : "") + fancy::formatFixed (value, decimals) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); }));
}

fancy::FancyProcessor::Layout createLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using BoolAttributes = juce::AudioParameterBoolAttributes;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    const auto option = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue, BoolAttributes {}.withAutomatable (false));
    };

    juce::StringArray sizeNames;

    for (const auto size : sizes)
        sizeNames.add (juce::String (size));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode",
                                          juce::StringArray { "EQ", "Gate", "Compressor", "Subtract", "Convolve L/R" }, eq));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "edit", 1 }, "Edit",
                                          juce::StringArray { "Points (smooth)", "Points (flat)", "Freehand" }, smoothPoints,
                                          ChoiceAttributes {}.withAutomatable (false)));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "fft", 1 }, "FFT size", sizeNames, 5));

    layout.add (toggle ("lowLatency", "Low latency", false));
    layout.add (toggle ("reduce", "Reduce artifacts", true));
    layout.add (dbParameter ("gain", "Gain", -24.0f, 24.0f, 0.0f));

    layout.add (fancy::levelParameter ("gateFloor", "Gate floor", gateFloorDb, 0.0f, -40.0f, gateFloorDb));
    layout.add (timeParameter ("gateAttack", "Gate attack", 0.0f, 200.0f, 20.0f, 1.0f));
    layout.add (timeParameter ("gateRelease", "Gate release", 0.0f, 2000.0f, 200.0f, 80.0f));

    layout.add (fancy::floatParameter (
        "compRatio", "Compressor ratio", fancy::logRange (0.5f, 100.0f, 0.01f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + ":1"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseExpansionRatio (text, 100.0f); })));
    layout.add (timeParameter ("compAttack", "Compressor attack", 0.0f, 200.0f, 20.0f, 5.0f));
    layout.add (timeParameter ("compRelease", "Compressor release", 0.0f, 2000.0f, 200.0f, 80.0f));

    layout.add (dbParameter ("reduction", "Reduction", -60.0f, 0.0f, -18.0f, true, 0));
    layout.add (dbParameter ("offset", "Offset", -20.0f, 20.0f, 0.0f));
    layout.add (fancy::percentParameter ("smoothing", "Smoothing", 35.0f));
    layout.add (timeParameter ("subRelease", "Subtract release", 10.0f, 500.0f, 80.0f, 80.0f));

    layout.add (fancy::floatParameter (
        "learnLength", "Learn length", juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withAutomatable (false)
            .withLabel ("s")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " s"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (toggle ("sidechain", "Sidechain detector", false));

    layout.add (option ("showAnalysis", "Show analysis", true));
    layout.add (dbParameter ("graphTop", "Graph top", 0.0f, 48.0f, 24.0f, false, 0));
    layout.add (dbParameter ("graphFloor", "Analysis floor", -150.0f, -30.0f, -90.0f, false, 0));

    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));
    layout.add (toggle ("bypass", "Bypass", false));

    // Phase 2.1 A, appended after everything above: Learned keeps Subtract as it was.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "source", 1 }, "Profile source",
                                                              juce::StringArray { "Learned", "Adaptive", "Both" }, 0));

    layout.add (fancy::floatParameter (
        "adaptSpeed", "Adapt speed", fancy::logRange (0.5f, 10.0f, 0.01f), 2.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("s")
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " s"; })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          {
                                              const auto value = fancy::parseNumber (text);
                                              return text.containsIgnoreCase ("ms") ? value / 1000.0f : value;
                                          })));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Gentle Tilt", {}, "tilt" },
        { "De-mud", {}, "demud" },
        { "Noise Reduction", { { "mode", (float) subtract }, { "reduction", -18.0f }, { "fft", 5.0f } } },
        { "Spectral Gate", { { "mode", (float) gate }, { "gateFloor", -30.0f }, { "gateRelease", 120.0f } }, "gate" },
        { "Spectral Compressor", { { "mode", (float) compressor }, { "compRatio", 3.0f }, { "compAttack", 10.0f }, { "compRelease", 150.0f } }, "compressor" },
    };

    // Phase 3 Q2.1: the preset browser's categories.
    fancy::setCategories (presets, {
        { "Gentle Tilt", "Tone" },
        { "De-mud", "Tone" },
        { "Noise Reduction", "Restoration" },
        { "Spectral Gate", "Restoration" },
        { "Spectral Compressor", "Dynamics" },
    });

    return presets;
}

juce::String pointsToText (const std::vector<CurvePoint>& curve)
{
    juce::StringArray parts;

    for (const auto& point : curve)
        parts.add (juce::String (point.hz, 3) + ":" + juce::String (point.db, 3));

    return parts.joinIntoString (";");
}

std::vector<CurvePoint> pointsFromText (const juce::String& text)
{
    std::vector<CurvePoint> result;

    for (const auto& part : juce::StringArray::fromTokens (text, ";", {}))
    {
        const auto hz = part.upToFirstOccurrenceOf (":", false, false).getDoubleValue();
        const auto db = part.fromFirstOccurrenceOf (":", false, false).getDoubleValue();

        if (std::isfinite (hz) && std::isfinite (db) && hz >= curveLowHz && hz <= curveHighHz && (int) result.size() < maxCurvePoints)
            result.push_back ({ hz, std::clamp (db, -150.0, 48.0) });
    }

    sortPoints (result);
    return result;
}
} // namespace

FirProcessor::FirProcessor()
    : FancyProcessor ("Fancy FIR", "FancyFir",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout(), createPresets()),
      buildThread (*this)
{
    for (int curve = 0; curve < numCurves; ++curve)
    {
        freehand[(size_t) curve].fill ((float) defaultCurveDb (curve));
        publishCurve (curve);
    }

    // The curves are in the state from the start, so undo and A/B restore the defaults as
    // they were (Phase 2.1 W).
    storeCurves();

    getApvts().addParameterListener ("edit", this);
}

FirProcessor::~FirProcessor()
{
    getApvts().removeParameterListener ("edit", this);
    buildThread.stopThread (2000);
}

float FirProcessor::read (const char* parameterId) const
{
    return const_cast<FirProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool FirProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

int FirProcessor::getMode() const
{
    return juce::jlimit (0, (int) convolve, juce::roundToInt (read ("mode")));
}

int FirProcessor::getEditMode() const
{
    return juce::jlimit (0, (int) fir::freehand, juce::roundToInt (read ("edit")));
}

int FirProcessor::getFftSize() const
{
    return sizes[juce::jlimit (0, numSizes - 1, juce::roundToInt (read ("fft")))];
}

FirDsp::Targets FirProcessor::readTargets() const
{
    FirDsp::Targets targets;

    auto& frame = targets.frame;

    frame.mode = getMode();
    frame.gateFloorGain = fancy::levelToGain (read ("gateFloor"), gateFloorDb);
    frame.gateAttackMs = (double) read ("gateAttack");
    frame.gateReleaseMs = (double) read ("gateRelease");

    const auto ratio = (double) read ("compRatio");
    frame.compSlope = 1.0 - 1.0 / ratio;
    frame.compAttackMs = (double) read ("compAttack");
    frame.compReleaseMs = (double) read ("compRelease");

    frame.reductionGain = fancy::dbToGain ((double) read ("reduction"));
    frame.offsetPower = std::pow (10.0, (double) read ("offset") / 10.0);
    frame.smoothing = (double) read ("smoothing") * 0.01;
    frame.subReleaseMs = (double) read ("subRelease");
    frame.reduceArtifacts = isOn ("reduce");
    frame.listen = isListening() && frame.mode != eq && frame.mode != convolve;
    frame.sidechain = isOn ("sidechain");
    frame.source = juce::jlimit (0, 2, juce::roundToInt (read ("source")));
    frame.adaptSeconds = (double) read ("adaptSpeed");

    targets.engine.fftSize = getFftSize();
    targets.engine.lowLatency = isOn ("lowLatency");
    targets.engine.convolve = frame.mode == convolve;

    targets.gain = fancy::dbToGain ((double) read ("gain"));
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.bypass = isOn ("bypass");

    return targets;
}

// Curves --------------------------------------------------------------------------------------

double FirProcessor::curveDbAt (int curve, double hz) const
{
    if (getEditMode() == fir::freehand)
        return freehandAt (freehand[(size_t) curve], hz);

    return evaluateCurve (points[(size_t) curve], getEditMode() == smoothPoints, hz, defaultCurveDb (curve));
}

void FirProcessor::publishCurve (int curve)
{
    auto& table = dsp.shared.curves[(size_t) curve];

    for (int i = 0; i < curvePoints; ++i)
        table[(size_t) i].store ((float) curveDbAt (curve, curveHz (i)), std::memory_order_relaxed);

    dsp.shared.curveVersions[(size_t) curve].fetch_add (1, std::memory_order_acq_rel);
}

void FirProcessor::storeCurves()
{
    auto& state = getApvts().state;

    state.removeChild (state.getChildWithName (curvesType), nullptr);

    juce::ValueTree curves (curvesType);

    for (int curve = 0; curve < numCurves; ++curve)
    {
        juce::ValueTree node (curveType);
        node.setProperty (indexProperty, curve, nullptr);
        node.setProperty (pointsProperty, pointsToText (points[(size_t) curve]), nullptr);

        juce::MemoryBlock block (freehand[(size_t) curve].data(), sizeof (float) * (size_t) curvePoints);
        node.setProperty (freehandProperty, block.toBase64Encoding(), nullptr);

        curves.appendChild (node, nullptr);
    }

    state.appendChild (curves, nullptr);
}

void FirProcessor::loadCurves (const juce::ValueTree& tree)
{
    for (int curve = 0; curve < numCurves; ++curve)
    {
        points[(size_t) curve].clear();
        freehand[(size_t) curve].fill ((float) defaultCurveDb (curve));
    }

    for (const auto& node : tree)
    {
        if (! node.hasType (curveType))
            continue;

        const auto curve = (int) node.getProperty (indexProperty, -1);

        if (curve < 0 || curve >= numCurves)
            continue;

        points[(size_t) curve] = pointsFromText (node.getProperty (pointsProperty).toString());

        juce::MemoryBlock block;

        if (block.fromBase64Encoding (node.getProperty (freehandProperty).toString()) && block.getSize() == sizeof (float) * (size_t) curvePoints)
        {
            const auto* values = static_cast<const float*> (block.getData());

            for (int i = 0; i < curvePoints; ++i)
                freehand[(size_t) curve][(size_t) i] = std::isfinite (values[i]) ? std::clamp (values[i], -150.0f, 48.0f) : (float) defaultCurveDb (curve);
        }
    }

    for (int curve = 0; curve < numCurves; ++curve)
        publishCurve (curve);
}

void FirProcessor::setPoints (int curve, std::vector<CurvePoint> newPoints)
{
    noteEdited();
    for (auto& point : newPoints)
    {
        point.hz = std::clamp (point.hz, curveLowHz, curveHighHz);
        point.db = std::clamp (point.db, -150.0, 48.0);
    }

    sortPoints (newPoints);

    if ((int) newPoints.size() > maxCurvePoints)
        newPoints.resize ((size_t) maxCurvePoints);

    points[(size_t) curve] = std::move (newPoints);

    publishCurve (curve);
    storeCurves();
}

void FirProcessor::setFreehand (int curve, const std::array<float, curvePoints>& table)
{
    noteEdited();
    freehand[(size_t) curve] = table;

    publishCurve (curve);
    storeCurves();
}

void FirProcessor::resetCurve (int curve)
{
    noteEdited();
    points[(size_t) curve].clear();
    freehand[(size_t) curve].fill ((float) defaultCurveDb (curve));

    publishCurve (curve);
    storeCurves();
}

void FirProcessor::parameterChanged (const juce::String&, float newValue)
{
    // Freehand starts from what the points draw, so switching doesn't change the sound.
    if (juce::roundToInt (newValue) == fir::freehand)
    {
        for (int curve = 0; curve < numCurves; ++curve)
            for (int i = 0; i < curvePoints; ++i)
                freehand[(size_t) curve][(size_t) i] = (float) evaluateCurve (points[(size_t) curve], true, curveHz (i), defaultCurveDb (curve));
    }

    if (juce::MessageManager::existsAndIsCurrentThread())
        storeCurves();

    for (int curve = 0; curve < numCurves; ++curve)
        publishCurve (curve);
}

// Profile ------------------------------------------------------------------------------------

void FirProcessor::startLearning()
{
    noteEdited();
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    dsp.shared.learnTarget.store ((int) std::lround ((double) read ("learnLength") * rate));
    dsp.shared.learnedSamples.store (0);
    dsp.shared.learnRequested.store (true);
}

void FirProcessor::clearProfile()
{
    noteEdited();
    dsp.shared.learnRequested.store (false);
    dsp.shared.learning.store (false);
    dsp.shared.hasProfile.store (false);
    dsp.shared.profileSeconds.store (0.0f);

    for (auto& value : dsp.shared.profile)
        value.store (0.0f, std::memory_order_relaxed);

    dsp.shared.profileVersion.fetch_add (1);
}

float FirProcessor::getLearnProgress() const noexcept
{
    const auto target = dsp.shared.learnTarget.load();
    return target > 0 ? juce::jlimit (0.0f, 1.0f, (float) dsp.shared.learnedSamples.load() / (float) target) : 0.0f;
}

void FirProcessor::addToState (juce::ValueTree& state)
{
    state.removeChild (state.getChildWithName (profileType), nullptr);

    juce::ValueTree profile (profileType);

    if (dsp.shared.hasProfile.load())
    {
        std::vector<float> values ((size_t) profilePoints);

        for (int i = 0; i < profilePoints; ++i)
            values[(size_t) i] = dsp.shared.profile[(size_t) i].load (std::memory_order_relaxed);

        juce::MemoryBlock block (values.data(), sizeof (float) * values.size());

        profile.setProperty (dataProperty, block.toBase64Encoding(), nullptr);
        profile.setProperty (secondsProperty, dsp.shared.profileSeconds.load(), nullptr);
    }

    state.appendChild (profile, nullptr);
}

void FirProcessor::loadProfile (const juce::ValueTree& tree)
{
    juce::MemoryBlock block;

    const auto valid = tree.isValid()
                    && block.fromBase64Encoding (tree.getProperty (dataProperty).toString())
                    && block.getSize() == sizeof (float) * (size_t) profilePoints;

    if (! valid)
    {
        clearProfile();
        return;
    }

    const auto* values = static_cast<const float*> (block.getData());

    for (int i = 0; i < profilePoints; ++i)
        dsp.shared.profile[(size_t) i].store (std::isfinite (values[i]) && values[i] >= 0.0f ? values[i] : 0.0f, std::memory_order_relaxed);

    dsp.shared.profileSeconds.store ((float) (double) tree.getProperty (secondsProperty, 0.0));
    dsp.shared.learning.store (false);
    dsp.shared.hasProfile.store (true);
    dsp.shared.profileVersion.fetch_add (1);
}

void FirProcessor::stateApplied()
{
    auto& state = getApvts().state;

    loadCurves (state.getChildWithName (curvesType));
    loadProfile (state.getChildWithName (profileType));

    // The profile lives in the engine's atomics, and goes back into the state when saved.
    state.removeChild (state.getChildWithName (profileType), nullptr);
    storeCurves();
}

void FirProcessor::factoryPresetLoaded (const juce::String& data)
{
    for (int curve = 0; curve < numCurves; ++curve)
    {
        points[(size_t) curve].clear();
        freehand[(size_t) curve].fill ((float) defaultCurveDb (curve));
    }

    if (data == "tilt")
        points[eq] = { { 20.0, -3.0 }, { 20000.0, 3.0 } };
    else if (data == "demud")
        points[eq] = { { 100.0, 0.0 }, { 250.0, -4.0 }, { 600.0, 0.0 } };
    else if (data == "gate")
        points[gate] = { { 20.0, -55.0 }, { 2000.0, -60.0 }, { 20000.0, -70.0 } };
    else if (data == "compressor")
        points[compressor] = { { 20.0, -24.0 }, { 20000.0, -36.0 } };

    for (int curve = 0; curve < numCurves; ++curve)
        publishCurve (curve);

    storeCurves();
}

// Audio ----------------------------------------------------------------------------------------

void FirProcessor::BuildThread::run()
{
    while (! threadShouldExit())
    {
        processor.dsp.serviceBuilds();
        wait (buildIntervalMs);
    }
}

void FirProcessor::prepareAudio (double sampleRate, int)
{
    buildThread.stopThread (2000);

    dsp.prepare (sampleRate, readTargets());
    dsp.setSynchronousBuilds (isNonRealtime());
    setLatencySamples (dsp.getLatencySamples());

    // Always running, as a host can switch between offline and real time without preparing
    // again. Offline, each block builds in-line and the thread finds nothing to do.
    buildThread.startThread (juce::Thread::Priority::low);
}

bool FirProcessor::waitForBackgroundWork (int timeoutMs)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (dsp.isBuildPending())
    {
        if (juce::Time::getMillisecondCounterHiRes() > limit)
            return false;

        buildThread.notify();
        juce::Thread::sleep (1);
    }

    return true;
}

void FirProcessor::releaseResources()
{
    buildThread.stopThread (2000);
}

void FirProcessor::reset()
{
    dsp.reset();
}

bool FirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void FirProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    auto sidechainBuffer = getBusBuffer (buffer, true, 1);

    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = mainBuffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto main = mainBuffer.getArrayOfWritePointers();
    const auto numSidechain = juce::jmin (sidechainBuffer.getNumChannels(), 2);
    const auto analysing = analyzerActive.load (std::memory_order_relaxed);

    inputMeter.push (main, numChannels, numSamples);

    if (analysing)
        inputFifo.push (main, numChannels, numSamples);

    sidechainWatch.update (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples, getSampleRate(), isOn ("sidechain"));

    dsp.setSynchronousBuilds (isNonRealtime());
    dsp.setTargets (readTargets());
    dsp.process (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples);

    outputMeter.push (main, numChannels, numSamples);

    if (analysing)
        outputFifo.push (main, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

juce::AudioProcessorEditor* FirProcessor::createEditor()
{
    return new FirEditor (*this);
}
